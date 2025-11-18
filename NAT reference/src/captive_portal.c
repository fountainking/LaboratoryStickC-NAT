#include "captive_portal.h"
#include "log_capture.h"
#include "ota_manager.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_ota_ops.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG = "Portal";

// Buffer for serving logs (16KB should be plenty)
#define LOG_RESPONSE_BUFFER_SIZE (16 * 1024)

// Laboratory portal HTML with WiFi setup
static const char LABORATORY_HTML[] =
"<!DOCTYPE html>"
"<html>"
"<head>"
"<meta charset=\"UTF-8\">"
"<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">"
"<title>Laboratory</title>"
"<link rel=\"icon\" href=\"data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 100 100'><text y='.9em' font-size='90'>⭐</text></svg>\">"
"<style>"
"*{margin:0;padding:0;box-sizing:border-box;}"
"body{font-family:-apple-system,BlinkMacSystemFont,\"Segoe UI\",Roboto,sans-serif;margin:0;padding:30px 20px;background:#f5f5f5;color:#000;min-height:100vh;display:flex;flex-direction:column;align-items:center;justify-content:flex-start;}"
".frame{max-width:600px;width:100%;background:#fff;border:3px solid #000;border-radius:40px;padding:35px 40px;margin-bottom:30px;}"
".description{text-align:center;line-height:1.7;font-size:1.05em;color:#000;}"
"input,select{width:100%;padding:12px;margin:10px 0;border:2px solid #000;border-radius:10px;font-size:1em;font-family:inherit;}"
"button{width:100%;padding:15px;background:#000;color:#fff;border:none;border-radius:10px;font-size:1.1em;cursor:pointer;margin-top:10px;font-family:inherit;}"
"button:hover{background:#333;}"
"button.secondary{background:#fff;color:#000;border:2px solid #000;}"
"button.secondary:hover{background:#f5f5f5;}"
".status{text-align:center;margin-top:15px;font-weight:bold;padding:15px;border-radius:10px;}"
".status.success{background:#d4edda;color:#155724;border:2px solid #28a745;}"
".status.connecting{background:#fff3cd;color:#856404;border:2px solid #ffc107;}"
".cta-button{display:block;background:#fff;color:#000;border:3px solid #000;border-radius:50px;padding:15px 40px;font-size:1.1em;font-weight:bold;text-align:center;margin:20px auto;max-width:350px;cursor:pointer;text-decoration:none;}"
".cta-button:hover{background:#f5f5f5;}"
".network{padding:12px;margin:8px 0;border:2px solid #000;border-radius:10px;cursor:pointer;display:flex;justify-content:space-between;align-items:center;}"
".network:hover{background:#f5f5f5;}"
".network.selected{background:#000;color:#fff;}"
"h1{text-align:center;font-size:2.5em;margin-bottom:20px;}"
"h2{text-align:center;font-size:1.5em;margin-bottom:15px;}"
"#networks{max-height:300px;overflow-y:auto;margin:10px 0;}"
".loading{text-align:center;padding:20px;}"
"</style>"
"</head>"
"<body>"
"<h1>LABORATORY</h1>"
"<div class=\"frame\">"
"<h2>WiFi Setup</h2>"
"<div id='scanView'>"
"<button onclick='scanNetworks()' class='secondary'>🔍 Scan for Networks</button>"
"<div id='networks'></div>"
"</div>"
"<form id='wifiForm' style='display:none;'>"
"<div id='selectedNetwork' style='margin:10px 0;font-weight:bold;'></div>"
"<input type='password' name='password' placeholder='WiFi Password' required>"
"<button type='submit'>Connect</button>"
"<button type='button' onclick='showScan()' class='secondary' style='margin-top:5px;'>Cancel</button>"
"</form>"
"<div class='status' id='status'></div>"
"</div>"
"<a href='/update' class='cta-button'>📡 OTA Update</a>"
"<a href='https://laboratory.mx' class='cta-button'>Visit Laboratory.mx</a>"
"<script>"
"let selectedSSID = '';"
"async function scanNetworks() {"
"  document.getElementById('networks').innerHTML = '<div class=\"loading\">Scanning...</div>';"
"  const res = await fetch('/wifi/scan');"
"  const networks = await res.json();"
"  let html = '';"
"  networks.forEach(net => {"
"    html += `<div class='network' onclick='selectNetwork(\"${net.ssid}\",${net.auth})'><span>${net.ssid}</span><span>${net.rssi} dBm ${net.auth ? '🔒' : ''}</span></div>`;"
"  });"
"  document.getElementById('networks').innerHTML = html || '<div class=\"loading\">No networks found</div>';"
"}"
"function selectNetwork(ssid, needsAuth) {"
"  selectedSSID = ssid;"
"  if (!needsAuth) {"
"    connectWiFi(ssid, '');"
"  } else {"
"    document.getElementById('scanView').style.display = 'none';"
"    document.getElementById('wifiForm').style.display = 'block';"
"    document.getElementById('selectedNetwork').textContent = 'Network: ' + ssid;"
"    document.querySelector('[name=password]').value = '';"
"    document.querySelector('[name=password]').focus();"
"  }"
"}"
"function showScan() {"
"  document.getElementById('scanView').style.display = 'block';"
"  document.getElementById('wifiForm').style.display = 'none';"
"}"
"async function connectWiFi(ssid, password) {"
"  const data = 'ssid=' + encodeURIComponent(ssid) + '&password=' + encodeURIComponent(password);"
"  const status = document.getElementById('status');"
"  status.textContent = 'Connecting...';"
"  status.className = 'status connecting';"
"  const response = await fetch('/wifi/connect', { method: 'POST', headers: {'Content-Type': 'application/x-www-form-urlencoded'}, body: data });"
"  const result = await response.text();"
"  status.textContent = result;"
"  status.className = 'status success';"
"}"
"document.getElementById('wifiForm').onsubmit = async (e) => {"
"  e.preventDefault();"
"  const password = e.target.password.value;"
"  connectWiFi(selectedSSID, password);"
"};"
"window.onload = () => scanNetworks();"
"</script>"
"</body>"
"</html>";

