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
#include <Preferences.h>

#include "battery.h"
#include "board_util.h"
#include "config.h"
#include "errors.h"
#include "io_utils.h"
#include "renderer.h"
#include "rtc_util.h"
#include "sd_card.h"
#include "spi_manager.h"
#include "string_utils.h"
#include "wifi_manager.h"
#ifdef USE_WEATHER
#include "weather.h"
#endif

#include <assets/icons/icons.h>

#include "google_drive.h"
#include "google_drive_client.h"

// Google Drive instance (initialized from JSON config)
photo_frame::google_drive drive;

// Helper function to cleanup temporary files from LittleFS
void cleanup_temp_image_file() {
    photo_frame::spi_manager::SPIManager::cleanup_temp_files("*.tmp");
}

#ifndef USE_SENSOR_MAX1704X
photo_frame::battery_reader battery_reader(BATTERY_PIN,
                                           BATTERY_RESISTORS_RATIO,
                                           BATTERY_NUM_READINGS,
                                           BATTERY_DELAY_BETWEEN_READINGS);
#else  // USE_SENSOR_MAX1704X
photo_frame::battery_reader battery_reader;
#endif // USE_SENSOR_MAX1704X

photo_frame::sd_card sdCard(SD_CS_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_SCK_PIN);
photo_frame::wifi_manager wifiManager;
#ifdef USE_WEATHER
photo_frame::weather::WeatherManager weatherManager;
#endif
Preferences prefs;

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
 * @return true if battery level allows continued operation, false if critical
 */
bool setup_battery_and_power(photo_frame::battery_info_t& battery_info,
                             esp_sleep_wakeup_cause_t wakeup_reason);

/**
 * @brief Setup time synchronization via WiFi and NTP, update weather if needed
 * @param battery_info Battery information for power conservation decisions
 * @param now Reference to store current DateTime
 * @param weatherManager Weather manager instance for updates
 * @return Error state after time/WiFi operations
 */
photo_frame::photo_frame_error_t
setup_time_and_connectivity(const photo_frame::battery_info_t& battery_info,
                            DateTime& now
#ifdef USE_WEATHER
                            ,
                            photo_frame::weather::WeatherManager& weatherManager
#endif
);

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
 * @param original_filename Original filename from SD card (used for format detection and error reporting)
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
                   const photo_frame::battery_info_t& battery_info
#ifdef USE_WEATHER
                   ,
                   photo_frame::weather::WeatherManager& weatherManager
#endif
);

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
 * @param original_filename Original filename from SD card (used for format detection and error reporting)
 * @param current_error Current error state
 * @param now Current DateTime for info display
 * @param refresh_delay Refresh delay info
 * @param image_index Current image index
 * @param total_files Total number of files
 * @param drive Google Drive instance
 * @param battery_info Battery information
 * @param weatherManager Weather manager instance (if USE_WEATHER is defined)
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
                                              const photo_frame::battery_info_t& battery_info
#ifdef USE_WEATHER
                                              ,
                                              photo_frame::weather::WeatherManager& weatherManager
#endif
);

void setup() {
    // Initialize hardware components
    if (!initialize_hardware()) {
        return;
    }

    // Determine wakeup reason and setup basic state
    esp_sleep_wakeup_cause_t wakeup_reason = photo_frame::board_utils::get_wakeup_reason();
    bool is_reset                          = wakeup_reason == ESP_SLEEP_WAKEUP_UNDEFINED;

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

    // Setup battery and power management
    photo_frame::battery_info_t battery_info;
    if (!setup_battery_and_power(battery_info, wakeup_reason)) {
        return;
    }

    // Setup time synchronization and connectivity
    DateTime now = DateTime((uint32_t)0);
    auto error   = setup_time_and_connectivity(battery_info,
                                             now
#ifdef USE_WEATHER
                                             ,
                                             weatherManager
#endif
    );

    // Handle Google Drive operations
    fs::File file;
    uint32_t image_index = 0, total_files = 0;
    String original_filename; // Store original filename for format detection
    error = handle_google_drive_operations(is_reset, file, original_filename, image_index, total_files, battery_info);

    wifiManager.disconnect(); // Disconnect from WiFi to save power
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
                                   battery_info
#ifdef USE_WEATHER
                                   ,
                                   weatherManager
#endif
        );
    }

    // Finalize and enter sleep
    finalize_and_enter_sleep(battery_info, now, wakeup_reason, refresh_delay);
}

