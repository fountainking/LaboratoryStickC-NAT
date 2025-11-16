#include <M5Unified.h>
#include "captive_portal.h"
#include "portal_content.h"
#include "wifi_transfer.h"
#include "boot_animation.h"
#include "star_emoji.h"
#include "wifi_setup_portal.h"
#include "ota_manager.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ArduinoJson.h>
#include <HTTPUpdate.h>
#include <Preferences.h>

// Forward declarations
void drawPortalDeck();
void drawPortalRunning();
void drawSettings();
void handleNavigation();
void startWiFiSetupPortal();
void stopWiFiSetupPortal();
void handleWiFiPortalLoop();
void drawWiFiPortalRunning();
void drawWiFiStatusDot();
void playBeep(int frequency, int duration);
void drawTransferRunning();
void drawFiles();
void updateScreensaver();
void enterScreensaver();

// Configuration
#define PORTAL_SSID "Laboratory"
#define BTN_C_PIN 35  // GPIO35 for Button C

// Laboratory colors
#define COLOR_BLACK     0x0000
#define COLOR_WHITE     0xFFFF
#define COLOR_RED       0xF800
#define COLOR_YELLOW    0xFFE0
#define COLOR_CYAN      0x07FF
#define COLOR_BLUE      0x001F
#define COLOR_GREEN     0x07E0
#define COLOR_DARKGRAY  0x7BEF
#define COLOR_LIGHTGRAY 0xC618

// UI State
enum UIState {
    UI_PORTAL_DECK,
    UI_PORTAL_RUNNING,
    UI_SETTINGS,
    UI_WIFI_PORTAL_RUNNING,
    UI_TRANSFER_RUNNING,
    UI_FILES,
    UI_SCREENSAVER
};

UIState uiState = UI_PORTAL_DECK;
int selectedMenuIndex = 0;
int selectedSettingIndex = 0;
int selectedFileIndex = 0;  // For Files screen navigation
bool folderExpanded[2] = {false, false};  // Track /html and /media expansion
bool portalRunning = false;
bool wifiPortalRunning = false;
unsigned long lastUpdate = 0;
const unsigned long UPDATE_INTERVAL = 1000;

// Settings state
bool dimEnabled = false;  // Screen dim on/off (default OFF)
bool buzzEnabled = true; // Buzzer on/off

// WiFi credentials from portal
String savedSSID = "";
String savedPassword = "";
bool wifiAttempted = false;  // Track if WiFi connection attempted this session

// WiFi Setup Portal
WebServer* wifiSetupServer = nullptr;
DNSServer* wifiSetupDNS = nullptr;

// Version
#define FIRMWARE_VERSION "1.0.0"

// Manual BtnC state tracking
bool btnC_lastState = HIGH;
bool btnC_wasPressed = false;

// BtnB hold tracking for power off
unsigned long btnB_pressStartTime = 0;
bool btnB_isPressing = false;
const unsigned long POWER_OFF_HOLD_TIME = 2500; // 2.5 seconds (feels like 4)

// BtnA hold tracking for screensaver
unsigned long btnA_pressStartTime = 0;
bool btnA_isPressing = false;
const unsigned long SCREENSAVER_HOLD_TIME = 2500; // 2.5 seconds

// Screensaver state
unsigned long lastActivity = 0;
const unsigned long SCREENSAVER_TIMEOUT = 60000; // 1 minute idle
int starX = 120, starY = 67;  // Center start
int starVelX = 5, starVelY = 5;  // Velocity (faster!)
unsigned long lastStarMove = 0;
int starColorIndex = 0;
uint16_t starColors[7] = {COLOR_RED, COLOR_YELLOW, COLOR_GREEN, COLOR_CYAN, COLOR_BLUE, 0xFCF5, COLOR_WHITE}; // Baby pink = 0xFCF5

// Menu options
#define MAX_MENU_ITEMS 3
struct MenuItem {
    String name;
    String ssid;
};

MenuItem menuItems[MAX_MENU_ITEMS] = {
    {"Portal", "Laboratory"},
    {"Transfer", ""},
    {"Settings", ""}
};

int numMenuItems = 3;

void setup() {
    // Initialize M5Unified
    auto cfg = M5.config();
    cfg.internal_imu = false; // Disable IMU to save resources
    M5.begin(cfg);
    M5.Speaker.begin();  // Initialize speaker/buzzer
    M5.Display.setRotation(1); // Landscape mode (240x135)
    M5.Display.setBrightness(200);
    M5.Display.fillScreen(COLOR_BLACK);

    Serial.begin(115200);
    delay(500); // Give serial time to stabilize
    Serial.flush(); // Clear any boot garbage

    // Initialize BtnC GPIO
    pinMode(BTN_C_PIN, INPUT_PULLUP);

    // Clear banner
    Serial.println("\n\n\n\n");
    Serial.println("========================================");
    Serial.println("    Laboratory StickC Plus 2 v1.0");
    Serial.println("========================================");
    Serial.printf("BtnC GPIO: %d\n", BTN_C_PIN);
    Serial.println("----------------------------------------\n");

    // Boot animation
    playBootAnimation();
    delay(500);

    // Try to auto-connect to saved WiFi (minimal memory)
    Preferences prefs;
    if (prefs.begin("wifi", true)) {  // Read-only
        String savedWifiSSID = prefs.getString("ssid", "");
        String savedWifiPass = prefs.getString("pass", "");
        prefs.end();

        if (savedWifiSSID.length() > 0) {
            Serial.println("[WiFi] Found saved credentials, attempting auto-connect...");
            savedSSID = savedWifiSSID;
            savedPassword = savedWifiPass;
            wifiAttempted = true;  // Mark as attempted so we know credentials exist

            WiFi.mode(WIFI_STA);
            WiFi.begin(savedWifiSSID.c_str(), savedWifiPass.c_str());

            int attempts = 0;
            while (WiFi.status() != WL_CONNECTED && attempts < 20) {
                delay(500);
                attempts++;
            }

            if (WiFi.status() == WL_CONNECTED) {
                Serial.printf("[WiFi] Auto-connected! IP: %s\n", WiFi.localIP().toString().c_str());
            } else {
                Serial.println("[WiFi] Auto-connect failed - will keep credentials and retry in background");
                // Don't turn off WiFi - let it keep trying to reconnect automatically
            }
        }
    }

    // Show portal deck
    uiState = UI_PORTAL_DECK;
    drawPortalDeck();

    // Initialize activity timer
    lastActivity = millis();

    Serial.println("\n[Main] Ready!");
    Serial.println("[Main] BtnC = Navigate | BtnA = Select/Launch");
}

