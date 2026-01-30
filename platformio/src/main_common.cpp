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

#include "main_common.h"

#include "esp32/spiram.h"
#include <Arduino.h>

#include "battery.h"
#include "board_util.h"
#include "config.h"
#include "datetime_utils.h"
#include "display_manager.h"
#include "errors.h"
#include "google_drive.h"
#include "io_utils.h"
#include "littlefs_manager.h"
#include "preferences_helper.h"
#include "renderer.h"
#include "rgb_status.h"
#include "string_utils.h"
#include "unified_config.h"

#include <assets/icons/icons.h>

// ============================================================================
// GLOBAL OBJECTS
// ============================================================================
// Note: DisplayManager is now a singleton - use DisplayManager::getInstance()
uint8_t display_rotation  = 0; // 0=0°,1=90°,2=180°,3=270°
unsigned long startupTime = 0;

#ifndef USE_SENSOR_MAX1704X
photo_frame::BatteryReader battery_reader(BATTERY_PIN,
                                          BATTERY_RESISTORS_RATIO,
                                          BATTERY_NUM_READINGS,
                                          BATTERY_DELAY_BETWEEN_READINGS);
#else  // USE_SENSOR_MAX1704X
photo_frame::BatteryReader battery_reader;
#endif // USE_SENSOR_MAX1704X

// ============================================================================
// COMMON IMPLEMENTATIONS
// ============================================================================

bool initialize_hardware() {
    analogReadResolution(12);
    startupTime = millis();
    photo_frame::board_utils::blink_builtin_led(1, 900, 100);
    photo_frame::board_utils::disable_built_in_led();

#ifdef LED_PWR_PIN
    pinMode(LED_PWR_PIN, OUTPUT);
    digitalWrite(LED_PWR_PIN, LOW);
#endif // LED_PWR_PIN

    // PSRAM initialization (mandatory)
    log_i("[PSRAM] Initializing PSRAM...");

    // Check if PSRAM is already initialized by the framework
    if (psramFound()) {
        log_i("[PSRAM] PSRAM already initialized by framework");
        log_i("[PSRAM] Available PSRAM: %u bytes", ESP.getPsramSize());
        log_i("[PSRAM] Free PSRAM: %u bytes", ESP.getFreePsram());
    } else {
        // Try manual initialization if not already done
        esp_err_t ret = esp_spiram_init();
        if (ret != ESP_OK) {
            log_e("[PSRAM] CRITICAL: Failed to initialize PSRAM: %s", esp_err_to_name(ret));
            log_e("[PSRAM] PSRAM is required for this board configuration!");
            log_e("[PSRAM] System will likely crash due to memory constraints");
            return false;
        } else {
            log_i("[PSRAM] PSRAM initialized successfully");

            // Note: esp_spiram_add_to_heapalloc() is deprecated in newer ESP-IDF
            // PSRAM should be automatically added to heap with CONFIG_SPIRAM_USE_MALLOC
            // which is set in platformio.ini via board_build.arduino.memory_type

            log_i("[PSRAM] Available PSRAM: %u bytes", ESP.getPsramSize());
            log_i("[PSRAM] Free PSRAM: %u bytes", ESP.getFreePsram());
        }
    }

#ifdef RGB_STATUS_ENABLED
    // Initialize RGB status system (FeatherS3 NeoPixel)
    log_i("[RGB] Initializing RGB status system...");
    if (!rgbStatus.begin()) {
        log_w("[RGB] Warning: Failed to initialize RGB status system");
    } else {
        RGB_SET_STATE(STARTING); // Show startup indication
    }
#else
    log_i("[RGB] RGB status system disabled");
#endif // RGB_STATUS_ENABLED

    log_i("------------------------------");
    log_i("Photo Frame %s", FIRMWARE_VERSION_STRING);
    log_i("------------------------------");

    photo_frame::board_utils::print_board_stats();

#if DEBUG_MODE
    photo_frame::board_utils::print_board_pins();
#endif // DEBUG_MODE

    return true;
}

bool init_image_buffer() {
    auto& display = photo_frame::DisplayManager::getInstance();
    log_i("[main] Initializing display buffer (Phase 1)...");

    if (!display.initBuffer(true)) { // true = prefer PSRAM
        log_e("[main] CRITICAL: Failed to initialize display buffer!");
        log_e("[main] Cannot continue without buffer");
        return false;
    }

    log_i("[main] Display buffer initialized successfully");
    log_i("[main] Buffer size: %u bytes", display.getBufferSize());
    return true;
}

