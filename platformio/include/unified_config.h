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

#ifndef __UNIFIED_CONFIG_H__
#define __UNIFIED_CONFIG_H__

#include <Arduino.h>
#include <ArduinoJson.h>
#include "errors.h"
#include "sd_card.h"

namespace photo_frame {

/**
 * @brief Unified configuration structure containing all system settings
 */
struct unified_config {
    // WiFi Configuration (supports up to 3 networks)
    struct wifi_network {
        String ssid;
        String password;

        bool is_valid() const
        {
            return ssid.length() > 0 && password.length() > 0;
        }
    };

    struct wifi_config {
        wifi_network networks[WIFI_MAX_NETWORKS];
        uint8_t network_count = 0;

        bool is_valid() const
        {
            // At least one valid network required
            if (network_count == 0) return false;
            for (uint8_t i = 0; i < network_count; i++) {
                if (!networks[i].is_valid()) return false;
            }
            return true;
        }

        // Helper to add a network
        bool add_network(const String& ssid, const String& password) {
            if (network_count >= WIFI_MAX_NETWORKS) return false;
            networks[network_count].ssid = ssid;
            networks[network_count].password = password;
            network_count++;
            return true;
        }
    } wifi;

    // Board-specific Configuration
    struct board_config {
        struct refresh_config {
            uint32_t min_seconds = 600; // 10 minutes default
            uint32_t max_seconds = 14400; // 4 hours default
            uint32_t step = 300; // 5 minutes default
            uint32_t default_seconds = 1800; // 30 minutes default (used when USE_POTENTIOMETER is false)
            uint8_t low_battery_multiplier = 3; // 3x multiplier default
        } refresh;

        uint8_t day_start_hour = 6; // 6 AM default
        uint8_t day_end_hour = 23; // 11 PM default

        bool is_valid() const
        {
            return day_start_hour < 24 && day_end_hour < 24 &&
                   refresh.min_seconds > 0 &&
                   refresh.max_seconds > refresh.min_seconds &&
                   refresh.default_seconds >= refresh.min_seconds &&
                   refresh.default_seconds <= refresh.max_seconds;
        }
    } board;

    // Google Drive Configuration
    struct google_drive_config {
        struct authentication {
            String service_account_email;
            String private_key_pem;
            String client_id;

            bool is_valid() const
            {
                return service_account_email.length() > 0 && private_key_pem.length() > 0 && client_id.length() > 0;
            }
        } auth;

        struct drive_settings {
            String folder_id;
            String root_ca_path;
            uint16_t list_page_size = 200;
            bool use_insecure_tls = true;

            bool is_valid() const
            {
                return folder_id.length() > 0;
            }
        } drive;

        struct caching_settings {
            String local_path = "/gdrive";
            uint32_t toc_max_age_seconds = 604800; // 7 days
        } caching;

        struct rate_limiting {
            uint16_t max_requests_per_window = 100;
            uint16_t rate_limit_window_seconds = 100;
            uint16_t min_request_delay_ms = 500;
            uint8_t max_retry_attempts = 3;
            uint16_t backoff_base_delay_ms = 5000;
            uint32_t max_wait_time_ms = 30000;
        } rate_limiting;

        bool is_valid() const
        {
            return auth.is_valid() && drive.is_valid();
        }
    } google_drive;

    // Weather Configuration
    struct weather_config {
        bool enabled = false;
        double latitude = 0.0;
        double longitude = 0.0;
        uint16_t update_interval_minutes = 120;
        bool celsius = true;
        uint8_t battery_threshold = 15;
        uint8_t max_age_hours = 3;
        String timezone = "auto";
        String temperature_unit = "celsius";
        String wind_speed_unit = "kmh";
        String precipitation_unit = "mm";

        bool is_valid() const
        {
            return latitude != 0.0 || longitude != 0.0; // At least one coordinate must be set
        }
    } weather;

