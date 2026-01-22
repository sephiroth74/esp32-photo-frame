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

#ifdef ENABLE_BT_IMAGE

#include "bt_utils.h"
#include "board_util.h"
#include "bt_protocol.h"
#include "config.h"
#include "display_manager.h"
#include "esp32/spiram.h"
#include "preferences_helper.h"
#include "rgb_status.h"
#include FONT_HEADER

namespace photo_frame {
namespace bt_utils {

photo_frame_error_t checkBatteryStatus(const battery_info_t& battery_info) {
    if (battery_info.is_critical()) {
        log_e("[BT] Battery is critical: %.1f%%", battery_info.percent);
        return error_type::BtLowBatterySkip;
    }

    if (battery_info.is_empty()) {
        log_e("[BT] Battery is empty: %.1f%%", battery_info.percent);
        return error_type::BatteryEmpty;
    }

    if (battery_info.is_low()) {
        log_w("[BT] Battery is low: %.1f%%", battery_info.percent);
        return error_type::BtLowBatterySkip;
    }

    return error_type::None;
}

void handleCriticalBattery(const battery_info_t& battery_info,
                           esp_sleep_wakeup_cause_t wakeup_reason) {
    log_e("[BT] Handling critical battery state: %.1f%%", battery_info.percent);

    RGB_SET_STATE(BATTERY_LOW);

    // Initialize display if not already done
    // This is safe to call even if display is partially initialized
    photo_frame::DisplayManager display;

    if (!display.isBufferInitialized()) {
        if (!display.initBuffer(true)) {
            log_e("[BT] Failed to init buffer for error display");
            // Can't display error, just sleep indefinitely
            board_utils::enter_deep_sleep(ESP_SLEEP_WAKEUP_EXT0, 0);
            return;
        }
    }

    board_utils::display_power_on();
    delay(100);

    if (!display.isDisplayInitialized()) {
        if (!display.initDisplay()) {
            log_e("[BT] Failed to init display for error display");
            // Can't display error, just sleep indefinitely
            board_utils::enter_deep_sleep(ESP_SLEEP_WAKEUP_EXT0, 0);
            return;
        }
    }

    delay(300);

    // Draw battery critical error
    display.clear(DISPLAY_COLOR_WHITE);
    display.drawError(error_type::BatteryLevelCritical, nullptr);

    // Add battery percentage info
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "Battery: %.1f%%", battery_info.percent);

    // Render
    display.render();

    delay(2000);

    // Power off and sleep indefinitely (only wake via GPIO1)
    board_utils::display_power_off();
    display.powerOff();

