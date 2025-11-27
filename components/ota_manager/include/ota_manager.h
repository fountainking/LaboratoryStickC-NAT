#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <stdbool.h>
#include "esp_err.h"

#define GITHUB_API_URL "https://api.github.com/repos/fountainking/labPORTAL/releases/latest"
#define FIRMWARE_VERSION "v1.2.3"

/**
 * Check GitHub for new firmware release and perform OTA update if found
 * Returns ESP_OK if update completed successfully (will reboot)
 * Returns ESP_FAIL if no update available or error occurred
 */
esp_err_t ota_check_and_update(void);

/**
 * Perform OTA update from direct firmware URL
 * Returns ESP_OK on success (will reboot), ESP_FAIL on error
 */
esp_err_t ota_update_from_url(const char* firmware_url);

#endif // OTA_MANAGER_H
