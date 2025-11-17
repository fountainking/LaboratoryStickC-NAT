#ifndef LOG_SCREEN_H
#define LOG_SCREEN_H

#include <stdbool.h>

/**
 * Initialize the log screen display
 */
void log_screen_init(void);

/**
 * Update the log screen with latest logs
 * Shows scrollable log output on display
 */
void log_screen_update(void);

/**
 * Scroll up in log view
 */
void log_screen_scroll_up(void);

/**
 * Scroll down in log view
 */
void log_screen_scroll_down(void);

/**
 * Task function for continuous log screen updates
 */
void log_screen_task(void *pvParameters);

#endif // LOG_SCREEN_H
