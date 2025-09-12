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

    // Add context if provided
    if (context) {
        error.context = context;
    }

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

    if (context) {
        error.context = context;
    }

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

    if (context) {
        error.context = context;
    }

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
    } else if (strcmp(errorType, "resolution_too_high") == 0) {
        error = error_type::ImageResolutionTooHigh;
    } else {
        error = error_type::ImageProcessingAborted;
    }

    // Build context string from available information
    String contextStr = "";
    if (filename) {
        contextStr += "File: ";
        contextStr += filename;
    }
    if (dimensions) {
        if (contextStr.length() > 0)
            contextStr += ", ";
        contextStr += "Size: ";
        contextStr += dimensions;
    }
    if (context) {
        if (contextStr.length() > 0)
            contextStr += ", ";
        contextStr += context;
    }

    if (contextStr.length() > 0) {
        // Note: Using static buffer for context string
        static char contextBuffer[256];
        contextStr.toCharArray(contextBuffer, sizeof(contextBuffer));
        error.context = contextBuffer;
    }

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

    // Build context string with battery metrics
    String contextStr = "";
    if (voltage >= 0) {
        contextStr += "Voltage: ";
        contextStr += String(voltage, 2);
        contextStr += "V";
    }
    if (percentage >= 0) {
        if (contextStr.length() > 0)
            contextStr += ", ";
        contextStr += "Level: ";
        contextStr += String(percentage, 1);
        contextStr += "%";
    }
    if (temperature > -999) {
        if (contextStr.length() > 0)
            contextStr += ", ";
        contextStr += "Temp: ";
        contextStr += String(temperature, 1);
        contextStr += "°C";
    }
    if (context) {
        if (contextStr.length() > 0)
            contextStr += ", ";
        contextStr += context;
    }

    if (contextStr.length() > 0) {
        static char contextBuffer[256];
        contextStr.toCharArray(contextBuffer, sizeof(contextBuffer));
        error.context = contextBuffer;
    }

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

    // Build context string with charging metrics
    String contextStr = "";
    if (current >= 0) {
        contextStr += "Current: ";
        contextStr += String(current, 2);
        contextStr += "A";
    }
    if (voltage >= 0) {
        if (contextStr.length() > 0)
            contextStr += ", ";
        contextStr += "Voltage: ";
        contextStr += String(voltage, 2);
        contextStr += "V";
    }
    if (context) {
        if (contextStr.length() > 0)
            contextStr += ", ";
        contextStr += context;
    }

    if (contextStr.length() > 0) {
        static char contextBuffer[256];
        contextStr.toCharArray(contextBuffer, sizeof(contextBuffer));
        error.context = contextBuffer;
    }

    error.set_timestamp();
    return error;
}

photo_frame_error create_power_supply_error(const char* errorType,
                                            float voltage,
                                            float current,
                                            const char* context) {
    photo_frame_error error;

    if (strcmp(errorType, "insufficient") == 0) {
        error = error_type::PowerSupplyInsufficient;
    } else if (strcmp(errorType, "unstable") == 0) {
        error = error_type::PowerSupplyUnstable;
    } else if (strcmp(errorType, "overvoltage") == 0) {
        error = error_type::PowerSupplyOvervoltage;
    } else if (strcmp(errorType, "undervoltage") == 0) {
        error = error_type::PowerSupplyUndervoltage;
    } else if (strcmp(errorType, "regulator_failed") == 0) {
        error = error_type::PowerRegulatorFailed;
    } else if (strcmp(errorType, "noise") == 0) {
        error = error_type::PowerSupplyNoise;
    } else if (strcmp(errorType, "efficiency_low") == 0) {
        error = error_type::PowerSupplyEfficiencyLow;
    } else if (strcmp(errorType, "overcurrent") == 0) {
        error = error_type::PowerSupplyOvercurrent;
    } else if (strcmp(errorType, "short_circuit") == 0) {
        error = error_type::PowerSupplyShortCircuit;
    } else if (strcmp(errorType, "disconnected") == 0) {
        error = error_type::PowerSupplyDisconnected;
    } else {
        error = error_type::PowerSupplyInsufficient; // Default fallback
    }

    // Build context string with power supply metrics
    String contextStr = "";
    if (voltage >= 0) {
        contextStr += "Voltage: ";
        contextStr += String(voltage, 2);
        contextStr += "V";
    }
    if (current >= 0) {
        if (contextStr.length() > 0)
            contextStr += ", ";
        contextStr += "Current: ";
        contextStr += String(current, 3);
        contextStr += "A";
    }
    if (context) {
        if (contextStr.length() > 0)
            contextStr += ", ";
        contextStr += context;
    }

    if (contextStr.length() > 0) {
        static char contextBuffer[256];
        contextStr.toCharArray(contextBuffer, sizeof(contextBuffer));
        error.context = contextBuffer;
    }

    error.set_timestamp();
    return error;
}

