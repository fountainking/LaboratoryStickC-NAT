#include "debug_screen.h"
#include "m5_display.h"
#include "color_theme.h"
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

    // Initialize random color theme
    color_theme_init();

    esp_err_t ret = m5_display_init(&display);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize display: %s", esp_err_to_name(ret));
        return;
    }

    const color_theme_t* theme = color_theme_get_current();
    ESP_LOGI(TAG, "Display initialized with %s theme, starting gradient test...", theme->name);

    // Gradient test - draw horizontal gradient from dark to light
    m5_display_clear(&display, COLOR_BLACK);
    for (int y = 0; y < LCD_HEIGHT; y++) {
        float progress = (float)y / (float)LCD_HEIGHT;
        uint16_t gradient_color = color_theme_gradient(theme->dark_variant, theme->light_variant, progress);
        m5_display_fill_rect(&display, 0, y, LCD_WIDTH, 1, gradient_color);
    }

    // Draw theme name on gradient
    char theme_label[32];
    snprintf(theme_label, sizeof(theme_label), "THEME: %s", theme->name);
    m5_display_draw_string(&display, 10, 50, theme_label, COLOR_WHITE, COLOR_BLACK);
    m5_display_draw_string(&display, 10, 65, "Starting...", COLOR_WHITE, COLOR_BLACK);
    m5_display_flush(&display);
    vTaskDelay(pdMS_TO_TICKS(2000));

    display_initialized = true;
    ESP_LOGI(TAG, "Debug screen initialized");
}

void debug_screen_update(void)
{
    if (!display_initialized) {
        return;  // Silently skip if display failed to init
    }

    const color_theme_t* theme = color_theme_get_current();
    char line[64];  // Increased buffer for longer strings
    int y = 2;
    int line_num = 0;
    const int total_lines = 12; // Approximate number of text lines for gradient

    // Clear screen
    m5_display_clear(&display, COLOR_BLACK);

    // Line 1: Title with theme label
    snprintf(line, sizeof(line), "LAB NAT [%s]", theme->name);
    uint16_t title_color = color_theme_gradient(theme->dark_variant, theme->light_variant, (float)line_num++ / total_lines);
    m5_display_draw_string(&display, 2, y, line, title_color, COLOR_BLACK);
    y += 12;

    // Line 2: Separator using theme color
    uint16_t sep_color = color_theme_gradient(theme->dark_variant, theme->light_variant, (float)line_num++ / total_lines);
    m5_display_fill_rect(&display, 0, y, LCD_WIDTH, 1, sep_color);
    y += 4;

    // Get WiFi info
    wifi_ap_record_t ap_info;
    esp_err_t sta_ret = esp_wifi_sta_get_ap_info(&ap_info);

    wifi_sta_list_t sta_list;
    esp_err_t ap_ret = esp_wifi_ap_get_sta_list(&sta_list);

    // Line 3: STA Status
    if (sta_ret == ESP_OK) {
        snprintf(line, sizeof(line), "STA: %s", ap_info.ssid);
        uint16_t color = color_theme_gradient(theme->base_color, COLOR_WHITE, (float)line_num++ / total_lines);
        m5_display_draw_string(&display, 2, y, line, color, COLOR_BLACK);
        y += 10;

        // Line 4: RSSI
        snprintf(line, sizeof(line), "RSSI: %d dBm", ap_info.rssi);
        color = color_theme_gradient(theme->base_color, COLOR_WHITE, (float)line_num++ / total_lines);
        m5_display_draw_string(&display, 2, y, line, color, COLOR_BLACK);
        y += 10;
    } else {
        uint16_t color = color_theme_gradient(theme->base_color, COLOR_WHITE, (float)line_num++ / total_lines);
        m5_display_draw_string(&display, 2, y, "STA: Disconnected", color, COLOR_BLACK);
        y += 10;
    }

    // Line 5: STA IP
    esp_netif_t *sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (sta_netif) {
        esp_netif_ip_info_t ip_info;
        if (esp_netif_get_ip_info(sta_netif, &ip_info) == ESP_OK) {
            snprintf(line, sizeof(line), "IP: " IPSTR, IP2STR(&ip_info.ip));
            uint16_t color = color_theme_gradient(theme->base_color, COLOR_WHITE, (float)line_num++ / total_lines);
            m5_display_draw_string(&display, 2, y, line, color, COLOR_BLACK);
            y += 10;
        }
    }

    y += 2;
    // Line 6: Separator
    uint16_t sep2_color = color_theme_gradient(theme->base_color, COLOR_WHITE, (float)line_num++ / total_lines);
    m5_display_fill_rect(&display, 0, y, LCD_WIDTH, 1, sep2_color);
    y += 4;

    // Line 7: AP Status
    uint16_t color = color_theme_gradient(theme->base_color, COLOR_WHITE, (float)line_num++ / total_lines);
    m5_display_draw_string(&display, 2, y, "AP: Laboratory", color, COLOR_BLACK);
    y += 10;

    // Line 8: Client count
    if (ap_ret == ESP_OK) {
        snprintf(line, sizeof(line), "Clients: %d/8", sta_list.num);
        color = color_theme_gradient(theme->base_color, COLOR_WHITE, (float)line_num++ / total_lines);
        m5_display_draw_string(&display, 2, y, line, color, COLOR_BLACK);
        y += 10;

        // Line 9+: Client MACs
        for (int i = 0; i < sta_list.num && i < 2; i++) {
            snprintf(line, sizeof(line), "%02X:%02X:%02X:%02X:%02X:%02X",
                     sta_list.sta[i].mac[0], sta_list.sta[i].mac[1],
                     sta_list.sta[i].mac[2], sta_list.sta[i].mac[3],
                     sta_list.sta[i].mac[4], sta_list.sta[i].mac[5]);
            color = color_theme_gradient(theme->base_color, COLOR_WHITE, (float)line_num++ / total_lines);
            m5_display_draw_string(&display, 2, y, line, color, COLOR_BLACK);
            y += 10;
        }
    } else {
        color = color_theme_gradient(theme->base_color, COLOR_WHITE, (float)line_num++ / total_lines);
        m5_display_draw_string(&display, 2, y, "Clients: 0/8", color, COLOR_BLACK);
        y += 10;
    }

    y += 2;
    // Line 10: Separator
    uint16_t sep3_color = color_theme_gradient(theme->base_color, COLOR_WHITE, (float)line_num++ / total_lines);
    m5_display_fill_rect(&display, 0, y, LCD_WIDTH, 1, sep3_color);
    y += 4;

    // Line 11: Heap
    uint32_t free_heap = esp_get_free_heap_size();
    snprintf(line, sizeof(line), "Heap: %lu KB", free_heap / 1024);
    color = color_theme_gradient(theme->base_color, COLOR_WHITE, (float)line_num++ / total_lines);
    m5_display_draw_string(&display, 2, y, line, color, COLOR_BLACK);
    y += 10;

    // Line 12: Uptime (last line, should be WHITE)
    uint32_t uptime_sec = esp_log_timestamp() / 1000;
    uint32_t uptime_min = uptime_sec / 60;
    uint32_t uptime_hr = uptime_min / 60;
    snprintf(line, sizeof(line), "Up: %luh %lum", uptime_hr, uptime_min % 60);
    color = color_theme_gradient(theme->base_color, COLOR_WHITE, (float)line_num++ / total_lines);
    m5_display_draw_string(&display, 2, y, line, color, COLOR_BLACK);

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
