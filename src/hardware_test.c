#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/i2c.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "esp_system.h"
#include <string.h>

static const char *TAG = "HW_TEST";

// Test all possible GPIO pins (SKIP 23 - causes WDT reset, likely display critical)
#define TEST_ALL_GPIOS {0, 2, 4, 5, 12, 13, 14, 15, 18, 19, 21, 22, 25, 26, 27, 32, 33, 34, 35, 36, 39}

// Display test pins (trying all combinations)
typedef struct {
    int mosi;
    int sclk;
    int cs;
    int dc;
    int rst;
    int bl;
} display_pin_combo_t;

static display_pin_combo_t display_combos[] = {
    // SKIP GPIO 23 - causes watchdog reset!
    // {15, 13, 5, 23, 18, 27},  // COMBO 1 - DISABLED (uses GPIO 23)
    {15, 13, 14, 27, 33, 32},     // Try this combo
    {15, 13, 5, 14, 12, 27},      // And this one
    {15, 13, 5, 14, 18, 27},      // Another variant
};

void test_gpio_output(int pin)
{
    ESP_LOGI(TAG, ">>> Testing GPIO %d as OUTPUT", pin);

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << pin),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "    GPIO %d config FAILED: %s", pin, esp_err_to_name(ret));
        return;
    }

    // Toggle it
    gpio_set_level(pin, 1);
    ESP_LOGI(TAG, "    GPIO %d set HIGH", pin);
    vTaskDelay(pdMS_TO_TICKS(100));

    gpio_set_level(pin, 0);
    ESP_LOGI(TAG, "    GPIO %d set LOW", pin);
    vTaskDelay(pdMS_TO_TICKS(100));

    ESP_LOGI(TAG, "    GPIO %d test COMPLETE", pin);
}

void test_i2c_bus(void)
{
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, ">>> Testing I2C Bus (SDA=21, SCL=22)");
    ESP_LOGI(TAG, "========================================");

    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = 21,
        .scl_io_num = 22,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000,
    };

    esp_err_t ret = i2c_param_config(I2C_NUM_0, &conf);
    if (ret == ESP_OK) {
        ret = i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "    I2C initialized successfully");

            // Scan for devices
            ESP_LOGI(TAG, "    Scanning I2C bus...");
            for (int addr = 1; addr < 127; addr++) {
                i2c_cmd_handle_t cmd = i2c_cmd_link_create();
                i2c_master_start(cmd);
                i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
                i2c_master_stop(cmd);

                ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(50));
                i2c_cmd_link_delete(cmd);

                if (ret == ESP_OK) {
                    ESP_LOGI(TAG, "    *** FOUND I2C DEVICE at address 0x%02X ***", addr);
                }
            }

            i2c_driver_delete(I2C_NUM_0);
        } else {
            ESP_LOGW(TAG, "    I2C driver install FAILED: %s", esp_err_to_name(ret));
        }
    } else {
        ESP_LOGW(TAG, "    I2C config FAILED: %s", esp_err_to_name(ret));
    }
}

