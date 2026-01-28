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

// ============================================================================
// BT MAIN - Bluetooth Image Mode
// ============================================================================
// This is the main entry point for Bluetooth image transfer mode.
// Allows receiving images via BLE from a companion app.

#ifdef ENABLE_BT_IMAGE

#include <Arduino.h>
#include <memory>

#include "battery.h"
#include "binary_utils.h"
#include "bluetooth_image_manager.h"
#include "board_util.h"
#include "bt_protocol.h"
#include "bt_utils.h"
#include "config.h"
#include "errors.h"
#include "io_utils.h"
#include "littlefs_manager.h"
#include "preferences_helper.h"
#include "rgb_status.h"

#include "main_bt.h"
#include "main_common.h"

#include FONT_HEADER

// ============================================================================
// BLUETOOTH MODE IMPLEMENTATION
// ============================================================================

photo_frame::photo_frame_error_t
load_littlefs_file(const char* filename,
                   photo_frame::littlefs_manager::LittleFsManager& littleFs,
                   photo_frame::DisplayManager& display) {
    log_i("[BT] Loading %s image from LittleFS", filename);

    // Create PFR1 wrapper with display dimensions
    photo_frame::binary_utils::PFR1BinaryFile wrapper(display.getWidth(), display.getHeight());

    // Try to open file
    File file = littleFs.open_file(filename, "r");
    if (!file) {
        return photo_frame::error_type::LittleFsFileNotFound;
    }

    // Load and validate file into wrapper
    auto validationError = photo_frame::binary_utils::validatePFR1File(file, wrapper);
    file.close();

    if (validationError != photo_frame::error_type::None) {
        log_e("[BT] %s PFR1 validation failed: %s", filename, validationError.message);
        return validationError;
    }

    // Copy payload into display buffer
    log_i("[BT] Loading %u payload bytes from %s", wrapper.header.payload_len, filename);
    memcpy(display.getBuffer(), wrapper.getPayload(), wrapper.header.payload_len);

    // Apply rotation from header (0-3)
    display.setRotation(wrapper.header.rotation % 4);

    log_i("[BT] Successfully loaded %s (PFR1) width=%u height=%u rotation=%u",
          filename,
          wrapper.header.width,
          wrapper.header.height,
          wrapper.header.rotation);
    return photo_frame::error_type::None;
}

void shutdown(photo_frame::littlefs_manager::LittleFsManager& littleFs,
              photo_frame::DisplayManager& display,
              unsigned long delay_ms = 0) {
    log_i("[BT] Shutting down");

    if (delay_ms > 0) {
        delay(delay_ms);
    }

    littleFs.release();
    display.powerOff();
    display.release();
    photo_frame::board_utils::display_power_off();
    photo_frame::board_utils::enter_deep_sleep(ESP_SLEEP_WAKEUP_EXT0, 0);
}

/**
 * @brief Main Bluetooth mode entry point
 *
 * Handles both first boot and subsequent wakeups in Bluetooth mode.
 * This function never returns - it either loops or enters deep sleep.
 */
