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
// DEFAULT MAIN - Normal Mode (Google Drive + SD Card)
// ============================================================================
// This is the main entry point for normal photo frame operation.
// Handles image loading from Google Drive or SD Card and display rendering.

#ifndef ENABLE_BT_IMAGE

#include <Arduino.h>

#include "battery.h"
#include "binary_utils.h"
#include "board_util.h"
#include "config.h"
#include "data_provider_gdrive.h"
#include "data_provider_manager.h"
#include "data_provider_sd.h"
#include "errors.h"
#include "google_drive.h"
#include "google_drive_client.h"
#include "image_load_result.h"
#include "io_utils.h"
#include "littlefs_manager.h"
#include "preferences_helper.h"
#include "renderer.h"
#include "rgb_status.h"
#include "sd_card.h"
#include "string_utils.h"
#include "unified_config.h"
#include "wifi_manager.h"

#include "main_common.h"
#include "main_default.h"

// ============================================================================
// LOCAL GLOBALS (Mode-specific)
// ============================================================================

// Google Drive instance (initialized from JSON config)
photo_frame::GoogleDrive drive;
photo_frame::SdCard sdCard; // SD_MMC uses fixed SDIO pins
photo_frame::WifiManager wifiManager;
photo_frame::unified_config systemConfig; // Unified configuration system

// Data provider instances (created locally in setup, no dynamic allocation needed)
// Removed - created as stack objects in default_main_setup()

// ============================================================================
// FORWARD DECLARATIONS (from original main.cpp)
// ============================================================================

photo_frame::photo_frame_error_t
setup_time_and_connectivity(const photo_frame::battery_info_t& battery_info,
                            bool is_reset,
                            DateTime& now);

// ============================================================================
// MODE-SPECIFIC IMPLEMENTATIONS
// ============================================================================

photo_frame::photo_frame_error_t
setup_time_and_connectivity(const photo_frame::battery_info_t& battery_info,
                            bool is_reset,
                            DateTime& now) {
    photo_frame::photo_frame_error_t error = photo_frame::error_type::None;

    log_i("--------------------------------------");
    log_i("- Initialize SD card and load configuration...");
    log_i("--------------------------------------");

    // PHASE 1: SD Card Operations - Display OFF to avoid SPI conflicts
    photo_frame::board_utils::display_power_off();

    RGB_SET_STATE(SD_READING); // Show SD card operations
    error = sdCard.begin();

    // Reduce RGB brightness if battery is low to save power
    if (battery_info.is_low()) {
#ifdef RGB_STATUS_ENABLED
        rgbStatus.setBrightness(32); // Reduce brightness to 50% of normal for low battery
#endif                               // RGB_STATUS_ENABLED
    }

    // Load unified configuration from SD card
    if (error == photo_frame::error_type::None) {
        log_i("Loading unified configuration...");
        error =
            photo_frame::load_unified_config_with_fallback(sdCard, CONFIG_FILEPATH, systemConfig);

        if (error != photo_frame::error_type::None) {
            log_w("Failed to load unified configuration: %d", error.code);
            // Configuration loading failed, but fallback values are loaded
            // Continue with fallback configuration
            error = photo_frame::error_type::None;
        }

        // Validate essential configuration
        if (!systemConfig.wifi.is_valid()) {
            log_w("WARNING: WiFi configuration is missing or invalid!");
            log_w("Please ensure CONFIG_FILEPATH contains valid WiFi credentials");
            error = photo_frame::error_type::WifiCredentialsNotFound;
        }
    } else {
        log_w("SD card initialization failed - using fallback configuration");

        // SD card failed, load fallback configuration and calculate extended sleep
        load_fallback_config(systemConfig);

        // Enter deep sleep immediately with extended duration
        // photo_frame::board_utils::enter_deep_sleep(ESP_SLEEP_WAKEUP_UNDEFINED,
        // fallback_sleep_microseconds);
        return error; // This line won't be reached, but included for completeness
    }

    // WiFi is optional for SD card mode, required for Google Drive
    bool wifiRequired = systemConfig.GoogleDrive.enabled;

    if (error == photo_frame::error_type::None) {
        log_i("Initializing WiFi manager with unified configuration...");
        RGB_SET_STATE(WIFI_CONNECTING); // Show WiFi connecting status

        // Initialize WiFi manager with multiple networks from unified config
        error = wifiManager.initWithNetworks(systemConfig.wifi);

        if (error == photo_frame::error_type::None) {
            // NTP-only time fetching
            log_i("Fetching time from NTP servers...");
            error = wifiManager.connect();
            if (error == photo_frame::error_type::None) {
                now = wifiManager.fetchDatetime(&error);
                if (!now.isValid() || error != photo_frame::error_type::None) {
                    log_e("Failed to fetch time from NTP!");
                    if (error == photo_frame::error_type::None) {
                        error = photo_frame::error_type::NTPSyncFailed;
                    }
                } else {
                    log_i("Successfully fetched time from NTP: %s",
                          now.timestamp(DateTime::TIMESTAMP_FULL).c_str());
                }
            }
        } else {
            log_e("WiFi initialization failed");
            RGB_SET_STATE_TIMED(WIFI_FAILED, 2000); // Show WiFi failed status
        }

        // If WiFi is not required (SD card mode) and it failed, clear the error
        if (!wifiRequired && error != photo_frame::error_type::None) {
            log_w("WiFi failed but not required for SD card mode, continuing without time sync");
            error = photo_frame::error_type::None;
            // Set a default time if WiFi failed
            now = DateTime(2024, 1, 1, 12, 0, 0);
        }
    }

    if (error == photo_frame::error_type::None) {
        log_i("Current time is valid: %s", now.isValid() ? "Yes" : "No");
    } else {
        log_e("Failed to fetch current time! Error code: %d", error.code);
    }

    return error;
}

