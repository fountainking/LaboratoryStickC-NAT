#include "portal_ui.h"
#include "m5_display.h"
#include "boot_animation.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include <string.h>
#include <stdio.h>

// External functions to control AP
extern void start_ap(const char *ssid, bool is_setup_mode);
extern void stop_ap(void);
extern const char* get_wifi_ssid(void);
extern bool is_wifi_connected(void);
extern void m5_power_off(void);

static const char *TAG = "PortalUI";

// Display instance
static m5_display_t display;

// UI state
static ui_state_t current_state = UI_MAIN_MENU;
static int selected_main_item = 0;
static int selected_portal_item = 0;
static int selected_settings_item = 0;
static int visitor_count = 0;
static bool portal_running = false;
static bool dim_enabled = false;  // Default: bright (dim OFF)

// Main menu items
static const char *main_menu[] = {
    "Portal",
    "Settings"
};
#define MAIN_MENU_COUNT 2

// Portal submenu items
static const char *portal_menu[] = {
    "Join WiFi",
    "Laboratory",
    "Transfer",
    "New (+)"
};
#define PORTAL_MENU_COUNT 4

// Settings submenu items (with toggle state indicators)
static const char *settings_menu[] = {
    "Sound",
    "Dim",
    "Update"
};
#define SETTINGS_MENU_COUNT 3

// Function to get sound setting display text
static void get_sound_text(char *buf, size_t len) {
    snprintf(buf, len, "Sound: %s", sound_system_is_enabled() ? "ON" : "OFF");
}

// Function to get dim setting display text
static void get_dim_text(char *buf, size_t len) {
    snprintf(buf, len, "Dim: %s", dim_enabled ? "ON" : "OFF");
}

// Load dim preference from NVS
static void load_dim_preference(void) {
    nvs_handle_t nvs;
    esp_err_t err = nvs_open("storage", NVS_READONLY, &nvs);
    if (err == ESP_OK) {
        uint8_t enabled = 0;  // Default: bright (dim OFF = 0)
        err = nvs_get_u8(nvs, "dim_enabled", &enabled);
        if (err == ESP_OK) {
            dim_enabled = (enabled != 0);
            m5_display_set_brightness(!dim_enabled);  // Invert: true = bright, false = dim
            ESP_LOGI(TAG, "Loaded dim preference: %s", dim_enabled ? "ON" : "OFF");
        } else {
            // No saved preference, use default (bright)
            m5_display_set_brightness(true);
        }
        nvs_close(nvs);
    } else {
        // NVS not available, use default (bright)
        m5_display_set_brightness(true);
    }
}

// Save dim preference to NVS
static void save_dim_preference(void) {
    nvs_handle_t nvs;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &nvs);
    if (err == ESP_OK) {
        uint8_t enabled = dim_enabled ? 1 : 0;
        err = nvs_set_u8(nvs, "dim_enabled", enabled);
        if (err == ESP_OK) {
            nvs_commit(nvs);
            ESP_LOGI(TAG, "Saved dim preference: %s", dim_enabled ? "ON" : "OFF");
        }
        nvs_close(nvs);
    }
}

// NAT test state
typedef struct {
    bool dns_ok;
    bool https_ok;
    bool ping_ok;
    int dns_ms;
    int https_ms;
    int ping_ms;
    bool running;
} nat_test_state_t;

static nat_test_state_t nat_test = {0};

// Forward declarations
static void draw_main_menu(void);
static void draw_portal_submenu(void);
static void draw_settings_submenu(void);
static void draw_wifi_setup(void);
static void draw_portal_running(void);
static void draw_transfer_running(void);
static void ui_task(void *param);

// Initialize UI
void portal_ui_init(void)
{
    ESP_LOGI(TAG, "Initializing Portal UI...");

    // Initialize display
    esp_err_t ret = m5_display_init(&display);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Display init failed: %s", esp_err_to_name(ret));
        return;
    }

    // Play boot animation
    ESP_LOGI(TAG, "Playing boot animation...");
    boot_animation_play(&display);

    // Clear screen after boot animation
    m5_display_clear(&display, COLOR_BLACK);
    m5_display_flush(&display);

    // Initialize buttons
    gpio_config_t btn_conf = {
        .pin_bit_mask = (1ULL << BTN_A_GPIO) | (1ULL << BTN_B_GPIO) | (1ULL << BTN_C_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&btn_conf);

    // Load dim preference from NVS
    load_dim_preference();

    ESP_LOGI(TAG, "Portal UI initialized");
}

