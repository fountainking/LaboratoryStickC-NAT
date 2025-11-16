#include "ota_manager.h"
#include <M5Unified.h>
#include <WiFiClientSecure.h>
#include "star_emoji.h"

// Global static client to avoid heap fragmentation
static WiFiClientSecure secureClient;

// Laboratory colors
#define COLOR_BLACK     0x0000
#define COLOR_WHITE     0xFFFF
#define COLOR_RED       0xF800
#define COLOR_YELLOW    0xFFE0
#define COLOR_BLUE      0x001F

// Helper: Interpolate between blue and white based on line position
uint16_t gradientColor(int line, int totalLines) {
    // Blue (0x001F) to White (0xFFFF) gradient
    // RGB565: Blue = R:0, G:0, B:31 -> White = R:31, G:63, B:31
    float ratio = (float)line / (float)totalLines;

    uint8_t r = (uint8_t)(ratio * 31);
    uint8_t g = (uint8_t)(ratio * 63);
    uint8_t b = 31; // Keep blue channel max

    return ((r & 0x1F) << 11) | ((g & 0x3F) << 5) | (b & 0x1F);
}

// Helper: Draw 2x scaled star emoji
void drawStarEmoji2x(int x, int y) {
    uint16_t starBuffer[16];
    for (int sy = 0; sy < 16; sy++) {
        for (int sx = 0; sx < 16; sx++) {
            starBuffer[sx] = pgm_read_word(&STAR_EMOJI_DATA[sy * 16 + sx]);
        }
        // Draw 2x2 blocks for each pixel
        for (int scale_y = 0; scale_y < 2; scale_y++) {
            for (int sx = 0; sx < 16; sx++) {
                if (starBuffer[sx] != STAR_TRANSPARENT) {
                    M5.Display.fillRect(x + (sx * 2), y + (sy * 2) + scale_y, 2, 1, starBuffer[sx]);
                }
            }
        }
    }
}

// Helper: Wait for button press and release (fixes restart bug)
void waitForButtonPressAndRelease() {
    // Wait for button press
    while (true) {
        M5.update();
        if (M5.BtnA.wasPressed() || M5.BtnB.wasPressed()) {
            break;
        }
        yield(); // Feed watchdog
        delay(10);
    }

    // Wait for button release
    while (true) {
        M5.update();
        if (!M5.BtnA.isPressed() && !M5.BtnB.isPressed()) {
            break;
        }
        yield(); // Feed watchdog
        delay(10);
    }
}

String OTAManager::parseJsonField(String json, String field) {
    String searchStr = "\"" + field + "\":\"";
    int startIdx = json.indexOf(searchStr);
    if (startIdx < 0) return "";

    startIdx += searchStr.length();
    int endIdx = json.indexOf("\"", startIdx);
    if (endIdx < 0) return "";

    return json.substring(startIdx, endIdx);
}

