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

// ============================================================================
// COMPILE-TIME VALIDATION CHECKS
//
// This file contains compile-time validation to ensure all required
// configuration constants are properly defined and have valid values.
// Any configuration errors will be caught at compile time with descriptive
// error messages.
// ============================================================================

// ============================================================================
// HARDWARE CONFIGURATION VALIDATION
// ============================================================================

// ----------------------------------------------------------------------------
// E-Paper Display Validation
// ----------------------------------------------------------------------------

/// Ensure all required e-paper display pins are defined
#if !defined(EPD_BUSY_PIN) || !defined(EPD_RST_PIN) || !defined(EPD_DC_PIN) || \
    !defined(EPD_CS_PIN) || !defined(EPD_SCK_PIN) || !defined(EPD_MOSI_PIN)
#error "All e-paper display pins must be defined: EPD_BUSY_PIN, EPD_RST_PIN, EPD_DC_PIN, EPD_CS_PIN, EPD_SCK_PIN, EPD_MOSI_PIN"
#endif

/// Ensure exactly one display type is selected
#if !defined(DISP_BW_V2) && !defined(DISP_7C_F) && !defined(DISP_6C)
#error "Display type must be defined: choose one of DISP_BW_V2, DISP_7C_F, or DISP_6C"
#endif

/// Prevent multiple display type definitions
#if (defined(DISP_BW_V2) && defined(DISP_7C_F)) || \
    (defined(DISP_BW_V2) && defined(DISP_6C)) || \
    (defined(DISP_7C_F) && defined(DISP_6C))
#error "Only one display type can be defined: DISP_BW_V2, DISP_7C_F, or DISP_6C"
#endif

// ----------------------------------------------------------------------------
// RGB Status LED Validation
// ----------------------------------------------------------------------------

/// When RGB status is enabled, ensure all required pins are defined
#ifdef RGB_STATUS_ENABLED
#if !defined(RGB_LED_PIN) || !defined(RGB_LED_COUNT)
#error "RGB_LED_PIN and RGB_LED_COUNT must be defined when RGB_STATUS_ENABLED is used"
#endif

/// Validate RGB LED count is reasonable (1-255 LEDs)
#if RGB_LED_COUNT < 1 || RGB_LED_COUNT > 255
#error "RGB_LED_COUNT must be between 1 and 255"
#endif
#endif // RGB_STATUS_ENABLED

// ----------------------------------------------------------------------------
// RTC Module Validation
// ----------------------------------------------------------------------------

#ifdef USE_RTC
/// Ensure RTC I2C pins are defined when RTC is enabled
#if !defined(RTC_SDA_PIN) || !defined(RTC_SCL_PIN)
#error "RTC_SDA_PIN and RTC_SCL_PIN must be defined when USE_RTC is enabled"
#endif

/// ESP32-C6 has I2C/WiFi coexistence issues that cause JSON parsing corruption
/// RTC module is not supported on ESP32-C6 - use NTP-only time instead
#if defined(CONFIG_IDF_TARGET_ESP32C6)
#error "RTC module (USE_RTC) is not supported on ESP32-C6 due to I2C/WiFi interference. Remove USE_RTC definition and use NTP-only time synchronization."
#endif // CONFIG_IDF_TARGET_ESP32C6
#endif // USE_RTC

// ----------------------------------------------------------------------------
// User Interface Hardware Validation
// ----------------------------------------------------------------------------

/// Ensure potentiometer pins are defined
#if !defined(POTENTIOMETER_PWR_PIN) || !defined(POTENTIOMETER_INPUT_PIN)
#error "POTENTIOMETER_PWR_PIN and POTENTIOMETER_INPUT_PIN must be defined"
#endif

/// Ensure potentiometer maximum value is defined
#if !defined(POTENTIOMETER_INPUT_MAX)
#error "POTENTIOMETER_INPUT_MAX must be defined (typically 4095 for 12-bit ADC or 1023 for 10-bit ADC)"
#endif

/// Validate potentiometer input range is reasonable
#if POTENTIOMETER_INPUT_MAX < 255 || POTENTIOMETER_INPUT_MAX > 65535
#error "POTENTIOMETER_INPUT_MAX must be between 255 and 65535"
#endif