void loop() {
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
                                              const photo_frame::battery_info_t& battery_info
#ifdef USE_WEATHER
                                              ,
                                              photo_frame::weather::WeatherManager& weatherManager
#endif
) {
    photo_frame::photo_frame_error_t error = current_error;
    const char* img_filename               = file.name();  // Used for logging temp filename
    const char* format_filename            = original_filename; // Used for format detection

    // Get display capabilities
    bool has_partial_update      = photo_frame::renderer::has_partial_update();
    bool has_fast_partial_update = photo_frame::renderer::has_fast_partial_update();

    if (error == photo_frame::error_type::None && !has_partial_update &&
        !has_fast_partial_update) { // if the display does not support partial update
        Serial.println(F("Warning: Display does not support partial update!"));

        int page_index        = 0;
        bool rendering_failed = false;
        uint16_t error_code   = 0;

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
                error_code = photo_frame::renderer::draw_bitmap_from_file(file, original_filename, 0, 0, false);
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

#ifdef USE_WEATHER
            if (!battery_info.is_critical()) {
                photo_frame::weather::WeatherData current_weather =
                    weatherManager.get_current_weather();
                rect_t box = photo_frame::renderer::get_weather_info_rect();
                photo_frame::renderer::draw_weather_info(current_weather, box);
            }
#endif // USE_WEATHER

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

#ifdef USE_WEATHER
                if (!battery_info.is_critical()) {
                    photo_frame::weather::WeatherData current_weather =
                        weatherManager.get_current_weather();
                    rect_t box = photo_frame::renderer::get_weather_info_rect();
                    photo_frame::renderer::draw_weather_info(current_weather, box);
                }
#endif // USE_WEATHER
            } while (display.nextPage());
        }

        file.close();              // Close the file after drawing
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
            error_code = photo_frame::renderer::draw_bitmap_from_file_buffered(file, original_filename, 0, 0, false);
        }
        file.close();              // Close the file after drawing
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

#ifdef USE_WEATHER
        if (!battery_info.is_critical()) {
            photo_frame::weather::WeatherData current_weather =
                weatherManager.get_current_weather();
            if (current_weather.is_displayable()) {
                rect_t box = photo_frame::renderer::get_weather_info_rect();

                display.setPartialWindow(box.x, box.y, box.width, box.height);
                display.firstPage();
                do {
                    photo_frame::renderer::draw_weather_info(current_weather, box);
                } while (display.nextPage());
            }
        }
#endif // USE_WEATHER
    }

    return error;
}

bool initialize_hardware() {
    Serial.begin(115200);
    analogReadResolution(12);

    startupTime = millis();

#if DEBUG_MODE
    delay(1000);
#endif

#if HAS_RGB_LED
    Serial.println(F("Initializing RGB LED..."));
    pinMode(LED_BLUE, OUTPUT);
    pinMode(LED_GREEN, OUTPUT);
    pinMode(LED_RED, OUTPUT);
#endif // HAS_RGB_LED

    photo_frame::board_utils::blink_builtin_led(1, 900, 100);
    photo_frame::board_utils::disable_built_in_led();
    photo_frame::board_utils::disable_rgb_led(); // Disable RGB LED to save power

    Serial.println(F("------------------------------"));
    Serial.print(F("Photo Frame "));
    Serial.println(F(FIRMWARE_VERSION_STRING));
    Serial.println(F("------------------------------"));

    photo_frame::board_utils::print_board_stats();
    photo_frame::board_utils::print_board_pins();

    return true;
}

bool setup_battery_and_power(photo_frame::battery_info_t& battery_info,
                             esp_sleep_wakeup_cause_t wakeup_reason) {
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
        return false;                                              // Battery too low to continue
#endif                                                             // BATTERY_POWER_SAVING
    } else if (battery_info.is_critical()) {
        Serial.println(F("Battery level is critical!"));
    }

    return true; // Battery level allows continued operation
}

