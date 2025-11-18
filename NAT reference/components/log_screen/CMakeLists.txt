idf_component_register(
    SRCS "log_screen.c"
    INCLUDE_DIRS "include"
    REQUIRES m5_display log_capture freertos
)
