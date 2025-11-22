#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/ip_addr.h"
#include "lwip/lwip_napt.h"
#include "lwip/sockets.h"
#include "lwip/inet.h"
#include "lwip/netif.h"

#include "ping/ping_sock.h"

#include "captive_portal.h"
#include "dns_server.h"
#include "portal_ui.h"
#include "log_capture.h"
#include "tcp_debug.h"
#include "hardware_test.h"
#include "axp2101_power.h"
#include "portal_mode.h"
#include "sound_system.h"

static const char *TAG = "Laboratory";

// Global netif handles for NAT configuration
static esp_netif_t *g_sta_netif = NULL;
static esp_netif_t *g_ap_netif = NULL;

// WiFi retry counter
static int wifi_retry_count = 0;
#define WIFI_MAX_RETRY 5

// Global flags
static bool wifi_connected = false;
bool setup_mode = true;

// Diagnostics tracking
static int total_clients_connected = 0;
static uint32_t portal_start_time = 0;

// Ping callback
static void ping_success(esp_ping_handle_t hdl, void *args)
{
    uint8_t ttl;
    uint16_t seqno;
    uint32_t elapsed_time, recv_len;
    ip_addr_t target_addr;
    esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO, &seqno, sizeof(seqno));
    esp_ping_get_profile(hdl, ESP_PING_PROF_TTL, &ttl, sizeof(ttl));
    esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &target_addr, sizeof(target_addr));
    esp_ping_get_profile(hdl, ESP_PING_PROF_SIZE, &recv_len, sizeof(recv_len));
    esp_ping_get_profile(hdl, ESP_PING_PROF_TIMEGAP, &elapsed_time, sizeof(elapsed_time));
    ESP_LOGI(TAG, "✓ PING SUCCESS: %lu bytes from %s icmp_seq=%d ttl=%d time=%lu ms",
             (unsigned long)recv_len, ipaddr_ntoa(&target_addr), seqno, ttl, (unsigned long)elapsed_time);
    tcp_debug_printf("[PING] ✓ SUCCESS: %s %lums\r\n", ipaddr_ntoa(&target_addr), (unsigned long)elapsed_time);
}

static void ping_timeout(esp_ping_handle_t hdl, void *args)
{
    uint16_t seqno;
    ip_addr_t target_addr;
    esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO, &seqno, sizeof(seqno));
    esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &target_addr, sizeof(target_addr));
    ESP_LOGW(TAG, "✗ PING TIMEOUT: From %s icmp_seq=%d", ipaddr_ntoa(&target_addr), seqno);
    tcp_debug_printf("[PING] ✗ TIMEOUT: %s\r\n", ipaddr_ntoa(&target_addr));
}

static void ping_end(esp_ping_handle_t hdl, void *args)
{
    uint32_t transmitted, received, total_time_ms;
    esp_ping_get_profile(hdl, ESP_PING_PROF_REQUEST, &transmitted, sizeof(transmitted));
    esp_ping_get_profile(hdl, ESP_PING_PROF_REPLY, &received, sizeof(received));
    esp_ping_get_profile(hdl, ESP_PING_PROF_DURATION, &total_time_ms, sizeof(total_time_ms));
    ESP_LOGI(TAG, "PING complete: %lu/%lu packets, %lu%% loss",
             (unsigned long)received, (unsigned long)transmitted, (unsigned long)(((transmitted - received) * 100) / transmitted));
    esp_ping_delete_session(hdl);
}

static void test_internet_connectivity(void)
{
    ESP_LOGI(TAG, "Testing internet connectivity - pinging 8.8.8.8...");
    tcp_debug_printf("[TEST] Pinging 8.8.8.8...\r\n");

    esp_ping_config_t ping_config = ESP_PING_DEFAULT_CONFIG();
    ping_config.target_addr.u_addr.ip4.addr = ipaddr_addr("8.8.8.8");
    ping_config.target_addr.type = IPADDR_TYPE_V4;
    ping_config.count = 3;
    ping_config.interval_ms = 1000;
    ping_config.timeout_ms = 2000;

    esp_ping_callbacks_t cbs = {
        .on_ping_success = ping_success,
        .on_ping_timeout = ping_timeout,
        .on_ping_end = ping_end,
        .cb_args = NULL
    };

    esp_ping_handle_t ping;
    esp_ping_new_session(&ping_config, &cbs, &ping);
    esp_ping_start(ping);
}

