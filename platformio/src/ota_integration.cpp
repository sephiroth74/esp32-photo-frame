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

#include "config.h"

#ifdef OTA_UPDATE_ENABLED

#include "_locale.h"
#include "battery.h"
#include "ota_update.h"
#include "preferences_helper.h"
#include "rgb_status.h"

namespace photo_frame {

/**
 * @brief Initialize OTA update system with compile-time configuration
 *
 * This function sets up the OTA update system using preprocessor constants
 * defined in config.h. It automatically configures server URLs, endpoints,
 * board name, and SSL settings.
 *
 * @return photo_frame_error_t Error code (error_type::None on success)
 */
photo_frame_error_t initialize_ota_updates() {
    using namespace photo_frame;

    RGB_SET_STATE(OTA_UPDATING); // Show OTA initialization status

    // Configure OTA settings using preprocessor constants
    ota_config_t ota_config;

    // Server configuration from preprocessor constants
    ota_config.server_url        = OTA_SERVER_URL;
    ota_config.version_endpoint  = OTA_VERSION_ENDPOINT;
    ota_config.firmware_endpoint = OTA_FIRMWARE_ENDPOINT;

    // Board and version info (automatically set from build constants)
    ota_config.board_name      = XSTR(OTA_CURRENT_BOARD_NAME);
    ota_config.current_version = FIRMWARE_VERSION_STRING;

    // Security settings
    ota_config.use_ssl = OTA_USE_SSL;
    // CA certificate can be set here if needed
    // ota_config.ca_cert = "-----BEGIN CERTIFICATE-----\n...";

    // Initialize OTA system
    auto error = ota_updater.begin(ota_config);
    if (error != error_type::None) {
        Serial.print(TXT_OTA_INIT_FAILED);
        Serial.println(error.code);
        return error;
    }

    Serial.println(TEXT_OTA_SYSTEM_INITIALIZED);

    // Print partition info for diagnostics
    ota_updater.print_partition_info();

    // Mark current firmware as valid (prevents rollback)
    if (ota_updater.is_first_boot_after_update()) {
        Serial.println(TEXT_OTA_FIRST_BOOT_AFTER_UPDATE);
        ota_updater.mark_firmware_valid();
    } else {
        Serial.print(TEXT_OTA_RUNNING_FROM_PARTITION);
        Serial.println(ota_updater.get_running_partition_name());
    }

    return error_type::None;
}

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
bool should_check_ota_updates(esp_sleep_wakeup_cause_t wakeup_reason) {
    // If wakeup reason is undefined, user pressed reset button - always check for updates
    if (wakeup_reason == ESP_SLEEP_WAKEUP_UNDEFINED) {
        Serial.println(F("[OTA] Reset button pressed - checking for updates"));
        return true;
    }

    Serial.print(F("[OTA] Wakeup reason: "));
    switch (wakeup_reason) {
    case ESP_SLEEP_WAKEUP_EXT0:     Serial.println(F("EXT0 (GPIO)")); break;
    case ESP_SLEEP_WAKEUP_EXT1:     Serial.println(F("EXT1 (GPIO)")); break;
    case ESP_SLEEP_WAKEUP_TIMER:    Serial.println(F("Timer")); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD: Serial.println(F("Touchpad")); break;
    case ESP_SLEEP_WAKEUP_ULP:      Serial.println(F("ULP program")); break;
    default:                        Serial.println(F("Unknown")); break;
    }

    // Check if enough time has elapsed since last OTA check
    auto& prefs            = PreferencesHelper::getInstance();

    time_t current_time    = time(NULL);
    time_t last_check_time = prefs.getOtaLastCheck();

    if (last_check_time == 0) {
        Serial.println(F("[OTA] No previous check recorded - performing first check"));
        return true;
    }

    time_t time_since_last_check  = current_time - last_check_time;
    time_t check_interval_seconds = OTA_CHECK_INTERVAL_HOURS * 3600;

    Serial.print(F("[OTA] Time since last check: "));
    Serial.print(time_since_last_check / 3600);
    Serial.print(F(" hours (interval: "));
    Serial.print(OTA_CHECK_INTERVAL_HOURS);
    Serial.println(F(" hours)"));

    if (time_since_last_check >= check_interval_seconds) {
        Serial.println(F("[OTA] Check interval elapsed - checking for updates"));
        return true;
    } else {
        Serial.println(F("[OTA] Check interval not elapsed - skipping OTA check"));
        return false;
    }
}

/**
 * @brief Handle OTA updates during setup only
 *
 * This function performs a complete OTA update cycle during device setup.
 * It should only be called from setup(), never from loop().
 *
 * Conditions for checking updates:
 * 1. OTA_CHECK_INTERVAL_HOURS have elapsed since last check, OR
 * 2. User pressed reset button (wakeup reason is undefined)
 *
 * @param wakeup_reason The wakeup reason already determined by main.cpp
 * @return photo_frame_error_t Error code (error_type::None if no update needed or update
 * successful)
 */
photo_frame_error_t handle_ota_updates_setup(esp_sleep_wakeup_cause_t wakeup_reason) {
    using namespace photo_frame;

    // Determine if we should check for updates
    bool should_check = should_check_ota_updates(wakeup_reason);
    bool force_check  = should_check; // Force check if wakeup reason was undefined (reset button)

    Serial.print(F("[OTA] Should check for updates: "));
    Serial.println(should_check ? F("YES") : F("NO"));

    if (OTA_IS_ACTIVE()) {
        Serial.println(TEXT_OTA_UPDATE_IN_PROGRESS);
        return error_type::OtaUpdateInProgress;
    }

    // Battery level check is now handled in main.cpp before OTA initialization
    // No need to check again here since this function is only called when battery is sufficient

    // Check for updates (force_check=true if reset button was pressed)
    RGB_SET_STATE(OTA_UPDATING); // Show OTA check in progress
    auto error = ota_updater.check_for_update(force_check);

    // Save the current time as the last check time regardless of the result
    // This prevents excessive checking even if there were errors
    auto& prefs         = PreferencesHelper::getInstance();
    time_t current_time = time(NULL);
    if (!prefs.setOtaLastCheck(current_time)) {
        Serial.println(F("[OTA] Warning: Failed to save last check time"));
    }

    if (error == error_type::None) {
        Serial.println(TEXT_OTA_UPDATE_AVAILABLE);
        Serial.println(F("[OTA] Starting update process - device will restart if successful"));

        // Keep RGB status showing OTA update in progress
        RGB_SET_STATE(OTA_UPDATING); // Maintain OTA status during download and installation

        // Start update process
        error = ota_updater.start_update();

        if (error != error_type::None) {
            Serial.print(TEXT_OTA_UPDATE_FAILED);
            Serial.println(error.code);
            return error;
        }
        // If successful, device will restart automatically - this line won't be reached

    } else if (error == error_type::NoUpdateNeeded) {
        Serial.println(TEXT_OTA_FIRMWARE_UP_TO_DATE);
        return error_type::None; // Not an error condition
    } else if (error == error_type::OtaVersionIncompatible) {
        Serial.println(F("[OTA] Current firmware is incompatible with available update"));
        Serial.println(F("[OTA] Manual firmware update required"));
        return error;
    } else {
        Serial.print(TEXT_OTA_UPDATE_CHECK_FAILED);
        Serial.println(error.code);
        return error;
    }

    return error_type::None;
}

/**
 * @brief Monitor OTA progress and display status updates
 *
 * Call this function in your main loop to monitor OTA progress
 * and display status updates every 5% of download progress.
 *
 * This function automatically handles completion and error states.
 */
void monitor_ota_progress() {
    using namespace photo_frame;

    if (!OTA_IS_ACTIVE()) {
        return;
    }

    RGB_SET_STATE(OTA_UPDATING); // Maintain OTA status during progress monitoring

    const auto& progress        = OTA_GET_PROGRESS();

    static uint8_t last_percent = 0;
    if (progress.progress_percent > last_percent + 5) { // Update every 5%
        Serial.print(TEXT_OTA_PROGRESS);
        Serial.print(progress.progress_percent);
        Serial.print(F("% ("));
        Serial.print(progress.downloaded_size);
        Serial.print(F("/"));
        Serial.print(progress.total_size);
        Serial.print(TEXT_OTA_BYTES);
        Serial.println(progress.message);

        last_percent = progress.progress_percent;
    }

    // Handle completion or errors
    if (progress.status == ota_status_t::COMPLETED) {
        Serial.println(TEXT_OTA_UPDATE_COMPLETED);
        // Device will restart automatically
    } else if (progress.status == ota_status_t::FAILED) {
        Serial.print(TEXT_OTA_UPDATE_FAILED);
        Serial.println(progress.error.message);
    }
}


/**
 * @brief Cancel any ongoing OTA update
 *
 * This function cancels the current OTA update process and cleans up resources.
 * Use this if you need to abort an update for any reason.
 */
void cancel_ota_update() {
    RGB_SET_STATE(OTA_UPDATING); // Show OTA cancellation in progress
    ota_updater.cancel_update();
    Serial.println(TEXT_OTA_UPDATE_CANCELLED);
}

/**
 * @brief Get OTA update status information
 *
 * @return String containing current OTA status and progress information
 */
String get_ota_status_info() {
    using namespace photo_frame;

    if (!OTA_IS_ACTIVE()) {
        return String(TEXT_OTA_SYSTEM_IDLE);
    }

    const auto& progress = OTA_GET_PROGRESS();
    String status        = String(TEXT_OTA_STATUS) + ": ";

    switch (progress.status) {
    case ota_status_t::NOT_STARTED:      status += TEXT_OTA_STATUS_NOT_STARTED; break;
    case ota_status_t::CHECKING_VERSION: status += TEXT_OTA_STATUS_CHECKING; break;
    case ota_status_t::DOWNLOADING:
        status +=
            String(TEXT_OTA_STATUS_DOWNLOADING) + " (" + String(progress.progress_percent) + "%)";
        break;
    case ota_status_t::INSTALLING: status += TEXT_OTA_STATUS_INSTALLING; break;
    case ota_status_t::COMPLETED:  status += TEXT_OTA_STATUS_COMPLETED; break;
    case ota_status_t::FAILED:
        status += String(TEXT_OTA_STATUS_FAILED) + ": " + progress.error.message;
        break;
    case ota_status_t::ROLLBACK:         status += "Rolling back"; break;
    case ota_status_t::NO_UPDATE_NEEDED: status += TEXT_OTA_STATUS_UP_TO_DATE; break;
    }

    return status;
}

} // namespace photo_frame

#endif // OTA_UPDATE_ENABLED