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

// SD Card specific errors (100-119) - Enhanced granular errors
const photo_frame_error SdCardWriteProtected{TXT_SD_CARD_WRITE_PROTECTED, 100, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_STORAGE};
const photo_frame_error SdCardCorrupted{TXT_SD_CARD_CORRUPTED, 101, ERROR_SEVERITY_CRITICAL, ERROR_CATEGORY_STORAGE};
const photo_frame_error SdCardInsufficientSpace{TXT_SD_CARD_INSUFFICIENT_SPACE, 102, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_STORAGE};
const photo_frame_error SdCardSlowResponse{TXT_SD_CARD_SLOW_RESPONSE, 103, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_STORAGE};
const photo_frame_error SdCardReadOnly{TXT_SD_CARD_READ_ONLY, 104, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_STORAGE};
const photo_frame_error SdCardBadSector{TXT_SD_CARD_BAD_SECTOR, 105, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_STORAGE};
const photo_frame_error SdCardSizeInvalid{TXT_SD_CARD_SIZE_INVALID, 106, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_STORAGE};
const photo_frame_error SdCardInitFailed{TXT_SD_CARD_INIT_FAILED, 107, ERROR_SEVERITY_CRITICAL, ERROR_CATEGORY_STORAGE};
const photo_frame_error SdCardVersionUnsupported{TXT_SD_CARD_VERSION_UNSUPPORTED, 108, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_STORAGE};
const photo_frame_error SdCardFileSystemUnsupported{TXT_SD_CARD_FILESYSTEM_UNSUPPORTED, 109, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_STORAGE};
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

// Network/WiFi specific errors (120-139) - Enhanced granular errors
const photo_frame_error WifiSignalTooWeak{TXT_WIFI_SIGNAL_TOO_WEAK, 120, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_NETWORK};
const photo_frame_error WifiAuthenticationFailed{TXT_WIFI_AUTHENTICATION_FAILED, 121, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_NETWORK};
const photo_frame_error WifiDhcpFailed{TXT_WIFI_DHCP_FAILED, 122, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_NETWORK};
const photo_frame_error WifiDnsResolutionFailed{TXT_WIFI_DNS_RESOLUTION_FAILED, 123, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_NETWORK};
const photo_frame_error WifiNetworkNotFound{TXT_WIFI_NETWORK_NOT_FOUND, 124, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_NETWORK};
const photo_frame_error WifiPasswordIncorrect{TXT_WIFI_PASSWORD_INCORRECT, 125, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_NETWORK};
const photo_frame_error WifiChannelBusy{TXT_WIFI_CHANNEL_BUSY, 126, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_NETWORK};
const photo_frame_error WifiFrequencyNotSupported{TXT_WIFI_FREQUENCY_NOT_SUPPORTED, 127, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_NETWORK};
const photo_frame_error HttpRequestTimeout{TXT_HTTP_REQUEST_TIMEOUT, 128, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_NETWORK};
const photo_frame_error HttpInvalidResponse{TXT_HTTP_INVALID_RESPONSE, 129, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_NETWORK};
const photo_frame_error SslHandshakeFailed{TXT_SSL_HANDSHAKE_FAILED, 130, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_NETWORK};
const photo_frame_error NetworkInterfaceDown{TXT_NETWORK_INTERFACE_DOWN, 131, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_NETWORK};
const photo_frame_error NetworkConfigInvalid{TXT_NETWORK_CONFIG_INVALID, 132, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_NETWORK};
const photo_frame_error ProxyConnectionFailed{TXT_PROXY_CONNECTION_FAILED, 133, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_NETWORK};
const photo_frame_error NetworkTimeoutExceeded{TXT_NETWORK_TIMEOUT_EXCEEDED, 134, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_NETWORK};

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

