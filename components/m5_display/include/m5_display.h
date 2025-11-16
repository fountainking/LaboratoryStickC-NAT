#ifndef M5_DISPLAY_H
#define M5_DISPLAY_H

#include <stdint.h>
#include "esp_err.h"

// M5StickC Plus2 ST7789 Display Configuration
#define LCD_WIDTH  240
#define LCD_HEIGHT 135

// Pin definitions for M5StickC Plus2 (verified from M5Stack sources)
#define LCD_PIN_SDA       15  // MOSI - confirmed
#define LCD_PIN_SCL       13  // SCLK - confirmed
#define LCD_PIN_CS        5   // Chip Select (trying common M5 pins)
#define LCD_PIN_DC        23  // Data/Command (updated)
#define LCD_PIN_RST       18  // Reset (updated)
#define LCD_PIN_BL        27  // Backlight - confirmed

#define LCD_PIXEL_CLOCK_HZ (40 * 1000 * 1000)
#define LCD_CMD_BITS       8
#define LCD_PARAM_BITS     8

// Color definitions (RGB565)
#define COLOR_BLACK   0x0000
#define COLOR_WHITE   0xFFFF
#define COLOR_RED     0xF800
#define COLOR_GREEN   0x07E0
#define COLOR_BLUE    0x001F
#define COLOR_YELLOW  0xFFE0
#define COLOR_CYAN    0x07FF
#define COLOR_MAGENTA 0xF81F

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

// Draw a character (8x8 bitmap font)
void m5_display_draw_char(m5_display_t *display, int x, int y, char c, uint16_t color, uint16_t bg);

// Draw a string
void m5_display_draw_string(m5_display_t *display, int x, int y, const char *str, uint16_t color, uint16_t bg);

// Flush framebuffer to display
void m5_display_flush(m5_display_t *display);

#endif // M5_DISPLAY_H