// Start UI task
void portal_ui_start(void)
{
    xTaskCreate(ui_task, "portal_ui", 4096, NULL, 5, NULL);
}

// Update visitor count
void portal_ui_set_visitors(int count)
{
    visitor_count = count;
}

// Get portal running state
bool portal_ui_is_portal_running(void)
{
    return portal_running;
}

// Button handlers
void portal_ui_button_a_pressed(void)
{
    ESP_LOGI(TAG, "Button A pressed");
    sound_system_play(SOUND_SELECT);  // Selection beep

    switch (current_state) {
        case UI_MAIN_MENU:
            // Enter selected submenu
            if (selected_main_item == 0) {
                // Portal submenu
                current_state = UI_PORTAL_SUBMENU;
            } else if (selected_main_item == 1) {
                // Settings submenu
                current_state = UI_SETTINGS_SUBMENU;
            }
            break;

        case UI_PORTAL_SUBMENU:
            // Launch selected portal option
            if (selected_portal_item == 0) {
                // Join WiFi - setup mode = true
                start_ap("labPORTAL Wifi Setup", true);
                current_state = UI_WIFI_SETUP;
                ESP_LOGI(TAG, "WiFi Setup opened");
            } else if (selected_portal_item == 1) {
                // Laboratory portal - setup mode = false
                start_ap("Laboratory", false);
                portal_running = true;
                current_state = UI_PORTAL_RUNNING;
                ESP_LOGI(TAG, "Laboratory portal started");
            } else if (selected_portal_item == 2) {
                // Transfer
                current_state = UI_TRANSFER_RUNNING;
                ESP_LOGI(TAG, "Transfer mode");
            } else if (selected_portal_item == 3) {
                // New (+) - does nothing for now
                ESP_LOGI(TAG, "New portal (not implemented)");
            }
            break;

        case UI_SETTINGS_SUBMENU:
            // Handle settings options
            if (selected_settings_item == 0) {
                // Toggle sound
                bool new_state = !sound_system_is_enabled();
                sound_system_set_enabled(new_state);
                sound_system_save_preference();
                // Play confirmation beep if sound is now ON
                if (new_state) {
                    sound_system_play(SOUND_SELECT);
                }
                ESP_LOGI(TAG, "Sound toggled to %s", new_state ? "ON" : "OFF");
            } else if (selected_settings_item == 1) {
                // Toggle dim
                dim_enabled = !dim_enabled;
                m5_display_set_brightness(!dim_enabled);  // Invert: true = bright, false = dim
                save_dim_preference();
                sound_system_play(SOUND_SELECT);
                ESP_LOGI(TAG, "Dim toggled to %s", dim_enabled ? "ON" : "OFF");
            } else {
                ESP_LOGI(TAG, "Settings option %d selected (not implemented)", selected_settings_item);
            }
            break;

        default:
            break;
    }
}

void portal_ui_button_b_pressed(void)
{
    ESP_LOGI(TAG, "Button B pressed");
    sound_system_play(SOUND_NAV);  // Navigation beep (back)

    switch (current_state) {
        case UI_PORTAL_SUBMENU:
        case UI_SETTINGS_SUBMENU:
            // Back to main menu
            current_state = UI_MAIN_MENU;
            break;

        case UI_WIFI_SETUP:
            // Stop WiFi setup AP and return to portal submenu
            stop_ap();
            current_state = UI_PORTAL_SUBMENU;
            break;

        case UI_PORTAL_RUNNING:
            // Stop Laboratory portal and return to portal submenu
            stop_ap();
            portal_running = false;
            current_state = UI_PORTAL_SUBMENU;
            break;

        case UI_TRANSFER_RUNNING:
            // Return to portal submenu
            current_state = UI_PORTAL_SUBMENU;
            break;

        default:
            break;
    }
}