// Root handler - serves the Laboratory portal
static esp_err_t root_handler(httpd_req_t *req)
{
    httpd_resp_send(req, LABORATORY_HTML, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static const httpd_uri_t root_uri = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = root_handler,
    .user_ctx  = NULL
};

// Debug logs handler - serves captured logs as plain text
static esp_err_t logs_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Serving debug logs");

    // Allocate buffer for logs
    char *log_buffer = malloc(LOG_RESPONSE_BUFFER_SIZE);
    if (!log_buffer) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // Get all captured logs
    size_t log_size = log_capture_get_all(log_buffer, LOG_RESPONSE_BUFFER_SIZE);

    if (log_size == 0) {
        snprintf(log_buffer, LOG_RESPONSE_BUFFER_SIZE, "No logs captured yet.\n");
    }

    // Send as plain text
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_send(req, log_buffer, HTTPD_RESP_USE_STRLEN);

    free(log_buffer);
    return ESP_OK;
}

static const httpd_uri_t logs_uri = {
    .uri       = "/debug/logs",
    .method    = HTTP_GET,
    .handler   = logs_handler,
    .user_ctx  = NULL
};

// Recent logs handler - serves last N lines (default 20)
static esp_err_t logs_recent_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Serving recent logs");

    char *log_buffer = malloc(LOG_RESPONSE_BUFFER_SIZE);
    if (!log_buffer) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // Get query parameter for number of lines
    char query[32];
    int num_lines = 20; // default
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        char param[16];
        if (httpd_query_key_value(query, "lines", param, sizeof(param)) == ESP_OK) {
            num_lines = atoi(param);
        }
    }

    // Get recent logs
    size_t log_size = log_capture_get_recent(log_buffer, LOG_RESPONSE_BUFFER_SIZE, num_lines);

    if (log_size == 0) {
        snprintf(log_buffer, LOG_RESPONSE_BUFFER_SIZE, "No logs captured yet.\n");
    }

    httpd_resp_set_type(req, "text/plain");
    httpd_resp_send(req, log_buffer, HTTPD_RESP_USE_STRLEN);

    free(log_buffer);
    return ESP_OK;
}

static const httpd_uri_t logs_recent_uri = {
    .uri       = "/debug/recent",
    .method    = HTTP_GET,
    .handler   = logs_recent_handler,
    .user_ctx  = NULL
};

