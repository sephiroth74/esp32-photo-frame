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
#include <Preferences.h>
#include <Wire.h>

#include "battery.h"
#include "board_util.h"
#include "config.h"
#include "errors.h"
#include "renderer.h"
#include "rtc_util.h"
#include "sd_card.h"
#include "wifi_manager.h"
#include "string_utils.h"

#include <assets/icons/icons.h>

#if defined(USE_GOOGLE_DRIVE)
#include "google_drive_client.h"
#include "google_drive.h"

photo_frame::google_drive_client_config client_config = {
    .serviceAccountEmail = GOOGLE_DRIVE_SERVICE_ACCOUNT_EMAIL,
    .privateKeyPem = GOOGLE_DRIVE_PRIVATE_KEY_PEM,
    .clientId = GOOGLE_DRIVE_CLIENT_ID
};

photo_frame::google_drive_config drive_config = {
    .tocMaxAge = GOOGLE_DRIVE_TOC_MAX_AGE,
    .localPath = GOOGLE_DRIVE_LOCAL_PATH,
    .localTocFilename = GOOGLE_DRIVE_TOC_FILENAME,
    .driveFolderId = GOOGLE_DRIVE_FOLDER_ID
};

photo_frame::google_drive_client driveClient = photo_frame::google_drive_client(client_config);
photo_frame::google_drive drive = photo_frame::google_drive(driveClient, drive_config);
#endif

#ifndef USE_SENSOR_MAX1704X
photo_frame::battery_reader battery_reader(BATTERY_PIN,
    BATTERY_RESISTORS_RATIO,
    BATTERY_NUM_READINGS,
    BATTERY_DELAY_BETWEEN_READINGS);
#else // USE_SENSOR_MAX1704X
photo_frame::battery_reader battery_reader;
#endif // USE_SENSOR_MAX1704X

photo_frame::sd_card sdCard(SD_CS_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_SCK_PIN);
photo_frame::wifi_manager wifiManager;
Preferences prefs;

unsigned long startupTime = 0;

typedef struct {
    long refresh_seconds;
    uint64_t refresh_microseconds;
} refresh_delay_t;

refresh_delay_t calculate_wakeup_delay(photo_frame::battery_info_t& battery_info, DateTime& now);

