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

#include <Arduino.h>
#include <cstdint>

// Forward declarations
class String;

namespace photo_frame {

/**
 * @brief Error severity levels for granular error reporting.
 */
enum error_severity {
    ERROR_SEVERITY_INFO = 0,      ///< Informational messages
    ERROR_SEVERITY_WARNING = 1,   ///< Warning conditions
    ERROR_SEVERITY_ERROR = 2,     ///< Error conditions
    ERROR_SEVERITY_CRITICAL = 3   ///< Critical system errors
};

/**
 * @brief Error categories for better error classification.
 */
enum error_category {
    ERROR_CATEGORY_GENERAL = 0,      ///< General errors
    ERROR_CATEGORY_NETWORK = 1,      ///< Network/WiFi related errors
    ERROR_CATEGORY_STORAGE = 2,      ///< SD card/file system errors
    ERROR_CATEGORY_HARDWARE = 3,     ///< Hardware component errors
    ERROR_CATEGORY_CONFIG = 4,       ///< Configuration validation errors
    ERROR_CATEGORY_AUTHENTICATION = 5, ///< Authentication/JWT errors
    ERROR_CATEGORY_BATTERY = 6,      ///< Battery related errors
    ERROR_CATEGORY_DISPLAY = 7       ///< Display/rendering errors
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
    const char* message;     ///< Human-readable error message
    uint16_t code;          ///< Numeric error code for identification
    
    // Enhanced fields for granular reporting
    error_severity severity; ///< Error severity level
    error_category category; ///< Error category for classification
    uint32_t timestamp;     ///< When error occurred (millis())
    const char* context;    ///< Additional context/details
    const char* source_file; ///< Source file where error occurred
    uint16_t source_line;   ///< Source line where error occurred

    // Default constructor - backward compatible
    photo_frame_error() : 
        message("No error"), 
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
    constexpr photo_frame_error(const char* msg, uint16_t err_code, 
                               error_severity sev, error_category cat,
                               const char* ctx = nullptr,
                               const char* file = nullptr,
                               uint16_t line = 0) :
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
    constexpr bool operator!=(const photo_frame_error& other) const { 
        return !(*this == other); 
    }

    /**
     * @brief Set the timestamp to current time.
     */
    void set_timestamp() {
        timestamp = millis();
    }

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
    bool is_critical() const {
        return severity == ERROR_SEVERITY_CRITICAL;
    }

private:
    /**
     * @brief Convert severity enum to string.
     * @return String representation of severity
     */
    const char* severity_to_string() const {
        switch (severity) {
            case ERROR_SEVERITY_INFO: return "INFO";
            case ERROR_SEVERITY_WARNING: return "WARN";
            case ERROR_SEVERITY_ERROR: return "ERROR";
            case ERROR_SEVERITY_CRITICAL: return "CRITICAL";
            default: return "UNKNOWN";
        }
    }

