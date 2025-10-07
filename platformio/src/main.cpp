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

#define LOG_TO_FILE 1

#include <Arduino.h>
#include <LittleFS.h>

#include "esp32/spiram.h"

#include "battery.h"
#include "board_util.h"
#include "config.h"
#include "errors.h"
#include "io_utils.h"
#include "preferences_helper.h"
#include "renderer.h"
#include "rgb_status.h"

#include "rtc_util.h"
#include "sd_card.h"
#include "spi_manager.h"
#include "string_utils.h"
#include "unified_config.h"
#include "wifi_manager.h"

#include "weather.h"

#include <assets/icons/icons.h>

#include "google_drive.h"
#include "google_drive_client.h"

#ifdef OTA_UPDATE_ENABLED
#include "ota_integration.h"
#endif // OTA_UPDATE_ENABLED

#ifdef ENABLE_DISPLAY_DIAGNOSTIC
#include "display_diagnostic.h"
#endif // ENABLE_DISPLAY_DIAGNOSTIC

// Google Drive instance (initialized from JSON config)
photo_frame::google_drive drive;

// Helper function to cleanup temporary files from LittleFS
void cleanup_temp_image_file()
{
    photo_frame::spi_manager::SPIManager::cleanup_temp_files("*.tmp");
}

#ifndef USE_SENSOR_MAX1704X
photo_frame::battery_reader battery_reader(BATTERY_PIN,
    BATTERY_RESISTORS_RATIO,
    BATTERY_NUM_READINGS,
    BATTERY_DELAY_BETWEEN_READINGS);
#else // USE_SENSOR_MAX1704X
photo_frame::battery_reader battery_reader;
#endif // USE_SENSOR_MAX1704X

photo_frame::sd_card sdCard; // SD_MMC uses fixed SDIO pins
photo_frame::wifi_manager wifiManager;
photo_frame::unified_config systemConfig; // Unified configuration system
photo_frame::weather::WeatherManager weatherManager;

unsigned long startupTime = 0;

typedef struct {
    long refresh_seconds;
    uint64_t refresh_microseconds;
} refresh_delay_t;

refresh_delay_t calculate_wakeup_delay(photo_frame::battery_info_t& battery_info, DateTime& now);

/**
 * @brief Initialize hardware components (Serial, RGB LED, board utilities)
 * @return true if initialization successful, false otherwise
 */
bool initialize_hardware();

/**
 * @brief Read and validate battery level, handle critical battery states
 * @param battery_info Reference to store battery information
 * @param wakeup_reason Current wakeup reason for power management decisions
 * @return Error state after battery check
 */
photo_frame::photo_frame_error_t setup_battery_and_power(photo_frame::battery_info_t& battery_info,
    esp_sleep_wakeup_cause_t wakeup_reason);

/**
 * @brief Setup time synchronization via WiFi and NTP
 * @param battery_info Battery information for power conservation decisions
 * @param is_reset Whether this is a reset/startup (affects NTP sync behavior)
 * @param now Reference to store current DateTime
 * @return Error state after time/WiFi operations
 */
photo_frame::photo_frame_error_t
setup_time_and_connectivity(const photo_frame::battery_info_t& battery_info, bool is_reset, DateTime& now);

/**
 * @brief Handle weather data fetching with WiFi connection management
 * @param battery_info Battery information for power conservation decisions
 * @param weatherManager Weather manager instance for updates
 * @return Error state after weather operations
 */
photo_frame::photo_frame_error_t
handle_weather_operations(const photo_frame::battery_info_t& battery_info,
    photo_frame::weather::WeatherManager& weatherManager);

/**
 * @brief Handle Google Drive operations (initialization, file selection, download)
 * @param is_reset Whether this is a reset/startup (affects TOC writing)
 * @param file Reference to store opened file handle
 * @param original_filename Reference to store original filename for format detection
 * @param image_index Reference to store selected image index
 * @param total_files Reference to store total file count
 * @param battery_info Battery information for download decisions
 * @return Error state after Google Drive operations
 */
photo_frame::photo_frame_error_t
handle_google_drive_operations(bool is_reset,
    fs::File& file,
    String& original_filename,
    uint32_t& image_index,
    uint32_t& total_files,
    const photo_frame::battery_info_t& battery_info);

/**
 * @brief Process image file (validation, copying to LittleFS, rendering)
 * @param file File handle for image to process (temporary file in LittleFS)
 * @param original_filename Original filename from SD card (used for format detection and error
 * reporting)
 * @param current_error Current error state
 * @param now Current DateTime for display
 * @param refresh_delay Refresh delay information
 * @param image_index Current image index
 * @param total_files Total number of files
 * @param battery_info Battery information
 * @param weatherManager Weather manager instance
 * @return Updated error state after image processing
 */
photo_frame::photo_frame_error_t
process_image_file(fs::File& file,
    const char* original_filename,
    photo_frame::photo_frame_error_t current_error,
    const DateTime& now,
    const refresh_delay_t& refresh_delay,
    uint32_t image_index,
    uint32_t total_files,
    const photo_frame::battery_info_t& battery_info,
    photo_frame::weather::WeatherManager& weatherManager);

/**
 * @brief Handle final cleanup and prepare for deep sleep
 * @param battery_info Battery information for sleep calculations
 * @param now Current DateTime for sleep timing
 * @param wakeup_reason Wakeup reason for sleep decisions
 * @param refresh_delay Pre-calculated refresh delay to avoid re-reading potentiometer
 */
void finalize_and_enter_sleep(photo_frame::battery_info_t& battery_info,
    DateTime& now,
    esp_sleep_wakeup_cause_t wakeup_reason,
    const refresh_delay_t& refresh_delay);

/**
 * @brief Handle image rendering with full/partial display modes and error handling
 * @param file The validated image file to render (temporary file in LittleFS)
 * @param original_filename Original filename from SD card (used for format detection and error
 * reporting)
 * @param current_error Current error state
 * @param now Current DateTime for info display
 * @param refresh_delay Refresh delay info
 * @param image_index Current image index
 * @param total_files Total number of files
 * @param drive Google Drive instance
 * @param battery_info Battery information
 * @param weatherManager Weather manager instance
 * @return Updated error state after rendering attempt
 */
