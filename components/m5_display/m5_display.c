#include "m5_display.h"
#include "esp_log.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

static const char *TAG = "M5Display";

// Brightness state
static bool brightness_high = true;  // Default: bright

// Backlight PWM configuration
#define BL_LEDC_TIMER        LEDC_TIMER_1
#define BL_LEDC_MODE         LEDC_LOW_SPEED_MODE
#define BL_LEDC_CHANNEL      LEDC_CHANNEL_1
#define BL_LEDC_DUTY_RES     LEDC_TIMER_8_BIT
#define BL_DUTY_BRIGHT       (255)   // 100% duty cycle
#define BL_DUTY_DIM          (102)   // 40% duty cycle (60% dimmer)

static bool backlight_pwm_initialized = false;

// ST7789 Commands
#define ST7789_SWRESET 0x01
#define ST7789_SLPOUT  0x11
#define ST7789_INVON   0x21
#define ST7789_DISPON  0x29
#define ST7789_CASET   0x2A
#define ST7789_RASET   0x2B
#define ST7789_RAMWR   0x2C
#define ST7789_MADCTL  0x36
#define ST7789_COLMOD  0x3A

static spi_device_handle_t spi_handle;

// 8x8 bitmap font (ASCII 32-126)
static const uint8_t font8x8_basic[96][8] = {
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},  // ' '
    {0x18, 0x3C, 0x3C, 0x18, 0x18, 0x00, 0x18, 0x00},  // '!'
    {0x36, 0x36, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},  // '"'
    {0x36, 0x36, 0x7F, 0x36, 0x7F, 0x36, 0x36, 0x00},  // '#'
    {0x0C, 0x3E, 0x03, 0x1E, 0x30, 0x1F, 0x0C, 0x00},  // '$'
    {0x00, 0x63, 0x33, 0x18, 0x0C, 0x66, 0x63, 0x00},  // '%'
    {0x1C, 0x36, 0x1C, 0x6E, 0x3B, 0x33, 0x6E, 0x00},  // '&'
    {0x06, 0x06, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00},  // '''
    {0x18, 0x0C, 0x06, 0x06, 0x06, 0x0C, 0x18, 0x00},  // '('
    {0x06, 0x0C, 0x18, 0x18, 0x18, 0x0C, 0x06, 0x00},  // ')'
    {0x00, 0x66, 0x3C, 0xFF, 0x3C, 0x66, 0x00, 0x00},  // '*'
    {0x00, 0x0C, 0x0C, 0x3F, 0x0C, 0x0C, 0x00, 0x00},  // '+'
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C, 0x06},  // ','
    {0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x00},  // '-'
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C, 0x00},  // '.'
    {0x60, 0x30, 0x18, 0x0C, 0x06, 0x03, 0x01, 0x00},  // '/'
    {0x3E, 0x63, 0x73, 0x7B, 0x6F, 0x67, 0x3E, 0x00},  // '0'
    {0x0C, 0x0E, 0x0C, 0x0C, 0x0C, 0x0C, 0x3F, 0x00},  // '1'
    {0x1E, 0x33, 0x30, 0x1C, 0x06, 0x33, 0x3F, 0x00},  // '2'
    {0x1E, 0x33, 0x30, 0x1C, 0x30, 0x33, 0x1E, 0x00},  // '3'
    {0x38, 0x3C, 0x36, 0x33, 0x7F, 0x30, 0x78, 0x00},  // '4'
    {0x3F, 0x03, 0x1F, 0x30, 0x30, 0x33, 0x1E, 0x00},  // '5'
    {0x1C, 0x06, 0x03, 0x1F, 0x33, 0x33, 0x1E, 0x00},  // '6'
    {0x3F, 0x33, 0x30, 0x18, 0x0C, 0x0C, 0x0C, 0x00},  // '7'
    {0x1E, 0x33, 0x33, 0x1E, 0x33, 0x33, 0x1E, 0x00},  // '8'
    {0x1E, 0x33, 0x33, 0x3E, 0x30, 0x18, 0x0E, 0x00},  // '9'
    {0x00, 0x0C, 0x0C, 0x00, 0x00, 0x0C, 0x0C, 0x00},  // ':'
    {0x00, 0x0C, 0x0C, 0x00, 0x00, 0x0C, 0x0C, 0x06},  // ';'
    {0x18, 0x0C, 0x06, 0x03, 0x06, 0x0C, 0x18, 0x00},  // '<'
    {0x00, 0x00, 0x3F, 0x00, 0x00, 0x3F, 0x00, 0x00},  // '='
    {0x06, 0x0C, 0x18, 0x30, 0x18, 0x0C, 0x06, 0x00},  // '>'
    {0x1E, 0x33, 0x30, 0x18, 0x0C, 0x00, 0x0C, 0x00},  // '?'
    {0x3E, 0x63, 0x7B, 0x7B, 0x7B, 0x03, 0x1E, 0x00},  // '@'
    {0x0C, 0x1E, 0x33, 0x33, 0x3F, 0x33, 0x33, 0x00},  // 'A'
    {0x3F, 0x66, 0x66, 0x3E, 0x66, 0x66, 0x3F, 0x00},  // 'B'
    {0x3C, 0x66, 0x03, 0x03, 0x03, 0x66, 0x3C, 0x00},  // 'C'
    {0x1F, 0x36, 0x66, 0x66, 0x66, 0x36, 0x1F, 0x00},  // 'D'
    {0x7F, 0x46, 0x16, 0x1E, 0x16, 0x46, 0x7F, 0x00},  // 'E'
    {0x7F, 0x46, 0x16, 0x1E, 0x16, 0x06, 0x0F, 0x00},  // 'F'
    {0x3C, 0x66, 0x03, 0x03, 0x73, 0x66, 0x7C, 0x00},  // 'G'
    {0x33, 0x33, 0x33, 0x3F, 0x33, 0x33, 0x33, 0x00},  // 'H'
    {0x1E, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00},  // 'I'
    {0x78, 0x30, 0x30, 0x30, 0x33, 0x33, 0x1E, 0x00},  // 'J'
    {0x67, 0x66, 0x36, 0x1E, 0x36, 0x66, 0x67, 0x00},  // 'K'
    {0x0F, 0x06, 0x06, 0x06, 0x46, 0x66, 0x7F, 0x00},  // 'L'
    {0x63, 0x77, 0x7F, 0x7F, 0x6B, 0x63, 0x63, 0x00},  // 'M'
    {0x63, 0x67, 0x6F, 0x7B, 0x73, 0x63, 0x63, 0x00},  // 'N'
    {0x1C, 0x36, 0x63, 0x63, 0x63, 0x36, 0x1C, 0x00},  // 'O'
    {0x3F, 0x66, 0x66, 0x3E, 0x06, 0x06, 0x0F, 0x00},  // 'P'
    {0x1E, 0x33, 0x33, 0x33, 0x3B, 0x1E, 0x38, 0x00},  // 'Q'
    {0x3F, 0x66, 0x66, 0x3E, 0x36, 0x66, 0x67, 0x00},  // 'R'
    {0x1E, 0x33, 0x07, 0x0E, 0x38, 0x33, 0x1E, 0x00},  // 'S'
    {0x3F, 0x2D, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00},  // 'T'
    {0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x3F, 0x00},  // 'U'
    {0x33, 0x33, 0x33, 0x33, 0x33, 0x1E, 0x0C, 0x00},  // 'V'
    {0x63, 0x63, 0x63, 0x6B, 0x7F, 0x77, 0x63, 0x00},  // 'W'
    {0x63, 0x63, 0x36, 0x1C, 0x1C, 0x36, 0x63, 0x00},  // 'X'
    {0x33, 0x33, 0x33, 0x1E, 0x0C, 0x0C, 0x1E, 0x00},  // 'Y'
    {0x7F, 0x63, 0x31, 0x18, 0x4C, 0x66, 0x7F, 0x00},  // 'Z'
    {0x1E, 0x06, 0x06, 0x06, 0x06, 0x06, 0x1E, 0x00},  // '['
    {0x03, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x40, 0x00},  // '\'
    {0x1E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x1E, 0x00},  // ']'
    {0x08, 0x1C, 0x36, 0x63, 0x00, 0x00, 0x00, 0x00},  // '^'
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF},  // '_'
    {0x0C, 0x0C, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00},  // '`'
    {0x00, 0x00, 0x1E, 0x30, 0x3E, 0x33, 0x6E, 0x00},  // 'a'
    {0x07, 0x06, 0x06, 0x3E, 0x66, 0x66, 0x3B, 0x00},  // 'b'
    {0x00, 0x00, 0x1E, 0x33, 0x03, 0x33, 0x1E, 0x00},  // 'c'
    {0x38, 0x30, 0x30, 0x3e, 0x33, 0x33, 0x6E, 0x00},  // 'd'
    {0x00, 0x00, 0x1E, 0x33, 0x3f, 0x03, 0x1E, 0x00},  // 'e'
    {0x1C, 0x36, 0x06, 0x0f, 0x06, 0x06, 0x0F, 0x00},  // 'f'
    {0x00, 0x00, 0x6E, 0x33, 0x33, 0x3E, 0x30, 0x1F},  // 'g'
    {0x07, 0x06, 0x36, 0x6E, 0x66, 0x66, 0x67, 0x00},  // 'h'
    {0x0C, 0x00, 0x0E, 0x0C, 0x0C, 0x0C, 0x1E, 0x00},  // 'i'
    {0x30, 0x00, 0x30, 0x30, 0x30, 0x33, 0x33, 0x1E},  // 'j'
    {0x07, 0x06, 0x66, 0x36, 0x1E, 0x36, 0x67, 0x00},  // 'k'
    {0x0E, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00},  // 'l'
    {0x00, 0x00, 0x33, 0x7F, 0x7F, 0x6B, 0x63, 0x00},  // 'm'
    {0x00, 0x00, 0x1F, 0x33, 0x33, 0x33, 0x33, 0x00},  // 'n'
    {0x00, 0x00, 0x1E, 0x33, 0x33, 0x33, 0x1E, 0x00},  // 'o'
    {0x00, 0x00, 0x3B, 0x66, 0x66, 0x3E, 0x06, 0x0F},  // 'p'
    {0x00, 0x00, 0x6E, 0x33, 0x33, 0x3E, 0x30, 0x78},  // 'q'
    {0x00, 0x00, 0x3B, 0x6E, 0x66, 0x06, 0x0F, 0x00},  // 'r'
    {0x00, 0x00, 0x3E, 0x03, 0x1E, 0x30, 0x1F, 0x00},  // 's'
    {0x08, 0x0C, 0x3E, 0x0C, 0x0C, 0x2C, 0x18, 0x00},  // 't'
    {0x00, 0x00, 0x33, 0x33, 0x33, 0x33, 0x6E, 0x00},  // 'u'
    {0x00, 0x00, 0x33, 0x33, 0x33, 0x1E, 0x0C, 0x00},  // 'v'
    {0x00, 0x00, 0x63, 0x6B, 0x7F, 0x7F, 0x36, 0x00},  // 'w'
    {0x00, 0x00, 0x63, 0x36, 0x1C, 0x36, 0x63, 0x00},  // 'x'
    {0x00, 0x00, 0x33, 0x33, 0x33, 0x3E, 0x30, 0x1F},  // 'y'
    {0x00, 0x00, 0x3F, 0x19, 0x0C, 0x26, 0x3F, 0x00},  // 'z'
    {0x38, 0x0C, 0x0C, 0x07, 0x0C, 0x0C, 0x38, 0x00},  // '{'
    {0x18, 0x18, 0x18, 0x00, 0x18, 0x18, 0x18, 0x00},  // '|'
    {0x07, 0x0C, 0x0C, 0x38, 0x0C, 0x0C, 0x07, 0x00},  // '}'
    {0x6E, 0x3B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},  // '~'
};