// Diagnostic monitoring task
static void diagnostics_task(void *pvParameters)
{
    ESP_LOGI(TAG, "✓ Diagnostics monitoring started");

    uint32_t min_free_heap = esp_get_free_heap_size();

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));  // Log every 10 seconds

        uint32_t free_heap = esp_get_free_heap_size();
        uint32_t min_heap = esp_get_minimum_free_heap_size();

        if (free_heap < min_free_heap) {
            min_free_heap = free_heap;
        }

        // Calculate uptime
        uint32_t uptime_sec = (xTaskGetTickCount() * portTICK_PERIOD_MS) / 1000;
        uint32_t uptime_min = uptime_sec / 60;

        // Portal uptime (if running)
        uint32_t portal_uptime_sec = 0;
        if (portal_start_time > 0) {
            portal_uptime_sec = ((xTaskGetTickCount() * portTICK_PERIOD_MS) - portal_start_time) / 1000;
        }

        ESP_LOGI(TAG, "[DIAG] Uptime: %lum%lus | Free heap: %lu / %lu (min: %lu) | Clients: %d | Portal: %lus",
                 (unsigned long)uptime_min,
                 (unsigned long)(uptime_sec % 60),
                 (unsigned long)free_heap,
                 (unsigned long)min_heap,
                 (unsigned long)min_free_heap,
                 total_clients_connected,
                 (unsigned long)portal_uptime_sec);

        tcp_debug_printf("[DIAG] Heap: %lu bytes | Clients: %d | Uptime: %lum\r\n",
                        (unsigned long)free_heap,
                        total_clients_connected,
                        (unsigned long)uptime_min);

        // Check for memory leak (heap dropping consistently)
        if (free_heap < 50000) {
            ESP_LOGW(TAG, "[DIAG] ⚠️  LOW MEMORY: %lu bytes free!", (unsigned long)free_heap);
            tcp_debug_printf("[DIAG] ⚠️  LOW MEMORY: %lu bytes\r\n", (unsigned long)free_heap);
        }
    }
}

// Portal UI mode - interactive menu system

#define AP_SSID_PORTAL "Laboratory"
#define AP_SSID_SETUP "labPORTAL Wifi Setup"
#define AP_PASS ""  // Open network
#define AP_CHANNEL 1
#define AP_MAX_CONN 10  // Hardware limit

// Function to start AP with DNS and HTTP services
void start_ap(const char *ssid, bool is_setup_mode)
{
    ESP_LOGI(TAG, "Starting AP: %s (setup_mode=%d)", ssid, is_setup_mode);

    // Set portal start time for diagnostics
    portal_start_time = xTaskGetTickCount() * portTICK_PERIOD_MS;

    // Set global setup mode flag for HTTP server
    setup_mode = is_setup_mode;

    // Configure AP with SSID
    wifi_config_t wifi_ap_config = {
        .ap = {
            .ssid = "",
            .ssid_len = 0,
            .channel = AP_CHANNEL,
            .password = AP_PASS,
            .max_connection = AP_MAX_CONN,
            .authmode = WIFI_AUTH_OPEN
        },
    };

    // Copy SSID
    strcpy((char*)wifi_ap_config.ap.ssid, ssid);
    wifi_ap_config.ap.ssid_len = strlen(ssid);

    // Switch to APSTA mode and configure AP
    esp_wifi_set_mode(WIFI_MODE_APSTA);
    esp_wifi_set_config(WIFI_IF_AP, &wifi_ap_config);

    // Start DNS server for captive portal detection
    dns_server_start();
    ESP_LOGI(TAG, "✓ DNS server started");

    // Start HTTP captive portal server
    start_captive_portal();
    ESP_LOGI(TAG, "✓ HTTP server started");

    // If STA is already connected, enable NAT
    if (wifi_connected) {
        ESP_LOGI(TAG, "STA already connected - enabling NAT");
        esp_err_t err = esp_netif_napt_enable(g_ap_netif);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "✓ NAT ENABLED on AP netif");
            tcp_debug_printf("[NAT] ✓ ENABLED: 192.168.4.x -> STA\r\n");
            // Keep captive mode enabled - access control via client approval
        } else {
            ESP_LOGE(TAG, "✗ NAT ENABLE FAILED: %s", esp_err_to_name(err));
        }
    }

    ESP_LOGI(TAG, "✓ AP active: %s", ssid);
    ESP_LOGI(TAG, "✓ Serving %s portal at http://192.168.4.1", is_setup_mode ? "SETUP" : "LABORATORY");
}

