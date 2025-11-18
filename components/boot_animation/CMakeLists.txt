idf_component_register(
    SRCS "boot_animation.cpp" "AnimatedGIF.cpp"
    INCLUDE_DIRS "include" "."
    REQUIRES m5_display esp_timer
)
