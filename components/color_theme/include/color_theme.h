#ifndef COLOR_THEME_H
#define COLOR_THEME_H

#include <stdint.h>

// Color theme structure
typedef struct {
    const char* name;
    uint16_t base_color;     // Base RGB565 color
    uint16_t dark_variant;   // Darker version (top of gradient)
    uint16_t light_variant;  // Lighter version (bottom of gradient)
} color_theme_t;

/**
 * Initialize random theme selection on boot
 * Selects a random theme and logs it
 */
void color_theme_init(void);

/**
 * Get current active theme
 */
const color_theme_t* color_theme_get_current(void);

/**
 * Get current theme name for debug display
 */
const char* color_theme_get_name(void);

/**
 * Generate gradient color between two colors
 * @param progress 0.0 (dark) to 1.0 (light)
 */
uint16_t color_theme_gradient(uint16_t dark, uint16_t light, float progress);

/**
 * Interpolate RGB565 colors
 * @param ratio 0.0 to 1.0
 */
uint16_t color_lerp(uint16_t color1, uint16_t color2, float ratio);

#endif // COLOR_THEME_H
