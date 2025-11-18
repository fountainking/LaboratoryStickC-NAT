extern "C" {
    #include "boot_animation.h"
    #include "embedded_gif.h"
    #include "esp_log.h"
    #include "freertos/FreeRTOS.h"
    #include "freertos/task.h"
}
#include "AnimatedGIF.h"
#include <string.h>

static const char *TAG = "BootAnim";
static AnimatedGIF gif;
static m5_display_t *g_display = NULL;

// GIF draw callback - called for each line of the GIF
static void GIFDraw(GIFDRAW *pDraw)
{
    uint8_t *s;
    uint16_t *usPalette;
    int x, y, iWidth;

    if (!g_display || !g_display->framebuffer) {
        return;
    }

    iWidth = pDraw->iWidth;
    if (iWidth > LCD_WIDTH) {
        iWidth = LCD_WIDTH;
    }

    usPalette = pDraw->pPalette;

    // Center the GIF horizontally and vertically
    int gifHeight = gif.getCanvasHeight();
    int gifWidth = gif.getCanvasWidth();
    int xOffset = (LCD_WIDTH - gifWidth) / 2;
    int yOffset = (LCD_HEIGHT - gifHeight) / 2;
    if (xOffset < 0) xOffset = 0;
    if (yOffset < 0) yOffset = 0;

    y = pDraw->iY + pDraw->y + yOffset;
    if (y >= LCD_HEIGHT) {
        return; // Off screen
    }

    s = pDraw->pPixels;

    // Handle disposal method (transparency)
    if (pDraw->ucDisposalMethod == 2) {
        for (x = 0; x < iWidth; x++) {
            if (s[x] == pDraw->ucTransparent) {
                s[x] = pDraw->ucBackground;
            }
        }
        pDraw->ucHasTransparency = 0;
    }

    // Draw the line to framebuffer
    if (pDraw->ucHasTransparency) {
        // With transparency - only draw non-transparent pixels
        uint8_t c, ucTransparent = pDraw->ucTransparent;
        for (x = 0; x < iWidth; x++) {
            if (s[x] != ucTransparent) {
                int fb_x = pDraw->iX + x + xOffset;
                int fb_y = y;
                if (fb_x >= 0 && fb_x < LCD_WIDTH && fb_y >= 0 && fb_y < LCD_HEIGHT) {
                    c = s[x];
                    // Write directly to framebuffer
                    g_display->framebuffer[fb_y * LCD_WIDTH + fb_x] = usPalette[c];
                }
            }
        }
    } else {
        // No transparency - draw entire line
        for (x = 0; x < iWidth; x++) {
            int fb_x = pDraw->iX + x + xOffset;
            int fb_y = y;
            if (fb_x >= 0 && fb_x < LCD_WIDTH && fb_y >= 0 && fb_y < LCD_HEIGHT) {
                g_display->framebuffer[fb_y * LCD_WIDTH + fb_x] = usPalette[s[x]];
            }
        }
    }
}

extern "C" bool boot_animation_play(m5_display_t *display)
{
    if (!display || !display->framebuffer) {
        ESP_LOGE(TAG, "Invalid display handle");
        return false;
    }

    g_display = display;

    // Clear screen to black
    m5_display_clear(display, COLOR_BLACK);
    m5_display_flush(display);

    ESP_LOGI(TAG, "Starting boot animation (GIF size: %d bytes)", gif_boot_len);

    // Initialize AnimatedGIF with Little Endian for ST7789
    gif.begin(GIF_PALETTE_RGB565_LE);

    // Open embedded GIF from flash memory
    if (!gif.open((uint8_t*)gif_boot, gif_boot_len, GIFDraw)) {
        ESP_LOGE(TAG, "Failed to open boot GIF");
        gif.close();
        return false;
    }

    GIFINFO info;
    gif.getInfo(&info);
    ESP_LOGI(TAG, "GIF opened: %dx%d, %ld frames",
             gif.getCanvasWidth(), gif.getCanvasHeight(), (long)info.iFrameCount);

    // Play all frames at maximum speed (bSync=false skips GIF delays)
    int frame_count = 0;
    while (gif.playFrame(false, NULL)) {
        frame_count++;
        // Flush framebuffer to display after each frame
        m5_display_flush(display);
    }

    ESP_LOGI(TAG, "Boot animation complete (%d frames played)", frame_count);

    // Close GIF
    gif.close();

    // Short delay before continuing
    vTaskDelay(pdMS_TO_TICKS(500));

    return true;
}
