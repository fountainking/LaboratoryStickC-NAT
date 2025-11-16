#include "boot_animation.h"
#include "embedded_gifs.h"
#include <SPI.h>

static AnimatedGIF bootGif;

// GIF draw callback for boot animation
void BootGIFDraw(GIFDRAW *pDraw) {
  uint8_t *s;
  uint16_t *usPalette, usTemp[320];
  int x, y, iWidth;

  iWidth = pDraw->iWidth;
  if (iWidth > 240) iWidth = 240;

  usPalette = pDraw->pPalette;

  // Center the GIF vertically
  int gifHeight = bootGif.getCanvasHeight();
  int yOffset = (135 - gifHeight) / 2;
  if (yOffset < 0) yOffset = 0;

  y = pDraw->iY + pDraw->y + yOffset;

  if (y >= 135) return; // Off screen

  s = pDraw->pPixels;
  if (pDraw->ucDisposalMethod == 2) {
    for (x = 0; x < iWidth; x++) {
      if (s[x] == pDraw->ucTransparent) {
        s[x] = pDraw->ucBackground;
      }
    }
    pDraw->ucHasTransparency = 0;
  }

  if (pDraw->ucHasTransparency) {
    uint8_t c, ucTransparent = pDraw->ucTransparent;
    int x, iCount;
    for (x = 0; x < iWidth; x++) {
      if (s[x] == ucTransparent) {
        continue;
      }
      c = s[x];
      usTemp[0] = usPalette[c];
      iCount = 1;
      while (x + iCount < iWidth && s[x + iCount] == c && s[x + iCount] != ucTransparent) {
        usTemp[iCount] = usPalette[c];
        iCount++;
      }
      M5.Display.pushImage(pDraw->iX + x, y, iCount, 1, usTemp);
      x += (iCount - 1);
    }
  } else {
    s = pDraw->pPixels;
    for (x = 0; x < iWidth; x++) {
      usTemp[x] = usPalette[s[x]];
    }
    M5.Display.pushImage(pDraw->iX, y, iWidth, 1, usTemp);
  }
}

bool playBootGIF() {
  M5.Display.fillScreen(TFT_BLACK);

  bootGif.begin(GIF_PALETTE_RGB565_BE);

  // Open embedded boot GIF from PROGMEM
  if (!bootGif.open((uint8_t*)gif_boot, gif_boot_len, BootGIFDraw)) {
    return false;
  }

  // Play all frames
  while (bootGif.playFrame(true, NULL) > 0) {
    // Keep playing until animation completes
  }

  bootGif.close();
  return true;
}

void playBootAnimation() {
  // Play embedded boot GIF
  if (playBootGIF()) {
    delay(500);
  } else {
    // Fallback if GIF failed to play
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextSize(3);
    M5.Display.setTextColor(TFT_YELLOW);
    M5.Display.setCursor(60, 55);
    M5.Display.println("LABORATORY");
    delay(300);
  }
}
