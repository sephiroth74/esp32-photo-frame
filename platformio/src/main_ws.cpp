// MIT License
//
// Copyright (c) 2025 Alessandro Crugnola
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifdef ENABLE_WEBSERVER_DATAPROVIDER

#include "main_ws.h"
#include "board_util.h"
#include "config.h"
#include "display_manager.h"
#include "littlefs_manager.h"
#include "main_common.h"
#include "preferences_helper.h"
#include "rgb_status.h"
#include "ws_ap_manager.h"
#include "ws_display_utils.h"
#include "ws_utils.h"
#include <Arduino.h>

void performFactoryReset() {
    log_i("[WS] ========================================");
    log_i("[WS] FACTORY RESET INITIATED");
    log_i("[WS] ========================================");

    // Step 1: Clear all BT preferences
    log_i("[WS] Step 1: Clearing WS preferences...");
    auto& prefs  = photo_frame::PreferencesHelper::getInstance();

    bool success = true;

    // TODO: Add any webserver-specific preferences to clear here

    success &= prefs.setDisplayRotation(DEFAULT_ORIENTATION); // reset to default orientation

    if (success) {
        log_i("[WS] ✓ All WS preferences cleared");
    } else {
        log_w("[WS] ⚠ Some preferences failed to clear");
    }
}

void shutdown(photo_frame::littlefs_manager::LittleFsManager& littleFs,
              photo_frame::DisplayManager& display,
              unsigned long delay_ms = 0) {
    log_i("[WS] Shutting down");

    if (delay_ms > 0) {
        delay(delay_ms);
    }

    // TODO: Add any webserver-specific shutdown steps here

    littleFs.release();
    display.powerOff();
    display.release();
    photo_frame::board_utils::display_power_off();
    photo_frame::board_utils::enter_deep_sleep(ESP_SLEEP_WAKEUP_EXT0, 0);
}

void main_webserver_setup() {
    Serial.begin(115200);
    delay(5000);

    // Initialize display power control (if configured)
    photo_frame::board_utils::init_display_power();
    auto& prefs   = photo_frame::PreferencesHelper::getInstance();
    auto littleFs = photo_frame::littlefs_manager::LittleFsManager::getInstance();
    auto& display = photo_frame::DisplayManager::getInstance();

    // Get wakeup reason
    esp_sleep_wakeup_cause_t wakeup_reason = photo_frame::board_utils::get_wakeup_reason();
    bool is_first_boot                     = wakeup_reason == ESP_SLEEP_WAKEUP_UNDEFINED;
    char wakeup_reason_string[32];
    photo_frame::board_utils::get_wakeup_reason_string(
        wakeup_reason, wakeup_reason_string, sizeof(wakeup_reason_string));
    log_d("[WS] Wakeup reason: %s (%d)", wakeup_reason_string, wakeup_reason);
    log_d("[WS] Is first boot: %s", is_first_boot ? "Yes" : "No");

    // Initialize hardware
    if (!initialize_hardware()) {
        log_e("[WS] CRITICAL! Failed to initialize hardware!");
        shutdown(littleFs, display, 0);
        return;
    }

    photo_frame::board_utils::display_power_off();

    // Check for factory reset button press (5 second long press on WAKEUP_PIN)
    // This must be checked early, before any other operations
    log_d("[WS] Checking for factory reset button...");
    if (wakeup_reason == ESP_SLEEP_WAKEUP_UNDEFINED) {
        log_d("[WS] Factory reset triggered!");
        performFactoryReset();

        if (littleFs.init()) {
            littleFs.delete_file(WS_CURRENT_IMAGE_FILENAME);
        }

        is_first_boot = true; // After reset, treat as first boot
    }
    log_d("[WS] No factory reset requested");

    // Check battery status
    photo_frame::battery_info_t battery_info;
    photo_frame::photo_frame_error_t error = setup_battery_and_power(battery_info, wakeup_reason);
    log_d("[WS] Battery: %.1f%%, %.1f mV", battery_info.percent, battery_info.millivolts);

    if (error == photo_frame::error_type::BatteryLevelCritical) {
        log_e("[BT] Battery is critical, showing error and sleeping");
        photo_frame::ws_utils::handleCriticalBattery(battery_info, wakeup_reason, display_rotation);
        return;
    }

    // Determine timeout based on boot type
    uint32_t timeout_ms = is_first_boot ? WS_FIRST_BOOT_TIMEOUT_MS : WS_LISTEN_TIMEOUT_MS;

    log_d("[WS] Starting image wait: %s",
          is_first_boot ? "First Boot Timeout" : "Subsequent Wakeup Timeout");
    log_d("[WS] Timeout set to %u ms", timeout_ms);

    // ========================================================================
    // Initialize display ONCE at the beginning (before WS operations)
    // ========================================================================
    log_i("[WS] Initializing display system...");

    // Phase 1: Initialize buffer
    if (!init_image_buffer()) {
        log_e("[WS] Failed to initialize display buffer");
        shutdown(littleFs, display, 10000);
        return;
    }

    // ========================================================================
    // Initialize WiFi Access Point
    // ========================================================================
    log_i("[WS] Initializing WiFi Access Point...");

    // Phase 2: Start WiFi AP
    photo_frame::WSAPManager apManager;
    if (!apManager.begin()) {
        log_e("[WS] Failed to start WiFi AP - will display error");
        // TODO: Display error on screen indicating AP initialization failure
        error = photo_frame::error_type::WifiConnectionFailed;
    } else {
        log_i("[WS] WiFi AP started: SSID=%s, IP=%s",
              apManager.getSSID().c_str(),
              apManager.getIP().c_str());
    }

    // Phase 3: Initialize hardware
    photo_frame::board_utils::display_power_on();

    if (!init_display_hardware()) {
        log_e("[WS] Failed to initialize display hardware");
        shutdown(littleFs, display, 10000);
        return;
    }

    delay(300);
    display.setRotation(display_rotation);

    // ========================================================================
    // Load and display image with connection info
    // ========================================================================
    log_i("[WS] Loading and displaying image...");

    // Load current or default image
    error = photo_frame::ws_display_utils::loadCurrentOrDefaultImage(littleFs, display);
    if (error != photo_frame::error_type::None) {
        log_w("[WS] Failed to load image, will show blank screen with connection info");
        // Clear canvas to white
        display.getCanvas().fillScreen(DISPLAY_COLOR_WHITE);
    }

    // Draw connection info box (QR code, SSID, IP)
    std::string wsUrl = apManager.getIP();
    photo_frame::ws_display_utils::drawConnectionInfoBox(
        display, apManager.getSSID(), apManager.getIP(), wsUrl);

    // Render to display
    if (!display.render()) {
        log_e("[WS] Failed to render display");
    } else {
        log_i("[WS] Display rendered successfully");
    }

    log_i("[WS] Setup complete - waiting for WebSocket connections");

    delay(10000); // Show image for 10 seconds before proceeding
    shutdown(littleFs, display, 0);
}

void main_webserver_loop() {}

#endif // ENABLE_WEBSERVER_DATAPROVIDER