photo_frame::photo_frame_error_t render_image(fs::File& file,
    const char* original_filename,
    photo_frame::photo_frame_error_t current_error,
    const DateTime& now,
    const refresh_delay_t& refresh_delay,
    uint32_t image_index,
    uint32_t total_files,
    photo_frame::google_drive& drive,
    const photo_frame::battery_info_t& battery_info,
    photo_frame::weather::WeatherManager& weatherManager);

void setup()
{
    // Initialize hardware components
    if (!initialize_hardware()) {
        return;
    }

    // Determine wakeup reason and setup basic state
    esp_sleep_wakeup_cause_t wakeup_reason = photo_frame::board_utils::get_wakeup_reason();
    bool is_reset = wakeup_reason == ESP_SLEEP_WAKEUP_UNDEFINED;

    char wakeup_reason_string[32];
    photo_frame::board_utils::get_wakeup_reason_string(
        wakeup_reason, wakeup_reason_string, sizeof(wakeup_reason_string));

    Serial.print(F("Wakeup reason: "));
    Serial.print(wakeup_reason_string);
    Serial.print(F(" ("));
    Serial.print(wakeup_reason);
    Serial.println(F(")"));

    Serial.print(F("Is reset: "));
    Serial.println(is_reset ? "Yes" : "No");

#ifdef ENABLE_DISPLAY_DIAGNOSTIC
    // Check for diagnostic mode trigger
    Serial.println(F("\n================================="));
    Serial.println(F("Press 'd' within 5 seconds to run display diagnostics..."));
    Serial.println(F("=================================\n"));

    unsigned long diagnostic_wait = millis();
    bool run_diagnostic = false;

    // Flush any existing serial data
    while(Serial.available()) {
        Serial.read();
    }

    // Wait for 'd' key press
    while(millis() - diagnostic_wait < 5000) {
        if(Serial.available()) {
            char c = Serial.read();
            if(c == 'd') {
                run_diagnostic = true;
                break;
            }
        }
        delay(10); // Small delay to prevent tight loop
    }

    if(run_diagnostic) {
        Serial.println(F("\n>>> Starting display diagnostics..."));

        // Initialize display using proper renderer method
        photo_frame::renderer::init_display();

        // Run the full diagnostic suite
        display_diagnostic::runFullDiagnostic();

        Serial.println(F("\n>>> Diagnostic complete. System will restart in 10 seconds..."));
        delay(10000);
        ESP.restart();
    }
#endif // ENABLE_DISPLAY_DIAGNOSTIC

    // Setup battery and power management
    photo_frame::battery_info_t battery_info;
    photo_frame::photo_frame_error_t error = setup_battery_and_power(battery_info, wakeup_reason);

#if BATTERY_POWER_SAVING
    if (error == photo_frame::error_type::BatteryEmpty) {
        // stop here as battery is empty and we entered deep sleep
        return;
    }
#endif // BATTERY_POWER_SAVING

    // Setup time synchronization and connectivity
    DateTime now = DateTime((uint32_t)0);

    if (error == photo_frame::error_type::None && !battery_info.is_critical()) {
        error = setup_time_and_connectivity(battery_info, is_reset, now);
    }

#ifdef OTA_UPDATE_ENABLED
    // Handle OTA updates only if battery level is sufficient
    if (error == photo_frame::error_type::None && !battery_info.is_critical() && battery_info.percent >= OTA_MIN_BATTERY_PERCENT) {
        Serial.println(F("--------------------------------------"));
        Serial.println(F("- Checking for OTA updates..."));
        Serial.println(F("--------------------------------------"));

        auto ota_error = photo_frame::initialize_ota_updates();
        if (ota_error == photo_frame::error_type::None) {
            Serial.println(F("OTA system initialized successfully"));

            ota_error = photo_frame::handle_ota_updates_setup(wakeup_reason);

            if (ota_error == photo_frame::error_type::None) {
                Serial.println(F("OTA check completed - no update needed"));
            } else if (ota_error == photo_frame::error_type::OtaVersionIncompatible) {
                Serial.println(F("Current firmware is too old - manual update required"));
            } else if (ota_error == photo_frame::error_type::NoUpdateNeeded) {
                Serial.println(F("Firmware is up to date"));
            } else {
                Serial.print(F("OTA check failed: "));
                Serial.println(ota_error.code);
            }
            // If update was successful, device would have restarted - this line won't execute
        } else {
            Serial.print(F("Failed to initialize OTA system: "));
            Serial.println(ota_error.code);
        }
    } else {
        Serial.print(F("Battery level too low for OTA updates ("));
        Serial.print(battery_info.percent);
        Serial.print(F("% < "));
        Serial.print(OTA_MIN_BATTERY_PERCENT);
        Serial.println(F("%)"));
    }
#endif // OTA_UPDATE_ENABLED

    // Handle weather operations after OTA updates
    if (error == photo_frame::error_type::None && !battery_info.is_critical()) {
        auto weather_error = handle_weather_operations(battery_info, weatherManager);
        // Weather errors are not critical - don't update main error state
        if (weather_error != photo_frame::error_type::None) {
            Serial.print(F("Weather operations failed: "));
            Serial.println(weather_error.code);
        }
    }

    // Handle Google Drive operations
    fs::File file;
    uint32_t image_index = 0, total_files = 0;
    String original_filename; // Store original filename for format detection

    if (error == photo_frame::error_type::None && !battery_info.is_critical()) {
        RGB_SET_STATE(GOOGLE_DRIVE); // Show Google Drive operations
        error = handle_google_drive_operations(
            is_reset, file, original_filename, image_index, total_files, battery_info);
    }

    // Safely disconnect WiFi with proper cleanup
    if (wifiManager.is_connected()) {
        Serial.println(F("Disconnecting WiFi to save power..."));
        // Add small delay to ensure pending WiFi operations complete
        delay(100);
        wifiManager.disconnect();
        // Wait for disconnect to complete
        delay(200);
    }

    Serial.println(F("WiFi operations complete - using NTP-only time"));

    // Calculate refresh delay
    Serial.println(F("--------------------------------------"));
    Serial.println(F("- Calculating refresh rate"));
    Serial.println(F("--------------------------------------"));
    refresh_delay_t refresh_delay = calculate_wakeup_delay(battery_info, now);

    // Initialize E-Paper display
    Serial.println(F("--------------------------------------"));
    Serial.println(F("- Initializing E-Paper display..."));
    Serial.println(F("--------------------------------------"));

    delay(100);
    RGB_SET_STATE(RENDERING); // Show display rendering
    photo_frame::renderer::init_display();
    delay(100);

    display.clearScreen();

    // Check if file is available
    if (error == photo_frame::error_type::None && !file) {
        Serial.println(F("File is not open!"));
        error = photo_frame::error_type::SdCardFileOpenFailed;
    }

    // Handle errors or process image file
    if (error != photo_frame::error_type::None) {
        RGB_SET_STATE(ERROR); // Show error status
        display.firstPage();
        do {
            photo_frame::renderer::draw_error(error);
            photo_frame::renderer::draw_error_message(gravity_t::TOP_RIGHT, error.code);

            if (error != photo_frame::error_type::BatteryLevelCritical) {
                photo_frame::renderer::draw_last_update(now, refresh_delay.refresh_seconds);
            }
        } while (display.nextPage());
    } else {
        // Process image file (validation and rendering)
        error = process_image_file(file,
            original_filename.c_str(),
            error,
            now,
            refresh_delay,
            image_index,
            total_files,
            battery_info,
            weatherManager);
    }

    // Finalize and enter sleep - show sleep preparation with delay
    RGB_SET_STATE(SLEEP_PREP); // Show sleep preparation
    delay(2500); // Allow sleep preparation animation to complete
    finalize_and_enter_sleep(battery_info, now, wakeup_reason, refresh_delay);
}