static void lcd_cmd(uint8_t cmd)
{
    gpio_set_level(LCD_PIN_DC, 0);  // Command mode
    spi_transaction_t t = {
        .length = 8,
        .tx_buffer = &cmd,
    };
    spi_device_transmit(spi_handle, &t);
}

static void lcd_data(const uint8_t *data, int len)
{
    gpio_set_level(LCD_PIN_DC, 1);  // Data mode
    spi_transaction_t t = {
        .length = len * 8,
        .tx_buffer = data,
    };
    spi_device_transmit(spi_handle, &t);
}

static void lcd_data_byte(uint8_t data)
{
    lcd_data(&data, 1);
}

// Initialize backlight PWM
static esp_err_t init_backlight_pwm(void)
{
    // Configure LEDC timer for backlight
    ledc_timer_config_t timer_conf = {
        .speed_mode = BL_LEDC_MODE,
        .duty_resolution = BL_LEDC_DUTY_RES,
        .timer_num = BL_LEDC_TIMER,
        .freq_hz = 1000,  // 1kHz PWM frequency (lower to avoid issues)
        .clk_cfg = LEDC_AUTO_CLK
    };
    esp_err_t ret = ledc_timer_config(&timer_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Backlight timer config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // Configure LEDC channel for backlight
    ledc_channel_config_t channel_conf = {
        .gpio_num = LCD_PIN_BL,
        .speed_mode = BL_LEDC_MODE,
        .channel = BL_LEDC_CHANNEL,
        .timer_sel = BL_LEDC_TIMER,
        .duty = BL_DUTY_BRIGHT,  // Start at full brightness
        .hpoint = 0
    };
    ret = ledc_channel_config(&channel_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Backlight channel config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    backlight_pwm_initialized = true;
    ESP_LOGI(TAG, "Backlight PWM initialized on GPIO %d", LCD_PIN_BL);
    return ESP_OK;
}

esp_err_t m5_display_init(m5_display_t *display)
{
    ESP_LOGI(TAG, "Initializing M5StickC Plus2 display...");

    // Initialize backlight PWM for dimming support
    esp_err_t ret = init_backlight_pwm();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Backlight PWM init failed");
        return ret;
    }

    // Configure DC and RST pins
    gpio_config_t ctrl_conf = {
        .pin_bit_mask = (1ULL << LCD_PIN_DC) | (1ULL << LCD_PIN_RST),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&ctrl_conf);

    // SPI bus configuration
    spi_bus_config_t buscfg = {
        .mosi_io_num = LCD_PIN_SDA,
        .miso_io_num = -1,
        .sclk_io_num = LCD_PIN_SCL,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_WIDTH * LCD_HEIGHT * 2,
    };
    ret = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI bus init failed");
        return ret;
    }

    // SPI device configuration
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = LCD_PIXEL_CLOCK_HZ,
        .mode = 0,
        .spics_io_num = LCD_PIN_CS,
        .queue_size = 7,
    };
    ret = spi_bus_add_device(SPI2_HOST, &devcfg, &spi_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI device add failed");
        return ret;
    }

    // Hardware reset
    gpio_set_level(LCD_PIN_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(LCD_PIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(100));

    // Initialize ST7789
    lcd_cmd(ST7789_SWRESET);
    vTaskDelay(pdMS_TO_TICKS(150));

    lcd_cmd(ST7789_SLPOUT);
    vTaskDelay(pdMS_TO_TICKS(10));

    lcd_cmd(ST7789_COLMOD);
    lcd_data_byte(0x55);  // 16-bit color

    lcd_cmd(ST7789_MADCTL);
    lcd_data_byte(0x70);  // MY=0, MX=1, MV=1, ML=1 for landscape 240x135

    lcd_cmd(ST7789_INVON);
    vTaskDelay(pdMS_TO_TICKS(10));

    lcd_cmd(ST7789_DISPON);
    vTaskDelay(pdMS_TO_TICKS(10));

    // Set display dimensions
    display->width = LCD_WIDTH;
    display->height = LCD_HEIGHT;

    // Allocate framebuffer
    display->framebuffer = (uint16_t *)heap_caps_malloc(LCD_WIDTH * LCD_HEIGHT * sizeof(uint16_t), MALLOC_CAP_DMA);
    if (display->framebuffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate framebuffer!");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Display initialized: %dx%d", display->width, display->height);

    // Test with white screen to verify SPI is working
    m5_display_clear(display, COLOR_WHITE);
    m5_display_flush(display);

    ESP_LOGI(TAG, "White screen test flushed");

    return ESP_OK;
}

