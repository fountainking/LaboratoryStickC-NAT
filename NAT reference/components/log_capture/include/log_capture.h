#ifndef LOG_CAPTURE_H
#define LOG_CAPTURE_H

#include "esp_log.h"
#include <stddef.h>
#include <stdbool.h>

// Maximum number of log lines to store
#define LOG_BUFFER_SIZE 100
#define LOG_LINE_MAX_LENGTH 128

/**
 * Initialize the log capture system
 * Hooks into ESP-IDF logging to capture all log output
 */
void log_capture_init(void);

/**
 * Get all captured logs as a single string
 * @param buffer Output buffer to write logs to
 * @param buffer_size Size of output buffer
 * @return Number of bytes written
 */
size_t log_capture_get_all(char *buffer, size_t buffer_size);

/**
 * Get the most recent N log lines
 * @param buffer Output buffer
 * @param buffer_size Size of output buffer
 * @param num_lines Number of lines to retrieve (0 = all)
 * @return Number of bytes written
 */
size_t log_capture_get_recent(char *buffer, size_t buffer_size, int num_lines);

/**
 * Get a specific line by index (0 = oldest, size-1 = newest)
 * @param index Line index
 * @param buffer Output buffer
 * @param buffer_size Size of output buffer
 * @return true if line exists, false otherwise
 */
bool log_capture_get_line(int index, char *buffer, size_t buffer_size);

/**
 * Get total number of lines currently stored
 */
int log_capture_get_count(void);

/**
 * Clear all captured logs
 */
void log_capture_clear(void);

#endif // LOG_CAPTURE_H
