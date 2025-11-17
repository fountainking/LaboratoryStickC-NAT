idf_component_register(
    SRCS "ota_manager.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_https_ota esp_http_client nvs_flash app_update esp_wifi mbedtls
)
