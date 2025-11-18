#ifndef LED_CONTROL_H
#define LED_CONTROL_H

#include <stdint.h>
#include "esp_err.h"

// M5StickC Plus2 LED is on GPIO19 (active-low, accent LED)
#define LED_GPIO 19

/**
 * Initialize LED with PWM control
 * @return ESP_OK on success
 */
esp_err_t led_init(void);

/**
 * Set LED brightness
 * @param brightness 0-255 (0=off, 255=full brightness)
 */
void led_set_brightness(uint8_t brightness);

/**
 * Get current LED brightness
 * @return Current brightness 0-255
 */
uint8_t led_get_brightness(void);

/**
 * Turn LED on at full brightness
 */
void led_on(void);

/**
 * Turn LED off
 */
void led_off(void);

/**
 * Toggle LED on/off
 */
void led_toggle(void);

/**
 * Pulse LED (fade in and out) - non-blocking, call repeatedly
 * @param speed_ms Time for full fade cycle in milliseconds
 */
void led_pulse(uint32_t speed_ms);

#endif // LED_CONTROL_H
