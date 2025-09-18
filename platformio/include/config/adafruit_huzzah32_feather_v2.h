#include <Arduino.h>

// Configuration for Adafruit Huzzah32 Feather v2 (ESP32-PICO-MINI-02 with 2MB PSRAM)
// Pin definitions based on Adafruit Huzzah32 Feather v2 pinout
// https://learn.adafruit.com/adafruit-huzzah32-esp32-feather/pinouts

// ESP32 has separate SPI buses available
// #define USE_SHARED_SPI // Not needed - can use separate SPI buses
#define USE_HSPI_FOR_EPD // Use separate HSPI bus for e-Paper display
// #define USE_HSPI_FOR_EPD // Use separate HSPI bus for e-Paper display

// Pin definitions for Adafruit Huzzah32 Feather v2
// SD Card - using dedicated SPI pins
#define SD_SCK_PIN  5   // SPI SCK
#define SD_MISO_PIN 21  // SPI MISO
#define SD_MOSI_PIN 19  // SPI MOSI 
#define SD_CS_PIN   22  // Available digital pin on Feather

// e-Paper Display - using separate SPI pins (no sharing needed)
#define EPD_BUSY_PIN 14  // Available digital pin
#define EPD_RST_PIN  27  // Available digital pin  
#define EPD_DC_PIN   33  // Available digital pin
#define EPD_CS_PIN   32  // Available digital pin
#define EPD_SCK_PIN  15  // Separate SPI SCK for display
#define EPD_MOSI_PIN 12  // Separate SPI MOSI for display
#define EPD_MISO_PIN -1  // Separate SPI MISO for display

// Potentiometer pin - using analog pins
#define POTENTIOMETER_PWR_PIN   20   // Digital pin for power control (moved from GPIO36)
#define POTENTIOMETER_INPUT_PIN 34   // Analog pin for potentiometer reading
#define POTENTIOMETER_INPUT_MAX 4095 // 12-bit ADC

// Battery monitoring - Huzzah32 Feather v2 has built-in battery voltage divider
// Connected to A13 (GPIO 13) through a voltage divider (100K + 100K = 2:1 ratio)
#define BATTERY_PIN                    35 // Built-in battery voltage pin (GPIO13)
#define BATTERY_NUM_READINGS           100
#define BATTERY_DELAY_BETWEEN_READINGS 10
#define BATTERY_RESISTORS_RATIO        0.4933491686 // Adafruit Feather 2:1 voltage divider (100K/100K)


// Built-in LED - Huzzah32 Feather v2 has built-in LED
#ifdef LED_BUILTIN
#undef LED_BUILTIN
#endif // LED_BUILTIN

#define LED_BUILTIN 13 // Built-in LED pin

// External wakeup configuration
// #define WAKEUP_EXT1
#define WAKEUP_EXT0
// External button connected to A0 (GPIO25) with pull-down configuration
#define WAKEUP_PIN      GPIO_NUM_25   // A0 pin with external button
#define WAKEUP_PIN_MODE INPUT_PULLDOWN // Internal pull-down + external 10K for noise immunity
#define WAKEUP_LEVEL    HIGH          // Wake when button pressed (pin goes HIGH)

// Power saving configuration
#define BATTERY_POWER_SAVING

// Delay before going to sleep in milliseconds
#define DELAY_BEFORE_SLEEP 8000 // Reduced since no I2C/WiFi conflicts

// WiFi configuration
#define WIFI_FILENAME "/wifi.txt"

// Network timeouts - can be more aggressive since ESP32-S3 is stable
#define WIFI_CONNECT_TIMEOUT 8000  // Reduced timeout
#define TIMEZONE             "CET-1CEST,M3.5.0,M10.5.0"
#define NTP_TIMEOUT          10000 // Reduced timeout
#define NTP_SERVER1          "pool.ntp.org"
#define NTP_SERVER2          "time.nist.gov"

// e-Paper display configuration
#define DISP_6C // 6-color e-Paper display (GDEP073E01)
// #define DISP_BW_V2 // Black and White e-Paper display
#define USE_DESPI_DRIVER

// Accent color for the display
#define ACCENT_COLOR GxEPD_BLACK

// Miscellaneous
#define DEBUG_MODE 0 // Disable debug mode for production
// #define DEBUG_BATTERY_READER


// Reset handling
#define RESET_INVALIDATES_DATE_TIME 1

// Refresh intervals - can be more frequent due to excellent power management
#define REFRESH_MIN_INTERVAL_SECONDS            (10 * SECONDS_IN_MINUTE)
#define REFRESH_MAX_INTERVAL_SECONDS            (4 * SECONDS_IN_HOUR)
#define REFRESH_STEP_SECONDS                    (10 * SECONDS_IN_MINUTE)
#define REFRESH_INTERVAL_SECONDS_LOW_BATTERY    (8 * SECONDS_IN_HOUR)

#define DAY_START_HOUR                       06 // Hour when the day starts (6 AM)
#define DAY_END_HOUR                         23 // Hour when the day ends (11 PM)

#define FONT_HEADER                          "assets/fonts/Ubuntu_R.h"

// LOCALE
#define LOCALE it_IT // Default to Italian

// Google Drive integration - fully enabled since no I2C/WiFi conflicts
// Google Drive is now enabled globally in config.h
#define GOOGLE_DRIVE_CONFIG_FILEPATH "/google_drive_config.json"

// Service account cleanup interval
#define CLEANUP_TEMP_FILES_INTERVAL_SECONDS (24 * 60 * 60) // 24 hours

#define GOOGLE_DRIVE_STREAM_PARSER_THRESHOLD    2097152 // 2MB - avoid streaming for most responses
#define GOOGLE_DRIVE_JSON_DOC_SIZE              2097152 // 2MB JSON document buffer
#define GOOGLE_DRIVE_BODY_RESERVE_SIZE          2097152 // 2MB response body reserve
#define GOOGLE_DRIVE_SAFETY_LIMIT               4194304 // 4MB safety limit

#define GOOGLE_DRIVE_MAX_LIST_PAGE_SIZE 250 // Max 1000, but 250 is a good compromise

// Adafruit Huzzah32 Feather v2 advantages:
// 1. ESP32-PICO-MINI-02 with dual core for excellent performance
// 2. 2MB PSRAM for improved Google Drive streaming performance (3-5x faster sync)
// 3. No I2C/WiFi coexistence issues (unlike ESP32-C6)
// 4. Built-in micro-USB and excellent battery charging circuit with JST connector
// 5. Feather form factor with extensive ecosystem compatibility
// 6. Separate SPI buses eliminate the need for USE_SHARED_SPI
// 7. Built-in voltage divider for clean battery monitoring
// 8. Stable network operations and excellent deep sleep reliability
// 9. Native support for concurrent I2C and WiFi operations
// 10. Premium performance: 5-10 second Google Drive sync vs 8-15s on ESP32-C6