// Function to stop AP (return to STA-only mode)
void stop_ap(void)
{
    ESP_LOGI(TAG, "Stopping AP...");

    // Switch back to STA-only mode
    esp_wifi_set_mode(WIFI_MODE_STA);

    ESP_LOGI(TAG, "✓ AP stopped (STA-only mode)");
}

// No hardcoded WiFi credentials - device must learn networks via Portal UI

// Function to get current WiFi SSID
const char* get_wifi_ssid(void)
{
    static wifi_config_t wifi_config;
    esp_wifi_get_config(WIFI_IF_STA, &wifi_config);
    return (const char*)wifi_config.sta.ssid;
}

// Function to check if WiFi is connected
bool is_wifi_connected(void)
{
    return wifi_connected;
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_connected = false;
        if (wifi_retry_count < WIFI_MAX_RETRY) {
            wifi_retry_count++;
            ESP_LOGI(TAG, "Disconnected from AP, retry %d/%d...", wifi_retry_count, WIFI_MAX_RETRY);
            tcp_debug_printf("[WIFI] STA disconnected, retry %d/%d...\r\n", wifi_retry_count, WIFI_MAX_RETRY);
            esp_wifi_connect();
        } else {
            ESP_LOGW(TAG, "WiFi connection failed after %d attempts - use Portal > Join WiFi to configure", WIFI_MAX_RETRY);
            tcp_debug_printf("[WIFI] Connection failed after %d attempts\r\n", WIFI_MAX_RETRY);
        }
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        total_clients_connected++;  // Track total client connections
        ESP_LOGI(TAG, "Client connected to AP - MAC: %02x:%02x:%02x:%02x:%02x:%02x (total: %d)",
                 event->mac[0], event->mac[1], event->mac[2],
                 event->mac[3], event->mac[4], event->mac[5],
                 total_clients_connected);
        tcp_debug_printf("[AP] Client connected: %02x:%02x:%02x:%02x:%02x:%02x (total: %d)\r\n",
                 event->mac[0], event->mac[1], event->mac[2],
                 event->mac[3], event->mac[4], event->mac[5],
                 total_clients_connected);
        sound_system_play(SOUND_SUCCESS);  // Success beep for client connection
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "Client disconnected from AP - MAC: %02x:%02x:%02x:%02x:%02x:%02x",
                 event->mac[0], event->mac[1], event->mac[2],
                 event->mac[3], event->mac[4], event->mac[5]);
        tcp_debug_printf("[AP] Client disconnected: %02x:%02x:%02x:%02x:%02x:%02x\r\n",
                 event->mac[0], event->mac[1], event->mac[2],
                 event->mac[3], event->mac[4], event->mac[5]);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "STA Got IP:" IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "Gateway: " IPSTR, IP2STR(&event->ip_info.gw));
        ESP_LOGI(TAG, "Netmask: " IPSTR, IP2STR(&event->ip_info.netmask));
        tcp_debug_printf("[NAT] STA got IP: " IPSTR "\r\n", IP2STR(&event->ip_info.ip));
        wifi_connected = true;
        wifi_retry_count = 0;  // Reset retry counter on successful connection
        sound_system_play(SOUND_SUCCESS);  // Success beep for internet connection

        // Check if AP is already running (user manually started a portal)
        wifi_mode_t current_mode;
        esp_wifi_get_mode(&current_mode);
        if (current_mode == WIFI_MODE_APSTA) {
            // ENABLE NAT - Masquerade AP subnet through STA interface
            ESP_LOGI(TAG, "AP already running - enabling NAT");

            vTaskDelay(pdMS_TO_TICKS(500));  // Wait for AP to stabilize

            esp_err_t err = esp_netif_napt_enable(g_ap_netif);
            if (err == ESP_OK) {
                ESP_LOGI(TAG, "✓ NAT ENABLED on AP netif (192.168.4.1)");
                ESP_LOGI(TAG, "✓ IP Forwarding: AP clients (192.168.4.x) -> masquerade as STA (" IPSTR ")", IP2STR(&event->ip_info.ip));
                tcp_debug_printf("[NAT] ✓ ENABLED: 192.168.4.x -> " IPSTR "\r\n", IP2STR(&event->ip_info.ip));
            } else {
                ESP_LOGE(TAG, "✗ NAT ENABLE FAILED: %s", esp_err_to_name(err));
                tcp_debug_printf("[NAT] ✗ ENABLE FAILED: %s\r\n", esp_err_to_name(err));
            }

            // Keep captive portal enabled - already set when user started the portal
        } else {
            ESP_LOGI(TAG, "✓ WiFi connected - Launch Laboratory from menu to share internet");
        }

        // Test internet connectivity from ESP32 itself
        vTaskDelay(pdMS_TO_TICKS(2000));  // Wait 2s for routing to settle
        test_internet_connectivity();

        // Start TCP debug server after we have IP
        tcp_debug_init();
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "=== Laboratory StickC Plus 2 - NAT Router v1.0 ===");

    // Initialize M5 power control FIRST - set GPIO 4 HIGH to keep power on
    m5_power_init();

    // Initialize log capture to catch all logs
    log_capture_init();

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /*  HARDWARE TEST - DISABLED (use when needed)
    run_hardware_tests();
    return;
    */

    // Initialize network stack
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Create STA and AP network interfaces
    g_sta_netif = esp_netif_create_default_wifi_sta();
    g_ap_netif = esp_netif_create_default_wifi_ap();

    // Configure AP IP
    esp_netif_ip_info_t ip_info;
    IP4_ADDR(&ip_info.ip, 192, 168, 4, 1);
    IP4_ADDR(&ip_info.gw, 192, 168, 4, 1);
    IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);

    esp_netif_dhcps_stop(g_ap_netif);
    esp_netif_set_ip_info(g_ap_netif, &ip_info);

    // Configure DHCP server to offer our DNS proxy (192.168.4.1)
    // This is CRITICAL - clients must use our DNS proxy for captive portal detection
    // and for forwarding real DNS queries to 8.8.8.8
    uint32_t dns_server = ESP_IP4TOADDR(192, 168, 4, 1);
    esp_netif_dhcps_option(g_ap_netif, ESP_NETIF_OP_SET, ESP_NETIF_DOMAIN_NAME_SERVER, &dns_server, sizeof(dns_server));

    ESP_LOGI(TAG, "✓ DHCP DNS configured to offer 192.168.4.1 (our DNS proxy)");

    esp_netif_dhcps_start(g_ap_netif);

    // WiFi init
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    // Configure STA mode - load from NVS only (no hardcoded fallback)
    wifi_config_t wifi_sta_config = {0};

    // Try to load saved WiFi credentials from NVS
    nvs_handle_t nvs;
    if (nvs_open("storage", NVS_READONLY, &nvs) == ESP_OK) {
        size_t ssid_len = sizeof(wifi_sta_config.sta.ssid);
        size_t pass_len = sizeof(wifi_sta_config.sta.password);

        if (nvs_get_str(nvs, "wifi_ssid", (char*)wifi_sta_config.sta.ssid, &ssid_len) == ESP_OK &&
            nvs_get_str(nvs, "wifi_pass", (char*)wifi_sta_config.sta.password, &pass_len) == ESP_OK) {
            ESP_LOGI(TAG, "✓ Loaded saved network from NVS: %s", wifi_sta_config.sta.ssid);
            setup_mode = false;
        } else {
            ESP_LOGI(TAG, "No saved networks - device must connect via Portal UI");
            setup_mode = true;
        }
        nvs_close(nvs);
    } else {
        ESP_LOGI(TAG, "NVS not available - device must connect via Portal UI");
        setup_mode = true;
    }

    // Start in STA-only mode - AP will only be started when user activates it from menu
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_sta_config));

    // Disable WiFi power management to prevent sleep/cutoff on battery
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    ESP_LOGI(TAG, "WiFi power management DISABLED (always on)");

    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi started in STA-only mode");
    ESP_LOGI(TAG, "Searching for network: %s",
             strlen((char*)wifi_sta_config.sta.ssid) > 0 ? (char*)wifi_sta_config.sta.ssid : "[NONE]");
    ESP_LOGI(TAG, "AP disabled on boot - use Portal UI to start if needed");

    // DNS server and captive portal are NOT started on boot
    // They will be started when user activates AP from the Portal UI menu

    // Initialize sound system
    sound_system_init();
    ESP_LOGI(TAG, "Sound system initialized");

    // Start diagnostics monitoring task
    xTaskCreate(diagnostics_task, "diagnostics", 4096, NULL, 3, NULL);
    ESP_LOGI(TAG, "Diagnostics task started");

    // Initialize Portal UI
    portal_ui_init();
    ESP_LOGI(TAG, "Portal UI initialized");
    vTaskDelay(pdMS_TO_TICKS(1000));
    portal_ui_start();
    ESP_LOGI(TAG, "Portal UI started");

    ESP_LOGI(TAG, "=== System Ready ===");
    ESP_LOGI(TAG, "Portal: http://192.168.4.1");
    ESP_LOGI(TAG, "Logs: http://192.168.4.1/debug/logs");
}
