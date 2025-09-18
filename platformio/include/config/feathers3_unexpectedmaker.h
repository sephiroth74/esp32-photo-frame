#include <Arduino.h>

// Configuration for Unexpected Maker FeatherS3 (ESP32-S3)
// Pin definitions based on FeatherS3 pinout
// https://feathers3.io/pinout.html

// #define USE_SHARED_SPI // Use shared SPI for SD card and display
#define USE_HSPI_FOR_EPD // Use separate HSPI bus for e-Paper display

// Pin definitions for FeatherS3
// SD Card - using SPI pins
#define SD_CS_PIN   5  // Available digital pin
#define SD_MISO_PIN 37 // SPI MISO
#define SD_MOSI_PIN 35 // SPI MOSI
#define SD_SCK_PIN  36 // SPI SCK

// e-Paper Display - using SPI pins (shared with SD card)
#define EPD_BUSY_PIN 6  // Available digital pin
#define EPD_RST_PIN  7  // Available digital pin
#define EPD_DC_PIN   8  // Available digital pin
#define EPD_CS_PIN   4  // Available digital pin
#define EPD_SCK_PIN  36 // Shared SPI SCK
#define EPD_MOSI_PIN 35 // Shared SPI MOSI
#define EPD_MISO_PIN 37 // Shared SPI MISO

// Potentiometer pin - using analog pins
#define POTENTIOMETER_PWR_PIN   10   // Digital pin for power control
#define POTENTIOMETER_INPUT_PIN A0   // Analog pin A0 (GPIO1)
#define POTENTIOMETER_INPUT_MAX 4095 // 12-bit ADC

// Battery monitoring - FeatherS3 has built-in battery voltage divider
#define BATTERY_PIN                    A13 // Built-in battery voltage pin (GPIO13)
#define BATTERY_NUM_READINGS           100
#define BATTERY_DELAY_BETWEEN_READINGS 10
#define BATTERY_RESISTORS_RATIO        0.5 // FeatherS3 built-in divider ratio

// Built-in LED - FeatherS3 uses RGB LED on pin 40
#ifdef LED_BUILTIN
#undef LED_BUILTIN
#endif // LED_BUILTIN

#define LED_BUILTIN 13 // FeatherS3 RGB LED (NeoPixel)

// External wakeup configuration
#define WAKEUP_EXT0
// Use available RTC GPIO pin for wakeup
#define WAKEUP_PIN      GPIO_NUM_25
#define WAKEUP_PIN_MODE INPUT_PULLDOWN // Internal pull-down to prevent floating pin issues
#define WAKEUP_LEVEL    HIGH

// ESP32-S3 has many RTC IO pins available for deep sleep wakeup
// We're using GPIO3 which is definitely an RTC IO pin

// Power saving configuration
#define BATTERY_POWER_SAVING

// Delay before going to sleep in milliseconds
#define DELAY_BEFORE_SLEEP 8000 // Reduced since no I2C/WiFi conflicts

// WiFi configuration
#define WIFI_FILENAME "/wifi.txt"

// Network timeouts - can be more aggressive since ESP32-S3 is more stable
#define WIFI_CONNECT_TIMEOUT 8000 // Reduced timeout
#define TIMEZONE             "CET-1CEST,M3.5.0,M10.5.0"
#define NTP_TIMEOUT          10000 // Reduced timeout
#define NTP_SERVER1          "pool.ntp.org"
#define NTP_SERVER2          "time.nist.gov"

// e-Paper display configuration
#define DISP_6C // 6-color e-Paper display (GDEP073E01)
// #define DISP_BW_V2 // Black and White e-Paper display
#define USE_DESPI_DRIVER

// Accent color for the display
#define ACCENT_COLOR GxEPD_RED

// Miscellaneous
#define DEBUG_MODE 0
#define DEBUG_BATTERY_READER

// Reset handling
#define RESET_INVALIDATES_DATE_TIME 1

// Refresh intervals - can be more frequent due to better power management
#define REFRESH_MIN_INTERVAL_SECONDS            (10 * SECONDS_IN_MINUTE)
#define REFRESH_MAX_INTERVAL_SECONDS            (4 * SECONDS_IN_HOUR)
#define REFRESH_STEP_SECONDS                    (10 * SECONDS_IN_MINUTE)
#define REFRESH_INTERVAL_SECONDS_LOW_BATTERY    (8 * SECONDS_IN_HOUR)

#define DAY_START_HOUR                       06 // Hour when the day starts (6 AM)
#define DAY_END_HOUR                         23 // Hour when the day ends (11 PM)

#define FONT_HEADER                          "assets/fonts/Ubuntu_R.h"

// LOCALE
#define LOCALE it_IT // Default to Italian for new configuration

// Google Drive integration - fully enabled since no I2C/WiFi conflicts
// Google Drive is now enabled globally in config.h
#define GOOGLE_DRIVE_CONFIG_FILEPATH "/google_drive_config.json"

// Service account cleanup interval
#define CLEANUP_TEMP_FILES_INTERVAL_SECONDS (24 * 60 * 60) // 24 hours

#define GOOGLE_DRIVE_STREAM_PARSER_THRESHOLD    2097152 // 2MB - avoid streaming for most responses
#define GOOGLE_DRIVE_JSON_DOC_SIZE              2097152 // 2MB JSON document buffer
#define GOOGLE_DRIVE_BODY_RESERVE_SIZE          2097152 // 2MB response body reserve
#define GOOGLE_DRIVE_SAFETY_LIMIT               2097152 // 2MB safety limit

#define GOOGLE_DRIVE_MAX_LIST_PAGE_SIZE 250 // Max 1000, but 250 is a good compromise

// Comments about FeatherS3 advantages:
// 1. No I2C/WiFi coexistence issues (unlike ESP32-C6)
// 2. Built-in USB-C and battery charging
// 3. 8MB PSRAM for better image processing
// 4. More stable network operations
// 5. Better deep sleep wakeup reliability
// 6. Native support for concurrent I2C and WiFi operations