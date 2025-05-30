#include <Arduino.h>
#include <Wire.h>

#include <Battery.h>
#include <RTClib.h>
#include <WiFi.h>

#include "config.h"
#include "renderer.h"
#include <assets/icons/icons.h>

#include "sd_card.h"

/* #endregion */

/* #region Battery */

#define BATTERY_PIN         A0      // the analog input pin
#define BATTERY_R1          1000000 // Resistor 1 value in Ohms
#define BATTERY_R2          1000000 // Resistor 2 value in Ohms

#define BATTERY_ADJUSTMENT  1.0  // Adjustment factor for battery voltage calculation
#define BATTERY_MIN_VOLTAGE 3300 // Minimum voltage in mV
#define BATTERY_MAX_VOLTAGE 4200 // Maximum voltage in mV

/* #endregion */

/* #region SD CARD */

photo_frame::SDCard sdCard(SD_CS_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_SCK_PIN);

// SPIClass hspi(HSPI); // Create an SPI object for HSPI (sdcard)

/* #endregion SD CARD */

// #define SCREEN_WIDTH         128  // OLED display width, in pixels
// #define SCREEN_HEIGHT        32   // OLED display height, in pixels
// #define SCREEN_ADDRESS       0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for
// 128x32 #define SSD_1306_SDA_PIN     A4 #define SSD_1306_SCL_PIN     A5

// Adafruit_SSD1306 display;

BatteryLib batteryLib(BATTERY_PIN,
                      BATTERY_R1,
                      BATTERY_R2,
                      BATTERY_MIN_VOLTAGE,
                      BATTERY_MAX_VOLTAGE); // 3300 mV to 4150 mV

char dateTimeBuffer[32]     = {0}; // Buffer to hold formatted date and time
const char dateTimeFormat[] = "%04d/%02d/%02d %02d:%02d:%02d";

const char* format_datetime(DateTime& now) {
    sprintf(dateTimeBuffer,
            dateTimeFormat,
            now.year(),
            now.month(),
            now.day(),
            now.hour(),
            now.minute(),
            now.second());
    return dateTimeBuffer;
} // format_datetime

void print_board_stats() {
    Serial.print("Heap:");
    Serial.println(ESP.getHeapSize());
    Serial.print("Flash: ");
    Serial.println(ESP.getFlashChipSize());
    Serial.print("Free Heap: ");
    Serial.println(ESP.getFreeHeap());
    Serial.print("Free Flash: ");
    Serial.println(ESP.getFreeSketchSpace());
    Serial.print("Free PSRAM: ");
    Serial.println(ESP.getFreePsram());
    Serial.print("Chip Model: ");
    Serial.println(ESP.getChipModel());
    Serial.print("Chip Revision: ");
    Serial.println(ESP.getChipRevision());
    Serial.print("Chip Cores: ");
    Serial.println(ESP.getChipCores());
    Serial.print("CPU Freq: ");
    Serial.println(ESP.getCpuFreqMHz());
    Serial.println("");
} // print_board_stats

DateTime fetch_remote_datetime() {
    Serial.println(F("Fetching time from WiFi..."));
    DateTime result = DateTime((uint32_t)0);

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    unsigned long timeout         = millis() + WIFI_CONNECT_TIMEOUT;
    wl_status_t connection_status = WiFi.status();
    while ((connection_status != WL_CONNECTED) && (millis() < timeout)) {
        Serial.print(".");
        delay(200);
        connection_status = WiFi.status();
    }
    Serial.println();

    if (connection_status != WL_CONNECTED) {
        Serial.println(F("Failed to connect to WiFi!"));
        Serial.println(F("Please check your WiFi credentials."));
        return result;
    } else {
        Serial.println(F("Connected to WiFi!"));
    }

    // acquire the time from the NTP server
    configTzTime(TIMEZONE, NTP_SERVER1, NTP_SERVER2);

    // wait for the time to be set
    Serial.print(F("Waiting for time."));
    while (time(nullptr) < 1000000000) {
        Serial.print(F("."));
        delay(1000);
    }
    Serial.println(F("done."));

    // Set the RTC to the time we just set
    // get the local time

    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, NTP_TIMEOUT)) {
        Serial.println(F("Failed to obtain time from NTP server!"));
        return result;
    }

    result = DateTime(timeinfo.tm_year + 1900,
                      timeinfo.tm_mon + 1,
                      timeinfo.tm_mday,
                      timeinfo.tm_hour,
                      timeinfo.tm_min,
                      timeinfo.tm_sec);

    WiFi.disconnect();
    return result;
} // featch_remote_datetime

