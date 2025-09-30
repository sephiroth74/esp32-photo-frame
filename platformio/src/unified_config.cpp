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
#include <ArduinoJson.h>

namespace photo_frame {

/**
 * @brief Load fallback configuration values from config.h defines
 */
void load_fallback_config(unified_config& config) {
    Serial.println(F("[unified_config] Loading fallback configuration from config.h"));

    // WiFi - no fallback, must be provided
    config.wifi.ssid = "";
    config.wifi.password = "";

    // Board configuration fallbacks
    config.board.refresh.min_seconds = REFRESH_MIN_INTERVAL_SECONDS;
    config.board.refresh.max_seconds = REFRESH_MAX_INTERVAL_SECONDS;
    config.board.refresh.step = REFRESH_STEP_SECONDS;
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

    // Weather - fallback to disabled
    config.weather.enabled = false;
    config.weather.latitude = 0.0;
    config.weather.longitude = 0.0;
    config.weather.update_interval_minutes = 120;
    config.weather.celsius = true;
    config.weather.battery_threshold = 15;
    config.weather.max_age_hours = 3;
    config.weather.timezone = "auto";
    config.weather.temperature_unit = "celsius";
    config.weather.wind_speed_unit = "kmh";
    config.weather.precipitation_unit = "mm";
}

photo_frame_error_t load_unified_config(sd_card& sdCard,
                                       const char* config_path,
                                       unified_config& config) {
    Serial.print(F("[unified_config] Loading configuration from: "));
    Serial.println(config_path);

    if (!sdCard.is_initialized()) {
        Serial.println(F("[unified_config] SD card not initialized"));
        return error_type::CardMountFailed;
    }

    if (!sdCard.file_exists(config_path)) {
        Serial.println(F("[unified_config] Configuration file not found"));
        return error_type::SdCardFileNotFound;
    }

    // Open configuration file
    fs::File config_file = sdCard.open(config_path, FILE_READ);
    if (!config_file) {
        Serial.println(F("[unified_config] Failed to open configuration file"));
        return error_type::CardOpenFileFailed;
    }

    // Read file size
    size_t file_size = config_file.size();
    if (file_size == 0) {
        Serial.println(F("[unified_config] Configuration file is empty"));
        config_file.close();
        return error_type::SdCardFileNotFound; // Treat empty file as not found
    }

    Serial.print(F("[unified_config] Configuration file size: "));
    Serial.print(file_size);
    Serial.println(F(" bytes"));

    // Allocate buffer for JSON parsing using PSRAM
    size_t buffer_size = file_size + 512; // Extra space for JSON parsing
    char* buffer = (char*)heap_caps_malloc(buffer_size, MALLOC_CAP_SPIRAM);
    if (!buffer) {
        buffer = (char*)malloc(buffer_size);
    }

    if (!buffer) {
        Serial.println(F("[unified_config] Failed to allocate buffer for config"));
        config_file.close();
        return error_type::CardOpenFileFailed; // Use available error type
    }

    // Read file content
    size_t bytes_read = config_file.readBytes(buffer, file_size);
    config_file.close();

    if (bytes_read != file_size) {
        Serial.println(F("[unified_config] Failed to read complete configuration file"));
        free(buffer);
        return error_type::CardOpenFileFailed; // Use available error type
    }

    buffer[bytes_read] = '\0'; // Null terminate

    // Parse JSON
    DynamicJsonDocument doc(buffer_size);
    DeserializationError json_error = deserializeJson(doc, buffer);
    free(buffer);

    if (json_error) {
        Serial.print(F("[unified_config] JSON parsing failed: "));
        Serial.println(json_error.c_str());
        return error_type::JsonParseFailed;
    }

    // Extract WiFi configuration
    if (doc.containsKey("wifi")) {
        JsonObject wifi_obj = doc["wifi"];
        config.wifi.ssid = wifi_obj["ssid"].as<String>();
        config.wifi.password = wifi_obj["password"].as<String>();
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

            Serial.print(F("[unified_config] Refresh config - min: "));
            Serial.print(config.board.refresh.min_seconds);
            Serial.print(F("s, max: "));
            Serial.print(config.board.refresh.max_seconds);
            Serial.print(F("s, step: "));
            Serial.print(config.board.refresh.step);
            Serial.print(F("s, multiplier: "));
            Serial.println(config.board.refresh.low_battery_multiplier);
        }

        // Load and validate day hours (0-23)
        uint8_t start_hour = board_obj["day_start_hour"] | DAY_START_HOUR;
        uint8_t end_hour = board_obj["day_end_hour"] | DAY_END_HOUR;

        config.board.day_start_hour = min(start_hour, (uint8_t)23);
        config.board.day_end_hour = min(end_hour, (uint8_t)23);

        Serial.print(F("[unified_config] Day schedule - start: "));
        Serial.print(config.board.day_start_hour);
        Serial.print(F("h, end: "));
        Serial.print(config.board.day_end_hour);
        Serial.println(F("h"));
    }