    log_i("[BT] Battery critical - entering indefinite sleep (wake via GPIO1 only)");
    board_utils::enter_deep_sleep(ESP_SLEEP_WAKEUP_EXT0, 0); // 0 = indefinite sleep
}

void displayBatteryWarning(const battery_info_t& battery_info) {
    log_w("[BT] Displaying battery warning: %.1f%%", battery_info.percent);

    RGB_SET_STATE(BATTERY_LOW);

    // Initialize display infrastructure
    photo_frame::DisplayManager display;

    if (!display.isBufferInitialized()) {
        if (!display.initBuffer(true)) {
            log_e("[BT] Failed to init buffer for warning display");
            return;
        }
    }

    board_utils::display_power_on();
    delay(100);

    if (!display.isDisplayInitialized()) {
        if (!display.initDisplay()) {
            log_e("[BT] Failed to init display for warning display");
            return;
        }
    }

    delay(300);

    // Draw battery low error
    display.clear(DISPLAY_COLOR_WHITE);
    display.drawError(error_type::BatteryLevelCritical, nullptr);

    // Render
    display.render();

    // Keep display on for a few seconds so user sees it
    delay(3000);

    // Power off display
    board_utils::display_power_off();
    display.powerOff();
}

void displayFirstBootTimeout() {
    log_i("[BT] Timeout on first boot - displaying message");

    RGB_SET_STATE(SLEEP_PREP);

    // Initialize display
    photo_frame::DisplayManager display;

    if (!display.isBufferInitialized()) {
        if (!display.initBuffer(true)) {
            log_e("[BT] Failed to init buffer for timeout display");
            return;
        }
    }

    board_utils::display_power_on();
    delay(100);

    if (!display.isDisplayInitialized()) {
        if (!display.initDisplay()) {
            log_e("[BT] Failed to init display for timeout display");
            return;
        }
    }

    delay(300);

    // Draw timeout message
    display.clear(DISPLAY_COLOR_WHITE);

    // Get canvas for drawing
    GFXcanvas8& canvas = display.getCanvas();

    // Set text color and size
    canvas.setTextColor(DISPLAY_COLOR_BLACK);
    canvas.setFont(&FONT_14pt8b);

    // Calculate center positions
    uint16_t display_width  = display.getWidth();
    uint16_t display_height = display.getHeight();

    // Title
    const char* title = TXT_BT_WAITING_IMAGE_TIMEOUT;
    int16_t x1, y1;
    uint16_t w1, h1;
    canvas.getTextBounds(title, 0, 0, &x1, &y1, &w1, &h1);
    int16_t title_x = (display_width - w1) / 2;
    int16_t title_y = (display_height / 2) - 40;
    canvas.setCursor(title_x, title_y);
    canvas.print(title);

    // Message line 1
    canvas.setFont(&FONT_12pt8b);
    const char* msg1 = TXT_BT_TIMEOUT_EXPIRED;
    canvas.getTextBounds(msg1, 0, 0, &x1, &y1, &w1, &h1);
    int16_t msg1_x = (display_width - w1) / 2;
    int16_t msg1_y = title_y + 40;
    canvas.setCursor(msg1_x, msg1_y);
    canvas.print(msg1);

    // Message line 2
    const char* msg2 = TXT_BT_PRESS_BUTTON_RETRY;
    canvas.getTextBounds(msg2, 0, 0, &x1, &y1, &w1, &h1);
    int16_t msg2_x = (display_width - w1) / 2;
    int16_t msg2_y = msg1_y + 20;
    canvas.setCursor(msg2_x, msg2_y);
    canvas.print(msg2);

    // Render
    display.render();

    delay(2000);

    // Power off display
    board_utils::display_power_off();
    display.powerOff();
}

void performFactoryReset() {
    log_i("[BT] ========================================");
    log_i("[BT] FACTORY RESET INITIATED");
    log_i("[BT] ========================================");

    // Step 1: Clear all BT preferences
    log_i("[BT] Step 1: Clearing BT preferences...");
    auto& prefs  = PreferencesHelper::getInstance();

    bool success = true;
    success &= prefs.setBtRotation(0);
    success &= prefs.setBtFirstBoot(true);
    success &= prefs.setBtImageAvailable(false);
    success &= prefs.setBtLastError(0);
    success &= prefs.setBtRetryCount(0);

    if (success) {
        log_i("[BT] ✓ All BT preferences cleared");
    } else {
        log_w("[BT] ⚠ Some preferences failed to clear");
    }

    // Step 2: Show confirmation message on display
    log_i("[BT] Step 2: Showing confirmation message...");

    // Initialize display
    DisplayManager display;

    if (!display.initBuffer(true)) {
        log_e("[BT] Failed to init display buffer for reset confirmation");
    } else {
        board_utils::display_power_on();
        delay(100);

        if (!display.initDisplay()) {
            log_e("[BT] Failed to init display hardware for reset confirmation");
        } else {
            delay(300);

            // Draw confirmation message
            display.clear(DISPLAY_COLOR_WHITE);

            GFXcanvas8& canvas = display.getCanvas();
            canvas.setTextColor(DISPLAY_COLOR_BLACK);
            canvas.setFont(&FONT_26pt8b);

            uint16_t display_width  = display.getWidth();
            uint16_t display_height = display.getHeight();

            // Title
            const char* title = TXT_BT_FACTORY_RESET;
            int16_t x1, y1;
            uint16_t w1, h1;
            canvas.getTextBounds(title, 0, 0, &x1, &y1, &w1, &h1);
            int16_t title_x = (display_width - w1) / 2;
            int16_t title_y = (display_height / 2) - 60;
            canvas.setCursor(title_x, title_y);
            canvas.print(title);

            // Message line 1
            canvas.setFont(&FONT_18pt8b);
            const char* msg1 = TXT_BT_RESET_COMPLETE;
            canvas.getTextBounds(msg1, 0, 0, &x1, &y1, &w1, &h1);
            int16_t msg1_x = (display_width - w1) / 2;
            int16_t msg1_y = title_y + 60;
            canvas.setCursor(msg1_x, msg1_y);
            canvas.print(msg1);

            // Message line 2
            canvas.setFont(&FONT_12pt8b);
            const char* msg2 = TXT_BT_SETTINGS_RESTORED;
            canvas.getTextBounds(msg2, 0, 0, &x1, &y1, &w1, &h1);
            int16_t msg2_x = (display_width - w1) / 2;
            int16_t msg2_y = msg1_y + 40;
            canvas.setCursor(msg2_x, msg2_y);
            canvas.print(msg2);

            // Message line 3
            const char* msg3 = TXT_BT_SETTINGS_RESTORED_STATE;
            canvas.getTextBounds(msg3, 0, 0, &x1, &y1, &w1, &h1);
            int16_t msg3_x = (display_width - w1) / 2;
            int16_t msg3_y = msg2_y + 20;
            canvas.setCursor(msg3_x, msg3_y);
            canvas.print(msg3);

            // Message line 4
            canvas.setFont(&FONT_12pt8b);
            const char* msg4 = TXT_BT_RESTARTING;
            canvas.getTextBounds(msg4, 0, 0, &x1, &y1, &w1, &h1);
            int16_t msg4_x = (display_width - w1) / 2;
            int16_t msg4_y = msg3_y + 40;
            canvas.setCursor(msg4_x, msg4_y);
            canvas.print(msg4);

            // Render
            display.render();

            log_i("[BT] ✓ Confirmation displayed");

            // Keep message visible for 3 seconds
            delay(3000);

            board_utils::display_power_off();
            display.powerOff();
        }
    }

    // Step 3: Restart device
    log_i("[BT] Step 3: Restarting device...");
    log_i("[BT] ========================================");

    delay(500);
    ESP.restart();

    // Never reached
}

bool checkFactoryResetButton(gpio_num_t button_pin, uint32_t press_duration_ms) {
    log_i("[BT] Checking for factory reset button press on GPIO%d...", button_pin);

    // Configure button pin with pull-up
    pinMode(button_pin, INPUT_PULLUP);

    // Wait a moment for pin to stabilize
    delay(50);

    // Check if button is pressed (LOW due to pull-up)
    if (digitalRead(button_pin) == HIGH) {
        log_d("[BT] Button not pressed, skipping factory reset check");
        return false;
    }

    log_i("[BT] Button pressed, monitoring for %u ms long press...", press_duration_ms);

    // Monitor button for the required duration
    uint32_t start_time = millis();
    uint32_t last_log   = start_time;

    while (millis() - start_time < press_duration_ms) {
        // Check if button was released
        if (digitalRead(button_pin) == HIGH) {
            uint32_t held_duration = millis() - start_time;
            log_i("[BT] Button released after %u ms (required: %u ms)",
                  held_duration,
                  press_duration_ms);
            return false;
        }

        // Log progress every second
        if (millis() - last_log >= 1000) {
            uint32_t remaining = press_duration_ms - (millis() - start_time);
            log_i("[BT] Button held... %u ms remaining", remaining);
            last_log = millis();
        }

        delay(100);
    }

    // Button was held for the full duration
    log_i("[BT] Factory reset button held for %u ms - TRIGGERING RESET", press_duration_ms);

    return true;
}

bool displayWaitingMessage(DisplayManager& display, bool is_first_boot) {
    log_i("[BT] Initializing display for BT mode (first_boot: %s)",
          is_first_boot ? "true" : "false");

    // Phase 1: Initialize buffer
    if (!display.isBufferInitialized()) {
        if (!display.initBuffer(true)) { // prefer PSRAM
            log_e("[BT] Failed to initialize display buffer");
            return false;
        }
    }

    // For subsequent boots, don't touch the display - leave it showing the last image
    if (!is_first_boot) {
        log_i("[BT] Subsequent boot - skipping display update (keeping last image)");
        return true;
    }

    // First boot only: Show waiting message
    log_i("[BT] First boot - showing waiting message");

    // Phase 2: Initialize hardware
    board_utils::display_power_on();
    delay(100);

    if (!display.initDisplay()) {
        log_e("[BT] Failed to initialize display hardware");
        return false;
    }

    delay(300);

    // Draw waiting message
    display.clear(DISPLAY_COLOR_WHITE);

    GFXcanvas8& canvas = display.getCanvas();
    canvas.setFont(&FONT_24pt8b);
    canvas.setTextColor(DISPLAY_COLOR_BLACK);

    uint16_t display_width  = display.getWidth();
    uint16_t display_height = display.getHeight();

    // Title
    const char* title = "Primo Avvio";
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
    snprintf(device_name_line, sizeof(device_name_line), "Cerca: %s", bt_protocol::BT_DEVICE_NAME);
    canvas.getTextBounds(device_name_line, 0, 0, &x1, &y1, &w1, &h1);
    int16_t device_x = (display_width - w1) / 2;
    int16_t device_y = title_y + 30;
    canvas.setCursor(device_x, device_y);
    canvas.print(device_name_line);

    // Message line 1
    const char* msg = "In attesa di una nuova immagine...";
    canvas.getTextBounds(msg, 0, 0, &x1, &y1, &w1, &h1);
    int16_t msg_x = (display_width - w1) / 2;
    int16_t msg_y = device_y + 25;
    canvas.setCursor(msg_x, msg_y);
    canvas.print(msg);

    // Timeout info
    const char* timeout_msg = "Timeout: 30 minuti";
    canvas.getTextBounds(timeout_msg, 0, 0, &x1, &y1, &w1, &h1);
    int16_t timeout_x = (display_width - w1) / 2;
    int16_t timeout_y = msg_y + 20;
    canvas.setCursor(timeout_x, timeout_y);
    canvas.print(timeout_msg);

    // Render to display
    display.render();

    // Power off display to save battery during wait
    display.powerOff();
    board_utils::display_power_off();

    log_i("[BT] Display initialized and waiting message shown");

    return true;
}

} // namespace bt_utils
} // namespace photo_frame

#endif // ENABLE_BT_IMAGE