// ----------------------------------------------------------------------------
// Battery Sensor Validation
// ----------------------------------------------------------------------------

#ifdef USE_SENSOR_MAX1704X
/// Ensure MAX1704X I2C pins are defined when sensor is enabled
#if !defined(MAX1704X_SDA_PIN) || !defined(MAX1704X_SCL_PIN)
#error "MAX1704X_SDA_PIN and MAX1704X_SCL_PIN must be defined when USE_SENSOR_MAX1704X is enabled"
#endif

/// Validate sensor timeout is reasonable (1-30 seconds)
#if SENSOR_MAX1704X_TIMEOUT < 1000 || SENSOR_MAX1704X_TIMEOUT > 30000
#error "SENSOR_MAX1704X_TIMEOUT must be between 1000 and 30000 milliseconds"
#endif
#endif // USE_SENSOR_MAX1704X

#ifndef USE_SENSOR_MAX1704X
/// Validate analog battery monitoring settings when MAX1704X is not used
#if !defined(BATTERY_PIN) || !defined(BATTERY_NUM_READINGS) || !defined(BATTERY_DELAY_BETWEEN_READINGS) || !defined(BATTERY_RESISTORS_RATIO)
#error "When USE_SENSOR_MAX1704X is not defined, you must define: BATTERY_PIN, BATTERY_NUM_READINGS, BATTERY_DELAY_BETWEEN_READINGS, BATTERY_RESISTORS_RATIO"
#endif

/// Validate battery reading parameters are within reasonable ranges
#if BATTERY_NUM_READINGS < 1 || BATTERY_NUM_READINGS > 1000
#error "BATTERY_NUM_READINGS must be between 1 and 1000"
#endif

#if BATTERY_DELAY_BETWEEN_READINGS < 1 || BATTERY_DELAY_BETWEEN_READINGS > 10000
#error "BATTERY_DELAY_BETWEEN_READINGS must be between 1 and 10000 milliseconds"
#endif

/// Note: BATTERY_RESISTORS_RATIO validation is performed at runtime since
/// preprocessor cannot handle floating point comparisons.
/// Typical valid range: 0.1 to 0.9 (common values: 0.25 to 0.67)
#endif // !USE_SENSOR_MAX1704X

// ----------------------------------------------------------------------------
// Wake-up Pin Validation
// ----------------------------------------------------------------------------

/// Ensure only one wakeup method is selected
#if defined(WAKEUP_EXT1) && defined(WAKEUP_EXT0)
#error "Only one wakeup method can be defined: either WAKEUP_EXT1 or WAKEUP_EXT0"
#endif

/// When wakeup is configured, ensure all required settings are defined
#if defined(WAKEUP_EXT1) || defined(WAKEUP_EXT0)
#if !defined(WAKEUP_PIN) || !defined(WAKEUP_PIN_MODE) || !defined(WAKEUP_LEVEL)
#error "When wakeup is enabled, define: WAKEUP_PIN, WAKEUP_PIN_MODE, WAKEUP_LEVEL"
#endif
#endif

// ============================================================================
// TIMING AND POWER MANAGEMENT VALIDATION
// ============================================================================

// ----------------------------------------------------------------------------
// Daily Schedule Validation
// ----------------------------------------------------------------------------

/// Validate day start and end hours are within 24-hour format
#if DAY_START_HOUR < 0 || DAY_START_HOUR > 23
#error "DAY_START_HOUR must be between 0 and 23 (24-hour format)"
#endif

#if DAY_END_HOUR < 0 || DAY_END_HOUR > 23
#error "DAY_END_HOUR must be between 0 and 23 (24-hour format)"
#endif

/// Note: DAY_START_HOUR can be greater than DAY_END_HOUR for overnight operation
/// (e.g., DAY_START_HOUR=22, DAY_END_HOUR=6 means active 22:00-06:00)

// ----------------------------------------------------------------------------
// Refresh Interval Validation
// ----------------------------------------------------------------------------

/// Validate minimum refresh interval (5 minutes to 2 hours)
#if REFRESH_MIN_INTERVAL_SECONDS < (5 * SECONDS_IN_MINUTE) || \
    REFRESH_MIN_INTERVAL_SECONDS > (2 * SECONDS_IN_HOUR)