// Configuration specific errors (140-159) - Enhanced granular errors
const photo_frame_error ConfigFileCorrupted{TXT_CONFIG_FILE_CORRUPTED, 140, ERROR_SEVERITY_CRITICAL, ERROR_CATEGORY_CONFIG};
const photo_frame_error ConfigJsonSyntaxError{TXT_CONFIG_JSON_SYNTAX_ERROR, 141, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_CONFIG};
const photo_frame_error ConfigVersionMismatch{TXT_CONFIG_VERSION_MISMATCH, 142, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_CONFIG};
const photo_frame_error ConfigFieldTypeMismatch{TXT_CONFIG_FIELD_TYPE_MISMATCH, 143, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_CONFIG};
const photo_frame_error ConfigEncryptionKeyInvalid{TXT_CONFIG_ENCRYPTION_KEY_INVALID, 144, ERROR_SEVERITY_CRITICAL, ERROR_CATEGORY_CONFIG};
const photo_frame_error ConfigBackupFailed{TXT_CONFIG_BACKUP_FAILED, 145, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_CONFIG};
const photo_frame_error ConfigRestoreFailed{TXT_CONFIG_RESTORE_FAILED, 146, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_CONFIG};
const photo_frame_error ConfigValidationFailed{TXT_CONFIG_VALIDATION_FAILED, 147, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_CONFIG};
const photo_frame_error ConfigDefaultsMissing{TXT_CONFIG_DEFAULTS_MISSING, 148, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_CONFIG};
const photo_frame_error ConfigSchemaInvalid{TXT_CONFIG_SCHEMA_INVALID, 149, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_CONFIG};
const photo_frame_error ConfigAccessDenied{TXT_CONFIG_ACCESS_DENIED, 150, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_CONFIG};
const photo_frame_error ConfigFormatUnsupported{TXT_CONFIG_FORMAT_UNSUPPORTED, 151, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_CONFIG};
const photo_frame_error ConfigSizeLimitExceeded{TXT_CONFIG_SIZE_LIMIT_EXCEEDED, 152, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_CONFIG};
const photo_frame_error ConfigDependencyMissing{TXT_CONFIG_DEPENDENCY_MISSING, 153, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_CONFIG};
const photo_frame_error ConfigEnvironmentMismatch{TXT_CONFIG_ENVIRONMENT_MISMATCH, 154, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_CONFIG};

// OAuth/Authentication specific errors (40-49)
const photo_frame_error OAuthTokenExpired{TXT_OAUTH_TOKEN_EXPIRED, 40, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_AUTHENTICATION};
const photo_frame_error OAuthTokenInvalid{TXT_OAUTH_TOKEN_INVALID, 41, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_AUTHENTICATION};
const photo_frame_error OAuthRefreshTokenMissing{TXT_OAUTH_REFRESH_TOKEN_MISSING, 42, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_AUTHENTICATION};
const photo_frame_error OAuthRefreshTokenInvalid{TXT_OAUTH_REFRESH_TOKEN_INVALID, 43, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_AUTHENTICATION};
const photo_frame_error OAuthScopeInsufficient{TXT_OAUTH_SCOPE_INSUFFICIENT, 44, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_AUTHENTICATION};
const photo_frame_error OAuthJwtSigningFailed{TXT_OAUTH_JWT_SIGNING_FAILED, 45, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_AUTHENTICATION};
const photo_frame_error OAuthServiceAccountKeyInvalid{TXT_OAUTH_SERVICE_ACCOUNT_KEY_INVALID, 46, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_AUTHENTICATION};
const photo_frame_error OAuthTokenRequestFailed{TXT_OAUTH_TOKEN_REQUEST_FAILED, 47, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_AUTHENTICATION};
const photo_frame_error OAuthTokenRefreshFailed{TXT_OAUTH_TOKEN_REFRESH_FAILED, 48, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_AUTHENTICATION};

