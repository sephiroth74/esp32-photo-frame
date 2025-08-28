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
#if !defined(EPD_BUSY_PIN) || !defined(EPD_RST_PIN) || !defined(EPD_DC_PIN) || !defined(EPD_CS_PIN) || !defined(EPD_SCK_PIN) || !defined(EPD_MOSI_PIN)
#error "Please define all E-Paper display pins"
#endif

// Check display type
#if !defined(DISP_BW_V2) && !defined(DISP_7C_F) && !defined(DISP_6C)
#error "Please define DISP_BW_V2 or DISP_7C_F or DISP_6C"
#endif

#if defined(DISP_BW_V2) && defined(DISP_7C_F) && defined(DISP_6C)
#error "Please define only one display type: either DISP_BW_V2 or DISP_7C_F or DISP_6C"
#endif

#if defined(USE_GOOGLE_DRIVE)
#if !defined(GOOGLE_DRIVE_SERVICE_ACCOUNT_EMAIL) || !defined(GOOGLE_DRIVE_PRIVATE_KEY_PEM) || !defined(GOOGLE_DRIVE_CLIENT_ID) || !defined(GOOGLE_DRIVE_FOLDER_ID)
#error "Please define all Google Drive settings (GOOGLE_DRIVE_SERVICE_ACCOUNT_EMAIL, GOOGLE_DRIVE_PRIVATE_KEY_PEM, GOOGLE_DRIVE_CLIENT_ID, GOOGLE_DRIVE_FOLDER_ID)"
#endif

#if GOOGLE_DRIVE_TOC_MAX_AGE > 30 * SECONDS_IN_DAY
#error "GOOGLE_DRIVE_TOC_MAX_AGE must be less than or equal to 30 days"
#endif

#if !defined(USE_INSECURE_TLS) && !defined(GOOGLE_DRIVE_ROOT_CA)
#error "When USE_INSECURE_TLS is not defined, GOOGLE_DRIVE_ROOT_CA must be defined with the path to the root CA certificate file"
#endif
#endif // USE_GOOGLE_DRIVE

// Check all SD-Card pins are defined
#if !defined(SD_CS_PIN) || !defined(SD_MISO_PIN) || !defined(SD_MOSI_PIN) || !defined(SD_SCK_PIN)
#error "Please define all SD-Card pins"
#endif

#if !defined(USE_HSPI_FOR_EPD) && !defined(USE_HSPI_FOR_SD)
#error "Please define USE_HSPI_FOR_EPD or USE_HSPI_FOR_SD"
#endif // USE_HSPI_FOR_EPD or USE_HSPI_FOR_SD

// Check only one SPI bus is used
#if defined(USE_HSPI_FOR_SD) && defined(USE_HSPI_FOR_EPD)
#error "Please define only one SPI bus: either USE_HSPI_FOR_SD or USE_HSPI_FOR_EPD"
#endif // USE_HSPI_FOR_SD or USE_HSPI_FOR_EPD

// Check all RTC pins are defined
#ifdef USE_RTC
#if !defined(RTC_SDA_PIN) || !defined(RTC_SCL_PIN)
#error "Please define all RTC pins"
#endif
#endif // USE_RTC

// Check potentiometer pins are all set
#if !defined(POTENTIOMETER_PWR_PIN) || !defined(POTENTIOMETER_INPUT_PIN)
#error "Please define all potentiometer pins"
#endif // POTENTIOMETER_PWR_PIN || POTENTIOMETER_INPUT_PIN

#if defined(SENSOR_MAX1704X)
#if !defined(MAX1704X_SDA_PIN) || !defined(MAX1704X_SCL_PIN)
#error "Please define SENSOR_MAX1704X_PIN"
#endif // !defined(MAX1704X_SDA_PIN) || !defined(MAX1704X_SCL_PIN)
#endif // SENSOR_MAX1704X

// validate DAY_START and DAY_END
#if (DAY_START_HOUR < 0 || DAY_START_HOUR > 23)
#error "DAY_START_HOUR must be between 0 and 23"
#endif // DAY_START_HOUR
#if (DAY_END_HOUR < 0 || DAY_END_HOUR > 23)
#error "DAY_END_HOUR must be between 0 and 23"
#endif // DAY_END_HOUR

// validate REFRESH_MIN_INTERVAL_SECONDS and REFRESH_MAX_INTERVAL_SECONDS
#if (REFRESH_MIN_INTERVAL_SECONDS < (5 * SECONDS_IN_MINUTE) || REFRESH_MIN_INTERVAL_SECONDS > (2 * SECONDS_IN_HOUR))
#error "REFRESH_MIN_INTERVAL_SECONDS must be between 5 minutes and 2 hours"
#endif // REFRESH_MIN_INTERVAL_SECONDS

#if (REFRESH_MAX_INTERVAL_SECONDS < (10 * SECONDS_IN_MINUTE) || REFRESH_MAX_INTERVAL_SECONDS > (4 * SECONDS_IN_HOUR))
#error "REFRESH_MAX_INTERVAL_SECONDS must be between 10 minutes and 4 hours"
#endif // REFRESH_MAX_INTERVAL_SECONDS

#if (REFRESH_MIN_INTERVAL_SECONDS > REFRESH_MAX_INTERVAL_SECONDS)
#error "REFRESH_MIN_INTERVAL_SECONDS must be less than REFRESH_MAX_INTERVAL_SECONDS"
#endif // REFRESH_MIN_INTERVAL_SECONDS > REFRESH_MAX_INTERVAL_SECONDS

#if (REFRESH_MAX_INTERVAL_SECONDS > ((24 - DAY_END_HOUR) + DAY_START_HOUR) * SECONDS_IN_HOUR)
#error \
    "REFRESH_MAX_INTERVAL_SECONDS must be less than or equal to the number of seconds between DAY_START_HOUR and DAY_END_HOUR"
#endif // REFRESH_MAX_INTERVAL_SECONDS > (DAY_END_HOUR - DAY_START_HOUR) * SECONDS_IN_HOUR

// Include local configuration overrides
// Here you can define the local configuration overrides not to be committed to the repository

#ifndef SENSOR_MAX1704X
// validate battery settings
#if (BATTERY_NUM_READINGS < 1 || BATTERY_NUM_READINGS > 100)
#error "BATTERY_NUM_READINGS must be between 1 and 100"
#endif // BATTERY_NUM_READINGS
#if (BATTERY_DELAY_BETWEEN_READINGS < 1 || BATTERY_DELAY_BETWEEN_READINGS > 1000)
#error "BATTERY_DELAY_BETWEEN_READINGS must be between 0 and 1000 milliseconds"
#endif // BATTERY_DELAY_BETWEEN_READINGS
#endif // SENSORS_MAX1704X

#ifndef SD_DIRNAME
#error "SD_DIRNAME must be defined"
#endif // SD_DIRNAME

//
// Define the file extension and directory name based on the display type and EPD_USE_BINARY_FILE setting
//
#if defined(EPD_USE_BINARY_FILE)
const char* LOCAL_FILE_EXTENSION = ".bin";
#else
const char* LOCAL_FILE_EXTENSION = ".bmp";
#endif // EPD_USE_BINARY_FILE
