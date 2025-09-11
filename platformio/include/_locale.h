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

#ifndef __PHOTO_FRAME_LOCALE_H__
#define __PHOTO_FRAME_LOCALE_H__

extern const char* TXT_NO_ERROR;
extern const char* TXT_CARD_MOUNT_FAILED;
extern const char* TXT_NO_SD_CARD_ATTACHED;
extern const char* TXT_UNKNOWN_SD_CARD_TYPE;
extern const char* TXT_CARD_OPEN_FILE_FAILED;
extern const char* TXT_CARD_CREATE_FILE_FAILED;
extern const char* TXT_SD_CARD_FILE_NOT_FOUND;
extern const char* TXT_SD_CARD_FILE_OPEN_FAILED;
extern const char* TXT_SD_CARD_FILE_CREATE_FAILED;
extern const char* TXT_SD_CARD_DIR_CREATE_FAILED;
extern const char* TXT_IMAGE_FORMAT_NOT_SUPPORTED;
extern const char* TXT_BATTERY_LEVEL_CRITICAL;
extern const char* TXT_CARD_TOC_OPEN_FILE_FAILED;
extern const char* TXT_PREFERENCES_OPEN_FAILED;
extern const char* TXT_NO_IMAGES_FOUND;
extern const char* TXT_BATTERY_EMPTY;
extern const char* TXT_RTC_MODULE_NOT_FOUND;
extern const char* TXT_RTC_MODULE_NOT_FOUND;
extern const char* TXT_JWT_CREATION_FAILED;
extern const char* TXT_HTTP_POST_FAILED;
extern const char* TXT_JSON_PARSE_FAILED;
extern const char* TXT_TOKEN_MISSING;
extern const char* TXT_FILE_OPEN_FAILED;
extern const char* TXT_HTTP_CONNECT_FAILED;
extern const char* TXT_HTTP_GET_FAILED;
extern const char* TXT_DOWNLOAD_FAILED;
extern const char* TXT_SSL_CERTIFICATE_LOAD_FAILED;
extern const char* TXT_RATE_LIMIT_TIMEOUT_EXCEEDED;
extern const char* TXT_WIFI_CREDENTIALS_NOT_FOUND;
extern const char* TXT_WIFI_CONNECTION_FAILED;

// Google Drive configuration validation error messages
extern const char* TXT_CONFIG_MISSING_SECTION;
extern const char* TXT_CONFIG_MISSING_FIELD;
extern const char* TXT_CONFIG_INVALID_EMAIL;
extern const char* TXT_CONFIG_INVALID_PEM_KEY;
extern const char* TXT_CONFIG_INVALID_PATH;
extern const char* TXT_CONFIG_INVALID_FILENAME;
extern const char* TXT_CONFIG_VALUE_OUT_OF_RANGE;

extern const char* TXT_ERROR_CODE;

// ========================================
// COMPREHENSIVE ERROR SYSTEM LOCALIZATION  
// ========================================

// SD Card Specific Errors (100-119)
extern const char* TXT_SD_CARD_WRITE_PROTECTED;
extern const char* TXT_SD_CARD_CORRUPTED;
extern const char* TXT_SD_CARD_INSUFFICIENT_SPACE;
extern const char* TXT_SD_CARD_SLOW_RESPONSE;
extern const char* TXT_SD_CARD_READ_ONLY;
extern const char* TXT_SD_CARD_BAD_SECTOR;
extern const char* TXT_SD_CARD_SIZE_INVALID;
extern const char* TXT_SD_CARD_INIT_FAILED;
extern const char* TXT_SD_CARD_VERSION_UNSUPPORTED;
extern const char* TXT_SD_CARD_FILESYSTEM_UNSUPPORTED;
// LittleFS Specific Errors (110-119)
extern const char* TXT_LITTLEFS_INIT_FAILED;
extern const char* TXT_LITTLEFS_FILE_CREATE_FAILED;
extern const char* TXT_LITTLEFS_FILE_OPEN_FAILED;

