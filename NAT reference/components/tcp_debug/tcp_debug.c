#include "tcp_debug.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

static const char *TAG = "TCPDebug";

#define MAX_CLIENTS 4
static int client_sockets[MAX_CLIENTS] = {-1, -1, -1, -1};
static SemaphoreHandle_t clients_mutex = NULL;

static void tcp_server_task(void *pvParameters)
{
    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(TCP_DEBUG_PORT);

    int listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (listen_sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        vTaskDelete(NULL);
        return;
    }

    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    int err = bind(listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err != 0) {
        ESP_LOGE(TAG, "Socket bind failed: errno %d", errno);
        close(listen_sock);
        vTaskDelete(NULL);
        return;
    }

    err = listen(listen_sock, 1);
    if (err != 0) {
        ESP_LOGE(TAG, "Error occurred during listen: errno %d", errno);
        close(listen_sock);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "TCP debug server listening on port %d", TCP_DEBUG_PORT);
    ESP_LOGI(TAG, "Connect via: nc <device_ip> %d", TCP_DEBUG_PORT);

    while (1) {
        struct sockaddr_in source_addr;
        socklen_t addr_len = sizeof(source_addr);
        int sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
        if (sock < 0) {
            ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
            continue;
        }

        char addr_str[128];
        inet_ntoa_r(source_addr.sin_addr, addr_str, sizeof(addr_str) - 1);
        ESP_LOGI(TAG, "Client connected from %s", addr_str);

        // Add client to list
        xSemaphoreTake(clients_mutex, portMAX_DELAY);
        bool added = false;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (client_sockets[i] == -1) {
                client_sockets[i] = sock;
                added = true;
                ESP_LOGI(TAG, "Added client to slot %d", i);
                break;
            }
        }
        xSemaphoreGive(clients_mutex);

        if (!added) {
            ESP_LOGW(TAG, "Max clients reached, closing connection");
            const char *msg = "Debug server full. Try again later.\r\n";
            send(sock, msg, strlen(msg), 0);
            close(sock);
        } else {
            // Send welcome message
            const char *welcome = "\r\n=== Laboratory NAT Debug Server ===\r\n";
            send(sock, welcome, strlen(welcome), 0);
        }
    }

    close(listen_sock);
    vTaskDelete(NULL);
}

void tcp_debug_init(void)
{
    ESP_LOGI(TAG, "Initializing TCP debug server...");

    clients_mutex = xSemaphoreCreateMutex();
    if (clients_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex!");
        return;
    }

    xTaskCreate(tcp_server_task, "tcp_debug_server", 4096, NULL, 5, NULL);
}

void tcp_debug_send(const char *message, size_t len)
{
    if (clients_mutex == NULL) return;

    xSemaphoreTake(clients_mutex, portMAX_DELAY);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_sockets[i] != -1) {
            int err = send(client_sockets[i], message, len, 0);
            if (err < 0) {
                ESP_LOGW(TAG, "Send failed to client %d, removing", i);
                close(client_sockets[i]);
                client_sockets[i] = -1;
            }
        }
    }

    xSemaphoreGive(clients_mutex);
}

void tcp_debug_printf(const char *format, ...)
{
    char buffer[256];
    va_list args;
    va_start(args, format);
    int len = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    if (len > 0) {
        tcp_debug_send(buffer, len);
    }
}
