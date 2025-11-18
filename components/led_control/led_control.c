#include "led_control.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <math.h>

static const char *TAG = "LED";

// LEDC configuration
#define LED_LEDC_TIMER      LEDC_TIMER_1
#define LED_LEDC_CHANNEL    LEDC_CHANNEL_1
#define LED_LEDC_SPEED      LEDC_LOW_SPEED_MODE
#define LED_LEDC_RESOLUTION LEDC_TIMER_8_BIT
#define LED_LEDC_FREQ_HZ    5000

static uint8_t current_brightness = 0;
static bool led_initialized = false;

esp_err_t led_init(void)
{
    if (led_initialized) {
        return ESP_OK;
    }

    // Configure LEDC timer
    ledc_timer_config_t timer_conf = {
        .speed_mode = LED_LEDC_SPEED,
        .duty_resolution = LED_LEDC_RESOLUTION,
        .timer_num = LED_LEDC_TIMER,
        .freq_hz = LED_LEDC_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK
    };

    esp_err_t ret = ledc_timer_config(&timer_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LEDC timer config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // Configure LEDC channel for LED
    // GPIO19 is active-low (LED cathode connected to GPIO, anode to 3.3V)
    ledc_channel_config_t channel_conf = {
        .gpio_num = LED_GPIO,
        .speed_mode = LED_LEDC_SPEED,
        .channel = LED_LEDC_CHANNEL,
        .timer_sel = LED_LEDC_TIMER,
        .duty = 255,  // Start OFF (inverted: 255 = off, 0 = full on)
        .hpoint = 0,
        .flags.output_invert = 1  // Invert output for active-low LED
    };

    ret = ledc_channel_config(&channel_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LEDC channel config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    led_initialized = true;
    current_brightness = 0;

    ESP_LOGI(TAG, "LED initialized on GPIO%d with PWM dimming", LED_GPIO);
    return ESP_OK;
}

void led_set_brightness(uint8_t brightness)
{
    if (!led_initialized) {
        ESP_LOGW(TAG, "LED not initialized");
        return;
    }

    current_brightness = brightness;

    // Set duty cycle (with output_invert flag, we can use brightness directly)
    ledc_set_duty(LED_LEDC_SPEED, LED_LEDC_CHANNEL, brightness);
    ledc_update_duty(LED_LEDC_SPEED, LED_LEDC_CHANNEL);
}

uint8_t led_get_brightness(void)
{
    return current_brightness;
}

void led_on(void)
{
    led_set_brightness(255);
}

void led_off(void)
{
    led_set_brightness(0);
}

void led_toggle(void)
{
    if (current_brightness > 0) {
        led_off();
    } else {
        led_on();
    }
}

void led_pulse(uint32_t speed_ms)
{
    if (!led_initialized) {
        return;
    }

    // Calculate brightness based on time (sine wave)
    int64_t now_us = esp_timer_get_time();
    float phase = (float)(now_us % (speed_ms * 1000)) / (speed_ms * 1000) * 2.0f * M_PI;

    // Sine wave from 0 to 1, scaled to 0-255
    uint8_t brightness = (uint8_t)((sinf(phase) + 1.0f) * 0.5f * 255.0f);

    led_set_brightness(brightness);
}