bool init_display_hardware() {
    auto& display = photo_frame::DisplayManager::getInstance();
    log_i("[main] Initializing display hardware (Phase 2)...");

    if (!display.initDisplay()) {
        log_e("[main] CRITICAL: Failed to initialize display hardware!");
        log_e("[main] Cannot continue without display");
        return false;
    }

    log_i("[main] Display hardware initialized successfully");
    return true;
}

void cleanup_image_buffer() {
    auto& display = photo_frame::DisplayManager::getInstance();
    if (display.isInitialized()) {
        log_i("[main] Releasing display manager");
        display.release();
        log_i("[main] Display manager released");
    }
}

photo_frame::photo_frame_error_t setup_battery_and_power(photo_frame::battery_info_t& battery_info,
                                                         esp_sleep_wakeup_cause_t wakeup_reason) {
    log_i("--------------------------------------");
    log_i("- Reading battery level...");
    log_i("--------------------------------------");

    battery_reader.init();
    battery_info = battery_reader.read();

    // print the battery levels
#ifdef DEBUG_BATTERY_READER
    log_i("Battery level: %d%%, %d mV, Raw mV: %d",
          battery_info.percent,
          battery_info.millivolts,
          battery_info.raw_millivolts);
#else
    log_i("Battery level: %.1f%%, %.1f mV", battery_info.percent, battery_info.millivolts);
#endif // DEBUG_BATTERY_READER

    if (battery_info.is_empty()) {
        log_e("Battery is empty!");
#ifdef BATTERY_POWER_SAVING
        unsigned long elapsed = millis() - startupTime;
        log_i("Elapsed seconds since startup: %lu s", elapsed / 1000);

        log_i("Entering deep sleep to preserve battery...");
        photo_frame::board_utils::enter_deep_sleep(wakeup_reason); // Enter deep sleep mode
                                                                   // Battery too low to continue
#endif                                                             // BATTERY_POWER_SAVING
#ifdef RGB_STATUS_ENABLED
        rgbStatus.disable();
        log_i("[RGB] Disabled RGB LED to conserve battery power");
#endif // RGB_STATUS_ENABLED
        return photo_frame::error_type::BatteryEmpty;
    } else if (battery_info.is_critical()) {
        log_w("Battery level is critical!");
        RGB_SET_STATE_TIMED(BATTERY_LOW, 3000); // Show battery critical warning briefly

        // Disable RGB after warning to save maximum power
        delay(3500);
#ifdef RGB_STATUS_ENABLED
        rgbStatus.disable();
        log_i("[RGB] Disabled RGB LED to conserve battery power");
#endif // RGB_STATUS_ENABLED
       // Indicate critical battery error
        return photo_frame::error_type::BatteryLevelCritical;
    }

    // Battery level allows continued operation
    return photo_frame::error_type::None;
}