    /**
     * @brief Convert category enum to string.
     * @return String representation of category
     */
    const char* category_to_string() const {
        switch (category) {
            case ERROR_CATEGORY_GENERAL: return "General";
            case ERROR_CATEGORY_NETWORK: return "Network";
            case ERROR_CATEGORY_STORAGE: return "Storage";
            case ERROR_CATEGORY_HARDWARE: return "Hardware";
            case ERROR_CATEGORY_CONFIG: return "Config";
            case ERROR_CATEGORY_AUTHENTICATION: return "Auth";
            case ERROR_CATEGORY_BATTERY: return "Battery";
            case ERROR_CATEGORY_DISPLAY: return "Display";
            default: return "Unknown";
        }
    }
} photo_frame_error_t;

// Helper macros for creating enhanced errors with location info
#define MAKE_ERROR(msg, code, sev, cat) \
  photo_frame_error(msg, (uint16_t)(code), sev, cat, nullptr, __FILE__, __LINE__)

#define MAKE_ERROR_WITH_CONTEXT(msg, code, sev, cat, ctx) \
  photo_frame_error(msg, (uint16_t)(code), sev, cat, ctx, __FILE__, __LINE__)

/**
 * @brief Namespace containing predefined error instances.
 *
 * This namespace provides a collection of commonly used error types
 * throughout the photo frame system. Each error has a predefined
 * message, code, severity, and category for consistent error handling.
 */
namespace error_type {
// No error
const photo_frame_error None{"No error", 0, ERROR_SEVERITY_INFO, ERROR_CATEGORY_GENERAL};

// Storage/SD Card errors (Critical/Error severity)
const photo_frame_error CardMountFailed{"SD card mount failed", 3, ERROR_SEVERITY_CRITICAL, ERROR_CATEGORY_STORAGE};
const photo_frame_error NoSdCardAttached{"No SD card attached", 4, ERROR_SEVERITY_CRITICAL, ERROR_CATEGORY_STORAGE};
const photo_frame_error UnknownSdCardType{"Unknown SD card type", 5, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_STORAGE};
const photo_frame_error CardOpenFileFailed{"Failed to open file on SD card", 6, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_STORAGE};
const photo_frame_error SdCardFileNotFound{"File not found on SD card", 7, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_STORAGE};
const photo_frame_error SdCardFileOpenFailed{"Failed to open SD card file", 8, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_STORAGE};
const photo_frame_error SdCardFileCreateFailed{"Failed to create SD card file", 24, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_STORAGE};
const photo_frame_error SdCardDirCreateFailed{"Failed to create SD card directory", 36, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_STORAGE};
const photo_frame_error CardTocOpenFileFailed{"Failed to open TOC file on SD card", 11, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_STORAGE};
const photo_frame_error FileOpenFailed{"File open failed", 20, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_STORAGE};
const photo_frame_error PreferencesOpenFailed{"Preferences open failed", 12, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_STORAGE};

// SD Card specific errors (100-119) - Enhanced granular errors
const photo_frame_error SdCardWriteProtected{"SD card is write protected", 100, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_STORAGE};
const photo_frame_error SdCardCorrupted{"SD card file system is corrupted", 101, ERROR_SEVERITY_CRITICAL, ERROR_CATEGORY_STORAGE};
const photo_frame_error SdCardInsufficientSpace{"SD card has insufficient free space", 102, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_STORAGE};
const photo_frame_error SdCardSlowResponse{"SD card is responding slowly", 103, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_STORAGE};
const photo_frame_error SdCardReadOnly{"SD card is in read-only mode", 104, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_STORAGE};
const photo_frame_error SdCardBadSector{"SD card has bad sectors", 105, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_STORAGE};
const photo_frame_error SdCardSizeInvalid{"SD card reports invalid size", 106, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_STORAGE};
const photo_frame_error SdCardInitFailed{"SD card initialization failed", 107, ERROR_SEVERITY_CRITICAL, ERROR_CATEGORY_STORAGE};
const photo_frame_error SdCardVersionUnsupported{"SD card version not supported", 108, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_STORAGE};
const photo_frame_error SdCardFileSystemUnsupported{"SD card file system not supported", 109, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_STORAGE};

// Display/Image errors
const photo_frame_error ImageFormatNotSupported{"Image format not supported", 9, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_DISPLAY};
const photo_frame_error NoImagesFound{"No images found", 13, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_DISPLAY};

// Battery errors (Critical severity)
const photo_frame_error BatteryLevelCritical{"Battery level is critical", 10, ERROR_SEVERITY_CRITICAL, ERROR_CATEGORY_BATTERY};
const photo_frame_error BatteryEmpty{"Battery is empty", 14, ERROR_SEVERITY_CRITICAL, ERROR_CATEGORY_BATTERY};

// Hardware errors
const photo_frame_error RTCInitializationFailed{"RTC module not found", 15, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_HARDWARE};

// Authentication errors
const photo_frame_error JwtCreationFailed{"JWT creation failed", 16, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_AUTHENTICATION};
const photo_frame_error TokenMissing{"Token missing", 19, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_AUTHENTICATION};

// Network errors
const photo_frame_error HttpPostFailed{"HTTP POST request failed", 17, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_NETWORK};
const photo_frame_error HttpConnectFailed{"HTTP connection failed", 21, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_NETWORK};
const photo_frame_error HttpGetFailed{"HTTP GET request failed", 22, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_NETWORK};
const photo_frame_error DownloadFailed{"File download failed", 23, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_NETWORK};
const photo_frame_error SslCertificateLoadFailed{"SSL certificate load failed", 25, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_NETWORK};
const photo_frame_error RateLimitTimeoutExceeded{"Rate limit timeout exceeded", 26, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_NETWORK};
const photo_frame_error WifiCredentialsNotFound{"WiFi credentials not found", 27, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_NETWORK};
const photo_frame_error WifiConnectionFailed{"WiFi connection failed", 28, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_NETWORK};

// Network/WiFi specific errors (120-139) - Enhanced granular errors
const photo_frame_error WifiSignalTooWeak{"WiFi signal is too weak", 120, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_NETWORK};
const photo_frame_error WifiAuthenticationFailed{"WiFi authentication failed", 121, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_NETWORK};
const photo_frame_error WifiDhcpFailed{"WiFi DHCP assignment failed", 122, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_NETWORK};
const photo_frame_error WifiDnsResolutionFailed{"WiFi DNS resolution failed", 123, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_NETWORK};
const photo_frame_error WifiNetworkNotFound{"WiFi network not found", 124, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_NETWORK};
const photo_frame_error WifiPasswordIncorrect{"WiFi password is incorrect", 125, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_NETWORK};
const photo_frame_error WifiChannelBusy{"WiFi channel is busy or congested", 126, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_NETWORK};
const photo_frame_error WifiFrequencyNotSupported{"WiFi frequency not supported", 127, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_NETWORK};
const photo_frame_error HttpRequestTimeout{"HTTP request timed out", 128, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_NETWORK};
const photo_frame_error HttpInvalidResponse{"HTTP response is invalid or malformed", 129, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_NETWORK};
const photo_frame_error SslHandshakeFailed{"SSL/TLS handshake failed", 130, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_NETWORK};
const photo_frame_error NetworkInterfaceDown{"Network interface is down", 131, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_NETWORK};
const photo_frame_error NetworkConfigInvalid{"Network configuration is invalid", 132, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_NETWORK};
const photo_frame_error ProxyConnectionFailed{"Proxy connection failed", 133, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_NETWORK};
const photo_frame_error NetworkTimeoutExceeded{"Network operation timeout exceeded", 134, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_NETWORK};

// General errors
const photo_frame_error JsonParseFailed{"JSON parsing failed", 18, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_GENERAL};

// Configuration validation errors
const photo_frame_error ConfigMissingSection{"Configuration section missing", 29, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_CONFIG};
const photo_frame_error ConfigMissingField{"Configuration field missing", 30, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_CONFIG};
const photo_frame_error ConfigInvalidEmail{"Invalid email address in config", 31, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_CONFIG};
const photo_frame_error ConfigInvalidPemKey{"Invalid PEM key in config", 32, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_CONFIG};
const photo_frame_error ConfigInvalidPath{"Invalid path in config", 33, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_CONFIG};
const photo_frame_error ConfigInvalidFilename{"Invalid filename in config", 34, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_CONFIG};
const photo_frame_error ConfigValueOutOfRange{"Configuration value out of range", 35, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_CONFIG};

// Configuration specific errors (140-159) - Enhanced granular errors
const photo_frame_error ConfigFileCorrupted{"Configuration file is corrupted", 140, ERROR_SEVERITY_CRITICAL, ERROR_CATEGORY_CONFIG};
const photo_frame_error ConfigJsonSyntaxError{"Configuration JSON syntax error", 141, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_CONFIG};
const photo_frame_error ConfigVersionMismatch{"Configuration version mismatch", 142, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_CONFIG};
const photo_frame_error ConfigFieldTypeMismatch{"Configuration field type mismatch", 143, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_CONFIG};
const photo_frame_error ConfigEncryptionKeyInvalid{"Configuration encryption key invalid", 144, ERROR_SEVERITY_CRITICAL, ERROR_CATEGORY_CONFIG};
const photo_frame_error ConfigBackupFailed{"Configuration backup failed", 145, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_CONFIG};
const photo_frame_error ConfigRestoreFailed{"Configuration restore failed", 146, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_CONFIG};
const photo_frame_error ConfigValidationFailed{"Configuration validation failed", 147, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_CONFIG};
const photo_frame_error ConfigDefaultsMissing{"Configuration defaults missing", 148, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_CONFIG};
const photo_frame_error ConfigSchemaInvalid{"Configuration schema is invalid", 149, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_CONFIG};
const photo_frame_error ConfigAccessDenied{"Configuration file access denied", 150, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_CONFIG};
const photo_frame_error ConfigFormatUnsupported{"Configuration format not supported", 151, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_CONFIG};
const photo_frame_error ConfigSizeLimitExceeded{"Configuration size limit exceeded", 152, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_CONFIG};
const photo_frame_error ConfigDependencyMissing{"Configuration dependency missing", 153, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_CONFIG};
const photo_frame_error ConfigEnvironmentMismatch{"Configuration environment mismatch", 154, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_CONFIG};

// OAuth/Authentication specific errors (40-49)
const photo_frame_error OAuthTokenExpired{"OAuth access token has expired", 40, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_AUTHENTICATION};
const photo_frame_error OAuthTokenInvalid{"OAuth access token is invalid or malformed", 41, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_AUTHENTICATION};
const photo_frame_error OAuthRefreshTokenMissing{"OAuth refresh token is missing", 42, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_AUTHENTICATION};
const photo_frame_error OAuthRefreshTokenInvalid{"OAuth refresh token is invalid", 43, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_AUTHENTICATION};
const photo_frame_error OAuthScopeInsufficient{"OAuth token has insufficient permissions", 44, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_AUTHENTICATION};
const photo_frame_error OAuthJwtSigningFailed{"Failed to sign JWT for service account", 45, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_AUTHENTICATION};
const photo_frame_error OAuthServiceAccountKeyInvalid{"Service account private key is invalid", 46, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_AUTHENTICATION};
const photo_frame_error OAuthTokenRequestFailed{"Failed to request OAuth access token", 47, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_AUTHENTICATION};
const photo_frame_error OAuthTokenRefreshFailed{"Failed to refresh OAuth access token", 48, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_AUTHENTICATION};

// Google Drive API specific errors (50-69)
const photo_frame_error GoogleDriveApiQuotaExceeded{"Google Drive API quota exceeded", 50, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_NETWORK};
const photo_frame_error GoogleDriveApiRateLimited{"Google Drive API rate limit exceeded", 51, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_NETWORK};
const photo_frame_error GoogleDriveFileNotFound{"File not found in Google Drive", 52, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_NETWORK};
const photo_frame_error GoogleDriveFolderNotFound{"Folder not found in Google Drive", 53, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_NETWORK};
const photo_frame_error GoogleDrivePermissionDenied{"Permission denied to access Google Drive resource", 54, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_NETWORK};
const photo_frame_error GoogleDriveStorageQuotaExceeded{"Google Drive storage quota exceeded", 55, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_NETWORK};
const photo_frame_error GoogleDriveApiDisabled{"Google Drive API is disabled for this project", 56, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_NETWORK};
const photo_frame_error GoogleDriveFileTooBig{"File is too large to download", 57, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_NETWORK};
const photo_frame_error GoogleDriveFileCorrupted{"Downloaded file appears to be corrupted", 58, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_NETWORK};
const photo_frame_error GoogleDriveInvalidQuery{"Invalid query syntax for Google Drive search", 59, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_NETWORK};
const photo_frame_error GoogleDriveApiInternalError{"Google Drive API internal server error", 60, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_NETWORK};
const photo_frame_error GoogleDriveNetworkTimeout{"Network timeout while contacting Google Drive", 61, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_NETWORK};
const photo_frame_error GoogleDriveResponseMalformed{"Malformed response from Google Drive API", 62, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_NETWORK};
const photo_frame_error GoogleDriveApiUnavailable{"Google Drive API is temporarily unavailable", 63, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_NETWORK};
const photo_frame_error GoogleDriveDownloadInterrupted{"File download was interrupted", 64, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_NETWORK};
const photo_frame_error GoogleDriveMetadataInvalid{"Invalid or missing file metadata", 65, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_NETWORK};
const photo_frame_error GoogleDriveFolderEmpty{"Google Drive folder contains no files", 66, ERROR_SEVERITY_INFO, ERROR_CATEGORY_NETWORK};
const photo_frame_error GoogleDriveApiVersionUnsupported{"Unsupported Google Drive API version", 67, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_NETWORK};

// HTTP specific errors (70-79)
const photo_frame_error HttpUnauthorized{"HTTP 401: Unauthorized access", 70, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_NETWORK};
const photo_frame_error HttpForbidden{"HTTP 403: Forbidden access", 71, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_NETWORK};
const photo_frame_error HttpNotFound{"HTTP 404: Resource not found", 72, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_NETWORK};
const photo_frame_error HttpTooManyRequests{"HTTP 429: Too many requests", 73, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_NETWORK};
const photo_frame_error HttpInternalServerError{"HTTP 500: Internal server error", 74, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_NETWORK};
const photo_frame_error HttpBadGateway{"HTTP 502: Bad gateway", 75, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_NETWORK};
const photo_frame_error HttpServiceUnavailable{"HTTP 503: Service unavailable", 76, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_NETWORK};
const photo_frame_error HttpGatewayTimeout{"HTTP 504: Gateway timeout", 77, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_NETWORK};
const photo_frame_error HttpBadRequest{"HTTP 400: Bad request", 78, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_NETWORK};

// Image Processing specific errors (80-99)
const photo_frame_error ImageFileCorrupted{"Image file appears to be corrupted or invalid", 80, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_DISPLAY};
const photo_frame_error ImageFileTooLarge{"Image file is too large for processing", 81, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_DISPLAY};
const photo_frame_error ImageDimensionsInvalid{"Image dimensions are invalid or unsupported", 82, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_DISPLAY};
const photo_frame_error ImageDimensionsMismatch{"Image dimensions don't match expected size", 83, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_DISPLAY};
const photo_frame_error ImageColorDepthUnsupported{"Image color depth is not supported", 84, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_DISPLAY};
const photo_frame_error ImagePixelDataCorrupted{"Pixel data is corrupted or unreadable", 85, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_DISPLAY};
const photo_frame_error ImageFileSeekFailed{"Failed to seek to position in image file", 86, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_DISPLAY};
const photo_frame_error ImageFileReadFailed{"Failed to read data from image file", 87, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_DISPLAY};
const photo_frame_error ImageBufferOverflow{"Image processing buffer overflow", 88, ERROR_SEVERITY_CRITICAL, ERROR_CATEGORY_DISPLAY};
const photo_frame_error ImageBufferUnderflow{"Image processing buffer underflow", 89, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_DISPLAY};
const photo_frame_error ImageRenderTimeout{"Image rendering timed out", 90, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_DISPLAY};
const photo_frame_error ImageDisplayWriteFailed{"Failed to write pixel data to display", 91, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_DISPLAY};
const photo_frame_error ImageFileHeaderInvalid{"Image file header is invalid or missing", 92, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_DISPLAY};
const photo_frame_error ImageFileEmpty{"Image file is empty or has no content", 93, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_DISPLAY};
const photo_frame_error ImageFileTruncated{"Image file appears to be truncated", 94, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_DISPLAY};
const photo_frame_error ImageMemoryAllocationFailed{"Failed to allocate memory for image processing", 95, ERROR_SEVERITY_CRITICAL, ERROR_CATEGORY_DISPLAY};
const photo_frame_error ImagePaletteInvalid{"Image palette data is invalid", 96, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_DISPLAY};
const photo_frame_error ImageConversionFailed{"Failed to convert image data to display format", 97, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_DISPLAY};
const photo_frame_error ImageResolutionTooHigh{"Image resolution exceeds display capabilities", 98, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_DISPLAY};
const photo_frame_error ImageProcessingAborted{"Image processing was aborted due to error", 99, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_DISPLAY};

// Battery/Power specific errors (160-179) - Enhanced granular errors
const photo_frame_error BatteryVoltageLow{"Battery voltage is below safe threshold", 160, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_BATTERY};
const photo_frame_error BatteryVoltageUnstable{"Battery voltage is unstable or fluctuating", 161, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_BATTERY};
const photo_frame_error BatteryTemperatureHigh{"Battery temperature is too high", 162, ERROR_SEVERITY_CRITICAL, ERROR_CATEGORY_BATTERY};
const photo_frame_error BatteryTemperatureLow{"Battery temperature is too low", 163, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_BATTERY};
const photo_frame_error BatteryAgingDetected{"Battery shows signs of aging or degradation", 164, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_BATTERY};
const photo_frame_error BatteryCalibrationNeeded{"Battery gauge needs recalibration", 165, ERROR_SEVERITY_INFO, ERROR_CATEGORY_BATTERY};
const photo_frame_error BatteryDischargeRateTooHigh{"Battery discharge rate is abnormally high", 166, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_BATTERY};
const photo_frame_error BatteryChargeRateTooSlow{"Battery charging rate is too slow", 167, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_BATTERY};
const photo_frame_error BatteryCapacityReduced{"Battery capacity has significantly reduced", 168, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_BATTERY};
const photo_frame_error BatteryHealthPoor{"Battery health is poor", 169, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_BATTERY};

// Charging specific errors (170-179)
const photo_frame_error ChargingFailed{"Battery charging has failed", 170, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_BATTERY};
const photo_frame_error ChargerNotConnected{"Charger is not connected", 171, ERROR_SEVERITY_INFO, ERROR_CATEGORY_BATTERY};
const photo_frame_error ChargerIncompatible{"Charger is incompatible or insufficient", 172, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_BATTERY};
const photo_frame_error ChargingOverheat{"Charging stopped due to overheating", 173, ERROR_SEVERITY_CRITICAL, ERROR_CATEGORY_BATTERY};
const photo_frame_error ChargingTimeout{"Charging process timed out", 174, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_BATTERY};
const photo_frame_error ChargeCurrentTooHigh{"Charge current is dangerously high", 175, ERROR_SEVERITY_CRITICAL, ERROR_CATEGORY_BATTERY};
const photo_frame_error ChargeCurrentTooLow{"Charge current is too low for efficient charging", 176, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_BATTERY};
const photo_frame_error ChargingCircuitFault{"Charging circuit has a fault", 177, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_BATTERY};
const photo_frame_error BatteryNotDetected{"Battery is not detected by the system", 178, ERROR_SEVERITY_CRITICAL, ERROR_CATEGORY_BATTERY};
const photo_frame_error BatteryAuthenticationFailed{"Battery authentication failed", 179, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_BATTERY};

// Power Supply errors (180-189)
const photo_frame_error PowerSupplyInsufficient{"Power supply is insufficient for operation", 180, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_HARDWARE};
const photo_frame_error PowerSupplyUnstable{"Power supply voltage is unstable", 181, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_HARDWARE};
const photo_frame_error PowerSupplyOvervoltage{"Power supply voltage is too high", 182, ERROR_SEVERITY_CRITICAL, ERROR_CATEGORY_HARDWARE};
const photo_frame_error PowerSupplyUndervoltage{"Power supply voltage is too low", 183, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_HARDWARE};
const photo_frame_error PowerRegulatorFailed{"Power regulator has failed", 184, ERROR_SEVERITY_CRITICAL, ERROR_CATEGORY_HARDWARE};
const photo_frame_error PowerSupplyNoise{"Power supply has excessive noise or interference", 185, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_HARDWARE};
const photo_frame_error PowerSupplyEfficiencyLow{"Power supply efficiency is below threshold", 186, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_HARDWARE};
const photo_frame_error PowerSupplyOvercurrent{"Power supply overcurrent protection triggered", 187, ERROR_SEVERITY_CRITICAL, ERROR_CATEGORY_HARDWARE};
const photo_frame_error PowerSupplyShortCircuit{"Power supply short circuit detected", 188, ERROR_SEVERITY_CRITICAL, ERROR_CATEGORY_HARDWARE};
const photo_frame_error PowerSupplyDisconnected{"Power supply has been disconnected", 189, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_HARDWARE};

// Power Management errors (190-199)
const photo_frame_error PowerSavingModeEnterFailed{"Failed to enter power saving mode", 190, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_HARDWARE};
const photo_frame_error PowerSavingModeExitFailed{"Failed to exit power saving mode", 191, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_HARDWARE};
const photo_frame_error SleepModeActivationFailed{"Failed to activate sleep mode", 192, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_HARDWARE};
const photo_frame_error WakeupSourceInvalid{"Wakeup source configuration is invalid", 193, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_HARDWARE};
const photo_frame_error PowerConsumptionTooHigh{"Power consumption exceeds expected limits", 194, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_HARDWARE};
const photo_frame_error PowerManagerInitFailed{"Power management system initialization failed", 195, ERROR_SEVERITY_CRITICAL, ERROR_CATEGORY_HARDWARE};
const photo_frame_error ClockFrequencyError{"CPU clock frequency adjustment failed", 196, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_HARDWARE};
const photo_frame_error VoltageScalingFailed{"Dynamic voltage scaling failed", 197, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_HARDWARE};
const photo_frame_error PowerDomainError{"Power domain control error", 198, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_HARDWARE};
const photo_frame_error ThermalThrottlingActive{"Thermal throttling is active due to overheating", 199, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_HARDWARE};

// Display Hardware errors (200-219) - Enhanced granular errors
const photo_frame_error DisplayInitializationFailed{"Display initialization failed", 200, ERROR_SEVERITY_CRITICAL, ERROR_CATEGORY_DISPLAY};
const photo_frame_error DisplayDriverError{"Display driver communication error", 201, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_DISPLAY};
const photo_frame_error DisplaySpiCommError{"Display SPI communication error", 202, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_DISPLAY};
const photo_frame_error DisplayBusyTimeout{"Display busy signal timeout", 203, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_DISPLAY};
const photo_frame_error DisplayResetFailed{"Display reset sequence failed", 204, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_DISPLAY};
const photo_frame_error DisplayPowerOnFailed{"Display power-on sequence failed", 205, ERROR_SEVERITY_CRITICAL, ERROR_CATEGORY_DISPLAY};
const photo_frame_error DisplayPowerOffFailed{"Display power-off sequence failed", 206, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_DISPLAY};
const photo_frame_error DisplayWakeupFailed{"Display wakeup from sleep failed", 207, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_DISPLAY};
const photo_frame_error DisplayCommandError{"Display command execution error", 208, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_DISPLAY};
const photo_frame_error DisplayHardwareFault{"Display hardware fault detected", 209, ERROR_SEVERITY_CRITICAL, ERROR_CATEGORY_DISPLAY};

// E-Paper Specific errors (210-229)
const photo_frame_error EpaperRefreshFailed{"E-paper display refresh failed", 210, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_DISPLAY};
const photo_frame_error EpaperPartialRefreshNotSupported{"E-paper partial refresh not supported", 211, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_DISPLAY};
const photo_frame_error EpaperGhostingDetected{"E-paper ghosting artifacts detected", 212, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_DISPLAY};
const photo_frame_error EpaperTemperatureCompensationFailed{"E-paper temperature compensation failed", 213, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_DISPLAY};
const photo_frame_error EpaperWaveformError{"E-paper waveform selection error", 214, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_DISPLAY};
const photo_frame_error EpaperVoltageRegulationError{"E-paper voltage regulation error", 215, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_DISPLAY};
const photo_frame_error EpaperPixelStuckError{"E-paper pixels permanently stuck", 216, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_DISPLAY};
const photo_frame_error EpaperContrastPoor{"E-paper contrast is poor or faded", 217, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_DISPLAY};
const photo_frame_error EpaperRefreshTooFrequent{"E-paper refresh rate too frequent", 218, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_DISPLAY};
const photo_frame_error EpaperLifetimeExceeded{"E-paper lifetime refresh count exceeded", 219, ERROR_SEVERITY_CRITICAL, ERROR_CATEGORY_DISPLAY};

// Display Rendering errors (230-249)
const photo_frame_error DisplayBufferOverflow{"Display rendering buffer overflow", 230, ERROR_SEVERITY_CRITICAL, ERROR_CATEGORY_DISPLAY};
const photo_frame_error DisplayBufferUnderflow{"Display rendering buffer underflow", 231, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_DISPLAY};
const photo_frame_error DisplayMemoryAllocationFailed{"Display memory allocation failed", 232, ERROR_SEVERITY_CRITICAL, ERROR_CATEGORY_DISPLAY};
const photo_frame_error DisplayFramebufferCorrupted{"Display framebuffer is corrupted", 233, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_DISPLAY};
const photo_frame_error DisplayPixelFormatError{"Display pixel format conversion error", 234, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_DISPLAY};
const photo_frame_error DisplayColorSpaceError{"Display color space conversion error", 235, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_DISPLAY};
const photo_frame_error DisplayScalingError{"Display image scaling error", 236, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_DISPLAY};
const photo_frame_error DisplayRotationError{"Display rotation operation error", 237, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_DISPLAY};
const photo_frame_error DisplayClippingError{"Display content clipping error", 238, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_DISPLAY};
const photo_frame_error DisplayRenderingTimeout{"Display rendering operation timed out", 239, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_DISPLAY};

// Display Configuration errors (250-259)
const photo_frame_error DisplayResolutionMismatch{"Display resolution configuration mismatch", 250, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_DISPLAY};
const photo_frame_error DisplayColorDepthUnsupported{"Display color depth not supported", 251, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_DISPLAY};
const photo_frame_error DisplayOrientationInvalid{"Display orientation setting is invalid", 252, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_DISPLAY};
const photo_frame_error DisplayRefreshRateInvalid{"Display refresh rate setting is invalid", 253, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_DISPLAY};
const photo_frame_error DisplayGammaConfigError{"Display gamma correction configuration error", 254, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_DISPLAY};
const photo_frame_error DisplayBrightnessControlError{"Display brightness control error", 255, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_DISPLAY};
const photo_frame_error DisplayContrastControlError{"Display contrast control error", 256, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_DISPLAY};
const photo_frame_error DisplayTimingConfigError{"Display timing configuration error", 257, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_DISPLAY};
const photo_frame_error DisplayModeNotSupported{"Display mode not supported by hardware", 258, ERROR_SEVERITY_ERROR, ERROR_CATEGORY_DISPLAY};
const photo_frame_error DisplayCalibrationRequired{"Display calibration is required", 259, ERROR_SEVERITY_INFO, ERROR_CATEGORY_DISPLAY};

// Add more errors here
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
                                                const char* context = nullptr);

} // namespace error_utils

} // namespace photo_frame

#endif // __PHOTO_FRAME_ERRORS_H__