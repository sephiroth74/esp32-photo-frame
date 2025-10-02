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

#ifndef __PHOTO_FRAME_CONFIG_H__
#define __PHOTO_FRAME_CONFIG_H__

#include "_locale.h"
#include <Arduino.h>

// ============================================================================
// MACRO UTILITIES
// ============================================================================

/// String conversion macros for preprocessor constants
#define STR(x)  #x
#define XSTR(x) STR(x)

// ============================================================================
// FIRMWARE VERSION INFORMATION
// ============================================================================

/// Current firmware version components (used for version comparison and OTA updates)
#define FIRMWARE_VERSION_MAJOR  0
#define FIRMWARE_VERSION_MINOR  8
#define FIRMWARE_VERSION_PATCH  1
#define FIRMWARE_VERSION_STRING "v0.8.1"

/// Minimum supported version for OTA compatibility
/// Devices with firmware older than this version must be manually updated
#define OTA_MIN_SUPPORTED_VERSION_MAJOR 0
#define OTA_MIN_SUPPORTED_VERSION_MINOR 4
#define OTA_MIN_SUPPORTED_VERSION_PATCH 0

// ============================================================================
// HARDWARE CONFIGURATION
// ============================================================================

// ----------------------------------------------------------------------------
// E-Paper Display Configuration
// ----------------------------------------------------------------------------

/// E-Paper display pin definitions (to be set in board-specific config files)
/// These pins connect the ESP32 to the e-paper display via SPI interface
// #define EPD_BUSY_PIN  [GPIO_NUM]  // Display busy status pin
// #define EPD_RST_PIN   [GPIO_NUM]  // Display reset pin
// #define EPD_DC_PIN    [GPIO_NUM]  // Display data/command pin
// #define EPD_CS_PIN    [GPIO_NUM]  // Display chip select pin (SPI CS)
// #define EPD_SCK_PIN   [GPIO_NUM]  // Display serial clock pin (SPI CLK)
// #define EPD_MOSI_PIN  [GPIO_NUM]  // Display master out slave in pin (SPI MOSI)

/// Display type selection (define exactly ONE of these in board config)
// #define DISP_BW_V2    // Black and White e-Paper display (GDEH0154D67)
// #define DISP_7C_F     // 7-color e-Paper display (GDEY073D46)
// #define DISP_6C       // 6-color e-Paper display (GDEP073E01)

/// Display driver selection (optional - system will auto-detect if not specified)
// #define USE_DESPI_DRIVER      // Use Despi driver for e-Paper display
// #define USE_WAVESHARE_DRIVER  // Use Waveshare driver for e-Paper display

// ----------------------------------------------------------------------------
// SD Card Configuration (SDIO Interface)
// ----------------------------------------------------------------------------

/// SD Card pin definitions using SD_MMC (SDIO interface) with available pins
/// Note: These pins are typically fixed for SDIO interface on most ESP32 boards
// #define SD_MMC_CLK_PIN  14  // SD card clock pin
// #define SD_MMC_D0_PIN   7   // SD card data 0 pin
// #define SD_MMC_CMD_PIN  17  // SD card command pin
// #define SD_MMC_D3_PIN   11  // SD card data 3 pin (chip select)
// #define SD_MMC_D1_PIN   3   // SD card data 1 pin
// #define SD_MMC_D2_PIN   12  // SD card data 2 pin

// ----------------------------------------------------------------------------
// Battery Monitoring Configuration
// ----------------------------------------------------------------------------

/// MAX1704X I2C battery sensor configuration (preferred method)
/// Enable this for accurate battery monitoring with dedicated sensor
// #define USE_SENSOR_MAX1704X     // Enable MAX1704X battery fuel gauge sensor
// #define MAX1704X_SDA_PIN   A1   // I2C data pin for MAX1704X sensor
// #define MAX1704X_SCL_PIN   A2   // I2C clock pin for MAX1704X sensor