// Google Drive API specific errors (50-69)
const photo_frame_error GoogleDriveApiQuotaExceeded{TXT_GOOGLE_DRIVE_API_QUOTA_EXCEEDED, 50, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_NETWORK};
const photo_frame_error GoogleDriveApiRateLimited{TXT_GOOGLE_DRIVE_API_RATE_LIMITED, 51, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_NETWORK};
const photo_frame_error GoogleDriveFileNotFound{TXT_GOOGLE_DRIVE_FILE_NOT_FOUND, 52, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_NETWORK};
const photo_frame_error GoogleDriveFolderNotFound{TXT_GOOGLE_DRIVE_FOLDER_NOT_FOUND, 53, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_NETWORK};
const photo_frame_error GoogleDrivePermissionDenied{TXT_GOOGLE_DRIVE_PERMISSION_DENIED, 54, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_NETWORK};
const photo_frame_error GoogleDriveStorageQuotaExceeded{TXT_GOOGLE_DRIVE_STORAGE_QUOTA_EXCEEDED, 55, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_NETWORK};
const photo_frame_error GoogleDriveApiDisabled{TXT_GOOGLE_DRIVE_API_DISABLED, 56, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_NETWORK};
const photo_frame_error GoogleDriveFileTooBig{TXT_GOOGLE_DRIVE_FILE_TOO_BIG, 57, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_NETWORK};
const photo_frame_error GoogleDriveFileCorrupted{TXT_GOOGLE_DRIVE_FILE_CORRUPTED, 58, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_NETWORK};
const photo_frame_error GoogleDriveInvalidQuery{TXT_GOOGLE_DRIVE_INVALID_QUERY, 59, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_NETWORK};
const photo_frame_error GoogleDriveApiInternalError{TXT_GOOGLE_DRIVE_API_INTERNAL_ERROR, 60, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_NETWORK};
const photo_frame_error GoogleDriveNetworkTimeout{TXT_GOOGLE_DRIVE_NETWORK_TIMEOUT, 61, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_NETWORK};
const photo_frame_error GoogleDriveResponseMalformed{TXT_GOOGLE_DRIVE_RESPONSE_MALFORMED, 62, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_NETWORK};
const photo_frame_error GoogleDriveApiUnavailable{TXT_GOOGLE_DRIVE_API_UNAVAILABLE, 63, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_NETWORK};
const photo_frame_error GoogleDriveDownloadInterrupted{TXT_GOOGLE_DRIVE_DOWNLOAD_INTERRUPTED, 64, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_NETWORK};
const photo_frame_error GoogleDriveMetadataInvalid{TXT_GOOGLE_DRIVE_METADATA_INVALID, 65, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_NETWORK};
const photo_frame_error GoogleDriveFolderEmpty{TXT_GOOGLE_DRIVE_FOLDER_EMPTY, 66, ERROR_SEVERITY_INFO, ERROR_CATEGORY_NETWORK};
const photo_frame_error GoogleDriveApiVersionUnsupported{TXT_GOOGLE_DRIVE_API_VERSION_UNSUPPORTED, 67, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_NETWORK};

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
const photo_frame_error ImageBufferUnderflow{TXT_IMAGE_BUFFER_UNDERFLOW, 89, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_DISPLAY};
const photo_frame_error ImageRenderTimeout{TXT_IMAGE_RENDER_TIMEOUT, 90, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_DISPLAY};
const photo_frame_error ImageDisplayWriteFailed{TXT_IMAGE_DISPLAY_WRITE_FAILED, 91, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_DISPLAY};
const photo_frame_error ImageFileHeaderInvalid{TXT_IMAGE_FILE_HEADER_INVALID, 92, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_DISPLAY};
const photo_frame_error ImageFileEmpty{TXT_IMAGE_FILE_EMPTY, 93, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_DISPLAY};
const photo_frame_error ImageFileTruncated{TXT_IMAGE_FILE_TRUNCATED, 94, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_DISPLAY};
const photo_frame_error ImageMemoryAllocationFailed{TXT_IMAGE_MEMORY_ALLOCATION_FAILED, 95, ERROR_SEVERITY_CRITICAL, ERROR_CATEGORY_DISPLAY};
const photo_frame_error ImagePaletteInvalid{TXT_IMAGE_PALETTE_INVALID, 96, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_DISPLAY};
const photo_frame_error ImageConversionFailed{TXT_IMAGE_CONVERSION_FAILED, 97, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_DISPLAY};
const photo_frame_error ImageResolutionTooHigh{TXT_IMAGE_RESOLUTION_TOO_HIGH, 98, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_DISPLAY};
const photo_frame_error ImageProcessingAborted{TXT_IMAGE_PROCESSING_ABORTED, 99, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_DISPLAY};