refresh_delay_t calculate_wakeup_delay(photo_frame::battery_info_t& battery_info, DateTime& now) {
    refresh_delay_t refresh_delay = {0, 0};

    if (battery_info.is_critical()) {
        log_w("Battery is critical, using low battery refresh interval");
        refresh_delay.refresh_seconds = REFRESH_INTERVAL_SECONDS_CRITICAL_BATTERY;
        refresh_delay.refresh_microseconds =
            (uint64_t)refresh_delay.refresh_seconds * MICROSECONDS_IN_SECOND;
        return refresh_delay;
    }

    if (!now.isValid()) {
        log_w("Time is invalid, using minimum refresh interval as fallback");
        refresh_delay.refresh_seconds = REFRESH_MIN_INTERVAL_SECONDS;
        refresh_delay.refresh_microseconds =
            (uint64_t)refresh_delay.refresh_seconds * MICROSECONDS_IN_SECOND;
        return refresh_delay;
    }

    if (true) { // Changed from the original condition to always execute
        // NOTE: systemConfig must be available in the calling main file
        extern photo_frame::unified_config systemConfig;

        refresh_delay.refresh_seconds =
            photo_frame::board_utils::read_refresh_seconds(systemConfig, battery_info.is_low());

        log_i("Refresh seconds: %ld", refresh_delay.refresh_seconds);

        // add the refresh time to the current time
        // Update the current time with the refresh interval
        DateTime nextRefresh = now + TimeSpan(refresh_delay.refresh_seconds);

        // check if the next refresh time is after DAY_END_HOUR
        DateTime dayEnd = DateTime(now.year(), now.month(), now.day(), DAY_END_HOUR, 0, 0);

        if (nextRefresh > dayEnd) {
            log_i("Next refresh time is after DAY_END_HOUR");
            // Check if we're currently in the inactive period (before DAY_START_HOUR)
            if (now.hour() < DAY_START_HOUR) {
                // We're in early morning hours, schedule for DAY_START_HOUR today
                nextRefresh = DateTime(now.year(), now.month(), now.day(), DAY_START_HOUR, 0, 0);
            } else {
                // We're past DAY_END_HOUR, schedule for DAY_START_HOUR tomorrow
                DateTime tomorrow = now + TimeSpan(1, 0, 0, 0); // Add 1 day safely
                nextRefresh       = DateTime(
                    tomorrow.year(), tomorrow.month(), tomorrow.day(), DAY_START_HOUR, 0, 0);
            }
            refresh_delay.refresh_seconds = nextRefresh.unixtime() - now.unixtime();
        }

        log_i("Next refresh time: %s", nextRefresh.timestamp(DateTime::TIMESTAMP_FULL).c_str());

        // Convert seconds to microseconds for deep sleep with overflow protection
        // ESP32 max sleep time is ~18 hours (281474976710655 microseconds)
        // Practical limit defined in config.h

        if (refresh_delay.refresh_seconds <= 0) {
            log_w("Warning: Invalid refresh interval, using minimum interval as fallback");
            refresh_delay.refresh_seconds = REFRESH_MIN_INTERVAL_SECONDS;
            refresh_delay.refresh_microseconds =
                (uint64_t)refresh_delay.refresh_seconds * MICROSECONDS_IN_SECOND;
        } else if (refresh_delay.refresh_seconds > MAX_DEEP_SLEEP_SECONDS) {
            refresh_delay.refresh_microseconds =
                (uint64_t)MAX_DEEP_SLEEP_SECONDS * MICROSECONDS_IN_SECOND;
            log_w("Warning: Refresh interval capped to %d seconds to prevent overflow",
                  MAX_DEEP_SLEEP_SECONDS);
        } else {
            // Safe calculation using 64-bit arithmetic
            refresh_delay.refresh_microseconds =
                (uint64_t)refresh_delay.refresh_seconds * MICROSECONDS_IN_SECOND;
        }

        char humanReadable[64];
        photo_frame::string_utils::seconds_to_human(
            humanReadable, sizeof(humanReadable), refresh_delay.refresh_seconds);

        log_i("Refresh interval in: %s", humanReadable);
    }

    // Final validation - ensure we never return 0 microseconds
    if (refresh_delay.refresh_microseconds == 0) {
        log_e("CRITICAL: refresh_microseconds is 0, forcing minimum interval");
        refresh_delay.refresh_seconds = REFRESH_MIN_INTERVAL_SECONDS;
        refresh_delay.refresh_microseconds =
            (uint64_t)refresh_delay.refresh_seconds * MICROSECONDS_IN_SECOND;
    }

    log_i("Final refresh delay: %ld seconds (%lu microseconds)",
          refresh_delay.refresh_seconds,
          (unsigned long)(refresh_delay.refresh_microseconds / 1000000ULL));

    return refresh_delay;
}

void finalize_and_enter_sleep(photo_frame::battery_info_t& battery_info,
                              DateTime& now,
                              esp_sleep_wakeup_cause_t wakeup_reason,
                              const refresh_delay_t& refresh_delay) {
#ifdef RGB_STATUS_ENABLED
    // Shutdown RGB status system to save power during sleep
    log_i("[RGB] Shutting down RGB status system for sleep");
    rgbStatus.end(); // Properly shutdown NeoPixel and FreeRTOS task
#endif               // RGB_STATUS_ENABLED

    // Power off display and release resources before sleep
    photo_frame::DisplayManager::getInstance().powerOff();
    cleanup_image_buffer();

    delay(100);

    // now go to sleep using the pre-calculated refresh delay
    if (!battery_info.is_critical() &&
        refresh_delay.refresh_microseconds > MICROSECONDS_IN_SECOND) {
        log_i("Going to sleep for %ld seconds (%lu seconds from microseconds)",
              refresh_delay.refresh_seconds,
              (unsigned long)(refresh_delay.refresh_microseconds / 1000000ULL));
    } else {
        if (battery_info.is_critical()) {
            log_w("Battery is critical, entering indefinite sleep");
        } else {
            log_w("Sleep time too short or invalid, entering default sleep");
        }
    }

    unsigned long elapsed = millis() - startupTime;
    log_i("Elapsed seconds since startup: %lu s", elapsed / 1000);

    photo_frame::board_utils::enter_deep_sleep(wakeup_reason, refresh_delay.refresh_microseconds);
}

