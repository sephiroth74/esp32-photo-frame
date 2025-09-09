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

#define STR(x)  #x
#define XSTR(x) STR(x)

/// ---- Customizable settings ----

// #define USE_HSPI_FOR_SD  // Use HSPI for SD card communication
// #define USE_HSPI_FOR_EPD // Use HSPI for e-Paper display
// #define USE_SHARED_SPI   // Use shared SPI for all devices

// Pin definitions

// -------------------------------------------
// SD Card
// -------------------------------------------
// #define SD_CS_PIN D3
// #define SD_MISO_PIN D5
// #define SD_MOSI_PIN D4
// #define SD_SCK_PIN D6

// -------------------------------------------
// e-Paper Display
// -------------------------------------------

// #define EPD_BUSY_PIN D7
// #define EPD_RST_PIN D8
// #define EPD_DC_PIN D9
// #define EPD_CS_PIN D10
// #define EPD_SCK_PIN D11
// #define EPD_MOSI_PIN D12

// -------------------------------------------
// RTC
// -------------------------------------------

// Use RTC module for timekeeping
// Uncomment the following line to use RTC module
// If this is set, then RTC_SDA_PIN and RTC_SCL_PIN must be defined
// #define USE_RTC
// #define RTC_SDA_PIN A4
// #define RTC_SCL_PIN A5

// -------------------------------------------
// Potentiometer
// -------------------------------------------
// #define POTENTIOMETER_PWR_PIN A6
// #define POTENTIOMETER_INPUT_PIN A7
// Maximum value for potentiometer input (12-bit ADC)
// #define POTENTIOMETER_INPUT_MIN 0
// #define POTENTIOMETER_INPUT_MAX 4095
// #define POTENTIOMETER_INPUT_MAX 1023 // Default for other platforms (10-bit ADC)

// -------------------------------------------
// Battery
// -------------------------------------------

// Use MAX1704X battery sensor
// Uncomment the following line to use MAX1704X battery sensor
// Use MAX1704 battery sensor
// #define USE_SENSOR_MAX1704X
// Timeout for MAX1704X sensor initialization in milliseconds
// #define SENSOR_MAX1704X_TIMEOUT 5000
// #define MAX1704X_SDA_PIN A1
// #define MAX1704X_SCL_PIN A2

// If USE_SENSOR_MAX1704X is defined, then ignore the followings settings for battery readings

// Analog pin for battery reading
// #define BATTERY_PIN A0
// Number of readings to average for battery voltage
// #define BATTERY_NUM_READINGS 100
// Delay between battery readings in milliseconds
// #define BATTERY_DELAY_BETWEEN_READINGS 30
// R1 = 680K Ohm
// R1 = 470K Ohm
// Ratio of the voltage divider resistors (R1 / (R1 + R2))
// #define BATTERY_RESISTORS_RATIO 0.460453401

// If set, the device will enter power-saving mode when battery is below a certain percentage
// #define BATTERY_POWER_SAVING

// Delay before going to sleep in milliseconds (when in debug mode or coming back from sleep without
// a wakeup reason)
// #define DELAY_BEFORE_SLEEP 20000

// -----------------------------
// Wake up pin
// -----------------------------

// Wakeup pin for the device
// Uncomment the following line to use a pin to wake up the device from deep sleep

// Uncomment the following lines to enable wakeup functionality either on EXT0 or EXT1
// #define WAKEUP_EXT1
// #define WAKEUP_EXT0

// Wakeup pin for the device
// #define WAKEUP_PIN      GPIO_NUM_7

// Pin mode for wakeup pin
// #define WAKEUP_PIN_MODE INPUT_PULLDOWN

// Wakeup level for the device
// #define WAKEUP_LEVEL    ESP_EXT1_WAKEUP_ANY_HIGH

// -------------------------------------------
// Misc
// -------------------------------------------
// In case the RTC module lost power, the WiFi credentials will be read from the SD card
// and used to connect to the WiFi network to fetch the current time from NTP servers.
// Filename for WiFi credentials inside the SD Card
// #define WIFI_FILENAME "/wifi.txt"

// Filename for Table of Contents (TOC) inside the SD Card
// Note: TOC is created the first time the device is started and contains the list of all images
// available on the SD card, so that the device can display them in a random order.
// The TOC is recreated if not found.
// When new images are added/removed to the SD card, the toc file on the SD card should be deleted
// so that it will be recreated the next time the device is started.
// #define TOC_FILENAME "/toc.txt"

