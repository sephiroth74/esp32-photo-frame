// ESP32 Photo Frame
// Copyright (C) 2025 Alessandro Crugnola
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#ifndef __UNIFIED_CONFIG_H__
#define __UNIFIED_CONFIG_H__

#include "errors.h"
#include "sd_card.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>

namespace photo_frame {

/**
 * @brief Unified configuration structure containing all system settings
 */
struct unified_config {
  // WiFi Configuration (supports up to 3 networks)
  struct wifi_network {
    String ssid;
    String password;

    bool is_valid() const { return ssid.length() > 0 && password.length() > 0; }
  };

  struct wifi_config {
    wifi_network networks[WIFI_MAX_NETWORKS];
    uint8_t network_count = 0;

    bool is_valid() const {
      // At least one valid network required
      if (network_count == 0)
        return false;
      for (uint8_t i = 0; i < network_count; i++) {
        if (!networks[i].is_valid())
          return false;
      }
      return true;
    }

    // Helper to add a network
    bool add_network(const String &ssid, const String &password) {
      if (network_count >= WIFI_MAX_NETWORKS)
        return false;
      networks[network_count].ssid = ssid;
      networks[network_count].password = password;
      network_count++;
      return true;
    }
  } wifi;

  // Board-specific Configuration
  struct board_config {
    struct refresh_config {
      uint32_t min_seconds = 600;         // 10 minutes default
      uint32_t max_seconds = 14400;       // 4 hours default
      uint32_t step = 300;                // 5 minutes default
      uint32_t default_seconds = 1800;    // 30 minutes default (used when USE_POTENTIOMETER is false)
      uint8_t low_battery_multiplier = 3; // 3x multiplier default
    } refresh;

    uint8_t day_start_hour = 6;   // 6 AM default
    uint8_t day_end_hour = 23;    // 11 PM default
    uint8_t display_rotation = 0; // 0-3 (0=0°, 1=90°, 2=180°, 3=270°)

    bool is_valid() const {
      return day_start_hour < 24 && day_end_hour < 24 && refresh.min_seconds > 0 && refresh.max_seconds > refresh.min_seconds &&
             refresh.default_seconds >= refresh.min_seconds && refresh.default_seconds <= refresh.max_seconds &&
             display_rotation < 4;
    }
  } board;

  // Google Drive Configuration
  struct GoogleDrive_config {
    bool enabled = true; // Enable/disable Google Drive image source

    struct authentication {
      String service_account_email;
      String private_key_pem;
      String client_id;

      bool is_valid() const { return service_account_email.length() > 0 && private_key_pem.length() > 0 && client_id.length() > 0; }
    } auth;

    struct drive_settings {
      std::vector<String> folder_ids;
      String root_ca_path;
      uint16_t list_page_size = 200;
      bool use_insecure_tls = true;

      bool is_valid() const { return !folder_ids.empty(); }
    } drive;

    struct caching_settings {
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

    bool is_valid() const {
      // Google Drive is valid if disabled OR if enabled with valid settings
      return !enabled || (auth.is_valid() && drive.is_valid());
    }
  } GoogleDrive;

  // SD Card Configuration
  struct sd_card_config {
    bool enabled = false;                 // Enable/disable SD card as image source
    std::vector<String> directories;      // Directories on SD card containing images
    uint32_t toc_max_age_seconds = 86400; // Max TOC age (24 hours)

    bool is_valid() const {
      // SD card is valid if disabled OR if enabled with valid directories list
      return !enabled || !directories.empty();
    }
  } sd_card;

