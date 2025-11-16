#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/ip_addr.h"
#include "lwip/lwip_napt.h"

#include "captive_portal.h"
#include "debug_screen.h"
#include "tcp_debug.h"

static const char *TAG = "Laboratory";

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
        tcp_debug_printf("[NAT] STA got IP: " IPSTR "\r\n", IP2STR(&event->ip_info.ip));

        // ENABLE NAT HERE!
        uint32_t ap_ip = 0x0104A8C0;  // 192.168.4.1 in little-endian

        // Enable NAT
        ip_napt_enable(ap_ip, 1);
        ESP_LOGI(TAG, "✓ NAT ENABLED on AP interface (192.168.4.1)");
        ESP_LOGI(TAG, "✓ IP Forwarding: STA -> AP NAT active");
        ESP_LOGI(TAG, "Clients can now access internet through this device!");
        tcp_debug_printf("[NAT] ✓ NAT ENABLED on 192.168.4.1\r\n");
        tcp_debug_printf("[NAT] ✓ IP Forwarding active - Clients can access internet!\r\n");

        // Start TCP debug server after we have IP
        tcp_debug_init();
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "=== Laboratory StickC Plus 2 - ESP-IDF with NAT ===");

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize network stack
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Create STA and AP network interfaces
    esp_netif_create_default_wifi_sta();
    esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();

    // Configure AP IP
    esp_netif_ip_info_t ip_info;
    IP4_ADDR(&ip_info.ip, 192, 168, 4, 1);
    IP4_ADDR(&ip_info.gw, 192, 168, 4, 1);
    IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);

    esp_netif_dhcps_stop(ap_netif);
    esp_netif_set_ip_info(ap_netif, &ip_info);

    // Configure DNS server for DHCP clients (Google DNS)
    esp_netif_dns_info_t dns_info;
    dns_info.ip.u_addr.ip4.addr = ESP_IP4TOADDR(8, 8, 8, 8);
    dns_info.ip.type = ESP_IPADDR_TYPE_V4;

    esp_err_t dns_ret = esp_netif_set_dns_info(ap_netif, ESP_NETIF_DNS_MAIN, &dns_info);
    if (dns_ret == ESP_OK) {
        ESP_LOGI(TAG, "✓ DHCP DNS set to 8.8.8.8");
    } else {
        ESP_LOGE(TAG, "✗ Failed to set DHCP DNS: %s", esp_err_to_name(dns_ret));
    }

    esp_netif_dhcps_start(ap_netif);

    // WiFi init
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    // Configure STA mode (connect to upstream WiFi)
    wifi_config_t wifi_sta_config = {
        .sta = {
            .ssid = STA_SSID,
            .password = STA_PASS,
        },
    };

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
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi started - AP: %s, connecting to STA: %s", AP_SSID, STA_SSID);
    ESP_LOGI(TAG, "Waiting for STA connection to enable NAT...");

    // Initialize debug screen FIRST for testing
    debug_screen_init();

    ESP_LOGI(TAG, "Display test starting in 2 seconds...");
    vTaskDelay(pdMS_TO_TICKS(2000));

    // Start captive portal
    start_captive_portal();

    // Start debug screen update task
    xTaskCreate(debug_screen_task, "debug_screen", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "=== System Ready ===");
}
