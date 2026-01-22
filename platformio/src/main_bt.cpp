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

#include "battery.h"
#include "bluetooth_image_manager.h"
#include "board_util.h"
#include "bt_protocol.h"
#include "bt_utils.h"
#include "config.h"
#include "errors.h"
#include "rgb_status.h"

#include "main_bt.h"
#include "main_common.h"

#include FONT_HEADER

// ============================================================================
// BLUETOOTH MODE IMPLEMENTATION
// ============================================================================

/**
 * @brief Main Bluetooth mode entry point
 *
 * Handles both first boot and subsequent wakeups in Bluetooth mode.
 * This function never returns - it either loops or enters deep sleep.
 */
void setup_bluetooth_mode() {
    log_i("[BT] Entering Bluetooth mode setup");

    photo_frame::battery_info_t battery_info;

    // Initialize hardware
    if (!initialize_hardware()) {
        log_e("[BT] Failed to initialize hardware!");
        return;
    }

    photo_frame::board_utils::display_power_off();
    delay(500);

    // Get wakeup reason
    esp_sleep_wakeup_cause_t wakeup_reason = photo_frame::board_utils::get_wakeup_reason();
    bool is_first_boot                     = wakeup_reason == ESP_SLEEP_WAKEUP_UNDEFINED;

    char wakeup_reason_string[32];
    photo_frame::board_utils::get_wakeup_reason_string(
        wakeup_reason, wakeup_reason_string, sizeof(wakeup_reason_string));

    log_i("[BT] Wakeup reason: %s (%d)", wakeup_reason_string, wakeup_reason);
    log_i("[BT] Is first boot: %s", is_first_boot ? "Yes" : "No");

    // Check for factory reset button press (5 second long press on WAKEUP_PIN)
    // This must be checked early, before any other operations
    log_i("[BT] Checking for factory reset button...");
    if (photo_frame::bt_utils::checkFactoryResetButton(WAKEUP_PIN, 5000)) {
        log_i("[BT] Factory reset triggered!");
        photo_frame::bt_utils::performFactoryReset();
        // Never returns - device will restart
    }
    log_i("[BT] No factory reset requested");

    // Check battery status
    battery_reader.init();
    battery_info = battery_reader.read();

    log_i("[BT] Battery: %.1f%%, %.1f mV", battery_info.percent, battery_info.millivolts);

    // Check if battery is critical or empty
    if (battery_info.is_empty()) {
        log_e("[BT] Battery is empty, entering indefinite sleep");
        photo_frame::board_utils::enter_deep_sleep(ESP_SLEEP_WAKEUP_EXT0, 0);
        return;
    }

    if (battery_info.is_critical()) {
        log_e("[BT] Battery is critical, showing error and sleeping");
        photo_frame::bt_utils::handleCriticalBattery(battery_info, wakeup_reason);
        return;
    }

    // Low battery warning (but continue operation)
    if (battery_info.is_low()) {
        log_w("[BT] Battery is low - continuing but will warn user");
        photo_frame::bt_utils::displayBatteryWarning(battery_info);
    }

    // Determine timeout based on boot type
    uint32_t timeout_ms = is_first_boot ? BT_FIRST_BOOT_TIMEOUT_MS : BT_LISTEN_TIMEOUT_MS;

    log_i("[BT] Starting image wait: %s", is_first_boot ? "30 min" : "5 min");

    // ========================================================================
    // Initialize display ONCE at the beginning (before BT operations)
    // ========================================================================
    log_i("[BT] Initializing display system...");

    // Phase 1: Initialize buffer
    if (!g_display.initBuffer(true)) {
        log_e("[BT] Failed to initialize display buffer");
        return;
    }

    // Phase 2: Initialize hardware
    photo_frame::board_utils::display_power_on();
    delay(100);

    if (!g_display.initDisplay()) {
        log_e("[BT] Failed to initialize display hardware");
        return;
    }

    delay(300);
    log_i("[BT] Display system ready");

    // Show waiting message (only for first boot)
    if (is_first_boot) {
        log_i("[BT] First boot - showing waiting message");

        g_display.clear(DISPLAY_COLOR_WHITE);
        GFXcanvas8& canvas = g_display.getCanvas();
        canvas.setTextColor(DISPLAY_COLOR_BLACK);
        canvas.setFont(&FONT_26pt8b);

        uint16_t display_width  = g_display.getWidth();
        uint16_t display_height = g_display.getHeight();

        // Title
        const char* title = TXT_BT_FIRST_BOOT;
        int16_t x1, y1;
        uint16_t w1, h1;
        canvas.getTextBounds(title, 0, 0, &x1, &y1, &w1, &h1);
        int16_t title_x = (display_width - w1) / 2;
        int16_t title_y = (display_height / 2) - 60;
        canvas.setCursor(title_x, title_y);
        canvas.print(title);

        // Device name line
        canvas.setFont(&FONT_14pt8b);
        char device_name_line[64];
        snprintf(device_name_line,
                 sizeof(device_name_line),
                 "%s%s",
                 TXT_BT_SEARCHING_DEVICE,
                 photo_frame::bt_protocol::BT_DEVICE_NAME);
        canvas.getTextBounds(device_name_line, 0, 0, &x1, &y1, &w1, &h1);
        int16_t device_x = (display_width - w1) / 2;
        int16_t device_y = title_y + 30;
        canvas.setCursor(device_x, device_y);
        canvas.print(device_name_line);

        // Message line 1
        const char* msg = TXT_BT_WAITING_IMAGE;
        canvas.getTextBounds(msg, 0, 0, &x1, &y1, &w1, &h1);
        int16_t msg_x = (display_width - w1) / 2;
        int16_t msg_y = device_y + 25;
        canvas.setCursor(msg_x, msg_y);
        canvas.print(msg);

        // Timeout info
        const char* timeout_msg = TXT_BT_TIMEOUT_30MIN;
        canvas.getTextBounds(timeout_msg, 0, 0, &x1, &y1, &w1, &h1);
        int16_t timeout_x = (display_width - w1) / 2;
        int16_t timeout_y = msg_y + 20;
        canvas.setCursor(timeout_x, timeout_y);
        canvas.print(timeout_msg);

        // Render waiting message
        g_display.render();
    } else {
        log_i("[BT] Subsequent boot - skipping display update (keeping last image)");
    }

    RGB_SET_STATE(BT_WAITING); // Show BT waiting status with LED

    // Initialize and start BLE manager
    photo_frame::BluetoothImageManager bt_manager;
    photo_frame::photo_frame_error_t bt_error = bt_manager.init(timeout_ms);

    if (bt_error != photo_frame::error_type::None) {
        log_e("[BT] Failed to initialize BLE: %s", bt_error.message);

        // Show BLE init error
        photo_frame::board_utils::display_power_on();
        delay(100);
        g_display.clear(DISPLAY_COLOR_WHITE);
        g_display.drawError(bt_error, nullptr);
        g_display.render();

        delay(2000);
        photo_frame::board_utils::display_power_off();

        // Sleep and retry
        photo_frame::board_utils::enter_deep_sleep(ESP_SLEEP_WAKEUP_EXT0, 0);
        return;
    }

    log_i("[BT] BLE manager initialized, waiting for image (timeout: %u ms)", timeout_ms);

    photo_frame::photo_frame_error_t wait_error = bt_manager.waitForImage(timeout_ms);

    // Shutdown BLE
    bt_manager.shutdown();

    if (wait_error != photo_frame::error_type::None) {
        log_w("[BT] Image wait failed: %s", wait_error.message);
        log_v("[BT] is first boot: %s", is_first_boot ? "Yes" : "No");

        // Only show timeout message on first boot
        if (is_first_boot) {
            photo_frame::bt_utils::displayFirstBootTimeout();
        }

        // Enter indefinite deep sleep
        log_i("[BT] Entering indefinite deep sleep (wake via GPIO1 only)");
        photo_frame::board_utils::enter_deep_sleep(ESP_SLEEP_WAKEUP_EXT0, 0);
        return;
    }

    // ========================================================================
    // Image received successfully - Render directly from memory
    // ========================================================================
    log_i("[BT] Image received successfully, rendering to display");

    // Get image buffer from BT manager
    const uint8_t* image_buffer = bt_manager.getImageBuffer();
    uint32_t image_size         = bt_manager.getImageSize();

    if (!image_buffer || image_size == 0) {
        log_e("[BT] No image data available");
        photo_frame::board_utils::enter_deep_sleep(ESP_SLEEP_WAKEUP_EXT0, 0);
        return;
    }

    // Copy image data to display buffer
    log_i("[BT] Copying %u bytes to display buffer", image_size);
    memcpy(g_display.getBuffer(), image_buffer, image_size);

    // Apply rotation from BLE config
    uint8_t rotation = bt_manager.getConfig().rotation;
    log_i("[BT] Applying rotation: %u", rotation);
    g_display.setRotation(rotation);

    // Draw overlay with date/time, BT icon, and battery status
    log_i("[BT] Drawing overlay with date/time, BT icon, and battery");

    // Get date/time from BT config (Unix timestamp sent by client)
    uint32_t image_timestamp = bt_manager.getConfig().timestamp;
    DateTime image_time      = DateTime((uint32_t)image_timestamp);

    // Draw overlay elements
    g_display.drawOverlay();

    // Draw date and time on the left (without next wake-up time, using image timestamp)
    if (image_time.isValid()) {
        g_display.drawLastUpdate(image_time, 0); // Pass 0 for refresh seconds to skip wake-up time
    }

    // Draw BT icon in center (as image source indicator)
    g_display.drawImageInfo("Bluetooth", photo_frame::IMAGE_SOURCE_BLUETOOTH);

    // Draw battery status on the right
    g_display.drawBatteryStatus(battery_info);

    // Render to display
    g_display.render();

    log_i("[BT] ✓ Image displayed successfully");
    delay(1000);

    // Enter indefinite deep sleep
    log_i("[BT] Entering indefinite deep sleep (wake via GPIO1 only)");
    photo_frame::board_utils::enter_deep_sleep(ESP_SLEEP_WAKEUP_EXT0, 0);
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