void m5_display_clear(m5_display_t *display, uint16_t color)
{
    for (int i = 0; i < LCD_WIDTH * LCD_HEIGHT; i++) {
        display->framebuffer[i] = color;
    }
}

void m5_display_fill_rect(m5_display_t *display, int x, int y, int w, int h, uint16_t color)
{
    for (int j = y; j < y + h && j < LCD_HEIGHT; j++) {
        for (int i = x; i < x + w && i < LCD_WIDTH; i++) {
            if (i >= 0 && j >= 0) {
                display->framebuffer[j * LCD_WIDTH + i] = color;
            }
        }
    }
}

void m5_display_fill_circle(m5_display_t *display, int cx, int cy, int r, uint16_t color)
{
    for (int y = -r; y <= r; y++) {
        for (int x = -r; x <= r; x++) {
            if (x * x + y * y <= r * r) {
                int px = cx + x;
                int py = cy + y;
                if (px >= 0 && px < LCD_WIDTH && py >= 0 && py < LCD_HEIGHT) {
                    display->framebuffer[py * LCD_WIDTH + px] = color;
                }
            }
        }
    }
}

void m5_display_draw_sprite(m5_display_t *display, int x, int y, int w, int h, const uint16_t *data, uint16_t transparent_color)
{
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            uint16_t pixel = data[j * w + i];
            if (pixel != transparent_color) {
                int px = x + i;
                int py = y + j;
                if (px >= 0 && px < LCD_WIDTH && py >= 0 && py < LCD_HEIGHT) {
                    display->framebuffer[py * LCD_WIDTH + px] = pixel;
                }
            }
        }
    }
}