void test_buzzer_pwm(void)
{
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, ">>> Testing Buzzer/Speaker PWM (trying GPIO 2, 25, 26)");
    ESP_LOGI(TAG, "========================================");

    int buzzer_pins[] = {2, 25, 26};

    for (int i = 0; i < 3; i++) {
        int pin = buzzer_pins[i];
        ESP_LOGI(TAG, "    Testing GPIO %d as buzzer...", pin);

        ledc_timer_config_t timer_conf = {
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .duty_resolution = LEDC_TIMER_10_BIT,
            .timer_num = LEDC_TIMER_0,
            .freq_hz = 2000, // 2kHz tone
            .clk_cfg = LEDC_AUTO_CLK
        };
        ledc_timer_config(&timer_conf);

        ledc_channel_config_t channel_conf = {
            .gpio_num = pin,
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel = LEDC_CHANNEL_0,
            .timer_sel = LEDC_TIMER_0,
            .duty = 512, // 50% duty
            .hpoint = 0
        };

        esp_err_t ret = ledc_channel_config(&channel_conf);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "    Playing 2kHz tone on GPIO %d...", pin);
            vTaskDelay(pdMS_TO_TICKS(200));
            ledc_stop(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
            ESP_LOGI(TAG, "    GPIO %d buzzer test COMPLETE", pin);
        } else {
            ESP_LOGW(TAG, "    GPIO %d buzzer config FAILED", pin);
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void test_display_combo(display_pin_combo_t combo, int combo_num)
{
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, ">>> Testing Display Combo #%d", combo_num);
    ESP_LOGI(TAG, "    MOSI=%d, SCLK=%d, CS=%d, DC=%d, RST=%d, BL=%d",
             combo.mosi, combo.sclk, combo.cs, combo.dc, combo.rst, combo.bl);
    ESP_LOGI(TAG, "========================================");

    // Configure backlight
    gpio_config_t bl_conf = {
        .pin_bit_mask = (1ULL << combo.bl),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&bl_conf);
    gpio_set_level(combo.bl, 1);
    ESP_LOGI(TAG, "    Backlight GPIO %d turned ON", combo.bl);

    // Configure control pins
    gpio_config_t ctrl_conf = {
        .pin_bit_mask = (1ULL << combo.dc) | (1ULL << combo.rst),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&ctrl_conf);

    // SPI configuration
    spi_bus_config_t buscfg = {
        .mosi_io_num = combo.mosi,
        .miso_io_num = -1,
        .sclk_io_num = combo.sclk,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 240 * 135 * 2,
    };

    esp_err_t ret = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "    SPI bus init FAILED: %s", esp_err_to_name(ret));
        gpio_set_level(combo.bl, 0);
        return;
    }

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 40 * 1000 * 1000,
        .mode = 0,
        .spics_io_num = combo.cs,
        .queue_size = 7,
    };

    spi_device_handle_t spi;
    ret = spi_bus_add_device(SPI2_HOST, &devcfg, &spi);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "    SPI device add FAILED: %s", esp_err_to_name(ret));
        spi_bus_free(SPI2_HOST);
        gpio_set_level(combo.bl, 0);
        return;
    }

    ESP_LOGI(TAG, "    SPI initialized successfully");

    // Hardware reset
    gpio_set_level(combo.rst, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(combo.rst, 1);
    vTaskDelay(pdMS_TO_TICKS(100));
    ESP_LOGI(TAG, "    Display reset sequence sent");

    // Try to send ST7789 init commands
    uint8_t cmd;

    // Software reset
    cmd = 0x01;
    gpio_set_level(combo.dc, 0);
    spi_transaction_t t = {.length = 8, .tx_buffer = &cmd};
    spi_device_transmit(spi, &t);
    ESP_LOGI(TAG, "    Sent SW RESET command (0x01)");
    vTaskDelay(pdMS_TO_TICKS(150));

    // Sleep out
    cmd = 0x11;
    gpio_set_level(combo.dc, 0);
    t.tx_buffer = &cmd;
    spi_device_transmit(spi, &t);
    ESP_LOGI(TAG, "    Sent SLEEP OUT command (0x11)");
    vTaskDelay(pdMS_TO_TICKS(10));

    // Display on
    cmd = 0x29;
    gpio_set_level(combo.dc, 0);
    t.tx_buffer = &cmd;
    spi_device_transmit(spi, &t);
    ESP_LOGI(TAG, "    Sent DISPLAY ON command (0x29)");

    ESP_LOGI(TAG, "    Combo #%d test COMPLETE - check screen for activity!", combo_num);
    ESP_LOGI(TAG, "    Waiting 2 seconds with display on...");
    vTaskDelay(pdMS_TO_TICKS(2000));

    // Cleanup
    spi_bus_remove_device(spi);
    spi_bus_free(SPI2_HOST);
    gpio_set_level(combo.bl, 0);
}

void run_hardware_tests(void)
{
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "################################################################");
    ESP_LOGI(TAG, "###                                                          ###");
    ESP_LOGI(TAG, "###    LABORATORY M5StickC Plus2 HARDWARE TEST SUITE        ###");
    ESP_LOGI(TAG, "###                                                          ###");
    ESP_LOGI(TAG, "################################################################");
    ESP_LOGI(TAG, "");

    vTaskDelay(pdMS_TO_TICKS(1000));

    // Test 1: I2C devices
    test_i2c_bus();
    vTaskDelay(pdMS_TO_TICKS(500));

    // Test 2: Buzzer/PWM
    test_buzzer_pwm();
    vTaskDelay(pdMS_TO_TICKS(500));

    // Test 3: All GPIO outputs
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, ">>> Testing All GPIO Pins as OUTPUT");
    ESP_LOGI(TAG, "========================================");

    int test_gpios[] = TEST_ALL_GPIOS;
    for (int i = 0; i < sizeof(test_gpios) / sizeof(int); i++) {
        test_gpio_output(test_gpios[i]);
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    // Test 4: Display combinations
    for (int i = 0; i < sizeof(display_combos) / sizeof(display_pin_combo_t); i++) {
        test_display_combo(display_combos[i], i + 1);
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "################################################################");
    ESP_LOGI(TAG, "###                                                          ###");
    ESP_LOGI(TAG, "###              ALL HARDWARE TESTS COMPLETE                 ###");
    ESP_LOGI(TAG, "###                                                          ###");
    ESP_LOGI(TAG, "###                  REBOOTING IN 5 SECONDS                  ###");
    ESP_LOGI(TAG, "###                                                          ###");
    ESP_LOGI(TAG, "################################################################");
    ESP_LOGI(TAG, "");

    vTaskDelay(pdMS_TO_TICKS(5000));
    esp_restart();
}
