#include "log_screen.h"
#include "log_capture.h"
#include "m5_display.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "LogScreen";
static m5_display_t display;
static bool display_initialized = false;
static int scroll_offset = 0;  // Current scroll position

// Display can fit approximately 13 lines of text (135px / 10px per line)
#define VISIBLE_LINES 13
#define LINE_HEIGHT 10
#define CHAR_WIDTH 6
#define MAX_CHARS_PER_LINE (LCD_WIDTH / CHAR_WIDTH)

void log_screen_init(void)
{
    ESP_LOGI(TAG, "Initializing log screen...");

    esp_err_t ret = m5_display_init(&display);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize display: %s", esp_err_to_name(ret));
        return;
    }

    // Initial splash
    m5_display_clear(&display, COLOR_BLACK);
    m5_display_draw_string(&display, 5, 60, "LOG MONITOR", COLOR_RED, COLOR_BLACK);
    m5_display_flush(&display);
    vTaskDelay(pdMS_TO_TICKS(1000));

    display_initialized = true;
    ESP_LOGI(TAG, "Log screen initialized");
}

void log_screen_update(void)
{
    if (!display_initialized) {
        return;
    }

    // Clear screen
    m5_display_clear(&display, COLOR_BLACK);

    // Title bar
    m5_display_draw_string(&display, 2, 2, "LOGS", COLOR_RED, COLOR_BLACK);
    m5_display_fill_rect(&display, 0, 12, LCD_WIDTH, 1, COLOR_RED);

    // Get total number of log lines
    int total_lines = log_capture_get_count();

    if (total_lines == 0) {
        m5_display_draw_string(&display, 2, 30, "No logs yet...", COLOR_YELLOW, COLOR_BLACK);
        m5_display_flush(&display);
        return;
    }

    // Clamp scroll offset
    int max_scroll = total_lines - VISIBLE_LINES;
    if (max_scroll < 0) max_scroll = 0;
    if (scroll_offset > max_scroll) scroll_offset = max_scroll;
    if (scroll_offset < 0) scroll_offset = 0;

    // Display log lines
    char line_buffer[LOG_LINE_MAX_LENGTH];
    int y = 15;

    for (int i = 0; i < VISIBLE_LINES && (scroll_offset + i) < total_lines; i++) {
        if (log_capture_get_line(scroll_offset + i, line_buffer, sizeof(line_buffer))) {
            // Truncate line if too long
            if (strlen(line_buffer) > MAX_CHARS_PER_LINE) {
                line_buffer[MAX_CHARS_PER_LINE - 3] = '.';
                line_buffer[MAX_CHARS_PER_LINE - 2] = '.';
                line_buffer[MAX_CHARS_PER_LINE - 1] = '.';
                line_buffer[MAX_CHARS_PER_LINE] = '\0';
            }

            // Color code based on log level
            uint16_t color = COLOR_WHITE;
            if (strstr(line_buffer, "E (") != NULL) {
                color = COLOR_RED;
            } else if (strstr(line_buffer, "W (") != NULL) {
                color = COLOR_YELLOW;
            } else if (strstr(line_buffer, "I (") != NULL) {
                color = COLOR_CYAN;
            } else if (strstr(line_buffer, "D (") != NULL) {
                color = COLOR_GREEN;
            }

            m5_display_draw_string(&display, 2, y, line_buffer, color, COLOR_BLACK);
            y += LINE_HEIGHT;
        }
    }

    // Scroll indicator at bottom
    if (total_lines > VISIBLE_LINES) {
        char scroll_info[40];
        int end_line = scroll_offset + VISIBLE_LINES > total_lines ? total_lines : scroll_offset + VISIBLE_LINES;
        snprintf(scroll_info, sizeof(scroll_info), "%d-%d/%d",
                 scroll_offset + 1, end_line, total_lines);
        m5_display_draw_string(&display, LCD_WIDTH - 60, 2, scroll_info, COLOR_YELLOW, COLOR_BLACK);
    }

    m5_display_flush(&display);
}

void log_screen_scroll_up(void)
{
    if (scroll_offset > 0) {
        scroll_offset--;
        ESP_LOGI(TAG, "Scroll up to offset %d", scroll_offset);
    }
}

void log_screen_scroll_down(void)
{
    int total_lines = log_capture_get_count();
    int max_scroll = total_lines - VISIBLE_LINES;
    if (max_scroll < 0) max_scroll = 0;

    if (scroll_offset < max_scroll) {
        scroll_offset++;
        ESP_LOGI(TAG, "Scroll down to offset %d", scroll_offset);
    }
}

void log_screen_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Log screen task started");

    while (1) {
        // Auto-scroll to bottom (latest logs)
        int total_lines = log_capture_get_count();
        int max_scroll = total_lines - VISIBLE_LINES;
        if (max_scroll < 0) max_scroll = 0;
        scroll_offset = max_scroll;

        log_screen_update();
        vTaskDelay(pdMS_TO_TICKS(1000));  // Update every second
    }
}