// Battery/Power specific errors (160-179) - Enhanced granular errors
const photo_frame_error BatteryVoltageLow{TXT_BATTERY_VOLTAGE_LOW, 160, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_BATTERY};
const photo_frame_error BatteryVoltageUnstable{TXT_BATTERY_VOLTAGE_UNSTABLE, 161, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_BATTERY};
const photo_frame_error BatteryTemperatureHigh{TXT_BATTERY_TEMPERATURE_HIGH, 162, ERROR_SEVERITY_CRITICAL, ERROR_CATEGORY_BATTERY};
const photo_frame_error BatteryTemperatureLow{TXT_BATTERY_TEMPERATURE_LOW, 163, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_BATTERY};
const photo_frame_error BatteryAgingDetected{TXT_BATTERY_AGING_DETECTED, 164, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_BATTERY};
const photo_frame_error BatteryCalibrationNeeded{TXT_BATTERY_CALIBRATION_NEEDED, 165, ERROR_SEVERITY_INFO, ERROR_CATEGORY_BATTERY};
const photo_frame_error BatteryDischargeRateTooHigh{TXT_BATTERY_DISCHARGE_RATE_TOO_HIGH, 166, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_BATTERY};
const photo_frame_error BatteryChargeRateTooSlow{TXT_BATTERY_CHARGE_RATE_TOO_SLOW, 167, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_BATTERY};
const photo_frame_error BatteryCapacityReduced{TXT_BATTERY_CAPACITY_REDUCED, 168, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_BATTERY};
const photo_frame_error BatteryHealthPoor{TXT_BATTERY_HEALTH_POOR, 169, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_BATTERY};

// Charging specific errors (170-179)
const photo_frame_error ChargingFailed{TXT_CHARGING_FAILED, 170, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_BATTERY};
const photo_frame_error ChargerNotConnected{TXT_CHARGER_NOT_CONNECTED, 171, ERROR_SEVERITY_INFO, ERROR_CATEGORY_BATTERY};
const photo_frame_error ChargerIncompatible{TXT_CHARGER_INCOMPATIBLE, 172, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_BATTERY};
const photo_frame_error ChargingOverheat{TXT_CHARGING_OVERHEAT, 173, ERROR_SEVERITY_CRITICAL, ERROR_CATEGORY_BATTERY};
const photo_frame_error ChargingTimeout{TXT_CHARGING_TIMEOUT, 174, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_BATTERY};
const photo_frame_error ChargeCurrentTooHigh{TXT_CHARGE_CURRENT_TOO_HIGH, 175, ERROR_SEVERITY_CRITICAL, ERROR_CATEGORY_BATTERY};
const photo_frame_error ChargeCurrentTooLow{TXT_CHARGE_CURRENT_TOO_LOW, 176, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_BATTERY};
const photo_frame_error ChargingCircuitFault{TXT_CHARGING_CIRCUIT_FAULT, 177, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_BATTERY};
const photo_frame_error BatteryNotDetected{TXT_BATTERY_NOT_DETECTED, 178, ERROR_SEVERITY_CRITICAL, ERROR_CATEGORY_BATTERY};
const photo_frame_error BatteryAuthenticationFailed{TXT_BATTERY_AUTHENTICATION_FAILED, 179, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_BATTERY};

