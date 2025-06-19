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

#include <Arduino.h>
#include <Preferences.h>
#include <Wire.h>

#include "battery.h"
#include "board_util.h"
#include "config.h"
#include "errors.h"
#include "renderer.h"
#include "rtc_util.h"
#include <assets/icons/icons.h>

#include "sd_card.h"

battery::BatteryReader battery_reader(BATTERY_PIN,
    BATTERY_RESISTORS_RATIO,
    BATTERY_NUM_READINGS,
    BATTERY_DELAY_BETWEEN_READINGS);

photo_frame::SDCard sdCard(SD_CS_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_SCK_PIN);

Preferences prefs;

void setup()
{
    Serial.begin(115200);

#if HAS_RGB_LED
    Serial.println(F("Initializing RGB LED..."));
    pinMode(LED_BLUE, OUTPUT);
    pinMode(LED_GREEN, OUTPUT);
    pinMode(LED_RED, OUTPUT);
#endif // HAS_RGB_LED

    photo_frame::disable_built_in_led();
    photo_frame::toggle_rgb_led(false, true, false); // Set RGB LED to green to indicate initialization
    delay(1000);
    photo_frame::disable_rgb_led(); // Disable RGB LED to save power

    Serial.println(F("Initializing..."));
    Serial.println(F("Photo Frame v0.1.0"));

    esp_sleep_wakeup_cause_t wakeup_reason = photo_frame::get_wakeup_reason();
    String wakeup_reason_string = photo_frame::get_wakeup_reason_string(wakeup_reason);

    // if the wakeup reason is undefined, it means the device is starting up after
    // a reset
    bool is_reset = wakeup_reason == ESP_SLEEP_WAKEUP_UNDEFINED;

    // if in reset state, we will write the TOC (Table of Contents) to the SD
    bool write_toc = is_reset;

    uint32_t image_index = 0; // Index of the image to display
    uint32_t total_files = 0; // Total number of image files on the SD card

    photo_frame::print_board_stats();
    photo_frame::print_config();

    Serial.print(F("Wakeup reason: "));
    Serial.print(wakeup_reason_string);
    Serial.print(F(" ("));
    Serial.print(wakeup_reason);
    Serial.println(F(")"));

    Serial.print(F("Is reset: "));
    Serial.println(is_reset ? "Yes" : "No");

#if DEBUG_MODE
    delay(2000);
#endif

    photo_frame::photo_frame_error_t error = photo_frame::error_type::None;
    fs::File file;
    DateTime now = DateTime((uint32_t)0); // Initialize DateTime with 0 timestamp
    long refresh_seconds = 0;
    unsigned long refresh_microseconds = refresh_seconds * MICROSECONDS_IN_SECOND;

    // -------------------------------------------------------------
    // 1. read the battery level
    // -------------------------------------------------------------
    Serial.println(F("1. Reading battery level..."));

    battery_reader.init();
    battery::battery_info_t battery_info = battery_reader.read();

    Serial.print(F("Battery millivolts: "));
    Serial.print(battery_info.millivolts);
    Serial.print(F(", percent: "));
    Serial.print(battery_info.percent);
    Serial.println(F("%"));

    if (battery_info.is_empty()) {
        Serial.println(F("Battery is empty!"));
        photo_frame::enter_deep_sleep(wakeup_reason); // Enter deep sleep mode
        return; // Exit if battery is empty
    } else if (battery_info.is_critical()) {
        Serial.println(F("Battery level is critical!"));
        error = photo_frame::error_type::BatteryLevelCritical;
    }

    // --------------------------------------------------------------
    // 2. Initialize the SD card and fetch the current time
    // --------------------------------------------------------------

    Serial.println(F("2. Initializing SD card..."));

    if (!battery_info.is_critical()) {
        error = sdCard.begin(); // Initialize the SD card
        if (error == photo_frame::error_type::None) {
            now = photo_frame::fetch_datetime(sdCard, (is_reset), &error);
        } else {
            Serial.println(F("Skipping datetime fetch due to SD card error!"));
        }

        if (error == photo_frame::error_type::None) {
            Serial.print("now is: ");
            Serial.println(now.timestamp(DateTime::TIMESTAMP_FULL));
            Serial.print("now is valid: ");
            Serial.println(now.isValid() ? "Yes" : "No");
        } else {
            Serial.print(F("Failed to fetch current time! Error code: "));
            Serial.println(error.code);
            Serial.println(F("Skipping datetime fetch!"));
        }
    } else {
        Serial.println(F("skipped"));
    }

    /* #region E-Paper display */

    // -------------------------------------------------------------
    // 3. Initialize the E-Paper display
    // -------------------------------------------------------------
    Serial.println(F("3. Initializing E-Paper display..."));

    delay(100);
    photo_frame::renderer::init_display();
    delay(100);

    display.clearScreen();

    /* #endregion */

    /* #region SDCARD */

    // -------------------------------------------------------------
    // 4. Read the next image file from the SD card
    // -------------------------------------------------------------
    Serial.println(F("4. Find the next image from the SD..."));

    if (error == photo_frame::error_type::None) {
        error = sdCard.begin(); // Initialize the SD card

        if (error == photo_frame::error_type::None) {
#if DEBUG_MODE
            sdCard.print_stats(); // Print SD card statistics
#endif

            if (!prefs.begin(PREFS_NAMESPACE, false)) {
                Serial.println(F("Failed to open preferences!"));
                error = photo_frame::error_type::PreferencesOpenFailed;
            } else {
                Serial.println(F("Preferences opened successfully!"));

                // upon reset, write the TOC (Table of Contents) to the SD card
                if (write_toc || !sdCard.file_exists(TOC_FILENAME)) {
#if DEBUG_MODE
                    Serial.println(F("Writing TOC to SD card..."));
#endif

                    error = sdCard.write_images_toc(&total_files);
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
                    image_index = random(0, total_files);

#if DEBUG_MODE
                    Serial.print(F("Next image index: "));
                    Serial.println(image_index);
#endif

                    String image_file_path = sdCard.get_toc_file_path(image_index);

                    if (image_file_path.isEmpty()) {
#if DEBUG_MODE
                        Serial.print(F("Image file path is empty at index: "));
                        Serial.println(image_index);
#endif
                        error = photo_frame::error_type::SdCardFileNotFound;
                    } else {
                        file = sdCard.open(image_file_path.c_str(), FILE_READ);
                    }
                }
            }
        }
    } else {
        Serial.println(F("skipped"));
    }

    /* #endregion SDCARD */

    /* #region Refresh Rate */

    // -------------------------------------------------------------
    // 5. Calculate the refresh rate based on the battery level
    // -------------------------------------------------------------
    Serial.println(F("5. Calculating refresh rate..."));

    if (!battery_info.is_critical() && now.isValid()) {
        refresh_seconds = photo_frame::read_refresh_seconds(battery_info.is_low());

#if DEBUG_MODE
        Serial.print(F("Refresh seconds read from potentiometer: "));
        Serial.println(refresh_seconds);
#endif

        // add the refresh time to the current time
        // Update the current time with the refresh interval
        DateTime nextRefresh = now + TimeSpan(refresh_seconds);

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

            refresh_seconds = nextRefresh.unixtime() - now.unixtime();
        }

        Serial.print(F("Next refresh time: "));
        Serial.println(nextRefresh.timestamp(DateTime::TIMESTAMP_FULL));

        // Convert minutes to microseconds for deep sleep
        refresh_microseconds = refresh_seconds * MICROSECONDS_IN_SECOND;

        Serial.print(F("Refresh interval in seconds: "));
        Serial.println(refresh_seconds);
    } else {
        Serial.println(F("skipped"));
    }

    /* #endregion */

    /* #region e-Paper display */

    if (error == photo_frame::error_type::None && !file) {
        Serial.println(F("File is not open!"));
        error = photo_frame::error_type::SdCardFileOpenFailed;
    }

    if (error != photo_frame::error_type::None) {
        photo_frame::blink_error(error);
        display.firstPage();
        do {
            photo_frame::renderer::draw_error(error);
            photo_frame::renderer::draw_error_message(gravity_t::TOP_RIGHT, error.code);

            if (error != photo_frame::error_type::BatteryLevelCritical) {
                photo_frame::renderer::draw_last_update(now, 0);
            }
        } while (display.nextPage());
    } else {
        if (!photo_frame::renderer::draw_bitmap_from_file_buffered(file, 0, 0, false)) {
            Serial.println(F("Failed to draw bitmap from file!"));
            display.firstPage();
            do {
                photo_frame::renderer::draw_error(photo_frame::error_type::ImageFormatNotSupported);
            } while (display.nextPage());
        }

        display.setPartialWindow(0, 0, display.width(), 16);
        display.firstPage();
        do {
            // display.fillRect(0, 0, display.width(), 20, GxEPD_WHITE);
            display.fillScreen(GxEPD_WHITE);

            photo_frame::renderer::draw_last_update(now, refresh_seconds);
            photo_frame::renderer::draw_image_info(image_index, total_files);
            photo_frame::renderer::draw_battery_status(
                battery_info.raw_value, battery_info.millivolts, battery_info.percent);

            // display.displayWindow(0, 0, display.width(), display.height()); //
            // Display the content in the partial window

        } while (display.nextPage()); // Clear the display
    }

    if (file) {
        file.close(); // Close the file after drawing
    }
    sdCard.end(); // Close the SD card

    photo_frame::disable_rgb_led();
    delay(500);
    photo_frame::renderer::power_off();

    /* #endregion e-Paper display */

    // Wait before going to sleep

    // now go to sleep
    if (!battery_info.is_critical() && refresh_microseconds > MICROSECONDS_IN_SECOND) {
        Serial.print(F("Going to sleep for "));
        Serial.print(refresh_seconds, DEC);
        Serial.println(F(" seconds..."));
        esp_sleep_enable_timer_wakeup(refresh_microseconds); // wake up after the refresh interval
    }

    photo_frame::enter_deep_sleep(wakeup_reason);
}

void loop()
{
    // Nothing to do here, the ESP32 will go to sleep after setup
    // The loop is intentionally left empty
    // All processing is done in the setup function
    // The ESP32 will wake up from deep sleep and run the setup function again

    delay(1000); // Just to avoid watchdog reset
}