// Wifi and NTP settings
// #define WIFI_CONNECT_TIMEOUT 10000
// #define TIMEZONE "CET-1CEST,M3.5.0,M10.5.0/3"
// #define NTP_TIMEOUT 20000
// #define NTP_SERVER1 "pool.ntp.org"
// #define NTP_SERVER2 "time.nist.gov"
// #define NTP_TIMEOUT 15000

// e-Paper display
// #define DISP_BW_V2       // Black and White e-Paper display (GDEH0154D67)
// #define DISP_7C_F        // 7-color e-Paper display (GDEY073D46)
// #define DISP_6C          // 6-color e-Paper display (GDEP073E01)

// #define USE_DESPI_DRIVER // Use Despi driver for e-Paper display
// #define USE_WAVESHARE_DRIVER // Use Waveshare driver for e-Paper display

// Accent color for the display
// #define ACCENT_COLOR GxEPD_BLACK

// Toggle debug mode (0 = off, 1 = on)
// When is on, for instance, the display will also print the battery raw value and there will be a
// delay before entering deep sleep
// #define DEBUG_MODE 1

// Toggle the use of the binary file for the e-Paper display
// When is defined, the display will use a binary files instead of regular bmp files.
// #define EPD_USE_BINARY_FILE

// Base directory for the SD card where the images are stored, based on the display type and the
// value of EPD_USE_BINARY_FILE #define SD_DIRNAME "/bin-bw"

// Forces the display to use the debug image instead of random from toc
// #define DEBUG_IMAGE_INDEX 381

// Reset the RTC so that it will be initialized with the current time from NTP servers
// It has not effect if USE_RTC is not defined
// #define RESET_INVALIDATES_DATE_TIME 1

// Minimum time between refreshes in seconds (minimum 5 minute, maximum 2 hours)
// #define REFRESH_MIN_INTERVAL_SECONDS (5 * SECONDS_IN_MINUTE)

// Maximum time between refreshes in seconds (minimum 10 minutes, maximum 4 hours)
// #define REFRESH_MAX_INTERVAL_SECONDS (2 * SECONDS_IN_HOUR)

// Step in seconds for the refresh rate
// #define REFRESH_STEP_SECONDS (5 * SECONDS_IN_MINUTE)

// time between refreshes in seconds when battery is low
// #define REFRESH_INTERVAL_SECONDS_LOW_BATTERY (6 * SECONDS_IN_HOUR)

// #define DAY_START_HOUR 06 // Hour when the day starts (6 AM)
// #define DAY_END_HOUR 23 // Hour when the day ends (10 PM) (midnight is 0)

// #define FONT_HEADER "assets/fonts/Ubuntu_R.h"
// #define FONT_HEADER                          "assets/fonts/UbuntuMono_R.h"
// #define FONT_HEADER                          "assets/fonts/FreeMono.h"
// #define FONT_HEADER                          "assets/fonts/FreeSans.h"
// #define FONT_HEADER                          "assets/fonts/Lato_Regular.h"
// #define FONT_HEADER                          "assets/fonts/Montserrat_Regular.h"
// #define FONT_HEADER                          "assets/fonts/OpenSans_Regular.h"
// #define FONT_HEADER                          "assets/fonts/Poppins_Regular.h"
// #define FONT_HEADER                          "assets/fonts/Quicksand_Regular.h"
// #define FONT_HEADER                          "assets/fonts/Raleway_Regular.h"
// #define FONT_HEADER                          "assets/fonts/Roboto_Regular.h"
// #define FONT_HEADER                          "assets/fonts/RobotoMono_Regular.h"
// #define FONT_HEADER                          "assets/fonts/RobotoSlab_Regular.h"

// -------------------------------------------
// LOCALE
// -------------------------------------------
//   Language (Territory)            code
//   English (United States)         en_US
//   Italiano (Italia)               it_IT
// #define LOCALE it_IT

// -------------------------------------------
// Google Drive
// -------------------------------------------

// Uncomment to use Google Drive as image source
// This will enable the use of Google Drive for image storage and retrieval, while the sd-card will
// be only used for caching and temporary storage
// #define USE_GOOGLE_DRIVE

// Path to the Google Drive configuration JSON file on the sd-card
// This file must be uploaded to your SD card and contain all Google Drive settings
// #define GOOGLE_DRIVE_CONFIG_FILEPATH "/config/google_drive_config.json"