void loop() {
    M5.update();

    // Manual BtnC detection
    bool btnC_currentState = digitalRead(BTN_C_PIN);
    btnC_wasPressed = false;
    if (btnC_lastState == HIGH && btnC_currentState == LOW) {
        btnC_wasPressed = true;
        Serial.println("[Debug] BtnC pressed (GPIO)!");
    }
    btnC_lastState = btnC_currentState;

    // Debug button presses
    if (M5.BtnA.wasPressed()) Serial.println("[Debug] BtnA pressed!");
    if (M5.BtnB.wasPressed()) Serial.println("[Debug] BtnB pressed!");

    // Check for screensaver activation (idle timeout or manual)
    if (uiState != UI_SCREENSAVER) {
        // Auto-launch screensaver after idle timeout
        if (millis() - lastActivity > SCREENSAVER_TIMEOUT) {
            enterScreensaver();
        }

        // Reset activity on any button press
        if (M5.BtnA.wasPressed() || M5.BtnB.wasPressed() || btnC_wasPressed) {
            lastActivity = millis();
        }
    } else {
        // Wake from screensaver on any button press
        if (M5.BtnA.wasPressed() || M5.BtnB.wasPressed() || btnC_wasPressed) {
            uiState = UI_PORTAL_DECK;
            M5.Display.setBrightness(dimEnabled ? 20 : 200);
            lastActivity = millis();
            drawPortalDeck();
            Serial.println("[Screensaver] Woke from screensaver");
        }
    }

    handleNavigation();

    // Update portal loop (runs in background during screensaver too!)
    if (portalRunning) {
        handlePortalLoop();
    }

    // Update portal running display (only if on portal screen)
    if (uiState == UI_PORTAL_RUNNING && portalRunning) {
        if (millis() - lastUpdate >= UPDATE_INTERVAL) {
            lastUpdate = millis();
            drawPortalRunning();
        }
    }

    // Update WiFi portal (runs in background even when not on WiFi screen)
    if (wifiPortalRunning) {
        handleWiFiPortalLoop();
    }

    // Update WiFi portal display if on WiFi screen
    if (uiState == UI_WIFI_PORTAL_RUNNING && wifiPortalRunning) {
        if (millis() - lastUpdate >= UPDATE_INTERVAL) {
            lastUpdate = millis();
            drawWiFiPortalRunning();
        }
    }

    // Handle Transfer server
    if (uiState == UI_TRANSFER_RUNNING) {
        handleTransferLoop();
    }

    // Update screensaver
    if (uiState == UI_SCREENSAVER) {
        updateScreensaver();
    }

    delay(10);
}