void m5_display_draw_sprite_scaled(m5_display_t *display, int x, int y, int w, int h, const uint16_t *data, uint16_t transparent_color, int scale)
{
    if (scale < 1) scale = 1;

    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            uint16_t pixel = data[j * w + i];
            if (pixel != transparent_color) {
                // Draw scaled pixel (scale x scale block)
                for (int sy = 0; sy < scale; sy++) {
                    for (int sx = 0; sx < scale; sx++) {
                        int px = x + (i * scale) + sx;
                        int py = y + (j * scale) + sy;
                        if (px >= 0 && px < LCD_WIDTH && py >= 0 && py < LCD_HEIGHT) {
                            display->framebuffer[py * LCD_WIDTH + px] = pixel;
                        }
                    }
                }
            }
        }
    }
}

void m5_display_draw_sprite_rotated(m5_display_t *display, int cx, int cy, int w, int h, const uint16_t *data, uint16_t transparent_color, int scale, float angle)
{
    if (scale < 1) scale = 1;

    float cos_a = cosf(angle);
    float sin_a = sinf(angle);

    int scaled_w = w * scale;
    int scaled_h = h * scale;
    int half_w = scaled_w / 2;
    int half_h = scaled_h / 2;

    // For each pixel in the output (bounding box around rotated sprite)
    int bound = (scaled_w > scaled_h ? scaled_w : scaled_h);

    for (int dy = -bound; dy < bound; dy++) {
        for (int dx = -bound; dx < bound; dx++) {
            // Rotate back to find source pixel
            float src_x = (dx * cos_a + dy * sin_a) + half_w;
            float src_y = (-dx * sin_a + dy * cos_a) + half_h;

            // Convert to source sprite coordinates
            int si = (int)(src_x / scale);
            int sj = (int)(src_y / scale);

            if (si >= 0 && si < w && sj >= 0 && sj < h) {
                uint16_t pixel = data[sj * w + si];
                if (pixel != transparent_color) {
                    int px = cx + dx;
                    int py = cy + dy;
                    if (px >= 0 && px < LCD_WIDTH && py >= 0 && py < LCD_HEIGHT) {
                        display->framebuffer[py * LCD_WIDTH + px] = pixel;
                    }
                }
            }
        }
    }
}

