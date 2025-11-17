#include "ota_manager.h"
#include <string.h>
#include "esp_log.h"
#include "esp_https_ota.h"
#include "esp_http_client.h"
#include "esp_ota_ops.h"

static const char* TAG = "OTA";

// Simple JSON parser helper - extracts value of "field":"value" pattern
static char* parse_json_field(const char* json, const char* field, char* out_buf, size_t buf_size) {
    char search[128];
    snprintf(search, sizeof(search), "\"%s\":\"", field);

    const char* start = strstr(json, search);
    if (!start) return NULL;

    start += strlen(search);
    const char* end = strchr(start, '"');
    if (!end) return NULL;

    size_t len = end - start;
    if (len >= buf_size) len = buf_size - 1;

    memcpy(out_buf, start, len);
    out_buf[len] = '\0';
    return out_buf;
}

// HTTP event handler for GitHub API fetch
static esp_err_t http_event_handler(esp_http_client_event_t *evt) {
    static char response_buffer[4096];
    static int response_len = 0;

    switch(evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            if (response_len + evt->data_len < sizeof(response_buffer)) {
                memcpy(response_buffer + response_len, evt->data, evt->data_len);
                response_len += evt->data_len;
                response_buffer[response_len] = '\0';
                *(char**)evt->user_data = response_buffer;
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            response_len = 0;
            break;
        case HTTP_EVENT_DISCONNECTED:
            response_len = 0;
            break;
        default:
            break;
    }
    return ESP_OK;
}

esp_err_t ota_check_and_update(void) {
    ESP_LOGI(TAG, "Checking for updates... (Current: %s)", FIRMWARE_VERSION);

    // Fetch GitHub API
    char* response_data = NULL;
    esp_http_client_config_t api_config = {
        .url = GITHUB_API_URL,
        .event_handler = http_event_handler,
        .user_data = &response_data,
        .timeout_ms = 30000,
        .skip_cert_common_name_check = true,
    };

    esp_http_client_handle_t client = esp_http_client_init(&api_config);
    esp_http_client_set_header(client, "User-Agent", "ESP32-Laboratory");

    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (err != ESP_OK || status != 200) {
        ESP_LOGE(TAG, "GitHub API request failed: %s (HTTP %d)", esp_err_to_name(err), status);
        return ESP_FAIL;
    }

    if (!response_data) {
        ESP_LOGE(TAG, "Empty response from GitHub API");
        return ESP_FAIL;
    }

    // Parse version tag
    char latest_version[32];
    if (!parse_json_field(response_data, "tag_name", latest_version, sizeof(latest_version))) {
        ESP_LOGE(TAG, "Failed to parse tag_name from GitHub response");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Latest version: %s", latest_version);

    // Compare versions
    if (strcmp(latest_version, FIRMWARE_VERSION) == 0) {
        ESP_LOGI(TAG, "Already up to date!");
        return ESP_FAIL;
    }

    // Find firmware.bin download URL
    char firmware_url[256];
    if (!parse_json_field(response_data, "browser_download_url", firmware_url, sizeof(firmware_url))) {
        ESP_LOGE(TAG, "Failed to parse browser_download_url");
        return ESP_FAIL;
    }

    // Verify it's a firmware.bin file
    if (!strstr(firmware_url, "firmware.bin")) {
        ESP_LOGE(TAG, "Download URL doesn't contain firmware.bin: %s", firmware_url);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "New firmware available! Downloading from: %s", firmware_url);
    return ota_update_from_url(firmware_url);
}

esp_err_t ota_update_from_url(const char* firmware_url) {
    ESP_LOGI(TAG, "Starting OTA update from: %s", firmware_url);

    esp_http_client_config_t ota_config = {
        .url = firmware_url,
        .timeout_ms = 60000,
        .keep_alive_enable = true,
        .skip_cert_common_name_check = true,
    };

    esp_https_ota_config_t ota_https_config = {
        .http_config = &ota_config,
    };

    esp_err_t err = esp_https_ota(&ota_https_config);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "OTA update successful! Rebooting...");
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_restart();
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "OTA update failed: %s", esp_err_to_name(err));
        return ESP_FAIL;
    }
}
