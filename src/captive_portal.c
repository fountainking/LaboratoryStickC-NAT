#include "captive_portal.h"
#include "esp_log.h"
#include "esp_http_server.h"

static const char *TAG = "Portal";

// Laboratory portal HTML (from original portal_content.cpp)
static const char LABORATORY_HTML[] =
"<!DOCTYPE html>"
"<html>"
"<head>"
"<meta charset=\"UTF-8\">"
"<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">"
"<title>Laboratory</title>"
"<link rel=\"stylesheet\" href=\"https://use.typekit.net/wop7tdt.css\">"
"<style>"
"*{margin:0;padding:0;box-sizing:border-box;}"
"body{font-family:\"automate\",-apple-system,BlinkMacSystemFont,\"Segoe UI\",Roboto,sans-serif;margin:0;padding:30px 20px;background:#f5f5f5;color:#000;min-height:100vh;display:flex;flex-direction:column;align-items:center;justify-content:flex-start;}"
".frame{max-width:600px;width:100%;background:#fff;border:3px solid #000;border-radius:40px;padding:35px 40px;margin-bottom:30px;}"
".description{text-align:center;line-height:1.7;font-size:1.05em;color:#000;}"
".recruiting-section{max-width:600px;width:100%;text-align:center;margin-bottom:30px;}"
".recruiting{text-align:center;margin:0 0 20px 0;font-size:1.05em;color:#000;line-height:1.6;}"
".cta-button{display:block;background:#fff;color:#000;border:3px solid #000;border-radius:50px;padding:15px 40px;font-size:1.1em;font-weight:bold;text-align:center;margin:0 auto;max-width:350px;cursor:pointer;text-decoration:none;font-family:\"automate\",sans-serif;}"
".cta-button:hover{background:#f5f5f5;}"
"h1{text-align:center;font-size:2.5em;margin-bottom:20px;}"
"</style>"
"</head>"
"<body>"
"<h1>LABORATORY</h1>"
"<div class=\"frame\">"
"<p class=\"description\">Welcome to the Laboratory captive portal. You now have internet access!</p>"
"</div>"
"<div class=\"recruiting-section\">"
"<p class=\"recruiting\">Interested in Laboratory? We're hiring engineers.</p>"
"<a href=\"https://laboratory.mx\" class=\"cta-button\">Visit Laboratory.mx</a>"
"</div>"
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

static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.lru_purge_enable = true;

    ESP_LOGI(TAG, "Starting web server on port %d", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_register_uri_handler(server, &root_uri);
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