void m5_display_draw_char(m5_display_t *display, int x, int y, char c, uint16_t color, uint16_t bg)
{
    if (c < 32 || c > 126) return;

    const uint8_t *glyph = font8x8_basic[c - 32];

    for (int j = 0; j < 8; j++) {
        for (int i = 0; i < 8; i++) {
            int px = x + i;
            int py = y + j;

            if (px >= 0 && px < LCD_WIDTH && py >= 0 && py < LCD_HEIGHT) {
                uint16_t pixel_color = (glyph[j] & (1 << i)) ? color : bg;
                display->framebuffer[py * LCD_WIDTH + px] = pixel_color;
            }
        }
    }
}

void m5_display_draw_string(m5_display_t *display, int x, int y, const char *str, uint16_t color, uint16_t bg)
{
    int cx = x;
    int cy = y;

    while (*str) {
        if (*str == '\n') {
            cy += 10;
            cx = x;
        } else {
            m5_display_draw_char(display, cx, cy, *str, color, bg);
            cx += 8;

            if (cx + 8 > LCD_WIDTH) {
                cx = x;
                cy += 10;
            }
        }
        str++;
    }
}

void m5_display_draw_char_scaled(m5_display_t *display, int x, int y, char c, uint16_t color, uint16_t bg, int scale)
{
    if (c < 32 || c > 126 || scale < 1) return;

    const uint8_t *glyph = font8x8_basic[c - 32];

    for (int j = 0; j < 8; j++) {
        for (int i = 0; i < 8; i++) {
            uint16_t pixel_color = (glyph[j] & (1 << i)) ? color : bg;

            // Draw scaled pixel (scale x scale block)
            for (int sy = 0; sy < scale; sy++) {
                for (int sx = 0; sx < scale; sx++) {
                    int px = x + (i * scale) + sx;
                    int py = y + (j * scale) + sy;

                    if (px >= 0 && px < LCD_WIDTH && py >= 0 && py < LCD_HEIGHT) {
                        display->framebuffer[py * LCD_WIDTH + px] = pixel_color;
                    }
                }
            }
        }
    }
}