/// Analog battery monitoring (fallback method when MAX1704X not available)
/// Used when USE_SENSOR_MAX1704X is not defined
// #define BATTERY_PIN                    A0           // Analog pin for battery voltage reading
// #define BATTERY_NUM_READINGS           100          // Number of readings to average for
// stability #define BATTERY_DELAY_BETWEEN_READINGS 30           // Delay between readings in
// milliseconds #define BATTERY_RESISTORS_RATIO        0.460453401  // Voltage divider ratio
// (R1/(R1+R2))

/// Battery power management settings
// #define BATTERY_POWER_SAVING  // Enable power-saving mode when battery is low

// ----------------------------------------------------------------------------
// RTC (Real Time Clock) Configuration
// ----------------------------------------------------------------------------

/// External RTC module configuration (optional - NTP used as fallback)
/// Note: RTC is not supported on ESP32-C6 due to I2C/WiFi interference
// #define USE_RTC           // Enable external RTC module for timekeeping
// #define RTC_SDA_PIN   A4  // I2C data pin for RTC module
// #define RTC_SCL_PIN   A5  // I2C clock pin for RTC module
// #define RTC_CLASS_PCF8523  // Uncomment if using PCF8523 RTC
// #define RTC_CLASS_DS3231   // Uncomment if using DS3231 RTC

// ----------------------------------------------------------------------------
// User Interface Hardware
// ----------------------------------------------------------------------------

/// Potentiometer configuration for refresh rate adjustment
// #define POTENTIOMETER_PWR_PIN    A6    // Power pin for potentiometer (optional)
// #define POTENTIOMETER_INPUT_PIN  A7    // Analog input pin for potentiometer
// #define POTENTIOMETER_INPUT_MIN  0     // Minimum ADC value (typically 0)
// #define POTENTIOMETER_INPUT_MAX  4095  // Maximum ADC value (12-bit: 4095, 10-bit: 1023)

/// RGB NeoPixel LED configuration for status indication
// #define RGB_STATUS_ENABLED        // Enable RGB status LED functionality
// #define RGB_LED_PIN       40      // GPIO pin connected to RGB NeoPixel LED
// #define RGB_LED_COUNT     1       // Number of RGB LEDs in the chain

/// Wake-up pin configuration for manual device activation
// #define WAKEUP_EXT0               // Enable EXT0 wakeup (single pin)
// #define WAKEUP_EXT1               // Enable EXT1 wakeup (multiple pins, OR logic)
// #define WAKEUP_PIN        GPIO_NUM_7        // GPIO pin for wakeup functionality
// #define WAKEUP_PIN_MODE   INPUT_PULLDOWN    // Pin mode (INPUT_PULLUP/INPUT_PULLDOWN)
// #define WAKEUP_LEVEL      ESP_EXT1_WAKEUP_ANY_HIGH  // Wakeup trigger level

// ============================================================================
// TIMING AND POWER MANAGEMENT
// ============================================================================

/// System timing constants
#define MICROSECONDS_IN_SECOND 1000000
#define SECONDS_IN_MINUTE      60
#define SECONDS_IN_HOUR        3600
#define SECONDS_IN_DAY         86400

/// Deep sleep configuration
#define MAX_DEEP_SLEEP_SECONDS SECONDS_IN_DAY // Maximum sleep duration (24 hours)

/// Display refresh timing configuration
#define REFRESH_MIN_INTERVAL_SECONDS (5 * SECONDS_IN_MINUTE)   // Minimum refresh interval (5 minutes) 
#define REFRESH_MAX_INTERVAL_SECONDS (4 * SECONDS_IN_HOUR)     // Maximum refresh interval (2 hours) 
#define REFRESH_STEP_SECONDS         (5 * SECONDS_IN_MINUTE)   // Step size for refresh adjustment 
#define REFRESH_INTERVAL_SECONDS_CRITICAL_BATTERY (6 * SECONDS_IN_HOUR)  // Critical battery refresh interval
#define REFRESH_INTERVAL_LOW_BATTERY_MULTIPLIER 3 // Multiplier for low battery refresh interval