void portal_ui_button_c_pressed(void)
{
    ESP_LOGI(TAG, "Button C pressed");
    sound_system_play(SOUND_NAV);  // Navigation beep (cycle)

    switch (current_state) {
        case UI_MAIN_MENU:
            selected_main_item = (selected_main_item + 1) % MAIN_MENU_COUNT;
            break;

        case UI_PORTAL_SUBMENU:
            selected_portal_item = (selected_portal_item + 1) % PORTAL_MENU_COUNT;
            break;

        case UI_SETTINGS_SUBMENU:
            selected_settings_item = (selected_settings_item + 1) % SETTINGS_MENU_COUNT;
            break;

        default:
            break;
    }
}

// Draw main menu - Portal, Settings
static void draw_main_menu(void)
{
    m5_display_clear(&display, COLOR_BLACK);

    // Title - 2x scale, 15px margins
    m5_display_draw_string_scaled(&display, 15, 15, "labPORTAL", COLOR_YELLOW, COLOR_BLACK, 2);

    // Get WiFi SSID - show below title ONLY if connected (with 4px margin)
    if (is_wifi_connected()) {
        const char *wifi_ssid = get_wifi_ssid();
        if (wifi_ssid && strlen(wifi_ssid) > 0) {
            m5_display_draw_string(&display, 15, 36, wifi_ssid, COLOR_WHITE, COLOR_BLACK);
        }
    } else {
        // Show hint if not connected
        m5_display_draw_string(&display, 15, 36, "(Connect Wifi in Settings)", COLOR_LIGHT_YELLOW, COLOR_BLACK);
    }

    // Menu items - 2x scale, white text with blue/yellow highlight
    int y = 55;
    for (int i = 0; i < MAIN_MENU_COUNT; i++) {
        if (i == selected_main_item) {
            // Settings (index 1) gets yellow highlight with black text
            // Portal (index 0) gets blue highlight with white text
            if (i == 1) {
                m5_display_fill_rect(&display, 0, y - 3, 240, 24, COLOR_YELLOW);
                m5_display_draw_string_scaled(&display, 15, y, main_menu[i], COLOR_BLACK, COLOR_YELLOW, 2);
            } else {
                m5_display_fill_rect(&display, 0, y - 3, 240, 24, COLOR_BLUE);
                m5_display_draw_string_scaled(&display, 15, y, main_menu[i], COLOR_WHITE, COLOR_BLUE, 2);
            }
        } else {
            // Unselected - white text
            m5_display_draw_string_scaled(&display, 15, y, main_menu[i], COLOR_WHITE, COLOR_BLACK, 2);
        }
        y += 23;
    }

    m5_display_flush(&display);
}

// Draw portal submenu - Join WiFi, Laboratory, Transfer, New (+)
static void draw_portal_submenu(void)
{
    m5_display_clear(&display, COLOR_BLACK);

    // Title - 15px margins
    m5_display_draw_string_scaled(&display, 15, 15, "PORTAL", COLOR_YELLOW, COLOR_BLACK, 2);

    // Menu items - yellow highlight with BLACK text
    int y = 38;
    for (int i = 0; i < PORTAL_MENU_COUNT; i++) {
        if (i == selected_portal_item) {
            m5_display_fill_rect(&display, 0, y - 3, 240, 24, COLOR_YELLOW);
            m5_display_draw_string_scaled(&display, 15, y, portal_menu[i], COLOR_BLACK, COLOR_YELLOW, 2);
        } else {
            m5_display_draw_string_scaled(&display, 15, y, portal_menu[i], COLOR_WHITE, COLOR_BLACK, 2);
        }
        y += 23;
    }

    m5_display_flush(&display);
}

// Draw settings submenu
static void draw_settings_submenu(void)
{
    m5_display_clear(&display, COLOR_BLACK);

    // Title - 15px margins to match other screens
    m5_display_draw_string_scaled(&display, 15, 15, "SETTINGS", COLOR_YELLOW, COLOR_BLACK, 2);

    // Menu items - yellow highlight with BLACK text
    int y = 38;
    char text_buf[32];
    for (int i = 0; i < SETTINGS_MENU_COUNT; i++) {
        // Get display text (with state for toggles)
        const char *display_text = settings_menu[i];
        if (i == 0) {  // Sound setting - show current state
            get_sound_text(text_buf, sizeof(text_buf));
            display_text = text_buf;
        } else if (i == 1) {  // Dim setting - show current state
            get_dim_text(text_buf, sizeof(text_buf));
            display_text = text_buf;
        }

        if (i == selected_settings_item) {
            m5_display_fill_rect(&display, 0, y - 3, 240, 24, COLOR_YELLOW);
            m5_display_draw_string_scaled(&display, 15, y, display_text, COLOR_BLACK, COLOR_YELLOW, 2);
        } else {
            m5_display_draw_string_scaled(&display, 15, y, display_text, COLOR_WHITE, COLOR_BLACK, 2);
        }
        y += 23;
    }

    m5_display_flush(&display);
}

