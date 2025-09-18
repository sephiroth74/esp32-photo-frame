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

#include "config.h"

// Check all the E-Paper pins are defined
#if !defined(EPD_BUSY_PIN) || !defined(EPD_RST_PIN) || !defined(EPD_DC_PIN) ||                     \
    !defined(EPD_CS_PIN) || !defined(EPD_SCK_PIN) || !defined(EPD_MOSI_PIN)
#error "Please define all E-Paper display pins"
#endif

// Check display type
#if !defined(DISP_BW_V2) && !defined(DISP_7C_F) && !defined(DISP_6C)
#error "Please define DISP_BW_V2 or DISP_7C_F or DISP_6C"
#endif

#if defined(DISP_BW_V2) && defined(DISP_7C_F) && defined(DISP_6C)
#error "Please define only one display type: either DISP_BW_V2 or DISP_7C_F or DISP_6C"
#endif

// check if GOOGLE_DRIVE_CONFIG_FILEPATH is defined (required for Google Drive functionality)
#if !defined(GOOGLE_DRIVE_CONFIG_FILEPATH)
#error "GOOGLE_DRIVE_CONFIG_FILEPATH must be defined for Google Drive functionality"
#endif // GOOGLE_DRIVE_CONFIG_FILEPATH

// Check all SD-Card pins are defined
#if !defined(SD_CS_PIN) || !defined(SD_MISO_PIN) || !defined(SD_MOSI_PIN) || !defined(SD_SCK_PIN)
#error "Please define all SD-Card pins"
#endif

#if !defined(USE_HSPI_FOR_EPD) && !defined(USE_HSPI_FOR_SD) && !defined(USE_SHARED_SPI)
#error "Please define USE_HSPI_FOR_EPD or USE_HSPI_FOR_SD or USE_SHARED_SPI"
#endif // USE_HSPI_FOR_EPD or USE_HSPI_FOR_SD or USE_SHARED_SPI

// Check only one SPI bus is used
#if defined(USE_HSPI_FOR_SD) && defined(USE_HSPI_FOR_EPD) && defined(USE_SHARED_SPI)
#error                                                                                             \
    "Please define only one SPI bus: either USE_HSPI_FOR_SD or USE_HSPI_FOR_EPD or USE_SHARED_SPI"
#endif // USE_HSPI_FOR_SD or USE_HSPI_FOR_EPD or USE_SHARED_SPI

// Check all RTC pins are defined
#ifdef USE_RTC
#if !defined(RTC_SDA_PIN) || !defined(RTC_SCL_PIN)
#error "Please define all RTC pins"
#endif

// ESP32-C6 has I2C/WiFi coexistence issues that cause JSON parsing corruption
// RTC module is not supported on ESP32-C6 - use NTP-only time instead
#if defined(CONFIG_IDF_TARGET_ESP32C6)
#error                                                                                             \
    "RTC module (USE_RTC) is not supported on ESP32-C6 due to I2C/WiFi interference. Remove USE_RTC definition and use NTP-only time synchronization."
#endif // CONFIG_IDF_TARGET_ESP32C6

#endif // USE_RTC

// Check potentiometer pins are all set
#if !defined(POTENTIOMETER_PWR_PIN) || !defined(POTENTIOMETER_INPUT_PIN)
#error "Please define all potentiometer pins"
#endif // POTENTIOMETER_PWR_PIN || POTENTIOMETER_INPUT_PIN

#if !defined(POTENTIOMETER_INPUT_MAX)
#error "Please define POTENTIOMETER_INPUT_MAX"
#endif // !defined(POTENTIOMETER_INPUT_MAX)

#if defined(USE_SENSOR_MAX1704X)
#if !defined(MAX1704X_SDA_PIN) || !defined(MAX1704X_SCL_PIN)
#error "Please define SENSOR_MAX1704X_PIN"
#endif // !defined(MAX1704X_SDA_PIN) || !defined(MAX1704X_SCL_PIN)
#endif // USE_SENSOR_MAX1704X

// validate DAY_START and DAY_END
#if (DAY_START_HOUR < 0 || DAY_START_HOUR > 23)
#error "DAY_START_HOUR must be between 0 and 23"
#endif // DAY_START_HOUR
#if (DAY_END_HOUR < 0 || DAY_END_HOUR > 23)
#error "DAY_END_HOUR must be between 0 and 23"
#endif // DAY_END_HOUR

// validate REFRESH_MIN_INTERVAL_SECONDS and REFRESH_MAX_INTERVAL_SECONDS
#if (REFRESH_MIN_INTERVAL_SECONDS < (5 * SECONDS_IN_MINUTE) ||                                     \
     REFRESH_MIN_INTERVAL_SECONDS > (2 * SECONDS_IN_HOUR))
