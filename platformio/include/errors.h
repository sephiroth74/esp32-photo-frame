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

#ifndef __PHOTO_FRAME_ERRORS_H__
#define __PHOTO_FRAME_ERRORS_H__

#include "_locale.h"
#include <Arduino.h>
#include <cstdint>

// Forward declarations
class String;

namespace photo_frame {

/**
 * @brief Error severity levels for granular error reporting.
 */
enum error_severity {
    ERROR_SEVERITY_INFO     = 0, ///< Informational messages
    ERROR_SEVERITY_WARNING  = 1, ///< Warning conditions
    ERROR_SEVERITY_ERROR    = 2, ///< Error conditions
    ERROR_SEVERITY_CRITICAL = 3  ///< Critical system errors
};

/**
 * @brief Error categories for better error classification.
 */
enum error_category {
    ERROR_CATEGORY_GENERAL        = 0, ///< General errors
    ERROR_CATEGORY_NETWORK        = 1, ///< Network/WiFi related errors
    ERROR_CATEGORY_STORAGE        = 2, ///< SD card/file system errors
    ERROR_CATEGORY_HARDWARE       = 3, ///< Hardware component errors
    ERROR_CATEGORY_CONFIG         = 4, ///< Configuration validation errors
    ERROR_CATEGORY_AUTHENTICATION = 5, ///< Authentication/JWT errors
    ERROR_CATEGORY_BATTERY        = 6, ///< Battery related errors
    ERROR_CATEGORY_DISPLAY        = 7  ///< Display/rendering errors
#ifdef OTA_UPDATE_ENABLED
    ,
    ERROR_CATEGORY_OTA            = 8  ///< OTA update errors
#endif // OTA_UPDATE_ENABLED
};

/**
 * @brief Enhanced error class for photo frame operations.
 *
 * This class encapsulates comprehensive error information including message,
 * code, severity, category, timing, and context. It provides both backward
 * compatibility and enhanced granular error reporting capabilities.
 */
typedef class photo_frame_error {
  public:
    const char* message; ///< Human-readable error message
    uint16_t code;       ///< Numeric error code for identification

    // Enhanced fields for granular reporting
    error_severity severity; ///< Error severity level
    error_category category; ///< Error category for classification
    uint32_t timestamp;      ///< When error occurred (millis())
    const char* context;     ///< Additional context/details
    const char* source_file; ///< Source file where error occurred
    uint16_t source_line;    ///< Source line where error occurred

    // Default constructor - backward compatible
    photo_frame_error() :
        message(TXT_NO_ERROR),
        code(0),
        severity(ERROR_SEVERITY_INFO),
        category(ERROR_CATEGORY_GENERAL),
        timestamp(0),
        context(nullptr),
        source_file(nullptr),
        source_line(0) {}

    /**
     * @brief Constructor with message and code (backward compatible).
     * @param msg Human-readable error message
     * @param err_code Numeric error code
     */
    constexpr photo_frame_error(const char* msg, uint16_t err_code) :
        message(msg),
        code(err_code),
        severity(ERROR_SEVERITY_ERROR),
        category(ERROR_CATEGORY_GENERAL),
        timestamp(0),
        context(nullptr),
        source_file(nullptr),
        source_line(0) {}

    /**
     * @brief Enhanced constructor with full error details.
     * @param msg Human-readable error message
     * @param err_code Numeric error code
     * @param sev Error severity level
     * @param cat Error category
     * @param ctx Additional context (optional)
     * @param file Source file name (optional)
     * @param line Source line number (optional)
     */
    constexpr photo_frame_error(const char* msg,
                                uint16_t err_code,
                                error_severity sev,
                                error_category cat,
                                const char* ctx  = nullptr,
                                const char* file = nullptr,
                                uint16_t line    = 0) :
        message(msg),
        code(err_code),
        severity(sev),
        category(cat),
        timestamp(0), // Will be set when error is created
        context(ctx),
        source_file(file),
        source_line(line) {}

    /**
     * @brief Equality operator comparing error codes (backward compatible).
     * @param other Another photo_frame_error to compare with
     * @return True if both errors have the same code
     */
    constexpr bool operator==(const photo_frame_error& other) const {
        return (this->code == other.code);
    }

    /**
     * @brief Inequality operator.
     * @param other Another photo_frame_error to compare with
     * @return True if errors are different (opposite of operator==)
     */
    constexpr bool operator!=(const photo_frame_error& other) const { return !(*this == other); }

    /**
     * @brief Set the timestamp to current time.
     */
    void set_timestamp() { timestamp = millis(); }

    /**
     * @brief Log detailed error information to Serial.
     */
    void log_detailed() const {
        Serial.print(F("["));
        Serial.print(severity_to_string());
        Serial.print(F("] "));
        Serial.print(F("Error "));
        Serial.print(code);
        Serial.print(F(" ("));
        Serial.print(category_to_string());
        Serial.print(F("): "));
        Serial.println(message);

        if (context) {
            Serial.print(F("  Context: "));
            Serial.println(context);
        }

        if (source_file && source_line > 0) {
            Serial.print(F("  Location: "));
            Serial.print(source_file);
            Serial.print(F(":"));
            Serial.println(source_line);
        }

        if (timestamp > 0) {
            Serial.print(F("  Time: "));
            Serial.print(timestamp);
            Serial.println(F("ms"));
        }
    }

    /**
     * @brief Format error for display purposes.
     * @return Formatted string for display
     */
    String format_for_display() const {
        String result = String(message);
        if (context) {
            result += "\n";
            result += String(context);
        }
        return result;
    }

    /**
     * @brief Check if this is a critical error.
     * @return True if error is critical
     */
    bool is_critical() const { return severity == ERROR_SEVERITY_CRITICAL; }

  private:
    /**
     * @brief Convert severity enum to string.
     * @return String representation of severity
     */
    const char* severity_to_string() const {
        switch (severity) {
        case ERROR_SEVERITY_INFO:     return "INFO";
        case ERROR_SEVERITY_WARNING:  return "WARN";
        case ERROR_SEVERITY_ERROR:    return "ERROR";
        case ERROR_SEVERITY_CRITICAL: return "CRITICAL";
        default:                      return "UNKNOWN";
        }
    }

    /**
     * @brief Convert category enum to string.
     * @return String representation of category
     */
    const char* category_to_string() const {
        switch (category) {
        case ERROR_CATEGORY_GENERAL:        return "General";
        case ERROR_CATEGORY_NETWORK:        return "Network";
        case ERROR_CATEGORY_STORAGE:        return "Storage";
        case ERROR_CATEGORY_HARDWARE:       return "Hardware";
        case ERROR_CATEGORY_CONFIG:         return "Config";
        case ERROR_CATEGORY_AUTHENTICATION: return "Auth";
        case ERROR_CATEGORY_BATTERY:        return "Battery";
        case ERROR_CATEGORY_DISPLAY:        return "Display";
#ifdef OTA_UPDATE_ENABLED
        case ERROR_CATEGORY_OTA:            return "OTA";
#endif // OTA_UPDATE_ENABLED
        default:                            return "Unknown";
        }
    }
} photo_frame_error_t;

// Helper macros for creating enhanced errors with location info
#define MAKE_ERROR(msg, code, sev, cat)                                                            \
    photo_frame_error(msg, (uint16_t)(code), sev, cat, nullptr, __FILE__, __LINE__)

#define MAKE_ERROR_WITH_CONTEXT(msg, code, sev, cat, ctx)                                          \
    photo_frame_error(msg, (uint16_t)(code), sev, cat, ctx, __FILE__, __LINE__)

/**
 * @brief Namespace containing predefined error instances.
 *
 * This namespace provides a collection of commonly used error types
 * throughout the photo frame system. Each error has a predefined
 * message, code, severity, and category for consistent error handling.
 */
namespace error_type {
// clang-format off
// No error
const photo_frame_error None{TXT_NO_ERROR, 0, ERROR_SEVERITY_INFO, ERROR_CATEGORY_GENERAL};

// Storage/SD Card errors (Critical/Error severity)
const photo_frame_error CardMountFailed{TXT_CARD_MOUNT_FAILED, 3, ERROR_SEVERITY_CRITICAL, ERROR_CATEGORY_STORAGE};
const photo_frame_error NoSdCardAttached{TXT_NO_SD_CARD_ATTACHED, 4, ERROR_SEVERITY_CRITICAL, ERROR_CATEGORY_STORAGE};
const photo_frame_error UnknownSdCardType{TXT_UNKNOWN_SD_CARD_TYPE, 5, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_STORAGE};
const photo_frame_error CardOpenFileFailed{TXT_CARD_OPEN_FILE_FAILED, 6, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_STORAGE};
const photo_frame_error SdCardFileNotFound{TXT_SD_CARD_FILE_NOT_FOUND, 7, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_STORAGE};
const photo_frame_error SdCardFileOpenFailed{TXT_SD_CARD_FILE_OPEN_FAILED, 8, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_STORAGE};
const photo_frame_error SdCardFileCreateFailed{TXT_SD_CARD_FILE_CREATE_FAILED, 24, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_STORAGE};
const photo_frame_error SdCardDirCreateFailed{TXT_SD_CARD_DIR_CREATE_FAILED, 36, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_STORAGE};
const photo_frame_error CardTocOpenFileFailed{TXT_CARD_TOC_OPEN_FILE_FAILED, 11, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_STORAGE};
const photo_frame_error FileOpenFailed{TXT_FILE_OPEN_FAILED, 20, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_STORAGE};
const photo_frame_error PreferencesOpenFailed{TXT_PREFERENCES_OPEN_FAILED, 12, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_STORAGE};

// LittleFS errors (110-119)
const photo_frame_error LittleFSInitFailed{TXT_LITTLEFS_INIT_FAILED, 110, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_STORAGE};
const photo_frame_error LittleFSFileCreateFailed{TXT_LITTLEFS_FILE_CREATE_FAILED, 111, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_STORAGE};
const photo_frame_error LittleFSFileOpenFailed{TXT_LITTLEFS_FILE_OPEN_FAILED, 112, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_STORAGE};

// Display/Image errors
const photo_frame_error ImageFormatNotSupported{TXT_IMAGE_FORMAT_NOT_SUPPORTED, 9, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_DISPLAY};
const photo_frame_error NoImagesFound{TXT_NO_IMAGES_FOUND, 13, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_DISPLAY};

// Battery errors (Critical severity)
const photo_frame_error BatteryLevelCritical{TXT_BATTERY_LEVEL_CRITICAL, 10, ERROR_SEVERITY_CRITICAL, ERROR_CATEGORY_BATTERY};
const photo_frame_error BatteryEmpty{TXT_BATTERY_EMPTY, 14, ERROR_SEVERITY_CRITICAL, ERROR_CATEGORY_BATTERY};

// Hardware errors
const photo_frame_error RTCInitializationFailed{TXT_RTC_MODULE_NOT_FOUND, 15, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_HARDWARE};

// Authentication errors
const photo_frame_error JwtCreationFailed{TXT_JWT_CREATION_FAILED, 16, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_AUTHENTICATION};
const photo_frame_error TokenMissing{TXT_TOKEN_MISSING, 19, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_AUTHENTICATION};

// Network errors
const photo_frame_error HttpPostFailed{TXT_HTTP_POST_FAILED, 17, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_NETWORK};
const photo_frame_error HttpConnectFailed{TXT_HTTP_CONNECT_FAILED, 21, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_NETWORK};
const photo_frame_error HttpGetFailed{TXT_HTTP_GET_FAILED, 22, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_NETWORK};
const photo_frame_error DownloadFailed{TXT_DOWNLOAD_FAILED, 23, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_NETWORK};
const photo_frame_error SslCertificateLoadFailed{TXT_SSL_CERTIFICATE_LOAD_FAILED, 25, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_NETWORK};
const photo_frame_error RateLimitTimeoutExceeded{TXT_RATE_LIMIT_TIMEOUT_EXCEEDED, 26, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_NETWORK};
const photo_frame_error WifiCredentialsNotFound{TXT_WIFI_CREDENTIALS_NOT_FOUND, 27, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_NETWORK};
const photo_frame_error WifiConnectionFailed{TXT_WIFI_CONNECTION_FAILED, 28, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_NETWORK};


// General errors
const photo_frame_error JsonParseFailed{TXT_JSON_PARSE_FAILED, 18, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_GENERAL};

// Configuration validation errors
const photo_frame_error ConfigMissingSection{TXT_CONFIG_MISSING_SECTION, 29, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_CONFIG};
const photo_frame_error ConfigMissingField{TXT_CONFIG_MISSING_FIELD, 30, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_CONFIG};
const photo_frame_error ConfigInvalidEmail{TXT_CONFIG_INVALID_EMAIL, 31, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_CONFIG};
const photo_frame_error ConfigInvalidPemKey{TXT_CONFIG_INVALID_PEM_KEY, 32, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_CONFIG};
const photo_frame_error ConfigInvalidPath{TXT_CONFIG_INVALID_PATH, 33, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_CONFIG};
const photo_frame_error ConfigInvalidFilename{TXT_CONFIG_INVALID_FILENAME, 34, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_CONFIG};
const photo_frame_error ConfigValueOutOfRange{TXT_CONFIG_VALUE_OUT_OF_RANGE, 35, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_CONFIG};


// OAuth/Authentication specific errors (40-49) - Keep only the ones actually used
const photo_frame_error OAuthTokenExpired{TXT_OAUTH_TOKEN_EXPIRED, 40, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_AUTHENTICATION};
const photo_frame_error OAuthTokenInvalid{TXT_OAUTH_TOKEN_INVALID, 41, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_AUTHENTICATION};
const photo_frame_error OAuthRefreshTokenInvalid{TXT_OAUTH_REFRESH_TOKEN_INVALID, 43, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_AUTHENTICATION};
const photo_frame_error OAuthScopeInsufficient{TXT_OAUTH_SCOPE_INSUFFICIENT, 44, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_AUTHENTICATION};
const photo_frame_error OAuthTokenRequestFailed{TXT_OAUTH_TOKEN_REQUEST_FAILED, 47, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_AUTHENTICATION};

// Google Drive API specific errors (50-69) - Keep only the ones actually used
const photo_frame_error GoogleDriveApiQuotaExceeded{TXT_GOOGLE_DRIVE_API_QUOTA_EXCEEDED, 50, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_NETWORK};
const photo_frame_error GoogleDriveApiRateLimited{TXT_GOOGLE_DRIVE_API_RATE_LIMITED, 51, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_NETWORK};
const photo_frame_error GoogleDriveFileNotFound{TXT_GOOGLE_DRIVE_FILE_NOT_FOUND, 52, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_NETWORK};
const photo_frame_error GoogleDrivePermissionDenied{TXT_GOOGLE_DRIVE_PERMISSION_DENIED, 54, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_NETWORK};
const photo_frame_error GoogleDriveStorageQuotaExceeded{TXT_GOOGLE_DRIVE_STORAGE_QUOTA_EXCEEDED, 55, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_NETWORK};
const photo_frame_error GoogleDriveApiDisabled{TXT_GOOGLE_DRIVE_API_DISABLED, 56, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_NETWORK};
const photo_frame_error GoogleDriveApiInternalError{TXT_GOOGLE_DRIVE_API_INTERNAL_ERROR, 60, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_NETWORK};
const photo_frame_error GoogleDriveNetworkTimeout{TXT_GOOGLE_DRIVE_NETWORK_TIMEOUT, 61, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_NETWORK};

// HTTP specific errors (70-79)
const photo_frame_error HttpUnauthorized{TXT_HTTP_UNAUTHORIZED, 70, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_NETWORK};
const photo_frame_error HttpForbidden{TXT_HTTP_FORBIDDEN, 71, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_NETWORK};
const photo_frame_error HttpNotFound{TXT_HTTP_NOT_FOUND, 72, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_NETWORK};
const photo_frame_error HttpTooManyRequests{TXT_HTTP_TOO_MANY_REQUESTS, 73, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_NETWORK};
const photo_frame_error HttpInternalServerError{TXT_HTTP_INTERNAL_SERVER_ERROR, 74, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_NETWORK};
const photo_frame_error HttpBadGateway{TXT_HTTP_BAD_GATEWAY, 75, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_NETWORK};
const photo_frame_error HttpServiceUnavailable{TXT_HTTP_SERVICE_UNAVAILABLE, 76, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_NETWORK};
const photo_frame_error HttpGatewayTimeout{TXT_HTTP_GATEWAY_TIMEOUT, 77, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_NETWORK};
const photo_frame_error HttpBadRequest{TXT_HTTP_BAD_REQUEST, 78, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_NETWORK};

// Image Processing specific errors (80-99)
const photo_frame_error ImageFileCorrupted{TXT_IMAGE_FILE_CORRUPTED, 80, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_DISPLAY};
const photo_frame_error ImageFileTooLarge{TXT_IMAGE_FILE_TOO_LARGE, 81, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_DISPLAY};
const photo_frame_error ImageDimensionsInvalid{TXT_IMAGE_DIMENSIONS_INVALID, 82, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_DISPLAY};
const photo_frame_error ImageDimensionsMismatch{TXT_IMAGE_DIMENSIONS_MISMATCH, 83, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_DISPLAY};
const photo_frame_error ImageColorDepthUnsupported{TXT_IMAGE_COLOR_DEPTH_UNSUPPORTED, 84, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_DISPLAY};
const photo_frame_error ImagePixelDataCorrupted{TXT_IMAGE_PIXEL_DATA_CORRUPTED, 85, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_DISPLAY};
const photo_frame_error ImageFileSeekFailed{TXT_IMAGE_FILE_SEEK_FAILED, 86, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_DISPLAY};
const photo_frame_error ImageFileReadFailed{TXT_IMAGE_FILE_READ_FAILED, 87, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_DISPLAY};
const photo_frame_error ImageBufferOverflow{TXT_IMAGE_BUFFER_OVERFLOW, 88, ERROR_SEVERITY_CRITICAL, ERROR_CATEGORY_DISPLAY};
const photo_frame_error ImageDisplayWriteFailed{TXT_IMAGE_DISPLAY_WRITE_FAILED, 91, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_DISPLAY};
const photo_frame_error ImageFileHeaderInvalid{TXT_IMAGE_FILE_HEADER_INVALID, 92, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_DISPLAY};
const photo_frame_error ImageFileEmpty{TXT_IMAGE_FILE_EMPTY, 93, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_DISPLAY};
const photo_frame_error ImageFileTruncated{TXT_IMAGE_FILE_TRUNCATED, 94, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_DISPLAY};
const photo_frame_error ImageMemoryAllocationFailed{TXT_IMAGE_MEMORY_ALLOCATION_FAILED, 95, ERROR_SEVERITY_CRITICAL, ERROR_CATEGORY_DISPLAY};
const photo_frame_error ImageConversionFailed{TXT_IMAGE_CONVERSION_FAILED, 97, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_DISPLAY};
const photo_frame_error ImageResolutionTooHigh{TXT_IMAGE_RESOLUTION_TOO_HIGH, 98, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_DISPLAY};
const photo_frame_error ImageProcessingAborted{TXT_IMAGE_PROCESSING_ABORTED, 99, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_DISPLAY};
const photo_frame_error ImageDimensionsNotProvided{TXT_IMAGE_DIMENSIONS_NOT_PROVIDED, 100, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_DISPLAY};

// Granular Image Rendering errors (200-209) - Function-specific error codes
const photo_frame_error BinaryRenderingFailed{TXT_IMAGE_FORMAT_NOT_SUPPORTED, 200, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_DISPLAY};
const photo_frame_error BitmapRenderingFailed{TXT_IMAGE_FORMAT_NOT_SUPPORTED, 201, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_DISPLAY};
const photo_frame_error BufferedBinaryRenderingFailed{TXT_IMAGE_FORMAT_NOT_SUPPORTED, 202, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_DISPLAY};
const photo_frame_error BufferedBitmapRenderingFailed{TXT_IMAGE_FORMAT_NOT_SUPPORTED, 203, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_DISPLAY};
const photo_frame_error BinaryFileInvalidSize{TXT_IMAGE_FORMAT_NOT_SUPPORTED, 204, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_DISPLAY};
const photo_frame_error BitmapHeaderCorrupted{TXT_IMAGE_FORMAT_NOT_SUPPORTED, 205, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_DISPLAY};
const photo_frame_error PixelReadError{TXT_IMAGE_FORMAT_NOT_SUPPORTED, 206, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_DISPLAY};
const photo_frame_error SequentialReadError{TXT_IMAGE_FORMAT_NOT_SUPPORTED, 207, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_DISPLAY};
const photo_frame_error SeekOperationFailed{TXT_IMAGE_FORMAT_NOT_SUPPORTED, 208, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_DISPLAY};
const photo_frame_error PaletteCorrupted{TXT_IMAGE_FORMAT_NOT_SUPPORTED, 209, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_DISPLAY};

// Battery/Power specific errors (300-319) - Enhanced granular errors
const photo_frame_error BatteryVoltageLow{TXT_BATTERY_VOLTAGE_LOW, 300, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_BATTERY};
const photo_frame_error BatteryVoltageUnstable{TXT_BATTERY_VOLTAGE_UNSTABLE, 301, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_BATTERY};
const photo_frame_error BatteryTemperatureHigh{TXT_BATTERY_TEMPERATURE_HIGH, 302, ERROR_SEVERITY_CRITICAL, ERROR_CATEGORY_BATTERY};
const photo_frame_error BatteryTemperatureLow{TXT_BATTERY_TEMPERATURE_LOW, 303, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_BATTERY};
const photo_frame_error BatteryAgingDetected{TXT_BATTERY_AGING_DETECTED, 304, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_BATTERY};
const photo_frame_error BatteryCalibrationNeeded{TXT_BATTERY_CALIBRATION_NEEDED, 305, ERROR_SEVERITY_INFO, ERROR_CATEGORY_BATTERY};
const photo_frame_error BatteryDischargeRateTooHigh{TXT_BATTERY_DISCHARGE_RATE_TOO_HIGH, 306, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_BATTERY};
const photo_frame_error BatteryChargeRateTooSlow{TXT_BATTERY_CHARGE_RATE_TOO_SLOW, 307, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_BATTERY};
const photo_frame_error BatteryCapacityReduced{TXT_BATTERY_CAPACITY_REDUCED, 308, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_BATTERY};
const photo_frame_error BatteryHealthPoor{TXT_BATTERY_HEALTH_POOR, 309, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_BATTERY};

// Charging specific errors (400-419)
const photo_frame_error ChargingFailed{TXT_CHARGING_FAILED, 400, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_BATTERY};
const photo_frame_error ChargerNotConnected{TXT_CHARGER_NOT_CONNECTED, 401, ERROR_SEVERITY_INFO, ERROR_CATEGORY_BATTERY};
const photo_frame_error ChargerIncompatible{TXT_CHARGER_INCOMPATIBLE, 402, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_BATTERY};
const photo_frame_error ChargingOverheat{TXT_CHARGING_OVERHEAT, 403, ERROR_SEVERITY_CRITICAL, ERROR_CATEGORY_BATTERY};
const photo_frame_error ChargingTimeout{TXT_CHARGING_TIMEOUT, 404, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_BATTERY};
const photo_frame_error ChargeCurrentTooHigh{TXT_CHARGE_CURRENT_TOO_HIGH, 405, ERROR_SEVERITY_CRITICAL, ERROR_CATEGORY_BATTERY};
const photo_frame_error ChargeCurrentTooLow{TXT_CHARGE_CURRENT_TOO_LOW, 406, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_BATTERY};
const photo_frame_error ChargingCircuitFault{TXT_CHARGING_CIRCUIT_FAULT, 407, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_BATTERY};
const photo_frame_error BatteryNotDetected{TXT_BATTERY_NOT_DETECTED, 408, ERROR_SEVERITY_CRITICAL, ERROR_CATEGORY_BATTERY};
const photo_frame_error BatteryAuthenticationFailed{TXT_BATTERY_AUTHENTICATION_FAILED, 409, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_BATTERY};

#ifdef OTA_UPDATE_ENABLED
// OTA Update errors - extern declarations (defined in errors.cpp)
extern const photo_frame_error OtaInitFailed;
extern const photo_frame_error OtaVersionCheckFailed;
extern const photo_frame_error OtaInvalidResponse;
extern const photo_frame_error OtaUpdateInProgress;
extern const photo_frame_error InsufficientSpace;
extern const photo_frame_error OtaPartitionNotFound;
extern const photo_frame_error OtaBeginFailed;
extern const photo_frame_error OtaDownloadFailed;
extern const photo_frame_error OtaWriteFailed;
extern const photo_frame_error OtaEndFailed;
extern const photo_frame_error OtaSetBootPartitionFailed;
extern const photo_frame_error OtaVersionIncompatible;
extern const photo_frame_error NoUpdateNeeded;
#endif // OTA_UPDATE_ENABLED

// Add more errors here
// clang-format on
} // namespace error_type

/**
 * @brief Helper functions for error mapping and creation
 *
 * This namespace provides utility functions for creating and mapping various
 * types of errors throughout the photo frame system. These functions help
 * standardize error creation and provide consistent error handling.
 */
namespace error_utils {

/**
 * @brief Map HTTP status code to appropriate photo frame error
 * @param statusCode HTTP status code received from server
 * @param context Optional context information for error details
 * @return Corresponding photo_frame_error with appropriate severity and category
 */
photo_frame_error map_http_status_to_error(int statusCode, const char* context = nullptr);

/**
 * @brief Map Google Drive API error response to photo frame error
 * @param statusCode HTTP status code from Google Drive API
 * @param responseBody Optional response body containing error details
 * @param context Optional context information
 * @return Corresponding photo_frame_error with Google Drive specific details
 */
photo_frame_error map_google_drive_error(int statusCode,
                                         const char* responseBody = nullptr,
                                         const char* context      = nullptr);

/**
 * @brief Create OAuth-specific error from error type string
 * @param errorType OAuth error type (e.g., "invalid_grant", "invalid_scope")
 * @param context Optional context information
 * @return OAuth-specific photo_frame_error with authentication category
 */
photo_frame_error create_oauth_error(const char* errorType, const char* context = nullptr);

/**
 * @brief Create image processing error with detailed context
 * @param errorType Type of image error encountered
 * @param filename Optional filename where error occurred
 * @param dimensions Optional image dimensions for context
 * @param context Optional additional context information
 * @return Image-specific photo_frame_error with display category
 */
photo_frame_error create_image_error(const char* errorType,
                                     const char* filename   = nullptr,
                                     const char* dimensions = nullptr,
                                     const char* context    = nullptr);

/**
 * @brief Validate image dimensions against maximum allowed values
 * @param width Image width in pixels
 * @param height Image height in pixels
 * @param maxWidth Maximum allowed width
 * @param maxHeight Maximum allowed height
 * @param filename Optional filename for error context
 * @return None if valid, appropriate error if dimensions exceed limits
 */
photo_frame_error validate_image_dimensions(int width,
                                            int height,
                                            int maxWidth,
                                            int maxHeight,
                                            const char* filename = nullptr);

/**
 * @brief Validate image file size against expected range
 * @param fileSize Actual file size in bytes
 * @param expectedMinSize Minimum expected file size
 * @param expectedMaxSize Maximum expected file size
 * @param filename Optional filename for error context
 * @return None if valid, appropriate error if size is out of range
 */
photo_frame_error validate_image_file_size(size_t fileSize,
                                           size_t expectedMinSize,
                                           size_t expectedMaxSize,
                                           const char* filename = nullptr);

/**
 * @brief Create battery-related error with comprehensive details
 * @param errorType Type of battery error
 * @param voltage Battery voltage (-1 if not applicable)
 * @param percentage Battery percentage (-1 if not applicable)
 * @param temperature Battery temperature (-999 if not applicable)
 * @param context Optional additional context
 * @return Battery-specific photo_frame_error with battery category
 */
photo_frame_error create_battery_error(const char* errorType,
                                       float voltage       = -1,
                                       float percentage    = -1,
                                       float temperature   = -999,
                                       const char* context = nullptr);

/**
 * @brief Validate battery voltage against safe operating range
 * @param voltage Current battery voltage
 * @param minVoltage Minimum safe voltage
 * @param maxVoltage Maximum safe voltage
 * @param context Optional context information
 * @return None if valid, appropriate error if voltage is out of safe range
 */
photo_frame_error validate_battery_voltage(float voltage,
                                           float minVoltage,
                                           float maxVoltage,
                                           const char* context = nullptr);

/**
 * @brief Validate battery temperature against safe operating range
 * @param temperature Current battery temperature in Celsius
 * @param minTemp Minimum safe temperature
 * @param maxTemp Maximum safe temperature
 * @param context Optional context information
 * @return None if valid, appropriate error if temperature is out of safe range
 */
photo_frame_error validate_battery_temperature(float temperature,
                                               float minTemp,
                                               float maxTemp,
                                               const char* context = nullptr);

/**
 * @brief Create charging-related error with electrical parameters
 * @param errorType Type of charging error
 * @param current Charging current (-1 if not applicable)
 * @param voltage Charging voltage (-1 if not applicable)
 * @param context Optional additional context
 * @return Charging-specific photo_frame_error with battery category
 */
photo_frame_error create_charging_error(const char* errorType,
                                        float current       = -1,
                                        float voltage       = -1,
                                        const char* context = nullptr);

} // namespace error_utils

} // namespace photo_frame

#endif // __PHOTO_FRAME_ERRORS_H__