bool OTAManager::checkForUpdate() {
    M5.Display.clear();

    // Draw 2x star emoji on right side (centered vertically)
    // Screen is 240x135, star is 32x32, so center at y=(135-32)/2 ≈ 51
    drawStarEmoji2x(240 - 32 - 10, 51);

    // Left margin: 15px, Top margin: 15px
    int lineY = 15;
    int lineSpacing = 10;

    M5.Display.setTextSize(1);

    // Line 0: "Current: vX.X.X" - gradient blue to white
    M5.Display.setCursor(15, lineY);
    M5.Display.setTextColor(gradientColor(0, 5));
    M5.Display.printf("Current: %s", FIRMWARE_VERSION);
    lineY += lineSpacing;

    // Line 1: "Checking for updates..."
    M5.Display.setCursor(15, lineY);
    M5.Display.setTextColor(gradientColor(1, 5));
    M5.Display.println("Checking for updates...");
    lineY += lineSpacing;

    // Debug: Check WiFi status
    M5.Display.setCursor(15, lineY);
    M5.Display.setTextColor(gradientColor(2, 5));
    if (WiFi.status() != WL_CONNECTED) {
        M5.Display.println("WiFi not connected!");
        lineY += lineSpacing;
        M5.Display.setCursor(15, lineY);
        M5.Display.println("Press any button...");
        waitForButtonPressAndRelease();
        return false;
    }
    M5.Display.printf("WiFi: %s", WiFi.SSID().c_str());
    lineY += lineSpacing;

    // Debug: Show RSSI
    M5.Display.setCursor(15, lineY);
    M5.Display.setTextColor(gradientColor(3, 5));
    M5.Display.printf("Signal: %d dBm", WiFi.RSSI());
    lineY += lineSpacing;

    secureClient.setInsecure(); // Skip certificate validation
    secureClient.setHandshakeTimeout(60); // 60 second TLS timeout

    // Debug: Connecting
    M5.Display.setCursor(15, lineY);
    M5.Display.setTextColor(gradientColor(4, 5));
    M5.Display.println("Connecting to GitHub...");
    lineY += lineSpacing;

    HTTPClient http;
    http.begin(secureClient, GITHUB_API_URL);
    http.addHeader("User-Agent", "M5StickC-Laboratory");
    http.setTimeout(60000); // 60 second timeout

    M5.Display.setCursor(15, lineY);
    M5.Display.setTextColor(COLOR_WHITE);
    M5.Display.println("Sending GET request...");
    lineY += lineSpacing;

    int httpCode = http.GET();

    if (httpCode != 200) {
        M5.Display.setCursor(15, lineY);
        M5.Display.setTextColor(gradientColor(2, 5));
        M5.Display.printf("Error: HTTP %d", httpCode);
        lineY += lineSpacing;
        M5.Display.setCursor(15, lineY);
        M5.Display.setTextColor(gradientColor(3, 5));
        M5.Display.println("Press any button...");
        http.end();
        waitForButtonPressAndRelease();
        return false;
    }

    String payload = http.getString();
    http.end();

    // Parse version tag
    String latestVersion = parseJsonField(payload, "tag_name");
    if (latestVersion.length() == 0) {
        M5.Display.setCursor(15, lineY);
        M5.Display.setTextColor(gradientColor(2, 5));
        M5.Display.println("Parse error: No tag found");
        lineY += lineSpacing;
        M5.Display.setCursor(15, lineY);
        M5.Display.setTextColor(gradientColor(3, 5));
        M5.Display.println("Press any button...");
        waitForButtonPressAndRelease();
        return false;
    }

    M5.Display.setCursor(15, lineY);
    M5.Display.setTextColor(gradientColor(2, 5));
    M5.Display.printf("Latest: %s", latestVersion.c_str());
    lineY += lineSpacing;

    // Compare versions
    if (latestVersion == String(FIRMWARE_VERSION)) {
        lineY += lineSpacing; // Extra space
        M5.Display.setCursor(15, lineY);
        M5.Display.setTextColor(gradientColor(3, 5));
        M5.Display.println("Already up to date!");
        lineY += lineSpacing;
        M5.Display.setCursor(15, lineY);
        M5.Display.setTextColor(gradientColor(4, 5));
        M5.Display.println("Press any button...");
        waitForButtonPressAndRelease();
        return false;
    }

    // Find firmware.bin download URL
    String searchStr = "\"browser_download_url\":\"";
    int urlStart = payload.indexOf(searchStr);
    if (urlStart < 0) {
        M5.Display.setCursor(15, lineY);
        M5.Display.setTextColor(gradientColor(3, 5));
        M5.Display.println("No firmware.bin found");
        lineY += lineSpacing;
        M5.Display.setCursor(15, lineY);
        M5.Display.setTextColor(gradientColor(4, 5));
        M5.Display.println("Press any button...");
        waitForButtonPressAndRelease();
        return false;
    }

    urlStart += searchStr.length();
    int urlEnd = payload.indexOf("\"", urlStart);
    String firmwareUrl = payload.substring(urlStart, urlEnd);

    // Confirm only if we find firmware.bin in the URL
    if (firmwareUrl.indexOf("firmware.bin") < 0) {
        M5.Display.setCursor(15, lineY);
        M5.Display.setTextColor(gradientColor(3, 5));
        M5.Display.println("Invalid firmware URL");
        lineY += lineSpacing;
        M5.Display.setCursor(15, lineY);
        M5.Display.setTextColor(gradientColor(4, 5));
        M5.Display.println("Press any button...");
        waitForButtonPressAndRelease();
        return false;
    }

    // Check if update is mandatory
    String releaseName = parseJsonField(payload, "name");
    bool isMandatory = (releaseName.indexOf("[MANDATORY]") >= 0) ||
                       (releaseName.indexOf("[CRITICAL]") >= 0);

    lineY += lineSpacing; // Extra space

    if (isMandatory) {
        // MANDATORY UPDATE - auto-install
        M5.Display.setCursor(15, lineY);
        M5.Display.setTextColor(COLOR_RED);
        M5.Display.println("CRITICAL UPDATE REQUIRED");
        lineY += lineSpacing;
        M5.Display.setCursor(15, lineY);
        M5.Display.setTextColor(COLOR_YELLOW);
        M5.Display.println("Installing automatically...");
        delay(2000); // Give user time to read
        return performUpdate(firmwareUrl);
    }

    // Optional update - show BtnA/BtnB options
    M5.Display.setCursor(15, lineY);
    M5.Display.setTextColor(gradientColor(3, 5));
    M5.Display.println("Update available!");
    lineY += lineSpacing;
    M5.Display.setCursor(15, lineY);
    M5.Display.setTextColor(gradientColor(4, 5));
    M5.Display.println("BtnA = Install");
    lineY += lineSpacing;
    M5.Display.setCursor(15, lineY);
    M5.Display.setTextColor(COLOR_WHITE);
    M5.Display.println("BtnB = Cancel");

    // Wait for user input
    while (true) {
        M5.update();
        if (M5.BtnA.wasPressed()) {
            return performUpdate(firmwareUrl);
        }
        if (M5.BtnB.wasPressed()) {
            lineY += lineSpacing;
            M5.Display.setCursor(15, lineY);
            M5.Display.setTextColor(COLOR_WHITE);
            M5.Display.println("Cancelled.");
            delay(1000);
            return false;
        }
        yield(); // Feed watchdog
        delay(10);
    }

    return false;
}