void m5_display_draw_string_scaled(m5_display_t *display, int x, int y, const char *str, uint16_t color, uint16_t bg, int scale)
{
    if (scale < 1) scale = 1;

    int cx = x;
    int cy = y;
    int char_width = 8 * scale;
    int line_height = 10 * scale;

    while (*str) {
        if (*str == '\n') {
            cy += line_height;
            cx = x;
        } else {
            m5_display_draw_char_scaled(display, cx, cy, *str, color, bg, scale);
            cx += char_width;

            if (cx + char_width > LCD_WIDTH) {
                cx = x;
                cy += line_height;
            }
        }
        str++;
    }
}

// Helper to interpolate between two RGB565 colors
static uint16_t interpolate_color(uint16_t c1, uint16_t c2, int step, int total_steps)
{
    if (total_steps <= 1) return c1;

    // Extract RGB565 components
    int r1 = (c1 >> 11) & 0x1F;
    int g1 = (c1 >> 5) & 0x3F;
    int b1 = c1 & 0x1F;

    int r2 = (c2 >> 11) & 0x1F;
    int g2 = (c2 >> 5) & 0x3F;
    int b2 = c2 & 0x1F;

    // Interpolate
    int r = r1 + ((r2 - r1) * step) / (total_steps - 1);
    int g = g1 + ((g2 - g1) * step) / (total_steps - 1);
    int b = b1 + ((b2 - b1) * step) / (total_steps - 1);

    return (r << 11) | (g << 5) | b;
}