void loop()
{
    // Nothing to do here, the ESP32 will go to sleep after setup
    // The loop is intentionally left empty
    // All processing is done in the setup function
    // The ESP32 will wake up from deep sleep and run the setup function again
    delay(1000); // Just to avoid watchdog reset
}

photo_frame::photo_frame_error_t render_image(fs::File& file,
    const char* original_filename,
    photo_frame::photo_frame_error_t current_error,
    const DateTime& now,
    const refresh_delay_t& refresh_delay,
    uint32_t image_index,
    uint32_t total_files,
    photo_frame::google_drive& drive,
    const photo_frame::battery_info_t& battery_info,
    photo_frame::weather::WeatherManager& weatherManager)
{
    photo_frame::photo_frame_error_t error = current_error;
    const char* img_filename = file.name(); // Used for logging temp filename
    const char* format_filename = original_filename; // Used for format detection

    // Get display capabilities
    bool has_partial_update = photo_frame::renderer::has_partial_update();
    bool has_fast_partial_update = photo_frame::renderer::has_fast_partial_update();

    if (error == photo_frame::error_type::None && !has_partial_update && !has_fast_partial_update) { // if the display does not support partial update
        Serial.println(F("Warning: Display does not support partial update!"));

        int page_index = 0;
        bool rendering_failed = false;
        uint16_t error_code = 0;

        display.setFullWindow();
        display.fillScreen(GxEPD_WHITE);
        display.firstPage();
        do {
            Serial.print(F("Drawing page: "));
            Serial.println(page_index);

            /**
             * Runtime File Format Detection and Rendering
             *
             * The system automatically detects file format based on extension (.bin or .bmp)
             * and selects the appropriate rendering engine. This replaces the old compile-time
             * EPD_USE_BINARY_FILE flag system, allowing mixed file formats in the same folder.
             *
             * - Binary files (.bin): Use optimized binary renderer for fast e-paper rendering
             * - Bitmap files (.bmp): Use standard bitmap renderer for compatibility
             */
            if (photo_frame::io_utils::is_binary_format(format_filename)) {
                error_code = photo_frame::renderer::draw_binary_from_file(
                    file, original_filename, DISP_WIDTH, DISP_HEIGHT, page_index);
            } else {
                error_code = photo_frame::renderer::draw_bitmap_from_file(
                    file, original_filename, 0, 0, false);
            }

            if (error_code != 0) {
                Serial.println(F("Failed to draw bitmap from file!"));
                rendering_failed = true;
                break; // Exit the page rendering loop immediately
            }

            display.writeFillRect(0, 0, display.width(), 16, GxEPD_WHITE);
            photo_frame::renderer::draw_last_update(now, refresh_delay.refresh_seconds);
            photo_frame::renderer::draw_image_info(
                image_index, total_files, drive.get_last_image_source());
            photo_frame::renderer::draw_battery_status(battery_info);

            if (!battery_info.is_critical()) {
                photo_frame::weather::WeatherData current_weather = weatherManager.get_current_weather();
                rect_t box = photo_frame::renderer::get_weather_info_rect();
                photo_frame::renderer::draw_weather_info(current_weather, box);
            }

            page_index++;
        } while (display.nextPage()); // Clear the display

        // Handle rendering errors in a separate loop to prevent cutoff
        if (rendering_failed) {
            display.firstPage();
            display.setFullWindow();
            display.fillScreen(GxEPD_WHITE);
            do {
                photo_frame::renderer::draw_error_with_details(
                    TXT_IMAGE_FORMAT_NOT_SUPPORTED, "", img_filename, error_code);

                display.writeFillRect(0, 0, display.width(), 16, GxEPD_WHITE);
                photo_frame::renderer::draw_last_update(now, refresh_delay.refresh_seconds);
                photo_frame::renderer::draw_image_info(
                    image_index, total_files, drive.get_last_image_source());
                photo_frame::renderer::draw_battery_status(battery_info);

                if (!battery_info.is_critical()) {
                    photo_frame::weather::WeatherData current_weather = weatherManager.get_current_weather();
                    rect_t box = photo_frame::renderer::get_weather_info_rect();
                    photo_frame::renderer::draw_weather_info(current_weather, box);
                }
            } while (display.nextPage());
        }

        file.close(); // Close the file after drawing
        cleanup_temp_image_file(); // Clean up temporary LittleFS file

    } else if (error == photo_frame::error_type::None) {
        Serial.println(F("Using partial update mode"));

        /**
         * Runtime File Format Detection for Buffered Rendering
         *
         * For partial update displays, the system uses buffered rendering with automatic
         * format detection. The appropriate buffered renderer is selected based on file
         * extension, maintaining the same flexibility as the full-page rendering mode.
         */
        uint16_t error_code;
        if (photo_frame::io_utils::is_binary_format(format_filename)) {
            error_code = photo_frame::renderer::draw_binary_from_file_buffered(
                file, original_filename, DISP_WIDTH, DISP_HEIGHT);
        } else {
            error_code = photo_frame::renderer::draw_bitmap_from_file_buffered(
                file, original_filename, 0, 0, false);
        }
        file.close(); // Close the file after drawing
        cleanup_temp_image_file(); // Clean up temporary LittleFS file

        if (error_code != 0) {
            Serial.println(F("Failed to draw bitmap from file!"));

            // Separate error display loop to prevent cutoff issues
            display.firstPage();
            do {
                photo_frame::renderer::draw_error_with_details(
                    TXT_IMAGE_FORMAT_NOT_SUPPORTED, "", img_filename, error_code);
            } while (display.nextPage());
        }

        delay(1000); // Wait for the display to update

        display.setPartialWindow(0, 0, display.width(), 16);
        display.firstPage();
        do {
            display.fillScreen(GxEPD_WHITE);
            photo_frame::renderer::draw_last_update(now, refresh_delay.refresh_seconds);
            photo_frame::renderer::draw_image_info(
                image_index, total_files, drive.get_last_image_source());
            photo_frame::renderer::draw_battery_status(battery_info);
        } while (display.nextPage()); // Clear the display

        if (!battery_info.is_critical()) {
            photo_frame::weather::WeatherData current_weather = weatherManager.get_current_weather();
            if (current_weather.is_displayable()) {
                rect_t box = photo_frame::renderer::get_weather_info_rect();

                display.setPartialWindow(box.x, box.y, box.width, box.height);
                display.firstPage();
                do {
                    photo_frame::renderer::draw_weather_info(current_weather, box);
                } while (display.nextPage());
            }
        }
    }

    return error;
}

