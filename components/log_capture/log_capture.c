#include "log_capture.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "LogCapture";

// Ring buffer for log lines
static char log_buffer[LOG_BUFFER_SIZE][LOG_LINE_MAX_LENGTH];
static int log_head = 0;  // Next write position
static int log_count = 0; // Total lines stored
static SemaphoreHandle_t log_mutex = NULL;

// Original log function pointer
static vprintf_like_t original_log_func = NULL;

// Custom log function that captures output
static int log_capture_vprintf(const char *fmt, va_list args)
{
    // Call original logger first
    int ret = 0;
    if (original_log_func) {
        va_list args_copy;
        va_copy(args_copy, args);
        ret = original_log_func(fmt, args_copy);
        va_end(args_copy);
    }

    // Capture to ring buffer
    if (log_mutex && xSemaphoreTake(log_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        // Format the log message
        vsnprintf(log_buffer[log_head], LOG_LINE_MAX_LENGTH, fmt, args);

        // Remove trailing newline if present
        size_t len = strlen(log_buffer[log_head]);
        if (len > 0 && log_buffer[log_head][len - 1] == '\n') {
            log_buffer[log_head][len - 1] = '\0';
        }

        // Advance head pointer
        log_head = (log_head + 1) % LOG_BUFFER_SIZE;

        // Update count
        if (log_count < LOG_BUFFER_SIZE) {
            log_count++;
        }

        xSemaphoreGive(log_mutex);
    }

    return ret;
}

void log_capture_init(void)
{
    // Create mutex for thread-safe access
    log_mutex = xSemaphoreCreateMutex();
    if (!log_mutex) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return;
    }

    // Clear buffer
    memset(log_buffer, 0, sizeof(log_buffer));
    log_head = 0;
    log_count = 0;

    // Hook into ESP-IDF logging system
    original_log_func = esp_log_set_vprintf(log_capture_vprintf);

    ESP_LOGI(TAG, "Log capture initialized (buffer size: %d lines)", LOG_BUFFER_SIZE);
}

size_t log_capture_get_all(char *buffer, size_t buffer_size)
{
    if (!buffer || buffer_size == 0 || !log_mutex) {
        return 0;
    }

    size_t written = 0;
    buffer[0] = '\0';

    if (xSemaphoreTake(log_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        // Calculate starting position (oldest log)
        int start = (log_count < LOG_BUFFER_SIZE) ? 0 : log_head;

        for (int i = 0; i < log_count; i++) {
            int idx = (start + i) % LOG_BUFFER_SIZE;
            size_t line_len = strlen(log_buffer[idx]);

            // Check if we have space for this line + newline + null terminator
            if (written + line_len + 2 >= buffer_size) {
                break;
            }

            // Append line
            strcat(buffer, log_buffer[idx]);
            strcat(buffer, "\n");
            written += line_len + 1;
        }

        xSemaphoreGive(log_mutex);
    }

    return written;
}

size_t log_capture_get_recent(char *buffer, size_t buffer_size, int num_lines)
{
    if (!buffer || buffer_size == 0 || !log_mutex) {
        return 0;
    }

    // If num_lines is 0 or greater than available, get all
    if (num_lines <= 0 || num_lines > log_count) {
        num_lines = log_count;
    }

    size_t written = 0;
    buffer[0] = '\0';

    if (xSemaphoreTake(log_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        // Start from (head - num_lines) to get most recent
        int start = (log_head - num_lines + LOG_BUFFER_SIZE) % LOG_BUFFER_SIZE;

        for (int i = 0; i < num_lines; i++) {
            int idx = (start + i) % LOG_BUFFER_SIZE;
            size_t line_len = strlen(log_buffer[idx]);

            if (written + line_len + 2 >= buffer_size) {
                break;
            }

            strcat(buffer, log_buffer[idx]);
            strcat(buffer, "\n");
            written += line_len + 1;
        }

        xSemaphoreGive(log_mutex);
    }

    return written;
}

bool log_capture_get_line(int index, char *buffer, size_t buffer_size)
{
    if (!buffer || buffer_size == 0 || !log_mutex || index < 0 || index >= log_count) {
        return false;
    }

    bool success = false;

    if (xSemaphoreTake(log_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        int start = (log_count < LOG_BUFFER_SIZE) ? 0 : log_head;
        int idx = (start + index) % LOG_BUFFER_SIZE;

        strncpy(buffer, log_buffer[idx], buffer_size - 1);
        buffer[buffer_size - 1] = '\0';
        success = true;

        xSemaphoreGive(log_mutex);
    }

    return success;
}

int log_capture_get_count(void)
{
    return log_count;
}

void log_capture_clear(void)
{
    if (log_mutex && xSemaphoreTake(log_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        memset(log_buffer, 0, sizeof(log_buffer));
        log_head = 0;
        log_count = 0;
        xSemaphoreGive(log_mutex);
    }
}