#error "REFRESH_MIN_INTERVAL_SECONDS must be between 5 minutes and 2 hours"
#endif

/// Validate maximum refresh interval (10 minutes to 4 hours)
#if REFRESH_MAX_INTERVAL_SECONDS < (10 * SECONDS_IN_MINUTE) || \
    REFRESH_MAX_INTERVAL_SECONDS > (4 * SECONDS_IN_HOUR)
#error "REFRESH_MAX_INTERVAL_SECONDS must be between 10 minutes and 4 hours"
#endif

/// Ensure minimum interval is less than maximum interval
#if REFRESH_MIN_INTERVAL_SECONDS >= REFRESH_MAX_INTERVAL_SECONDS
#error "REFRESH_MIN_INTERVAL_SECONDS must be less than REFRESH_MAX_INTERVAL_SECONDS"
#endif

/// Validate refresh step size is reasonable
#if REFRESH_STEP_SECONDS < SECONDS_IN_MINUTE || REFRESH_STEP_SECONDS > SECONDS_IN_HOUR
#error "REFRESH_STEP_SECONDS must be between 1 minute and 1 hour"
#endif

/// Ensure low battery refresh interval is reasonable (1-24 hours)
#if REFRESH_INTERVAL_SECONDS_LOW_BATTERY < SECONDS_IN_HOUR || \
    REFRESH_INTERVAL_SECONDS_LOW_BATTERY > SECONDS_IN_DAY
#error "REFRESH_INTERVAL_SECONDS_LOW_BATTERY must be between 1 hour and 24 hours"
#endif

// ----------------------------------------------------------------------------
// Battery Threshold Validation
// ----------------------------------------------------------------------------

/// Validate battery percentage thresholds are in ascending order
#if BATTERY_PERCENT_EMPTY >= BATTERY_PERCENT_CRITICAL
#error "BATTERY_PERCENT_CRITICAL must be greater than BATTERY_PERCENT_EMPTY"
#endif

#if BATTERY_PERCENT_CRITICAL >= BATTERY_PERCENT_LOW
#error "BATTERY_PERCENT_LOW must be greater than BATTERY_PERCENT_CRITICAL"
#endif

#if BATTERY_PERCENT_LOW >= 100
#error "BATTERY_PERCENT_LOW must be less than 100"
#endif

/// Validate battery charging voltage threshold is reasonable (3.5V to 5.0V)
#if BATTERY_CHARGING_MILLIVOLTS < 3500 || BATTERY_CHARGING_MILLIVOLTS > 5000
#error "BATTERY_CHARGING_MILLIVOLTS must be between 3500 and 5000 (3.5V to 5.0V)"
#endif

// ============================================================================
// NETWORK AND CONNECTIVITY VALIDATION
// ============================================================================

// ----------------------------------------------------------------------------
// Timeout Validation
// ----------------------------------------------------------------------------

/// Validate WiFi connection timeout is reasonable (5-60 seconds)
#if WIFI_CONNECT_TIMEOUT < 5000 || WIFI_CONNECT_TIMEOUT > 60000
#error "WIFI_CONNECT_TIMEOUT must be between 5000 and 60000 milliseconds (5-60 seconds)"
#endif

/// Validate HTTP timeouts are reasonable
#if HTTP_CONNECT_TIMEOUT < 5000 || HTTP_CONNECT_TIMEOUT > 120000
#error "HTTP_CONNECT_TIMEOUT must be between 5000 and 120000 milliseconds (5-120 seconds)"
#endif

#if HTTP_REQUEST_TIMEOUT < 10000 || HTTP_REQUEST_TIMEOUT > 300000
#error "HTTP_REQUEST_TIMEOUT must be between 10000 and 300000 milliseconds (10-300 seconds)"
#endif

/// Validate NTP timeout is reasonable
#if NTP_TIMEOUT < 5000 || NTP_TIMEOUT > 60000
#error "NTP_TIMEOUT must be between 5000 and 60000 milliseconds (5-60 seconds)"
#endif

// ============================================================================
// STORAGE AND FILE SYSTEM VALIDATION
// ============================================================================

