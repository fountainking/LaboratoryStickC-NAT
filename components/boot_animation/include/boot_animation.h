#ifndef BOOT_ANIMATION_H
#define BOOT_ANIMATION_H

#include <stdint.h>
#include <stdbool.h>
#include "m5_display.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Play the boot animation GIF
 *
 * Displays the embedded boot.gif animation on the M5StickC Plus2 display.
 * This is designed to be called during system startup.
 *
 * @param display Pointer to initialized m5_display_t structure
 * @return true if animation played successfully, false otherwise
 */
bool boot_animation_play(m5_display_t *display);

#ifdef __cplusplus
}
#endif

#endif // BOOT_ANIMATION_H