photo_frame::photo_frame_error_t
setup_time_and_connectivity(const photo_frame::battery_info_t& battery_info,
                            DateTime& now
#ifdef USE_WEATHER
                            ,
                            photo_frame::weather::WeatherManager& weatherManager
#endif
) {
    photo_frame::photo_frame_error_t error = photo_frame::error_type::None;

    Serial.println(F("--------------------------------------"));
    Serial.println(F("- Initialize SD card and fetch time..."));
    Serial.println(F("--------------------------------------"));

    if (!battery_info.is_critical()) {
        error = sdCard.begin();
        if (error == photo_frame::error_type::None) {
            Serial.println(F("Initializing WiFi manager..."));
            error = wifiManager.init(WIFI_FILENAME, sdCard);

#ifdef USE_WEATHER
            // Initialize weather manager (loads config from SD card)
            Serial.println(F("Initializing weather manager..."));
            if (!weatherManager.begin()) {
                Serial.println(F("Weather manager initialization failed or disabled"));
                // Weather failure is not critical - continue without weather
            }
#endif

            if (error == photo_frame::error_type::None) {
                // No RTC module - simple NTP-only time fetching
                Serial.println(F("Fetching time from NTP servers..."));
                error = wifiManager.connect();
                if (error == photo_frame::error_type::None) {
                    now = photo_frame::rtc_utils::fetch_datetime(wifiManager, false, &error);

#ifdef USE_WEATHER
                    // Piggyback weather fetch on existing WiFi session (with power management)
                    bool batteryConservationMode = battery_info.is_critical();
                    if (weatherManager.is_configured() && !batteryConservationMode &&
                        weatherManager.needs_update(battery_info.percent)) {
                        Serial.println(F("Fetching weather data during WiFi session..."));
                        if (weatherManager.fetch_weather()) {
                            Serial.println(F("Weather data updated successfully"));
                            weatherManager.reset_failures(); // Reset failure count on success
                        } else {
                            Serial.print(F("Weather fetch failed ("));
                            Serial.print(weatherManager.get_failure_count());
                            Serial.println(F(" consecutive failures)"));
                        }
                    } else if (weatherManager.is_configured()) {
                        if (batteryConservationMode) {
                            Serial.print(
                                F("Skipping weather update due to critical battery level ("));
                            Serial.print(battery_info.percent);
                            Serial.println(F("%) - preserving power"));
                        } else {
                            Serial.println(F("Weather update not needed at this time"));
                        }
                    }
#endif
                }
            } else {
                Serial.println(F("WiFi initialization failed"));
            }
        } else {
            Serial.println(F("Skipping datetime fetch due to SD card error!"));
        }

        if (error == photo_frame::error_type::None) {
            Serial.print(F("Current time is valid: "));
            Serial.println(now.isValid() ? "Yes" : "No");
        } else {
            Serial.print(F("Failed to fetch current time! Error code: "));
            Serial.println(String(error.code));
        }
    } else {
        Serial.println(F("skipped"));
    }

    return error;
}