// ============================================================================
// SETUP & LOOP
// ============================================================================

void default_main_setup() {
    Serial.begin(115200);
    delay(5000);

    // Initialize display power control (if configured)
    photo_frame::board_utils::init_display_power();

    log_i("\n==================================");
    log_i("*** NORMAL MODE ***");
    log_i("==================================");

    // Initialize hardware components
    if (!initialize_hardware()) {
        log_e("Failed to initialize hardware!");
        return;
    }

    // Determine wakeup reason and setup basic state
    esp_sleep_wakeup_cause_t wakeup_reason = photo_frame::board_utils::get_wakeup_reason();

    // Consider it a reset if it's an undefined wakeup (power on/reset)
    // EXT1 wakeup (button press) goes through normal TOC validation
    bool is_reset = wakeup_reason == ESP_SLEEP_WAKEUP_UNDEFINED;

    char wakeup_reason_string[32];
    photo_frame::board_utils::get_wakeup_reason_string(
        wakeup_reason, wakeup_reason_string, sizeof(wakeup_reason_string));

    log_i("Wakeup reason: %s (%d)", wakeup_reason_string, wakeup_reason);
    log_i("Is reset: %s", is_reset ? "Yes" : "No");

    // Setup battery and power management
    photo_frame::battery_info_t battery_info;
    photo_frame::photo_frame_error_t error = setup_battery_and_power(battery_info, wakeup_reason);

    // Setup time synchronization and connectivity
    DateTime now = DateTime((uint32_t)0);

    if (error == photo_frame::error_type::None && !battery_info.is_critical()) {
        error = setup_time_and_connectivity(battery_info, is_reset, now);
    }

    // Set rotation from config BEFORE initializing buffer
    display_rotation = systemConfig.board.display_rotation;

    // If config loading failed, try to get from preferences
    if (!systemConfig.is_valid()) {
        auto& prefs      = photo_frame::PreferencesHelper::getInstance();
        display_rotation = prefs.getDisplayRotation(); // Default to 0 (landscape) if not set
        log_w("Config invalid, using rotation from preferences: %u", display_rotation);
    }

    log_i("Display rotation: %u", display_rotation);

    // Phase 1: Initialize PSRAM image buffer BEFORE SD card operations
    // This allocates the buffer but does NOT initialize display hardware
    log_i("--------------------------------------");
    log_i("- Phase 1: Initializing image buffer...");
    log_i("--------------------------------------");
    if (!init_image_buffer()) {
        // Critical failure - cannot continue without buffer
        log_e("[main] FATAL: Buffer initialization failed!");
        log_e("[main] Entering deep sleep mode");

        const uint64_t emergency_sleep_duration = 60 * 60 * 1000000ULL; // 1 hour
        photo_frame::board_utils::enter_deep_sleep(ESP_SLEEP_WAKEUP_UNDEFINED,
                                                   emergency_sleep_duration);
        return;
    }

    // Handle image loading via data provider
    photo_frame::ImageLoadResult image_result;

    if (error == photo_frame::error_type::None && !battery_info.is_critical()) {
        // Create data providers as stack objects (no dynamic allocation)
        photo_frame::SdCardDataProvider sdcard_provider;
        photo_frame::GoogleDriveDataProvider gdrive_provider(drive);

        // Create and configure provider manager
        photo_frame::DataProviderManager provider_manager(sdCard, systemConfig);
        provider_manager.register_provider(&sdcard_provider);
        provider_manager.register_provider(&gdrive_provider);

        // Load image using manager wrapper (passes config and sdcard automatically)
        log_i("Using data provider: %s",
              provider_manager.get_active_provider()
                  ? provider_manager.get_active_provider()->name()
                  : "none");
        RGB_SET_STATE(GOOGLE_DRIVE); // Default to Google Drive status LED
        image_result = provider_manager.load_next_image(is_reset);
        error        = image_result.error;
    }

    // Cleanup data provider resources (automatic with unique_ptr)

    // Safely disconnect WiFi with proper cleanup
    if (wifiManager.isConnected()) {
        log_i("Disconnecting WiFi to save power...");
        // Add small delay to ensure pending WiFi operations complete
        delay(100);
        wifiManager.disconnect();
        // Wait for disconnect to complete
        delay(200);
    }

    log_i("WiFi operations complete - using NTP-only time");

    // Calculate refresh delay
    log_i("--------------------------------------");
    log_i("- Calculating refresh rate");
    log_i("--------------------------------------");
    refresh_delay_t refresh_delay = calculate_wakeup_delay(battery_info, now);

    // Phase 2: Initialize E-Paper display hardware (after SD card operations are complete)
    log_i("--------------------------------------");
    log_i("- Phase 2: Initializing display hardware...");
    log_i("--------------------------------------");

    // PHASE 2: Display Operations - Power ON display now that SD card is closed
    photo_frame::board_utils::display_power_on();

    delay(100);
    RGB_SET_STATE(RENDERING); // Show display rendering

    // Initialize the display hardware now that SD card is closed
    if (!init_display_hardware()) {
        // Critical failure - cannot continue without display
        log_e("[main] FATAL: Display hardware initialization failed!");
        log_e("[main] Entering deep sleep mode to preserve battery");

        const uint64_t emergency_sleep_duration = 60 * 60 * 1000000ULL; // 1 hour
        photo_frame::board_utils::enter_deep_sleep(ESP_SLEEP_WAKEUP_UNDEFINED,
                                                   emergency_sleep_duration);
        return;
    }

    // Allow time for display SPI bus initialization to complete
    delay(300);
    log_i("Display initialization complete");

    // Prepare display for rendering
    // Note: fillScreen() removed in v0.11.0 to eliminate race condition causing white vertical
    // stripes The full-screen image write will overwrite the entire display buffer, making
    // fillScreen() redundant
    log_i("Preparing display for rendering...");

    // Check if image was loaded successfully
    if (error == photo_frame::error_type::None && !image_result.is_success()) {
        log_e("Image file is not ready!");
        error = photo_frame::error_type::SdCardFileOpenFailed;
    }

    // Handle errors or process image file
    if (error != photo_frame::error_type::None) {
        RGB_SET_STATE(ERROR); // Show error status

        auto& display = photo_frame::DisplayManager::getInstance();
        // Clear display and draw error (include filename if available)
        display.clear(DISPLAY_COLOR_WHITE);
        display.drawError(error,
                          image_result.original_filename.isEmpty()
                              ? nullptr
                              : image_result.original_filename.c_str());

        if (error != photo_frame::error_type::BatteryLevelCritical && now.isValid()) {
            display.drawLastUpdate(now, refresh_delay.refresh_seconds);
        }

        // Render to display
        display.render();
    } else {
        // Render the image if it was successfully loaded
        if (error == photo_frame::error_type::None && image_result.is_success()) {
            log_i("Rendering validated binary image...");
            error = render_image(*image_result.image_file,
                                 image_result.original_filename.c_str(),
                                 error,
                                 now,
                                 refresh_delay,
                                 image_result.file_index,
                                 image_result.total_files,
                                 drive,
                                 battery_info);
        }
    }

    // Finalize and enter sleep - show sleep preparation with delay
    RGB_SET_STATE(SLEEP_PREP); // Show sleep preparation
    delay(2500);               // Allow sleep preparation animation to complete
    finalize_and_enter_sleep(battery_info, now, wakeup_reason, refresh_delay);
}

void default_main_loop() {
    delay(1000); // Just to avoid watchdog reset
}

#endif // ENABLE_BT_IMAGE