// Maximum age (in seconds) the google drive local toc file can be before it is refreshed
#define GOOGLE_DRIVE_TOC_MAX_AGE_SECONDS (30 * SECONDS_PER_DAY)

// Maximum number of API requests per time window
#define GOOGLE_DRIVE_MAX_REQUESTS_PER_WINDOW 200 // Conservative limit (Google allows 1000/100s)

// Maximum backoff delay for retries in milliseconds
#define GOOGLE_DRIVE_BACKOFF_MAX_DELAY_MS 120000UL

// Time window for rate limiting in seconds
#define GOOGLE_DRIVE_RATE_LIMIT_WINDOW_SECONDS 3600

// Minimum delay between requests in milliseconds
#define GOOGLE_DRIVE_MIN_REQUEST_DELAY_MS 10000

// Maximum number of retry attempts for failed requests
#define GOOGLE_DRIVE_MAX_RETRY_ATTEMPTS 10

// Maximum backoff delay for retries in milliseconds
#define GOOGLE_DRIVE_BACKOFF_BASE_DELAY_MS 60000

// Maximum wait time for rate limiting in milliseconds
#define GOOGLE_DRIVE_MAX_WAIT_TIME_MS 1200000

// Maximum number of files to retrieve per API request  
// Test with larger page size now that RTC is disabled
#define GOOGLE_DRIVE_MAX_LIST_PAGE_SIZE 100

// Stream parser threshold in bytes - platform specific
#ifdef BOARD_HAS_PSRAM
    // Feather S3 with PSRAM - use aggressive limits to maximize performance
    #define GOOGLE_DRIVE_STREAM_PARSER_THRESHOLD 4194304  // 4MB - avoid streaming for most responses
    #define GOOGLE_DRIVE_JSON_DOC_SIZE 4194304           // 4MB JSON document buffer
    #define GOOGLE_DRIVE_BODY_RESERVE_SIZE 6291456       // 6MB response body reserve
    #define GOOGLE_DRIVE_SAFETY_LIMIT 10485760           // 10MB safety limit
#else
    // Standard ESP32 - conservative limits for limited RAM
    #define GOOGLE_DRIVE_STREAM_PARSER_THRESHOLD 32768   // 32KB
    #define GOOGLE_DRIVE_JSON_DOC_SIZE 40960            // 40KB JSON document buffer
    #define GOOGLE_DRIVE_BODY_RESERVE_SIZE 65536        // 64KB response body reserve
    #define GOOGLE_DRIVE_SAFETY_LIMIT 100000            // 100KB safety limit
#endif

// -------------------------------------------
// Google Drive Configuration File
// -------------------------------------------
//
// Create a JSON file at the path specified by GOOGLE_DRIVE_CONFIG_FILEPATH with the following structure:
//
// {
//   "authentication": {
//     "service_account_email": "your-service-account@project.iam.gserviceaccount.com",
//     "private_key_pem": "-----BEGIN PRIVATE KEY-----\nYOUR_PRIVATE_KEY_CONTENT\n-----END PRIVATE KEY-----\n",
//     "client_id": "116262609282266881196"
//   },
//   "drive": {
//     "folder_id": "1XWK-Op3uMFXADykfi0VR46r6HnrZfaDr",
//     "root_ca_path": "/certs/google_root_ca.pem",
//     "list_page_size": 150,
//     "use_insecure_tls": false
//   },
//   "caching": {
//     "local_path": "/gdrive",
//     "toc_filename": "toc.txt",
//     "toc_max_age_seconds": 604800
//   },
//   "rate_limiting": {
//     "max_requests_per_window": 100,
//     "rate_limit_window_seconds": 100,
//     "min_request_delay_ms": 500,
//     "max_retry_attempts": 3,
//     "backoff_base_delay_ms": 5000,
//     "max_wait_time_ms": 30000
//   }
// }
//
// Configuration Guide:
//
// Authentication section:
// - service_account_email: Email from your Google Service Account JSON key
// - private_key_pem: Private key from your Google Service Account JSON key (keep newlines as \n)
// - client_id: Client ID from your Google Service Account
//
// Drive section:
// - folder_id: Google Drive folder ID to use as image source (share with service account)
// - root_ca_path: Path to Google Root CA certificate file (required if use_insecure_tls is false)
// - list_page_size: Number of files to request per API call (1-1000)
// - use_insecure_tls: Set to true to skip SSL certificate validation (not recommended)
//
// Caching section:
// - local_path: Directory on SD card for cached files
// - toc_filename: Filename for the table of contents file
// - toc_max_age_seconds: Maximum age of cached TOC before refresh (1-2592000 seconds)
//
// Rate limiting section:
// - max_requests_per_window: Maximum API requests per time window (1-100, limited by GOOGLE_DRIVE_MAX_REQUESTS_PER_WINDOW)
// - rate_limit_window_seconds: Time window for rate limiting (1-3600 seconds)
// - min_request_delay_ms: Minimum delay between requests (0-10000 ms)
// - max_retry_attempts: Maximum retry attempts for failed requests (0-10)
// - backoff_base_delay_ms: Base delay for exponential backoff (1-60000 ms)
// - max_wait_time_ms: Maximum wait time for rate limiting (1-300000 ms)

