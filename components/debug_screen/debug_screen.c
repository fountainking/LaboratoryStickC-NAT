#include "debug_screen.h"
#include "m5_display.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/ip_addr.h"
#include "esp_netif.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "DebugScreen";
static m5_display_t display;
static bool display_initialized = false;

void debug_screen_init(void)
{
    ESP_LOGI(TAG, "Initializing debug screen...");

    esp_err_t ret = m5_display_init(&display);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize display: %s", esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(TAG, "Display initialized, starting color test...");

    // Color test - full screen colors
    m5_display_clear(&display, COLOR_RED);
    m5_display_flush(&display);
    vTaskDelay(pdMS_TO_TICKS(500));

    m5_display_clear(&display, COLOR_GREEN);
    m5_display_flush(&display);
    vTaskDelay(pdMS_TO_TICKS(500));

    m5_display_clear(&display, COLOR_BLUE);
    m5_display_flush(&display);
    vTaskDelay(pdMS_TO_TICKS(500));

    m5_display_clear(&display, COLOR_WHITE);
    m5_display_flush(&display);
    vTaskDelay(pdMS_TO_TICKS(500));

    // Color bars
    m5_display_clear(&display, COLOR_BLACK);
    m5_display_fill_rect(&display, 0, 0, 40, LCD_HEIGHT, COLOR_RED);
    m5_display_fill_rect(&display, 40, 0, 40, LCD_HEIGHT, COLOR_GREEN);
    m5_display_fill_rect(&display, 80, 0, 40, LCD_HEIGHT, COLOR_BLUE);
    m5_display_fill_rect(&display, 120, 0, 40, LCD_HEIGHT, COLOR_YELLOW);
    m5_display_fill_rect(&display, 160, 0, 40, LCD_HEIGHT, COLOR_CYAN);
    m5_display_fill_rect(&display, 200, 0, 40, LCD_HEIGHT, COLOR_MAGENTA);
    m5_display_flush(&display);
    vTaskDelay(pdMS_TO_TICKS(1000));

    // Text test
    m5_display_clear(&display, COLOR_BLACK);
    m5_display_draw_string(&display, 5, 5, "LABORATORY", COLOR_RED, COLOR_BLACK);
    m5_display_draw_string(&display, 5, 20, "Display Test", COLOR_WHITE, COLOR_BLACK);
    m5_display_draw_string(&display, 5, 35, "RED text", COLOR_RED, COLOR_BLACK);
    m5_display_draw_string(&display, 5, 50, "GREEN text", COLOR_GREEN, COLOR_BLACK);
    m5_display_draw_string(&display, 5, 65, "BLUE text", COLOR_BLUE, COLOR_BLACK);
    m5_display_draw_string(&display, 5, 80, "YELLOW text", COLOR_YELLOW, COLOR_BLACK);
    m5_display_draw_string(&display, 5, 95, "CYAN text", COLOR_CYAN, COLOR_BLACK);
    m5_display_draw_string(&display, 5, 110, "MAGENTA text", COLOR_MAGENTA, COLOR_BLACK);
    m5_display_flush(&display);
    vTaskDelay(pdMS_TO_TICKS(2000));

    // Final splash
    m5_display_clear(&display, COLOR_BLACK);
    m5_display_draw_string(&display, 10, 50, "LABORATORY NAT", COLOR_RED, COLOR_BLACK);
    m5_display_draw_string(&display, 10, 65, "Starting...", COLOR_WHITE, COLOR_BLACK);
    m5_display_flush(&display);

    display_initialized = true;
    ESP_LOGI(TAG, "Debug screen initialized");
}