  /**
   * @brief Check if the entire configuration is valid
   * At least one image source must be enabled (Google Drive or SD Card)
   */
  bool is_valid() const {
    // Basic validation
    if (!wifi.is_valid() || !board.is_valid()) {
      return false;
    }

    // At least one image source must be enabled
    if (!GoogleDrive.enabled && !sd_card.enabled) {
      return false;
    }

    // Validate enabled sources
    if (!GoogleDrive.is_valid() || !sd_card.is_valid()) {
      return false;
    }

    return true;
  }

#ifdef ENABLE_DISPLAY_DIAGNOSTIC
  /**
   * @brief Serializes the configuration in JSON format
   */
  String to_json() const {
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
    doc["board_config"]["display_rotation"] = board.display_rotation;

    doc["google_drive_config"]["authentication"]["service_account_email"] = GoogleDrive.auth.service_account_email;
    doc["google_drive_config"]["authentication"]["private_key_pem"] = GoogleDrive.auth.private_key_pem;
    doc["google_drive_config"]["authentication"]["client_id"] = GoogleDrive.auth.client_id;

    JsonArray folderIds = doc["google_drive_config"]["drive"].createNestedArray("folder_ids");
    for (const auto &folder_id : GoogleDrive.drive.folder_ids) {
      folderIds.add(folder_id);
    }
    doc["google_drive_config"]["drive"]["root_ca_path"] = GoogleDrive.drive.root_ca_path;
    doc["google_drive_config"]["drive"]["list_page_size"] = GoogleDrive.drive.list_page_size;
    doc["google_drive_config"]["drive"]["use_insecure_tls"] = GoogleDrive.drive.use_insecure_tls;

    doc["google_drive_config"]["caching"]["toc_max_age_seconds"] = GoogleDrive.caching.toc_max_age_seconds;

    doc["google_drive_config"]["rate_limiting"]["max_requests_per_window"] = GoogleDrive.rate_limiting.max_requests_per_window;
    doc["google_drive_config"]["rate_limiting"]["rate_limit_window_seconds"] = GoogleDrive.rate_limiting.rate_limit_window_seconds;
    doc["google_drive_config"]["rate_limiting"]["min_request_delay_ms"] = GoogleDrive.rate_limiting.min_request_delay_ms;
    doc["google_drive_config"]["rate_limiting"]["max_retry_attempts"] = GoogleDrive.rate_limiting.max_retry_attempts;
    doc["google_drive_config"]["rate_limiting"]["backoff_base_delay_ms"] = GoogleDrive.rate_limiting.backoff_base_delay_ms;
    doc["google_drive_config"]["rate_limiting"]["max_wait_time_ms"] = GoogleDrive.rate_limiting.max_wait_time_ms;

    String output;
    serializeJson(doc, output);
    return output;
  }
#endif // ENABLE_DISPLAY_DIAGNOSTIC

  /**
   * @brief Get fallback sleep duration when SD card fails
   * Returns midpoint between refresh min and max intervals
   */
  uint32_t get_fallback_sleep_seconds() const { return (board.refresh.min_seconds + board.refresh.max_seconds) / 2; }
};

/**
 * @brief Load fallback configuration values from config.h defines
 *
 * @param config Output configuration structure (populated with defaults)
 */
void load_fallback_config(unified_config &config);

/**
 * @brief Load unified configuration from SD card
 *
 * @param sdCard Reference to SD card interface
 * @param config_path Path to the configuration file (default: CONFIG_FILEPATH)
 * @param config Output configuration structure
 * @return photo_frame_error_t Error code
 */
photo_frame_error_t load_unified_config(SdCard &sdCard, const char *config_path, unified_config &config);

/**
 * @brief Load unified configuration with fallback values
 *
 * If SD card cannot be read or config file is invalid, populates config with
 * fallback values from config.h defines.
 *
 * @param sdCard Reference to SD card interface
 * @param config_path Path to the configuration file
 * @param config Output configuration structure (populated with defaults on
 * failure)
 * @return photo_frame_error_t Error code (can return success even if file read
 * failed)
 */
photo_frame_error_t load_unified_config_with_fallback(SdCard &sdCard, const char *config_path, unified_config &config);

} // namespace photo_frame

#endif // __UNIFIED_CONFIG_H__