void setup_bluetooth_mode() {
    log_i("[BT] Entering Bluetooth mode setup");

    auto& prefs = photo_frame::PreferencesHelper::getInstance();

    // Initialize LittleFS
    auto littleFs = photo_frame::littlefs_manager::LittleFsManager::getInstance();
    auto& display = photo_frame::DisplayManager::getInstance();

    // Get wakeup reason
    esp_sleep_wakeup_cause_t wakeup_reason = photo_frame::board_utils::get_wakeup_reason();
    bool is_first_boot                     = wakeup_reason == ESP_SLEEP_WAKEUP_UNDEFINED;
    char wakeup_reason_string[32];
    photo_frame::board_utils::get_wakeup_reason_string(
        wakeup_reason, wakeup_reason_string, sizeof(wakeup_reason_string));
    log_d("[BT] Wakeup reason: %s (%d)", wakeup_reason_string, wakeup_reason);
    log_d("[BT] Is first boot: %s", is_first_boot ? "Yes" : "No");

    // Initialize hardware
    if (!initialize_hardware()) {
        log_e("[BT] CRITICAL! Failed to initialize hardware!");
        shutdown(littleFs, display, 0);
        return;
    }

    photo_frame::board_utils::display_power_off();

    // Check for factory reset button press (5 second long press on WAKEUP_PIN)
    // This must be checked early, before any other operations
    log_d("[BT] Checking for factory reset button...");
    if (photo_frame::bt_utils::checkFactoryResetButton(wakeup_reason, WAKEUP_PIN, 5000)) {
        log_d("[BT] Factory reset triggered!");
        photo_frame::bt_utils::performFactoryReset();

        if (littleFs.init()) {
            littleFs.delete_file(BT_CURRENT_IMAGE_FILENAME);
        }

        is_first_boot = true; // After reset, treat as first boot
    }
    log_d("[BT] No factory reset requested");

    display_rotation = prefs.getDisplayRotation();
    log_d("[BT] Loaded display rotation from preferences: %u", display_rotation);

    // is_first_boot = false; // TEMPORARY DISABLE FOR TESTING

    // Check battery status
    photo_frame::battery_info_t battery_info;
    photo_frame::photo_frame_error_t error = setup_battery_and_power(battery_info, wakeup_reason);
    log_d("[BT] Battery: %.1f%%, %.1f mV", battery_info.percent, battery_info.millivolts);

    if (error == photo_frame::error_type::BatteryLevelCritical) {
        log_e("[BT] Battery is critical, showing error and sleeping");
        photo_frame::bt_utils::handleCriticalBattery(battery_info, wakeup_reason, display_rotation);
        return;
    }

    // Determine timeout based on boot type
    uint32_t timeout_ms = is_first_boot ? BT_FIRST_BOOT_TIMEOUT_MS : BT_LISTEN_TIMEOUT_MS;

    log_d("[BT] Starting image wait: %s",
          is_first_boot ? "First Boot Timeout" : "Subsequent Wakeup Timeout");
    log_d("[BT] Timeout set to %u ms", timeout_ms);

    // ========================================================================
    // Initialize display ONCE at the beginning (before BT operations)
    // ========================================================================
    log_i("[BT] Initializing display system...");

    // Phase 1: Initialize buffer
    if (!init_image_buffer()) {
        log_e("[BT] Failed to initialize display buffer");
        shutdown(littleFs, display, 10000);
        return;
    }

    // Phase 2: Initialize hardware
    photo_frame::board_utils::display_power_on();

    if (!init_display_hardware()) {
        log_e("[BT] Failed to initialize display hardware");
        shutdown(littleFs, display, 10000);
        return;
    }

    delay(300);
    log_d("[BT] Display system ready");
    display.setRotation(display_rotation);

    // Show waiting message (only for first boot)
    log_d("[BT] First boot - showing waiting message");
    // we need to show a message indicating we're waiting for an image
    // it should be like: "Look for device 'PhotoFrame-XXXX'\n Timeout: 30 minutes"

    const String device_name = photo_frame::bt_utils::getBluetoothDeviceName();
    char timeout_message[32];
    sprintf(timeout_message, TXT_BT_TIMEOUT_MIN, timeout_ms / 60000);

    char full_message[128];
    sprintf(
        full_message, "%s: %s\n%s", TXT_BT_SEARCHING_DEVICE, device_name.c_str(), timeout_message);

    display.drawCenteredMessageWithIcon(
        display.getCanvas(), icon_name::bluetooth_0deg, TXT_BT_WAITING_IMAGE, full_message, 196);

    // Render waiting message
    display.render();

    RGB_SET_STATE(BT_WAITING); // Show BT waiting status with LED

    // Initialize and start BLE manager
    photo_frame::BluetoothImageManager bt_manager;
    bt_manager.setDeviceOrientation(display_rotation);
    photo_frame::photo_frame_error_t bt_error = bt_manager.init(timeout_ms);

    if (bt_error != photo_frame::error_type::None) {
        log_e("[BT] Failed to initialize BLE: %s", bt_error.message);
        display.clear(DISPLAY_COLOR_WHITE);
        display.drawError(bt_error, nullptr);
        display.render();

        // show the default image (or the last valid image if any) before sleeping

        shutdown(littleFs, display, 10000);
        return;
    }

    log_i("[BT] BLE manager initialized, waiting for image (timeout: %u ms)", timeout_ms);

    photo_frame::photo_frame_error_t wait_error = bt_manager.waitForImage(timeout_ms);

    // Shutdown BLE
    bt_manager.shutdown();

    // Timeout expired or error occurred
    if (wait_error != photo_frame::error_type::None) {
        log_w("[BT] Image wait failed: %s", wait_error.message);
        log_v("[BT] is first boot: %s", is_first_boot ? "Yes" : "No");

        // Initialize littlefs if not already done
        if (!littleFs.init()) {
            log_e("[BT] Failed to initialize littlefs");
            photo_frame::bt_utils::displayFirstBootTimeout();
            shutdown(littleFs, display, 0);
            return;
        }

        log_i("[BT] First boot timeout - attempting to load %s from littlefs",
              BT_CURRENT_IMAGE_FILENAME);

        error = load_littlefs_file(BT_CURRENT_IMAGE_FILENAME, littleFs, display);
        if (error != photo_frame::error_type::None) {
            error = load_littlefs_file(BT_DEFAULT_IMAGE_FILENAME, littleFs, display);

            if (error != photo_frame::error_type::None) {
                log_w("[BT] %s not found or invalid in littlefs", BT_DEFAULT_IMAGE_FILENAME);
                photo_frame::bt_utils::displayFirstBootTimeout();
                shutdown(littleFs, display, 0);
                return;
            }
        }
        // Draw overlay with current date/time and battery
        display.drawOverlay();
        display.drawImageInfo("Bluetooth", photo_frame::IMAGE_SOURCE_BLUETOOTH);
        display.drawBatteryStatus(battery_info);
        display.render();

        shutdown(littleFs, display, 1000);
        return; // in both cases we enter deep sleep
    }

    // Image received successfully - Render directly from memory
    log_i("[BT] Image received successfully, rendering to display");

    // Get image file wrapper from BT manager
    auto image_file = bt_manager.getImageFile();
    if (!image_file || !image_file->getBuffer() || image_file->getBufferSize() == 0 ||
        image_file->isValidated() == false) {
        log_e("[BT] No image data available");
        display.clear(DISPLAY_COLOR_WHITE);
        display.drawError(photo_frame::error_type::BtImageValidationFailed, nullptr);
        display.render();

        shutdown(littleFs, display, 2000);
        return;
    }

    // Copy payload (without header) to display buffer
    log_i("[BT] Copying %u bytes to display buffer", image_file->header.payload_len);
    memcpy(display.getBuffer(), image_file->getPayload(), image_file->header.payload_len);

    // Apply rotation from BLE config and persist it
    uint8_t rotation = bt_manager.getConfig().rotation;
    rotation         = rotation % 4; // ensure 0-3
    log_i("[BT] Applying rotation: %u", rotation);
    display.setRotation(rotation);
    display_rotation = rotation; // update global state for other screens

    // Save current orientation to preferences for future use (errors, other screens)
    if (prefs.setDisplayRotation(rotation)) {
        log_i("[BT] Saved display rotation: %u", rotation);
    } else {
        log_w("[BT] Failed to save display rotation to preferences");
    }

    // Save complete PFR1 file (header + payload + CRC) to littlefs
    log_i("[BT] Saving complete PFR1 file to littlefs as %s", BT_CURRENT_IMAGE_FILENAME);
    if (!littleFs.init()) {
        log_e("[BT] Failed to initialize littlefs for saving");
    } else if (littleFs.write_file(BT_CURRENT_IMAGE_FILENAME,
                                   image_file->getBuffer(),
                                   image_file->getBufferSize())) {
        log_i("[BT] Successfully saved %s (%u bytes)",
              BT_CURRENT_IMAGE_FILENAME,
              image_file->getBufferSize());
    } else {
        log_e("[BT] Failed to save %s to littlefs", BT_CURRENT_IMAGE_FILENAME);
    }

    // Draw overlay with date/time, BT icon, and battery status
    log_i("[BT] Drawing overlay with date/time, BT icon, and battery");

    // Get date/time from BT config (Unix timestamp sent by client)
    // Configure timezone to convert UTC timestamp to local time
    setenv("TZ", TIMEZONE, 1);
    tzset();

    uint32_t image_timestamp = bt_manager.getConfig().timestamp;
    time_t timestamp_time    = (time_t)image_timestamp;
    struct tm timeinfo;
    localtime_r(&timestamp_time, &timeinfo);

    DateTime image_time = DateTime(timeinfo.tm_year + 1900,
                                   timeinfo.tm_mon + 1,
                                   timeinfo.tm_mday,
                                   timeinfo.tm_hour,
                                   timeinfo.tm_min,
                                   timeinfo.tm_sec);

    // Draw overlay elements
    display.drawOverlay();

    // Draw date and time on the left (without next wake-up time, using image timestamp)
    if (image_time.isValid()) {
        display.drawLastUpdate(image_time, 0); // Pass 0 for refresh seconds to skip wake-up time
    }

    // Draw BT icon in center (as image source indicator)
    display.drawImageInfo("Bluetooth", photo_frame::IMAGE_SOURCE_BLUETOOTH);

    // Draw battery status on the right
    display.drawBatteryStatus(battery_info);

    // Render to display
    display.render();

    log_i("[BT] ✓ Image displayed successfully");
    shutdown(littleFs, display, 1000);
}

// ============================================================================
// SETUP & LOOP
// ============================================================================

void bt_main_setup() {
    Serial.begin(115200);
    delay(5000);

    // Initialize display power control (if configured)
    photo_frame::board_utils::init_display_power();

    log_i("\n==================================");
    log_i("*** BLUETOOTH IMAGE MODE ***");
    log_i("==================================");

    // Bluetooth image transfer mode
    setup_bluetooth_mode();
    // setup_bluetooth_mode never returns (enters sleep/loops)
}

void bt_main_loop() {
    delay(1000); // Just to avoid watchdog reset
}

#endif // ENABLE_BT_IMAGE