bool initialize_hardware()
{
    Serial.begin(115200);
    analogReadResolution(12);

    startupTime = millis();

    if (Serial.isConnected()) {
        Serial.println();
        Serial.println(F("Serial connected"));
        delay(1000);
    } else {
        // Wait for serial connection for up to 5 seconds
        Serial.println();
        Serial.println(F("Waiting for serial connection..."));
        unsigned long startWait = millis();
        while (!Serial.isConnected() && (millis() - startWait < 1000)) {
            delay(100);
        }
        if (Serial.isConnected()) {
            Serial.println(F("Serial connected"));
        } else {
            Serial.println(F("Serial not connected - proceeding without serial output"));
        }
    }

    photo_frame::board_utils::blink_builtin_led(1, 900, 100);
    photo_frame::board_utils::disable_built_in_led();

#ifdef LED_PWR_PIN
    pinMode(LED_PWR_PIN, OUTPUT);
    digitalWrite(LED_PWR_PIN, LOW);
#endif // LED_PWR_PIN

    // PSRAM initialization (mandatory)
    Serial.println(F("[PSRAM] Initializing PSRAM..."));

    // Check if PSRAM is already initialized by the framework
    if (psramFound()) {
        Serial.println(F("[PSRAM] PSRAM already initialized by framework"));
        Serial.print(F("[PSRAM] Available PSRAM: "));
        Serial.print(ESP.getPsramSize());
        Serial.println(F(" bytes"));
        Serial.print(F("[PSRAM] Free PSRAM: "));
        Serial.print(ESP.getFreePsram());
        Serial.println(F(" bytes"));
    } else {
        // Try manual initialization if not already done
        esp_err_t ret = esp_spiram_init();
        if (ret != ESP_OK) {
            Serial.print(F("[PSRAM] CRITICAL: Failed to initialize PSRAM: "));
            Serial.println(esp_err_to_name(ret));
            Serial.println(F("[PSRAM] PSRAM is required for this board configuration!"));
            Serial.println(F("[PSRAM] System will likely crash due to memory constraints"));
            // For ProS3(d) and other PSRAM-mandatory boards, we should halt
            #ifdef BOARD_HAS_PSRAM
                ESP.restart(); // Restart to try again
            #endif
        } else {
            Serial.println(F("[PSRAM] PSRAM initialized successfully"));

            // Note: esp_spiram_add_to_heapalloc() is deprecated in newer ESP-IDF
            // PSRAM should be automatically added to heap with CONFIG_SPIRAM_USE_MALLOC
            // which is set in platformio.ini via board_build.arduino.memory_type

            Serial.print(F("[PSRAM] Available PSRAM: "));
            Serial.print(ESP.getPsramSize());
            Serial.println(F(" bytes"));
            Serial.print(F("[PSRAM] Free PSRAM: "));
            Serial.print(ESP.getFreePsram());
            Serial.println(F(" bytes"));
        }
    }

#ifdef RGB_STATUS_ENABLED
    // Initialize RGB status system (FeatherS3 NeoPixel)
    Serial.println(F("[RGB] Initializing RGB status system..."));
    if (!rgbStatus.begin()) {
        Serial.println(F("[RGB] Warning: Failed to initialize RGB status system"));
    } else {
        RGB_SET_STATE(STARTING); // Show startup indication
    }
#else
    Serial.println(F("[RGB] RGB status system disabled"));
#endif // RGB_STATUS_ENABLED

    Serial.println(F("------------------------------"));
    Serial.print(F("Photo Frame "));
    Serial.println(F(FIRMWARE_VERSION_STRING));
    Serial.println(F("------------------------------"));

    photo_frame::board_utils::print_board_stats();

#if DEBUG_MODE
    photo_frame::board_utils::print_board_pins();
#endif // DEBUG_MODE

    return true;
}

