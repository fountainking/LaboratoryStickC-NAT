#include "sound_system.h"
#include "driver/ledc.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_log.h"

static const char *TAG = "Sound";

// Sound state
static bool sound_enabled = true;  // Default: ON
static bool initialized = false;

// LEDC configuration
#define LEDC_TIMER              LEDC_TIMER_0
#define LEDC_MODE               LEDC_LOW_SPEED_MODE
#define LEDC_CHANNEL            LEDC_CHANNEL_0
#define LEDC_DUTY_RES           LEDC_TIMER_10_BIT
#define LEDC_DUTY               (384)  // ~37% duty cycle for moderate volume

// Initialize sound system (buzzer PWM)
esp_err_t sound_system_init(void)
{
    ESP_LOGI(TAG, "Initializing sound system (buzzer on GPIO %d)", BUZZER_GPIO);

    // Configure LEDC timer
    ledc_timer_config_t timer_conf = {
        .speed_mode = LEDC_MODE,
        .duty_resolution = LEDC_DUTY_RES,
        .timer_num = LEDC_TIMER,
        .freq_hz = 2000,  // Default 2kHz
        .clk_cfg = LEDC_AUTO_CLK
    };
    esp_err_t ret = ledc_timer_config(&timer_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LEDC timer config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // Configure LEDC channel
    ledc_channel_config_t channel_conf = {
        .gpio_num = BUZZER_GPIO,
        .speed_mode = LEDC_MODE,
        .channel = LEDC_CHANNEL,
        .timer_sel = LEDC_TIMER,
        .duty = 0,  // Start silent
        .hpoint = 0
    };
    ret = ledc_channel_config(&channel_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LEDC channel config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    initialized = true;
    ESP_LOGI(TAG, "Sound system initialized");

    // Load preference from NVS
    sound_system_load_preference();

    return ESP_OK;
}

// Enable or disable sound
void sound_system_set_enabled(bool enabled)
{
    sound_enabled = enabled;
    ESP_LOGI(TAG, "Sound %s", enabled ? "ENABLED" : "DISABLED");
}

// Get current sound state
bool sound_system_is_enabled(void)
{
    return sound_enabled;
}

// Internal function to play a tone
static void play_tone(uint32_t freq_hz, uint32_t duration_ms)
{
    if (!initialized) {
        return;
    }

    // Set frequency
    ledc_set_freq(LEDC_MODE, LEDC_TIMER, freq_hz);

    // Turn on (50% duty)
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, LEDC_DUTY);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);

    // Wait
    vTaskDelay(pdMS_TO_TICKS(duration_ms));

    // Turn off
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, 0);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
}

// Play a sound (only if enabled)
void sound_system_play(sound_type_t type)
{
    if (!sound_enabled || !initialized) {
        return;
    }

    switch (type) {
        case SOUND_NAV:
            // Navigation click - soft tick
            play_tone(1500, 10);
            break;

        case SOUND_SELECT:
            // Selection click - slightly more pronounced
            play_tone(1800, 12);
            break;

        case SOUND_SUCCESS:
            // Success click - soft confirmation
            play_tone(2000, 10);
            break;

        case SOUND_ERROR:
            // Error double click - two soft ticks
            play_tone(1200, 12);
            vTaskDelay(pdMS_TO_TICKS(30));
            play_tone(1200, 12);
            break;

        case SOUND_CONNECT:
            // WiFi connected fanfare - triumphant A5-E6-A6!
            play_tone(880, 80);   // A5
            vTaskDelay(pdMS_TO_TICKS(30));
            play_tone(1319, 80);  // E6
            vTaskDelay(pdMS_TO_TICKS(30));
            play_tone(1760, 120); // A6 (hold it!)
            break;
    }
}

// Load sound preference from NVS
void sound_system_load_preference(void)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open("storage", NVS_READONLY, &nvs);
    if (err == ESP_OK) {
        uint8_t enabled = 1;  // Default: ON
        err = nvs_get_u8(nvs, "sound_enabled", &enabled);
        if (err == ESP_OK) {
            sound_enabled = (enabled != 0);
            ESP_LOGI(TAG, "Loaded sound preference from NVS: %s", sound_enabled ? "ON" : "OFF");
        } else {
            ESP_LOGI(TAG, "No saved sound preference, using default: ON");
        }
        nvs_close(nvs);
    }
}

// Save sound preference to NVS
void sound_system_save_preference(void)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &nvs);
    if (err == ESP_OK) {
        uint8_t enabled = sound_enabled ? 1 : 0;
        err = nvs_set_u8(nvs, "sound_enabled", enabled);
        if (err == ESP_OK) {
            nvs_commit(nvs);
            ESP_LOGI(TAG, "Saved sound preference to NVS: %s", sound_enabled ? "ON" : "OFF");
        }
        nvs_close(nvs);
    }
}
