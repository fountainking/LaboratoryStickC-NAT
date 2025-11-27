#ifndef M5_DISPLAY_H
#define M5_DISPLAY_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

// M5StickC Plus2 ST7789 Display Configuration
#define LCD_WIDTH  240
#define LCD_HEIGHT 135

// Pin definitions for M5StickC Plus2 (verified from official schematic)
#define LCD_PIN_SDA       15  // MOSI
#define LCD_PIN_SCL       13  // SCLK
#define LCD_PIN_CS        5   // Chip Select
#define LCD_PIN_DC        14  // Data/Command - FROM SCHEMATIC
#define LCD_PIN_RST       12  // Reset - FROM SCHEMATIC
#define LCD_PIN_BL        27  // Backlight

#define LCD_PIXEL_CLOCK_HZ (26 * 1000 * 1000)  // Max for non-IOMUX pins
#define LCD_CMD_BITS       8
#define LCD_PARAM_BITS     8

// Color definitions (RGB565 - standard format)
// ST7789 expects big-endian RGB565 over SPI
// We'll byte-swap during transmission
#define COLOR_BLACK   0x0000
#define COLOR_WHITE   0xFFFF
#define COLOR_RED     0xF800  // Standard RGB565
#define COLOR_GREEN   0x07E0  // Standard RGB565
#define COLOR_BLUE    0x001F  // Standard RGB565
#define COLOR_YELLOW  0xFFE0  // Standard (red + green)
#define COLOR_CYAN    0x07FF  // Standard (green + blue)
#define COLOR_MAGENTA 0xF81F  // Standard (red + blue)
#define COLOR_PURPLE  0x8010  // Purple
#define COLOR_LILAC   0xC618  // Lilac
#define COLOR_ORANGE  0xFD20  // Orange
#define COLOR_LIGHT_YELLOW 0xFFD4  // Light yellow - RGB565(31, 63, 20)

// Display handle
typedef struct {
    uint16_t width;
    uint16_t height;
    uint16_t *framebuffer;
} m5_display_t;

// Initialize the M5StickC Plus2 display
esp_err_t m5_display_init(m5_display_t *display);

// Clear the entire screen with a color
void m5_display_clear(m5_display_t *display, uint16_t color);

// Draw a filled rectangle
void m5_display_fill_rect(m5_display_t *display, int x, int y, int w, int h, uint16_t color);

// Draw a filled circle
void m5_display_fill_circle(m5_display_t *display, int cx, int cy, int r, uint16_t color);

// Draw a sprite with transparency (transparent_color pixels are skipped)
void m5_display_draw_sprite(m5_display_t *display, int x, int y, int w, int h, const uint16_t *data, uint16_t transparent_color);

// Draw a scaled sprite with transparency
void m5_display_draw_sprite_scaled(m5_display_t *display, int x, int y, int w, int h, const uint16_t *data, uint16_t transparent_color, int scale);

// Draw a rotated and scaled sprite (cx, cy = center, angle in radians)
void m5_display_draw_sprite_rotated(m5_display_t *display, int cx, int cy, int w, int h, const uint16_t *data, uint16_t transparent_color, int scale, float angle);

// Draw a rotated and scaled sprite with color tint (replaces fill color)
void m5_display_draw_sprite_rotated_tinted(m5_display_t *display, int cx, int cy, int w, int h, const uint16_t *data, uint16_t transparent_color, uint16_t tint_color, int scale, float angle);

// Draw a character (8x8 bitmap font)
void m5_display_draw_char(m5_display_t *display, int x, int y, char c, uint16_t color, uint16_t bg);

// Draw a string
void m5_display_draw_string(m5_display_t *display, int x, int y, const char *str, uint16_t color, uint16_t bg);

// Draw a scaled character (scale 1 = 8x8, scale 2 = 16x16, etc.)
void m5_display_draw_char_scaled(m5_display_t *display, int x, int y, char c, uint16_t color, uint16_t bg, int scale);

// Draw a scaled string (scale 1 = 8x8, scale 2 = 16x16, etc.)
void m5_display_draw_string_scaled(m5_display_t *display, int x, int y, const char *str, uint16_t color, uint16_t bg, int scale);

// Draw a string with horizontal gradient (color1 at start, color2 at end)
void m5_display_draw_string_gradient(m5_display_t *display, int x, int y, const char *str, uint16_t color1, uint16_t color2, uint16_t bg);

// Flush framebuffer to display
void m5_display_flush(m5_display_t *display);

// Set display brightness (false = dim, true = bright)
void m5_display_set_brightness(bool bright);

// Turn backlight completely off
void m5_display_backlight_off(void);

// Get current brightness state
bool m5_display_get_brightness(void);

#endif // M5_DISPLAY_H