photo_frame::photo_frame_error_t setup_battery_and_power(photo_frame::battery_info_t& battery_info,
    esp_sleep_wakeup_cause_t wakeup_reason)
{
    Serial.println(F("--------------------------------------"));
    Serial.println(F("- Reading battery level..."));
    Serial.println(F("--------------------------------------"));

    battery_reader.init();
    battery_info = battery_reader.read();

    // print the battery levels
    Serial.print(F("Battery level: "));
    Serial.print(battery_info.percent);
    Serial.print(F("%, "));
    Serial.print(battery_info.millivolts);
    Serial.print(F(" mV"));

#ifdef DEBUG_BATTERY_READER
    Serial.print(F(", Raw mV: "));
    Serial.print(battery_info.raw_millivolts);
#endif // DEBUG_BATTERY_READER
    Serial.println();

    if (battery_info.is_empty()) {
        Serial.println(F("Battery is empty!"));
#ifdef BATTERY_POWER_SAVING
        unsigned long elapsed = millis() - startupTime;
        Serial.printf("Elapsed seconds since startup: %lu s\n", elapsed / 1000);

        Serial.println(F("Entering deep sleep to preserve battery..."));
        photo_frame::board_utils::enter_deep_sleep(wakeup_reason); // Enter deep sleep mode
                                                                   // Battery too low to continue
#endif // BATTERY_POWER_SAVING
#ifdef RGB_STATUS_ENABLED
        rgbStatus.disable();
        Serial.println(F("[RGB] Disabled RGB LED to conserve battery power"));
#endif // RGB_STATUS_ENABLED
        return photo_frame::error_type::BatteryEmpty;
    } else if (battery_info.is_critical()) {
        Serial.println(F("Battery level is critical!"));
        RGB_SET_STATE_TIMED(BATTERY_LOW, 3000); // Show battery critical warning briefly

        // Disable RGB after warning to save maximum power
        delay(3500);
#ifdef RGB_STATUS_ENABLED
        rgbStatus.disable();
        Serial.println(F("[RGB] Disabled RGB LED to conserve battery power"));
#endif // RGB_STATUS_ENABLED
       // Indicate critical battery error
        return photo_frame::error_type::BatteryLevelCritical;
    }

    // Battery level allows continued operation
    return photo_frame::error_type::None;
}

photo_frame::photo_frame_error_t
setup_time_and_connectivity(const photo_frame::battery_info_t& battery_info, bool is_reset, DateTime& now)
{
    photo_frame::photo_frame_error_t error = photo_frame::error_type::None;

    Serial.println(F("--------------------------------------"));
    Serial.println(F("- Initialize SD card and load configuration..."));
    Serial.println(F("--------------------------------------"));

    RGB_SET_STATE(SD_READING); // Show SD card operations
    error = sdCard.begin();

    // Reduce RGB brightness if battery is low to save power
    if (battery_info.is_low()) {
#ifdef RGB_STATUS_ENABLED
        rgbStatus.setBrightness(32); // Reduce brightness to 50% of normal for low battery
#endif // RGB_STATUS_ENABLED
    }

    // Load unified configuration from SD card
    if (error == photo_frame::error_type::None) {
        Serial.println(F("Loading unified configuration..."));
        error = photo_frame::load_unified_config_with_fallback(sdCard, CONFIG_FILEPATH, systemConfig);

        if (error != photo_frame::error_type::None) {
            Serial.print(F("Failed to load unified configuration: "));
            Serial.println(error.code);
            // Configuration loading failed, but fallback values are loaded
            // Continue with fallback configuration
            error = photo_frame::error_type::None;
        }

        // Validate essential configuration
        if (!systemConfig.wifi.is_valid()) {
            Serial.println(F("WARNING: WiFi configuration is missing or invalid!"));
            Serial.println(F("Please ensure CONFIG_FILEPATH contains valid WiFi credentials"));
            error = photo_frame::error_type::WifiCredentialsNotFound;
        }
    } else {
        Serial.println(F("SD card initialization failed - using fallback configuration"));

        // SD card failed, load fallback configuration and calculate extended sleep
        load_fallback_config(systemConfig);

        // Calculate extended sleep duration (midpoint between min and max refresh intervals)
        // uint32_t fallback_sleep_seconds = systemConfig.get_fallback_sleep_seconds();
        // uint64_t fallback_sleep_microseconds = (uint64_t)fallback_sleep_seconds * MICROSECONDS_IN_SECOND;
        // Serial.print(F("SD card failed, entering extended sleep for "));
        // Serial.print(fallback_sleep_seconds);
        // Serial.println(F(" seconds"));

        // Enter deep sleep immediately with extended duration
        // photo_frame::board_utils::enter_deep_sleep(ESP_SLEEP_WAKEUP_UNDEFINED, fallback_sleep_microseconds);
        return error; // This line won't be reached, but included for completeness
    }

    if (error == photo_frame::error_type::None) {
        Serial.println(F("Initializing WiFi manager with unified configuration..."));
        RGB_SET_STATE(WIFI_CONNECTING); // Show WiFi connecting status

        // Initialize WiFi manager with configuration from unified config
        error = wifiManager.init_with_config(systemConfig.wifi.ssid, systemConfig.wifi.password);

        if (error == photo_frame::error_type::None) {
            // No RTC module - simple NTP-only time fetching
            Serial.println(F("Fetching time from NTP servers..."));
            error = wifiManager.connect();
            if (error == photo_frame::error_type::None) {
                now = photo_frame::rtc_utils::fetch_datetime(wifiManager, is_reset, &error);
            }
        } else {
            Serial.println(F("WiFi initialization failed"));
            RGB_SET_STATE_TIMED(WIFI_FAILED, 2000); // Show WiFi failed status
        }
    }

    if (error == photo_frame::error_type::None) {
        Serial.print(F("Current time is valid: "));
        Serial.println(now.isValid() ? "Yes" : "No");
    } else {
        Serial.print(F("Failed to fetch current time! Error code: "));
        Serial.println(String(error.code));
    }

    return error;
}