void debug_screen_update(void)
{
    if (!display_initialized) {
        return;  // Silently skip if display failed to init
    }

    char line[64];  // Increased buffer for longer strings
    int y = 2;

    // Clear screen
    m5_display_clear(&display, COLOR_BLACK);

    // Title
    m5_display_draw_string(&display, 2, y, "LAB NAT", COLOR_RED, COLOR_BLACK);
    y += 12;

    // Separator
    m5_display_fill_rect(&display, 0, y, LCD_WIDTH, 1, COLOR_RED);
    y += 4;

    // Get WiFi info
    wifi_ap_record_t ap_info;
    esp_err_t sta_ret = esp_wifi_sta_get_ap_info(&ap_info);

    wifi_sta_list_t sta_list;
    esp_err_t ap_ret = esp_wifi_ap_get_sta_list(&sta_list);

    // STA Status
    if (sta_ret == ESP_OK) {
        snprintf(line, sizeof(line), "STA: %s", ap_info.ssid);
        m5_display_draw_string(&display, 2, y, line, COLOR_GREEN, COLOR_BLACK);
        y += 10;

        snprintf(line, sizeof(line), "RSSI: %d dBm", ap_info.rssi);
        m5_display_draw_string(&display, 2, y, line, COLOR_YELLOW, COLOR_BLACK);
        y += 10;
    } else {
        m5_display_draw_string(&display, 2, y, "STA: Disconnected", COLOR_RED, COLOR_BLACK);
        y += 10;
    }

    // Get STA IP
    esp_netif_t *sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (sta_netif) {
        esp_netif_ip_info_t ip_info;
        if (esp_netif_get_ip_info(sta_netif, &ip_info) == ESP_OK) {
            snprintf(line, sizeof(line), "IP: " IPSTR, IP2STR(&ip_info.ip));
            m5_display_draw_string(&display, 2, y, line, COLOR_CYAN, COLOR_BLACK);
            y += 10;
        }
    }

    y += 2;
    m5_display_fill_rect(&display, 0, y, LCD_WIDTH, 1, COLOR_WHITE);
    y += 4;

    // AP Status
    m5_display_draw_string(&display, 2, y, "AP: Laboratory", COLOR_GREEN, COLOR_BLACK);
    y += 10;

    if (ap_ret == ESP_OK) {
        snprintf(line, sizeof(line), "Clients: %d/8", sta_list.num);
        m5_display_draw_string(&display, 2, y, line, COLOR_YELLOW, COLOR_BLACK);
        y += 10;

        // Show connected client MACs
        for (int i = 0; i < sta_list.num && i < 2; i++) {
            snprintf(line, sizeof(line), "%02X:%02X:%02X:%02X:%02X:%02X",
                     sta_list.sta[i].mac[0], sta_list.sta[i].mac[1],
                     sta_list.sta[i].mac[2], sta_list.sta[i].mac[3],
                     sta_list.sta[i].mac[4], sta_list.sta[i].mac[5]);
            m5_display_draw_string(&display, 2, y, line, COLOR_WHITE, COLOR_BLACK);
            y += 10;
        }
    } else {
        m5_display_draw_string(&display, 2, y, "Clients: 0/8", COLOR_WHITE, COLOR_BLACK);
        y += 10;
    }

    y += 2;
    m5_display_fill_rect(&display, 0, y, LCD_WIDTH, 1, COLOR_WHITE);
    y += 4;

    // Memory info
    uint32_t free_heap = esp_get_free_heap_size();
    snprintf(line, sizeof(line), "Heap: %lu KB", free_heap / 1024);
    m5_display_draw_string(&display, 2, y, line, COLOR_MAGENTA, COLOR_BLACK);
    y += 10;

    // Uptime
    uint32_t uptime_sec = esp_log_timestamp() / 1000;
    uint32_t uptime_min = uptime_sec / 60;
    uint32_t uptime_hr = uptime_min / 60;
    snprintf(line, sizeof(line), "Up: %luh %lum", uptime_hr, uptime_min % 60);
    m5_display_draw_string(&display, 2, y, line, COLOR_CYAN, COLOR_BLACK);

    // Flush to display
    m5_display_flush(&display);
}

void debug_screen_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Debug screen task started");

    while (1) {
        debug_screen_update();
        vTaskDelay(pdMS_TO_TICKS(1000));  // Update every second
    }
}