/// Daily operation schedule
#define DAY_START_HOUR 5   // Hour when device becomes active (5 AM)
#define DAY_END_HOUR   23  // Hour when device enters night mode (11 PM)

/// Sleep and startup delays
// #define DELAY_BEFORE_SLEEP 20000  // Delay before sleep in milliseconds (debug/fallback)

/// Battery level thresholds (percentages)
#define BATTERY_CHARGING_MILLIVOLTS 4300 // Voltage above which battery is considered charging
#define BATTERY_PERCENT_EMPTY       5    // Battery percentage considered empty
#define BATTERY_PERCENT_CRITICAL    10   // Battery percentage considered critical
#define BATTERY_PERCENT_LOW         25   // Battery percentage considered low

// ============================================================================
// NETWORK AND CONNECTIVITY
// ============================================================================

/// WiFi configuration (now handled by unified config system)

/// Network timeouts (configured in board-specific config or system defaults section)
#define WIFI_CONNECT_TIMEOUT 8000  // WiFi connection timeout in milliseconds
#define HTTP_CONNECT_TIMEOUT 15000  // HTTP connection timeout (15 seconds)
#define HTTP_REQUEST_TIMEOUT 30000  // HTTP request timeout (30 seconds)

/// NTP (Network Time Protocol) configuration (configured in board-specific config or system
/// defaults section)
#define NTP_TIMEOUT 10000           // NTP request timeout in milliseconds
#define NTP_SERVER1 "pool.ntp.org"  // Primary NTP server
#define NTP_SERVER2 "time.nist.gov" // Secondary NTP server

/// Time zone configuration (set in board-specific config)
// #define TIMEZONE "CET-1CEST,M3.5.0,M10.5.0/3"  // Example: Central European Time

/// Protocol-specific settings
#if defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32H2)
#define USE_HTTP_1_0 // Force HTTP/1.0 to avoid chunked encoding issues on ESP32-C6/H2
#endif

// ============================================================================
// STORAGE AND FILE MANAGEMENT
// ============================================================================

/// File system paths and names
#define PREFS_NAMESPACE          "photo_frame"       // Preferences namespace for settings
#define TOC_DATA_FILENAME        "toc_data.txt"      // Table of contents data file
#define TOC_META_FILENAME        "toc_meta.txt"      // Table of contents metadata file
#define ACCESS_TOKEN_FILENAME    "access_token.json" // OAuth access token cache file
#define LITTLEFS_TEMP_IMAGE_FILE "/temp_image.tmp"   // Temporary image file in LittleFS

/// Storage cleanup settings
#define SD_CARD_FREE_SPACE_THRESHOLD        (1024 * 1024 * 16) // 16 MB threshold for cleanup
#define CLEANUP_TEMP_FILES_INTERVAL_SECONDS (24 * SECONDS_IN_HOUR)     // Cleanup interval (24 hours)

/// Supported file formats for runtime detection
extern const char* ALLOWED_FILE_EXTENSIONS[];
extern const size_t ALLOWED_EXTENSIONS_COUNT;

// ===========================================================================
// GLOBAL CONFIGURATION
// ============================================================================
#define CONFIG_FILEPATH "/config.json"  // Global configuration file


// ============================================================================
// GOOGLE DRIVE CONFIGURATION
// ============================================================================

/// Google Drive integration settings (configured in board-specific config or system defaults section)
#define GOOGLE_DRIVE_TOC_MAX_AGE_SECONDS (30 * SECONDS_IN_DAY)           // TOC cache expiry (30 days) 
#define GOOGLE_DRIVE_TEMP_DIR            "temp"                          // Temporary files directory
#define GOOGLE_DRIVE_CACHE_DIR           "cache"                         // Cache directory
#define GOOGLE_DRIVE_CACHING_LOCAL_PATH  "/gdrive"                       // Local cache path on SD card