photo_frame::photo_frame_error_t
handle_google_drive_operations(bool is_reset,
                               fs::File& file,
                               String& original_filename,
                               uint32_t& image_index,
                               uint32_t& total_files,
                               const photo_frame::battery_info_t& battery_info) {
    photo_frame::photo_frame_error_t error    = photo_frame::error_type::None;
    photo_frame::photo_frame_error_t tocError = photo_frame::error_type::JsonParseFailed;
    bool write_toc                            = is_reset;

    Serial.println(F("--------------------------------------"));
    Serial.println(F(" - Find the next image from the SD..."));
    Serial.println(F("--------------------------------------"));

    error = sdCard.begin(); // Initialize the SD card

    if (error == photo_frame::error_type::None) {
#if DEBUG_MODE
        sdCard.print_stats(); // Print SD card statistics
#endif

        // Initialize Google Drive from JSON configuration file
        Serial.println(F("Initializing Google Drive from JSON config..."));
        error = drive.initialize_from_json(sdCard, GOOGLE_DRIVE_CONFIG_FILEPATH);

        if (error != photo_frame::error_type::None) {
            Serial.print(F("Failed to initialize Google Drive from JSON config! Error: "));
            Serial.println(error.code);
        } else {
            Serial.println(F("Google Drive initialized successfully from JSON config"));

            // Use Google Drive to download the next image from Google Drive folder
            // The google_drive class handles authentication, caching, and file management
        }

        if (error == photo_frame::error_type::None) {
            // Clean up any temporary files from previous incomplete downloads
            // Only run cleanup once per day to save battery
            bool shouldCleanup = write_toc; // Always cleanup if forced

            if (!shouldCleanup) {
                // Check if we need to run cleanup based on time interval
                if (!prefs.begin(PREFS_NAMESPACE, false)) {
                    Serial.println(F("Failed to open preferences for cleanup check!"));
                    shouldCleanup = true; // Default to cleanup if can't check
                } else {
                    time_t now         = time(NULL);
                    time_t lastCleanup = prefs.getULong("last_cleanup", 0);

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
                    prefs.end();
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
                if (!prefs.begin(PREFS_NAMESPACE, false)) {
                    Serial.println(F("Failed to open preferences to save cleanup time!"));
                } else {
                    prefs.putULong("last_cleanup", time(NULL));
                    prefs.end();
                    Serial.println(F("Updated last cleanup time"));
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
                    image_index  = random(0, drive.get_toc_file_count());
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
                    if (sdCard.file_exists(localFilePath.c_str()) &&
                        sdCard.get_file_size(localFilePath.c_str()) > 0) {
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
                        String filePath      = String(filename);
                        original_filename    = String(filename); // Store original filename for format detection

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
                            error               = photo_frame::io_utils::copy_sd_to_littlefs(
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
                if (tocError != photo_frame::error_type::None) {
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
                   const photo_frame::battery_info_t& battery_info
#ifdef USE_WEATHER
                   ,
                   photo_frame::weather::WeatherManager& weatherManager
#endif
) {
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
                             battery_info
#ifdef USE_WEATHER
                             ,
                             weatherManager
#endif
        );
    }

    if (file) {
        file.close(); // Close the file after drawing
    }

    return error;
}

void finalize_and_enter_sleep(photo_frame::battery_info_t& battery_info,
                              DateTime& now,
                              esp_sleep_wakeup_cause_t wakeup_reason,
                              const refresh_delay_t& refresh_delay) {
    photo_frame::board_utils::disable_rgb_led();
    delay(1000);
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
refresh_delay_t calculate_wakeup_delay(photo_frame::battery_info_t& battery_info, DateTime& now) {
    refresh_delay_t refresh_delay = {0, 0};

    if (battery_info.is_critical()) {
        Serial.println(F("Battery is critical, using low battery refresh interval"));
        refresh_delay.refresh_seconds = REFRESH_INTERVAL_SECONDS_LOW_BATTERY;
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
        refresh_delay.refresh_seconds =
            photo_frame::board_utils::read_refresh_seconds(battery_info.is_low());

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
                nextRefresh       = DateTime(
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
            Serial.println(F("Warning: Invalid refresh interval, using minimum interval as fallback"));
            refresh_delay.refresh_seconds = REFRESH_MIN_INTERVAL_SECONDS;
            refresh_delay.refresh_microseconds = (uint64_t)refresh_delay.refresh_seconds * MICROSECONDS_IN_SECOND;
        } else if (refresh_delay.refresh_seconds > MAX_DEEP_SLEEP_SECONDS) {
            refresh_delay.refresh_microseconds =
                (uint64_t)MAX_DEEP_SLEEP_SECONDS * MICROSECONDS_IN_SECOND;
            Serial.print(F("Warning: Refresh interval capped to "));
            Serial.print(MAX_DEEP_SLEEP_SECONDS);
            Serial.println(F(" seconds to prevent overflow"));
        } else {
            // Safe calculation using 64-bit arithmetic
            refresh_delay.refresh_microseconds =
                (uint64_t)refresh_delay.refresh_seconds * MICROSECONDS_IN_SECOND;
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