void m5_display_draw_string_gradient(m5_display_t *display, int x, int y, const char *str, uint16_t color1, uint16_t color2, uint16_t bg)
{
    int len = 0;
    const char *p = str;
    while (*p) { len++; p++; }

    if (len == 0) return;

    int cx = x;
    int i = 0;

    while (*str) {
        if (*str == '\n') {
            // Don't handle newlines for gradient - just skip
        } else {
            uint16_t color = interpolate_color(color1, color2, i, len);
            m5_display_draw_char(display, cx, y, *str, color, bg);
            cx += 8;
            i++;
        }
        str++;
    }
}

void m5_display_flush(m5_display_t *display)
{
    // M5StickC Plus2 offsets for 240x135 in landscape mode
    uint16_t x_offset = 40;
    uint16_t y_offset = 53;  // Adjusted to eliminate bottom artifact

    // Set column address (with offset)
    lcd_cmd(ST7789_CASET);
    uint8_t caset[4] = {
        (x_offset) >> 8,
        (x_offset) & 0xFF,
        (x_offset + LCD_WIDTH - 1) >> 8,
        (x_offset + LCD_WIDTH - 1) & 0xFF
    };
    lcd_data(caset, 4);

    // Set row address (with offset)
    lcd_cmd(ST7789_RASET);
    uint8_t raset[4] = {
        (y_offset) >> 8,
        (y_offset) & 0xFF,
        (y_offset + LCD_HEIGHT - 1) >> 8,
        (y_offset + LCD_HEIGHT - 1) & 0xFF
    };
    lcd_data(raset, 4);

    // Write RAM - ST7789 expects big-endian RGB565, ESP32 is little-endian
    // Byte-swap the framebuffer before transmission
    lcd_cmd(ST7789_RAMWR);
    gpio_set_level(LCD_PIN_DC, 1);  // Data mode

    // SPI transaction with byte swap flag
    spi_transaction_t t = {
        .length = LCD_WIDTH * LCD_HEIGHT * 16,  // bits
        .tx_buffer = display->framebuffer,
        .flags = SPI_TRANS_USE_TXDATA ? 0 : 0,  // No special flags needed
    };

    // Manual byte swap for endianness correction
    uint16_t *fb = display->framebuffer;
    int pixel_count = LCD_WIDTH * LCD_HEIGHT;
    for (int i = 0; i < pixel_count; i++) {
        fb[i] = __builtin_bswap16(fb[i]);
    }

    spi_device_transmit(spi_handle, &t);

    // Swap back for next render
    for (int i = 0; i < pixel_count; i++) {
        fb[i] = __builtin_bswap16(fb[i]);
    }
}

// Set display brightness (dim mode on/off)
void m5_display_set_brightness(bool bright)
{
    brightness_high = bright;

    if (!backlight_pwm_initialized) {
        ESP_LOGW(TAG, "Backlight PWM not initialized");
        return;
    }

    // Set PWM duty cycle for brightness
    uint32_t duty = bright ? BL_DUTY_BRIGHT : BL_DUTY_DIM;
    ledc_set_duty(BL_LEDC_MODE, BL_LEDC_CHANNEL, duty);
    ledc_update_duty(BL_LEDC_MODE, BL_LEDC_CHANNEL);

    ESP_LOGI(TAG, "Brightness set to %s (duty: %lu)", bright ? "HIGH" : "DIM", duty);
}

// Get current brightness state
bool m5_display_get_brightness(void)
{
    return brightness_high;
}
