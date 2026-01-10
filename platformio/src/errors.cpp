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

#include "errors.h"
#include <Arduino.h>

namespace photo_frame {
namespace error_utils {

// Context removed entirely to save maximum DRAM

photo_frame_error map_http_status_to_error(int statusCode, const char* context) {
    photo_frame_error error;

    switch (statusCode) {
    case 400: error = error_type::HttpBadRequest; break;
    case 401: error = error_type::HttpUnauthorized; break;
    case 403: error = error_type::HttpForbidden; break;
    case 404: error = error_type::HttpNotFound; break;
    case 429: error = error_type::HttpTooManyRequests; break;
    case 500: error = error_type::HttpInternalServerError; break;
    case 502: error = error_type::HttpBadGateway; break;
    case 503: error = error_type::HttpServiceUnavailable; break;
    case 504: error = error_type::HttpGatewayTimeout; break;
    default:  error = error_type::HttpGetFailed; break;
    }

    // Context removed to save DRAM

    error.set_timestamp();
    return error;
}

photo_frame_error
map_google_drive_error(int statusCode, const char* responseBody, const char* context) {
    photo_frame_error error;

    // First check for specific Google Drive error patterns
    if (responseBody) {
        // Check for specific error messages in response body
        String body = String(responseBody);

        if (body.indexOf("quotaExceeded") >= 0 || body.indexOf("rateLimitExceeded") >= 0) {
            error = (statusCode == 429) ? error_type::GoogleDriveApiRateLimited
                                        : error_type::GoogleDriveApiQuotaExceeded;
        } else if (body.indexOf("notFound") >= 0) {
            error = error_type::GoogleDriveFileNotFound;
        } else if (body.indexOf("insufficientPermissions") >= 0 || body.indexOf("forbidden") >= 0) {
            error = error_type::GoogleDrivePermissionDenied;
        } else if (body.indexOf("storageQuotaExceeded") >= 0) {
            error = error_type::GoogleDriveStorageQuotaExceeded;
        } else if (body.indexOf("accessNotConfigured") >= 0 ||
                   body.indexOf("apiNotActivated") >= 0) {
            error = error_type::GoogleDriveApiDisabled;
        } else {
            // Fall back to HTTP status code mapping
            error = map_http_status_to_error(statusCode, "Google Drive API");
        }
    } else {
        // No response body, use status code mapping
        switch (statusCode) {
        case 401: error = error_type::OAuthTokenExpired; break;
        case 403: error = error_type::GoogleDrivePermissionDenied; break;
        case 404: error = error_type::GoogleDriveFileNotFound; break;
        case 429: error = error_type::GoogleDriveApiRateLimited; break;
        case 500:
        case 502:
        case 503: error = error_type::GoogleDriveApiInternalError; break;
        case 504: error = error_type::GoogleDriveNetworkTimeout; break;
        default:  error = map_http_status_to_error(statusCode, "Google Drive API"); break;
        }
    }

    // Context removed to save DRAM

    error.set_timestamp();
    return error;
}

photo_frame_error create_oauth_error(const char* errorType, const char* context) {
    photo_frame_error error;

    if (strcmp(errorType, "invalid_token") == 0) {
        error = error_type::OAuthTokenInvalid;
    } else if (strcmp(errorType, "expired_token") == 0) {
        error = error_type::OAuthTokenExpired;
    } else if (strcmp(errorType, "insufficient_scope") == 0) {
        error = error_type::OAuthScopeInsufficient;
    } else if (strcmp(errorType, "invalid_grant") == 0) {
        error = error_type::OAuthRefreshTokenInvalid;
    } else {
        error = error_type::OAuthTokenRequestFailed;
    }

    // Context removed to save DRAM

    error.set_timestamp();
    return error;
}

photo_frame_error create_image_error(const char* errorType,
                                     const char* filename,
                                     const char* dimensions,
                                     const char* context) {
    photo_frame_error error;

    if (strcmp(errorType, "corrupted") == 0) {
        error = error_type::ImageFileCorrupted;
    } else if (strcmp(errorType, "too_large") == 0) {
        error = error_type::ImageFileTooLarge;
    } else if (strcmp(errorType, "invalid_dimensions") == 0) {
        error = error_type::ImageDimensionsInvalid;
    } else if (strcmp(errorType, "dimension_mismatch") == 0) {
        error = error_type::ImageDimensionsMismatch;
    } else if (strcmp(errorType, "unsupported_format") == 0) {
        error = error_type::ImageColorDepthUnsupported;
    } else if (strcmp(errorType, "pixel_corrupted") == 0) {
        error = error_type::ImagePixelDataCorrupted;
    } else if (strcmp(errorType, "seek_failed") == 0) {
        error = error_type::ImageFileSeekFailed;
    } else if (strcmp(errorType, "read_failed") == 0) {
        error = error_type::ImageFileReadFailed;
    } else if (strcmp(errorType, "buffer_overflow") == 0) {
        error = error_type::ImageBufferOverflow;
    } else if (strcmp(errorType, "display_write_failed") == 0) {
        error = error_type::ImageDisplayWriteFailed;
    } else if (strcmp(errorType, "invalid_header") == 0) {
        error = error_type::ImageFileHeaderInvalid;
    } else if (strcmp(errorType, "empty_file") == 0) {
        error = error_type::ImageFileEmpty;
    } else if (strcmp(errorType, "truncated") == 0) {
        error = error_type::ImageFileTruncated;
    } else if (strcmp(errorType, "memory_allocation") == 0) {
        error = error_type::ImageMemoryAllocationFailed;
    } else if (strcmp(errorType, "conversion_failed") == 0) {
        error = error_type::ImageConversionFailed;
    } else if (strcmp(errorType, "dimensions_not_provided") == 0) {
        error = error_type::ImageDimensionsNotProvided;
    } else {
        error = error_type::ImageProcessingAborted;
    }

    // Context removed to save DRAM - error message provides sufficient information

    error.set_timestamp();
    return error;
}

photo_frame_error validate_image_dimensions(int width,
                                            int height,
                                            int maxWidth,
                                            int maxHeight,
                                            const char* filename) {
    char dimensions[64];
    snprintf(dimensions, sizeof(dimensions), "%dx%d", width, height);

    log_v("validateImageDimensions: width=%d, height=%d, maxWidth=%d, maxHeight=%d",
          width,
          height,
          maxWidth,
          maxHeight);

    if (width <= 0 || height <= 0) {
        return create_image_error(
            "invalid_dimensions", filename, dimensions, "Width or height is zero or negative");
    }

    if (width > maxWidth || height > maxHeight) {
        char maxDimensions[64];
        snprintf(maxDimensions, sizeof(maxDimensions), "Max: %dx%d", maxWidth, maxHeight);
        return create_image_error("resolution_too_high", filename, dimensions, maxDimensions);
    }

    return error_type::None;
}

photo_frame_error validate_image_file_size(size_t fileSize,
                                           size_t expectedMinSize,
                                           size_t expectedMaxSize,
                                           const char* filename) {
    if (fileSize == 0) {
        return create_image_error("empty_file", filename, nullptr, "File has zero bytes");
    }

    if (fileSize < expectedMinSize) {
        char sizeInfo[128];
        snprintf(sizeInfo,
                 sizeof(sizeInfo),
                 "Size: %zu bytes, Min expected: %zu bytes",
                 fileSize,
                 expectedMinSize);
        return create_image_error("truncated", filename, nullptr, sizeInfo);
    }

    if (fileSize > expectedMaxSize) {
        char sizeInfo[128];
        snprintf(sizeInfo,
                 sizeof(sizeInfo),
                 "Size: %zu bytes, Max allowed: %zu bytes",
                 fileSize,
                 expectedMaxSize);
        return create_image_error("too_large", filename, nullptr, sizeInfo);
    }

    return error_type::None;
}

photo_frame_error create_battery_error(const char* errorType,
                                       float voltage,
                                       float percentage,
                                       float temperature,
                                       const char* context) {
    photo_frame_error error;

    if (strcmp(errorType, "voltage_low") == 0) {
        error = error_type::BatteryVoltageLow;
    } else if (strcmp(errorType, "voltage_unstable") == 0) {
        error = error_type::BatteryVoltageUnstable;
    } else if (strcmp(errorType, "temperature_high") == 0) {
        error = error_type::BatteryTemperatureHigh;
    } else if (strcmp(errorType, "temperature_low") == 0) {
        error = error_type::BatteryTemperatureLow;
    } else if (strcmp(errorType, "aging_detected") == 0) {
        error = error_type::BatteryAgingDetected;
    } else if (strcmp(errorType, "calibration_needed") == 0) {
        error = error_type::BatteryCalibrationNeeded;
    } else if (strcmp(errorType, "discharge_rate_high") == 0) {
        error = error_type::BatteryDischargeRateTooHigh;
    } else if (strcmp(errorType, "charge_rate_slow") == 0) {
        error = error_type::BatteryChargeRateTooSlow;
    } else if (strcmp(errorType, "capacity_reduced") == 0) {
        error = error_type::BatteryCapacityReduced;
    } else if (strcmp(errorType, "health_poor") == 0) {
        error = error_type::BatteryHealthPoor;
    } else if (strcmp(errorType, "critical") == 0) {
        error = error_type::BatteryLevelCritical;
    } else if (strcmp(errorType, "empty") == 0) {
        error = error_type::BatteryEmpty;
    } else if (strcmp(errorType, "not_detected") == 0) {
        error = error_type::BatteryNotDetected;
    } else {
        error = error_type::BatteryEmpty; // Default fallback
    }

    // Context removed to save DRAM - error message provides sufficient information - error message
    // provides sufficient information

    error.set_timestamp();
    return error;
}

photo_frame_error
validate_battery_voltage(float voltage, float minVoltage, float maxVoltage, const char* context) {
    if (voltage < minVoltage) {
        return create_battery_error("voltage_low", voltage, -1, -999, context);
    }

    if (voltage > maxVoltage) {
        char contextStr[128];
        snprintf(contextStr,
                 sizeof(contextStr),
                 "Voltage %.2fV exceeds maximum %.2fV%s%s",
                 voltage,
                 maxVoltage,
                 context ? ", " : "",
                 context ? context : "");
        return create_battery_error("voltage_unstable", voltage, -1, -999, contextStr);
    }

    return error_type::None;
}

photo_frame_error
validate_battery_temperature(float temperature, float minTemp, float maxTemp, const char* context) {
    if (temperature < minTemp) {
        return create_battery_error("temperature_low", -1, -1, temperature, context);
    }

    if (temperature > maxTemp) {
        return create_battery_error("temperature_high", -1, -1, temperature, context);
    }

    return error_type::None;
}

photo_frame_error
create_charging_error(const char* errorType, float current, float voltage, const char* context) {
    photo_frame_error error;

    if (strcmp(errorType, "charging_failed") == 0) {
        error = error_type::ChargingFailed;
    } else if (strcmp(errorType, "charger_not_connected") == 0) {
        error = error_type::ChargerNotConnected;
    } else if (strcmp(errorType, "charger_incompatible") == 0) {
        error = error_type::ChargerIncompatible;
    } else if (strcmp(errorType, "charging_overheat") == 0) {
        error = error_type::ChargingOverheat;
    } else if (strcmp(errorType, "charging_timeout") == 0) {
        error = error_type::ChargingTimeout;
    } else if (strcmp(errorType, "current_too_high") == 0) {
        error = error_type::ChargeCurrentTooHigh;
    } else if (strcmp(errorType, "current_too_low") == 0) {
        error = error_type::ChargeCurrentTooLow;
    } else if (strcmp(errorType, "circuit_fault") == 0) {
        error = error_type::ChargingCircuitFault;
    } else if (strcmp(errorType, "authentication_failed") == 0) {
        error = error_type::BatteryAuthenticationFailed;
    } else {
        error = error_type::ChargingFailed; // Default fallback
    }

    // Context removed to save DRAM - error message provides sufficient information

    error.set_timestamp();
    return error;
}

} // namespace error_utils

// Define the OTA error constants in the implementation file
// This allows the use of extern locale constants in the definitions
namespace error_type {} // namespace error_type

} // namespace photo_frame