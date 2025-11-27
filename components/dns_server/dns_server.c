#include "dns_server.h"
#include "esp_log.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "DNS";

#define DNS_PORT 53
#define DNS_MAX_LEN 512
#define UPSTREAM_DNS "8.8.8.8"
#define MAX_APPROVED_CLIENTS 16

// Global flag: enable/disable captive portal hijacking
// Disabled by default - enabled when user selects a portal from menu
static bool captive_mode_enabled = false;

// Client approval tracking
static uint32_t approved_clients[MAX_APPROVED_CLIENTS] = {0};
static int approved_count = 0;

// Server state tracking
static TaskHandle_t dns_task_handle = NULL;
static int dns_server_socket = -1;
static bool dns_server_running = false;

// DNS header structure
typedef struct {
    uint16_t id;
    uint16_t flags;
    uint16_t qdcount;
    uint16_t ancount;
    uint16_t nscount;
    uint16_t arcount;
} __attribute__((packed)) dns_header_t;

// Parse domain name from DNS query
static int parse_dns_name(const char *buffer, int offset, char *name, int max_len)
{
    int pos = offset;
    int name_pos = 0;
    int jumped = 0;
    int jump_pos = 0;

    while (buffer[pos] != 0 && name_pos < max_len - 1) {
        if ((buffer[pos] & 0xC0) == 0xC0) {
            // Pointer - follow it
            if (!jumped) {
                jump_pos = pos + 2;
            }
            jumped = 1;
            pos = ((buffer[pos] & 0x3F) << 8) | (buffer[pos + 1] & 0xFF);
            continue;
        }

        int len = buffer[pos++];
        if (len == 0) break;

        if (name_pos > 0) {
            name[name_pos++] = '.';
        }

        for (int i = 0; i < len && name_pos < max_len - 1; i++) {
            name[name_pos++] = buffer[pos++];
        }
    }

    name[name_pos] = '\0';
    return jumped ? jump_pos : pos + 1;
}