/// API rate limiting and request management (configured in board-specific config or system defaults
/// section)
#define GOOGLE_DRIVE_MAX_REQUESTS_PER_WINDOW     200      // Conservative API request limit
#define GOOGLE_DRIVE_RATE_LIMIT_WINDOW_SECONDS   3600     // Rate limiting time window (1 hour)
#define GOOGLE_DRIVE_MIN_REQUEST_DELAY_MS        10000    // Minimum delay between requests
#define GOOGLE_DRIVE_MAX_RETRY_ATTEMPTS          10       // Maximum retry attempts for failed requests
#define GOOGLE_DRIVE_BACKOFF_BASE_DELAY_MS       60000    // Base delay for exponential backoff 
#define GOOGLE_DRIVE_BACKOFF_MAX_DELAY_MS        120000UL // Maximum backoff delay
#define GOOGLE_DRIVE_MAX_WAIT_TIME_MS            1200000  // Maximum wait time for rate limiting
#define GOOGLE_DRIVE_MAX_LIST_PAGE_SIZE          500      // Files per API request

/// Memory management - Platform specific (PSRAM vs Standard ESP32, configured in system defaults section)
// Memory limits are automatically set based on BOARD_HAS_PSRAM in the system defaults section
// ESP32-S3 with PSRAM: 4MB JSON buffer, 6MB body reserve, 10MB safety limit
// Standard ESP32: 40KB JSON buffer, 64KB body reserve, 100KB safety limit

/// Development and testing
// #define GOOGLE_DRIVE_TEST_FILE "combined_portrait_214.bin"  // Force specific test file

// ============================================================================
// WEATHER DISPLAY CONFIGURATION
// ============================================================================

/// Weather functionality (optional feature)
#define WEATHER_CACHE_FILE  "/weather_cache.json"   // Weather data cache file

/// Weather data management
#define WEATHER_MAX_AGE_SECONDS (6 * SECONDS_IN_HOUR) // Weather data expiry time

// ============================================================================
// DISPLAY AND USER INTERFACE
// ============================================================================

/// Font configuration
// #define FONT_HEADER "assets/fonts/Ubuntu_R.h"          // Default font
// Available font options:
// - "assets/fonts/UbuntuMono_R.h"      // Monospace font
// - "assets/fonts/FreeMono.h"          // Free monospace font
// - "assets/fonts/FreeSans.h"          // Free sans-serif font
// - "assets/fonts/Lato_Regular.h"      // Lato regular font
// - "assets/fonts/Montserrat_Regular.h" // Montserrat regular font
// - "assets/fonts/OpenSans_Regular.h"  // Open Sans regular font
// - "assets/fonts/Poppins_Regular.h"   // Poppins regular font
// - "assets/fonts/Quicksand_Regular.h" // Quicksand regular font
// - "assets/fonts/Raleway_Regular.h"   // Raleway regular font
// - "assets/fonts/Roboto_Regular.h"    // Roboto regular font
// - "assets/fonts/RobotoMono_Regular.h" // Roboto monospace font
// - "assets/fonts/RobotoSlab_Regular.h" // Roboto slab serif font

/// Display color configuration (automatically determined from display type)
// #define ACCENT_COLOR GxEPD_BLACK  // Accent color for UI elements

/// Potentiometer input range (platform-specific)
#define POTENTIOMETER_INPUT_MAX 4095 // 12-bit ADC max value (ESP32 default)

// ============================================================================
// LOCALIZATION
// ============================================================================

/// Language and locale settings
// #define LOCALE it_IT  // Italian (Italy)
// #define LOCALE en_US  // English (United States) - default

/// Supported locales:
/// - en_US: English (United States)
/// - it_IT: Italiano (Italia)

// ============================================================================
// DEBUG AND DEVELOPMENT SETTINGS
// ============================================================================