    /**
     * @brief Check if the entire configuration is valid
     */
    bool is_valid() const
    {
        return wifi.is_valid() && board.is_valid() && google_drive.is_valid() && weather.is_valid();
    }

#ifdef ENABLE_DISPLAY_DIAGNOSTIC
    /**
     * @brief Serializza la configurazione in formato JSON
     */
    String to_json() const
    {
#if ARDUINOJSON_VERSION_MAJOR >= 7
        JsonDocument doc;
#else
        DynamicJsonDocument doc(4096);
#endif

        // Serialize WiFi networks array
        JsonArray wifiArray = doc["wifi"].to<JsonArray>();
        for (uint8_t i = 0; i < wifi.network_count; i++) {
            JsonObject wifiObj = wifiArray.add<JsonObject>();
            wifiObj["ssid"] = wifi.networks[i].ssid;
            wifiObj["password"] = wifi.networks[i].password;
        }

        doc["board_config"]["refresh"]["min_seconds"] = board.refresh.min_seconds;
        doc["board_config"]["refresh"]["max_seconds"] = board.refresh.max_seconds;
        doc["board_config"]["refresh"]["step"] = board.refresh.step;
        doc["board_config"]["refresh"]["low_battery_multiplier"] = board.refresh.low_battery_multiplier;
        doc["board_config"]["day_start_hour"] = board.day_start_hour;
        doc["board_config"]["day_end_hour"] = board.day_end_hour;

        doc["google_drive_config"]["authentication"]["service_account_email"] = google_drive.auth.service_account_email;
        doc["google_drive_config"]["authentication"]["private_key_pem"] = google_drive.auth.private_key_pem;
        doc["google_drive_config"]["authentication"]["client_id"] = google_drive.auth.client_id;

        doc["google_drive_config"]["drive"]["folder_id"] = google_drive.drive.folder_id;
        doc["google_drive_config"]["drive"]["root_ca_path"] = google_drive.drive.root_ca_path;
        doc["google_drive_config"]["drive"]["list_page_size"] = google_drive.drive.list_page_size;
        doc["google_drive_config"]["drive"]["use_insecure_tls"] = google_drive.drive.use_insecure_tls;

        doc["google_drive_config"]["caching"]["local_path"] = google_drive.caching.local_path;
        doc["google_drive_config"]["caching"]["toc_max_age_seconds"] = google_drive.caching.toc_max_age_seconds;

        doc["google_drive_config"]["rate_limiting"]["max_requests_per_window"] = google_drive.rate_limiting.max_requests_per_window;
        doc["google_drive_config"]["rate_limiting"]["rate_limit_window_seconds"] = google_drive.rate_limiting.rate_limit_window_seconds;
        doc["google_drive_config"]["rate_limiting"]["min_request_delay_ms"] = google_drive.rate_limiting.min_request_delay_ms;
        doc["google_drive_config"]["rate_limiting"]["max_retry_attempts"] = google_drive.rate_limiting.max_retry_attempts;
        doc["google_drive_config"]["rate_limiting"]["backoff_base_delay_ms"] = google_drive.rate_limiting.backoff_base_delay_ms;
        doc["google_drive_config"]["rate_limiting"]["max_wait_time_ms"] = google_drive.rate_limiting.max_wait_time_ms;

        doc["weather_config"]["enabled"] = weather.enabled;
        doc["weather_config"]["latitude"] = weather.latitude;
        doc["weather_config"]["longitude"] = weather.longitude;
        doc["weather_config"]["update_interval_minutes"] = weather.update_interval_minutes;
        doc["weather_config"]["celsius"] = weather.celsius;
        doc["weather_config"]["battery_threshold"] = weather.battery_threshold;
        doc["weather_config"]["max_age_hours"] = weather.max_age_hours;
        doc["weather_config"]["timezone"] = weather.timezone;
        doc["weather_config"]["temperature_unit"] = weather.temperature_unit;
        doc["weather_config"]["wind_speed_unit"] = weather.wind_speed_unit;
        doc["weather_config"]["precipitation_unit"] = weather.precipitation_unit;

        String output;
        serializeJson(doc, output);
        return output;
    }
#endif // ENABLE_DISPLAY_DIAGNOSTIC

    /**
     * @brief Get fallback sleep duration when SD card fails
     * Returns midpoint between refresh min and max intervals
     */
    uint32_t get_fallback_sleep_seconds() const
    {
        return (board.refresh.min_seconds + board.refresh.max_seconds) / 2;
    }
};

/**
 * @brief Load fallback configuration values from config.h defines
 *
 * @param config Output configuration structure (populated with defaults)
 */
void load_fallback_config(unified_config& config);

/**
 * @brief Load unified configuration from SD card
 *
 * @param sdCard Reference to SD card interface
 * @param config_path Path to the configuration file (default: CONFIG_FILEPATH)
 * @param config Output configuration structure
 * @return photo_frame_error_t Error code
 */
photo_frame_error_t load_unified_config(sd_card& sdCard,
    const char* config_path,
    unified_config& config);

/**
 * @brief Load unified configuration with fallback values
 *
 * If SD card cannot be read or config file is invalid, populates config with
 * fallback values from config.h defines.
 *
 * @param sdCard Reference to SD card interface
 * @param config_path Path to the configuration file
 * @param config Output configuration structure (populated with defaults on failure)
 * @return photo_frame_error_t Error code (can return success even if file read failed)
 */
photo_frame_error_t load_unified_config_with_fallback(sd_card& sdCard,
    const char* config_path,
    unified_config& config);

} // namespace photo_frame

#endif // __UNIFIED_CONFIG_H__