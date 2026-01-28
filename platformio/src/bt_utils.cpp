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

const String getBluetoothDeviceName() {
    uint32_t chip_id = ESP.getEfuseMac() & 0xFFFFFF;
    return String(photo_frame::bt_protocol::BT_DEVICE_NAME) + "-" + String(chip_id, HEX);
}

void handleCriticalBattery(const battery_info_t& battery_info,
                           esp_sleep_wakeup_cause_t wakeup_reason,
                           uint8_t display_rotation) {
    log_e("[BT] Handling critical battery state: %.1f%%", battery_info.percent);

    RGB_SET_STATE(BATTERY_LOW);

    // Use singleton DisplayManager instance
    auto& display = photo_frame::DisplayManager::getInstance();

    if (!display.initBuffer(true)) {
        log_e("[BT] Failed to init buffer for error display");
        board_utils::enter_deep_sleep(ESP_SLEEP_WAKEUP_EXT0, 0);
        return;
    }

    board_utils::display_power_on();

    if (!display.initDisplay()) {
        log_e("[BT] Failed to init display for error display");
        board_utils::enter_deep_sleep(ESP_SLEEP_WAKEUP_EXT0, 0);
        return;
    }

    delay(300);

    // Draw battery critical error
    display.setRotation(display_rotation);
    display.clear(DISPLAY_COLOR_WHITE);
    display.drawError(error_type::BatteryLevelCritical, nullptr);
    display.render();

    delay(2000);

    // Power off and sleep indefinitely (only wake via GPIO1)
    display.powerOff();
    board_utils::display_power_off();

    log_i("[BT] Battery critical - entering indefinite sleep (wake via GPIO1 only)");
    board_utils::enter_deep_sleep(ESP_SLEEP_WAKEUP_EXT0, 0); // 0 = indefinite sleep
}

void displayBatteryWarning(const battery_info_t& battery_info) {
    log_w("[BT] Displaying battery warning: %.1f%%", battery_info.percent);

    RGB_SET_STATE(BATTERY_LOW);

    // Use singleton DisplayManager instance
    auto& display = photo_frame::DisplayManager::getInstance();

    if (!display.initBuffer(true)) {
        log_e("[BT] Failed to init buffer for warning display");
        return;
    }

    board_utils::display_power_on();

    if (!display.initDisplay()) {
        log_e("[BT] Failed to init display for warning display");
        return;
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
    auto& display = photo_frame::DisplayManager::getInstance();

    if (!display.initBuffer(true)) {
        log_e("[BT] Failed to init buffer for timeout display");
        return;
    }

    board_utils::display_power_on();

    if (!display.initDisplay()) {
        log_e("[BT] Failed to init display for timeout display");
        return;
    }

    // Draw timeout message
    display.clear(DISPLAY_COLOR_WHITE);
    display.drawCenteredMessageWithIcon(display.getCanvas(),
                                        icon_name::bluetooth_0deg,
                                        TXT_BT_TIMEOUT_EXPIRED,    // Message: "Timeout scaduto"
                                        TXT_BT_PRESS_BUTTON_RETRY, // Title: "In attesa di immagine"
                                        196);

    // Render
    display.render();
}

void performFactoryReset() {
    log_i("[BT] ========================================");
    log_i("[BT] FACTORY RESET INITIATED");
    log_i("[BT] ========================================");

    // Step 1: Clear all BT preferences
    log_i("[BT] Step 1: Clearing BT preferences...");
    auto& prefs  = PreferencesHelper::getInstance();

    bool success = true;
    success &= prefs.setBtImageAvailable(false);
    success &= prefs.setBtLastError(0);
    success &= prefs.setBtRetryCount(0);
    success &= prefs.setDisplayRotation(DEFAULT_ORIENTATION); // reset to default orientation

    if (success) {
        log_i("[BT] ✓ All BT preferences cleared");
    } else {
        log_w("[BT] ⚠ Some preferences failed to clear");
    }
}

bool checkFactoryResetButton(esp_sleep_wakeup_cause_t wakeup_reason,
                             gpio_num_t button_pin,
                             uint32_t press_duration_ms) {
    log_i("[BT] Checking for factory reset button press on GPIO%d...", button_pin);
    log_d("[BT] Wakeup reason: %d", wakeup_reason);

    if (wakeup_reason == ESP_SLEEP_WAKEUP_UNDEFINED) {
        log_d("[BT] Wakeup reason is UNDEFINED - factory reset requested");
        return true;
    }

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

} // namespace bt_utils
} // namespace photo_frame

#endif // ENABLE_BT_IMAGE