void setup()
{
    Serial.begin(115200);
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

    photo_frame::board_utils::blink_builtin_led(1, 300, 300);
    photo_frame::board_utils::disable_built_in_led();
    photo_frame::board_utils::toggle_rgb_led(false, true, false); // Set RGB LED to green to indicate initialization
    photo_frame::board_utils::disable_rgb_led(); // Disable RGB LED to save power

    Serial.println(F("------------------------------"));
    Serial.println(F("Photo Frame v0.1.0"));
    Serial.println(F("------------------------------"));

    esp_sleep_wakeup_cause_t wakeup_reason = photo_frame::board_utils::get_wakeup_reason();
    String wakeup_reason_string = photo_frame::board_utils::get_wakeup_reason_string(wakeup_reason);

    // if the wakeup reason is undefined, it means the device is starting up after
    // a reset
    bool is_reset = wakeup_reason == ESP_SLEEP_WAKEUP_UNDEFINED;
    bool reset_rtc = is_reset && RESET_INVALIDATES_DATE_TIME;

    // if in reset state, we will write the TOC (Table of Contents) to the SD
    bool write_toc = false; //is_reset;

    uint32_t image_index = 0; // Index of the image to display
    uint32_t total_files = 0; // Total number of image files on the SD card

    photo_frame::board_utils::print_board_stats();
    photo_frame::board_utils::print_board_pins();

#if DEBUG_MODE
    Serial.print(F("Wakeup reason: "));
    Serial.print(wakeup_reason_string);
    Serial.print(F(" ("));
    Serial.print(wakeup_reason);
    Serial.println(F(")"));

    Serial.print(F("Is reset: "));
    Serial.println(is_reset ? "Yes" : "No");
    delay(2000);
#endif // DEBUG_MODE

    photo_frame::photo_frame_error_t error = photo_frame::error_type::None;
    fs::File file;
    DateTime now = DateTime((uint32_t)0); // Initialize DateTime with 0 timestamp

    // -------------------------------------------------------------
    // - Read the battery level
    // -------------------------------------------------------------
    Serial.println(F("--------------------------------------"));
    Serial.println(F("- Reading battery level..."));
    Serial.println(F("--------------------------------------"));

    battery_reader.init();
    photo_frame::battery_info_t battery_info = battery_reader.read();

    if (battery_info.is_empty()) {
        Serial.println(F("Battery is empty!"));
#ifdef BATTERY_POWER_SAVING
        unsigned long elapsed = millis() - startupTime;
        Serial.printf("Elapsed seconds since startup: %lu s\n", elapsed / 1000);

        Serial.println(F("Entering deep sleep to preserve battery..."));
        photo_frame::board_utils::enter_deep_sleep(wakeup_reason); // Enter deep sleep mode
        return; // Exit if battery is empty
#endif // BATTERY_POWER_SAVING
    } else if (battery_info.is_critical()) {
        Serial.println(F("Battery level is critical!"));
    }

    // --------------------------------------------------------------
    // - Initialize the SD card and fetch the current time
    // --------------------------------------------------------------
    Serial.println(F("--------------------------------------"));
    Serial.println(F(" - Fetching current time..."));
    Serial.println(F("--------------------------------------"));

    if (error != photo_frame::error_type::BatteryLevelCritical && error != photo_frame::error_type::BatteryEmpty) {
        error = sdCard.begin(); // Initialize the SD card
        if (error == photo_frame::error_type::None) {
            Serial.println(F("Initializing WiFi manager..."));
            error = wifiManager.init(WIFI_FILENAME, sdCard);
            if (error == photo_frame::error_type::None) {
#ifdef USE_GOOGLE_DRIVE
                error = wifiManager.connect();
                if (error == photo_frame::error_type::None) {
                    now = photo_frame::rtc_utils::fetch_datetime(wifiManager, reset_rtc, &error);
                }
#else
                now = photo_frame::rtc_utils::fetch_datetime(wifiManager, reset_rtc, &error);
#endif // USE_GOOGLE_DRIVE
            } else {
                Serial.println(F("WiFi initialization failed"));
#if !defined(USE_GOOGLE_DRIVE)
                Serial.println(F("Continuing without WiFi"));
                error = photo_frame::error_type::None;
#endif
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

    // -------------------------------------------------------------
    // - Calculate the refresh rate based on the battery level
    // -------------------------------------------------------------

    Serial.println(F("--------------------------------------"));
    Serial.println(F("- Calculating refresh rate"));
    Serial.println(F("--------------------------------------"));
    refresh_delay_t refresh_delay = calculate_wakeup_delay(battery_info, now);

    // -------------------------------------------------------------
    // - Read the next image file from the SD card or Google Drive
    // -------------------------------------------------------------

    Serial.println(F("--------------------------------------"));
    Serial.println(F(" - Find the next image from the SD..."));
    Serial.println(F("--------------------------------------"));

    if (error == photo_frame::error_type::None) {
        error = sdCard.begin(); // Initialize the SD card

        if (error == photo_frame::error_type::None) {
#if DEBUG_MODE
            sdCard.print_stats(); // Print SD card statistics
#endif

#ifdef USE_GOOGLE_DRIVE
            // Use Google Drive to download the next image from Google Drive folder
            // The google_drive class handles authentication, caching, and file management

#if !defined(USE_INSECURE_TLS)
            // Load root CA certificate for secure Google Drive connections
            String rootCA = photo_frame::google_drive::load_root_ca_certificate(sdCard);
            if (rootCA.length() > 0) {
                driveClient.set_root_ca_certificate(rootCA);
                Serial.println(F("Root CA certificate loaded for secure connections"));
            } else {
                Serial.println(F("WARNING: Failed to load root CA certificate"));
                error = photo_frame::error_type::SslCertificateLoadFailed;
            }
#endif

            if (error == photo_frame::error_type::None) {
                // Clean up any temporary files from previous incomplete downloads

                uint32_t cleanedFiles = photo_frame::google_drive::cleanup_temporary_files(sdCard, drive_config, write_toc);
                if (cleanedFiles > 0) {
                    Serial.print(F("Cleaned up "));
                    Serial.print(cleanedFiles);
                    Serial.println(F(" temporary files from previous session"));
                }

                // Retrieve Table of Contents (with caching)
                // If battery is critical, use cached TOC even if expired to save power
                bool batteryConservationMode = battery_info.is_critical();
                if (batteryConservationMode) {
                    Serial.print(F("Battery critical ("));
                    Serial.print(battery_info.percent);
                    Serial.println(F("%) - using cached TOC to preserve power"));
                }

                // Ensure TOC is up to date (but don't load files into memory)
                drive.retrieve_toc(batteryConservationMode);

                // Get file count efficiently from plain text TOC
                String tocFullPath = drive.get_toc_file_path();
                photo_frame::photo_frame_error_t tocError;
                total_files = drive.get_toc_file_count(tocFullPath, &tocError);

                if (tocError == photo_frame::error_type::None && total_files > 0) {
                    Serial.print(F("Total files in Google Drive folder: "));
                    Serial.println(total_files);

                    // Generate a random index to start from
                    image_index = random(0, total_files);

                    // Get the specific file by index efficiently
                    photo_frame::google_drive_file selectedFile = drive.get_toc_file_by_index(tocFullPath, image_index, &tocError);

                    if (tocError == photo_frame::error_type::None && selectedFile.id.length() > 0) {
                        Serial.print(F("Selected file: "));
                        Serial.println(selectedFile.name);

                        // if the selected file already exists in the sdcard, then use it
                        String localFilePath = photo_frame::string_utils::buildPath(drive_config.localPath, selectedFile.name);
                        if (sdCard.file_exists(localFilePath.c_str()) && sdCard.get_file_size(localFilePath.c_str()) > 0) {
                            Serial.println(F("File already exists in SD card, using cached version"));
                            file = sdCard.open(localFilePath.c_str(), FILE_READ);
                        } else {

                            // Check battery level before downloading file
                            if (batteryConservationMode) {
                                Serial.print(F("Skipping file download due to critical battery level ("));
                                Serial.print(battery_info.percent);
                                Serial.println(F("%) - will use cached files if available"));
                                error = photo_frame::error_type::BatteryLevelCritical;
                            } else {
                                // Download the selected file using the new google_drive class
                                file = drive.download_file(selectedFile, &error);
                            }
                        }
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

#else
            if (!prefs.begin(PREFS_NAMESPACE, false)) {
                Serial.println(F("Failed to open preferences!"));
                error = photo_frame::error_type::PreferencesOpenFailed;
            } else {
                Serial.println(F("Preferences opened successfully!"));

                // upon reset, write the TOC (Table of Contents) to the SD card
                if (write_toc || !sdCard.file_exists(TOC_FILENAME)) {
                    Serial.println(F("Writing TOC to SD card..."));

                    error = sdCard.write_images_toc(&total_files, TOC_FILENAME, SD_DIRNAME, LOCAL_FILE_EXTENSION);
                    // Remove the old total_files key
                    prefs.remove("total_files");

                    if (error == photo_frame::error_type::None) {
                        // Save the total number of files to preferences
                        prefs.putUInt("total_files", total_files);
                    } else {
                        Serial.print(F("Failed to write TOC! Error code: "));
                        Serial.println(error.code);
                    }
                } else {
                    // Read the total number of files from preferences
                    total_files = prefs.getUInt("total_files", 0);

                    Serial.print(F("Total files from preferences: "));
                    Serial.println(total_files);
                }

                prefs.end(); // Close the preferences

                if (total_files == 0) {
                    Serial.println(F("No image files found on the SD card!"));
                    error = photo_frame::error_type::NoImagesFound;
                } else {
                    // Generate a random index to start from

#ifdef DEBUG_IMAGE_INDEX
                    image_index = DEBUG_IMAGE_INDEX;
                    Serial.print(F("Using debug image index: "));
                    Serial.println(image_index);
#else
                    image_index = random(0, total_files);
#endif // DEBUG_IMAGE_INDEX

                    Serial.print(F("Next image index: "));
                    Serial.println(image_index);

                    String image_file_path = sdCard.get_toc_file_path(image_index);

                    if (image_file_path.isEmpty()) {
                        Serial.print(F("Image file path is empty at index: "));
                        Serial.println(image_index);
                        error = photo_frame::error_type::SdCardFileNotFound;
                    } else {
                        file = sdCard.open(image_file_path.c_str(), FILE_READ);
                    }
                }
            }
#endif // USE_GOOGLE_DRIVE
        }
    } else {
        Serial.println(F("skipped"));
    }

    wifiManager.disconnect(); // Disconnect from WiFi to save power

    // --------------------------------------
    // - Initialize E-Paper display
    // --------------------------------------
    Serial.println(F("- Initializing E-Paper display..."));
    Serial.println(F("--------------------------------------"));

    delay(100);
    photo_frame::renderer::init_display();
    delay(100);

    display.clearScreen();

    if (error == photo_frame::error_type::None && !file) {
        Serial.println(F("File is not open!"));
        error = photo_frame::error_type::SdCardFileOpenFailed;
    }

    if (error != photo_frame::error_type::None) {
        photo_frame::board_utils::blink_error(error);
        display.firstPage();
        do {
            photo_frame::renderer::draw_error(error);
            photo_frame::renderer::draw_error_message(gravity_t::TOP_RIGHT, error.code);

            if (error != photo_frame::error_type::BatteryLevelCritical) {
                photo_frame::renderer::draw_last_update(now, 0);
            }
        } while (display.nextPage());
    } else {
        bool has_partial_update = photo_frame::renderer::has_partial_update();
        Serial.print(F("*** Display supports partial update: "));
        Serial.print(has_partial_update ? "Yes" : "No");
        Serial.println(F(" *** "));

        Serial.print(F("*** Display pages: "));
        Serial.print(display.pages());
        Serial.println(F(" *** "));

        const char* img_filename = file.name();

        if (!has_partial_update) { // if the display does not support partial update
            display.setFullWindow();
            display.fillScreen(GxEPD_WHITE);

            int page_index = 0;
            display.firstPage();
            do {
                Serial.print(F("Drawing page: "));
                Serial.println(page_index);

                file.seek(0); // Reset file pointer to the beginning
#if defined(EPD_USE_BINARY_FILE)
                bool success = photo_frame::renderer::draw_binary_from_file(file, img_filename, DISP_WIDTH, DISP_HEIGHT);
#else
                bool success = photo_frame::renderer::draw_bitmap_from_file(file, img_filename, 0, 0, false);
#endif

                if (!success) {
                    Serial.println(F("Failed to draw bitmap from file!"));
                    photo_frame::renderer::draw_error(photo_frame::error_type::ImageFormatNotSupported);
                }

                display.writeFillRect(0, 0, display.width(), 16, GxEPD_WHITE);

                photo_frame::renderer::draw_last_update(now, refresh_delay.refresh_seconds);
                photo_frame::renderer::draw_image_info(image_index, total_files);
                photo_frame::renderer::draw_battery_status(battery_info);

                page_index++;
            } while (display.nextPage()); // Clear the display

            file.close(); // Close the file after drawing

        } else {
#ifdef EPD_USE_BINARY_FILE
            bool success = photo_frame::renderer::draw_binary_from_file_buffered(file, img_filename, DISP_WIDTH, DISP_HEIGHT);
#else
            bool success = photo_frame::renderer::draw_bitmap_from_file_buffered(file, img_filename, 0, 0, false);
#endif
            file.close(); // Close the file after drawing

            if (!success) {
                Serial.println(F("Failed to draw bitmap from file!"));
                display.firstPage();
                do {
                    photo_frame::renderer::draw_error(photo_frame::error_type::ImageFormatNotSupported);
                } while (display.nextPage());
            }

            delay(1000); // Wait for the display to update

            display.setPartialWindow(0, 0, display.width(), 16);
            display.firstPage();
            do {
                if (has_partial_update) {
                    display.fillScreen(GxEPD_WHITE);
                } else {
                    display.fillRect(0, 0, display.width(), 16, GxEPD_WHITE);
                }
                // display.fillRect(0, 0, display.width(), 16, GxEPD_WHITE);

                photo_frame::renderer::draw_last_update(now, refresh_delay.refresh_seconds);
                photo_frame::renderer::draw_image_info(image_index, total_files);
                photo_frame::renderer::draw_battery_status(battery_info);
                // display.displayWindow(0, 0, display.width(), display.height());

            } while (display.nextPage()); // Clear the display
        }
    }

    if (file) {
        file.close(); // Close the file after drawing
    }
    sdCard.end(); // Close the SD card

    photo_frame::board_utils::disable_rgb_led();

    delay(1000);

    photo_frame::renderer::power_off();

    /* #endregion e-Paper display */

    // Wait before going to sleep

    // now go to sleep
    if (!battery_info.is_critical() && refresh_delay.refresh_microseconds > MICROSECONDS_IN_SECOND) {
        Serial.print(F("Going to sleep for "));
        Serial.print(refresh_delay.refresh_seconds, DEC);
        Serial.print(F(" seconds ("));
        Serial.print((unsigned long)(refresh_delay.refresh_microseconds / 1000000ULL)); // Show actual seconds from microseconds
        Serial.println(F(" seconds from microseconds calculation)..."));
        esp_sleep_enable_timer_wakeup(refresh_delay.refresh_microseconds); // wake up after the refresh interval
    } else if (refresh_delay.refresh_microseconds <= MICROSECONDS_IN_SECOND) {
        Serial.println(F("Sleep interval too short, entering indefinite sleep"));
    }

    unsigned long elapsed = millis() - startupTime;
    Serial.printf("Elapsed seconds since startup: %lu s\n", elapsed / 1000);
    photo_frame::board_utils::enter_deep_sleep(wakeup_reason);
}

void loop()
{
    // Nothing to do here, the ESP32 will go to sleep after setup
    // The loop is intentionally left empty
    // All processing is done in the setup function
    // The ESP32 will wake up from deep sleep and run the setup function again

    delay(1000); // Just to avoid watchdog reset
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

    if (!battery_info.is_critical() && now.isValid()) {
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
            // Set the next refresh time to the start of the next day
            DateTime nextDayStart = DateTime(now.year(), now.month(), now.day() + 1, DAY_START_HOUR, 0, 0);
            DateTime nextDayEnd = DateTime(now.year(), now.month(), now.day() + 1, DAY_END_HOUR, 0, 0);
            nextRefresh = nextRefresh > nextDayStart
                ? (nextRefresh < nextDayEnd ? nextRefresh : nextDayEnd)
                : nextDayStart;

            refresh_delay.refresh_seconds = nextRefresh.unixtime() - now.unixtime();
        }

        Serial.print(F("Next refresh time: "));
        Serial.println(nextRefresh.timestamp(DateTime::TIMESTAMP_FULL));

        // Convert seconds to microseconds for deep sleep with overflow protection
        // ESP32 max sleep time is ~18 hours (281474976710655 microseconds)
        // Practical limit defined in config.h

        if (refresh_delay.refresh_seconds <= 0) {
            refresh_delay.refresh_microseconds = 0;
            Serial.println(F("Warning: Invalid refresh interval, sleep disabled"));
        } else if (refresh_delay.refresh_seconds > MAX_DEEP_SLEEP_SECONDS) {
            refresh_delay.refresh_microseconds = (uint64_t)MAX_DEEP_SLEEP_SECONDS * MICROSECONDS_IN_SECOND;
            Serial.print(F("Warning: Refresh interval capped to "));
            Serial.print(MAX_DEEP_SLEEP_SECONDS);
            Serial.println(F(" seconds to prevent overflow"));
        } else {
            // Safe calculation using 64-bit arithmetic
            refresh_delay.refresh_microseconds = (uint64_t)refresh_delay.refresh_seconds * MICROSECONDS_IN_SECOND;
        }

        String humanReadable = photo_frame::string_utils::secondsToHuman(refresh_delay.refresh_seconds);

        Serial.print(F("Refresh interval in: "));
        Serial.println(humanReadable);
    } else {
        Serial.println(F("skipped"));
    }

    return refresh_delay;
}