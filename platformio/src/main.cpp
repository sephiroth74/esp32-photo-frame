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

long read_refresh_seconds(bool is_battery_low = false) {
    Serial.println(F("Reading level input pin..."));

    if (is_battery_low) {
        Serial.println(F("Battery level is low, skipping level input pin reading."));
        return REFRESH_INTERVAL_SECONDS_LOW_BATTERY;
    }

    // now read the level
    pinMode(LEVEL_PWR_PIN, OUTPUT);
    pinMode(LEVEL_INPUT_PIN, INPUT); // Set the level input pin as input

    digitalWrite(LEVEL_PWR_PIN, HIGH); // Power on the level shifter
    delay(100);                        // Wait for the level shifter to power up

    // read the level input pin for 100ms and average the result (10 samples)
    unsigned long level = 0;
    for (int i = 0; i < 10; i++) {
        level += analogRead(LEVEL_INPUT_PIN); // Read the level input pin
        delay(1);                             // Wait for 10ms
    }
    digitalWrite(LEVEL_PWR_PIN, LOW); // Power off the level shifter
    level /= 10;                      // Average the result

#if DEBUG_LOG
    Serial.print(F("Raw level input pin reading: "));
    Serial.println(level);
#endif

    level = constrain(level, 0, LEVEL_INPUT_MAX); // Constrain the level to the maximum value

    // invert the value
    level = LEVEL_INPUT_MAX - level;

    Serial.println(F("Level input value: "));
    Serial.println(level);

    long refresh_seconds =
        map(level, 0, LEVEL_INPUT_MAX, REFRESH_MIN_INTERVAL_SECONDS, REFRESH_MAX_INTERVAL_SECONDS);
    return refresh_seconds;
}

void setup() {
    Serial.begin(115200);

#if HAS_RGB_LED
    Serial.println(F("Initializing RGB LED..."));
    pinMode(LED_BLUE, OUTPUT);
    pinMode(LED_GREEN, OUTPUT);
    pinMode(LED_RED, OUTPUT);
#endif // HAS_RGB_LED

    photo_frame::disable_built_in_led();
    photo_frame::disable_rgb_led();
    delay(1000);

    Serial.println(F("Initializing..."));
    Serial.println(F("Photo Frame v0.1.0"));

    photo_frame::print_board_stats();
    photo_frame::print_config();

#if DEBUG_LOG
    delay(2000);
#endif

    photo_frame::photo_frame_error_t error = photo_frame::error_type::None;
    fs::File file;
    DateTime now         = DateTime((uint32_t)0); // Initialize DateTime with 0 timestamp
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
        Serial.println(F("Going to deep sleep..."));

#if DEBUG_LOG
        delay(DELAY_BEFORE_SLEEP); // Wait before going to deep sleep
#endif
        esp_deep_sleep_start(); // Put the ESP32 into deep sleep mode

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
            now = photo_frame::fetch_datetime(sdCard);
        }
        Serial.print("now is: ");
        Serial.println(now.timestamp(DateTime::TIMESTAMP_FULL));
        Serial.print("now is valid: ");
        Serial.println(now.isValid() ? "Yes" : "No");
    } else {
        Serial.println(F("skipped"));
    }

    /* #region E-Paper display */

    // -------------------------------------------------------------
    // 3. Initialize the E-Paper display
    // -------------------------------------------------------------
    Serial.println(F("3. Initializing E-Paper display..."));

    delay(100);
    photo_frame::renderer::initDisplay();
    delay(200);

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
            auto cardType = sdCard.getCardType();
            Serial.print(F("SD card type: "));
            switch (cardType) {
            case CARD_MMC:     Serial.println(F("MMC")); break;
            case CARD_SD:      Serial.println(F("SDSC")); break;
            case CARD_SDHC:    Serial.println(F("SDHC")); break;
            case CARD_UNKNOWN: Serial.println(F("Unknown")); break;
            case CARD_NONE:    Serial.println(F("No SD card attached!")); break;
            default:           Serial.println(F("Unknown card type!")); break;
            }

#if DEBUG_LOG
            sdCard.printStats(); // Print SD card statistics
