idf_component_register(
    SRCS "debug_screen.c"
    INCLUDE_DIRS "include"
    REQUIRES m5_display color_theme esp_wifi lwip
)