photo_frame::photo_frame_error_t
render_image(const photo_frame::binary_utils::PFR1BinaryFile& image_file,
             const char* original_filename,
             photo_frame::photo_frame_error_t current_error,
             const DateTime& now,
             const refresh_delay_t& refresh_delay,
             uint32_t image_index,
             uint32_t total_files,
             photo_frame::GoogleDrive& drive,
             const photo_frame::battery_info_t& battery_info) {
    photo_frame::photo_frame_error_t error = current_error;

    // Get display capabilities
    bool has_partial_update = photo_frame::rendererHasPartialUpdate();

    if (error == photo_frame::error_type::None && !has_partial_update) {
        // ----------------------------------------------
        // the display does not support partial update
        // ----------------------------------------------
        log_w("Display does not support partial update!");

        bool rendering_failed = false;
        uint16_t error_code   = 0;

#if defined(DISP_6C)
        // ============================================
        // 6C/7C displays: Non-paged rendering
        // ============================================
        // These displays render from PSRAM buffer (binary format only)
        // Binary format - render from PSRAM buffer (already loaded)
        // For 6C/7C displays: Use GFXcanvas8 to draw overlays on buffer with proper renderer
        // functions
        log_i("[main] Rendering Mode 1 format from PSRAM buffer with overlays");

        // Check portrait mode from config (or preferences fallback)
        extern photo_frame::unified_config systemConfig;
        uint8_t rotation = systemConfig.board.display_rotation;
        if (!systemConfig.is_valid()) {
            auto& prefs = photo_frame::PreferencesHelper::getInstance();
            rotation    = prefs.getDisplayRotation();
        }

        // Set rotation for portrait mode if needed
        auto& display = photo_frame::DisplayManager::getInstance();
        display.setRotation(rotation);

        // Draw overlay based on portrait mode
        display.drawOverlay();

        // Draw status information
        display.drawLastUpdate(now, refresh_delay.refresh_seconds);
        display.drawImageInfo(image_index, total_files, drive.get_last_image_source());
        display.drawBatteryStatus(battery_info);

        // Render the image with overlays to the display
        log_i("Rendering image to display...");
        if (!display.render()) {
            log_e("Failed to render image!");
            rendering_failed = true;
        } else {
            log_i("Display render complete with overlays");
        }
#else
        // ============================================
        // B/W displays: Same system as 6C, no paged rendering
        // ============================================
        auto& display = photo_frame::DisplayManager::getInstance();

        // Set rotation for portrait mode if needed
        display.setRotation(display_rotation);

        // Draw overlay based on portrait mode
        display.drawOverlay();

        // Draw status information
        display.drawLastUpdate(now, refresh_delay.refresh_seconds);
        display.drawImageInfo(image_index, total_files, drive.get_last_image_source());
        display.drawBatteryStatus(battery_info);

        // Render the image with overlays to the display
        log_i("Rendering B/W image to display...");
        if (!display.render()) {
            log_e("Failed to render B/W image!");
            rendering_failed = true;
        } else {
            log_i("B/W display render complete with overlays");
        }
#endif

#if defined(DISP_6C)
        // Power recovery delay after page refresh for 6-color displays
        // Increased from 400ms to 1000ms in v0.11.1 to improve power stability
        // Helps prevent:
        // - White vertical stripes from voltage drop
        // - Washout from incomplete capacitor recharge
        // - Display artifacts from power supply instability
        delay(1000);
#endif

        // Handle rendering errors
        if (rendering_failed) {
            log_w("Rendering failed!");

            auto& display = photo_frame::DisplayManager::getInstance();
            // Clear display and draw error
            display.clear(DISPLAY_COLOR_WHITE);
            display.drawErrorWithDetails(
                TXT_IMAGE_FORMAT_NOT_SUPPORTED, "", original_filename, error_code);

            // Draw status information
            display.drawLastUpdate(now, refresh_delay.refresh_seconds);
            display.drawImageInfo(image_index, total_files, drive.get_last_image_source());
            display.drawBatteryStatus(battery_info);

            // Render error to display
            display.render();
        }

    } else if (error == photo_frame::error_type::None) {
        // -------------------------------
        // Display has partial update - NOT SUPPORTED IN NEW SYSTEM
        // -------------------------------
        log_w("Partial update mode not yet supported in new display system");

        auto& display = photo_frame::DisplayManager::getInstance();
        // For now, use same rendering as full update
        // Draw overlay and status
        display.drawOverlay();
        display.drawLastUpdate(now, refresh_delay.refresh_seconds);
        display.drawImageInfo(image_index, total_files, drive.get_last_image_source());
        display.drawBatteryStatus(battery_info);

        // Render to display
        display.render();
    }

    return error;
}