DateTime fetch_datetime() {
    Serial.println("Initializing RTC...");

    DateTime now;
    RTC_DS3231 rtc;

    pinMode(RTC_POWER_PIN, OUTPUT);
    digitalWrite(RTC_POWER_PIN, HIGH); // Power on the RTC
    delay(1000);                       // Wait for RTC to power up

    Wire1.begin(RTC_SDA_PIN /* SDA */, RTC_SCL_PIN /* SCL */);
    if (!rtc.begin(&Wire1)) {
        Serial.println(F("Couldn't find RTC"));
        now = fetch_remote_datetime();
        if (now.unixtime() == 0) {
            Serial.println(F("Failed to fetch time from WiFi!"));
            now = DateTime(F(__DATE__), F(__TIME__));
        }

    } else {
        rtc.disable32K();
        if (rtc.lostPower()) {
            // Set the time to a default value if the RTC lost power
            Serial.println(F("RTC lost power, setting the time!"));
            now = fetch_remote_datetime();
            if (now.unixtime() == 0) {
                Serial.println(F("Failed to fetch time from WiFi!"));
            } else {
                rtc.adjust(now);
            }
        } else {
            Serial.println(F("RTC is running!"));
            now = rtc.now();
        }
    }

    Wire1.end();
    digitalWrite(RTC_POWER_PIN, LOW); // Power off the RTC
    return now;
}

void disable_built_in_led() {
#if defined(LED_BUILTIN)
    Serial.print(F("Disabling built-in LED on pin "));
    Serial.println(LED_BUILTIN);

    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW); // Disable the built-in LED
#endif
} // disable_builtin_led

void blink_builtin_led(int count, int delay_ms = 500) {
#if defined(LED_BUILTIN)
    Serial.println(F("Blinking built-in LED..."));
    pinMode(LED_BUILTIN, OUTPUT);
    for (int i = 0; i < count; i++) {
        digitalWrite(LED_BUILTIN, HIGH); // Turn the LED on
        delay(delay_ms);
        digitalWrite(LED_BUILTIN, LOW); // Turn the LED off
        delay(delay_ms);
    }
#else
    Serial.println(F("LED_BUILTIN is not defined! Cannot blink the built-in LED."));
#endif
} // blink_builtin_led

void blink_builtin_led(photo_frame_error_t error) {
    Serial.print(F("Blinking built-in LED with error code: "));
    Serial.print(error.code);
    Serial.print(F(" ("));
    Serial.print(error.message);
    Serial.print(F(") - "));
    Serial.print(F("Blink count: "));
    Serial.println(error.blink_count);

    blink_builtin_led(error.blink_count, 200);
} // blink_builtin_led with error code

void print_default_pins() {
    Serial.println(F("Default Pin Mappings:"));
    Serial.print(F("CS_PIN: "));
    Serial.println(SS);
    Serial.print(F("MOSI_PIN: "));
    Serial.println(MOSI);
    Serial.print(F("MISO_PIN: "));
    Serial.println(MISO);
    Serial.print(F("SCK_PIN: "));
    Serial.println(SCK);
    Serial.print(F("SDA_PIN: "));
    Serial.println(SDA);
    Serial.print(F("SCL_PIN: "));
    Serial.println(SCL);
    Serial.print(F("RTC_SDA_PIN: "));
    Serial.println(RTC_SDA_PIN);
    Serial.print(F("RTC_SCL_PIN: "));
    Serial.println(RTC_SCL_PIN);
    Serial.print(F("RTC_POWER_PIN: "));
    Serial.println(RTC_POWER_PIN);
}

long read_refresh_seconds() {
    Serial.println(F("Reading level input pin..."));

    // now read the level
    pinMode(LEVEL_PWR_PIN, OUTPUT);
    pinMode(LEVEL_INPUT_PIN, INPUT); // Set the level input pin as input

    digitalWrite(LEVEL_PWR_PIN, HIGH); // Power on the level shifter
    delay(100);                        // Wait for the level shifter to power up

    // read the level input pin for 100ms and average the result (10 samples)
    unsigned long level = 0;
    for (int i = 0; i < 10; i++) {
        level += analogRead(LEVEL_INPUT_PIN); // Read the level input pin
        delay(10);                            // Wait for 10ms
    }
    digitalWrite(LEVEL_PWR_PIN, LOW); // Power off the level shifter
    level /= 10;                      // Average the result

    Serial.print(F("Raw level input pin reading: "));
    Serial.println(level);

    level = constrain(level, 0, LEVEL_INPUT_MAX); // Constrain the level to the maximum value

    // invert the value
    level = LEVEL_INPUT_MAX - level;

    Serial.println(F("Level input pin reading completed!"));
    Serial.print(F("Percent: "));
    Serial.println(level);

    long refresh_seconds =
        map(level, 0, LEVEL_INPUT_MAX, REFRESH_MIN_INTERVAL_SECONDS, REFRESH_MAX_INTERVAL_SECONDS);
    return refresh_seconds;
}