photo_frame::photo_frame_error_t
handle_weather_operations(const photo_frame::battery_info_t& battery_info,
    photo_frame::weather::WeatherManager& weatherManager)
{
    photo_frame::photo_frame_error_t error = photo_frame::error_type::None;

    Serial.println(F("--------------------------------------"));
    Serial.println(F("- Handling weather operations..."));
    Serial.println(F("--------------------------------------"));

    // Check if weather operations should be skipped due to battery conservation
    bool batteryConservationMode = battery_info.is_critical();
    if (batteryConservationMode) {
        Serial.print(F("Skipping weather operations due to critical battery level ("));
        Serial.print(battery_info.percent);
        Serial.println(F("%) - preserving power"));
        return photo_frame::error_type::None; // Not an error, just skipped for power conservation
    }

    // Initialize weather manager from unified configuration
    Serial.println(F("Initializing weather manager from unified config..."));
    if (!weatherManager.begin_with_unified_config(systemConfig.weather)) {
        Serial.println(F("Weather manager initialization failed or disabled"));
        return photo_frame::error_type::None; // Weather failure is not critical - continue without
                                              // weather
    }

    // Check if weather update is needed
    if (!weatherManager.is_configured()) {
        Serial.println(F("Weather manager not configured - skipping weather fetch"));
        return photo_frame::error_type::None;
    }

    if (!weatherManager.needs_update(battery_info.percent)) {
        Serial.println(F("Weather update not needed at this time"));
        return photo_frame::error_type::None;
    }

    // Connect to WiFi for weather data fetching
    Serial.println(F("Connecting to WiFi for weather data..."));
    RGB_SET_STATE(WIFI_CONNECTING); // Show WiFi connecting status

    error = wifiManager.connect();
    if (error != photo_frame::error_type::None) {
        Serial.println(F("WiFi connection failed for weather fetch"));
        RGB_SET_STATE_TIMED(WIFI_FAILED, 2000); // Show WiFi failed status
        return error; // Return error but don't make it critical
    }

    // Fetch weather data
    Serial.println(F("Fetching weather data..."));
    RGB_SET_STATE(WEATHER_FETCHING); // Show weather fetching status

    if (weatherManager.fetch_weather()) {
        Serial.println(F("Weather data updated successfully"));
        weatherManager.reset_failures(); // Reset failure count on success
    } else {
        Serial.print(F("Weather fetch failed ("));
        Serial.print(weatherManager.get_failure_count());
        Serial.println(F(" consecutive failures)"));
        // Don't treat weather fetch failure as critical error
    }
    return photo_frame::error_type::None; // Always return success for weather operations
}

