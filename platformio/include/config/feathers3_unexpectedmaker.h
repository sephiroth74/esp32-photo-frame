#include <Arduino.h>

// Configuration for Unexpected Maker FeatherS3 (ESP32-S3)
// Pin definitions based on FeatherS3 pinout
// https://feathers3.io/pinout.html

// #define USE_SHARED_SPI // Use shared SPI for SD card and display

// Pin definitions for FeatherS3
// SD Card - using SPI pins
#define SD_CS_PIN   5    // Available digital pin
#define SD_MISO_PIN 37   // SPI MISO
#define SD_MOSI_PIN 35   // SPI MOSI  
#define SD_SCK_PIN  36   // SPI SCK

// e-Paper Display - using SPI pins (shared with SD card)
#define EPD_BUSY_PIN 6   // Available digital pin
#define EPD_RST_PIN  7   // Available digital pin
#define EPD_DC_PIN   8   // Available digital pin  
#define EPD_CS_PIN   4   // Available digital pin
#define EPD_SCK_PIN  36  // Shared SPI SCK
#define EPD_MOSI_PIN 35  // Shared SPI MOSI
#define EPD_MISO_PIN 37  // Shared SPI MISO

// Potentiometer pin - using analog pins
#define POTENTIOMETER_PWR_PIN   10   // Digital pin for power control
#define POTENTIOMETER_INPUT_PIN A0   // Analog pin A0 (GPIO1)
#define POTENTIOMETER_INPUT_MAX 4095 // 12-bit ADC

// Battery monitoring - FeatherS3 has built-in battery voltage divider
#define BATTERY_PIN                    A13  // Built-in battery voltage pin (GPIO13)
#define BATTERY_NUM_READINGS           100
#define BATTERY_DELAY_BETWEEN_READINGS 10
#define BATTERY_RESISTORS_RATIO        0.5   // FeatherS3 built-in divider ratio

// RTC module - ESP32-S3 doesn't have I2C/WiFi issues, so RTC is enabled
#define USE_RTC
#define RTC_SDA_PIN 8    // I2C SDA (can share with EPD_DC_PIN with proper sequencing)
#define RTC_SCL_PIN 9    // I2C SCL

// Alternative I2C pins if conflicts arise
// #define RTC_SDA_PIN 41  // Alternative I2C SDA  
// #define RTC_SCL_PIN 40  // Alternative I2C SCL

// MAX1704X battery sensor option (alternative to voltage divider)
// #define USE_SENSOR_MAX1704X
// #define SENSOR_MAX1704X_TIMEOUT 10000
// #define MAX1704X_SDA_PIN 8   // Share I2C with RTC
// #define MAX1704X_SCL_PIN 9   // Share I2C with RTC

// Built-in LED - FeatherS3 uses RGB LED on pin 40
#ifdef LED_BUILTIN
#undef LED_BUILTIN
#endif // LED_BUILTIN

#define LED_BUILTIN 13  // FeatherS3 RGB LED (NeoPixel)

// External wakeup configuration
#define WAKEUP_EXT1
// Use available RTC GPIO pin for wakeup
#define WAKEUP_PIN      GPIO_NUM_3
#define WAKEUP_PIN_MODE INPUT_PULLDOWN  
#define WAKEUP_LEVEL    ESP_EXT1_WAKEUP_ANY_HIGH

// ESP32-S3 has many RTC IO pins available for deep sleep wakeup
// We're using GPIO3 which is definitely an RTC IO pin

// Power saving configuration
#define BATTERY_POWER_SAVING

// Delay before going to sleep in milliseconds
#define DELAY_BEFORE_SLEEP 10000  // Reduced since no I2C/WiFi conflicts

// WiFi configuration
#define WIFI_FILENAME "/wifi.txt"

// SD Card configuration
#define TOC_FILENAME "/toc.txt"

// Network timeouts - can be more aggressive since ESP32-S3 is more stable
#define WIFI_CONNECT_TIMEOUT 8000   // Reduced timeout
#define TIMEZONE             "CET-1CEST,M3.5.0,M10.5.0"
#define NTP_TIMEOUT          10000  // Reduced timeout
#define NTP_SERVER1          "pool.ntp.org"
#define NTP_SERVER2          "time.nist.gov"

// e-Paper display configuration
#define DISP_BW_V2 // Black and White e-Paper display
#define USE_DESPI_DRIVER

// Accent color for the display
#define ACCENT_COLOR GxEPD_BLACK

// Miscellaneous
#define DEBUG_MODE 1
#define EPD_USE_BINARY_FILE

// Reset handling
#define RESET_INVALIDATES_DATE_TIME 1

// Refresh intervals - can be more frequent due to better power management
#define REFRESH_MIN_INTERVAL_SECONDS         (5 * SECONDS_IN_MINUTE)   // More frequent
#define REFRESH_MAX_INTERVAL_SECONDS         (4 * SECONDS_IN_HOUR)
#define REFRESH_STEP_SECONDS                 (5 * SECONDS_IN_MINUTE)   // Finer steps
#define REFRESH_INTERVAL_SECONDS_LOW_BATTERY (6 * SECONDS_IN_HOUR)    // Less conservative

#define DAY_START_HOUR                       06 // Hour when the day starts (6 AM)
#define DAY_END_HOUR                         23 // Hour when the day ends (11 PM)

#define FONT_HEADER                          "assets/fonts/Ubuntu_R.h"

// LOCALE
#define LOCALE en_US  // Default to English for new configuration

// Google Drive integration - fully enabled since no I2C/WiFi conflicts
// Google Drive is now enabled globally in config.h
#define GOOGLE_DRIVE_CONFIG_FILEPATH "/google_drive_config.json"

// Service account cleanup interval
#define CLEANUP_TEMP_FILES_INTERVAL_SECONDS (24 * 60 * 60) // 24 hours

// ESP32-S3 specific optimizations
// Take advantage of PSRAM for better performance with large images
#ifndef CONFIG_SPIRAM_SUPPORT
#define CONFIG_SPIRAM_SUPPORT
#endif // CONFIG_SPIRAM_SUPPORT

#ifndef CONFIG_SPIRAM_USE_MALLOC
#define CONFIG_SPIRAM_USE_MALLOC
#endif // CONFIG_SPIRAM_USE_MALLOC

// Comments about FeatherS3 advantages:
// 1. No I2C/WiFi coexistence issues (unlike ESP32-C6)
// 2. Built-in USB-C and battery charging
// 3. 8MB PSRAM for better image processing
// 4. More stable network operations
// 5. Better deep sleep wakeup reliability
// 6. Native support for concurrent I2C and WiFi operations