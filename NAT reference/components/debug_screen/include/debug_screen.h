#ifndef DEBUG_SCREEN_H
#define DEBUG_SCREEN_H

#include "m5_display.h"

// Initialize the debug screen
void debug_screen_init(void);

// Update debug screen with current system state
void debug_screen_update(void);

// Task that continuously updates the screen
void debug_screen_task(void *pvParameters);

#endif // DEBUG_SCREEN_H