/// Debug mode configuration (disable in production)
// #define DEBUG_MODE 1              // General debug mode (0 = off, 1 = on)

/// Component-specific debug flags
// #define DEBUG_RENDERER            // Enable renderer debug output
// #define DEBUG_DATETIME_UTILS      // Enable date/time utilities debug output
// #define DEBUG_GOOGLE_DRIVE        // Enable Google Drive API debug output
// #define DEBUG_MEMORY_USAGE        // Enable memory usage monitoring
// #define DEBUG_BATTERY_READER      // Enable battery monitoring debug output
// #define DEBUG_WEATHER             // Enable weather API debug output
// #define DEBUG_WIFI                // Enable WiFi connection debug output
// #define DEBUG_SD_CARD             // Enable SD card operations debug output
// #define DEBUG_BOARD               // Enable board-specific debug output
// #define DEBUG_OTA                 // Enable OTA update debug output

/// Debug-specific settings
// #define DEBUG_IMAGE_INDEX 381     // Force specific image for testing
#define RESET_INVALIDATES_DATE_TIME 1  // Reset RTC time on device reset

// ============================================================================
// SYSTEM DEFAULTS AND VALIDATION
// ============================================================================

/// Include board-specific configuration (set via platformio.ini build flags)
#ifndef LOCAL_CONFIG_FILE
#error LOCAL_CONFIG_FILE not defined
#endif
#include XSTR(LOCAL_CONFIG_FILE)

// ============================================================================
// OTA (Over-The-Air) UPDATE CONFIGURATION
// ============================================================================

/// OTA update system (conditional compilation)
/// Enable OTA functionality by defining OTA_UPDATE_ENABLED in board config
#ifdef OTA_UPDATE_ENABLED

/// OTA server endpoints
#define OTA_SERVER_URL         "https://api.github.com/repos/sephiroth74/esp32-photo-frame"
#define OTA_VERSION_ENDPOINT   "/releases/latest"
#define OTA_FIRMWARE_ENDPOINT  "/releases/download/{version}/firmware-{board}.bin"
#define OTA_MANIFEST_URL       "https://github.com/sephiroth74/esp32-photo-frame/releases/latest/download/ota_manifest.json"

/// OTA timing and safety settings
#define OTA_CHECK_INTERVAL_HOURS 168   // Check interval (7 days)
#define OTA_MIN_BATTERY_PERCENT  30    // Minimum battery level for OTA updates
#define OTA_TIMEOUT_MS           30000 // 30 seconds timeout
#define OTA_BUFFER_SIZE          1024  // 1KB buffer for download chunks

/// OTA security settings
// #define OTA_USE_SSL false  // SSL usage for OTA downloads (set to true for production)

/// Board identification (automatically set from platformio.ini build flags)
#ifdef OTA_BOARD_NAME
#define OTA_CURRENT_BOARD_NAME OTA_BOARD_NAME
#else
#define OTA_CURRENT_BOARD_NAME "unknown"
#endif // OTA_BOARD_NAME
#endif // OTA_UPDATE_ENABLED

#ifdef OTA_UPDATE_ENABLED
#ifndef OTA_USE_SSL
#define OTA_USE_SSL false // Default to true for security
#endif                    // OTA_USE_SSL
#endif                    // OTA_UPDATE_ENABLED

/// Default value definitions with validation guards
#ifndef BATTERY_CHARGING_MILLIVOLTS
#define BATTERY_CHARGING_MILLIVOLTS 4300
#endif

#ifndef BATTERY_PERCENT_EMPTY
#define BATTERY_PERCENT_EMPTY 5
#endif

#ifndef BATTERY_PERCENT_CRITICAL
#define BATTERY_PERCENT_CRITICAL 10
#endif

#ifndef BATTERY_PERCENT_LOW
#define BATTERY_PERCENT_LOW 25
#endif

