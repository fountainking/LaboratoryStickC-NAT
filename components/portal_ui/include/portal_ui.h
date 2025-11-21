#ifndef PORTAL_UI_H
#define PORTAL_UI_H

#include "m5_display.h"
#include "sound_system.h"
#include <stdbool.h>

// UI States
typedef enum {
    UI_MAIN_MENU,        // Main menu: Portal, Settings
    UI_PORTAL_SUBMENU,   // Portal submenu: Join WiFi, Laboratory, Transfer, New (+)
    UI_SETTINGS_SUBMENU, // Settings submenu: Sound, Dim, OTA, etc
    UI_WIFI_SETUP,       // WiFi configuration screen (Join WiFi active)
    UI_PORTAL_RUNNING,   // Laboratory portal running
    UI_TRANSFER_RUNNING, // Transfer mode
    UI_SAVED_NETWORKS,   // Saved networks management
    UI_NAT_TEST          // Legacy NAT test
} ui_state_t;

// Button definitions (M5StickC Plus2)
#define BTN_A_GPIO  37  // Side button (top)
#define BTN_B_GPIO  39  // Side button (bottom)
#define BTN_C_GPIO  35  // Power button (acts as button C)

// Initialize the portal UI system
void portal_ui_init(void);

// Start the UI task
void portal_ui_start(void);

// Update portal visitor count (call from captive portal)
void portal_ui_set_visitors(int count);

// Get current portal running state
bool portal_ui_is_portal_running(void);

// Button handlers (called from interrupt or polling)
void portal_ui_button_a_pressed(void);
void portal_ui_button_b_pressed(void);
void portal_ui_button_c_pressed(void);

#endif // PORTAL_UI_H