#endif

            photo_frame::SdCardEntry entry;

            // Open preferences for reading

            uint32_t index = 0; // Initialize the index to 0

            if (!prefs.begin(PREFS_NAMESPACE, false)) {
                Serial.println(F("Failed to open preferences!"));
                // Get the count of .bmp files on the SD card
                auto count = sdCard.getFileCount(".bmp");
                // Generate a random index to start from
                index = random(0, count);
            } else {
                Serial.println(F("Preferences opened successfully!"));
                index = prefs.getUInt("last_index", 0); // Read the last index from preferences
                Serial.print(F("Last index from preferences: "));
                Serial.println(index);
            }

            // Read the next image file from the SD card
            error = sdCard.findNextImage(&index, ".bmp", &entry);
            prefs.putUInt("last_index", index + 1); // Save the last index to preferences
            prefs.end();                            // Close the preferences

            if (error == photo_frame::error_type::None) {
                Serial.print(F("Next image file: "));
                Serial.println(entry.to_string());
                Serial.print(F("Entry is valid:"));
                Serial.println(entry ? "Yes" : "No");

                // Open the file for reading
                file = sdCard.open(entry.path.c_str(), FILE_READ);
            } else {
                Serial.print(F("Failed to find next image file! Error code: "));
                Serial.println(error.code);
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
        refresh_seconds = read_refresh_seconds(battery_info.is_low());

#if DEBUG_LOG
        Serial.print(F("Refresh seconds read from level input pin: "));
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
            DateTime nextDayStart =
                DateTime(now.year(), now.month(), now.day() + 1, DAY_START_HOUR, 0, 0);
            DateTime nextDayEnd =
                DateTime(now.year(), now.month(), now.day() + 1, DAY_END_HOUR, 0, 0);
            nextRefresh     = nextRefresh > nextDayStart
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
        photo_frame::blink_builtin_led(error);
        display.firstPage();
        do {
            photo_frame::renderer::drawError(error);
            photo_frame::renderer::drawErrorMessage(gravity_t::TOP_RIGHT, error.code);

            if (error != photo_frame::error_type::BatteryLevelCritical) {
                photo_frame::renderer::drawLastUpdate(now, nullptr);
            }
        } while (display.nextPage());
    } else {
        if (!photo_frame::renderer::drawBitmapFromFile_Buffered(file, 0, 0, false)) {
            Serial.println(F("Failed to draw bitmap from file!"));
            display.firstPage();
            do {
                photo_frame::renderer::drawError(photo_frame::error_type::ImageFormatNotSupported);
            } while (display.nextPage());
        }

        display.setPartialWindow(0, 0, display.width(), 16);
        display.firstPage();
        do {

            // display.fillRect(0, 0, display.width(), 20, GxEPD_WHITE);
            display.fillScreen(GxEPD_WHITE);

            // format the refresh time string in the format "Refresh: 0' 0" (e.g., "Refresh: 10'
            // 30")
            String refreshStr = "";
            long hours        = refresh_seconds / 3600; // Calculate hours
            long minutes      = refresh_seconds / 60;   // Calculate minutes
            long seconds      = refresh_seconds % 60;   // Calculate remaining seconds

            if (hours > 0) {
                refreshStr += String(hours) + "h "; // Append hours with a single quote
                minutes -= hours * 60;              // Adjust minutes after calculating hours
                seconds -= hours * 3600;            // Adjust seconds after calculating hours
            }

            if (minutes > 0) {
                refreshStr += String(minutes) + "m "; // Append minutes with a single quote
                seconds -= minutes * 60;              // Adjust seconds after calculating minutes
            }

            if (seconds > 0) {
                refreshStr += String(seconds) + "s"; // Append seconds with a double quote
            }

            photo_frame::renderer::drawLastUpdate(now, refreshStr.c_str());
            photo_frame::renderer::drawBatteryStatus(
                battery_info.raw_value, battery_info.millivolts, battery_info.percent);

            // display.displayWindow(0, 0, display.width(), display.height()); // Display the
            // content in the partial window

        } while (display.nextPage()); // Clear the display
    }

    if (file) {
        file.close(); // Close the file after drawing
    }
    sdCard.end(); // Close the SD card

    delay(500);
    photo_frame::renderer::powerOffDisplay();

/* #endregion e-Paper display */

// Wait before going to sleep
#if DEBUG_LOG
    delay(DELAY_BEFORE_SLEEP);
#endif

    // now go to sleep
    if (!battery_info.is_critical() && refresh_microseconds > MICROSECONDS_IN_SECOND) {
        Serial.print(F("Going to sleep for "));
        Serial.print(refresh_seconds, DEC);
        Serial.println(F(" seconds..."));
        esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, 1);         // wake up on GPIO 0
        esp_sleep_enable_timer_wakeup(refresh_microseconds); // wake up after the refresh interval
    }
    esp_deep_sleep_start();
}

void loop() {
    // Nothing to do here, the ESP32 will go to sleep after setup
    // The loop is intentionally left empty
    // All processing is done in the setup function
    // The ESP32 will wake up from deep sleep and run the setup function again

    delay(1000); // Just to avoid watchdog reset
}