bool OTAManager::performUpdate(String firmwareURL) {
    M5.Display.clear();

    // Draw 2x star emoji on right side (centered vertically)
    drawStarEmoji2x(240 - 32 - 10, 51);

    // Left margin: 15px, Top margin: 15px
    int lineY = 15;
    int lineSpacing = 10;

    M5.Display.setTextSize(1);

    // Line 0: "Downloading firmware..."
    M5.Display.setCursor(15, lineY);
    M5.Display.setTextColor(gradientColor(0, 5));
    M5.Display.println("Downloading firmware...");
    lineY += lineSpacing;

    // Line 1: Firmware URL (truncate if too long)
    M5.Display.setCursor(15, lineY);
    M5.Display.setTextColor(gradientColor(1, 5));
    String displayURL = firmwareURL;
    if (displayURL.length() > 35) {
        displayURL = displayURL.substring(0, 32) + "...";
    }
    M5.Display.println(displayURL);
    lineY += lineSpacing;

    // Debug: WiFi check
    M5.Display.setCursor(15, lineY);
    M5.Display.setTextColor(gradientColor(2, 5));
    if (WiFi.status() != WL_CONNECTED) {
        M5.Display.println("WiFi disconnected!");
        lineY += lineSpacing;
        M5.Display.setCursor(15, lineY);
        M5.Display.println("Press any button...");
        waitForButtonPressAndRelease();
        return false;
    }
    M5.Display.printf("WiFi OK (%d dBm)", WiFi.RSSI());
    lineY += lineSpacing;

    secureClient.setInsecure(); // Skip certificate validation
    secureClient.setHandshakeTimeout(60); // 60 second TLS timeout

    lineY += lineSpacing; // Extra space
    M5.Display.setCursor(15, lineY);
    M5.Display.setTextColor(gradientColor(3, 5));
    M5.Display.println("TLS handshake...");
    lineY += lineSpacing;

    HTTPClient http;
    http.begin(secureClient, firmwareURL);
    http.setTimeout(60000); // 60 second timeout for download
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

    M5.Display.setCursor(15, lineY);
    M5.Display.setTextColor(gradientColor(4, 5));
    M5.Display.println("Requesting firmware...");
    lineY += lineSpacing;

    int httpCode = http.GET();

    M5.Display.setCursor(15, lineY);
    M5.Display.setTextColor(COLOR_WHITE);
    M5.Display.printf("Response: %d", httpCode);
    lineY += lineSpacing;

    if (httpCode != 200) {
        lineY += lineSpacing;
        M5.Display.setCursor(15, lineY);
        M5.Display.setTextColor(gradientColor(4, 5));
        M5.Display.printf("Download failed: %d", httpCode);
        lineY += lineSpacing;
        if (httpCode == -1) {
            M5.Display.setCursor(15, lineY);
            M5.Display.setTextColor(COLOR_WHITE);
            M5.Display.println("Connection error");
            lineY += lineSpacing;
            M5.Display.setCursor(15, lineY);
            M5.Display.println("Check WiFi signal");
            lineY += lineSpacing;
        }
        M5.Display.setCursor(15, lineY);
        M5.Display.setTextColor(COLOR_WHITE);
        M5.Display.println("Press any button...");
        http.end();
        waitForButtonPressAndRelease();
        return false;
    }

    int contentLength = http.getSize();
    lineY += lineSpacing;
    M5.Display.setCursor(15, lineY);
    M5.Display.setTextColor(gradientColor(4, 5));
    M5.Display.printf("Size: %d bytes", contentLength);
    lineY += lineSpacing;

    if (contentLength <= 0) {
        M5.Display.setCursor(15, lineY);
        M5.Display.setTextColor(COLOR_WHITE);
        M5.Display.println("Invalid content length");
        lineY += lineSpacing;
        M5.Display.setCursor(15, lineY);
        M5.Display.println("Press any button...");
        http.end();
        waitForButtonPressAndRelease();
        return false;
    }

    bool canBegin = Update.begin(contentLength);
    if (!canBegin) {
        M5.Display.setCursor(15, lineY);
        M5.Display.setTextColor(COLOR_WHITE);
        M5.Display.println("Not enough space!");
        lineY += lineSpacing;
        M5.Display.setCursor(15, lineY);
        M5.Display.printf("Needed: %d bytes", contentLength);
        lineY += lineSpacing;
        M5.Display.setCursor(15, lineY);
        M5.Display.println("Press any button...");
        http.end();
        waitForButtonPressAndRelease();
        return false;
    }

    WiFiClient* stream = http.getStreamPtr();
    size_t written = 0;
    uint8_t buff[128];
    int lastProgress = -1;

    while (http.connected() && (written < contentLength)) {
        size_t available = stream->available();
        if (available) {
            int c = stream->readBytes(buff, min(available, sizeof(buff)));
            Update.write(buff, c);
            written += c;

            int progress = (written * 100) / contentLength;
            if (progress != lastProgress) {
                displayProgress(written, contentLength);
                lastProgress = progress;
            }
        }
        delay(1);
    }

    http.end();

    if (written != contentLength) {
        M5.Display.setCursor(15, 100);
        M5.Display.setTextColor(COLOR_WHITE);
        M5.Display.printf("Write incomplete!");
        M5.Display.setCursor(15, 110);
        M5.Display.printf("Written: %d / %d", written, contentLength);
        M5.Display.setCursor(15, 120);
        M5.Display.println("Press any button...");
        Update.abort();
        waitForButtonPressAndRelease();
        return false;
    }

    if (Update.end()) {
        if (Update.isFinished()) {
            M5.Display.setCursor(15, 100);
            M5.Display.setTextColor(COLOR_WHITE);
            M5.Display.println("Update complete!");
            M5.Display.setCursor(15, 110);
            M5.Display.println("Rebooting in 3s...");
            delay(3000);
            ESP.restart();
            return true;
        }
    }

    M5.Display.setCursor(15, 100);
    M5.Display.setTextColor(COLOR_WHITE);
    M5.Display.println("Update failed!");
    M5.Display.setCursor(15, 110);
    M5.Display.printf("Error: %s", Update.errorString());
    M5.Display.setCursor(15, 120);
    M5.Display.println("Press any button...");
    waitForButtonPressAndRelease();
    return false;
}

void OTAManager::displayProgress(int current, int total) {
    int percent = (current * 100) / total;
    int barWidth = 200;
    int barHeight = 20;
    int barX = 20;
    int barY = 115; // Bottom of screen (135 - 20)

    // Draw progress bar background
    M5.Display.drawRect(barX, barY, barWidth, barHeight, COLOR_WHITE);

    // Draw progress bar fill (YELLOW = 0xFFE0)
    int fillWidth = (barWidth * current) / total;
    M5.Display.fillRect(barX + 2, barY + 2, fillWidth - 4, barHeight - 4, COLOR_YELLOW);

    // Draw percentage text centered inside the bar
    M5.Display.setTextSize(1);
    M5.Display.setCursor(barX + barWidth / 2 - 12, barY + 6);
    M5.Display.setTextColor(COLOR_BLACK, COLOR_YELLOW); // Black text on yellow background
    M5.Display.printf("%d%%", percent);
}