photo_frame::photo_frame_error_t
handle_google_drive_operations(bool is_reset,
    fs::File& file,
    String& original_filename,
    uint32_t& image_index,
    uint32_t& total_files,
    const photo_frame::battery_info_t& battery_info)
{
    photo_frame::photo_frame_error_t error = photo_frame::error_type::None;
    photo_frame::photo_frame_error_t tocError = photo_frame::error_type::JsonParseFailed;
    bool write_toc = is_reset;

    Serial.println(F("--------------------------------------"));
    Serial.println(F(" - Find the next image from the SD..."));
    Serial.println(F("--------------------------------------"));

    error = sdCard.begin(); // Initialize the SD card

    if (error == photo_frame::error_type::None) {
#if DEBUG_MODE
        sdCard.print_stats(); // Print SD card statistics
#endif

        // Initialize Google Drive from unified configuration
        Serial.println(F("Initializing Google Drive from unified config..."));
        error = drive.initialize_from_unified_config(systemConfig.google_drive);

        if (error != photo_frame::error_type::None) {
            Serial.print(F("Failed to initialize Google Drive from unified config! Error: "));
            Serial.println(error.code);
        } else {
            Serial.println(F("Google Drive initialized successfully from unified config"));

            // Use Google Drive to download the next image from Google Drive folder
            // The google_drive class handles authentication, caching, and file management
        }

        if (error == photo_frame::error_type::None) {
            // Clean up any temporary files from previous incomplete downloads
            // Only run cleanup once per day to save battery
            bool shouldCleanup = write_toc; // Always cleanup if forced

            if (!shouldCleanup) {
                // Check if we need to run cleanup based on time interval
                auto& prefs = photo_frame::PreferencesHelper::getInstance();
                time_t now = time(NULL);
                time_t lastCleanup = prefs.getLastCleanup();

                if (now - lastCleanup >= CLEANUP_TEMP_FILES_INTERVAL_SECONDS) {
                    shouldCleanup = true;
                    Serial.print(F("Time since last cleanup: "));
                    Serial.print(now - lastCleanup);
                    Serial.println(F(" seconds"));
                } else {
                    Serial.print(F("Skipping cleanup, only "));
                    Serial.print(now - lastCleanup);
                    Serial.print(F(" seconds since last cleanup (need "));
                    Serial.print(CLEANUP_TEMP_FILES_INTERVAL_SECONDS);
                    Serial.println(F(" seconds)"));
                }
            }

            if (shouldCleanup) {
                uint32_t cleanedFiles = drive.cleanup_temporary_files(sdCard, write_toc);
                if (cleanedFiles > 0) {
                    Serial.print(F("Cleaned up "));
                    Serial.print(cleanedFiles);
                    Serial.println(F(" temporary files from previous session"));
                }

                // Update last cleanup time
                auto& prefs = photo_frame::PreferencesHelper::getInstance();
                if (prefs.setLastCleanup(time(NULL))) {
                    Serial.println(F("Updated last cleanup time"));
                } else {
                    Serial.println(F("Failed to save cleanup time to preferences"));
                }
            }

            // Retrieve Table of Contents (with caching)
            // If battery is critical, use cached TOC even if expired to save power
            bool batteryConservationMode = battery_info.is_critical();
            if (batteryConservationMode) {
                Serial.print(F("Battery critical ("));
                Serial.print(battery_info.percent);
                Serial.println(F("%) - using cached TOC to preserve power"));
            }

            if (error == photo_frame::error_type::None) {
                error = drive.create_directories(sdCard);
            }

            if (error == photo_frame::error_type::None) {
                // Retrieve TOC and get file count directly
                // I2C is already shut down before all WiFi operations to prevent ESP32-C6
                // interference
                total_files = drive.retrieve_toc(sdCard, batteryConservationMode);
            } else {
                Serial.println(F("Google Drive not initialized - skipping"));
                total_files = 0;
            }

            if (total_files > 0) {
                Serial.print(F("Total files in Google Drive folder: "));
                Serial.println(total_files);

                photo_frame::google_drive_file selectedFile;

#ifdef GOOGLE_DRIVE_TEST_FILE
                // Use specific test file if defined
                Serial.print(F("Using test file: "));
                Serial.println(GOOGLE_DRIVE_TEST_FILE);
                selectedFile = drive.get_toc_file_by_name(GOOGLE_DRIVE_TEST_FILE, &tocError);
                if (tocError != photo_frame::error_type::None) {
                    Serial.print(F("Test file not found in TOC, falling back to random "
                                   "selection. Error: "));
                    Serial.println(tocError.code);
                    // Fallback to random selection
                    image_index = random(0, drive.get_toc_file_count());
                    selectedFile = drive.get_toc_file_by_index(image_index, &tocError);
                }
#else
                // Generate a random index to start from
                image_index = random(0, drive.get_toc_file_count(sdCard));

                // Get the specific file by index efficiently
                selectedFile = drive.get_toc_file_by_index(sdCard, image_index, &tocError);
#endif // GOOGLE_DRIVE_TEST_FILE

                if (tocError == photo_frame::error_type::None && selectedFile.id.length() > 0) {
                    Serial.print(F("Selected file: "));
                    Serial.println(selectedFile.name);

                    // Always download to SD card first for better caching
                    String localFilePath = drive.get_cached_file_path(selectedFile.name);
                    if (sdCard.file_exists(localFilePath.c_str()) && sdCard.get_file_size(localFilePath.c_str()) > 0) {
                        Serial.println(F("File already exists in SD card, using cached version"));
                        file = sdCard.open(localFilePath.c_str(), FILE_READ);
                        drive.set_last_image_source(photo_frame::IMAGE_SOURCE_LOCAL_CACHE);
                    } else {
                        // Check battery level before downloading file
                        if (batteryConservationMode) {
                            Serial.print(
                                F("Skipping file download due to critical battery level ("));
                            Serial.print(battery_info.percent);
                            Serial.println(F("%) - will use cached files if available"));
                            error = photo_frame::error_type::BatteryLevelCritical;
                        } else {
                            // Download the selected file to SD card
                            file = drive.download_file(sdCard, selectedFile, &error);
                            // Note: image source is set to IMAGE_SOURCE_CLOUD inside
                            // download_file
                        }
                    }

                    // Validate image file before copying to LittleFS
                    if (error == photo_frame::error_type::None && file) {
                        const char* filename = file.name();
                        String filePath = String(filename);
                        original_filename = String(filename); // Store original filename for format detection

                        Serial.println(F("🛡️ Validating downloaded image file..."));
                        auto validationError = photo_frame::io_utils::validate_image_file(
                            file, filename, DISP_WIDTH, DISP_HEIGHT);

                        if (validationError != photo_frame::error_type::None) {
                            Serial.print(F("❌ Image validation FAILED: "));
                            Serial.println(validationError.message);

                            // Close file before deletion
                            file.close();

                            // Delete corrupted file from SD card
                            if (sdCard.file_exists(filePath.c_str())) {
                                Serial.print(F("🗑️ Deleting corrupted file from SD card: "));
                                Serial.println(filePath);
                                if (sdCard.remove(filePath.c_str())) {
                                    Serial.println(F("✅ Corrupted file successfully deleted"));
                                } else {
                                    Serial.println(F("❌ Failed to delete corrupted file"));
                                }
                            }

                            error = validationError;
                        } else {
                            Serial.println(F("✅ Image validation PASSED"));

                            // Now copy validated file to LittleFS
                            String littlefsPath = LITTLEFS_TEMP_IMAGE_FILE;
                            error = photo_frame::io_utils::copy_sd_to_littlefs(
                                file, littlefsPath, DISP_WIDTH, DISP_HEIGHT);

                            // Always close the SD card file after copy attempt
                            file.close();

                            if (error == photo_frame::error_type::None) {
                                // Copy successful - shutdown SD card and open LittleFS file
                                Serial.println(
                                    F("Shutting down SD card after successful copy to LittleFS"));

                                // Open the LittleFS file for reading
                                file = LittleFS.open(littlefsPath.c_str(), FILE_READ);
                                if (!file) {
                                    Serial.println(F("Failed to open LittleFS file for reading"));
                                    error = photo_frame::error_type::LittleFSFileOpenFailed;
                                }
                            }
                        }
                    }

                    // Always close the SD card after file operations are complete to avoid
                    // conflicts
                    sdCard.end();
                } else {
                    Serial.print(F("Failed to get file by index. Error code: "));
                    Serial.println(tocError.code);
                    error = tocError;
                }

                if (error == photo_frame::error_type::None && file) {
                    Serial.println(F("File downloaded and ready for display!"));
                } else {
                    Serial.print(F("Failed to download file from Google Drive! Error code: "));
                    Serial.println(error.code);
                }
            } else {
                // Check if there was an error during TOC retrieval (e.g., access token failure)
                photo_frame::photo_frame_error_t lastDriveError = drive.get_last_error();
                if (lastDriveError != photo_frame::error_type::None) {
                    Serial.print(F("Failed to retrieve TOC. Error code: "));
                    Serial.println(lastDriveError.code);
                    error = lastDriveError;
                } else if (tocError != photo_frame::error_type::None) {
                    Serial.print(F("Failed to read TOC file count. Error code: "));
                    Serial.println(tocError.code);
                    error = tocError;
                } else {
                    Serial.println(F("No files found in Google Drive folder!"));
                    error = photo_frame::error_type::NoImagesFound;
                }
            }
        }
    } else {
        Serial.print(F("Failed to initialize SD card. Error code: "));
        Serial.println(error.code);
    }

    return error;
}

photo_frame::photo_frame_error_t
process_image_file(fs::File& file,
    const char* original_filename,
    photo_frame::photo_frame_error_t current_error,
    const DateTime& now,
    const refresh_delay_t& refresh_delay,
    uint32_t image_index,
    uint32_t total_files,
    const photo_frame::battery_info_t& battery_info,
    photo_frame::weather::WeatherManager& weatherManager)
{
    photo_frame::photo_frame_error_t error = current_error;

    if (error == photo_frame::error_type::None && file) {
        // Image was already validated before copying to LittleFS, proceed directly to rendering
        Serial.println(F("Rendering validated image file..."));
        error = render_image(file,
            original_filename,
            error,
            now,
            refresh_delay,
            image_index,
            total_files,
            drive,
            battery_info,
            weatherManager);
    }

    if (file) {
        file.close(); // Close the file after drawing
    }

    return error;
}