// Check if domain is a captive portal detection URL
static int is_captive_portal_domain(const char *domain)
{
    const char *captive_domains[] = {
        "captive.apple.com",
        "connectivitycheck.gstatic.com",
        "clients3.google.com",
        "connectivitycheck.android.com",
        "www.msftconnecttest.com",
        "www.msftncsi.com",
        "ipv6.msftconnecttest.com",
        NULL
    };

    for (int i = 0; captive_domains[i] != NULL; i++) {
        if (strcasecmp(domain, captive_domains[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

// Build DNS response with our AP IP (192.168.4.1)
static int build_captive_response(char *tx_buffer, const char *rx_buffer, int rx_len)
{
    memcpy(tx_buffer, rx_buffer, rx_len);
    dns_header_t *resp_header = (dns_header_t *)tx_buffer;

    // Set response flags: QR=1 (response), AA=1 (authoritative)
    resp_header->flags = htons(0x8400);
    resp_header->ancount = htons(1);  // 1 answer

    // Skip to answer section (after question)
    int answer_offset = rx_len;

    // Answer: pointer to question name (0xC00C)
    tx_buffer[answer_offset++] = 0xC0;
    tx_buffer[answer_offset++] = 0x0C;

    // Type A (0x0001)
    tx_buffer[answer_offset++] = 0x00;
    tx_buffer[answer_offset++] = 0x01;

    // Class IN (0x0001)
    tx_buffer[answer_offset++] = 0x00;
    tx_buffer[answer_offset++] = 0x01;

    // TTL (60 seconds for captive portal)
    tx_buffer[answer_offset++] = 0x00;
    tx_buffer[answer_offset++] = 0x00;
    tx_buffer[answer_offset++] = 0x00;
    tx_buffer[answer_offset++] = 0x3C;

    // Data length (4 bytes for IPv4)
    tx_buffer[answer_offset++] = 0x00;
    tx_buffer[answer_offset++] = 0x04;

    // IP address 192.168.4.1
    tx_buffer[answer_offset++] = 192;
    tx_buffer[answer_offset++] = 168;
    tx_buffer[answer_offset++] = 4;
    tx_buffer[answer_offset++] = 1;

    return answer_offset;
}

// Forward DNS query to upstream DNS server (8.8.8.8)
static int forward_dns_query(const char *query, int query_len, char *response, int max_response_len)
{
    struct sockaddr_in upstream_addr;
    upstream_addr.sin_family = AF_INET;
    upstream_addr.sin_port = htons(DNS_PORT);
    inet_pton(AF_INET, UPSTREAM_DNS, &upstream_addr.sin_addr);

    int upstream_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (upstream_sock < 0) {
        ESP_LOGE(TAG, "Failed to create upstream socket: errno %d", errno);
        // If socket creation fails, wait a bit and try once more
        vTaskDelay(pdMS_TO_TICKS(100));
        upstream_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
        if (upstream_sock < 0) {
            ESP_LOGE(TAG, "Socket creation retry failed: errno %d", errno);
            return -1;
        }
    }

    // Set timeout (2 seconds)
    struct timeval timeout;
    timeout.tv_sec = 2;
    timeout.tv_usec = 0;
    setsockopt(upstream_sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    // Send query to upstream DNS
    int sent = sendto(upstream_sock, query, query_len, 0,
                     (struct sockaddr *)&upstream_addr, sizeof(upstream_addr));
    if (sent < 0) {
        ESP_LOGE(TAG, "Failed to send to upstream DNS: errno %d", errno);
        close(upstream_sock);
        return -1;
    }

    // Receive response
    struct sockaddr_in from_addr;
    socklen_t from_len = sizeof(from_addr);
    int recv_len = recvfrom(upstream_sock, response, max_response_len, 0,
                           (struct sockaddr *)&from_addr, &from_len);

    close(upstream_sock);

    if (recv_len < 0) {
        ESP_LOGW(TAG, "Upstream DNS timeout or error: errno %d", errno);
        return -1;
    }

    return recv_len;
}

static void dns_server_task(void *pvParameters)
{
    char rx_buffer[DNS_MAX_LEN];
    char tx_buffer[DNS_MAX_LEN];
    char domain[256];
    char addr_str[128];

    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(DNS_PORT);

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        dns_server_running = false;
        vTaskDelete(NULL);
        return;
    }

    // Allow socket reuse
    int reuse = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    int err = bind(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err < 0) {
        ESP_LOGE(TAG, "Socket bind failed: errno %d", errno);
        close(sock);
        dns_server_running = false;
        vTaskDelete(NULL);
        return;
    }

    // Store socket for cleanup
    dns_server_socket = sock;

    ESP_LOGI(TAG, "DNS proxy server started on port 53");
    ESP_LOGI(TAG, "Captive portal domains -> 192.168.4.1");
    ESP_LOGI(TAG, "All other domains -> forwarded to %s", UPSTREAM_DNS);

    while (dns_server_running) {
        struct sockaddr_in source_addr;
        socklen_t socklen = sizeof(source_addr);

        int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer), 0,
                          (struct sockaddr *)&source_addr, &socklen);

        if (len < 0) {
            ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
            continue;
        }

        if (len < sizeof(dns_header_t)) {
            continue; // Packet too small
        }

        dns_header_t *header = (dns_header_t *)rx_buffer;

        // Only respond to queries (QR=0)
        if ((ntohs(header->flags) & 0x8000) != 0) {
            continue; // Already a response, ignore
        }

        // Parse the domain name from the query
        parse_dns_name(rx_buffer, sizeof(dns_header_t), domain, sizeof(domain));

        inet_ntoa_r(source_addr.sin_addr, addr_str, sizeof(addr_str) - 1);

        uint32_t client_ip = source_addr.sin_addr.s_addr;
        bool client_approved = dns_is_client_approved(client_ip);

        // Access control logic:
        // Only hijack captive detection domains for NON-approved clients
        // Once approved, forward everything so phone thinks auth succeeded
        if (captive_mode_enabled && is_captive_portal_domain(domain) && !client_approved) {
            // New client - hijack to show portal popup
            ESP_LOGI(TAG, "CAPTIVE: %s -> 192.168.4.1 (new client, trigger popup)", domain);

            int tx_len = build_captive_response(tx_buffer, rx_buffer, len);

            sendto(sock, tx_buffer, tx_len, 0,
                  (struct sockaddr *)&source_addr, sizeof(source_addr));
        } else {
            // Forward to upstream DNS - NAT will handle the traffic
            if (client_approved && is_captive_portal_domain(domain)) {
                ESP_LOGI(TAG, "APPROVED: %s -> %s (client approved, releasing)", domain, UPSTREAM_DNS);
            } else {
                ESP_LOGD(TAG, "FORWARD: %s -> %s", domain, UPSTREAM_DNS);
            }

            int response_len = forward_dns_query(rx_buffer, len, tx_buffer, sizeof(tx_buffer));

            if (response_len > 0) {
                sendto(sock, tx_buffer, response_len, 0,
                      (struct sockaddr *)&source_addr, sizeof(source_addr));
            } else {
                // Upstream timeout - send SERVFAIL
                memcpy(tx_buffer, rx_buffer, len);
                dns_header_t *resp_header = (dns_header_t *)tx_buffer;
                resp_header->flags = htons(0x8002); // SERVFAIL
                sendto(sock, tx_buffer, len, 0,
                      (struct sockaddr *)&source_addr, sizeof(source_addr));
            }
        }
    }

    close(sock);
    dns_server_socket = -1;
    dns_task_handle = NULL;
    vTaskDelete(NULL);
}

void dns_server_start(void)
{
    // Check if already running
    if (dns_server_running) {
        ESP_LOGI(TAG, "DNS server already running");
        return;
    }

    dns_server_running = true;
    xTaskCreate(dns_server_task, "dns_server", 8192, NULL, 5, &dns_task_handle);
}

void dns_server_stop(void)
{
    if (!dns_server_running) {
        ESP_LOGI(TAG, "DNS server not running");
        return;
    }

    ESP_LOGI(TAG, "Stopping DNS server...");
    dns_server_running = false;

    // Close socket to unblock recvfrom
    if (dns_server_socket >= 0) {
        close(dns_server_socket);
        dns_server_socket = -1;
    }

    // Give task time to exit
    vTaskDelay(pdMS_TO_TICKS(100));

    // Delete task if still running
    if (dns_task_handle != NULL) {
        vTaskDelete(dns_task_handle);
        dns_task_handle = NULL;
    }

    // Clear approved clients for fresh start
    approved_count = 0;
    memset(approved_clients, 0, sizeof(approved_clients));

    ESP_LOGI(TAG, "✓ DNS server stopped");
}

void dns_set_captive_mode(bool enable)
{
    captive_mode_enabled = enable;
    if (enable) {
        ESP_LOGI(TAG, "✓ Captive mode ENABLED - hijacking portal detection domains");
    } else {
        ESP_LOGI(TAG, "✓ Captive mode DISABLED - forwarding all DNS queries upstream");
    }
}

void dns_approve_client(uint32_t client_ip)
{
    // Check if already approved
    for (int i = 0; i < approved_count; i++) {
        if (approved_clients[i] == client_ip) {
            uint8_t *ip_bytes = (uint8_t*)&client_ip;
            ESP_LOGI(TAG, "Client already approved: %d.%d.%d.%d", ip_bytes[0], ip_bytes[1], ip_bytes[2], ip_bytes[3]);
            return;
        }
    }

    // Add to approved list
    if (approved_count < MAX_APPROVED_CLIENTS) {
        approved_clients[approved_count++] = client_ip;
        uint8_t *ip_bytes = (uint8_t*)&client_ip;
        ESP_LOGI(TAG, "✓ APPROVED client for internet access: %d.%d.%d.%d", ip_bytes[0], ip_bytes[1], ip_bytes[2], ip_bytes[3]);
    } else {
        uint8_t *ip_bytes = (uint8_t*)&client_ip;
        ESP_LOGW(TAG, "Approved client list full! Cannot approve: %d.%d.%d.%d", ip_bytes[0], ip_bytes[1], ip_bytes[2], ip_bytes[3]);
    }
}

bool dns_is_client_approved(uint32_t client_ip)
{
    for (int i = 0; i < approved_count; i++) {
        if (approved_clients[i] == client_ip) {
            return true;
        }
    }
    return false;
}
