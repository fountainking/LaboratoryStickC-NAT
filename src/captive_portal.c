#include "captive_portal.h"
#include "dns_server.h"
#include "log_capture.h"
#include "ota_manager.h"
#include "portal_mode.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_ota_ops.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "lwip/sockets.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG = "Portal";

// Buffer for serving logs (16KB should be plenty)
#define LOG_RESPONSE_BUFFER_SIZE (16 * 1024)

// Landing pages removed - portal now redirects straight to /wifi scanner
// WiFi Setup Portal - Shown when device needs configuration (UNUSED - kept for reference)
/*
static const char SETUP_PORTAL_HTML[] =
"<!DOCTYPE html>"
"<html>"
"<head>"
"<meta charset=\"UTF-8\">"
"<meta name=\"viewport\" content=\"width=device-width,initial-scale=1.0\">"
"<title>labPORTAL WiFi Setup</title>"
"<style>"
"*{margin:0;padding:0;box-sizing:border-box;}"
"body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,Helvetica,Arial,sans-serif;margin:0;padding:30px 20px;background:#000;color:#fff;min-height:100vh;}"
".container{max-width:600px;margin:0 auto;}"
"h1{color:#f5c429;text-align:center;margin-bottom:30px;font-size:2em;}"
".info{background:rgba(245,196,41,0.1);border:2px solid #f5c429;border-radius:20px;padding:30px;margin:30px 0;text-align:center;}"
".info p{line-height:1.8;font-size:1.2em;margin:10px 0;}"
".cta-button{display:block;background:#f5c429;color:#000;border:3px solid #f5c429;border-radius:50px;padding:18px 40px;font-size:1.3em;font-weight:bold;text-align:center;margin:20px auto;max-width:350px;cursor:pointer;text-decoration:none;}"
".cta-button:hover{background:#FFE000;border-color:#FFE000;}"
"</style>"
"</head>"
"<body>"
"<div class=\"container\">"
"<h1>labPORTAL WiFi Setup</h1>"
"<div class=\"info\">"
"<p><strong>Welcome!</strong></p>"
"<p>Configure your internet connection to enable the NAT bridge and share WiFi with visitors.</p>"
"</div>"
"<a href=\"/wifi\" class=\"cta-button\">CONFIGURE WIFI</a>"
"</div>"
"</body>"
"</html>";

// Laboratory Portal - Official design with NAT connectivity testing
static const char LABORATORY_HTML[] =
"<!DOCTYPE html>"
"<html>"
"<head>"
"<meta charset=\"UTF-8\">"
"<meta name=\"viewport\" content=\"width=device-width,initial-scale=1.0\">"
"<meta http-equiv=\"Cache-Control\" content=\"no-cache,no-store,must-revalidate\">"
"<title>Laboratory</title>"
"<style>"
"*{margin:0;padding:0;box-sizing:border-box;}"
"body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,Helvetica,Arial,sans-serif;margin:0;padding:30px 20px;background:#f5f5f5;color:#000;min-height:100vh;display:flex;flex-direction:column;align-items:center;justify-content:flex-start;}"
".frame{max-width:600px;width:100%;background:#fff;border:3px solid #000;border-radius:40px;padding:35px 40px;margin-bottom:30px;}"
".logo-container{max-width:600px;width:100%;display:flex;justify-content:center;align-items:center;margin-bottom:30px;}"
".logo-container svg{width:100%;max-width:400px;height:auto;}"
".description{text-align:center;line-height:1.7;font-size:1.2em;color:#000;}"
".recruiting-section{max-width:600px;width:100%;text-align:center;margin-bottom:30px;}"
".recruiting{text-align:center;margin:0 0 20px 0;font-size:1.2em;color:#000;line-height:1.6;}"
".cta-button{display:block;background:#fff;color:#000;border:3px solid #000;border-radius:50px;padding:18px 40px;font-size:1.3em;text-align:center;margin:15px auto;max-width:350px;cursor:pointer;text-decoration:none;}"
".cta-button:hover{background:#f5f5f5;}"
".cta-button:active{background:#e0e0e0;}"
"</style>"
"</head>"
"<body>"
"<div class=\"logo-container\">"
"<svg viewBox=\"0 0 379 96.1\" xmlns=\"http://www.w3.org/2000/svg\">"
"<defs><style>.st0{fill:#f5c429;stroke-width:4px}.st0,.st1{stroke:#000;stroke-miterlimit:10}.st2{fill:#fff}</style></defs>"
"<g><rect class=\"st2\" x=\"6\" y=\"6\" width=\"367\" height=\"84\" rx=\"42\" ry=\"42\"/>"
"<path d=\"M12,48c0-19.9,16.4-35.8,36.1-36s8.7,0,13.1,0c24.2,0,48.5,0,72.7,0,32.8,0,65.6,0,98.4,0,26.3,0,52.6,0,79,0s12.1,0,18.1,0c14.4,0,27.5,7.1,34,20.3,9.9,20-1.8,44.9-23.3,50.5-5.6,1.5-11.5,1.2-17.2,1.2h-66.9c-32.4,0-64.8,0-97.2,0s-55.9,0-83.8,0c-16.4,0-36.6,3-50-8.4-8.2-6.9-12.7-16.9-12.9-27.6S-.1,40.3,0,48c.4,24.5,18.7,45.1,43.2,47.7s10.2.3,15.2.3c24.7,0,49.4,0,74.1,0h102.7c26.7,0,53.3,0,80,0,5.2,0,10.3,0,15.5,0,20.4-.2,38.4-12.7,45.5-31.9,8.2-22.2-2.3-48.3-23.5-58.9S334.8,0,325.2,0h-67c-34,0-67.9,0-101.9,0h-85.9c-6.8,0-13.6,0-20.5,0-15,0-29.3,5.6-39,17.4S0,36.9,0,48s12,7.7,12,0Z\"/></g>"
"<g><g><path class=\"st1\" d=\"M47.8,59.8c1,0,1.4.8,1.4,1.7s-.5,1.7-1.4,1.7h-16.1v-28.3c0-1,.9-1.4,1.9-1.4s1.9.5,1.9,1.4v24.8h12.4Z\"/>"
"<path class=\"st1\" d=\"M75.6,62.1c0,1-.8,1.5-1.8,1.5s-1.9-.5-1.9-1.5v-8.5h-14v8.5c0,1-.8,1.5-1.8,1.5s-1.8-.5-1.8-1.5v-16.5c0-2.2.5-3.9,2.2-5.6l5.9-5.8c.5-.5,1.3-.9,2.5-.9s2,.5,2.5.9l6,5.8c1.7,1.6,2.2,3.3,2.2,5.6v16.5ZM72,50.4v-4.6c0-1.5-.3-2.5-1.4-3.5l-5.7-5.5-5.7,5.5c-1.1,1.1-1.4,2-1.4,3.5v4.6h14.1Z\"/>"
"<path class=\"st1\" d=\"M103.2,56.2c0,4.4-2.5,7.2-7.4,7.2h-12.9v-29.5h12.9c4.4,0,7.3,3,7.3,7.2v2.1c0,3-1.2,4.5-2.7,5.3,1.6.7,2.8,2.7,2.8,5v2.8ZM96.3,46.8c2.2,0,3.1-.8,3.1-3.7v-2.4c0-2.2-1.4-3.5-3.4-3.5h-9.6v9.6h9.9ZM95.7,60.2c2.4,0,3.8-1.2,3.8-3.5v-2.7c0-2.7-1.1-3.8-3-3.8h-10.2v10.1h9.3Z\"/>"
"<path d=\"M131.9,56.3c0,4-3.2,7.3-7.6,7.3h-6.8c-4.4,0-7.6-3.2-7.6-7.3v-15.3c0-4.1,3.4-7.3,7.4-7.3h7.1c4.2,0,7.5,3.2,7.5,7.3v15.3ZM124.7,60.1c2.2,0,3.7-1.7,3.7-3.5v-15.9c0-1.9-1.6-3.5-3.5-3.5h-7.8c-2.1,0-3.5,1.6-3.5,3.5v15.9c0,1.9,1.5,3.5,3.7,3.5h7.5Z\"/>"
"<path d=\"M159.6,62.1c0,1-.8,1.5-1.8,1.5s-1.9-.5-1.9-1.5v-8c0-2.4-1.3-3.7-3.5-3.7h-9.6v11.7c0,1-.8,1.5-1.7,1.5s-1.8-.5-1.8-1.5v-28.4h13.2c4.1,0,7.1,2.9,7.1,7v2.8c0,2.9-1.4,4.9-3.1,5.7,1.7.6,3.1,2.5,3.1,4.7v8.3ZM152.8,47.1c2.2,0,3.3-1.2,3.3-3.3v-3.5c0-2-1.3-3.2-3.2-3.2h-10.1v10h10Z\"/>"
"<path d=\"M187.7,62.1c0,1-.8,1.5-1.8,1.5s-1.9-.5-1.9-1.5v-8.5h-14v8.5c0,1-.8,1.5-1.8,1.5s-1.8-.5-1.8-1.5v-16.5c0-2.2.5-3.9,2.2-5.6l5.9-5.8c.5-.5,1.3-.9,2.5-.9s2,.5,2.5.9l6,5.8c1.7,1.6,2.2,3.3,2.2,5.6v16.5ZM184.1,50.4v-4.6c0-1.5-.3-2.5-1.4-3.5l-5.7-5.5-5.7,5.5c-1.1,1.1-1.4,2-1.4,3.5v4.6h14.1Z\"/>"
"<path d=\"M205.8,62.1c0,1-.9,1.5-1.9,1.5s-2-.5-2-1.5v-24.7h-7.8c-1,0-1.4-.7-1.4-1.7s.5-1.8,1.4-1.8h19.5c1,0,1.5.8,1.5,1.8s-.5,1.7-1.5,1.7h-7.8v24.7Z\"/>"
"<path d=\"M241.5,56.3c0,4-3.2,7.3-7.6,7.3h-6.8c-4.4,0-7.6-3.2-7.6-7.3v-15.3c0-4.1,3.4-7.3,7.4-7.3h7.1c4.2,0,7.5,3.2,7.5,7.3v15.3ZM234.3,60.1c2.2,0,3.7-1.7,3.7-3.5v-15.9c0-1.9-1.6-3.5-3.5-3.5h-7.8c-2.1,0-3.5,1.6-3.5,3.5v15.9c0,1.9,1.5,3.5,3.7,3.5h7.5Z\"/>"
"<path d=\"M269.2,62.1c0,1-.8,1.5-1.8,1.5s-1.9-.5-1.9-1.5v-8c0-2.4-1.3-3.7-3.5-3.7h-9.6v11.7c0,1-.8,1.5-1.7,1.5s-1.8-.5-1.8-1.5v-28.4h13.2c4.1,0,7.1,2.9,7.1,7v2.8c0,2.9-1.3,4.9-3.1,5.7,1.7.6,3.1,2.5,3.1,4.7v8.3ZM262.4,47.1c2.2,0,3.3-1.2,3.3-3.3v-3.5c0-2-1.3-3.2-3.2-3.2h-10.1v10h10Z\"/>"
"<path d=\"M287.7,62.1c0,1-.8,1.5-1.8,1.5s-1.9-.5-1.9-1.5v-9.9l-7-6.4c-1.1-1.1-1.7-2.6-1.7-4.5v-6.3c0-1,.8-1.4,1.8-1.4s1.8.5,1.8,1.4v6.1c0,1.2,0,1.9.8,2.5l5.7,5.2h.7l5.7-5.2c.7-.6.9-1.3.9-2.5v-6.1c0-1,.8-1.4,1.8-1.4s1.8.5,1.8,1.4v6.3c0,2-.3,3.2-1.7,4.5l-7,6.4v9.9Z\"/></g>"
"<path class=\"st0\" d=\"M330.7,26.9l5,10.2c.4.8,1.2,1.4,2,1.5l11.2,1.6c2.2.3,3.1,3.1,1.5,4.6l-8.1,7.9c-.6.6-.9,1.5-.8,2.4l1.9,11.2c.4,2.2-1.9,3.9-3.9,2.9l-10-5.3c-.8-.4-1.7-.4-2.5,0l-10,5.3c-2,1-4.3-.6-3.9-2.9l1.9-11.2c.2-.9-.1-1.8-.8-2.4l-8.1-7.9c-1.6-1.6-.7-4.3,1.5-4.6l11.2-1.6c.9-.1,1.6-.7,2-1.5l5-10.2c1-2,3.9-2,4.9,0Z\"/></g>"
"</svg>"
"</div>"
"<div class=\"frame\">"
"<div class=\"description\">"
"<strong>Laboratory</strong> is a workforce economic program centered around entrepreneurship that offers physical classrooms, retail store fronts, and content production studios designed to improve economic outcomes by addressing the skills, training, and opportunities individuals need to succeed."
"</div>"
"</div>"
"<div class=\"recruiting-section\">"
"<div class=\"recruiting\">"
"We're recruiting for our<br>"
"<strong>*EXPERT COMMITTEE*</strong><br>"
"If you know anything about<br>"
"being an entreprenuer:"
"</div>"
"<a href=\"mailto:info@laboratory.mx?subject=Expert%20Committee%20Inquiry&body=Hi%20Laboratory%20Team%2C%0A%0AI'm%20interested%20in%20joining%20the%20Expert%20Committee.\" class=\"cta-button\">JOIN THE MISSION!</a>"
"</div>"
"</body>"
"</html>";
*/