// Draw WiFi Setup screen
static void draw_wifi_setup(void)
{
    m5_display_clear(&display, COLOR_BLACK);

    // Title
    m5_display_draw_string(&display, 10, 10, "WIFI SETUP", COLOR_YELLOW, COLOR_BLACK);

    // Instructions
    m5_display_draw_string(&display, 10, 35, "Connect phone to", COLOR_WHITE, COLOR_BLACK);
    m5_display_draw_string(&display, 10, 50, "labPORTAL Wifi", COLOR_YELLOW, COLOR_BLACK);
    m5_display_draw_string(&display, 10, 65, "Setup", COLOR_YELLOW, COLOR_BLACK);

    m5_display_draw_string(&display, 10, 85, "Go to:", COLOR_WHITE, COLOR_BLACK);
    m5_display_draw_string(&display, 10, 100, "192.168.4.1/wifi", COLOR_YELLOW, COLOR_BLACK);

    m5_display_flush(&display);
}

// Draw portal running screen
static void draw_portal_running(void)
{
    m5_display_clear(&display, COLOR_BLACK);

    // Title
    m5_display_draw_string_scaled(&display, 5, 10, "Laboratory", COLOR_YELLOW, COLOR_BLACK, 2);

    // Status
    m5_display_draw_string(&display, 10, 40, "Portal Active", COLOR_GREEN, COLOR_BLACK);

    // Visitors
    char visitor_str[32];
    snprintf(visitor_str, sizeof(visitor_str), "Visitors: %d", visitor_count);
    m5_display_draw_string(&display, 10, 55, visitor_str, COLOR_WHITE, COLOR_BLACK);

    // AP Info
    m5_display_draw_string(&display, 10, 75, "Connect to:", COLOR_WHITE, COLOR_BLACK);
    m5_display_draw_string(&display, 10, 90, "Laboratory", COLOR_YELLOW, COLOR_BLACK);
    m5_display_draw_string(&display, 10, 105, "192.168.4.1", COLOR_YELLOW, COLOR_BLACK);

    m5_display_flush(&display);
}

// Draw transfer running screen
static void draw_transfer_running(void)
{
    m5_display_clear(&display, COLOR_BLACK);

    // Title
    m5_display_draw_string_scaled(&display, 5, 10, "TRANSFER", COLOR_YELLOW, COLOR_BLACK, 2);

    // Coming soon message
    m5_display_draw_string(&display, 30, 60, "Coming Soon!", COLOR_WHITE, COLOR_BLACK);

    m5_display_flush(&display);
}