// Network/WiFi Specific Errors (120-139)
extern const char* TXT_WIFI_SIGNAL_TOO_WEAK;
extern const char* TXT_WIFI_AUTHENTICATION_FAILED;
extern const char* TXT_WIFI_DHCP_FAILED;
extern const char* TXT_WIFI_DNS_RESOLUTION_FAILED;
extern const char* TXT_WIFI_NETWORK_NOT_FOUND;
extern const char* TXT_WIFI_PASSWORD_INCORRECT;
extern const char* TXT_WIFI_CHANNEL_BUSY;
extern const char* TXT_WIFI_FREQUENCY_NOT_SUPPORTED;
extern const char* TXT_HTTP_REQUEST_TIMEOUT;
extern const char* TXT_HTTP_INVALID_RESPONSE;
extern const char* TXT_SSL_HANDSHAKE_FAILED;
extern const char* TXT_NETWORK_INTERFACE_DOWN;
extern const char* TXT_NETWORK_CONFIG_INVALID;
extern const char* TXT_PROXY_CONNECTION_FAILED;
extern const char* TXT_NETWORK_TIMEOUT_EXCEEDED;

// Configuration Specific Errors (140-159)
extern const char* TXT_CONFIG_FILE_CORRUPTED;
extern const char* TXT_CONFIG_JSON_SYNTAX_ERROR;
extern const char* TXT_CONFIG_VERSION_MISMATCH;
extern const char* TXT_CONFIG_FIELD_TYPE_MISMATCH;
extern const char* TXT_CONFIG_ENCRYPTION_KEY_INVALID;
extern const char* TXT_CONFIG_BACKUP_FAILED;
extern const char* TXT_CONFIG_RESTORE_FAILED;
extern const char* TXT_CONFIG_VALIDATION_FAILED;
extern const char* TXT_CONFIG_DEFAULTS_MISSING;
extern const char* TXT_CONFIG_SCHEMA_INVALID;
extern const char* TXT_CONFIG_ACCESS_DENIED;
extern const char* TXT_CONFIG_FORMAT_UNSUPPORTED;
extern const char* TXT_CONFIG_SIZE_LIMIT_EXCEEDED;
extern const char* TXT_CONFIG_DEPENDENCY_MISSING;
extern const char* TXT_CONFIG_ENVIRONMENT_MISMATCH;

// OAuth/Authentication Specific Errors (40-49)
extern const char* TXT_OAUTH_TOKEN_EXPIRED;
extern const char* TXT_OAUTH_TOKEN_INVALID;
extern const char* TXT_OAUTH_REFRESH_TOKEN_MISSING;
extern const char* TXT_OAUTH_REFRESH_TOKEN_INVALID;
extern const char* TXT_OAUTH_SCOPE_INSUFFICIENT;
extern const char* TXT_OAUTH_JWT_SIGNING_FAILED;
extern const char* TXT_OAUTH_SERVICE_ACCOUNT_KEY_INVALID;
extern const char* TXT_OAUTH_TOKEN_REQUEST_FAILED;
extern const char* TXT_OAUTH_TOKEN_REFRESH_FAILED;

// Google Drive API Specific Errors (50-69)
extern const char* TXT_GOOGLE_DRIVE_API_QUOTA_EXCEEDED;
extern const char* TXT_GOOGLE_DRIVE_API_RATE_LIMITED;
extern const char* TXT_GOOGLE_DRIVE_FILE_NOT_FOUND;
extern const char* TXT_GOOGLE_DRIVE_FOLDER_NOT_FOUND;
extern const char* TXT_GOOGLE_DRIVE_PERMISSION_DENIED;
extern const char* TXT_GOOGLE_DRIVE_STORAGE_QUOTA_EXCEEDED;
extern const char* TXT_GOOGLE_DRIVE_API_DISABLED;
extern const char* TXT_GOOGLE_DRIVE_FILE_TOO_BIG;
extern const char* TXT_GOOGLE_DRIVE_FILE_CORRUPTED;
extern const char* TXT_GOOGLE_DRIVE_INVALID_QUERY;
extern const char* TXT_GOOGLE_DRIVE_API_INTERNAL_ERROR;
extern const char* TXT_GOOGLE_DRIVE_NETWORK_TIMEOUT;
extern const char* TXT_GOOGLE_DRIVE_RESPONSE_MALFORMED;
extern const char* TXT_GOOGLE_DRIVE_API_UNAVAILABLE;
extern const char* TXT_GOOGLE_DRIVE_DOWNLOAD_INTERRUPTED;
extern const char* TXT_GOOGLE_DRIVE_METADATA_INVALID;
extern const char* TXT_GOOGLE_DRIVE_FOLDER_EMPTY;
extern const char* TXT_GOOGLE_DRIVE_API_VERSION_UNSUPPORTED;

