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

// #define USE_HSPI_FOR_SD  // Use HSPI for SD card communication
#define USE_HSPI_FOR_EPD // Use HSPI for e-Paper display

// Pin definitions

// SD Card
extern const int8_t SD_CS_PIN;
extern const int8_t SD_MISO_PIN;
extern const int8_t SD_MOSI_PIN;
extern const int8_t SD_SCK_PIN;

// e-Paper Display
extern const int8_t EPD_CS_PIN;
extern const int8_t EPD_DC_PIN;
extern const int8_t EPD_RST_PIN;
extern const int8_t EPD_BUSY_PIN;
extern const int8_t EPD_MISO_PIN;
extern const int8_t EPD_MOSI_PIN;
extern const int8_t EPD_SCK_PIN;
extern const int8_t EPD_PWR_PIN;

// RTC
extern const int8_t RTC_SDA_PIN;
extern const int8_t RTC_SCL_PIN;
extern const int8_t RTC_POWER_PIN;
// Use RTC module for timekeeping
extern const bool USE_RTC; 

// Potentiometer pin
extern const int8_t POTENTIOMETER_PWR_PIN; // Power pin for potentiometer
extern const int8_t POTENTIOMETER_INPUT_PIN; // SDA pin for potentiometer
extern const uint16_t POTENTIOMETER_INPUT_MAX;

// Battery

// Analog pin for battery reading
extern const int8_t BATTERY_PIN;
// Resistor ratio for battery voltage divider
extern const double BATTERY_RESISTORS_RATIO;
// Number of readings to average for battery voltage
#define BATTERY_NUM_READINGS 20
// Delay between battery readings in milliseconds
#define BATTERY_DELAY_BETWEEN_READINGS 1

// Voltage of battery reading in millivolts above which the battery is considered charging
extern const uint32_t BATTERY_CHARGING_MILLIVOLTS;

// Minimum battery percentage to consider the battery empty
extern const uint8_t BATTERY_PERCENT_EMPTY;
// Minimum battery percentage to consider the battery critical
extern const uint8_t BATTERY_PERCENT_CRITICAL;
// Minimum battery percentage to consider the battery low
extern const uint8_t BATTERY_PERCENT_LOW;

#ifndef LED_BUILTIN
// Built-in LED pin for ESP32
#define LED_BUILTIN 2
#endif // LED_BUILTIN

// Delay before going to sleep in milliseconds (when in debug mode or coming back from sleep without
// a wakeup reason)
#define DELAY_BEFORE_SLEEP 20000

// Wifi
// In case the RTC module lost power, the WiFi credentials will be read from the SD card
// and used to connect to the WiFi network to fetch the current time from NTP servers.
extern const char* WIFI_FILENAME; // Filename for WiFi credentials inside the SD Card

// SdCard
// Filename for Table of Contents (TOC) inside the SD Card
// Note: TOC is created the first time the device is started and contains the list of all images
// available on the SD card, so that the device can display them in a random order.
// The TOC is recreated if not found.
// When new images are added/removed to the SD card, the toc file on the SD card should be deleted
// so that it will be recreated the next time the device is started.
extern const char* TOC_FILENAME;

// 10 seconds
#define WIFI_CONNECT_TIMEOUT 10000
#define TIMEZONE "CET-1CEST,M3.5.0,M10.5.0/3"
// ms
#define NTP_TIMEOUT 20000
#define NTP_SERVER1 "pool.ntp.org"
#define NTP_SERVER2 "time.nist.gov"

// e-Paper display

#define DISP_BW_V2 // Black and White e-Paper display (GDEH0154D67)
// #define DISP_7C_F        // 7-color e-Paper display (GDEY073C46)
// #define DISP_6C             // 6-color e-Paper display (GDEP073E01)

#define USE_DESPI_DRIVER // Use Despi driver for e-Paper display
// #define USE_WAVESHARE_DRIVER // Use Waveshare driver for e-Paper display

extern const int ACCENT_COLOR; // Accent color for the display

// Miscellaneous

// Toggle debug mode (0 = off, 1 = on)
// When is on, for instance, the display will also print the battery raw value and there will be a
// delay before entering deep sleep
#define DEBUG_MODE 0

// Toggle the use of the binary file for the e-Paper display
// When is on, the display will use a binary files instead of regular image files.
#define EPD_USE_BINARY_FILE 1

// Base directory for the SD card where the images are stored, based on the display type and the value of EPD_USE_BINARY_FILE
extern const char* SD_DIRNAME;

// File extension for images on the SD card, based on the display type and the value of EPD_USE_BINARY_FILE
extern const char* SD_FILE_EXTENSION;

// Forces the display to use the debug image instead of random from toc
// #define DEBUG_IMAGE_INDEX 381

// Reset the RTC so that it will be initialized with the current time from NTP servers
#define RESET_INVALIDATES_DATE_TIME 1

#define MICROSECONDS_IN_SECOND 1000000
#define SECONDS_IN_MINUTE 60
#define SECONDS_IN_HOUR 3600

// Minimum time between refreshes in seconds (minimum 5 minute, maximum 2 hours)
#define REFRESH_MIN_INTERVAL_SECONDS (5 * SECONDS_IN_MINUTE)

// Maximum time between refreshes in seconds (minimum 10 minutes, maximum 4 hours)
#define REFRESH_MAX_INTERVAL_SECONDS (2 * SECONDS_IN_HOUR)

// Step in seconds for the refresh rate
#define REFRESH_STEP_SECONDS (5 * SECONDS_IN_MINUTE)

// time between refreshes in seconds when battery is low
#define REFRESH_INTERVAL_SECONDS_LOW_BATTERY (6 * SECONDS_IN_HOUR)

#define DAY_START_HOUR 06 // Hour when the day starts (6 AM)
#define DAY_END_HOUR 23 // Hour when the day ends (10 PM) (midnight is 0)

#define FONT_HEADER "assets/fonts/Ubuntu_R.h"
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

// LOCALE
//   Language (Territory)            code
//   English (United States)         en_US
//   Italiano (Italia)               it_IT
#define LOCALE it_IT

#define PREFS_NAMESPACE "photo_frame"

// Checks if has rgb leds defined
#ifdef ARDUINO_ARCH_ESP32
#define HAS_RGB_LED 1
#else
#define HAS_RGB_LED 0
#endif // HAS_RGB_LED

namespace photo_frame {
void print_config();
} // namespace photo_frame

#endif // __PHOTO_FRAME_CONFIG_H__
