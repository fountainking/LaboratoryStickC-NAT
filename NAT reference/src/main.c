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
#include "debug_screen.h"
#include "log_screen.h"
#include "log_capture.h"
#include "tcp_debug.h"
#include "hardware_test.h"
#include "axp2101_power.h"

static const char *TAG = "Laboratory";

// Global netif handles for NAT configuration
static esp_netif_t *g_sta_netif = NULL;
static esp_netif_t *g_ap_netif = NULL;

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

// Mode selection: 0 = debug screen, 1 = log screen
static int display_mode = 0;  // Use debug screen (cleaner)

#define AP_SSID "Laboratory"
#define AP_PASS ""  // Open network
#define AP_CHANNEL 1
#define AP_MAX_CONN 8

// WiFi credentials
#define STA_SSID "ThankYouGod"
#define STA_PASS "Pokemon1500"

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "Disconnected from AP, retrying...");
        tcp_debug_printf("[WIFI] STA disconnected, retrying...\r\n");
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "Client connected to AP - MAC: %02x:%02x:%02x:%02x:%02x:%02x",
                 event->mac[0], event->mac[1], event->mac[2],
                 event->mac[3], event->mac[4], event->mac[5]);
        tcp_debug_printf("[AP] Client connected: %02x:%02x:%02x:%02x:%02x:%02x\r\n",
                 event->mac[0], event->mac[1], event->mac[2],
                 event->mac[3], event->mac[4], event->mac[5]);
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

        // ENABLE NAT - Masquerade AP subnet (192.168.4.0/24) through STA interface
        ESP_LOGI(TAG, "Enabling NAT on AP netif (192.168.4.0/24 -> STA)");

        // Enable NAPT on the AP netif using the esp_netif API
        esp_err_t err = esp_netif_napt_enable(g_ap_netif);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "✓ NAT ENABLED on AP netif (192.168.4.1)");
            ESP_LOGI(TAG, "✓ IP Forwarding: AP clients (192.168.4.x) -> masquerade as STA (" IPSTR ")", IP2STR(&event->ip_info.ip));
            tcp_debug_printf("[NAT] ✓ ENABLED: 192.168.4.x -> " IPSTR "\r\n", IP2STR(&event->ip_info.ip));
        } else {
            ESP_LOGE(TAG, "✗ NAT ENABLE FAILED: %s", esp_err_to_name(err));
            tcp_debug_printf("[NAT] ✗ ENABLE FAILED: %s\r\n", esp_err_to_name(err));
        }

        // Test internet connectivity from ESP32 itself
        vTaskDelay(pdMS_TO_TICKS(2000));  // Wait 2s for routing to settle
        test_internet_connectivity();

        // Disable captive portal hijacking - let iOS/Android see real internet
        dns_set_captive_mode(false);

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

    // Configure STA mode (try to load from NVS first, fallback to hardcoded)
    wifi_config_t wifi_sta_config = {0};

    // Try to load saved WiFi credentials from NVS
    nvs_handle_t nvs;
    bool credentials_loaded = false;
    if (nvs_open("storage", NVS_READONLY, &nvs) == ESP_OK) {
        size_t ssid_len = sizeof(wifi_sta_config.sta.ssid);
        size_t pass_len = sizeof(wifi_sta_config.sta.password);

        if (nvs_get_str(nvs, "wifi_ssid", (char*)wifi_sta_config.sta.ssid, &ssid_len) == ESP_OK &&
            nvs_get_str(nvs, "wifi_pass", (char*)wifi_sta_config.sta.password, &pass_len) == ESP_OK) {
            ESP_LOGI(TAG, "Loaded saved WiFi credentials from NVS: %s", wifi_sta_config.sta.ssid);
            credentials_loaded = true;
        }
        nvs_close(nvs);
    }

    // Fallback to hardcoded credentials if none saved
    if (!credentials_loaded) {
        strcpy((char*)wifi_sta_config.sta.ssid, STA_SSID);
        strcpy((char*)wifi_sta_config.sta.password, STA_PASS);
        ESP_LOGI(TAG, "Using hardcoded WiFi credentials: %s", STA_SSID);
    }

    // Configure AP mode
    wifi_config_t wifi_ap_config = {
        .ap = {
            .ssid = AP_SSID,
            .ssid_len = strlen(AP_SSID),
            .channel = AP_CHANNEL,
            .password = AP_PASS,
            .max_connection = AP_MAX_CONN,
            .authmode = WIFI_AUTH_OPEN
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_sta_config));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_ap_config));

    // Disable WiFi power management to prevent sleep/cutoff on battery
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    ESP_LOGI(TAG, "WiFi power management DISABLED (always on)");

    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi started - AP: %s, connecting to STA: %s", AP_SSID, STA_SSID);
    ESP_LOGI(TAG, "Waiting for STA connection to enable NAT...");

    // Initialize display based on mode
    if (display_mode == 0) {
        debug_screen_init();
        ESP_LOGI(TAG, "Starting in DEBUG screen mode");
        vTaskDelay(pdMS_TO_TICKS(2000));
        xTaskCreate(debug_screen_task, "debug_screen", 4096, NULL, 5, NULL);
    } else {
        log_screen_init();
        ESP_LOGI(TAG, "Starting in LOG screen mode");
        vTaskDelay(pdMS_TO_TICKS(2000));
        xTaskCreate(log_screen_task, "log_screen", 4096, NULL, 5, NULL);
    }

    // Start DNS hijack server (triggers captive portal popup)
    dns_server_start();

    // Start captive portal (includes HTTP log endpoints)
    start_captive_portal();

    ESP_LOGI(TAG, "=== System Ready ===");
    ESP_LOGI(TAG, "DNS server: Hijacking all queries -> 192.168.4.1");
    ESP_LOGI(TAG, "HTTP endpoints:");
    ESP_LOGI(TAG, "  http://192.168.4.1/           - Portal page");
    ESP_LOGI(TAG, "  http://192.168.4.1/debug/logs - All logs");
    ESP_LOGI(TAG, "  http://192.168.4.1/debug/recent?lines=20 - Recent logs");
}