// HTTP Specific Errors (70-79)
extern const char* TXT_HTTP_UNAUTHORIZED;
extern const char* TXT_HTTP_FORBIDDEN;
extern const char* TXT_HTTP_NOT_FOUND;
extern const char* TXT_HTTP_TOO_MANY_REQUESTS;
extern const char* TXT_HTTP_INTERNAL_SERVER_ERROR;
extern const char* TXT_HTTP_BAD_GATEWAY;
extern const char* TXT_HTTP_SERVICE_UNAVAILABLE;
extern const char* TXT_HTTP_GATEWAY_TIMEOUT;
extern const char* TXT_HTTP_BAD_REQUEST;

// Image Processing Specific Errors (80-99)
extern const char* TXT_IMAGE_FILE_CORRUPTED;
extern const char* TXT_IMAGE_FILE_TOO_LARGE;
extern const char* TXT_IMAGE_DIMENSIONS_INVALID;
extern const char* TXT_IMAGE_DIMENSIONS_MISMATCH;
extern const char* TXT_IMAGE_COLOR_DEPTH_UNSUPPORTED;
extern const char* TXT_IMAGE_PIXEL_DATA_CORRUPTED;
extern const char* TXT_IMAGE_FILE_SEEK_FAILED;
extern const char* TXT_IMAGE_FILE_READ_FAILED;
extern const char* TXT_IMAGE_BUFFER_OVERFLOW;
extern const char* TXT_IMAGE_BUFFER_UNDERFLOW;
extern const char* TXT_IMAGE_RENDER_TIMEOUT;
extern const char* TXT_IMAGE_DISPLAY_WRITE_FAILED;
extern const char* TXT_IMAGE_FILE_HEADER_INVALID;
extern const char* TXT_IMAGE_FILE_EMPTY;
extern const char* TXT_IMAGE_FILE_TRUNCATED;
extern const char* TXT_IMAGE_MEMORY_ALLOCATION_FAILED;
extern const char* TXT_IMAGE_PALETTE_INVALID;
extern const char* TXT_IMAGE_CONVERSION_FAILED;
extern const char* TXT_IMAGE_RESOLUTION_TOO_HIGH;
extern const char* TXT_IMAGE_PROCESSING_ABORTED;

// Battery/Power Errors (160-199)
extern const char* TXT_BATTERY_VOLTAGE_LOW;
extern const char* TXT_BATTERY_VOLTAGE_UNSTABLE;
extern const char* TXT_BATTERY_TEMPERATURE_HIGH;
extern const char* TXT_BATTERY_TEMPERATURE_LOW;
extern const char* TXT_BATTERY_AGING_DETECTED;
extern const char* TXT_BATTERY_CALIBRATION_NEEDED;
extern const char* TXT_BATTERY_DISCHARGE_RATE_TOO_HIGH;
extern const char* TXT_BATTERY_CHARGE_RATE_TOO_SLOW;
extern const char* TXT_BATTERY_CAPACITY_REDUCED;
extern const char* TXT_BATTERY_HEALTH_POOR;

// Charging Specific Errors (170-179)
extern const char* TXT_CHARGING_FAILED;
extern const char* TXT_CHARGER_NOT_CONNECTED;
extern const char* TXT_CHARGER_INCOMPATIBLE;
extern const char* TXT_CHARGING_OVERHEAT;
extern const char* TXT_CHARGING_TIMEOUT;
extern const char* TXT_CHARGE_CURRENT_TOO_HIGH;
extern const char* TXT_CHARGE_CURRENT_TOO_LOW;
extern const char* TXT_CHARGING_CIRCUIT_FAULT;
extern const char* TXT_BATTERY_NOT_DETECTED;
extern const char* TXT_BATTERY_AUTHENTICATION_FAILED;

// Power Supply Errors (180-189)
extern const char* TXT_POWER_SUPPLY_INSUFFICIENT;
extern const char* TXT_POWER_SUPPLY_UNSTABLE;
extern const char* TXT_POWER_SUPPLY_OVERVOLTAGE;
extern const char* TXT_POWER_SUPPLY_UNDERVOLTAGE;
extern const char* TXT_POWER_REGULATOR_FAILED;
extern const char* TXT_POWER_SUPPLY_NOISE;
extern const char* TXT_POWER_SUPPLY_EFFICIENCY_LOW;
extern const char* TXT_POWER_SUPPLY_OVERCURRENT;
extern const char* TXT_POWER_SUPPLY_SHORT_CIRCUIT;
extern const char* TXT_POWER_SUPPLY_DISCONNECTED;