void handleNavigation() {
    if (uiState == UI_PORTAL_DECK) {
        // BtnC = Navigate down (cycle through menu)
        if (btnC_wasPressed) {
            selectedMenuIndex = (selectedMenuIndex + 1) % numMenuItems;
            playBeep(800, 50);  // Nav beep
            drawPortalDeck();
            Serial.println("[UI] Selected: " + menuItems[selectedMenuIndex].name);
        }

        // BtnA long press = Launch screensaver (check before quick press)
        if (M5.BtnA.isPressed()) {
            if (!btnA_isPressing) {
                btnA_isPressing = true;
                btnA_pressStartTime = millis();
            }
            unsigned long holdTime = millis() - btnA_pressStartTime;
            if (holdTime >= SCREENSAVER_HOLD_TIME) {
                enterScreensaver();
                btnA_isPressing = false;
                return;  // Don't process quick press
            }
        } else {
            // Only process quick press if it wasn't a long press
            if (btnA_isPressing && millis() - btnA_pressStartTime < SCREENSAVER_HOLD_TIME) {
                // This was a quick press, will be handled by wasPressed() below
            }
            btnA_isPressing = false;
        }

        // BtnB = Back (quick press) or Power off (hold 2.5s)
        if (M5.BtnB.wasPressed()) {
            // Quick press - go back (same as BtnA for now)
            Serial.println("[UI] BtnB quick press - ignored on main menu");
        }

        if (M5.BtnB.isPressed()) {
            if (!btnB_isPressing) {
                btnB_isPressing = true;
                btnB_pressStartTime = millis();
            }
            unsigned long holdTime = millis() - btnB_pressStartTime;
            if (holdTime >= POWER_OFF_HOLD_TIME) {
                Serial.println("[UI] Powering off...");
                M5.Display.fillScreen(COLOR_BLACK);
                M5.Display.setTextSize(2);
                M5.Display.setTextColor(COLOR_YELLOW);
                M5.Display.setCursor(72, 60);  // Centered (8 chars * 12px = 96px, (240-96)/2 = 72)
                M5.Display.println("Goodbye!");
                delay(1000);
                M5.Power.powerOff();
            }
        } else {
            btnB_isPressing = false;
        }

        // BtnA = Launch selected option
        if (M5.BtnA.wasPressed()) {
            if (selectedMenuIndex == 0) {
                // Laboratory portal - stop WiFi setup portal if running
                if (wifiPortalRunning) {
                    stopWiFiSetupPortal();
                }
                String portalHTML = getStoredPortalHTML();
                startCaptivePortal(menuItems[selectedMenuIndex].ssid, portalHTML);
                portalRunning = true;
                uiState = UI_PORTAL_RUNNING;
                drawPortalRunning();
                Serial.println("[UI] Launched: " + menuItems[selectedMenuIndex].name);
            } else if (selectedMenuIndex == 1) {
                // Transfer - Start web server for uploading portal HTML
                if (WiFi.status() != WL_CONNECTED) {
                    M5.Display.fillScreen(COLOR_BLACK);
                    M5.Display.setTextSize(2);
                    M5.Display.setTextColor(COLOR_RED);
                    M5.Display.setCursor(30, 50);  // Centered (14 chars * 12px = 168px, (240-168)/2 = 36, adjusted to 30)
                    M5.Display.println("WiFi Required!");
                    M5.Display.setTextSize(1);
                    M5.Display.setTextColor(COLOR_LIGHTGRAY);
                    M5.Display.setCursor(20, 80);  // Centered (25 chars * 6px = 150px, (240-150)/2 = 45, adjusted to 20)
                    M5.Display.println("Connect via Settings first");
                    delay(2000);
                    drawPortalDeck();
                    Serial.println("[UI] Transfer - WiFi not connected");
                } else {
                    startTransferServer();
                    uiState = UI_TRANSFER_RUNNING;
                    drawTransferRunning();
                    Serial.println("[UI] Transfer server started");
                }
            } else if (selectedMenuIndex == 2) {
                // Settings - Enter settings menu
                uiState = UI_SETTINGS;
                selectedSettingIndex = 0;
                drawSettings();
                Serial.println("[UI] Entered Settings");
            }
        }
    }
    else if (uiState == UI_SETTINGS) {
        // BtnC = Navigate settings
        if (btnC_wasPressed) {
            selectedSettingIndex = (selectedSettingIndex + 1) % 5;  // 5 settings (WiFi, Sound, Dim, Update, Files)
            playBeep(800, 50);  // Nav beep
            drawSettings();
            Serial.println("[UI] Setting: " + String(selectedSettingIndex));
        }

        // BtnB = Back to main menu
        if (M5.BtnB.wasPressed()) {
            uiState = UI_PORTAL_DECK;
            drawPortalDeck();
            Serial.println("[UI] Back to main menu");
        }

        // BtnA = Select/Toggle setting
        if (M5.BtnA.wasPressed()) {
            if (selectedSettingIndex == 0) {
                // WiFi - Launch WiFi setup portal (stop Laboratory portal if running)
                if (portalRunning) {
                    stopCaptivePortal();
                    portalRunning = false;
                }
                startWiFiSetupPortal();
                uiState = UI_WIFI_PORTAL_RUNNING;
                drawWiFiPortalRunning();
                Serial.println("[UI] WiFi setup portal launched");
            } else if (selectedSettingIndex == 1) {
                // Sound/Buzz toggle
                buzzEnabled = !buzzEnabled;
                if (buzzEnabled) {
                    // Beep to confirm
                    M5.Speaker.tone(1000, 100);
                    delay(100);
                    M5.Speaker.tone(1500, 100);
                }
                drawSettings();
                Serial.println("[Settings] Buzz: " + String(buzzEnabled ? "ON" : "OFF"));
            } else if (selectedSettingIndex == 2) {
                // Dim toggle - check battery first
                int batteryLevel = M5.Power.getBatteryLevel();
                if (batteryLevel < 20) {
                    // Battery low - force dim ON
                    dimEnabled = true;
                    M5.Display.setBrightness(20);
                } else {
                    dimEnabled = !dimEnabled;
                    M5.Display.setBrightness(dimEnabled ? 20 : 200);
                }
                drawSettings();
                Serial.println("[Settings] Dim: " + String(dimEnabled ? "ON" : "OFF"));
            } else if (selectedSettingIndex == 3) {
                // Update - Launch OTA update check
                Serial.println("[Settings] Launching OTA update check...");
                OTAManager::checkForUpdate();
                // Return to settings after OTA check
                drawSettings();
                Serial.println("[Settings] Returned from OTA check");
            } else if (selectedSettingIndex == 4) {
                // Files - Enter Files UI
                uiState = UI_FILES;
                selectedFileIndex = 0;
                drawFiles();
                Serial.println("[UI] Entered Files");
            }
        }
    }
    else if (uiState == UI_PORTAL_RUNNING) {
        // BtnB = Back to main menu
        if (M5.BtnB.wasPressed()) {
            if (portalRunning) {
                stopCaptivePortal();
                portalRunning = false;
                Serial.println("[UI] Stopped portal");
            }
            uiState = UI_PORTAL_DECK;
            drawPortalDeck();
            Serial.println("[UI] Back to main menu");
        }
    }
    else if (uiState == UI_WIFI_PORTAL_RUNNING) {
        // BtnB = Back to settings menu (keep portal running)
        if (M5.BtnB.wasPressed()) {
            uiState = UI_SETTINGS;
            drawSettings();
            Serial.println("[UI] Back to settings (portal still running)");
        }
    }
    else if (uiState == UI_TRANSFER_RUNNING) {
        // BtnB = Back to main menu (stop transfer server)
        if (M5.BtnB.wasPressed()) {
            stopTransferServer();
            uiState = UI_PORTAL_DECK;
            drawPortalDeck();
            Serial.println("[UI] Stopped transfer server");
        }
    }
    else if (uiState == UI_FILES) {
        // BtnB = Back to Settings menu
        if (M5.BtnB.wasPressed()) {
            uiState = UI_SETTINGS;
            drawSettings();
            Serial.println("[UI] Back to Settings from Files");
        }
        // BtnC = Navigate through folders
        if (btnC_wasPressed) {
            selectedFileIndex = (selectedFileIndex + 1) % 3;  // 3 items: /html, /media, README
            playBeep(800, 50);
            drawFiles();
            Serial.println("[UI] Files - Selected: " + String(selectedFileIndex));
        }
        // BtnA = Toggle folder expansion or show README
        if (M5.BtnA.wasPressed()) {
            if (selectedFileIndex == 0) {
                // Toggle /html folder
                folderExpanded[0] = !folderExpanded[0];
                playBeep(1000, 50);
                drawFiles();
                Serial.println("[UI] /html folder " + String(folderExpanded[0] ? "expanded" : "collapsed"));
            } else if (selectedFileIndex == 1) {
                // Toggle /media folder
                folderExpanded[1] = !folderExpanded[1];
                playBeep(1000, 50);
                drawFiles();
                Serial.println("[UI] /media folder " + String(folderExpanded[1] ? "expanded" : "collapsed"));
            } else if (selectedFileIndex == 2) {
                // Show README content
                M5.Display.fillScreen(COLOR_BLACK);
                M5.Display.setTextSize(2);
                M5.Display.setTextColor(COLOR_YELLOW);
                M5.Display.setCursor(70, 10);
                M5.Display.println("README");
                M5.Display.setTextSize(1);
                M5.Display.setTextColor(COLOR_WHITE);
                M5.Display.setCursor(10, 35);
                M5.Display.println("Laboratory Portal Device");
                M5.Display.setCursor(10, 50);
                M5.Display.println("Upload HTML: Use Transfer");
                M5.Display.setCursor(10, 65);
                M5.Display.println("Upload Media: Use Transfer");
                M5.Display.setCursor(10, 85);
                M5.Display.setTextColor(COLOR_LIGHTGRAY);
                M5.Display.println("Max sizes:");
                M5.Display.setCursor(10, 97);
                M5.Display.println("  HTML: 100KB");
                M5.Display.setCursor(10, 109);
                M5.Display.println("  Media: 2MB");
                M5.Display.setTextColor(COLOR_DARKGRAY);
                M5.Display.setCursor(75, 125);
                M5.Display.print("BtnB = Back");
                playBeep(1200, 50);
                Serial.println("[UI] Showing README");
            }
        }
    }
}