photo_frame_error create_display_error(const char* errorType,
                                       int width,
                                       int height,
                                       const char* displayMode,
                                       const char* context) {
    photo_frame_error error;

    if (strcmp(errorType, "init_failed") == 0) {
        error = error_type::DisplayInitializationFailed;
    } else if (strcmp(errorType, "driver_error") == 0) {
        error = error_type::DisplayDriverError;
    } else if (strcmp(errorType, "spi_comm_error") == 0) {
        error = error_type::DisplaySpiCommError;
    } else if (strcmp(errorType, "busy_timeout") == 0) {
        error = error_type::DisplayBusyTimeout;
    } else if (strcmp(errorType, "reset_failed") == 0) {
        error = error_type::DisplayResetFailed;
    } else if (strcmp(errorType, "power_on_failed") == 0) {
        error = error_type::DisplayPowerOnFailed;
    } else if (strcmp(errorType, "power_off_failed") == 0) {
        error = error_type::DisplayPowerOffFailed;
    } else if (strcmp(errorType, "wakeup_failed") == 0) {
        error = error_type::DisplayWakeupFailed;
    } else if (strcmp(errorType, "command_error") == 0) {
        error = error_type::DisplayCommandError;
    } else if (strcmp(errorType, "hardware_fault") == 0) {
        error = error_type::DisplayHardwareFault;
    } else if (strcmp(errorType, "resolution_mismatch") == 0) {
        error = error_type::DisplayResolutionMismatch;
    } else if (strcmp(errorType, "color_depth_unsupported") == 0) {
        error = error_type::DisplayColorDepthUnsupported;
    } else if (strcmp(errorType, "orientation_invalid") == 0) {
        error = error_type::DisplayOrientationInvalid;
    } else if (strcmp(errorType, "mode_not_supported") == 0) {
        error = error_type::DisplayModeNotSupported;
    } else {
        error = error_type::DisplayInitializationFailed; // Default fallback
    }

    // Build context string with display information
    String contextStr = "";
    if (width > 0 && height > 0) {
        contextStr += "Resolution: ";
        contextStr += String(width);
        contextStr += "x";
        contextStr += String(height);
    }
    if (displayMode) {
        if (contextStr.length() > 0)
            contextStr += ", ";
        contextStr += "Mode: ";
        contextStr += displayMode;
    }
    if (context) {
        if (contextStr.length() > 0)
            contextStr += ", ";
        contextStr += context;
    }

    if (contextStr.length() > 0) {
        static char contextBuffer[256];
        contextStr.toCharArray(contextBuffer, sizeof(contextBuffer));
        error.context = contextBuffer;
    }

    error.set_timestamp();
    return error;
}

photo_frame_error create_epaper_error(const char* errorType,
                                      int refreshCount,
                                      float temperature,
                                      const char* waveform,
                                      const char* context) {
    photo_frame_error error;

    if (strcmp(errorType, "refresh_failed") == 0) {
        error = error_type::EpaperRefreshFailed;
    } else if (strcmp(errorType, "partial_refresh_not_supported") == 0) {
        error = error_type::EpaperPartialRefreshNotSupported;
    } else if (strcmp(errorType, "ghosting_detected") == 0) {
        error = error_type::EpaperGhostingDetected;
    } else if (strcmp(errorType, "temperature_compensation_failed") == 0) {
        error = error_type::EpaperTemperatureCompensationFailed;
    } else if (strcmp(errorType, "waveform_error") == 0) {
        error = error_type::EpaperWaveformError;
    } else if (strcmp(errorType, "voltage_regulation_error") == 0) {
        error = error_type::EpaperVoltageRegulationError;
    } else if (strcmp(errorType, "pixel_stuck") == 0) {
        error = error_type::EpaperPixelStuckError;
    } else if (strcmp(errorType, "contrast_poor") == 0) {
        error = error_type::EpaperContrastPoor;
    } else if (strcmp(errorType, "refresh_too_frequent") == 0) {
        error = error_type::EpaperRefreshTooFrequent;
    } else if (strcmp(errorType, "lifetime_exceeded") == 0) {
        error = error_type::EpaperLifetimeExceeded;
    } else {
        error = error_type::EpaperRefreshFailed; // Default fallback
    }

    // Build context string with e-paper specific information
    String contextStr = "";
    if (refreshCount >= 0) {
        contextStr += "Refresh count: ";
        contextStr += String(refreshCount);
    }
    if (temperature > -999) {
        if (contextStr.length() > 0)
            contextStr += ", ";
        contextStr += "Temp: ";
        contextStr += String(temperature, 1);
        contextStr += "°C";
    }
    if (waveform) {
        if (contextStr.length() > 0)
            contextStr += ", ";
        contextStr += "Waveform: ";
        contextStr += waveform;
    }
    if (context) {
        if (contextStr.length() > 0)
            contextStr += ", ";
        contextStr += context;
    }

    if (contextStr.length() > 0) {
        static char contextBuffer[256];
        contextStr.toCharArray(contextBuffer, sizeof(contextBuffer));
        error.context = contextBuffer;
    }

    error.set_timestamp();
    return error;
}

