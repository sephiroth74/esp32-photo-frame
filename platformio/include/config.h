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

#define STR(x) #x
#define XSTR(x) STR(x)

/// ---- Customizable settings ----

// #define USE_HSPI_FOR_SD  // Use HSPI for SD card communication
// #define USE_HSPI_FOR_EPD // Use HSPI for e-Paper display

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
// #define RTC_POWER_PIN A3
// #define RTC_SDA_PIN A4
// #define RTC_SCL_PIN A5

// -------------------------------------------
// Potentiometer
// -------------------------------------------
// #define POTENTIOMETER_PWR_PIN A6
// #define POTENTIOMETER_INPUT_PIN A7
// Maximum value for potentiometer input (12-bit ADC)
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
// #define DISP_BW_V2 // Black and White e-Paper display (GDEH0154D67)
// #define DISP_7C_F        // 7-color e-Paper display (GDEY073C46)
// #define DISP_6C             // 6-color e-Paper display (GDEP073E01)

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

// Base directory for the SD card where the images are stored, based on the display type and the value of EPD_USE_BINARY_FILE
// #define SD_DIRNAME "/bin-bw"

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
// This will enable the use of Google Drive for image storage and retrieval, while the sd-card will be
// only used for caching and temporary storage
// #define USE_GOOGLE_DRIVE

// Service account (from your JSON key)
// #define GOOGLE_DRIVE_SERVICE_ACCOUNT_EMAIL "your_service_account_email"
// #define GOOGLE_DRIVE_PRIVATE_KEY_PEM "-----BEGIN PRIVATE KEY-----your_private_key-----END PRIVATE KEY-----\n"
// #define GOOGLE_DRIVE_CLIENT_ID "client_id"
// Drive folder to list/download from (share this folder with SERVICE_ACCOUNT_EMAIL)
// #define GOOGLE_DRIVE_FOLDER_ID "folder_id"
// #define GOOGLE_DRIVE_LIST_PAGE_SIZE 300

// Uncomment to use insecure TLS (not recommended)
// #define USE_INSECURE_TLS

// Root CA certificate file for Google Drive SSL/TLS connections
// This file should contain the Google Root CA certificate in PEM format
// If USE_INSECURE_TLS is not defined, this file must exist and be valid
// #define GOOGLE_DRIVE_ROOT_CA "/certs/google_root_ca.pem"

// Max age (in seconds) of the google drive toc, after which it will be refreshed
// #define GOOGLE_DRIVE_TOC_MAX_AGE 7 * SECONDS_IN_DAY

// Filename for the Google Drive TOC on the sd-card
// #define GOOGLE_DRIVE_TOC_FILENAME "toc.json"

// Path to the Google Drive folder on the sd-card where the toc and the images will be stored
// #define GOOGLE_DRIVE_LOCAL_PATH "/gdrive"

// Free space threshold on SD card before cleanup
// #define SD_CARD_FREE_SPACE_THRESHOLD 1024 * 1024 * 16 // 16 MB (in bytes)

/// ---- End ----

// File extension for images on the SD card, based on the display type and the value of EPD_USE_BINARY_FILE
extern const char* LOCAL_FILE_EXTENSION;

#ifndef LOCAL_CONFIG_FILE
#error LOCAL_CONFIG_FILE not defined
#endif

#include XSTR(LOCAL_CONFIG_FILE)

#define PREFS_NAMESPACE "photo_frame"

#define MICROSECONDS_IN_SECOND 1000000
#define SECONDS_IN_MINUTE 60
#define SECONDS_IN_HOUR 3600
#define SECONDS_IN_DAY 86400

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
#endif // HTTP_CONNECT_TIMEOUT

#ifndef HTTP_REQUEST_TIMEOUT
#define HTTP_REQUEST_TIMEOUT 30000 // 30 seconds for full request
#endif // HTTP_REQUEST_TIMEOUT

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
#endif // DISP_BW_V2 or DISP_7C_F or DISP_6C
#endif // ACCENT_COLOR

#ifdef USE_GOOGLE_DRIVE

#ifndef GOOGLE_DRIVE_TOC_MAX_AGE
#define GOOGLE_DRIVE_TOC_MAX_AGE 7 * SECONDS_IN_DAY
#endif // GOOGLE_DRIVE_TOC_MAX_AGE

#ifndef GOOGLE_DRIVE_TOC_FILENAME
#define GOOGLE_DRIVE_TOC_FILENAME "toc.txt"
#endif // GOOGLE_DRIVE_TOC_FILENAME

#ifndef GOOGLE_DRIVE_LOCAL_PATH
#define GOOGLE_DRIVE_LOCAL_PATH "/gdrive"
#endif // GOOGLE_DRIVE_LOCAL_PATH

#ifndef GOOGLE_DRIVE_ROOT_CA
#define GOOGLE_DRIVE_ROOT_CA "/certs/google_root_ca.pem"
#endif // GOOGLE_DRIVE_ROOT_CA

// Google Drive API rate limiting settings
#ifndef GOOGLE_DRIVE_MAX_REQUESTS_PER_WINDOW
#define GOOGLE_DRIVE_MAX_REQUESTS_PER_WINDOW 100 // Conservative limit (Google allows 1000/100s)
#endif // GOOGLE_DRIVE_MAX_REQUESTS_PER_WINDOW

#ifndef GOOGLE_DRIVE_RATE_LIMIT_WINDOW_SECONDS
#define GOOGLE_DRIVE_RATE_LIMIT_WINDOW_SECONDS 100
#endif // GOOGLE_DRIVE_RATE_LIMIT_WINDOW_SECONDS

#ifndef GOOGLE_DRIVE_MIN_REQUEST_DELAY_MS
#define GOOGLE_DRIVE_MIN_REQUEST_DELAY_MS 500 // 500 milliseconds between requests
#endif // GOOGLE_DRIVE_MIN_REQUEST_DELAY_MS

#ifndef GOOGLE_DRIVE_MAX_RETRY_ATTEMPTS
#define GOOGLE_DRIVE_MAX_RETRY_ATTEMPTS 3
#endif // GOOGLE_DRIVE_MAX_RETRY_ATTEMPTS

#ifndef GOOGLE_DRIVE_BACKOFF_BASE_DELAY_MS
#define GOOGLE_DRIVE_BACKOFF_BASE_DELAY_MS 5000 // 5 seconds base delay for backoff
#endif // GOOGLE_DRIVE_BACKOFF_BASE_DELAY_MS

#ifndef GOOGLE_DRIVE_MAX_WAIT_TIME_MS
#define GOOGLE_DRIVE_MAX_WAIT_TIME_MS 30000 // 30 seconds maximum wait time for rate limiting
#endif // GOOGLE_DRIVE_MAX_WAIT_TIME_MS

#endif // USE_GOOGLE_DRIVE

#ifndef GOOGLE_DRIVE_LIST_PAGE_SIZE
#define GOOGLE_DRIVE_LIST_PAGE_SIZE 150
#endif // GOOGLE_DRIVE_LIST_PAGE_SIZE

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
#endif // SD_CARD_FREE_SPACE_THRESHOLD

#endif // __PHOTO_FRAME_CONFIG_H__