// Power Supply errors (180-189)
const photo_frame_error PowerSupplyInsufficient{TXT_POWER_SUPPLY_INSUFFICIENT, 180, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_HARDWARE};
const photo_frame_error PowerSupplyUnstable{TXT_POWER_SUPPLY_UNSTABLE, 181, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_HARDWARE};
const photo_frame_error PowerSupplyOvervoltage{TXT_POWER_SUPPLY_OVERVOLTAGE, 182, ERROR_SEVERITY_CRITICAL, ERROR_CATEGORY_HARDWARE};
const photo_frame_error PowerSupplyUndervoltage{TXT_POWER_SUPPLY_UNDERVOLTAGE, 183, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_HARDWARE};
const photo_frame_error PowerRegulatorFailed{TXT_POWER_REGULATOR_FAILED, 184, ERROR_SEVERITY_CRITICAL, ERROR_CATEGORY_HARDWARE};
const photo_frame_error PowerSupplyNoise{TXT_POWER_SUPPLY_NOISE, 185, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_HARDWARE};
const photo_frame_error PowerSupplyEfficiencyLow{TXT_POWER_SUPPLY_EFFICIENCY_LOW, 186, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_HARDWARE};
const photo_frame_error PowerSupplyOvercurrent{TXT_POWER_SUPPLY_OVERCURRENT, 187, ERROR_SEVERITY_CRITICAL, ERROR_CATEGORY_HARDWARE};
const photo_frame_error PowerSupplyShortCircuit{TXT_POWER_SUPPLY_SHORT_CIRCUIT, 188, ERROR_SEVERITY_CRITICAL, ERROR_CATEGORY_HARDWARE};
const photo_frame_error PowerSupplyDisconnected{TXT_POWER_SUPPLY_DISCONNECTED, 189, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_HARDWARE};

// Power Management errors (190-199)
const photo_frame_error PowerSavingModeEnterFailed{TXT_POWER_SAVING_MODE_ENTER_FAILED, 190, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_HARDWARE};
const photo_frame_error PowerSavingModeExitFailed{TXT_POWER_SAVING_MODE_EXIT_FAILED, 191, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_HARDWARE};
const photo_frame_error SleepModeActivationFailed{TXT_SLEEP_MODE_ACTIVATION_FAILED, 192, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_HARDWARE};
const photo_frame_error WakeupSourceInvalid{TXT_WAKEUP_SOURCE_INVALID, 193, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_HARDWARE};
const photo_frame_error PowerConsumptionTooHigh{TXT_POWER_CONSUMPTION_TOO_HIGH, 194, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_HARDWARE};
const photo_frame_error PowerManagerInitFailed{TXT_POWER_MANAGER_INIT_FAILED, 195, ERROR_SEVERITY_CRITICAL, ERROR_CATEGORY_HARDWARE};
const photo_frame_error ClockFrequencyError{TXT_CLOCK_FREQUENCY_ERROR, 196, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_HARDWARE};
const photo_frame_error VoltageScalingFailed{TXT_VOLTAGE_SCALING_FAILED, 197, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_HARDWARE};
const photo_frame_error PowerDomainError{TXT_POWER_DOMAIN_ERROR, 198, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_HARDWARE};
const photo_frame_error ThermalThrottlingActive{TXT_THERMAL_THROTTLING_ACTIVE, 199, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_HARDWARE};

// Display Hardware errors (200-219) - Enhanced granular errors
const photo_frame_error DisplayInitializationFailed{TXT_DISPLAY_INITIALIZATION_FAILED, 200, ERROR_SEVERITY_CRITICAL, ERROR_CATEGORY_DISPLAY};
const photo_frame_error DisplayDriverError{TXT_DISPLAY_DRIVER_ERROR, 201, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_DISPLAY};
const photo_frame_error DisplaySpiCommError{TXT_DISPLAY_SPI_COMM_ERROR, 202, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_DISPLAY};
const photo_frame_error DisplayBusyTimeout{TXT_DISPLAY_BUSY_TIMEOUT, 203, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_DISPLAY};
const photo_frame_error DisplayResetFailed{TXT_DISPLAY_RESET_FAILED, 204, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_DISPLAY};
const photo_frame_error DisplayPowerOnFailed{TXT_DISPLAY_POWER_ON_FAILED, 205, ERROR_SEVERITY_CRITICAL, ERROR_CATEGORY_DISPLAY};
const photo_frame_error DisplayPowerOffFailed{TXT_DISPLAY_POWER_OFF_FAILED, 206, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_DISPLAY};
const photo_frame_error DisplayWakeupFailed{TXT_DISPLAY_WAKEUP_FAILED, 207, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_DISPLAY};
const photo_frame_error DisplayCommandError{TXT_DISPLAY_COMMAND_ERROR, 208, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_DISPLAY};
const photo_frame_error DisplayHardwareFault{TXT_DISPLAY_HARDWARE_FAULT, 209, ERROR_SEVERITY_CRITICAL, ERROR_CATEGORY_DISPLAY};