photo_frame_error validate_display_resolution(int width,
                                              int height,
                                              int maxWidth,
                                              int maxHeight,
                                              const char* context) {
    if (width <= 0 || height <= 0) {
        return create_display_error(
            "resolution_mismatch", width, height, nullptr, "Width or height is zero or negative");
    }

    if (width > maxWidth || height > maxHeight) {
        char contextStr[128];
        snprintf(contextStr,
                 sizeof(contextStr),
                 "Exceeds maximum %dx%d%s%s",
                 maxWidth,
                 maxHeight,
                 context ? ", " : "",
                 context ? context : "");
        return create_display_error("resolution_mismatch", width, height, nullptr, contextStr);
    }

    return error_type::None;
}

photo_frame_error validate_display_refresh_rate(float refreshRate,
                                                float minRate,
                                                float maxRate,
                                                const char* context) {
    if (refreshRate < minRate) {
        char contextStr[128];
        snprintf(contextStr,
                 sizeof(contextStr),
                 "Rate %.2fHz below minimum %.2fHz%s%s",
                 refreshRate,
                 minRate,
                 context ? ", " : "",
                 context ? context : "");
        return create_display_error("refresh_rate_invalid", -1, -1, nullptr, contextStr);
    }

    if (refreshRate > maxRate) {
        char contextStr[128];
        snprintf(contextStr,
                 sizeof(contextStr),
                 "Rate %.2fHz exceeds maximum %.2fHz%s%s",
                 refreshRate,
                 maxRate,
                 context ? ", " : "",
                 context ? context : "");
        return create_display_error("refresh_rate_invalid", -1, -1, nullptr, contextStr);
    }

    return error_type::None;
}

photo_frame_error create_display_rendering_error(const char* errorType,
                                                 size_t bufferSize,
                                                 size_t memoryUsed,
                                                 const char* operation,
                                                 const char* context) {
    photo_frame_error error;

    if (strcmp(errorType, "buffer_overflow") == 0) {
        error = error_type::DisplayBufferOverflow;
    } else if (strcmp(errorType, "buffer_underflow") == 0) {
        error = error_type::DisplayBufferUnderflow;
    } else if (strcmp(errorType, "memory_allocation_failed") == 0) {
        error = error_type::DisplayMemoryAllocationFailed;
    } else if (strcmp(errorType, "framebuffer_corrupted") == 0) {
        error = error_type::DisplayFramebufferCorrupted;
    } else if (strcmp(errorType, "pixel_format_error") == 0) {
        error = error_type::DisplayPixelFormatError;
    } else if (strcmp(errorType, "color_space_error") == 0) {
        error = error_type::DisplayColorSpaceError;
    } else if (strcmp(errorType, "scaling_error") == 0) {
        error = error_type::DisplayScalingError;
    } else if (strcmp(errorType, "rotation_error") == 0) {
        error = error_type::DisplayRotationError;
    } else if (strcmp(errorType, "clipping_error") == 0) {
        error = error_type::DisplayClippingError;
    } else if (strcmp(errorType, "rendering_timeout") == 0) {
        error = error_type::DisplayRenderingTimeout;
    } else {
        error = error_type::DisplayBufferOverflow; // Default fallback
    }

    // Build context string with rendering information
    String contextStr = "";
    if (bufferSize > 0) {
        contextStr += "Buffer: ";
        contextStr += String(bufferSize);
        contextStr += " bytes";
    }
    if (memoryUsed > 0) {
        if (contextStr.length() > 0)
            contextStr += ", ";
        contextStr += "Memory used: ";
        contextStr += String(memoryUsed);
        contextStr += " bytes";
    }
    if (operation) {
        if (contextStr.length() > 0)
            contextStr += ", ";
        contextStr += "Operation: ";
        contextStr += operation;
    }
    if (context) {
        if (contextStr.length() > 0)
            contextStr += ", ";
        contextStr += context;
    }

    if (contextStr.length() > 0) {
        static char contextBuffer[256];
        contextStr.toCharArray(contextBuffer, sizeof(contextBuffer));
        error.context = contextBuffer;
    }

    error.set_timestamp();
    return error;
}

} // namespace error_utils
} // namespace photo_frame