// Root handler - serves the appropriate portal based on mode
static esp_err_t root_handler(httpd_req_t *req)
{
    // Show Laboratory landing page with grant access button and laboratory.mx link
    const char* landing_html =
    "<!DOCTYPE html><html><head>"
    "<meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1.0'>"
    "<title>Laboratory Portal</title>"
    "<link rel=\"icon\" href=\"data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 100 100'%3E%3Ctext y='.9em' font-size='90'%3E⭐%3C/text%3E%3C/svg%3E\">"
    "<style>"
    "*{margin:0;padding:0;box-sizing:border-box;}"
    "body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,Helvetica,Arial,sans-serif;background:#f5f5f5;color:#000;padding:30px 20px;min-height:100vh;}"
    ".container{max-width:500px;margin:0 auto;}"
    "h1{text-align:center;margin-bottom:20px;font-size:2.5em;}"
    ".frame{background:#fff;border:3px solid #000;border-radius:40px;padding:35px 40px;margin-bottom:15px;}"
    ".description{text-align:center;line-height:1.6;font-size:1em;margin-bottom:20px;}"
    "a{width:100%;padding:18px;background:#fff;color:#000;border:3px solid #000;border-radius:50px;font-size:1.2em;cursor:pointer;margin:10px 0;font-weight:bold;text-decoration:none;display:block;text-align:center;}"
    "a:hover{background:#f5f5f5;}"
    ".primary{background:#000;color:#fff;}"
    ".primary:hover{background:#333;}"
    ".link{font-size:0.9em;padding:12px;background:transparent;border:2px solid #666;color:#666;}"
    ".link:hover{background:#f5f5f5;border-color:#000;color:#000;}"
    "</style>"
    "</head><body>"
    "<div class='container'>"
    "<h1>⭐ Laboratory</h1>"
    "<div class='frame'>"
    "<div class='description'>"
    "<strong>Welcome to Laboratory Network</strong><br>"
    "A workforce economic program centered around entrepreneurship offering physical classrooms, retail storefronts, and content production studios."
    "</div>"
    "<a href='/grant' class='primary'>Get Internet Access</a>"
    "<a href='https://laboratory.mx' class='link' target='_blank'>Visit Laboratory.mx →</a>"
    "<a href='/wifi'>Configure WiFi</a>"
    "</div>"
    "</div>"
    "</body></html>";

    httpd_resp_send(req, landing_html, HTTPD_RESP_USE_STRLEN);
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

// WiFi settings page - white background Laboratory style
static esp_err_t wifi_page_handler(httpd_req_t *req)
{
    const char* wifi_html =
    "<!DOCTYPE html><html><head>"
    "<meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1.0'>"
    "<title>WiFi Settings - Laboratory</title>"
    "<link rel=\"icon\" href=\"data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 100 100'%3E%3Ctext y='.9em' font-size='90'%3E⭐%3C/text%3E%3C/svg%3E\">"
    "<style>"
    "*{margin:0;padding:0;box-sizing:border-box;}"
    "body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,Helvetica,Arial,sans-serif;background:#f5f5f5;color:#000;padding:30px 20px;min-height:100vh;}"
    ".container{max-width:500px;margin:0 auto;}"
    "h1{text-align:center;margin-bottom:30px;font-size:2em;}"
    ".frame{background:#fff;border:3px solid #000;border-radius:40px;padding:35px 40px;margin-bottom:20px;}"
    "button{width:100%;padding:15px;background:#fff;color:#000;border:3px solid #000;border-radius:50px;font-size:1.1em;cursor:pointer;margin:10px 0;font-weight:bold;}"
    "button:hover{background:#f5f5f5;}"
    "button:active{background:#e0e0e0;}"
    ".network{background:#fff;border:2px solid #000;padding:15px 20px;margin:10px 0;border-radius:25px;cursor:pointer;display:flex;justify-content:space-between;align-items:center;}"
    ".network:hover{background:#f5f5f5;}"
    "#networks{margin:20px 0;max-height:350px;overflow-y:auto;}"
    ".status{text-align:center;padding:15px;margin:15px 0;border-radius:25px;background:#f5f5f5;border:2px solid #000;font-weight:bold;}"
    "input{width:100%;padding:15px;margin:10px 0;border:3px solid #000;border-radius:25px;font-size:1em;background:#fff;color:#000;}"
    "input:focus{outline:none;border-color:#000;}"
    ".back{background:transparent;}"
    ".scanning{text-align:center;padding:20px;color:#666;}"
    "</style>"
    "</head><body>"
    "<div class='container'>"
    "<h1>WiFi Settings</h1>"
    "<div class='frame'>"
    "<div id='scanView'>"
    "<button onclick='scanNetworks()'>Scan for Networks</button>"
    "<div id='networks'></div>"
    "</div>"
    "<div id='formView' style='display:none;'>"
    "<div id='selectedNetwork' style='margin:10px 0;font-weight:bold;text-align:center;'></div>"
    "<input type='password' id='password' placeholder='WiFi Password'>"
    "<button onclick='connect()'>Connect</button>"
    "<button class='back' onclick='showScan()'>Cancel</button>"
    "</div>"
    "<div class='status' id='status' style='display:none;'></div>"
    "</div>"
    "<button class='back' onclick='location.href=\"/\"'>Back to Portal</button>"
    "</div>"
    "<script>"
    "let selectedSSID='';"
    "async function scanNetworks(){"
    "  document.getElementById('networks').innerHTML='<div class=\"scanning\">Scanning...</div>';"
    "  const res=await fetch('/wifi/scan');"
    "  const networks=await res.json();"
    "  let html='';"
    "  networks.forEach(n=>{"
    "    html+=`<div class='network' onclick='select(\"${n.ssid}\",${n.auth})'><span>${n.ssid}</span><span style='color:#666;font-size:0.9em;'>${n.rssi}dBm ${n.auth?'🔒':''}</span></div>`;"
    "  });"
    "  document.getElementById('networks').innerHTML=html||'<div class=\"scanning\">No networks found</div>';"
    "}"
    "function select(ssid,auth){"
    "  selectedSSID=ssid;"
    "  if(!auth){connect();}else{"
    "    document.getElementById('scanView').style.display='none';"
    "    document.getElementById('formView').style.display='block';"
    "    document.getElementById('selectedNetwork').textContent='Network: '+ssid;"
    "    document.getElementById('password').value='';"
    "    document.getElementById('password').focus();"
    "  }"
    "}"
    "function showScan(){"
    "  document.getElementById('scanView').style.display='block';"
    "  document.getElementById('formView').style.display='none';"
    "}"
    "async function connect(){"
    "  const pass=document.getElementById('password')?.value||'';"
    "  const data='ssid='+encodeURIComponent(selectedSSID)+'&password='+encodeURIComponent(pass);"
    "  const status=document.getElementById('status');"
    "  status.style.display='block';"
    "  status.textContent='Connecting...';"
    "  const res=await fetch('/wifi/connect',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:data});"
    "  status.textContent=await res.text();"
    "}"
    "window.onload=()=>scanNetworks();"
    "</script>"
    "</body></html>";

    httpd_resp_send(req, wifi_html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static const httpd_uri_t wifi_page_uri = {
    .uri       = "/wifi",
    .method    = HTTP_GET,
    .handler   = wifi_page_handler,
    .user_ctx  = NULL
};

// File transfer page - simple upload interface
static esp_err_t transfer_page_handler(httpd_req_t *req)
{
    const char* transfer_html =
    "<!DOCTYPE html><html><head>"
    "<meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1.0'>"
    "<title>File Transfer</title>"
    "<link rel=\"icon\" href=\"data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 100 100'%3E%3Ctext y='.9em' font-size='90'%3E⭐%3C/text%3E%3C/svg%3E\">"
    "<style>"
    "*{margin:0;padding:0;box-sizing:border-box;}"
    "body{font-family:-apple-system,sans-serif;background:#000;color:#fff;padding:20px;min-height:100vh;}"
    ".container{max-width:600px;margin:0 auto;}"
    "h1{color:#FFE000;text-align:center;margin-bottom:30px;font-size:2em;}"
    "h2{color:#FFE000;margin:20px 0 10px 0;}"
    ".info{background:rgba(255,255,255,0.1);padding:20px;border-radius:10px;margin:20px 0;line-height:1.8;}"
    ".feature{margin:10px 0;padding-left:20px;}"
    ".feature:before{content:'→';color:#FFE000;margin-right:10px;}"
    "button{width:100%;padding:15px;background:#FFE000;color:#000;border:none;border-radius:10px;font-size:1.1em;cursor:pointer;margin:10px 0;font-weight:bold;}"
    "button:hover{background:#fff;}"
    ".back{background:transparent;border:2px solid #FFE000;color:#FFE000;}"
    ".back:hover{background:rgba(255,224,0,0.1);}"
    "input[type=file]{display:block;width:100%;padding:12px;margin:15px 0;border:2px solid #FFE000;border-radius:10px;background:#000;color:#fff;}"
    ".status{text-align:center;padding:15px;margin:15px 0;border-radius:10px;background:rgba(255,224,0,0.2);color:#FFE000;font-weight:bold;}"
    "</style>"
    "</head><body>"
    "<div class='container'>"
    "<h1>📁 File Transfer</h1>"
    "<div class='info'>"
    "<h2>Coming Soon</h2>"
    "<p>File transfer functionality will allow you to:</p>"
    "<div class='feature'>Upload custom portal HTML</div>"
    "<div class='feature'>Upload media files (images, videos)</div>"
    "<div class='feature'>Manage stored content</div>"
    "<div class='feature'>View file system status</div>"
    "</div>"
    "<div class='info' style='background:rgba(255,224,0,0.1);border:2px solid #FFE000;'>"
    "<p style='text-align:center;'>This feature is currently under development.<br>Check back in a future firmware update!</p>"
    "</div>"
    "<button class='back' onclick='location.href=\"/\"'>← Back to Portal</button>"
    "</div>"
    "</body></html>";

    httpd_resp_send(req, transfer_html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static const httpd_uri_t transfer_page_uri = {
    .uri       = "/transfer",
    .method    = HTTP_GET,
    .handler   = transfer_page_handler,
    .user_ctx  = NULL
};

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

// Grant internet access handler - approves client via DNS filtering
static esp_err_t grant_access_handler(httpd_req_t *req)
{
    // Get client IP from socket descriptor
    int sockfd = httpd_req_to_sockfd(req);
    ESP_LOGI(TAG, "Socket descriptor: %d", sockfd);

    if (sockfd < 0) {
        ESP_LOGE(TAG, "Invalid socket descriptor");
        // Fallback: approve the most recent DHCP client (192.168.4.2)
        // This is a workaround - TODO: find better way to get client IP
        uint32_t fallback_ip = (2 << 24) | (4 << 16) | (168 << 8) | 192; // 192.168.4.2 in network byte order
        ESP_LOGW(TAG, "Using fallback IP: 192.168.4.2");
        dns_approve_client(fallback_ip);
    } else {
        struct sockaddr_storage addr;
        socklen_t addr_size = sizeof(addr);

        int ret = getpeername(sockfd, (struct sockaddr *)&addr, &addr_size);
        ESP_LOGI(TAG, "getpeername returned: %d, errno: %d", ret, errno);
        ESP_LOGI(TAG, "Address family: %d (AF_INET=%d)", addr.ss_family, AF_INET);

        if (ret < 0) {
            ESP_LOGE(TAG, "Failed to get client address: errno %d", errno);
            // Fallback
            uint32_t fallback_ip = (2 << 24) | (4 << 16) | (168 << 8) | 192;
            ESP_LOGW(TAG, "Using fallback IP: 192.168.4.2");
            dns_approve_client(fallback_ip);
        } else if (addr.ss_family == AF_INET) {
            struct sockaddr_in *addr_in = (struct sockaddr_in *)&addr;
            uint32_t client_ip = addr_in->sin_addr.s_addr;

            uint8_t *ip_bytes = (uint8_t*)&client_ip;
            ESP_LOGI(TAG, "Grant request from client: %d.%d.%d.%d",
                     ip_bytes[0], ip_bytes[1], ip_bytes[2], ip_bytes[3]);

            dns_approve_client(client_ip);
        } else {
            ESP_LOGE(TAG, "Unexpected address family: %d", addr.ss_family);
            // Fallback
            uint32_t fallback_ip = (2 << 24) | (4 << 16) | (168 << 8) | 192;
            ESP_LOGW(TAG, "Using fallback IP: 192.168.4.2");
            dns_approve_client(fallback_ip);
        }
    }

    // Send success page
    const char* success_html =
    "<!DOCTYPE html><html><head>"
    "<meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1.0'>"
    "<title>Access Granted - Laboratory</title>"
    "<link rel=\"icon\" href=\"data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 100 100'%3E%3Ctext y='.9em' font-size='90'%3E⭐%3C/text%3E%3C/svg%3E\">"
    "<style>"
    "*{margin:0;padding:0;box-sizing:border-box;}"
    "body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,Helvetica,Arial,sans-serif;background:#f5f5f5;color:#000;padding:30px 20px;min-height:100vh;display:flex;align-items:center;justify-content:center;}"
    ".container{max-width:500px;text-align:center;}"
    ".frame{background:#fff;border:3px solid #000;border-radius:40px;padding:40px;}"
    "h1{font-size:3em;margin-bottom:20px;}"
    "p{font-size:1.3em;line-height:1.6;margin:15px 0;}"
    ".success{color:#00AA00;font-weight:bold;}"
    "</style>"
    "</head><body>"
    "<div class='container'>"
    "<div class='frame'>"
    "<h1>✅</h1>"
    "<p class='success'>Internet Access Granted!</p>"
    "<p>You can now browse the internet through Laboratory.</p>"
    "<p style='font-size:0.9em;color:#666;margin-top:30px;'>You can close this page.</p>"
    "</div>"
    "</div>"
    "</body></html>";

    httpd_resp_send(req, success_html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static const httpd_uri_t grant_access_uri = {
    .uri       = "/grant",
    .method    = HTTP_GET,
    .handler   = grant_access_handler,
    .user_ctx  = NULL
};

// Captive portal detection handlers
// Serve actual portal page to trigger captive portal popup
static esp_err_t captive_redirect_handler(httpd_req_t *req)
{
    // Return simple HTML that triggers captive portal detection
    // This forces the phone to show the captive portal popup
    const char* captive_html =
    "<!DOCTYPE html><html><head>"
    "<meta http-equiv='refresh' content='0;url=http://192.168.4.1'>"
    "</head><body>"
    "<script>window.location.href='http://192.168.4.1';</script>"
    "</body></html>";

    httpd_resp_send(req, captive_html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// Android connectivity checks
static esp_err_t generate_204_handler(httpd_req_t *req)
{
    return captive_redirect_handler(req);
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

// iOS/Apple captive portal detection
static esp_err_t hotspot_detect_handler(httpd_req_t *req)
{
    return captive_redirect_handler(req);
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
    return captive_redirect_handler(req);
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

// Linux connectivity checks
static const httpd_uri_t canonical_uri = {
    .uri       = "/canonical.html",
    .method    = HTTP_GET,
    .handler   = captive_redirect_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t connectivity_check_uri = {
    .uri       = "/connectivity-check.html",
    .method    = HTTP_GET,
    .handler   = captive_redirect_handler,
    .user_ctx  = NULL
};

// Firefox connectivity check
static const httpd_uri_t success_txt_uri = {
    .uri       = "/success.txt",
    .method    = HTTP_GET,
    .handler   = captive_redirect_handler,
    .user_ctx  = NULL
};

static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.lru_purge_enable = true;
    config.max_uri_handlers = 24;  // Increase to fit all URIs + captive detection

    // Multi-device support: handle iOS/Android captive detection (10+ parallel connections)
    config.max_open_sockets = 13;   // 16 LWIP limit - 3 reserved = 13 usable
    config.recv_wait_timeout = 3;   // Faster cleanup
    config.send_wait_timeout = 3;

    ESP_LOGI(TAG, "Starting web server on port %d with %d sockets",
             config.server_port, config.max_open_sockets);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Core endpoints
        httpd_register_uri_handler(server, &root_uri);
        httpd_register_uri_handler(server, &grant_access_uri);
        httpd_register_uri_handler(server, &wifi_page_uri);
        httpd_register_uri_handler(server, &transfer_page_uri);
        httpd_register_uri_handler(server, &logs_uri);
        httpd_register_uri_handler(server, &logs_recent_uri);
        httpd_register_uri_handler(server, &ota_page_uri);
        httpd_register_uri_handler(server, &ota_upload_uri);
        httpd_register_uri_handler(server, &wifi_scan_uri);
        httpd_register_uri_handler(server, &wifi_connect_uri);

        // Captive portal detection endpoints (all return 302 redirect)
        httpd_register_uri_handler(server, &generate_204_uri);   // Android
        httpd_register_uri_handler(server, &gen_204_uri);        // Android
        httpd_register_uri_handler(server, &hotspot_detect_uri); // iOS/macOS
        httpd_register_uri_handler(server, &library_test_uri);   // iOS/macOS
        httpd_register_uri_handler(server, &connecttest_uri);    // Windows
        httpd_register_uri_handler(server, &ncsi_uri);           // Windows
        httpd_register_uri_handler(server, &canonical_uri);      // Linux
        httpd_register_uri_handler(server, &connectivity_check_uri); // Linux
        httpd_register_uri_handler(server, &success_txt_uri);    // Firefox

        ESP_LOGI(TAG, "✓ Registered core endpoints: /, /wifi, /transfer, /update");
        ESP_LOGI(TAG, "✓ Registered debug endpoints: /debug/logs, /debug/recent");
        ESP_LOGI(TAG, "✓ Registered WiFi API: /wifi/scan, /wifi/connect");
        ESP_LOGI(TAG, "✓ Registered captive portal detection (Android, iOS, Windows, Linux, Firefox)");
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
