#ifndef SOUND_SYSTEM_H
#define SOUND_SYSTEM_H

#include <stdbool.h>
#include "esp_err.h"

// Buzzer GPIO pin (M5StickC Plus2)
#define BUZZER_GPIO 2

// Sound types
typedef enum {
    SOUND_NAV,        // Navigation beep (short, 2kHz)
    SOUND_SELECT,     // Selection beep (medium, 2.5kHz)
    SOUND_SUCCESS,    // Success beep (long, 3kHz)
    SOUND_ERROR       // Error beep (double beep, 1kHz)
} sound_type_t;

// Initialize sound system
esp_err_t sound_system_init(void);

// Enable or disable sound globally
void sound_system_set_enabled(bool enabled);

// Get current sound enabled state
bool sound_system_is_enabled(void);

// Play a sound (only if sound is enabled)
void sound_system_play(sound_type_t type);

// Load sound preference from NVS
void sound_system_load_preference(void);

// Save sound preference to NVS
void sound_system_save_preference(void);

#endif // SOUND_SYSTEM_H
