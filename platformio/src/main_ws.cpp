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
#include "board_info.h"
#include "board_util.h"
#include "config.h"
#include "display_manager.h"
#include "littlefs_manager.h"
#include "main_common.h"
#include "preferences_helper.h"
#include "rgb_status.h"
#include "ws_ap_manager.h"
#include "ws_display_utils.h"
#include "ws_server.h"
#include "ws_utils.h"
#include <Arduino.h>

using namespace photo_frame::ws;

/// Flag to track if load_image is currently executing
volatile bool g_isLoadingImage = false;

void performFactoryReset() {
    log_i("[WS] ========================================");
    log_i("[WS] FACTORY RESET INITIATED");
    log_i("[WS] ========================================");

    // Step 1: Clear all BT preferences
    log_i("[WS] Step 1: Clearing WS preferences...");
    auto& prefs = photo_frame::PreferencesHelper::getInstance();
    // prefs.clearAll();

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

void displayReceivedFile(const char* filename,
                         uint8_t orientation,
                         uint32_t timestamp,
                         photo_frame::BatteryInfo& BatteryInfo) {
    g_isLoadingImage = true;
    log_i("[WS] Loading image %s (orientation=%u, timestamp=%u)", filename, orientation, timestamp);

    auto& littleFs = photo_frame::littlefs_manager::LittleFsManager::getInstance();
    auto& display  = photo_frame::DisplayManager::getInstance();

    photo_frame::binary_utils::PFR1BinaryFile wrapper(display.getWidth(), display.getHeight());

    // Get date/time from BT config (Unix timestamp sent by client)
    // Configure timezone to convert UTC timestamp to local time
    setenv("TZ", TIMEZONE, 1);
    tzset();

    time_t timestamp_time = (time_t)timestamp;
    struct tm timeinfo;
    localtime_r(&timestamp_time, &timeinfo);

    DateTime image_time = DateTime(timeinfo.tm_year + 1900,
                                   timeinfo.tm_mon + 1,
                                   timeinfo.tm_mday,
                                   timeinfo.tm_hour,
                                   timeinfo.tm_min,
                                   timeinfo.tm_sec);

    auto error          = photo_frame::ws_utils::loadLittleFsFile(filename, littleFs, wrapper);
    display.clear(DISPLAY_COLOR_WHITE);

    if (error != photo_frame::error_type::None) {
        error.log_detailed();
        display.drawError(error, filename);
        display.render();
        g_isLoadingImage = false;
        return;
    }

    memcpy(display.getBuffer(), wrapper.getPayload(), wrapper.header.payload_len);

    // Draw overlay with current date/time and battery
    display.drawOverlay();

    // Draw date and time on the left (without next wake-up time, using image timestamp)
    if (image_time.isValid()) {
        display.drawLastUpdate(image_time, 0); // Pass 0 for refresh seconds to skip wake-up time
    }

    display.drawImageInfo("Upload", photo_frame::IMAGE_SOURCE_WEBSOCKET);
    display.drawBatteryStatus(BatteryInfo);
    display.render();

    g_isLoadingImage = false;
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

    display_rotation = prefs.getDisplayRotation();

    // Check battery status
    photo_frame::BatteryInfo BatteryInfo;
    photo_frame::photo_frame_error_t error = setup_battery_and_power(BatteryInfo, wakeup_reason);
    log_d("[WS] Battery: %.1f%%, %.1f mV", BatteryInfo.percent, BatteryInfo.millivolts);

    // Provide current runtime info to BoardInfo for GET_CONFIG
    BoardInfo::setBatteryInfo(BatteryInfo);
    BoardInfo::setDisplayRotation(static_cast<uint16_t>(display_rotation) * 90);

    if (error == photo_frame::error_type::BatteryLevelCritical) {
        log_e("[BT] Battery is critical, showing error and sleeping");
        photo_frame::ws_utils::handleCriticalBattery(BatteryInfo, wakeup_reason, display_rotation);
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
    photo_frame::ws::WSAPManager apManager;
    if (!apManager.begin()) {
        log_e("[WS] Failed to start WiFi AP - will display error");
        // TODO: Display error on screen indicating AP initialization failure
        error = photo_frame::error_type::WifiConnectionFailed;
    } else {
        log_i("[WS] WiFi AP started: SSID=%s, IP=%s",
              apManager.getSSID().c_str(),
              apManager.getIP().c_str());
    }

    // ========================================================================
    // Initialize LittleFS and load image (if AP started successfully)
    // ========================================================================
    error = littleFs.init() ? error : photo_frame::error_type::LittleFSInitFailed;

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
    log_d("[WS] Loading and displaying image...");

    if (error == photo_frame::error_type::None) {
        // Load current or default image
        error = photo_frame::ws_display_utils::loadCurrentOrDefaultImage(littleFs, display);
        if (error != photo_frame::error_type::None) {
            log_w("[WS] Failed to load image, will show blank screen with connection info");
            display.clear(DISPLAY_COLOR_WHITE);
            // Clear error to continue with blank screen
            error = photo_frame::error_type::None;
        }
    } else {
        // just show the error and shut down
        log_w("[WS] Skipping image load due to previous error: %d", error.code);
        display.clear(DISPLAY_COLOR_WHITE);
        display.drawError(error);
        display.render();
        shutdown(littleFs, display, 10000);
        return;
    }

    // Draw connection info box (QR code, SSID, IP)
    std::string wsUrl = "ws://" + apManager.getIP() + ":" + std::to_string(WS_PORT);
    photo_frame::ws_display_utils::drawConnectionInfoBox(
        display, apManager.getSSID(), apManager.getIP(), wsUrl);

    // Render to display
    if (!display.render()) {
        log_e("[WS] Failed to render display");
    } else {
        log_d("[WS] Display rendered successfully");
    }

    log_d("[WS] Setup complete - waiting for WebSocket connections");

    // Verify WiFi AP is still active before starting WebSocket
    log_d("[WS] Verifying WiFi AP status...");
    log_d("[WS] AP SSID: %s", apManager.getSSID().c_str());
    log_d("[WS] AP IP: %s", apManager.getIP().c_str());
    log_d("[WS] AP Running: %s",
          apManager.isClientConnected() ? "Yes (client connected)" : "Yes (no clients yet)");

    // Give WiFi AP time to fully stabilize before starting WebSocket
    log_v("[WS] Waiting 1 second for WiFi AP to stabilize...");
    delay(1000);

    // Start WebSocket server for GET_CONFIG testing
    log_d("[WS] Creating WebSocket server on port %u...", WS_PORT);
    WSServer wsServer(
        WS_PORT, [&littleFs, &display, &wsServer, &BatteryInfo](const WSEvent& event) {
            switch (event.type) {
            case WSEventType::ERROR:
                log_e("[WS] WebSocket error: %s", event.message.c_str());
                break;
            case WSEventType::CLIENT_CONNECTED: log_i("[WS] WebSocket client connected"); break;
            case WSEventType::CLIENT_DISCONNECTED:
                log_i("[WS] WebSocket client disconnected");
                break;
            case WSEventType::IMAGE_RECEIVED:
                log_i("[WS] Image received: %s (timestamp: %u, orientation: %u)",
                      event.filepath.c_str(),
                      event.timestamp,
                      event.orientation);
                // File is already saved to LittleFS at event.filepath
                // TODO: Trigger display update with new image
                displayReceivedFile(
                    event.filepath.c_str(), event.orientation, event.timestamp, BatteryInfo);
                break;
            case WSEventType::SHUTDOWN_REQUEST:
                log_d("[WS] ========================================");
                log_d("[WS] Shutdown request received via WebSocket");

                // Check if an upload or image loading is in progress
                if (wsServer.isUploadActive()) {
                    log_w("[WS] Cannot shutdown: upload session is active");
                    break;
                }

                if (g_isLoadingImage) {
                    log_w("[WS] Cannot shutdown: image is being loaded");
                    break;
                }

                log_d("[WS] Entering deep sleep in 100ms...");
                shutdown(littleFs, display, 100);
                break;
            default: break;
            }
        });

    if (!wsServer.begin()) {
        log_e("[WS] Failed to start WebSocket server");
    } else {
        log_i("[WS] WebSocket server listening on port %u", WS_PORT);
    }

    while (true) {
        delay(100);
        yield();
    }
}

void main_webserver_loop() {
    // Keep the main loop running to prevent watchdog reset
    // The WebSocket server runs in its own FreeRTOS task
    delay(100);
}

#endif // ENABLE_WEBSERVER_DATAPROVIDER