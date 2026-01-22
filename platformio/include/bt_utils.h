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

#pragma once

#include "battery.h"
#include "display_manager.h"
#include "errors.h"
#include <Arduino.h>

namespace photo_frame {
namespace bt_utils {

/**
 * @brief Battery management utilities for Bluetooth mode
 */

/**
 * @brief Check battery status and determine action
 *
 * @param battery_info Current battery information
 * @return Error if battery is critical, None otherwise
 */
photo_frame_error_t checkBatteryStatus(const battery_info_t& battery_info);

/**
 * @brief Display battery critical error and sleep
 *
 * Shows error on display and enters deep sleep.
 *
 * @param battery_info Battery information to display
 * @param wakeup_reason Current wakeup reason
 */
void handleCriticalBattery(const battery_info_t& battery_info,
                           esp_sleep_wakeup_cause_t wakeup_reason);

/**
 * @brief Display battery low warning
 *
 * Shows warning message on display about low battery.
 *
 * @param battery_info Battery information to display
 */
void displayBatteryWarning(const battery_info_t& battery_info);

/**
 * @brief Display timeout message for first boot
 *
 * Shows "Premere next e riprovare" message on display.
 */
void displayFirstBootTimeout();

/**
 * @brief Display waiting message and initialize display for BT mode
 *
 * Handles display initialization and shows appropriate waiting message:
 * - First boot: Shows "Primo Avvio" message with 30min timeout
 * - Subsequent boot: Leaves display untouched (shows last image)
 *
 * @param display Display manager instance
 * @param is_first_boot Whether this is first boot or subsequent
 * @return true if display initialized successfully, false otherwise
 */
bool displayWaitingMessage(DisplayManager& display, bool is_first_boot);

/**
 * @brief Perform factory reset for Bluetooth mode
 *
 * Clears all BT preferences, deletes saved image file,
 * shows confirmation message, and restarts device.
 *
 * This function never returns - device will restart after completion.
 *
 * Steps:
 * 1. Clear all BT preferences (rotation, first_boot, image_available, error, retry_count)
 * 2. Delete saved image file from SD card (/bt_images/last.bin)
 * 3. Show confirmation message on display
 * 4. Restart device
 */
void performFactoryReset();

/**
 * @brief Check if factory reset button is pressed (long press)
 *
 * Monitors GPIO button for long press (5 seconds).
 * If pressed, performs factory reset.
 *
 * @param button_pin GPIO pin to monitor
 * @param press_duration_ms Duration in milliseconds for long press detection
 * @return true if factory reset was triggered, false otherwise
 */
bool checkFactoryResetButton(gpio_num_t button_pin, uint32_t press_duration_ms = 5000);

} // namespace bt_utils
} // namespace photo_frame

#endif // ENABLE_BT_IMAGE