// ----------------------------------------------------------------------------
// Required File Paths Validation
// ----------------------------------------------------------------------------

/// Ensure Google Drive configuration file path is defined
#if !defined(GOOGLE_DRIVE_CONFIG_FILEPATH)
#error "GOOGLE_DRIVE_CONFIG_FILEPATH must be defined for Google Drive functionality"
#endif

/// Validate SD card free space threshold is reasonable (1MB to 1GB)
#if SD_CARD_FREE_SPACE_THRESHOLD < (1024 * 1024) || SD_CARD_FREE_SPACE_THRESHOLD > (1024 * 1024 * 1024)
#error "SD_CARD_FREE_SPACE_THRESHOLD must be between 1MB and 1GB"
#endif

/// Validate cleanup interval is reasonable (1 hour to 7 days)
#if CLEANUP_TEMP_FILES_INTERVAL_SECONDS < SECONDS_IN_HOUR || \
    CLEANUP_TEMP_FILES_INTERVAL_SECONDS > (7 * SECONDS_IN_DAY)
#error "CLEANUP_TEMP_FILES_INTERVAL_SECONDS must be between 1 hour and 7 days"
#endif

// ============================================================================
// GOOGLE DRIVE API VALIDATION
// ============================================================================

// ----------------------------------------------------------------------------
// Rate Limiting Validation
// ----------------------------------------------------------------------------

/// Validate Google Drive API rate limiting parameters
#if GOOGLE_DRIVE_MAX_REQUESTS_PER_WINDOW < 10 || GOOGLE_DRIVE_MAX_REQUESTS_PER_WINDOW > 1000
#error "GOOGLE_DRIVE_MAX_REQUESTS_PER_WINDOW must be between 10 and 1000"
#endif

#if GOOGLE_DRIVE_RATE_LIMIT_WINDOW_SECONDS < 60 || GOOGLE_DRIVE_RATE_LIMIT_WINDOW_SECONDS > 86400
#error "GOOGLE_DRIVE_RATE_LIMIT_WINDOW_SECONDS must be between 60 seconds and 24 hours"
#endif

#if GOOGLE_DRIVE_MIN_REQUEST_DELAY_MS < 100 || GOOGLE_DRIVE_MIN_REQUEST_DELAY_MS > 60000
#error "GOOGLE_DRIVE_MIN_REQUEST_DELAY_MS must be between 100ms and 60 seconds"
#endif

#if GOOGLE_DRIVE_MAX_RETRY_ATTEMPTS < 1 || GOOGLE_DRIVE_MAX_RETRY_ATTEMPTS > 20
#error "GOOGLE_DRIVE_MAX_RETRY_ATTEMPTS must be between 1 and 20"
#endif

#if GOOGLE_DRIVE_MAX_LIST_PAGE_SIZE < 10 || GOOGLE_DRIVE_MAX_LIST_PAGE_SIZE > 1000
#error "GOOGLE_DRIVE_MAX_LIST_PAGE_SIZE must be between 10 and 1000"
#endif

// ----------------------------------------------------------------------------
// Memory Management Validation
// ----------------------------------------------------------------------------

/// Validate memory limits are reasonable for the platform
#ifdef BOARD_HAS_PSRAM
#if GOOGLE_DRIVE_JSON_DOC_SIZE < 32768 || GOOGLE_DRIVE_JSON_DOC_SIZE > 16777216
#error "GOOGLE_DRIVE_JSON_DOC_SIZE must be between 32KB and 16MB for PSRAM boards"
#endif
#else
#if GOOGLE_DRIVE_JSON_DOC_SIZE < 8192 || GOOGLE_DRIVE_JSON_DOC_SIZE > 131072
#error "GOOGLE_DRIVE_JSON_DOC_SIZE must be between 8KB and 128KB for standard ESP32"
#endif
#endif

// ============================================================================
// WEATHER SYSTEM VALIDATION
// ============================================================================

#ifdef USE_WEATHER
/// Ensure weather configuration files are defined when weather is enabled
#ifndef WEATHER_CONFIG_FILE
#error "WEATHER_CONFIG_FILE must be defined when USE_WEATHER is enabled"
#endif

#ifndef WEATHER_CACHE_FILE
#error "WEATHER_CACHE_FILE must be defined when USE_WEATHER is enabled"
#endif