// E-Paper Specific errors (210-229)
const photo_frame_error EpaperRefreshFailed{TXT_EPAPER_REFRESH_FAILED, 210, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_DISPLAY};
const photo_frame_error EpaperPartialRefreshNotSupported{TXT_EPAPER_PARTIAL_REFRESH_NOT_SUPPORTED, 211, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_DISPLAY};
const photo_frame_error EpaperGhostingDetected{TXT_EPAPER_GHOSTING_DETECTED, 212, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_DISPLAY};
const photo_frame_error EpaperTemperatureCompensationFailed{TXT_EPAPER_TEMPERATURE_COMPENSATION_FAILED, 213, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_DISPLAY};
const photo_frame_error EpaperWaveformError{TXT_EPAPER_WAVEFORM_ERROR, 214, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_DISPLAY};
const photo_frame_error EpaperVoltageRegulationError{TXT_EPAPER_VOLTAGE_REGULATION_ERROR, 215, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_DISPLAY};
const photo_frame_error EpaperPixelStuckError{TXT_EPAPER_PIXEL_STUCK_ERROR, 216, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_DISPLAY};
const photo_frame_error EpaperContrastPoor{TXT_EPAPER_CONTRAST_POOR, 217, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_DISPLAY};
const photo_frame_error EpaperRefreshTooFrequent{TXT_EPAPER_REFRESH_TOO_FREQUENT, 218, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_DISPLAY};
const photo_frame_error EpaperLifetimeExceeded{TXT_EPAPER_LIFETIME_EXCEEDED, 219, ERROR_SEVERITY_CRITICAL, ERROR_CATEGORY_DISPLAY};

// Display Rendering errors (230-249)
const photo_frame_error DisplayBufferOverflow{TXT_DISPLAY_BUFFER_OVERFLOW, 230, ERROR_SEVERITY_CRITICAL, ERROR_CATEGORY_DISPLAY};
const photo_frame_error DisplayBufferUnderflow{TXT_DISPLAY_BUFFER_UNDERFLOW, 231, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_DISPLAY};
const photo_frame_error DisplayMemoryAllocationFailed{TXT_DISPLAY_MEMORY_ALLOCATION_FAILED, 232, ERROR_SEVERITY_CRITICAL, ERROR_CATEGORY_DISPLAY};
const photo_frame_error DisplayFramebufferCorrupted{TXT_DISPLAY_FRAMEBUFFER_CORRUPTED, 233, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_DISPLAY};
const photo_frame_error DisplayPixelFormatError{TXT_DISPLAY_PIXEL_FORMAT_ERROR, 234, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_DISPLAY};
const photo_frame_error DisplayColorSpaceError{TXT_DISPLAY_COLOR_SPACE_ERROR, 235, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_DISPLAY};
const photo_frame_error DisplayScalingError{TXT_DISPLAY_SCALING_ERROR, 236, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_DISPLAY};
const photo_frame_error DisplayRotationError{TXT_DISPLAY_ROTATION_ERROR, 237, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_DISPLAY};
const photo_frame_error DisplayClippingError{TXT_DISPLAY_CLIPPING_ERROR, 238, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_DISPLAY};
const photo_frame_error DisplayRenderingTimeout{TXT_DISPLAY_RENDERING_TIMEOUT, 239, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_DISPLAY};

