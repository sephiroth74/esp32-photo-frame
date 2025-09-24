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

#ifndef __OTA_INTEGRATION_H__
#define __OTA_INTEGRATION_H__

#include "config.h"

#ifdef OTA_UPDATE_ENABLED

#include "errors.h"
#include <Arduino.h>

namespace photo_frame {

/**
 * @brief Initialize OTA update system with compile-time configuration
 *
 * This function sets up the OTA update system using preprocessor constants
 * defined in config.h. It automatically configures server URLs, endpoints,
 * board name, and SSL settings.
 *
 * Example usage:
 * @code
 * void setup() {
 *     // ... other initialization code ...
 *
 *     #ifdef OTA_UPDATE_ENABLED
 *     auto error = initialize_ota_updates();
 *     if (error != error_type::None) {
 *         Serial.println("Failed to initialize OTA system");
 *     }
 *     #endif
 * }
 * @endcode
 *
 * @return photo_frame_error_t Error code (error_type::None on success)
 */
photo_frame_error_t initialize_ota_updates();

/**
 * @brief Check if OTA update should be performed during setup
 *
 * This function determines whether an OTA check should be performed based on:
 * 1. Time elapsed since last check (OTA_CHECK_INTERVAL_HOURS)
 * 2. Wakeup reason (if undefined, user pressed reset button)
 *
 * @param wakeup_reason The wakeup reason already determined by main.cpp
 * @return true if OTA check should be performed
 */
bool should_check_ota_updates(esp_sleep_wakeup_cause_t wakeup_reason);

/**
 * @brief Handle OTA updates during setup only
 *
 * This function performs a complete OTA update cycle during device setup.
 * It should ONLY be called from setup(), never from loop().
 *
 * Conditions for checking updates:
 * 1. OTA_CHECK_INTERVAL_HOURS have elapsed since last check, OR
 * 2. User pressed reset button (wakeup reason is undefined)
 *
 * Example usage:
 * @code
 * void setup() {
 *     // ... other initialization code ...
 *     esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
 *
 *     #ifdef OTA_UPDATE_ENABLED
 *     auto error = initialize_ota_updates();
 *     if (error == error_type::None) {
 *         // Handle OTA updates during setup only
 *         handle_ota_updates_setup(wakeup_reason);
 *         // If update was available and successful, device will restart
 *         // If no update or error, execution continues normally
 *     }
 *     #endif
 * }
 *
 * void loop() {
 *     // NO OTA handling in loop - all done during setup
 *     // ... main application logic ...
 * }
 * @endcode
 *
 * @param wakeup_reason The wakeup reason already determined by main.cpp
 * @return photo_frame_error_t Error code (error_type::None if no update needed or update
 * successful)
 */
photo_frame_error_t handle_ota_updates_setup(esp_sleep_wakeup_cause_t wakeup_reason);

/**
 * @brief Monitor OTA progress and display status updates
 *
 * Call this function in your main loop to monitor OTA progress
 * and display status updates every 5% of download progress.
 *
 * This function automatically handles completion and error states.
 * Safe to call even when no OTA update is active.
 */
void monitor_ota_progress();

/**
 * @brief Validate battery level for OTA updates
 *
 * Checks if the current battery level is sufficient for performing
 * an OTA update. Uses the OTA_MIN_BATTERY_PERCENT threshold.
 *
 * @return true if battery level is sufficient for OTA update
 */
bool validate_ota_battery_level();

/**
 * @brief Cancel any ongoing OTA update
 *
 * This function cancels the current OTA update process and cleans up resources.
 * Use this if you need to abort an update for any reason.
 *
 * Safe to call even when no OTA update is active.
 */
void cancel_ota_update();

/**
 * @brief Get OTA update status information
 *
 * Returns a human-readable string describing the current OTA status
 * and progress. Useful for debugging or displaying status to users.
 *
 * @return String containing current OTA status and progress information
 */
String get_ota_status_info();

} // namespace photo_frame

// Convenience macros for easy integration
#define INITIALIZE_OTA()                        photo_frame::initialize_ota_updates()
#define SHOULD_CHECK_OTA(wakeup_reason)         photo_frame::should_check_ota_updates(wakeup_reason)
#define HANDLE_OTA_UPDATES_SETUP(wakeup_reason) photo_frame::handle_ota_updates_setup(wakeup_reason)
#define MONITOR_OTA_PROGRESS()                  photo_frame::monitor_ota_progress()
#define CANCEL_OTA_UPDATE()                     photo_frame::cancel_ota_update()
#define GET_OTA_STATUS()                        photo_frame::get_ota_status_info()
#define VALIDATE_OTA_BATTERY()                  photo_frame::validate_ota_battery_level()

#else

// No-op macros when OTA is disabled
#define INITIALIZE_OTA()                                                                           \
    do {                                                                                           \
    } while (0)
#define SHOULD_CHECK_OTA(wakeup_reason) false
#define HANDLE_OTA_UPDATES_SETUP(wakeup_reason)                                                    \
    do {                                                                                           \
    } while (0)
#define MONITOR_OTA_PROGRESS()                                                                     \
    do {                                                                                           \
    } while (0)
#define CANCEL_OTA_UPDATE()                                                                        \
    do {                                                                                           \
    } while (0)
#define GET_OTA_STATUS()       String("")
#define VALIDATE_OTA_BATTERY() true

#endif // OTA_UPDATE_ENABLED

#endif // __OTA_INTEGRATION_H__