    // Extract Google Drive configuration
    if (doc.containsKey("google_drive_config")) {
        JsonObject gd_obj = doc["google_drive_config"];

        if (gd_obj.containsKey("authentication")) {
            JsonObject auth_obj = gd_obj["authentication"];
            config.google_drive.auth.service_account_email = auth_obj["service_account_email"].as<String>();
            config.google_drive.auth.private_key_pem = auth_obj["private_key_pem"].as<String>();
            config.google_drive.auth.client_id = auth_obj["client_id"].as<String>();
        }

        if (gd_obj.containsKey("drive")) {
            JsonObject drive_obj = gd_obj["drive"];
            config.google_drive.drive.folder_id = drive_obj["folder_id"].as<String>();
            config.google_drive.drive.root_ca_path = drive_obj["root_ca_path"].as<String>();
            config.google_drive.drive.list_page_size = min(drive_obj["list_page_size"] | GOOGLE_DRIVE_MAX_LIST_PAGE_SIZE, GOOGLE_DRIVE_MAX_LIST_PAGE_SIZE);
            config.google_drive.drive.use_insecure_tls = drive_obj["use_insecure_tls"] | true;
        }

        if (gd_obj.containsKey("caching")) {
            JsonObject cache_obj = gd_obj["caching"];
            config.google_drive.caching.local_path = cache_obj["local_path"] | "/gdrive";
            config.google_drive.caching.toc_max_age_seconds = cache_obj["toc_max_age_seconds"] | GOOGLE_DRIVE_TOC_MAX_AGE_SECONDS;
        }

        if (gd_obj.containsKey("rate_limiting")) {
            JsonObject rate_obj = gd_obj["rate_limiting"];
            config.google_drive.rate_limiting.max_requests_per_window = rate_obj["max_requests_per_window"] | GOOGLE_DRIVE_MAX_REQUESTS_PER_WINDOW;
            config.google_drive.rate_limiting.rate_limit_window_seconds = rate_obj["rate_limit_window_seconds"] | GOOGLE_DRIVE_RATE_LIMIT_WINDOW_SECONDS;
            config.google_drive.rate_limiting.min_request_delay_ms = rate_obj["min_request_delay_ms"] | GOOGLE_DRIVE_MIN_REQUEST_DELAY_MS;
            config.google_drive.rate_limiting.max_retry_attempts = rate_obj["max_retry_attempts"] | GOOGLE_DRIVE_MAX_RETRY_ATTEMPTS;
            config.google_drive.rate_limiting.backoff_base_delay_ms = rate_obj["backoff_base_delay_ms"] | GOOGLE_DRIVE_BACKOFF_BASE_DELAY_MS;
            config.google_drive.rate_limiting.max_wait_time_ms = rate_obj["max_wait_time_ms"] | GOOGLE_DRIVE_MAX_WAIT_TIME_MS;
        }
    }

    // Extract weather configuration
    if (doc.containsKey("weather_config")) {
        JsonObject weather_obj = doc["weather_config"];
        config.weather.enabled = weather_obj["enabled"] | false;
        config.weather.latitude = weather_obj["latitude"] | 0.0;
        config.weather.longitude = weather_obj["longitude"] | 0.0;
        config.weather.update_interval_minutes = weather_obj["update_interval_minutes"] | 120;
        config.weather.celsius = weather_obj["celsius"] | true;
        config.weather.battery_threshold = weather_obj["battery_threshold"] | 15;
        config.weather.max_age_hours = weather_obj["max_age_hours"] | 3;
        config.weather.timezone = weather_obj["timezone"] | "auto";
        config.weather.temperature_unit = weather_obj["temperature_unit"] | "celsius";
        config.weather.wind_speed_unit = weather_obj["wind_speed_unit"] | "kmh";
        config.weather.precipitation_unit = weather_obj["precipitation_unit"] | "mm";
    }

    Serial.println(F("[unified_config] Configuration loaded successfully"));
    return error_type::None;
}

photo_frame_error_t load_unified_config_with_fallback(sd_card& sdCard,
                                                     const char* config_path,
                                                     unified_config& config) {
    // First try to load from file
    photo_frame_error_t result = load_unified_config(sdCard, config_path, config);

    if (result != error_type::None) {
        Serial.println(F("[unified_config] Failed to load config from file, using fallbacks"));
        load_fallback_config(config);

        // Return success even if file read failed, since we have fallbacks
        // The caller can check config.is_valid() to see if essential config is missing
        return error_type::None;
    }

    return error_type::None;
}

} // namespace photo_frame