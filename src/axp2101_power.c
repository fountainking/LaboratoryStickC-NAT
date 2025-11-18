#include "axp2101_power.h"
#include "esp_log.h"
#include "driver/gpio.h"

static const char *TAG = "M5Power";

void m5_power_init(void)
{
    ESP_LOGI(TAG, "Initializing M5StickC Plus2 power control...");

    // M5StickC Plus2 has NO PMIC (no AXP192/AXP2101)
    // Must set GPIO 4 (HOLD pin) to HIGH to keep power on after wake

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << M5_POWER_HOLD_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    gpio_config(&io_conf);

    // Set HOLD pin HIGH to keep power ON
    gpio_set_level(M5_POWER_HOLD_PIN, 1);

    ESP_LOGI(TAG, "✓ Power HOLD enabled (GPIO 4 = HIGH)");
    ESP_LOGI(TAG, "✓ Device will stay on when unplugged");
}

void m5_power_off(void)
{
    ESP_LOGI(TAG, "Powering off device...");

    // Set HOLD pin LOW to cut power
    gpio_set_level(M5_POWER_HOLD_PIN, 0);

    // Device will power off after this
    ESP_LOGI(TAG, "✓ Power off initiated");
}
