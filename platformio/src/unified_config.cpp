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

#include "unified_config.h"
#include "config.h"
#include "preferences_helper.h"
#include <ArduinoJson.h>

namespace photo_frame {

/**
 * @brief Load fallback configuration values from config.h defines
 */
void load_fallback_config(unified_config& config) {
    log_i("Loading fallback configuration from config.h");

    // WiFi - no fallback, must be provided
    config.wifi.network_count = 0;

    // Board configuration fallbacks
    config.board.refresh.min_seconds = REFRESH_MIN_INTERVAL_SECONDS;
    config.board.refresh.max_seconds = REFRESH_MAX_INTERVAL_SECONDS;
    config.board.refresh.step = REFRESH_STEP_SECONDS;
    config.board.refresh.default_seconds = REFRESH_DEFAULT_INTERVAL_SECONDS;
    config.board.refresh.low_battery_multiplier = REFRESH_INTERVAL_LOW_BATTERY_MULTIPLIER;
    config.board.day_start_hour = DAY_START_HOUR;
    config.board.day_end_hour = DAY_END_HOUR;

    // Google Drive - no fallback, must be provided
    config.google_drive.auth.service_account_email = "";
    config.google_drive.auth.private_key_pem = "";
    config.google_drive.auth.client_id = "";
    config.google_drive.drive.folder_id = "";
    config.google_drive.drive.root_ca_path = "";
    config.google_drive.drive.list_page_size = GOOGLE_DRIVE_MAX_LIST_PAGE_SIZE;
    config.google_drive.drive.use_insecure_tls = true;
    config.google_drive.caching.local_path = GOOGLE_DRIVE_CACHING_LOCAL_PATH;
    config.google_drive.caching.toc_max_age_seconds = GOOGLE_DRIVE_TOC_MAX_AGE_SECONDS;
}

photo_frame_error_t load_unified_config(sd_card& sdCard,
                                       const char* config_path,
                                       unified_config& config) {
    log_i("Loading configuration from: %s", config_path);

    if (!sdCard.is_initialized()) {
        log_e("SD card not initialized");
        return error_type::CardMountFailed;
    }

    if (!sdCard.file_exists(config_path)) {
        log_e("Configuration file not found");
        return error_type::SdCardFileNotFound;
    }

    // Open configuration file
    fs::File config_file = sdCard.open(config_path, FILE_READ);
    if (!config_file) {
        log_e("Failed to open configuration file");
        return error_type::CardOpenFileFailed;
    }

    // Read file size
    size_t file_size = config_file.size();
    if (file_size == 0) {
        log_e("Configuration file is empty");
        config_file.close();
        return error_type::SdCardFileNotFound; // Treat empty file as not found
    }

    log_d("Configuration file size: %lu bytes", file_size);

    // Allocate buffer for JSON parsing using PSRAM
    size_t buffer_size = file_size + 512; // Extra space for JSON parsing
    char* buffer = (char*)heap_caps_malloc(buffer_size, MALLOC_CAP_SPIRAM);
    if (!buffer) {
        buffer = (char*)malloc(buffer_size);
    }

    if (!buffer) {
        log_e("Failed to allocate buffer for config");
        config_file.close();
        return error_type::CardOpenFileFailed; // Use available error type
    }

    // Read file content
    size_t bytes_read = config_file.readBytes(buffer, file_size);
    config_file.close();

    if (bytes_read != file_size) {
        log_e("Failed to read complete configuration file");
        free(buffer);
        return error_type::CardOpenFileFailed; // Use available error type
    }

    buffer[bytes_read] = '\0'; // Null terminate

    // Parse JSON
    DynamicJsonDocument doc(buffer_size);
    DeserializationError json_error = deserializeJson(doc, buffer);
    free(buffer);

    if (json_error) {
        log_e("JSON parsing failed: %s", json_error.c_str());
        return error_type::JsonParseFailed;
    }

    // Extract WiFi configuration with backward compatibility
    // Supports both array format (v0.11.0+) and legacy object format (v0.10.0 and earlier)
    //
    // New format (v0.11.0+):
    //   "wifi": [
    //     {"ssid": "Network1", "password": "pass1"},
    //     {"ssid": "Network2", "password": "pass2"}
    //   ]
    //
    // Legacy format (v0.10.0 and earlier):
    //   "wifi": {
    //     "ssid": "Network1",
    //     "password": "pass1"
    //   }
    //
    // The legacy format is automatically migrated to array format at runtime
    // for backward compatibility with existing config.json files.
    if (doc.containsKey("wifi")) {
        config.wifi.network_count = 0;

        if (doc["wifi"].is<JsonArray>()) {
            // New format: Array of networks (v0.11.0+)
            JsonArray wifiArray = doc["wifi"].as<JsonArray>();

            for (JsonObject wifiObj : wifiArray) {
                if (config.wifi.network_count >= WIFI_MAX_NETWORKS) {
                    log_w("Maximum number of WiFi networks (%d) exceeded, ignoring additional entries", WIFI_MAX_NETWORKS);
                    break;
                }

                String ssid = wifiObj["ssid"].as<String>();
                String password = wifiObj["password"].as<String>();

                if (ssid.length() > 0 && password.length() > 0) {
                    config.wifi.add_network(ssid, password);
                    log_d("WiFi network #%d - SSID: %s", config.wifi.network_count, ssid.c_str());
                } else {
                    log_w("Invalid WiFi network entry (empty SSID or password), skipping");
                }
            }

            if (config.wifi.network_count == 0) {
                log_e("No valid WiFi networks found in configuration");
            } else {
                log_i("Loaded %d WiFi network(s) from configuration", config.wifi.network_count);
            }
        } else if (doc["wifi"].is<JsonObject>()) {
            // Legacy format: Single network object (v0.10.0 and earlier)
            // Automatically migrates old config format to new array-based format
            // This ensures existing config.json files continue to work without manual updates
            log_i("Detected legacy WiFi configuration format (single object) - migrating to array format");
            JsonObject wifi_obj = doc["wifi"];

            String ssid = wifi_obj["ssid"].as<String>();
            String password = wifi_obj["password"].as<String>();

            if (ssid.length() > 0 && password.length() > 0) {
                config.wifi.add_network(ssid, password);
                log_i("Successfully migrated legacy WiFi configuration (SSID: %s)", ssid.c_str());
                log_i("Consider updating config.json to new array format for future compatibility");
            } else {
                log_e("Invalid legacy WiFi configuration (empty SSID or password)");
            }
        } else {
            log_w("WiFi configuration has unsupported format (must be array or object)");
        }
    } else {
        log_w("WiFi configuration missing");
    }

    // Extract board configuration
    if (doc.containsKey("board_config")) {
        JsonObject board_obj = doc["board_config"];

        if (board_obj.containsKey("refresh")) {
            JsonObject refresh_obj = board_obj["refresh"];

            // Load values with fallbacks, then apply validation/capping
            uint32_t min_seconds = refresh_obj["min_seconds"] | REFRESH_MIN_INTERVAL_SECONDS;
            uint32_t max_seconds = refresh_obj["max_seconds"] | REFRESH_MAX_INTERVAL_SECONDS;
            uint32_t step = refresh_obj["step"] | REFRESH_STEP_SECONDS;
            uint32_t default_seconds = refresh_obj["default"] | REFRESH_DEFAULT_INTERVAL_SECONDS;
            uint8_t multiplier = refresh_obj["low_battery_multiplier"] | REFRESH_INTERVAL_LOW_BATTERY_MULTIPLIER;

            // Apply validation and capping
            config.board.refresh.min_seconds = max(min_seconds, (uint32_t)REFRESH_MIN_INTERVAL_SECONDS);
            config.board.refresh.max_seconds = max(max_seconds, (uint32_t)REFRESH_MAX_INTERVAL_SECONDS);
            config.board.refresh.step = max(step, (uint32_t)REFRESH_STEP_SECONDS);
            config.board.refresh.low_battery_multiplier = max(multiplier, (uint8_t)REFRESH_INTERVAL_LOW_BATTERY_MULTIPLIER);

            // Ensure max >= min
            if (config.board.refresh.max_seconds < config.board.refresh.min_seconds) {
                config.board.refresh.max_seconds = config.board.refresh.min_seconds;
            }

            // Validate and clamp default_seconds to be within [min, max] range
            config.board.refresh.default_seconds = max(default_seconds, config.board.refresh.min_seconds);
            config.board.refresh.default_seconds = min(config.board.refresh.default_seconds, config.board.refresh.max_seconds);

            log_d("Refresh config - min: %d", config.board.refresh.min_seconds);
            log_d("Refresh config - max: %d", config.board.refresh.max_seconds);
            log_d("Refresh config - step: %d", config.board.refresh.step);
            log_d("Refresh config - default: %d", config.board.refresh.default_seconds);
            log_d("Refresh config - low battery multiplier: %d", config.board.refresh.low_battery_multiplier);
        } else {
            log_w("Board configuration missing 'refresh'");
        }

        // Load and validate day hours (0-23)

        if(!board_obj.containsKey("day_start_hour") || !board_obj.containsKey("day_end_hour")) {
            log_w("Board configuration missing 'day_start_hour' or 'day_end_hour'");
        }

        uint8_t start_hour = board_obj["day_start_hour"] | DAY_START_HOUR;
        uint8_t end_hour = board_obj["day_end_hour"] | DAY_END_HOUR;

        config.board.day_start_hour = min(start_hour, (uint8_t)23);
        config.board.day_end_hour = min(end_hour, (uint8_t)23);

        log_d("Day schedule - start: %d", config.board.day_start_hour);
        log_d("Day schedule - end: %d", config.board.day_end_hour);

        // Load portrait mode (dynamic display orientation)
        config.board.portrait_mode = board_obj["portrait_mode"] | false;
        log_i("Portrait mode: %s", config.board.portrait_mode ? "enabled" : "disabled");

    } else {
        log_w("Board configuration missing 'board_config'");
    }

    // Extract Google Drive configuration
    if (doc.containsKey("google_drive_config")) {
        JsonObject gd_obj = doc["google_drive_config"];

        // Check if Google Drive is enabled
        config.google_drive.enabled = gd_obj["enabled"] | true; // Default to true for backward compatibility
        log_i("Google Drive: %s", config.google_drive.enabled ? "enabled" : "disabled");

        if (gd_obj.containsKey("authentication")) {
            JsonObject auth_obj = gd_obj["authentication"];
            config.google_drive.auth.service_account_email = auth_obj["service_account_email"].as<String>();
            config.google_drive.auth.private_key_pem = auth_obj["private_key_pem"].as<String>();
            config.google_drive.auth.client_id = auth_obj["client_id"].as<String>();

            log_d("Google Drive authentication - email: %s", config.google_drive.auth.service_account_email.c_str());
            log_d("Google Drive authentication - client ID: %s", config.google_drive.auth.client_id.c_str());

        } else {
            log_w("Google Drive configuration missing 'authentication'");
        }

        if (gd_obj.containsKey("drive")) {
            JsonObject drive_obj = gd_obj["drive"];
            config.google_drive.drive.folder_id = drive_obj["folder_id"].as<String>();
            config.google_drive.drive.root_ca_path = drive_obj["root_ca_path"].as<String>();
            config.google_drive.drive.list_page_size = min(drive_obj["list_page_size"] | GOOGLE_DRIVE_MAX_LIST_PAGE_SIZE, GOOGLE_DRIVE_MAX_LIST_PAGE_SIZE);
            config.google_drive.drive.use_insecure_tls = drive_obj["use_insecure_tls"] | true;

            log_d("Google Drive configuration - folder ID: %s", config.google_drive.drive.folder_id.c_str());
            log_d("Google Drive configuration - root CA path: %s", config.google_drive.drive.root_ca_path.c_str());
            log_d("Google Drive configuration - list page size: %d", config.google_drive.drive.list_page_size);
            log_d("Google Drive configuration - use insecure TLS: %s", config.google_drive.drive.use_insecure_tls ? "true" : "false");

        } else {
            log_w("Google Drive configuration missing 'drive'");
        }

        if (gd_obj.containsKey("caching")) {
            JsonObject cache_obj = gd_obj["caching"];
            config.google_drive.caching.local_path = cache_obj["local_path"] | "/gdrive";
            config.google_drive.caching.toc_max_age_seconds = cache_obj["toc_max_age_seconds"] | GOOGLE_DRIVE_TOC_MAX_AGE_SECONDS;
        } else {
            log_w("Google Drive configuration missing 'caching'");
        }

        if (gd_obj.containsKey("rate_limiting")) {
            JsonObject rate_obj = gd_obj["rate_limiting"];
            config.google_drive.rate_limiting.max_requests_per_window = rate_obj["max_requests_per_window"] | GOOGLE_DRIVE_MAX_REQUESTS_PER_WINDOW;
            config.google_drive.rate_limiting.rate_limit_window_seconds = rate_obj["rate_limit_window_seconds"] | GOOGLE_DRIVE_RATE_LIMIT_WINDOW_SECONDS;
            config.google_drive.rate_limiting.min_request_delay_ms = rate_obj["min_request_delay_ms"] | GOOGLE_DRIVE_MIN_REQUEST_DELAY_MS;
            config.google_drive.rate_limiting.max_retry_attempts = rate_obj["max_retry_attempts"] | GOOGLE_DRIVE_MAX_RETRY_ATTEMPTS;
            config.google_drive.rate_limiting.backoff_base_delay_ms = rate_obj["backoff_base_delay_ms"] | GOOGLE_DRIVE_BACKOFF_BASE_DELAY_MS;
            config.google_drive.rate_limiting.max_wait_time_ms = rate_obj["max_wait_time_ms"] | GOOGLE_DRIVE_MAX_WAIT_TIME_MS;
        } else {
            log_w("Google Drive configuration missing 'rate_limiting'");
        }
    } else {
        log_w("Google Drive configuration missing: 'google_drive_config'");
    }

    // Extract SD Card configuration
    if (doc.containsKey("sd_card_config")) {
        JsonObject sd_obj = doc["sd_card_config"];

        config.sd_card.enabled = sd_obj["enabled"] | false;
        config.sd_card.images_directory = sd_obj["images_directory"] | "/images";

        log_i("SD Card: %s", config.sd_card.enabled ? "enabled" : "disabled");
        if (config.sd_card.enabled) {
            log_i("SD Card images directory: %s", config.sd_card.images_directory.c_str());
        }
    } else {
        // Default SD card config
        config.sd_card.enabled = false;
        config.sd_card.images_directory = "/images";
        log_d("SD Card configuration not found, using defaults");
    }

    // Save portrait_mode to preferences for fallback when SD card fails
    // This ensures correct display orientation even if SD card fails on next boot
    auto& prefs = photo_frame::PreferencesHelper::getInstance();
    if (prefs.setPortraitMode(config.board.portrait_mode)) {
        log_i("Saved portrait_mode to preferences: %s", config.board.portrait_mode ? "true" : "false");
    } else {
        log_w("Failed to save portrait_mode to preferences");
    }

    log_d("Configuration loaded successfully");
    return error_type::None;
}

photo_frame_error_t load_unified_config_with_fallback(sd_card& sdCard,
                                                     const char* config_path,
                                                     unified_config& config) {
    // First try to load from file
    photo_frame_error_t result = load_unified_config(sdCard, config_path, config);

    if (result != error_type::None) {
        log_w("Failed to load config from file, using fallbacks");
        load_fallback_config(config);

        // Return success even if file read failed, since we have fallbacks
        // The caller can check config.is_valid() to see if essential config is missing
        return error_type::None;
    }

    // Validate configuration: at least one image source must be enabled
    if (!config.google_drive.enabled && !config.sd_card.enabled) {
        log_e("Configuration error: No image source enabled! At least one of Google Drive or SD Card must be enabled.");
        return error_type::InvalidConfigNoImageSource;
    }

    return error_type::None;
}

} // namespace photo_frame