void drawPortalDeck() {
    M5.Display.fillScreen(COLOR_BLACK);

    // Title: "labPORTAL" - left aligned, size 3
    M5.Display.setTextSize(3);
    M5.Display.setTextColor(COLOR_YELLOW);
    M5.Display.setCursor(10, 5);
    M5.Display.print("labPORTAL");
    // Title height: 5 (top) + 24 (text height at size 3) = 29

    // Show WiFi network name with status dot below title (with spacing)
    if (WiFi.status() == WL_CONNECTED) {
        String wifiName = WiFi.SSID();
        if (wifiName.length() > 18) {
            wifiName = wifiName.substring(0, 18);
        }

        // Draw WiFi name in blue gradient - aligned at x=20
        M5.Display.setTextSize(1);
        int xPos = 20;  // Start at x=20
        int yPos = 34;  // Vertically centered

        // Draw status dot: BLUE if portal broadcasting, GREEN if just WiFi
        uint16_t dotColor = isPortalRunning() ? COLOR_BLUE : COLOR_GREEN;
        M5.Display.fillCircle(14, 38, 3, dotColor);  // 6px to the left of text

        for (int i = 0; i < wifiName.length(); i++) {
            // Gradient from light blue to dark blue
            uint8_t progress = (i * 255) / max(1, (int)wifiName.length() - 1);
            uint16_t r = map(progress, 0, 255, 15, 0);
            uint16_t g = map(progress, 0, 255, 31, 0);
            uint16_t b = 31;
            uint16_t color = ((r & 0x1F) << 11) | ((g & 0x3F) << 5) | (b & 0x1F);

            M5.Display.setTextColor(color);
            M5.Display.setCursor(xPos + (i * 6), yPos);
            M5.Display.print(wifiName[i]);
        }
    }

    // Draw menu list - Size 2 (5px margin below AP name)
    M5.Display.setTextSize(2);
    // AP line: 34 (text top) + 8 (text height) = 42, then 5px gap = 47
    int yStart = 47;
    int rowHeight = 22;

    // Calculate scroll offset to keep selected item visible
    int scrollOffset = 0;
    int visibleRows = 3;  // Can fit ~3 items on screen
    if (selectedMenuIndex >= visibleRows) {
        scrollOffset = -(selectedMenuIndex - visibleRows + 1) * rowHeight;
    }

    for (int i = 0; i < numMenuItems; i++) {
        int yPos = yStart + (i * rowHeight) + scrollOffset;

        // Only draw if visible on screen
        if (yPos >= 40 && yPos < 135) {
            // Highlight selected item
            if (i == selectedMenuIndex) {
                // Settings gets yellow highlight, others get blue
                if (i == 2) {  // Settings is index 2
                    M5.Display.fillRoundRect(5, yPos - 2, 230, 20, 3, COLOR_YELLOW);
                    M5.Display.setTextColor(COLOR_BLACK);
                } else {
                    M5.Display.fillRoundRect(5, yPos - 2, 230, 20, 3, COLOR_BLUE);
                    M5.Display.setTextColor(COLOR_WHITE);
                }
            } else {
                // Settings is yellow when not selected, others are gray
                if (i == 2) {
                    M5.Display.setTextColor(COLOR_YELLOW);
                } else {
                    M5.Display.setTextColor(COLOR_LIGHTGRAY);
                }
            }

            // Menu name
            String name = menuItems[i].name;
            if (name.length() > 15) {
                name = name.substring(0, 15) + "..";
            }
            M5.Display.setCursor(20, yPos);  // Move right to x=20
            M5.Display.println(name);

            // Draw star emoji for Settings (right-aligned, 1x native size)
            if (i == 2) {  // Settings is index 2
                int starX = 224;  // Right-aligned
                int starY = yPos + 8;  // Center vertically with text

                // Draw yellow star upright (1x size = 16x16 native) with black outline
                for (int sy = 0; sy < 16; sy++) {
                    for (int sx = 0; sx < 16; sx++) {
                        uint16_t pixelColor = pgm_read_word(&STAR_EMOJI_DATA[sy * 16 + sx]);
                        if (pixelColor != STAR_TRANSPARENT) {
                            // Keep black outline, fill with yellow
                            if (pixelColor != COLOR_BLACK) {
                                pixelColor = COLOR_YELLOW;
                            }
                            // Draw single pixel (native 16x16 size)
                            M5.Display.drawPixel(starX - 8 + sx, starY - 8 + sy, pixelColor);
                        }
                    }
                }
            }
        }
    }
}