void setup() {
    // disable built-in LED
    disable_built_in_led();

    delay(1000);
    Serial.begin(115200);
    delay(1000);
    Serial.println(F("Initializing..."));
    Serial.println(F("Photo Frame v0.1.0"));
    delay(1000);

    print_board_stats();
    // print_default_pins();

    /* #region RTC */
    DateTime now = fetch_datetime();
    delay(1000);

    Serial.println("-------------------------------------------");
    Serial.print(F("RTC time: "));
    Serial.println(now.timestamp(DateTime::TIMESTAMP_FULL));
    Serial.println("-------------------------------------------");
    /* #endregion RTC */

    /* #region SDCARD */
    Serial.println(F("Initializing SD card..."));

    photo_frame_error_t error = sdCard.begin(); // Initialize the SD card
    if (error != error_type::None) {
        blink_builtin_led(error);
        sdCard.end(); // Close the SD card
        return;       // Exit if SD card initialization failed
    }

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

    sdCard.printStats(); // Print SD card statistics

    photo_frame::SdCardEntry entry;
    // sdCard.listFiles(".bmp"); // List all .bmp files on the SD card
    auto count = sdCard.getFileCount(".bmp"); // Get the count of .bmp files on the SD card
    // Serial.print(F("Number of .bmp files on SD card: "));
    // Serial.println(count);

    uint32_t index = random(0, count); // Generate a random index to start from

    error =
        sdCard.findNextImage(&index, ".bmp", &entry); // Read the next image file from the SD card
    if (error != error_type::None) {
        blink_builtin_led(error);
        sdCard.end(); // Close the SD card
        return;       // Exit if reading the next image failed
    }

    Serial.print(F("Next image file: "));
    Serial.println(entry.to_string());
    Serial.print(F("Entry is valid:"));
    Serial.println(entry ? "Yes" : "No");

    fs::File file = sdCard.open(entry.path.c_str(), FILE_READ); // Open the file for reading
    if (!file) {
        Serial.println(F("Failed to open file!"));
        error = error_type::SdCardFileOpenFailed;
        blink_builtin_led(error);
        sdCard.end(); // Close the SD card
        return;       // Exit if opening the file failed
    }

    // sdCard.listFiles(); // List files on the SD card
    // sdCard.end(); // Close the SD card

    /* #endregion SDCARD */

    /* #region Refresh Rate */

    long refresh_seconds = read_refresh_seconds();

    // add the refresh time to the current time
    // Update the current time with the refresh interval
    DateTime nextRefresh = now + TimeSpan(refresh_seconds);

    // check if the next refresh time is after DAY_END_HOUR
    Serial.println("-------------------------------------------");

    if (nextRefresh.hour() < DAY_START_HOUR || nextRefresh.hour() >= DAY_END_HOUR) {
        Serial.println(
            F("Next refresh time is before DAY_START_HOUR or after DAY_END_HOUR, setting to "
              "DAY_START_HOUR..."));
        nextRefresh = DateTime(now.year(), now.month(), now.day() + 1, DAY_START_HOUR, 0, 0);
        Serial.print(F("Next refresh time set to: "));
        Serial.println(nextRefresh.timestamp(DateTime::TIMESTAMP_FULL));
        refresh_seconds = nextRefresh.unixtime() - now.unixtime();
    }

    Serial.print(F("Next refresh time: "));
    Serial.println(nextRefresh.timestamp(DateTime::TIMESTAMP_FULL));

    // Convert minutes to microseconds for deep sleep
    unsigned long refresh_microseconds = refresh_seconds * MICROSECONDS_IN_SECOND;

    Serial.print(F("Refresh interval in seconds: "));
    Serial.println(refresh_seconds);
    Serial.println("-------------------------------------------");

    /* #endregion */

    /* #region e-Paper display */

    delay(1000);                          // Pause for 1 second
    photo_frame::renderer::initDisplay(); // Initialize the e-Paper display
    delay(1000);                          // Pause for 1 second

    display.clearScreen();
    // display.setFullWindow();

    // display.firstPage();
    // do {
    //     photo_frame::renderer::drawError(error_type::ImageFormatNotSupported);
    //     photo_frame::renderer::drawErrorMessage(gravity_t::TOP_RIGHT,
    //                                             error_type::ImageFormatNotSupported.code);
    // } while (display.nextPage());
    // return;

    if (!photo_frame::renderer::drawBitmapFromFile_Buffered(file, 0, 0, false)) {
        Serial.println(F("Failed to draw bitmap from file!"));
        display.firstPage();
        do {
            photo_frame::renderer::drawError(error_type::ImageFormatNotSupported);
        } while (display.nextPage());
    }

    display.setPartialWindow(0, 0, display.width(), 16);
    display.firstPage();
    do {

        // display.fillRect(0, 0, display.width(), 20, GxEPD_WHITE);
        display.fillScreen(GxEPD_WHITE);

        // format the refresh time string in the format "Refresh: 0' 0" (e.g., "Refresh: 10' 30")
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
        photo_frame::renderer::drawLastUpdate(now, refreshStr.c_str()); // Draw the last update time

        // display.displayWindow(0, 0, display.width(), display.height()); // Display the content in
        // the partial window

    } while (display.nextPage()); // Clear the display

    // display.display(true);

    // display.refresh(); // Refresh the display to show the drawn content

    // photo_frame::renderer::clearScreen();
    // photo_frame::renderer::drawBitmapFromFile(file, 0, 0, false); // Draw the bitmap from the
    // file Serial.println(F("Bitmap drawn successfully!"));
    file.close(); // Close the file after drawing

    // photo_frame::renderer::drawLastUpdate(now);
    // photo_frame::renderer::refresh();

    delay(500); // Pause for 1 second
    photo_frame::renderer::powerOffDisplay();

    sdCard.end(); // Close the SD card

    /* #endregion e-Paper display */

    // now go to sleep
    Serial.print(F("Going to sleep for "));
    Serial.print(refresh_seconds, DEC);
    Serial.println(F(" seconds..."));

// Wait for 10 seconds before going to sleep
#if DEBUG_LOG
    delay(10000);
#endif

    esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, 1);         // wake up on GPIO 0
    esp_sleep_enable_timer_wakeup(refresh_microseconds); // wake up after the refresh interval
    esp_deep_sleep_start();

    /* #region Adafruit SSD1306 */
    // Serial.println("Initializing OLED display...");
    // Wire.begin(SSD_1306_SDA_PIN /* SDA */, SSD_1306_SCL_PIN /* SCL */); // SDA, SCL pins for
    // ESP32 display = Adafruit_SSD1306(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1); if
    // (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    //     Serial.println(F("SSD1306 allocation failed"));
    //     for (;;)
    //         ;
    // }

    // Serial.println(F("SSD1306 initialized successfully!"));
    // display.display();
    // delay(1500); // Pause for 2 seconds
    // display.clearDisplay();
    // display.setTextSize(1);
    // display.setTextColor(SSD1306_WHITE);
    // display.setCursor(0, 0); // Start at top-left corner
    // display.println(F("Date: "));
    // const char* dateTime = format_datetime(now);
    // display.println(dateTime);
    // display.println(now.timestamp(DateTime::TIMESTAMP_FULL));
    // display.display();
    /* #endregion Adafruit SSD1306 */

    delay(2000); // Pause for 2 seconds
}