// Draw NAT test screen (legacy)
static void draw_nat_test(void)
{
    m5_display_clear(&display, COLOR_BLACK);

    // Title
    m5_display_draw_string(&display, 10, 5, "NAT TEST", COLOR_YELLOW, COLOR_BLACK);

    // DNS Test
    m5_display_draw_string(&display, 10, 30, "DNS:", COLOR_WHITE, COLOR_BLACK);
    if (nat_test.running) {
        m5_display_draw_string(&display, 60, 30, nat_test.dns_ok ? "OK" : "FAIL",
                               nat_test.dns_ok ? COLOR_GREEN : COLOR_RED, COLOR_BLACK);
        if (nat_test.dns_ms > 0) {
            char ms_str[16];
            snprintf(ms_str, sizeof(ms_str), "%dms", nat_test.dns_ms);
            m5_display_draw_string(&display, 110, 30, ms_str, COLOR_LILAC, COLOR_BLACK);
        }
    } else {
        m5_display_draw_string(&display, 60, 30, "---", COLOR_LILAC, COLOR_BLACK);
    }

    // HTTPS Test
    m5_display_draw_string(&display, 10, 50, "HTTPS:", COLOR_WHITE, COLOR_BLACK);
    if (nat_test.running) {
        m5_display_draw_string(&display, 70, 50, nat_test.https_ok ? "OK" : "FAIL",
                               nat_test.https_ok ? COLOR_GREEN : COLOR_RED, COLOR_BLACK);
        if (nat_test.https_ms > 0) {
            char ms_str[16];
            snprintf(ms_str, sizeof(ms_str), "%dms", nat_test.https_ms);
            m5_display_draw_string(&display, 120, 50, ms_str, COLOR_LILAC, COLOR_BLACK);
        }
    } else {
        m5_display_draw_string(&display, 70, 50, "---", COLOR_LILAC, COLOR_BLACK);
    }

    // Ping Test
    m5_display_draw_string(&display, 10, 70, "PING:", COLOR_WHITE, COLOR_BLACK);
    if (nat_test.running) {
        m5_display_draw_string(&display, 70, 70, nat_test.ping_ok ? "OK" : "FAIL",
                               nat_test.ping_ok ? COLOR_GREEN : COLOR_RED, COLOR_BLACK);
        if (nat_test.ping_ms > 0) {
            char ms_str[16];
            snprintf(ms_str, sizeof(ms_str), "%dms", nat_test.ping_ms);
            m5_display_draw_string(&display, 120, 70, ms_str, COLOR_LILAC, COLOR_BLACK);
        }
    } else {
        m5_display_draw_string(&display, 70, 70, "---", COLOR_LILAC, COLOR_BLACK);
    }

    // Status
    if (nat_test.running) {
        m5_display_draw_string(&display, 60, 95, "TESTING...", COLOR_YELLOW, COLOR_BLACK);
    } else {
        m5_display_draw_string(&display, 40, 95, "Press A to Test", COLOR_LILAC, COLOR_BLACK);
    }

    // Help
    m5_display_draw_string(&display, 30, 120, "A:Test B:Back", COLOR_LILAC, COLOR_BLACK);

    m5_display_flush(&display);
}

// Draw settings screen
static void draw_settings(void)
{
    m5_display_clear(&display, COLOR_BLACK);

    m5_display_draw_string(&display, 10, 5, "SETTINGS", COLOR_YELLOW, COLOR_BLACK);

    m5_display_draw_string(&display, 10, 35, "WiFi: Connected", COLOR_GREEN, COLOR_BLACK);
    m5_display_draw_string(&display, 10, 50, "NAT: Enabled", COLOR_GREEN, COLOR_BLACK);
    m5_display_draw_string(&display, 10, 65, "Portal: Ready", COLOR_WHITE, COLOR_BLACK);

    m5_display_draw_string(&display, 60, 120, "B:Back", COLOR_LILAC, COLOR_BLACK);

    m5_display_flush(&display);
}

// Draw info screen
static void draw_info(void)
{
    m5_display_clear(&display, COLOR_BLACK);

    m5_display_draw_string(&display, 10, 5, "labPORTAL v1.0", COLOR_YELLOW, COLOR_BLACK);

    m5_display_draw_string(&display, 10, 30, "M5StickC Plus2", COLOR_WHITE, COLOR_BLACK);
    m5_display_draw_string(&display, 10, 45, "ESP-IDF NAT Router", COLOR_WHITE, COLOR_BLACK);
    m5_display_draw_string(&display, 10, 60, "Captive Portal", COLOR_WHITE, COLOR_BLACK);

    m5_display_draw_string(&display, 10, 85, "Connect to:", COLOR_LILAC, COLOR_BLACK);
    m5_display_draw_string(&display, 10, 100, "Laboratory", COLOR_YELLOW, COLOR_BLACK);

    m5_display_draw_string(&display, 60, 120, "B:Back", COLOR_LILAC, COLOR_BLACK);

    m5_display_flush(&display);
}