void drawSettings() {
    M5.Display.fillScreen(COLOR_BLACK);

    // Settings list - scrollable list showing all items
    M5.Display.setTextSize(2);
    int yStart = 10;
    int rowHeight = 24;

    // Total of 5 settings (0-4): WiFi, Sound, Dim, Update, Files
    const char* settingNames[5] = {"WiFi", "Sound/Buzz", "Dim", "Update", "Files"};

    // Draw all 5 items (should fit in 135px height: 10 + 5*24 = 130px)
    for (int i = 0; i < 5; i++) {
        int yPos = yStart + (i * rowHeight);

        // Highlight selected item
        if (i == selectedSettingIndex) {
            M5.Display.fillRoundRect(5, yPos - 2, 230, 20, 3, COLOR_BLUE);
            M5.Display.setTextColor(COLOR_WHITE);
        } else {
            M5.Display.setTextColor(COLOR_LIGHTGRAY);
        }

        M5.Display.setCursor(10, yPos);

        // Setting-specific rendering
        if (i == 0) {  // WiFi
            M5.Display.println("WiFi");
        } else if (i == 1) {  // Sound/Buzz
            M5.Display.print("Sound ");
            M5.Display.setTextColor(buzzEnabled ? COLOR_YELLOW : COLOR_RED);
            M5.Display.println(buzzEnabled ? "ON" : "OFF");
        } else if (i == 2) {  // Dim
            M5.Display.print("Dim ");
            int batteryLevel = M5.Power.getBatteryLevel();
            if (batteryLevel < 20) {
                M5.Display.setTextColor(COLOR_RED);
                M5.Display.println("LoBat");
            } else {
                M5.Display.setTextColor(dimEnabled ? COLOR_YELLOW : COLOR_RED);
                M5.Display.println(dimEnabled ? "ON" : "OFF");
            }
        } else if (i == 3) {  // Update
            M5.Display.print("Update v");
            M5.Display.println(FIRMWARE_VERSION);
        } else if (i == 4) {  // Files
            M5.Display.println("Files");
        }
    }

    // Draw WiFi status dot
    drawWiFiStatusDot();
}

void drawTransferRunning() {
    M5.Display.fillScreen(COLOR_BLACK);
    M5.Display.setTextSize(2);

    // Title - centered
    M5.Display.setTextColor(COLOR_YELLOW);
    M5.Display.setCursor(60, 10);  // TRANSFER = 8 chars * 12px = 96px, (240-96)/2 = 72, adjusted to 60
    M5.Display.println("TRANSFER");

    // Status - centered
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(COLOR_WHITE);
    M5.Display.setCursor(55, 40);  // Centered
    M5.Display.println("Server running");

    // IP address - centered
    M5.Display.setTextColor(COLOR_YELLOW);
    String ip = "http://" + WiFi.localIP().toString();
    int ipWidth = ip.length() * 6;  // Size 1 = 6px per char
    M5.Display.setCursor((240 - ipWidth) / 2, 55);
    M5.Display.println(ip);

    // Upload count - centered
    M5.Display.setTextColor(COLOR_LIGHTGRAY);
    String uploadStr = "Uploads: " + String(uploadCount);
    int uploadWidth = uploadStr.length() * 6;
    M5.Display.setCursor((240 - uploadWidth) / 2, 75);
    M5.Display.println(uploadStr);

    // Instructions - centered
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(COLOR_DARKGRAY);
    M5.Display.setCursor(30, 100);  // Centered
    M5.Display.println("Upload portal HTML");
    M5.Display.setCursor(50, 112);  // Centered
    M5.Display.println("via browser");

    // Back button hint - centered
    M5.Display.setCursor(75, 125);  // Centered
    M5.Display.print("BtnB = Back");
}

