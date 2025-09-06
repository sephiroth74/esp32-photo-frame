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

namespace photo_frame {

/**
 * @brief Error class for photo frame operations.
 *
 * This class encapsulates error information including a human-readable message
 * and error code. It provides a standardized way to handle and communicate 
 * errors throughout the photo frame system.
 */
typedef class photo_frame_error {
  public:
    const char* message; ///< Human-readable error message
    uint8_t code;        ///< Numeric error code for identification

    photo_frame_error() : message(TXT_NO_ERROR), code(0) {}

    /**
     * @brief Constructor with message and code.
     * @param msg Human-readable error message
     * @param err_code Numeric error code
     */
    constexpr photo_frame_error(const char* msg, uint8_t err_code) :
        message(msg),
        code(err_code) {}

    /**
     * @brief Equality operator comparing error codes.
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
} photo_frame_error_t;

/**
 * @brief Namespace containing predefined error instances.
 *
 * This namespace provides a collection of commonly used error types
 * throughout the photo frame system. Each error has a predefined
 * message and code for consistent error handling.
 */
namespace error_type {
const photo_frame_error None{TXT_NO_ERROR, 0}; ///< No error occurred
const photo_frame_error CardMountFailed{TXT_CARD_MOUNT_FAILED, 3};
const photo_frame_error NoSdCardAttached{TXT_NO_SD_CARD_ATTACHED, 4};
const photo_frame_error UnknownSdCardType{TXT_UNKNOWN_SD_CARD_TYPE, 5};
const photo_frame_error CardOpenFileFailed{TXT_CARD_OPEN_FILE_FAILED, 6};
const photo_frame_error SdCardFileNotFound{TXT_SD_CARD_FILE_NOT_FOUND, 7};
const photo_frame_error SdCardFileOpenFailed{TXT_SD_CARD_FILE_OPEN_FAILED, 8};
const photo_frame_error SdCardFileCreateFailed{TXT_SD_CARD_FILE_CREATE_FAILED, 24};
const photo_frame_error SdCardDirCreateFailed{TXT_SD_CARD_DIR_CREATE_FAILED, 36};
const photo_frame_error ImageFormatNotSupported{TXT_IMAGE_FORMAT_NOT_SUPPORTED, 9};
const photo_frame_error BatteryLevelCritical{TXT_BATTERY_LEVEL_CRITICAL, 10};
const photo_frame_error CardTocOpenFileFailed{TXT_CARD_TOC_OPEN_FILE_FAILED, 11};
const photo_frame_error PreferencesOpenFailed{TXT_PREFERENCES_OPEN_FAILED, 12};
const photo_frame_error NoImagesFound{TXT_NO_IMAGES_FOUND, 13};
const photo_frame_error BatteryEmpty{TXT_BATTERY_EMPTY, 14};
const photo_frame_error RTCInitializationFailed{TXT_RTC_MODULE_NOT_FOUND, 15};
const photo_frame_error JwtCreationFailed{TXT_JWT_CREATION_FAILED, 16};
const photo_frame_error HttpPostFailed{TXT_HTTP_POST_FAILED, 17};
const photo_frame_error JsonParseFailed{TXT_JSON_PARSE_FAILED, 18};
const photo_frame_error TokenMissing{TXT_TOKEN_MISSING, 19};
const photo_frame_error FileOpenFailed{TXT_FILE_OPEN_FAILED, 20};
const photo_frame_error HttpConnectFailed{TXT_HTTP_CONNECT_FAILED, 21};
const photo_frame_error HttpGetFailed{TXT_HTTP_GET_FAILED, 22};
const photo_frame_error DownloadFailed{TXT_DOWNLOAD_FAILED, 23};
const photo_frame_error SslCertificateLoadFailed{TXT_SSL_CERTIFICATE_LOAD_FAILED, 25};
const photo_frame_error RateLimitTimeoutExceeded{TXT_RATE_LIMIT_TIMEOUT_EXCEEDED, 26};
const photo_frame_error WifiCredentialsNotFound{TXT_WIFI_CREDENTIALS_NOT_FOUND, 27};
const photo_frame_error WifiConnectionFailed{TXT_WIFI_CONNECTION_FAILED, 28};

// Google Drive configuration validation errors
const photo_frame_error ConfigMissingSection{TXT_CONFIG_MISSING_SECTION, 29};
const photo_frame_error ConfigMissingField{TXT_CONFIG_MISSING_FIELD, 30};
const photo_frame_error ConfigInvalidEmail{TXT_CONFIG_INVALID_EMAIL, 31};
const photo_frame_error ConfigInvalidPemKey{TXT_CONFIG_INVALID_PEM_KEY, 32};
const photo_frame_error ConfigInvalidPath{TXT_CONFIG_INVALID_PATH, 33};
const photo_frame_error ConfigInvalidFilename{TXT_CONFIG_INVALID_FILENAME, 34};
const photo_frame_error ConfigValueOutOfRange{TXT_CONFIG_VALUE_OUT_OF_RANGE, 35};

// Add more errors here
} // namespace error_type

} // namespace photo_frame

#endif // __PHOTO_FRAME_ERRORS_H__