// UI task - updates display and polls buttons
static void ui_task(void *param)
{
    ESP_LOGI(TAG, "UI task started");

    // Button state tracking
    static int btn_a_last = 1, btn_b_last = 1, btn_c_last = 1;
    static uint32_t btn_b_hold_start = 0;
    static bool btn_b_hold_triggered = false;

    // Draw initial screen
    draw_main_menu();

    while (1) {
        // Poll buttons (simple debounce)
        int btn_a = gpio_get_level(BTN_A_GPIO);
        int btn_b = gpio_get_level(BTN_B_GPIO);
        int btn_c = gpio_get_level(BTN_C_GPIO);

        // Track button B hold for power off (4 seconds)
        if (btn_b == 0) {
            if (btn_b_last == 1) {
                // Just pressed - start hold timer
                btn_b_hold_start = xTaskGetTickCount();
                btn_b_hold_triggered = false;
            } else if (!btn_b_hold_triggered) {
                // Still holding - check if 2.5 seconds elapsed
                uint32_t hold_time = (xTaskGetTickCount() - btn_b_hold_start) * portTICK_PERIOD_MS;
                if (hold_time >= 2500) {
                    // 2.5 seconds reached - power off!
                    btn_b_hold_triggered = true;
                    ESP_LOGI(TAG, "Button B held for 2.5 seconds - powering off");

                    // Clear screen and show goodbye (centered at 240x135)
                    m5_display_clear(&display, COLOR_BLACK);
                    // "Goodbye!" = 8 chars × 16px (scale 2) = 128px wide, center = (240-128)/2 = 56
                    m5_display_draw_string_scaled(&display, 56, 60, "Goodbye!", COLOR_YELLOW, COLOR_BLACK, 2);
                    m5_display_flush(&display);

                    // Wait a moment for message to be visible
                    vTaskDelay(pdMS_TO_TICKS(1500));

                    // Clear screen before power off
                    m5_display_clear(&display, COLOR_BLACK);
                    m5_display_flush(&display);
                    vTaskDelay(pdMS_TO_TICKS(100));

                    // Power off (GPIO 4 LOW)
                    m5_power_off();

                    // Infinite loop in case power off fails
                    while(1) {
                        vTaskDelay(pdMS_TO_TICKS(1000));
                    }
                }
            }
        }

        // Detect falling edge (button pressed) - only if not in hold mode
        if (btn_a == 0 && btn_a_last == 1) {
            vTaskDelay(pdMS_TO_TICKS(50)); // Debounce
            if (gpio_get_level(BTN_A_GPIO) == 0) {
                portal_ui_button_a_pressed();
            }
        }

        if (btn_b == 0 && btn_b_last == 1 && !btn_b_hold_triggered) {
            // Only handle short press if not in hold mode
            // We'll handle this on release instead to avoid conflicting with hold
        } else if (btn_b == 1 && btn_b_last == 0 && !btn_b_hold_triggered) {
            // Button B released before 2.5 seconds - handle as normal press
            uint32_t hold_time = (xTaskGetTickCount() - btn_b_hold_start) * portTICK_PERIOD_MS;
            if (hold_time < 2500) {
                portal_ui_button_b_pressed();
            }
        }

        if (btn_c == 0 && btn_c_last == 1) {
            vTaskDelay(pdMS_TO_TICKS(50));
            if (gpio_get_level(BTN_C_GPIO) == 0) {
                portal_ui_button_c_pressed();
            }
        }

        btn_a_last = btn_a;
        btn_b_last = btn_b;
        btn_c_last = btn_c;

        // Run NAT tests if active
        if (current_state == UI_NAT_TEST && nat_test.running) {
            // Simulate tests (in real implementation, call actual network functions)
            // For now, just show fake results
            nat_test.dns_ok = true;
            nat_test.dns_ms = 45;
            nat_test.https_ok = true;
            nat_test.https_ms = 230;
            nat_test.ping_ok = true;
            nat_test.ping_ms = 12;
        }

        // Redraw screen based on state
        switch (current_state) {
            case UI_MAIN_MENU:
                draw_main_menu();
                break;
            case UI_PORTAL_SUBMENU:
                draw_portal_submenu();
                break;
            case UI_SETTINGS_SUBMENU:
                draw_settings_submenu();
                break;
            case UI_WIFI_SETUP:
                draw_wifi_setup();
                break;
            case UI_PORTAL_RUNNING:
                draw_portal_running();
                break;
            case UI_TRANSFER_RUNNING:
                draw_transfer_running();
                break;
            case UI_NAT_TEST:
                draw_nat_test();
                break;
        }

        vTaskDelay(pdMS_TO_TICKS(100)); // 10 FPS
    }
}