void finalize_and_enter_sleep(photo_frame::battery_info_t& battery_info,
    DateTime& now,
    esp_sleep_wakeup_cause_t wakeup_reason,
    const refresh_delay_t& refresh_delay)
{
#ifdef RGB_STATUS_ENABLED
    // Shutdown RGB status system to save power during sleep
    Serial.println(F("[RGB] Shutting down RGB status system for sleep"));
    rgbStatus.end(); // Properly shutdown NeoPixel and FreeRTOS task
#endif // RGB_STATUS_ENABLED

    delay(500);
    photo_frame::renderer::power_off();

    // now go to sleep using the pre-calculated refresh delay
    if (!battery_info.is_critical() && refresh_delay.refresh_microseconds > MICROSECONDS_IN_SECOND) {
        Serial.print(F("Going to sleep for "));
        Serial.print(refresh_delay.refresh_seconds, DEC);
        Serial.print(F(" seconds ("));
        Serial.print((unsigned long)(refresh_delay.refresh_microseconds / 1000000ULL));
        Serial.println(F(" seconds from microseconds)"));
    } else {
        if (battery_info.is_critical()) {
            Serial.println(F("Battery is critical, entering indefinite sleep"));
        } else {
            Serial.println(F("Sleep time too short or invalid, entering default sleep"));
        }
    }

    unsigned long elapsed = millis() - startupTime;
    Serial.printf("Elapsed seconds since startup: %lu s\n", elapsed / 1000);
    photo_frame::board_utils::enter_deep_sleep(wakeup_reason, refresh_delay.refresh_microseconds);
}

/**
 * @brief Calculate the wakeup delay based on the battery level and current time.
 *
 * @param battery_info The battery information structure.
 * @param now The current time.
 * @return The calculated wakeup delay.
 */
refresh_delay_t calculate_wakeup_delay(photo_frame::battery_info_t& battery_info, DateTime& now)
{
    refresh_delay_t refresh_delay = { 0, 0 };

    if (battery_info.is_critical()) {
        Serial.println(F("Battery is critical, using low battery refresh interval"));
        refresh_delay.refresh_seconds = REFRESH_INTERVAL_SECONDS_CRITICAL_BATTERY;
        refresh_delay.refresh_microseconds = (uint64_t)refresh_delay.refresh_seconds * MICROSECONDS_IN_SECOND;
        return refresh_delay;
    }

    if (!now.isValid()) {
        Serial.println(F("Time is invalid, using minimum refresh interval as fallback"));
        refresh_delay.refresh_seconds = REFRESH_MIN_INTERVAL_SECONDS;
        refresh_delay.refresh_microseconds = (uint64_t)refresh_delay.refresh_seconds * MICROSECONDS_IN_SECOND;
        return refresh_delay;
    }

    if (true) { // Changed from the original condition to always execute
        refresh_delay.refresh_seconds = photo_frame::board_utils::read_refresh_seconds(battery_info.is_low());

        Serial.print(F("Refresh seconds read from potentiometer: "));
        Serial.println(refresh_delay.refresh_seconds);

        // add the refresh time to the current time
        // Update the current time with the refresh interval
        DateTime nextRefresh = now + TimeSpan(refresh_delay.refresh_seconds);

        // check if the next refresh time is after DAY_END_HOUR
        DateTime dayEnd = DateTime(now.year(), now.month(), now.day(), DAY_END_HOUR, 0, 0);

        if (nextRefresh > dayEnd) {
            Serial.println(F("Next refresh time is after DAY_END_HOUR"));
            // Check if we're currently in the inactive period (before DAY_START_HOUR)
            if (now.hour() < DAY_START_HOUR) {
                // We're in early morning hours, schedule for DAY_START_HOUR today
                nextRefresh = DateTime(now.year(), now.month(), now.day(), DAY_START_HOUR, 0, 0);
            } else {
                // We're past DAY_END_HOUR, schedule for DAY_START_HOUR tomorrow
                DateTime tomorrow = now + TimeSpan(1, 0, 0, 0); // Add 1 day safely
                nextRefresh = DateTime(
                    tomorrow.year(), tomorrow.month(), tomorrow.day(), DAY_START_HOUR, 0, 0);
            }
            refresh_delay.refresh_seconds = nextRefresh.unixtime() - now.unixtime();
        }

        Serial.print(F("Next refresh time: "));
        Serial.println(nextRefresh.timestamp(DateTime::TIMESTAMP_FULL));

        // Convert seconds to microseconds for deep sleep with overflow protection
        // ESP32 max sleep time is ~18 hours (281474976710655 microseconds)
        // Practical limit defined in config.h

        if (refresh_delay.refresh_seconds <= 0) {
            Serial.println(
                F("Warning: Invalid refresh interval, using minimum interval as fallback"));
            refresh_delay.refresh_seconds = REFRESH_MIN_INTERVAL_SECONDS;
            refresh_delay.refresh_microseconds = (uint64_t)refresh_delay.refresh_seconds * MICROSECONDS_IN_SECOND;
        } else if (refresh_delay.refresh_seconds > MAX_DEEP_SLEEP_SECONDS) {
            refresh_delay.refresh_microseconds = (uint64_t)MAX_DEEP_SLEEP_SECONDS * MICROSECONDS_IN_SECOND;
            Serial.print(F("Warning: Refresh interval capped to "));
            Serial.print(MAX_DEEP_SLEEP_SECONDS);
            Serial.println(F(" seconds to prevent overflow"));
        } else {
            // Safe calculation using 64-bit arithmetic
            refresh_delay.refresh_microseconds = (uint64_t)refresh_delay.refresh_seconds * MICROSECONDS_IN_SECOND;
        }

        char humanReadable[64];
        photo_frame::string_utils::seconds_to_human(
            humanReadable, sizeof(humanReadable), refresh_delay.refresh_seconds);

        Serial.print(F("Refresh interval in: "));
        Serial.println(humanReadable);
    }

    // Final validation - ensure we never return 0 microseconds
    if (refresh_delay.refresh_microseconds == 0) {
        Serial.println(F("CRITICAL: refresh_microseconds is 0, forcing minimum interval"));
        refresh_delay.refresh_seconds = REFRESH_MIN_INTERVAL_SECONDS;
        refresh_delay.refresh_microseconds = (uint64_t)refresh_delay.refresh_seconds * MICROSECONDS_IN_SECOND;
    }

    Serial.print(F("Final refresh delay: "));
    Serial.print(refresh_delay.refresh_seconds);
    Serial.print(F(" seconds ("));
    Serial.print((unsigned long)(refresh_delay.refresh_microseconds / 1000000ULL));
    Serial.println(F(" microseconds)"));

    return refresh_delay;
}