// OTA update handler - accepts firmware binary via POST
static esp_err_t ota_upload_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "OTA upload started, content length: %d bytes", req->content_len);

    if (req->content_len <= 0 || req->content_len > 2 * 1024 * 1024) { // Max 2MB
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid firmware size");
        return ESP_FAIL;
    }

    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
    if (!update_partition) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No OTA partition found");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Writing to partition: %s at offset 0x%lx", update_partition->label, (unsigned long)update_partition->address);

    esp_ota_handle_t ota_handle;
    esp_err_t err = esp_ota_begin(update_partition, req->content_len, &ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "OTA begin failed: %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA begin failed");
        return ESP_FAIL;
    }

    char buf[1024];
    int received = 0;
    int remaining = req->content_len;

    while (remaining > 0) {
        size_t read_size = (remaining < sizeof(buf)) ? remaining : sizeof(buf);
        int recv_len = httpd_req_recv(req, buf, read_size);
        if (recv_len < 0) {
            if (recv_len == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            }
            esp_ota_abort(ota_handle);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Upload interrupted");
            return ESP_FAIL;
        }

        err = esp_ota_write(ota_handle, buf, recv_len);
        if (err != ESP_OK) {
            esp_ota_abort(ota_handle);
            ESP_LOGE(TAG, "OTA write failed: %s", esp_err_to_name(err));
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA write failed");
            return ESP_FAIL;
        }

        received += recv_len;
        remaining -= recv_len;

        // Log progress every 64KB
        if (received % (64 * 1024) == 0) {
            ESP_LOGI(TAG, "OTA progress: %d / %d bytes", received, req->content_len);
        }
    }

    err = esp_ota_end(ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "OTA end failed: %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA validation failed");
        return ESP_FAIL;
    }

    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Set boot partition failed: %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to set boot partition");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "OTA upload successful! Rebooting in 3 seconds...");

    const char* success_html = "<!DOCTYPE html><html><head><title>OTA Success</title></head>"
                               "<body style='font-family:sans-serif;text-align:center;padding:50px;'>"
                               "<h1>✅ Update Successful!</h1>"
                               "<p>Device will reboot in 3 seconds...</p>"
                               "</body></html>";

    httpd_resp_send(req, success_html, HTTPD_RESP_USE_STRLEN);

    // Reboot after response
    vTaskDelay(pdMS_TO_TICKS(3000));
    esp_restart();

    return ESP_OK;
}

static const httpd_uri_t ota_upload_uri = {
    .uri       = "/ota",
    .method    = HTTP_POST,
    .handler   = ota_upload_handler,
    .user_ctx  = NULL
};