void loop() {
    // display.clearDisplay();
    // display.setTextSize(1);              // Normal 1:1 pixel scale
    // display.setTextColor(SSD1306_WHITE); // Draw white text
    // display.setCursor(0, 0);             // Start at top-left corner

    // Battery battery = batteryLib.read(); // Read the battery voltage

    // Serial.print(F("Battery: "));
    // Serial.print(battery.toString());
    // Serial.println();

    // display.print(F("Raw: "));
    // display.print(battery.raw); // Print the raw ADC value

    // display.print(F(" - "));
    // display.print(battery.voltage / 1000.0, 2);
    // display.println(F(" V"));

    // display.print(F("Perc: "));
    // display.print(battery.percent, 2);
    // display.println("%");

    // const char* dateTime = format_datetime(now);
    // display.println(dateTime);

    // // draw a black filled rectangle with white stroke at the bottom
    // display.fillRect(0, 24, 128, 8, SSD1306_BLACK);
    // display.drawRect(0, 24, 128, 8, SSD1306_WHITE);

    // // draw the battery percentage as a filled rectangle
    // int barWidth = map(battery.percent, 0, 100, 0, 128);
    // display.fillRect(0, 24, barWidth, 8, SSD1306_WHITE);
    // display.display();
    // delay(DELAY_TIME); // Wait for 1 seconds
}