// Free space threshold on SD card before cleanup
// #define SD_CARD_FREE_SPACE_THRESHOLD 1024 * 1024 * 16 // 16 MB (in bytes)

// Force the use of a specific test file
// #define GOOGLE_DRIVE_TEST_FILE "combined_portrait_214.bin"

// Cleanup temporary files interval (in seconds) - default is once per day
#ifndef CLEANUP_TEMP_FILES_INTERVAL_SECONDS
#define CLEANUP_TEMP_FILES_INTERVAL_SECONDS (24 * 60 * 60) // 24 hours
#endif

/// ---- End ----

// File extension for images on the SD card, based on the display type and the value of
// EPD_USE_BINARY_FILE
extern const char* LOCAL_FILE_EXTENSION;

#ifndef LOCAL_CONFIG_FILE
#error LOCAL_CONFIG_FILE not defined
#endif

#include XSTR(LOCAL_CONFIG_FILE)

#define PREFS_NAMESPACE        "photo_frame"

#define MICROSECONDS_IN_SECOND 1000000
#define SECONDS_IN_MINUTE      60
#define SECONDS_IN_HOUR        3600
#define SECONDS_IN_DAY         86400

// Maximum deep sleep duration to prevent overflow (24 hours)
#define MAX_DEEP_SLEEP_SECONDS SECONDS_IN_DAY

// Default values, if not defined

// Voltage of battery reading in millivolts above which the battery is considered charging
// Millivolts above which the battery is considered charging
#ifndef BATTERY_CHARGING_MILLIVOLTS
#define BATTERY_CHARGING_MILLIVOLTS 4300
#endif // BATTERY_CHARGING_MILLIVOLTS

// Minimum battery percentage to consider the battery empty
#ifndef BATTERY_PERCENT_EMPTY
#define BATTERY_PERCENT_EMPTY 5
#endif // BATTERY_PERCENT_EMPTY

// Minimum battery percentage to consider the battery critical
#ifndef BATTERY_PERCENT_CRITICAL
#define BATTERY_PERCENT_CRITICAL 10
#endif // BATTERY_PERCENT_CRITICAL

// Minimum battery percentage to consider the battery low
#ifndef BATTERY_PERCENT_LOW
#define BATTERY_PERCENT_LOW 25
#endif // BATTERY_PERCENT_LOW

#ifndef DELAY_BEFORE_SLEEP
#define DELAY_BEFORE_SLEEP 20000
#endif // DELAY_BEFORE_SLEEP

#ifndef WIFI_FILENAME
#define WIFI_FILENAME "/wifi.txt"
#endif // WIFI_FILENAME

#ifndef TOC_FILENAME
#define TOC_FILENAME "/toc.txt"
#endif // TOC_FILENAME

#ifndef WIFI_CONNECT_TIMEOUT
#define WIFI_CONNECT_TIMEOUT 10000
#endif // WIFI_CONNECT_TIMEOUT

#ifndef NTP_TIMEOUT
#define NTP_TIMEOUT 20000
#endif // NTP_TIMEOUT

// HTTP operation timeouts
#ifndef HTTP_CONNECT_TIMEOUT
#define HTTP_CONNECT_TIMEOUT 15000 // 15 seconds for connection
#endif                             // HTTP_CONNECT_TIMEOUT

#ifndef HTTP_REQUEST_TIMEOUT
#define HTTP_REQUEST_TIMEOUT 30000 // 30 seconds for full request
#endif                             // HTTP_REQUEST_TIMEOUT

#ifndef NTP_SERVER1
#define NTP_SERVER1 "pool.ntp.org"
#endif // NTP_SERVER1