#error "REFRESH_MIN_INTERVAL_SECONDS must be between 5 minutes and 2 hours"
#endif // REFRESH_MIN_INTERVAL_SECONDS

#if (REFRESH_MAX_INTERVAL_SECONDS < (10 * SECONDS_IN_MINUTE) ||                                    \
     REFRESH_MAX_INTERVAL_SECONDS > (4 * SECONDS_IN_HOUR))
#error "REFRESH_MAX_INTERVAL_SECONDS must be between 10 minutes and 4 hours"
#endif // REFRESH_MAX_INTERVAL_SECONDS

#if (REFRESH_MIN_INTERVAL_SECONDS > REFRESH_MAX_INTERVAL_SECONDS)
#error "REFRESH_MIN_INTERVAL_SECONDS must be less than REFRESH_MAX_INTERVAL_SECONDS"
#endif // REFRESH_MIN_INTERVAL_SECONDS > REFRESH_MAX_INTERVAL_SECONDS

#if (REFRESH_MAX_INTERVAL_SECONDS > ((24 - DAY_END_HOUR) + DAY_START_HOUR) * SECONDS_IN_HOUR)
#error                                                                                             \
    "REFRESH_MAX_INTERVAL_SECONDS must be less than or equal to the number of seconds between DAY_START_HOUR and DAY_END_HOUR"
#endif // REFRESH_MAX_INTERVAL_SECONDS > (DAY_END_HOUR - DAY_START_HOUR) * SECONDS_IN_HOUR

// Include local configuration overrides
// Here you can define the local configuration overrides not to be committed to the repository

#ifndef USE_SENSOR_MAX1704X
// validate battery settings
#if (BATTERY_NUM_READINGS < 1 || BATTERY_NUM_READINGS > 100)
#error "BATTERY_NUM_READINGS must be between 1 and 100"
#endif // BATTERY_NUM_READINGS
#if (BATTERY_DELAY_BETWEEN_READINGS < 1 || BATTERY_DELAY_BETWEEN_READINGS > 1000)
#error "BATTERY_DELAY_BETWEEN_READINGS must be between 0 and 1000 milliseconds"
#endif // BATTERY_DELAY_BETWEEN_READINGS
#endif // USE_SENSOR_MAX1704X

/**
 * @brief Array of allowed file extensions for image files
 *
 * This array contains all file extensions that the ESP32 photo frame can process.
 * The system supports runtime file format detection, allowing both binary and bitmap
 * formats to coexist in the same directory or Google Drive folder.
 *
 * Supported formats:
 * - ".bin": Binary format optimized for ESP32 e-paper displays (compressed, fast rendering)
 * - ".bmp": Standard bitmap format for viewing/debugging (uncompressed, compatible)
 *
 * @note The rendering engine automatically selects the appropriate decoder based on
 *       the detected file extension at runtime.
 * @see photo_frame::io_utils::is_binary_format() for format detection logic
 */
const char* ALLOWED_FILE_EXTENSIONS[] = {".bin", ".bmp"};

/**
 * @brief Number of elements in the ALLOWED_FILE_EXTENSIONS array
 *
 * This constant provides the count of supported file extensions and is used
 * throughout the codebase for iterating over the extensions array safely.
 *
 * @note This value is calculated automatically and should not be modified manually.
 */
const size_t ALLOWED_EXTENSIONS_COUNT = sizeof(ALLOWED_FILE_EXTENSIONS) / sizeof(ALLOWED_FILE_EXTENSIONS[0]);

#if defined(WAKEUP_EXT1) && defined(WAKEUP_EXT0)
#error "Please define only one wakeup pin: either WAKEUP_EXT1 or WAKEUP_EXT0"
#endif // WAKEUP_EXT1 && WAKEUP_EXT0

#if defined(WAKEUP_EXT1) || defined(WAKEUP_EXT0)
#if !defined(WAKEUP_PIN) || !defined(WAKEUP_PIN_MODE) || !defined(WAKEUP_LEVEL)
#error "Please define all wakeup pin settings"
#endif // WAKEUP_PIN && WAKEUP_PIN_MODE && WAKEUP_LEVEL
#endif // WAKEUP_EXT1 || WAKEUP_EXT0

#ifdef USE_WEATHER
#ifndef WEATHER_CONFIG_FILE
#error "WEATHER_CONFIG_FILE must be defined"
#endif // WEATHER_CONFIG_FILE

#ifndef WEATHER_CACHE_FILE
#error "WEATHER_CACHE_FILE must be defined"
#endif // WEATHER_CACHE_FILE
#endif // USE_WEATHER