#ifndef DELAY_BEFORE_SLEEP
#define DELAY_BEFORE_SLEEP 20000
#endif

#ifndef LOCALE
#define LOCALE en_US
#endif

#ifdef USE_SENSOR_MAX1704X
#ifndef SENSOR_MAX1704X_TIMEOUT
#define SENSOR_MAX1704X_TIMEOUT 5000
#endif
#endif

#ifndef FONT_HEADER
#define FONT_HEADER "assets/fonts/Ubuntu_R.h"
#endif

#ifndef SD_CARD_FREE_SPACE_THRESHOLD
#define SD_CARD_FREE_SPACE_THRESHOLD (1024 * 1024 * 16) // 16 MB
#endif

#ifndef POTENTIOMETER_INPUT_MAX
#define POTENTIOMETER_INPUT_MAX 4095
#endif

/// Flash memory size detection for optimal buffer sizing
#ifndef FLASH_SIZE_MB
    // Detect flash size based on build configuration or board type
    #if defined(ARDUINO_ESP32S3_DEV) || defined(ESP32S3)
        // ESP32-S3 boards typically have 8MB+ flash
        #define FLASH_SIZE_MB 16
    #elif defined(ARDUINO_ESP32_DEV) || defined(ESP32)
        // ESP32 boards typically have 4MB flash
        #define FLASH_SIZE_MB 4
    #else
        // Default assumption
        #define FLASH_SIZE_MB 8
    #endif
#endif

/// Google Drive memory management - scaled based on flash memory and PSRAM availability
#ifndef GOOGLE_DRIVE_JSON_DOC_SIZE
    #if FLASH_SIZE_MB >= 16
        #define GOOGLE_DRIVE_JSON_DOC_SIZE 4194304 // 4MB JSON document buffer for 16MB+ flash
    #elif FLASH_SIZE_MB >= 8
        #define GOOGLE_DRIVE_JSON_DOC_SIZE 2097152 // 2MB JSON document buffer for 8MB+ flash
    #else
        #define GOOGLE_DRIVE_JSON_DOC_SIZE 1048576 // 1MB JSON document buffer for smaller flash
    #endif
#endif

#ifndef GOOGLE_DRIVE_BODY_RESERVE_SIZE
    #if FLASH_SIZE_MB >= 16
        #define GOOGLE_DRIVE_BODY_RESERVE_SIZE 4194304 // 4MB response body reserve for 16MB+ flash
    #elif FLASH_SIZE_MB >= 8
        #define GOOGLE_DRIVE_BODY_RESERVE_SIZE 2097152 // 2MB response body reserve for 8MB+ flash
    #else
        #define GOOGLE_DRIVE_BODY_RESERVE_SIZE 1048576 // 1MB response body reserve for smaller flash
    #endif
#endif

#ifndef GOOGLE_DRIVE_SAFETY_LIMIT
    #if FLASH_SIZE_MB >= 16
        #define GOOGLE_DRIVE_SAFETY_LIMIT 12582912 // 12MB safety limit for 16MB+ flash
    #elif FLASH_SIZE_MB >= 8
        #define GOOGLE_DRIVE_SAFETY_LIMIT 6291456  // 6MB safety limit for 8MB+ flash
    #else
        #define GOOGLE_DRIVE_SAFETY_LIMIT 3145728  // 3MB safety limit for smaller flash
    #endif
#endif

/// Display accent color determination (based on display type)
#ifndef ACCENT_COLOR
#if defined(DISP_BW_V2)
#define ACCENT_COLOR GxEPD_BLACK // Black for B&W displays
#elif defined(DISP_7C_F)
#define ACCENT_COLOR GxEPD_RED // Red for 7-color displays
#elif defined(DISP_6C)
#define ACCENT_COLOR GxEPD_RED // Red for 6-color displays
#else
#define ACCENT_COLOR GxEPD_BLACK // Default to black
#endif
#endif

#endif // __PHOTO_FRAME_CONFIG_H__