#ifndef NTP_SERVER2
#define NTP_SERVER2 "time.nist.gov"
#endif // NTP_SERVER2

#ifndef NTP_TIMEOUT
#define NTP_TIMEOUT 20000
#endif // NTP_TIMEOUT

// Verify ACCENT_COLOR
#ifndef ACCENT_COLOR
#if defined(DISP_BW_V2)
#define ACCENT_COLOR GxEPD_BLACK // black
#elif defined(DISP_7C_F)
#define ACCENT_COLOR GxEPD_RED // red
#elif defined(DISP_6C)
#define ACCENT_COLOR GxEPD_RED // red for 6
#else
#define ACCENT_COLOR GxEPD_BLACK // default to black if no display type is defined
#endif                           // DISP_BW_V2 or DISP_7C_F or DISP_6C
#endif                           // ACCENT_COLOR

#ifdef USE_GOOGLE_DRIVE

#ifndef GOOGLE_DRIVE_CONFIG_FILEPATH
#define GOOGLE_DRIVE_CONFIG_FILEPATH "/config/google_drive_config.json"
#endif // GOOGLE_DRIVE_CONFIG_FILEPATH

#endif // USE_GOOGLE_DRIVE

#ifndef LOCALE
#define LOCALE en_US
#endif // LOCALE

#if defined(USE_SENSOR_MAX1704X)
#ifndef SENSOR_MAX1704X_TIMEOUT
#define SENSOR_MAX1704X_TIMEOUT 5000
#endif // SENSOR_MAX1704X_TIMEOUT
#endif // USE_SENSOR_MAX1704X

#ifndef RESET_INVALIDATES_DATE_TIME
#define RESET_INVALIDATES_DATE_TIME 1
#endif // RESET_INVALIDATES_DATE_TIME

#ifndef REFRESH_MIN_INTERVAL_SECONDS
#define REFRESH_MIN_INTERVAL_SECONDS (5 * SECONDS_IN_MINUTE)
#endif // REFRESH_MIN_INTERVAL_SECONDS

#ifndef REFRESH_MAX_INTERVAL_SECONDS
#define REFRESH_MAX_INTERVAL_SECONDS (2 * SECONDS_IN_HOUR)
#endif // REFRESH_MAX_INTERVAL_SECONDS

#ifndef REFRESH_STEP_SECONDS
#define REFRESH_STEP_SECONDS (5 * SECONDS_IN_MINUTE)
#endif // REFRESH_STEP_SECONDS

#ifndef REFRESH_INTERVAL_SECONDS_LOW_BATTERY
#define REFRESH_INTERVAL_SECONDS_LOW_BATTERY (6 * SECONDS_IN_HOURS)
#endif // REFRESH_INTERVAL_SECONDS_LOW_BATTERY

#ifndef DAY_START_HOUR
#define DAY_START_HOUR 6
#endif // DAY_START_HOUR

#ifndef DAY_END_HOUR
#define DAY_END_HOUR 23
#endif // DAY_END_HOUR

#ifndef FONT_HEADER
#define FONT_HEADER "assets/fonts/Ubuntu_R.h"
#endif

// Checks if has rgb leds defined
#ifndef HAS_RGB_LED
#if defined(ARDUINO_ARCH_ESP32) && defined(NANO_ESP32)
#define HAS_RGB_LED 1
#elif defined(LED_BLUE) && defined(LED_GREEN) && defined(LED_RED)
#define HAS_RGB_LED 1
#else
#define HAS_RGB_LED 0
#endif // HAS_RGB_LED
#endif // HAS_RGB_LED

#ifndef SD_CARD_FREE_SPACE_THRESHOLD
#define SD_CARD_FREE_SPACE_THRESHOLD 1024 * 1024 * 16 // 16 MB (in bytes)
#endif                                                // SD_CARD_FREE_SPACE_THRESHOLD

// Google Drive folder structure defines
#ifndef GOOGLE_DRIVE_TEMP_DIR
#define GOOGLE_DRIVE_TEMP_DIR "temp"
#endif

#ifndef GOOGLE_DRIVE_CACHE_DIR  
#define GOOGLE_DRIVE_CACHE_DIR "cache"
#endif

#ifndef POTENTIOMETER_INPUT_MAX
#define POTENTIOMETER_INPUT_MAX 4095
#endif // POTENTIOMETER_INPUT_MAX

#endif // __PHOTO_FRAME_CONFIG_H__