// OTA page handler - serves upload form
static esp_err_t ota_page_handler(httpd_req_t *req)
{
    const char* ota_html =
    "<!DOCTYPE html><html><head>"
    "<meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1.0'>"
    "<title>Laboratory OTA</title>"
    "<style>"
    "*{margin:0;padding:0;box-sizing:border-box;}"
    "body{font-family:sans-serif;padding:30px;background:#f5f5f5;}"
    ".container{max-width:600px;margin:0 auto;background:#fff;border:3px solid #000;border-radius:20px;padding:40px;}"
    "h1{text-align:center;margin-bottom:20px;}"
    ".info{background:#f0f0f0;padding:15px;border-radius:10px;margin-bottom:20px;}"
    "input[type=file]{display:block;width:100%;padding:10px;margin:20px 0;border:2px solid #000;border-radius:10px;}"
    "button{width:100%;padding:15px;background:#000;color:#fff;border:none;border-radius:10px;font-size:1.1em;cursor:pointer;}"
    "button:hover{background:#333;}"
    ".progress{display:none;margin-top:20px;}"
    ".progress-bar{width:100%;height:30px;background:#f0f0f0;border-radius:15px;overflow:hidden;}"
    ".progress-fill{height:100%;background:#000;transition:width 0.3s;}"
    "</style>"
    "</head><body>"
    "<div class='container'>"
    "<h1>🔧 Laboratory OTA Update</h1>"
    "<div class='info'><strong>Current Version:</strong> " FIRMWARE_VERSION "</div>"
    "<form id='otaForm'>"
    "<input type='file' id='firmware' accept='.bin' required>"
    "<button type='submit'>Upload Firmware</button>"
    "</form>"
    "<div class='progress' id='progress'>"
    "<p>Uploading...</p>"
    "<div class='progress-bar'><div class='progress-fill' id='progressFill'></div></div>"
    "</div>"
    "</div>"
    "<script>"
    "document.getElementById('otaForm').onsubmit = async (e) => {"
    "  e.preventDefault();"
    "  const file = document.getElementById('firmware').files[0];"
    "  if (!file) return;"
    "  document.getElementById('progress').style.display = 'block';"
    "  const response = await fetch('/ota', { method: 'POST', body: file });"
    "  if (response.ok) {"
    "    alert('Update successful! Device rebooting...');"
    "  } else {"
    "    alert('Update failed: ' + await response.text());"
    "    document.getElementById('progress').style.display = 'none';"
    "  }"
    "};"
    "</script>"
    "</body></html>";

    httpd_resp_send(req, ota_html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static const httpd_uri_t ota_page_uri = {
    .uri       = "/update",
    .method    = HTTP_GET,
    .handler   = ota_page_handler,
    .user_ctx  = NULL
};

// WiFi connect handler - saves credentials and reconnects STA
// WiFi scan handler - returns JSON list of available networks
static esp_err_t wifi_scan_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "WiFi scan request");

    // Trigger WiFi scan
    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = false
    };

    esp_err_t err = esp_wifi_scan_start(&scan_config, true); // blocking scan
    if (err != ESP_OK) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // Get scan results
    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);

    if (ap_count == 0) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "[]", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }

    wifi_ap_record_t *ap_list = malloc(sizeof(wifi_ap_record_t) * ap_count);
    if (!ap_list) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    esp_wifi_scan_get_ap_records(&ap_count, ap_list);

    // Build JSON response
    char *json_buf = malloc(4096);
    if (!json_buf) {
        free(ap_list);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    int offset = 0;
    offset += snprintf(json_buf + offset, 4096 - offset, "[");

    for (int i = 0; i < ap_count && i < 20; i++) { // Limit to 20 networks
        if (i > 0) offset += snprintf(json_buf + offset, 4096 - offset, ",");
        offset += snprintf(json_buf + offset, 4096 - offset,
            "{\"ssid\":\"%s\",\"rssi\":%d,\"auth\":%s}",
            ap_list[i].ssid,
            ap_list[i].rssi,
            ap_list[i].authmode == WIFI_AUTH_OPEN ? "false" : "true"
        );
    }

    offset += snprintf(json_buf + offset, 4096 - offset, "]");

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_buf, HTTPD_RESP_USE_STRLEN);

    free(json_buf);
    free(ap_list);
    return ESP_OK;
}

static const httpd_uri_t wifi_scan_uri = {
    .uri       = "/wifi/scan",
    .method    = HTTP_GET,
    .handler   = wifi_scan_handler,
    .user_ctx  = NULL
};