void drawFiles() {
    M5.Display.fillScreen(COLOR_BLACK);
    M5.Display.setTextSize(2);
    M5.Display.setTextColor(COLOR_YELLOW);
    M5.Display.setCursor(10, 5);
    M5.Display.println("FILES");

    // File/folder items
    const char* items[3] = {"/html", "/media", "README"};
    int yPositions[3] = {35, 65, 95};

    // Check if any folder is expanded
    bool anyExpanded = folderExpanded[0] || folderExpanded[1];

    for (int i = 0; i < 3; i++) {
        // Skip non-selected items if a folder is expanded (to prevent overlap)
        if (anyExpanded && i != selectedFileIndex) continue;

        M5.Display.setTextSize(3);

        // Highlight selected item with yellow background
        if (i == selectedFileIndex) {
            M5.Display.fillRoundRect(5, yPositions[i] - 3, 230, 28, 3, COLOR_YELLOW);
            M5.Display.setTextColor(COLOR_BLACK);
        } else {
            M5.Display.setTextColor(COLOR_WHITE);
        }

        M5.Display.setCursor(10, yPositions[i]);

        // Show expansion indicator for folders
        if (i < 2) {  // /html and /media are folders
            if (folderExpanded[i]) {
                M5.Display.print("v ");  // Expanded indicator
            } else {
                M5.Display.print("> ");  // Collapsed indicator
            }
        }
        M5.Display.println(items[i]);

        // Show folder contents if expanded
        if (i < 2 && folderExpanded[i]) {
            M5.Display.setTextSize(1);
            M5.Display.setTextColor(COLOR_DARKGRAY);
            M5.Display.setCursor(30, yPositions[i] + 22);
            M5.Display.print("(empty)");
            M5.Display.setTextSize(3);
        }
    }

    // Footer info (only show if nothing expanded)
    if (!anyExpanded) {
        M5.Display.setTextSize(1);
        M5.Display.setTextColor(COLOR_DARKGRAY);
        M5.Display.setCursor(5, 122);
        M5.Display.print("Max: 100KB html, 2MB media");
    }
}

void drawPortalRunning() {
    M5.Display.fillScreen(COLOR_BLACK);
    M5.Display.setTextSize(1);

    // Portal name at top (red)
    M5.Display.setTextColor(COLOR_RED);
    M5.Display.setCursor(10, 5);
    String runningPortal = menuItems[selectedMenuIndex].name;
    if (runningPortal.length() > 30) {
        runningPortal = runningPortal.substring(0, 30);
    }
    M5.Display.println(runningPortal);

    // SSID below name (yellow)
    M5.Display.setTextColor(COLOR_YELLOW);
    M5.Display.setCursor(10, 18);
    M5.Display.println(getPortalSSID());

    // IP address
    M5.Display.setTextColor(COLOR_YELLOW);
    M5.Display.setCursor(10, 31);
    M5.Display.println(getPortalIP().toString());

    // Visitor count - BIG in center
    M5.Display.setTextColor(COLOR_YELLOW);
    M5.Display.setTextSize(5);
    int visitors = getPortalVisitorCount();
    String visitorStr = String(visitors);
    // Center text (each char is ~30px wide at size 5)
    int textWidth = visitorStr.length() * 30;
    int xPos = (240 - textWidth) / 2;
    M5.Display.setCursor(xPos, 55);
    M5.Display.println(visitorStr);

    // "VISITORS" label
    M5.Display.setTextSize(2);
    M5.Display.setTextColor(COLOR_WHITE);
    M5.Display.setCursor(70, 100);
    M5.Display.println("VISITORS");

    // Instructions
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(COLOR_DARKGRAY);
    M5.Display.setCursor(85, 122);
    M5.Display.println("BtnA=Stop");

    // Status indicator - pulsing dot
    int brightness = (millis() / 500) % 2;
    uint16_t dotColor = brightness ? COLOR_RED : COLOR_DARKGRAY;
    M5.Display.fillCircle(230, 7, 3, dotColor);
}