// Display Configuration errors (250-259)
const photo_frame_error DisplayResolutionMismatch{TXT_DISPLAY_RESOLUTION_MISMATCH, 250, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_DISPLAY};
const photo_frame_error DisplayColorDepthUnsupported{TXT_DISPLAY_COLOR_DEPTH_UNSUPPORTED, 251, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_DISPLAY};
const photo_frame_error DisplayOrientationInvalid{TXT_DISPLAY_ORIENTATION_INVALID, 252, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_DISPLAY};
const photo_frame_error DisplayRefreshRateInvalid{TXT_DISPLAY_REFRESH_RATE_INVALID, 253, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_DISPLAY};
const photo_frame_error DisplayGammaConfigError{TXT_DISPLAY_GAMMA_CONFIG_ERROR, 254, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_DISPLAY};
const photo_frame_error DisplayBrightnessControlError{TXT_DISPLAY_BRIGHTNESS_CONTROL_ERROR, 255, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_DISPLAY};
const photo_frame_error DisplayContrastControlError{TXT_DISPLAY_CONTRAST_CONTROL_ERROR, 256, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_DISPLAY};
const photo_frame_error DisplayTimingConfigError{TXT_DISPLAY_TIMING_CONFIG_ERROR, 257, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_DISPLAY};
const photo_frame_error DisplayModeNotSupported{TXT_DISPLAY_MODE_NOT_SUPPORTED, 258, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_DISPLAY};
const photo_frame_error DisplayCalibrationRequired{TXT_DISPLAY_CALIBRATION_REQUIRED, 259, ERROR_SEVERITY_INFO, ERROR_CATEGORY_DISPLAY};

// Add more errors here
// clang-format on
} // namespace error_type

/**
 * @brief Helper functions for error mapping and creation
 */
namespace error_utils {

// Function declarations - implementations are in errors.cpp
photo_frame_error map_http_status_to_error(int statusCode, const char* context = nullptr);
photo_frame_error map_google_drive_error(int statusCode, const char* responseBody = nullptr, const char* context = nullptr);
photo_frame_error create_oauth_error(const char* errorType, const char* context = nullptr);
photo_frame_error create_image_error(const char* errorType, const char* filename = nullptr, 
                                    const char* dimensions = nullptr, const char* context = nullptr);
photo_frame_error validate_image_dimensions(int width, int height, int maxWidth, int maxHeight, 
                                            const char* filename = nullptr);
photo_frame_error validate_image_file_size(size_t fileSize, size_t expectedMinSize, size_t expectedMaxSize,
                                           const char* filename = nullptr);

// Battery/Power error helper functions
photo_frame_error create_battery_error(const char* errorType, float voltage = -1, float percentage = -1, 
                                      float temperature = -999, const char* context = nullptr);
photo_frame_error validate_battery_voltage(float voltage, float minVoltage, float maxVoltage, 
                                           const char* context = nullptr);
photo_frame_error validate_battery_temperature(float temperature, float minTemp, float maxTemp, 
                                               const char* context = nullptr);
photo_frame_error create_charging_error(const char* errorType, float current = -1, float voltage = -1, 
                                        const char* context = nullptr);
photo_frame_error create_power_supply_error(const char* errorType, float voltage = -1, float current = -1, 
                                            const char* context = nullptr);

// Display error helper functions
photo_frame_error create_display_error(const char* errorType, int width = -1, int height = -1, 
                                      const char* displayMode = nullptr, const char* context = nullptr);
photo_frame_error create_epaper_error(const char* errorType, int refreshCount = -1, float temperature = -999, 
                                     const char* waveform = nullptr, const char* context = nullptr);
photo_frame_error validate_display_resolution(int width, int height, int maxWidth, int maxHeight, 
                                              const char* context = nullptr);
photo_frame_error validate_display_refresh_rate(float refreshRate, float minRate, float maxRate, 
                                                const char* context = nullptr);
photo_frame_error create_display_rendering_error(const char* errorType, size_t bufferSize = 0, 
                                                size_t memoryUsed = 0, const char* operation = nullptr, 
                                                 const char* context   = nullptr);

} // namespace error_utils

} // namespace photo_frame

#endif // __PHOTO_FRAME_ERRORS_H__