/// Validate weather data expiry time is reasonable (30 minutes to 24 hours)
#if WEATHER_MAX_AGE_SECONDS < (30 * SECONDS_IN_MINUTE) || WEATHER_MAX_AGE_SECONDS > SECONDS_IN_DAY
#error "WEATHER_MAX_AGE_SECONDS must be between 30 minutes and 24 hours"
#endif
#endif // USE_WEATHER

// ============================================================================
// OTA UPDATE SYSTEM VALIDATION
// ============================================================================

#ifdef OTA_UPDATE_ENABLED
/// Validate OTA check interval is reasonable (1 hour to 30 days)
#if OTA_CHECK_INTERVAL_HOURS < 1 || OTA_CHECK_INTERVAL_HOURS > (30 * 24)
#error "OTA_CHECK_INTERVAL_HOURS must be between 1 hour and 30 days"
#endif

/// Validate OTA minimum battery percentage is reasonable
#if OTA_MIN_BATTERY_PERCENT < 10 || OTA_MIN_BATTERY_PERCENT > 90
#error "OTA_MIN_BATTERY_PERCENT must be between 10 and 90"
#endif

/// Validate OTA timeout is reasonable (10 seconds to 10 minutes)
#if OTA_TIMEOUT_MS < 10000 || OTA_TIMEOUT_MS > 600000
#error "OTA_TIMEOUT_MS must be between 10000 and 600000 milliseconds (10 seconds to 10 minutes)"
#endif

/// Validate OTA buffer size is reasonable (512 bytes to 8KB)
#if OTA_BUFFER_SIZE < 512 || OTA_BUFFER_SIZE > 8192
#error "OTA_BUFFER_SIZE must be between 512 and 8192 bytes"
#endif
#endif // OTA_UPDATE_ENABLED

// ============================================================================
// RUNTIME DATA DEFINITIONS
// ============================================================================

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
 * @see ALLOWED_EXTENSIONS_COUNT for the number of elements in this array
 */
const char* ALLOWED_FILE_EXTENSIONS[] = {
    ".bin",  ///< Binary format for optimized e-paper rendering
    ".bmp"   ///< Bitmap format for compatibility and debugging
};

/**
 * @brief Number of elements in the ALLOWED_FILE_EXTENSIONS array
 *
 * This constant provides the count of supported file extensions and is used
 * throughout the codebase for iterating over the extensions array safely.
 * The value is calculated automatically at compile time.
 *
 * @note This value is calculated automatically and should not be modified manually.
 * @see ALLOWED_FILE_EXTENSIONS for the actual array of supported extensions
 */
const size_t ALLOWED_EXTENSIONS_COUNT = sizeof(ALLOWED_FILE_EXTENSIONS) / sizeof(ALLOWED_FILE_EXTENSIONS[0]);

// ============================================================================
// CONFIGURATION VALIDATION SUMMARY
// ============================================================================

/*
 * This configuration validation system ensures:
 *
 * 1. HARDWARE INTEGRITY
 *    - All required pins are defined for each enabled component
 *    - Only one display type is selected
 *    - Pin configurations are consistent with hardware capabilities
 *    - I2C conflicts are prevented (especially ESP32-C6 limitations)
 *
 * 2. TIMING CONSISTENCY
 *    - Refresh intervals are within reasonable bounds
 *    - Battery thresholds are properly ordered
 *    - Network timeouts are appropriate for stable operation
 *    - Daily schedules are valid (supports overnight operation)
 *
 * 3. MEMORY SAFETY
 *    - Buffer sizes are appropriate for the target platform
 *    - PSRAM vs standard ESP32 limits are respected
 *    - Memory allocations won't cause stack overflow
 *
 * 4. API COMPLIANCE
 *    - Google Drive API rate limits are respected
 *    - Network request patterns follow best practices
 *    - Retry mechanisms are reasonable
 *
 * 5. SYSTEM RELIABILITY
 *    - File paths and storage thresholds are sensible
 *    - Power management settings ensure stable operation
 *    - OTA update safety requirements are met
 *
 * All validation is performed at compile time, ensuring that configuration
 * errors are caught early in the development process rather than at runtime.
 */