// WiFi Setup Portal Functions
void startWiFiSetupPortal() {
    stopWiFiSetupPortal();

    Serial.println("[WiFi Setup] Starting WiFi Setup Portal...");

    // Start AP mode
    WiFi.mode(WIFI_AP);
    delay(100);

    IPAddress local_IP(192, 168, 4, 1);
    IPAddress gateway(192, 168, 4, 1);
    IPAddress subnet(255, 255, 255, 0);

    if (!WiFi.softAPConfig(local_IP, gateway, subnet)) {
        Serial.println("[WiFi Setup] ERROR: softAPConfig failed!");
    }

    if (!WiFi.softAP("Laboratory-WiFiSetup")) {
        Serial.println("[WiFi Setup] ERROR: softAP start failed!");
    }

    delay(200);
    IPAddress IP = WiFi.softAPIP();
    Serial.println("[WiFi Setup] AP Started! SSID: Laboratory-WiFiSetup");
    Serial.println("[WiFi Setup] IP: " + IP.toString());

    // Start DNS server
    wifiSetupDNS = new DNSServer();
    wifiSetupDNS->start(53, "*", IP);

    // Start web server
    wifiSetupServer = new WebServer(80);

    // Serve main page
    wifiSetupServer->on("/", HTTP_GET, []() {
        Serial.println("[WiFi Setup] Serving main page");
        wifiSetupServer->send_P(200, "text/html", WIFI_SETUP_HTML);
    });

    // Scan endpoint
    wifiSetupServer->on("/scan", HTTP_GET, []() {
        Serial.println("[WiFi Setup] Scanning networks...");
        int n = WiFi.scanNetworks();

        StaticJsonDocument<2048> doc;
        JsonArray networks = doc.createNestedArray("networks");

        for (int i = 0; i < n && i < 20; i++) {
            JsonObject net = networks.createNestedObject();
            net["ssid"] = WiFi.SSID(i);
            net["rssi"] = WiFi.RSSI(i);
            net["encryption"] = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "open" : "secured";
        }

        String response;
        serializeJson(doc, response);
        wifiSetupServer->send(200, "application/json", response);
        Serial.printf("[WiFi Setup] Found %d networks\n", n);
    });

    // Connect endpoint
    wifiSetupServer->on("/connect", HTTP_POST, []() {
        String body = wifiSetupServer->arg("plain");
        Serial.println("[WiFi Setup] Connect request: " + body);

        StaticJsonDocument<512> doc;
        deserializeJson(doc, body);

        savedSSID = doc["ssid"].as<String>();
        savedPassword = doc["password"].as<String>();

        Serial.printf("[WiFi Setup] Connecting to %s...\n", savedSSID.c_str());

        // Mark that we've attempted WiFi connection
        wifiAttempted = true;

        // Try to connect
        WiFi.mode(WIFI_AP_STA);
        WiFi.begin(savedSSID.c_str(), savedPassword.c_str());

        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 20) {
            delay(500);
            attempts++;
        }

        StaticJsonDocument<256> response;
        if (WiFi.status() == WL_CONNECTED) {
            response["success"] = true;
            response["ip"] = WiFi.localIP().toString();
            response["redirect"] = "/success";  // Redirect to success page
            Serial.printf("[WiFi Setup] Connected! IP: %s\n", WiFi.localIP().toString().c_str());

            // Save WiFi credentials to NVS (minimal memory)
            Preferences prefs;
            if (prefs.begin("wifi", false)) {
                prefs.putString("ssid", savedSSID);
                prefs.putString("pass", savedPassword);
                prefs.end();
                Serial.println("[WiFi] Credentials saved to NVS");
            }
        } else {
            response["success"] = false;
            response["message"] = "Connection failed";
            Serial.println("[WiFi Setup] Connection failed");
        }

        String responseStr;
        serializeJson(response, responseStr);
        wifiSetupServer->send(200, "application/json", responseStr);
    });

    // Success page - auto-redirects to laboratory.mx
    wifiSetupServer->on("/success", HTTP_GET, []() {
        String successHTML = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>WiFi Connected!</title>
    <link rel="stylesheet" href="https://use.typekit.net/wop7tdt.css">
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: 'automate', -apple-system, sans-serif;
            background: #000;
            color: #fff;
            display: flex;
            align-items: center;
            justify-content: center;
            min-height: 100vh;
            padding: 20px;
        }
        .container {
            max-width: 500px;
            text-align: center;
        }
        h1 {
            font-size: 48px;
            color: #FFE000;
            margin-bottom: 20px;
            animation: pulse 1.5s ease-in-out infinite;
        }
        .checkmark {
            font-size: 80px;
            color: #00ff00;
            margin-bottom: 30px;
            animation: bounce 0.6s ease-out;
        }
        p {
            font-size: 18px;
            line-height: 1.6;
            margin-bottom: 30px;
            color: #ccc;
        }
        .url {
            font-size: 32px;
            font-weight: bold;
            color: #FFE000;
            margin: 30px 0;
        }
        .redirect-box {
            background: #111;
            border: 2px solid #00ff00;
            border-radius: 15px;
            padding: 25px;
            margin-top: 30px;
        }
        .countdown {
            font-size: 48px;
            color: #00ff00;
            margin: 20px 0;
            font-weight: bold;
        }
        @keyframes pulse {
            0%, 100% { opacity: 1; }
            50% { opacity: 0.7; }
        }
        @keyframes bounce {
            0% { transform: scale(0); }
            50% { transform: scale(1.2); }
            100% { transform: scale(1); }
        }
    </style>
    <script>
        let countdown = 3;
        function updateCountdown() {
            document.getElementById('countdown').textContent = countdown;
            if (countdown === 0) {
                window.location.href = 'https://laboratory.mx';
            } else {
                countdown--;
                setTimeout(updateCountdown, 1000);
            }
        }
        window.onload = updateCountdown;
    </script>
</head>
<body>
    <div class="container">
        <div class="checkmark">✓</div>
        <h1>CONNECTED!</h1>
        <p>Your device is now online and ready</p>

        <div class="redirect-box">
            <p style="font-size: 20px; color: #fff; margin-bottom: 10px;">Redirecting to Laboratory in</p>
            <div class="countdown" id="countdown">3</div>
            <div class="url">laboratory.mx</div>
            <p style="margin-top: 20px; font-size: 14px; color: #888;">
                If you're not redirected, <a href="https://laboratory.mx" style="color: #FFE000;">click here</a>
            </p>
        </div>

        <p style="margin-top: 30px; font-size: 14px; color: #666;">
            Your device will remember this WiFi network
        </p>
    </div>
</body>
</html>
)rawliteral";
        wifiSetupServer->send(200, "text/html", successHTML);
    });

    // Catch-all for captive portal
    wifiSetupServer->onNotFound([]() {
        Serial.println("[WiFi Setup] Catch-all triggered: " + wifiSetupServer->uri());
        wifiSetupServer->send_P(200, "text/html", WIFI_SETUP_HTML);
    });

    wifiSetupServer->begin();
    wifiPortalRunning = true;

    Serial.println("[WiFi Setup] ===== WIFI SETUP PORTAL RUNNING =====");
    Serial.println("[WiFi Setup] Connect to: Laboratory-WiFiSetup");
    Serial.println("[WiFi Setup] Then visit: http://" + IP.toString());
}

