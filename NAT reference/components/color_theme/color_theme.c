#include "color_theme.h"
#include "esp_log.h"
#include "esp_random.h"
#include <string.h>

static const char* TAG = "ColorTheme";

// RGB565 color definitions
#define RGB565(r, g, b) ((((r) & 0x1F) << 11) | (((g) & 0x3F) << 5) | ((b) & 0x1F))

// Available color themes
static const color_theme_t themes[] = {
    {
        .name = "RED",
        .base_color = RGB565(31, 0, 0),      // Pure red
        .dark_variant = RGB565(8, 0, 0),     // Dark red
        .light_variant = RGB565(31, 20, 20), // Light red/pink
    },
    {
        .name = "GREEN",
        .base_color = RGB565(0, 63, 0),      // Pure green
        .dark_variant = RGB565(0, 16, 0),    // Dark green
        .light_variant = RGB565(20, 63, 20), // Light green
    },
    {
        .name = "BLUE",
        .base_color = RGB565(0, 0, 31),      // Pure blue
        .dark_variant = RGB565(0, 0, 8),     // Dark blue
        .light_variant = RGB565(20, 40, 31), // Light blue/cyan
    },
    {
        .name = "YELLOW",
        .base_color = RGB565(31, 63, 0),     // Pure yellow
        .dark_variant = RGB565(8, 16, 0),    // Dark yellow/brown
        .light_variant = RGB565(31, 63, 20), // Light yellow
    },
    {
        .name = "CYAN",
        .base_color = RGB565(0, 63, 31),     // Pure cyan
        .dark_variant = RGB565(0, 16, 8),    // Dark cyan
        .light_variant = RGB565(20, 63, 31), // Light cyan
    },
    {
        .name = "MAGENTA",
        .base_color = RGB565(31, 0, 31),     // Pure magenta
        .dark_variant = RGB565(8, 0, 8),     // Dark magenta
        .light_variant = RGB565(31, 40, 31), // Light magenta
    },
    {
        .name = "ORANGE",
        .base_color = RGB565(31, 32, 0),     // Orange
        .dark_variant = RGB565(8, 8, 0),     // Dark orange
        .light_variant = RGB565(31, 50, 10), // Light orange
    },
    {
        .name = "PURPLE",
        .base_color = RGB565(16, 0, 31),     // Purple
        .dark_variant = RGB565(4, 0, 8),     // Dark purple
        .light_variant = RGB565(25, 40, 31), // Light purple
    },
};

static const int NUM_THEMES = sizeof(themes) / sizeof(themes[0]);
static int current_theme_index = 0;

void color_theme_init(void) {
    // Select random theme based on ESP32 hardware RNG
    uint32_t random = esp_random();
    current_theme_index = random % NUM_THEMES;

    const color_theme_t* theme = &themes[current_theme_index];
    ESP_LOGI(TAG, "Selected theme: %s (dark: 0x%04X, base: 0x%04X, light: 0x%04X)",
             theme->name, theme->dark_variant, theme->base_color, theme->light_variant);
}

const color_theme_t* color_theme_get_current(void) {
    return &themes[current_theme_index];
}

const char* color_theme_get_name(void) {
    return themes[current_theme_index].name;
}

uint16_t color_lerp(uint16_t color1, uint16_t color2, float ratio) {
    // Extract RGB components from RGB565
    uint8_t r1 = (color1 >> 11) & 0x1F;
    uint8_t g1 = (color1 >> 5) & 0x3F;
    uint8_t b1 = color1 & 0x1F;

    uint8_t r2 = (color2 >> 11) & 0x1F;
    uint8_t g2 = (color2 >> 5) & 0x3F;
    uint8_t b2 = color2 & 0x1F;

    // Interpolate
    uint8_t r = r1 + (uint8_t)((r2 - r1) * ratio);
    uint8_t g = g1 + (uint8_t)((g2 - g1) * ratio);
    uint8_t b = b1 + (uint8_t)((b2 - b1) * ratio);

    // Pack back to RGB565
    return RGB565(r, g, b);
}

uint16_t color_theme_gradient(uint16_t dark, uint16_t light, float progress) {
    // Clamp progress to 0.0-1.0
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;

    return color_lerp(dark, light, progress);
}