// Power Management Errors (190-199)
extern const char* TXT_POWER_SAVING_MODE_ENTER_FAILED;
extern const char* TXT_POWER_SAVING_MODE_EXIT_FAILED;
extern const char* TXT_SLEEP_MODE_ACTIVATION_FAILED;
extern const char* TXT_WAKEUP_SOURCE_INVALID;
extern const char* TXT_POWER_CONSUMPTION_TOO_HIGH;
extern const char* TXT_POWER_MANAGER_INIT_FAILED;
extern const char* TXT_CLOCK_FREQUENCY_ERROR;
extern const char* TXT_VOLTAGE_SCALING_FAILED;
extern const char* TXT_POWER_DOMAIN_ERROR;
extern const char* TXT_THERMAL_THROTTLING_ACTIVE;

// Display Hardware Errors (200-219)
extern const char* TXT_DISPLAY_INITIALIZATION_FAILED;
extern const char* TXT_DISPLAY_DRIVER_ERROR;
extern const char* TXT_DISPLAY_SPI_COMM_ERROR;
extern const char* TXT_DISPLAY_BUSY_TIMEOUT;
extern const char* TXT_DISPLAY_RESET_FAILED;
extern const char* TXT_DISPLAY_POWER_ON_FAILED;
extern const char* TXT_DISPLAY_POWER_OFF_FAILED;
extern const char* TXT_DISPLAY_WAKEUP_FAILED;
extern const char* TXT_DISPLAY_COMMAND_ERROR;
extern const char* TXT_DISPLAY_HARDWARE_FAULT;

// E-Paper Specific Errors (210-229)
extern const char* TXT_EPAPER_REFRESH_FAILED;
extern const char* TXT_EPAPER_PARTIAL_REFRESH_NOT_SUPPORTED;
extern const char* TXT_EPAPER_GHOSTING_DETECTED;
extern const char* TXT_EPAPER_TEMPERATURE_COMPENSATION_FAILED;
extern const char* TXT_EPAPER_WAVEFORM_ERROR;
extern const char* TXT_EPAPER_VOLTAGE_REGULATION_ERROR;
extern const char* TXT_EPAPER_PIXEL_STUCK_ERROR;
extern const char* TXT_EPAPER_CONTRAST_POOR;
extern const char* TXT_EPAPER_REFRESH_TOO_FREQUENT;
extern const char* TXT_EPAPER_LIFETIME_EXCEEDED;

// Display Rendering Errors (230-249)
extern const char* TXT_DISPLAY_BUFFER_OVERFLOW;
extern const char* TXT_DISPLAY_BUFFER_UNDERFLOW;
extern const char* TXT_DISPLAY_MEMORY_ALLOCATION_FAILED;
extern const char* TXT_DISPLAY_FRAMEBUFFER_CORRUPTED;
extern const char* TXT_DISPLAY_PIXEL_FORMAT_ERROR;
extern const char* TXT_DISPLAY_COLOR_SPACE_ERROR;
extern const char* TXT_DISPLAY_SCALING_ERROR;
extern const char* TXT_DISPLAY_ROTATION_ERROR;
extern const char* TXT_DISPLAY_CLIPPING_ERROR;
extern const char* TXT_DISPLAY_RENDERING_TIMEOUT;

// Display Configuration Errors (250-259)
extern const char* TXT_DISPLAY_RESOLUTION_MISMATCH;
extern const char* TXT_DISPLAY_COLOR_DEPTH_UNSUPPORTED;
extern const char* TXT_DISPLAY_ORIENTATION_INVALID;
extern const char* TXT_DISPLAY_REFRESH_RATE_INVALID;
extern const char* TXT_DISPLAY_GAMMA_CONFIG_ERROR;
extern const char* TXT_DISPLAY_BRIGHTNESS_CONTROL_ERROR;
extern const char* TXT_DISPLAY_CONTRAST_CONTROL_ERROR;
extern const char* TXT_DISPLAY_TIMING_CONFIG_ERROR;
extern const char* TXT_DISPLAY_MODE_NOT_SUPPORTED;
extern const char* TXT_DISPLAY_CALIBRATION_REQUIRED;

#endif