static esp_err_t wifi_connect_handler(httpd_req_t *req)
{
    char buf[256];
    int ret, remaining = req->content_len;

    if (remaining >= sizeof(buf)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Payload too large");
        return ESP_FAIL;
    }

    ret = httpd_req_recv(req, buf, remaining);
    if (ret <= 0) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive data");
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    // Parse form data: ssid=xxx&password=yyy
    char ssid[33] = {0};
    char password[64] = {0};

    char *ssid_start = strstr(buf, "ssid=");
    char *pass_start = strstr(buf, "password=");

    if (!ssid_start || !pass_start) {
        httpd_resp_send(req, "Missing SSID or password", HTTPD_RESP_USE_STRLEN);
        return ESP_FAIL;
    }

    // URL decode and extract SSID
    ssid_start += 5; // Skip "ssid="
    char *ssid_end = strchr(ssid_start, '&');
    if (ssid_end) {
        int len = ssid_end - ssid_start;
        if (len > 32) len = 32;
        strncpy(ssid, ssid_start, len);
        // Simple URL decode for spaces
        for (int i = 0; ssid[i]; i++) {
            if (ssid[i] == '+') ssid[i] = ' ';
        }
    }

    // URL decode and extract password
    pass_start += 9; // Skip "password="
    strncpy(password, pass_start, 63);
    // Simple URL decode for spaces
    for (int i = 0; password[i]; i++) {
        if (password[i] == '+') password[i] = ' ';
    }

    ESP_LOGI(TAG, "WiFi connect request: SSID='%s'", ssid);

    // Save to NVS
    nvs_handle_t nvs;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &nvs);
    if (err == ESP_OK) {
        nvs_set_str(nvs, "wifi_ssid", ssid);
        nvs_set_str(nvs, "wifi_pass", password);
        nvs_commit(nvs);
        nvs_close(nvs);
        ESP_LOGI(TAG, "WiFi credentials saved to NVS");
    }

    // Update WiFi config and reconnect
    wifi_config_t wifi_config = {0};
    strncpy((char*)wifi_config.sta.ssid, ssid, 32);
    strncpy((char*)wifi_config.sta.password, password, 64);

    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_disconnect();
    esp_wifi_connect();

    httpd_resp_send(req, "✅ SUCCESS! Connecting to WiFi network. You can close this page.", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static const httpd_uri_t wifi_connect_uri = {
    .uri       = "/wifi/connect",
    .method    = HTTP_POST,
    .handler   = wifi_connect_handler,
    .user_ctx  = NULL
};

// Captive portal detection handlers
// Android connectivity checks - MUST return 204 No Content for detection to work
static esp_err_t generate_204_handler(httpd_req_t *req)
{
    // Return portal HTML with 200 OK to trigger captive popup
    // Android expects something OTHER than 204 to know there's a portal
    httpd_resp_send(req, LABORATORY_HTML, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static const httpd_uri_t generate_204_uri = {
    .uri       = "/generate_204",
    .method    = HTTP_GET,
    .handler   = generate_204_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t gen_204_uri = {
    .uri       = "/gen_204",
    .method    = HTTP_GET,
    .handler   = generate_204_handler,
    .user_ctx  = NULL
};

// iOS/Apple captive portal detection - expects "Success" or HTML to trigger portal
static esp_err_t hotspot_detect_handler(httpd_req_t *req)
{
    // iOS detects portal when response is NOT "Success"
    // Returning HTML triggers the captive portal browser
    httpd_resp_send(req, LABORATORY_HTML, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static const httpd_uri_t hotspot_detect_uri = {
    .uri       = "/hotspot-detect.html",
    .method    = HTTP_GET,
    .handler   = hotspot_detect_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t library_test_uri = {
    .uri       = "/library/test/success.html",
    .method    = HTTP_GET,
    .handler   = hotspot_detect_handler,
    .user_ctx  = NULL
};

// Windows connectivity check
static esp_err_t connecttest_handler(httpd_req_t *req)
{
    httpd_resp_send(req, LABORATORY_HTML, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static const httpd_uri_t connecttest_uri = {
    .uri       = "/connecttest.txt",
    .method    = HTTP_GET,
    .handler   = connecttest_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t ncsi_uri = {
    .uri       = "/ncsi.txt",
    .method    = HTTP_GET,
    .handler   = connecttest_handler,
    .user_ctx  = NULL
};

static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.lru_purge_enable = true;
    config.max_uri_handlers = 17;  // Increase from default 8 to fit all captive portal URIs + WiFi scan

    ESP_LOGI(TAG, "Starting web server on port %d", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Core endpoints
        httpd_register_uri_handler(server, &root_uri);
        httpd_register_uri_handler(server, &logs_uri);
        httpd_register_uri_handler(server, &logs_recent_uri);
        httpd_register_uri_handler(server, &ota_page_uri);
        httpd_register_uri_handler(server, &ota_upload_uri);
        httpd_register_uri_handler(server, &wifi_scan_uri);
        httpd_register_uri_handler(server, &wifi_connect_uri);

        // Captive portal detection endpoints
        httpd_register_uri_handler(server, &generate_204_uri);
        httpd_register_uri_handler(server, &gen_204_uri);
        httpd_register_uri_handler(server, &hotspot_detect_uri);
        httpd_register_uri_handler(server, &library_test_uri);
        httpd_register_uri_handler(server, &connecttest_uri);
        httpd_register_uri_handler(server, &ncsi_uri);

        ESP_LOGI(TAG, "✓ Registered core endpoints: /, /debug/logs, /debug/recent, /update, /ota, /wifi/connect");
        ESP_LOGI(TAG, "✓ Registered captive portal detection URLs (iOS, Android, Windows)");
        return server;
    }

    ESP_LOGE(TAG, "Failed to start web server!");
    return NULL;
}

void start_captive_portal(void)
{
    ESP_LOGI(TAG, "Starting captive portal...");

    // Start HTTP server
    start_webserver();

    // TODO: Add DNS server to hijack all queries
    // For now, portal is accessible at 192.168.4.1

    ESP_LOGI(TAG, "Captive portal running at http://192.168.4.1");
}