void stopWiFiSetupPortal() {
    if (wifiSetupDNS != nullptr) {
        wifiSetupDNS->stop();
        delete wifiSetupDNS;
        wifiSetupDNS = nullptr;
    }

    if (wifiSetupServer != nullptr) {
        wifiSetupServer->stop();
        delete wifiSetupServer;
        wifiSetupServer = nullptr;
    }

    wifiPortalRunning = false;
    Serial.println("[WiFi Setup] Portal stopped");
}

void handleWiFiPortalLoop() {
    if (wifiPortalRunning) {
        if (wifiSetupDNS != nullptr) {
            wifiSetupDNS->processNextRequest();
        }
        if (wifiSetupServer != nullptr) {
            wifiSetupServer->handleClient();
        }
    }
}

void drawWiFiPortalRunning() {
    M5.Display.fillScreen(COLOR_BLACK);
    M5.Display.setTextSize(2);

    M5.Display.setTextColor(COLOR_YELLOW);
    M5.Display.setCursor(10, 5);
    M5.Display.println("WiFi Setup");

    M5.Display.setTextSize(1);
    M5.Display.setTextColor(COLOR_WHITE);
    M5.Display.setCursor(10, 30);
    M5.Display.println("Connect to:");

    M5.Display.setTextSize(2);
    M5.Display.setTextColor(COLOR_YELLOW);
    M5.Display.setCursor(10, 50);
    M5.Display.println("Laboratory-");
    M5.Display.setCursor(10, 70);
    M5.Display.println("WiFiSetup");

    M5.Display.setTextSize(1);
    M5.Display.setTextColor(COLOR_DARKGRAY);
    M5.Display.setCursor(10, 95);
    M5.Display.println("Setup page will");
    M5.Display.setCursor(10, 107);
    M5.Display.println("pop up automatically");

    if (WiFi.status() == WL_CONNECTED) {
        M5.Display.setTextColor(COLOR_GREEN);
        M5.Display.setCursor(10, 120);
        M5.Display.print("Connected! ");
        M5.Display.println(WiFi.localIP().toString());
    }

    // Pulsing dot (while portal running)
    int brightness = (millis() / 500) % 2;
    uint16_t dotColor = brightness ? COLOR_YELLOW : COLOR_DARKGRAY;
    M5.Display.fillCircle(228, 6, 3, dotColor);
}

// Draw WiFi status dot (persistent across screens)
void drawWiFiStatusDot() {
    // Yellow = No attempt yet, Blue = Broadcasting, Green = Connected, Red = Disconnected after connection
    uint16_t dotColor;

    if (wifiPortalRunning) {
        dotColor = COLOR_BLUE;  // Broadcasting WiFi setup portal
    } else if (WiFi.status() == WL_CONNECTED) {
        dotColor = COLOR_GREEN;  // Connected
    } else if (wifiAttempted) {
        dotColor = COLOR_RED;  // Disconnected after successful connection
    } else {
        dotColor = COLOR_YELLOW;  // No connection attempted yet
    }

    M5.Display.fillCircle(223, 9, 3, dotColor);  // Down 1px, left 3px from (226, 8)
}

// Play beep if sound enabled
void playBeep(int frequency, int duration) {
    if (buzzEnabled) {
        M5.Speaker.tone(frequency, duration);
    }
}

// Enter screensaver mode
void enterScreensaver() {
    uiState = UI_SCREENSAVER;
    M5.Display.setBrightness(5);  // Very dim default
    M5.Display.fillScreen(COLOR_BLACK);

    // Star in center
    starX = 120;
    starY = 67;
    lastStarMove = millis();

    // Draw yellow star rotated 90° left (2x size = 32x32)
    // Rotate by swapping x/y and inverting one axis
    for (int sy = 0; sy < 16; sy++) {
        for (int sx = 0; sx < 16; sx++) {
            uint16_t pixelColor = pgm_read_word(&STAR_EMOJI_DATA[sy * 16 + sx]);
            if (pixelColor != STAR_TRANSPARENT) {
                // Replace black outline with white, fill with yellow
                if (pixelColor == COLOR_BLACK) {
                    pixelColor = COLOR_WHITE;
                } else {
                    pixelColor = COLOR_YELLOW;
                }
                // Rotate 90° left: (x,y) -> (y, 15-x)
                int rotX = sy;
                int rotY = 15 - sx;
                // Draw 2x2 blocks
                M5.Display.fillRect(starX - 16 + (rotX * 2), starY - 16 + (rotY * 2), 2, 2, pixelColor);
            }
        }
    }

    drawWiFiStatusDot();

    Serial.println("[Screensaver] Activated - Twinkling yellow star");
}

// Update twinkling star screensaver (COOL - dramatic brightness flicker!)
void updateScreensaver() {
    if (millis() - lastStarMove > 200) {  // Every 200ms - faster flicker!
        // Dramatic brightness flicker (twinkle effect)
        int brightness = random(3, 25);  // Wide range from very dim to brighter
        M5.Display.setBrightness(brightness);

        lastStarMove = millis();
    }
}
