idf_component_register(
    SRCS "portal_ui.c"
    INCLUDE_DIRS "include"
    REQUIRES "m5_display" "boot_animation" "driver" "sound_system" "mpu6886" "dns_server"
)
