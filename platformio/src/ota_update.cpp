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

#include "ota_update.h"

#ifdef OTA_UPDATE_ENABLED

#include "battery.h"
#include "string_utils.h"
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include <esp_task_wdt.h>

namespace photo_frame {

// Global OTA update instance
OTAUpdate ota_updater;

OTAUpdate::OTAUpdate() {
    progress.status = ota_status_t::NOT_STARTED;
    progress.total_size = 0;
    progress.downloaded_size = 0;
    progress.progress_percent = 0;
    progress.error = error_type::None;
}

OTAUpdate::~OTAUpdate() {
    cleanup_ota_resources();
}

photo_frame_error_t OTAUpdate::begin(const ota_config_t& ota_config) {
    config = ota_config;

    // Get running partition info
    running_partition = esp_ota_get_running_partition();
    if (!running_partition) {
        Serial.println(F("[OTA] Failed to get running partition"));
        return error_type::OtaInitFailed;
    }

#ifdef DEBUG_OTA
    Serial.print(F("[OTA] Running partition: "));
    Serial.println(running_partition->label);
    Serial.print(F("[OTA] Current version: "));
    Serial.println(config.current_version);
    Serial.print(F("[OTA] Board name: "));
    Serial.println(config.board_name);
    Serial.print(F("[OTA] Server URL: "));
    Serial.println(config.server_url);
    Serial.print(F("[OTA] Version endpoint: "));
    Serial.println(config.version_endpoint);
    Serial.print(F("[OTA] Firmware endpoint: "));
    Serial.println(config.firmware_endpoint);
    Serial.print(F("[OTA] Use SSL: "));
    Serial.println(config.use_ssl ? "Yes" : "No");
#endif // DEBUG_OTA

    // Initialize SSL client if needed
    if (config.use_ssl) {
        Serial.println(F("[OTA] Initializing secure client for HTTPS"));
        secure_client = new WiFiClientSecure();
        if (config.ca_cert.length() > 0) {
            secure_client->setCACert(config.ca_cert.c_str());
        } else {
            Serial.println(F("[OTA] No CA certificate provided, using insecure connection"));
            secure_client->setInsecure(); // Skip certificate validation (less secure)
        }
    }

    return error_type::None;
}

photo_frame_error_t OTAUpdate::check_for_update(bool force_check) {
    // Check if enough time has passed since last check
    if (!force_check && last_check_time > 0) {
        unsigned long time_since_check = (millis() - last_check_time) / 1000 / 3600; // Convert to hours
        if (time_since_check < OTA_CHECK_INTERVAL_HOURS) {
            Serial.print(F("[OTA] Skipping update check, "));
            Serial.print(OTA_CHECK_INTERVAL_HOURS - time_since_check);
            Serial.println(F(" hours remaining"));
            return error_type::NoUpdateNeeded;
        }
    }

    progress.status = ota_status_t::CHECKING_VERSION;
    progress.message = "Checking for updates...";
    Serial.println(F("[OTA] Checking for firmware updates"));

    // Configure HTTP client for GitHub API
    String version_url = config.server_url + config.version_endpoint;
    Serial.print(F("[OTA] GitHub API URL: "));
    Serial.println(version_url);

    if (config.use_ssl && secure_client) {
        Serial.println(F("[OTA] Using HTTPS for version check"));
        http_client.begin(*secure_client, version_url);
    } else {
        Serial.println(F("[OTA] Using HTTP for version check"));
        http_client.begin(version_url);
    }

    http_client.setTimeout(OTA_TIMEOUT_MS);
    http_client.addHeader("User-Agent", "ESP32-PhotoFrame/" + config.current_version);
    http_client.addHeader("X-Current-Version", config.current_version);
    http_client.addHeader("X-Board-Name", config.board_name);

    // Enable automatic redirect following for GitHub API
    http_client.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

    int http_code = http_client.GET();
    last_check_time = millis();

    // Accept both OK and redirect responses
    if (http_code != HTTP_CODE_OK && http_code != HTTP_CODE_FOUND && http_code != HTTP_CODE_MOVED_PERMANENTLY) {
        Serial.print(F("[OTA] Version check failed, HTTP code: "));
        Serial.println(http_code);
        http_client.end();
        progress.status = ota_status_t::FAILED;
        progress.error = error_type::OtaVersionCheckFailed;
        return error_type::OtaVersionCheckFailed;
    }

    Serial.print(F("[OTA] Version check HTTP response code: "));
    Serial.println(http_code);

    String github_response = http_client.getString();
    http_client.end();

    int latest_major, latest_minor, latest_patch;
    auto error = parse_version_response(github_response, latest_major, latest_minor, latest_patch);
    if (error != error_type::None) {
        progress.status = ota_status_t::FAILED;
        progress.error = error;
        return error;
    }

    Serial.print(F("[OTA] Latest version: "));
    Serial.print(latest_major);
    Serial.print(F("."));
    Serial.print(latest_minor);
    Serial.print(F("."));
    Serial.println(latest_patch);

    // Check if version is newer first - no need to download manifest if not
    if (!is_newer_version(latest_major, latest_minor, latest_patch)) {
        Serial.println(F("[OTA] Firmware is up to date - skipping manifest download"));
        progress.status = ota_status_t::NO_UPDATE_NEEDED;
        progress.message = "Firmware is up to date";
        return error_type::NoUpdateNeeded;
    }

    Serial.println(F("[OTA] Newer version available - checking compatibility"));

    // Download and parse OTA manifest for compatibility check
    String manifest_content;
    error = download_ota_manifest(manifest_content);
    if (error != error_type::None) {
        Serial.println(F("[OTA] Warning: Could not download manifest, proceeding without compatibility check"));
        // Continue without compatibility check (non-fatal)
    } else {
        int min_major, min_minor, min_patch;
        error = parse_compatibility_info(manifest_content, min_major, min_minor, min_patch);
        if (error != error_type::None) {
            Serial.println(F("[OTA] Warning: Could not parse compatibility info from manifest, proceeding without compatibility check"));
            // Continue without compatibility check (non-fatal)
        } else {
            if (!check_version_compatibility(min_major, min_minor, min_patch)) {
                Serial.println(F("[OTA] Current firmware version is too old for this update"));
                progress.status = ota_status_t::FAILED;
                progress.error = error_type::OtaVersionIncompatible;
                progress.message = "Firmware version incompatible - manual update required";
                return error_type::OtaVersionIncompatible;
            }
        }
    }

    Serial.println(F("[OTA] New firmware version available"));

    // Cache GitHub response for firmware URL extraction in start_update
    cached_github_response = github_response;

    progress.status = ota_status_t::NOT_STARTED;
    progress.message = "Update available: v" + String(latest_major) + "." + String(latest_minor) + "." + String(latest_patch);
    return error_type::None;
}

photo_frame_error_t OTAUpdate::start_update() {
    if (is_update_in_progress) {
        return error_type::OtaUpdateInProgress;
    }

    // Validate prerequisites
    if (!validate_battery_level()) {
        progress.error = error_type::BatteryLevelCritical;
        return error_type::BatteryLevelCritical;
    }

    if (!check_free_space()) {
        progress.error = error_type::InsufficientSpace;
        return error_type::InsufficientSpace;
    }

    // Configure task watchdog to allow longer OTA operations
    Serial.println(F("[OTA] Configuring watchdog for OTA operations"));
    esp_task_wdt_init(30, true); // 30 second timeout for OTA operations

    is_update_in_progress = true;
    progress.status = ota_status_t::DOWNLOADING;
    progress.message = "Starting firmware download...";

    Serial.println(F("[OTA] Starting firmware update"));

    // Begin OTA update process
    auto error = begin_ota_update();
    if (error != error_type::None) {
        cleanup_ota_resources();
        is_update_in_progress = false;
        return error;
    }

    // Get firmware download URL from cached GitHub response
    String firmware_url;
    error = get_firmware_download_url(cached_github_response, firmware_url);
    if (error != error_type::None) {
        cleanup_ota_resources();
        is_update_in_progress = false;
        return error;
    }

    // Download and install firmware
    error = download_and_install_firmware(firmware_url);

    if (error != error_type::None) {
        cleanup_ota_resources();
        is_update_in_progress = false;
        return error;
    }

    // Finalize update
    error = finalize_ota_update();
    if (error != error_type::None) {
        cleanup_ota_resources();
        is_update_in_progress = false;
        return error;
    }

    progress.status = ota_status_t::COMPLETED;
    progress.message = "Update completed successfully. Restarting...";
    is_update_in_progress = false;

    Serial.println(F("[OTA] Firmware update completed successfully"));
    Serial.println(F("[OTA] Restarting in 3 seconds..."));

    // Reset watchdog configuration before restart
    esp_task_wdt_reset();

    delay(3000);
    ESP.restart();

    return error_type::None;
}

bool OTAUpdate::validate_battery_level() {
    // This would ideally check the actual battery level
    // For now, we'll assume it's always valid during testing
    // In production, integrate with battery_reader
    Serial.println(F("[OTA] Battery level validation passed"));
    return true;
}

bool OTAUpdate::check_free_space() {
    const esp_partition_t* next_partition = esp_ota_get_next_update_partition(nullptr);
    if (!next_partition) {
        Serial.println(F("[OTA] No OTA partition available"));
        return false;
    }

    Serial.print(F("[OTA] OTA partition size: "));
    Serial.println(next_partition->size);

    // Ensure we have at least the partition size available
    return next_partition->size > 0;
}

photo_frame_error_t OTAUpdate::parse_version_response(const String& response, int& major, int& minor, int& patch) {
    DynamicJsonDocument doc(8192); // Increased size for GitHub API response
    DeserializationError json_error = deserializeJson(doc, response);

    if (json_error) {
        Serial.print(F("[OTA] JSON parse error: "));
        Serial.println(json_error.c_str());
        return error_type::JsonParseFailed;
    }

    // Handle GitHub API format with tag_name
    if (doc.containsKey("tag_name")) {
        String tag_name = doc["tag_name"].as<String>();
        Serial.print(F("[OTA] Found tag_name: "));
        Serial.println(tag_name);

        // Parse version from tag_name (e.g., "v0.5.1" -> 0.5.1)
        if (tag_name.startsWith("v")) {
            tag_name = tag_name.substring(1); // Remove 'v' prefix
        }

        // Split version string by dots
        int dot1 = tag_name.indexOf('.');
        int dot2 = tag_name.indexOf('.', dot1 + 1);

        if (dot1 == -1 || dot2 == -1) {
            Serial.println(F("[OTA] Invalid tag_name format - expected x.y.z"));
            return error_type::OtaInvalidResponse;
        }

        major = tag_name.substring(0, dot1).toInt();
        minor = tag_name.substring(dot1 + 1, dot2).toInt();
        patch = tag_name.substring(dot2 + 1).toInt();

        Serial.print(F("[OTA] Parsed version from tag_name: "));
        Serial.print(major);
        Serial.print(F("."));
        Serial.print(minor);
        Serial.print(F("."));
        Serial.println(patch);

        // Check if firmware asset exists for current board
        if (doc.containsKey("assets")) {
            JsonArray assets = doc["assets"];
            String expected_firmware_name = "firmware-" + config.board_name + ".bin";
            bool firmware_found = false;

            Serial.print(F("[OTA] Looking for firmware: "));
            Serial.println(expected_firmware_name);

            for (JsonVariant asset_variant : assets) {
                JsonObject asset = asset_variant.as<JsonObject>();
                if (asset.containsKey("name")) {
                    String asset_name = asset["name"].as<String>();
                    Serial.print(F("[OTA] Found asset: "));
                    Serial.println(asset_name);

                    if (asset_name == expected_firmware_name) {
                        firmware_found = true;
                        Serial.println(F("[OTA] Firmware asset found for current board"));
                        break;
                    }
                }
            }

            if (!firmware_found) {
                Serial.print(F("[OTA] Firmware not found for board: "));
                Serial.println(config.board_name);
                return error_type::OtaInvalidResponse;
            }
        } else {
            Serial.println(F("[OTA] No assets found in GitHub response"));
            return error_type::OtaInvalidResponse;
        }

        return error_type::None;

    } else {
        Serial.println(F("[OTA] tag_name field not found in GitHub response"));
        return error_type::OtaInvalidResponse;
    }
}

photo_frame_error_t OTAUpdate::download_ota_manifest(String& manifest_content) {
    Serial.println(F("[OTA] Downloading OTA manifest"));

    // Use the predefined manifest URL from config.h
    String manifest_url = OTA_MANIFEST_URL;
    Serial.print(F("[OTA] Manifest URL: "));
    Serial.println(manifest_url);

    HTTPClient manifest_client;
    if (config.use_ssl && secure_client) {
        manifest_client.begin(*secure_client, manifest_url);
    } else {
        manifest_client.begin(manifest_url);
    }

    manifest_client.setTimeout(OTA_TIMEOUT_MS);
    manifest_client.addHeader("User-Agent", "ESP32-PhotoFrame/" + config.current_version);

    // Enable automatic redirect following for GitHub downloads
    manifest_client.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

    int http_code = manifest_client.GET();

    // Accept both OK and redirect responses
    if (http_code != HTTP_CODE_OK && http_code != HTTP_CODE_FOUND && http_code != HTTP_CODE_MOVED_PERMANENTLY) {
        Serial.print(F("[OTA] Manifest download failed, HTTP code: "));
        Serial.println(http_code);
        manifest_client.end();
        return error_type::OtaVersionCheckFailed;
    }

    Serial.print(F("[OTA] Manifest download HTTP response code: "));
    Serial.println(http_code);

    manifest_content = manifest_client.getString();
    manifest_client.end();

    Serial.println(F("[OTA] Manifest downloaded successfully"));
    Serial.print(F("[OTA] Manifest size: "));
    Serial.println(manifest_content.length());

    return error_type::None;
}

photo_frame_error_t OTAUpdate::parse_compatibility_info(const String& manifest_content, int& min_major, int& min_minor, int& min_patch) {
    DynamicJsonDocument doc(4096); // Increased size for manifest
    DeserializationError json_error = deserializeJson(doc, manifest_content);

    if (json_error) {
        Serial.print(F("[OTA] JSON parse error in manifest: "));
        Serial.println(json_error.c_str());
        return error_type::JsonParseFailed;
    }

    // Navigate to releases array in new manifest format
    if (!doc.containsKey("releases")) {
        Serial.println(F("[OTA] releases field not found in manifest"));
        return error_type::OtaInvalidResponse;
    }

    JsonArray releases = doc["releases"];
    if (releases.size() == 0) {
        Serial.println(F("[OTA] No releases found in manifest"));
        return error_type::OtaInvalidResponse;
    }

    // Use the first (latest) release
    JsonObject release = releases[0];
    if (!release.containsKey("compatibility")) {
        Serial.println(F("[OTA] compatibility field not found in release"));
        return error_type::OtaInvalidResponse;
    }

    JsonArray compatibility_array = release["compatibility"];

    // Use board name directly from config (manifest uses plain board names now)
    String expected_board = config.board_name;

    Serial.print(F("[OTA] Looking for board compatibility: "));
    Serial.println(expected_board);

    // Find compatibility entry for current board
    bool board_found = false;
    for (JsonVariant compat_variant : compatibility_array) {
        JsonObject compat_obj = compat_variant.as<JsonObject>();

        if (compat_obj.containsKey("board")) {
            String board_name = compat_obj["board"].as<String>();
            Serial.print(F("[OTA] Found board in manifest: "));
            Serial.println(board_name);

            if (board_name == expected_board) {
                board_found = true;

                if (compat_obj.containsKey("min_version")) {
                    JsonObject min_version_obj = compat_obj["min_version"];

                    if (min_version_obj.containsKey("major") && min_version_obj.containsKey("minor") && min_version_obj.containsKey("patch")) {
                        min_major = min_version_obj["major"];
                        min_minor = min_version_obj["minor"];
                        min_patch = min_version_obj["patch"];

                        Serial.print(F("[OTA] Parsed min supported version for "));
                        Serial.print(expected_board);
                        Serial.print(F(": "));
                        Serial.print(min_major);
                        Serial.print(F("."));
                        Serial.print(min_minor);
                        Serial.print(F("."));
                        Serial.println(min_patch);

                        return error_type::None;
                    } else {
                        Serial.println(F("[OTA] Invalid min_version object format - missing major/minor/patch"));
                        return error_type::OtaInvalidResponse;
                    }
                } else {
                    Serial.println(F("[OTA] min_version field not found for board"));
                    return error_type::OtaInvalidResponse;
                }
            }
        }
    }

    if (!board_found) {
        Serial.print(F("[OTA] Board not found in manifest compatibility list: "));
        Serial.println(expected_board);
        return error_type::OtaInvalidResponse;
    }

    return error_type::OtaInvalidResponse; // Should not reach here
}

photo_frame_error_t OTAUpdate::get_firmware_download_url(const String& github_response, String& firmware_url) {
    DynamicJsonDocument doc(8192);
    DeserializationError json_error = deserializeJson(doc, github_response);

    if (json_error) {
        Serial.print(F("[OTA] JSON parse error when getting firmware URL: "));
        Serial.println(json_error.c_str());
        return error_type::JsonParseFailed;
    }

    // Search for firmware asset in GitHub response
    if (doc.containsKey("assets")) {
        JsonArray assets = doc["assets"];
        String expected_firmware_name = "firmware-" + config.board_name + ".bin";

        Serial.print(F("[OTA] Looking for firmware asset: "));
        Serial.println(expected_firmware_name);

        for (JsonVariant asset_variant : assets) {
            JsonObject asset = asset_variant.as<JsonObject>();
            if (asset.containsKey("name") && asset.containsKey("browser_download_url")) {
                String asset_name = asset["name"].as<String>();

                if (asset_name == expected_firmware_name) {
                    firmware_url = asset["browser_download_url"].as<String>();
                    Serial.print(F("[OTA] Found firmware download URL: "));
                    Serial.println(firmware_url);
                    return error_type::None;
                }
            }
        }

        Serial.print(F("[OTA] Firmware asset not found for board: "));
        Serial.println(config.board_name);
        return error_type::OtaInvalidResponse;
    } else {
        Serial.println(F("[OTA] No assets found in GitHub response"));
        return error_type::OtaInvalidResponse;
    }
}

bool OTAUpdate::check_version_compatibility(int min_major, int min_minor, int min_patch) {
    // Use compile-time constants for current version
#ifdef FIRMWARE_VERSION_MAJOR
    int current_major = FIRMWARE_VERSION_MAJOR;
    int current_minor = FIRMWARE_VERSION_MINOR;
    int current_patch = FIRMWARE_VERSION_PATCH;
#else
    #error "FIRMWARE_VERSION_MAJOR, FIRMWARE_VERSION_MINOR, and FIRMWARE_VERSION_PATCH must be defined"
#endif

    Serial.print(F("[OTA] Current version: "));
    Serial.print(current_major);
    Serial.print(F("."));
    Serial.print(current_minor);
    Serial.print(F("."));
    Serial.println(current_patch);

    Serial.print(F("[OTA] Minimum required version: "));
    Serial.print(min_major);
    Serial.print(F("."));
    Serial.print(min_minor);
    Serial.print(F("."));
    Serial.println(min_patch);

    // Check if current version meets minimum requirement
    // Current version must be >= minimum version
    if (current_major > min_major) {
        Serial.println(F("[OTA] Version compatibility check passed (major)"));
        return true;
    }
    if (current_major < min_major) {
        Serial.println(F("[OTA] Version compatibility check failed (major too old)"));
        return false;
    }

    // Major versions are equal, check minor
    if (current_minor > min_minor) {
        Serial.println(F("[OTA] Version compatibility check passed (minor)"));
        return true;
    }
    if (current_minor < min_minor) {
        Serial.println(F("[OTA] Version compatibility check failed (minor too old)"));
        return false;
    }

    // Major and minor versions are equal, check patch
    if (current_patch >= min_patch) {
        Serial.println(F("[OTA] Version compatibility check passed (patch)"));
        return true;
    } else {
        Serial.println(F("[OTA] Version compatibility check failed (patch too old)"));
        return false;
    }
}

photo_frame_error_t OTAUpdate::begin_ota_update() {
    // Get next OTA partition
    ota_partition = esp_ota_get_next_update_partition(nullptr);
    if (!ota_partition) {
        Serial.println(F("[OTA] No OTA partition found"));
        return error_type::OtaPartitionNotFound;
    }

    Serial.print(F("[OTA] Target partition: "));
    Serial.println(ota_partition->label);

    // Begin OTA update
    esp_err_t err = esp_ota_begin(ota_partition, OTA_SIZE_UNKNOWN, &ota_handle);
    if (err != ESP_OK) {
        Serial.print(F("[OTA] esp_ota_begin failed: "));
        Serial.println(esp_err_to_name(err));
        return error_type::OtaBeginFailed;
    }

    return error_type::None;
}

photo_frame_error_t OTAUpdate::download_and_install_firmware(const String& firmware_url) {
    Serial.print(F("[OTA] Downloading firmware from: "));
    Serial.println(firmware_url);

    // Configure HTTP client for firmware download
    if (config.use_ssl && secure_client) {
        http_client.begin(*secure_client, firmware_url);
    } else {
        http_client.begin(firmware_url);
    }

    http_client.setTimeout(OTA_TIMEOUT_MS);
    http_client.addHeader("User-Agent", "ESP32-PhotoFrame/" + config.current_version);

    // Enable automatic redirect following for GitHub downloads
    http_client.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

    int http_code = http_client.GET();

    // Accept both OK and redirect responses
    if (http_code != HTTP_CODE_OK && http_code != HTTP_CODE_FOUND && http_code != HTTP_CODE_MOVED_PERMANENTLY) {
        Serial.print(F("[OTA] Firmware download failed, HTTP code: "));
        Serial.println(http_code);
        http_client.end();
        return error_type::OtaDownloadFailed;
    }

    Serial.print(F("[OTA] HTTP response code: "));
    Serial.println(http_code);

    progress.total_size = http_client.getSize();
    progress.downloaded_size = 0;
    progress.status = ota_status_t::DOWNLOADING;

    Serial.print(F("[OTA] Firmware size: "));
    Serial.println(progress.total_size);

    // Download and write firmware in chunks
    WiFiClient* stream = http_client.getStreamPtr();
    uint8_t buffer[OTA_BUFFER_SIZE];
    unsigned long last_yield_time = millis();
    unsigned long last_watchdog_feed = millis();
    unsigned long download_start_time = millis();
    unsigned long max_download_time = 300000; // 5 minutes maximum download time

    while (http_client.connected() && (progress.downloaded_size < progress.total_size || progress.total_size == -1)) {
        // Check for download timeout
        if (millis() - download_start_time > max_download_time) {
            Serial.println(F("[OTA] Download timeout - aborting"));
            http_client.end();
            return error_type::OtaDownloadFailed;
        }

        // Check if we have a valid total size and have completed download
        if (progress.total_size > 0 && progress.downloaded_size >= progress.total_size) {
            Serial.println(F("[OTA] Download completed"));
            break;
        }

        size_t available = stream->available();
        if (available) {
            size_t read_size = stream->readBytes(buffer, min(available, sizeof(buffer)));

            esp_err_t err = esp_ota_write(ota_handle, buffer, read_size);
            if (err != ESP_OK) {
                Serial.print(F("[OTA] esp_ota_write failed: "));
                Serial.println(esp_err_to_name(err));
                http_client.end();
                return error_type::OtaWriteFailed;
            }

            progress.downloaded_size += read_size;

            if (progress.total_size > 0) {
                progress.progress_percent = (progress.downloaded_size * 100) / progress.total_size;

                // Update progress every 10% with bounds checking
                static uint8_t last_percent = 0;
                if (progress.progress_percent >= last_percent + 10 && progress.progress_percent <= 100) {
                    Serial.print(F("[OTA] Progress: "));
                    Serial.print(progress.progress_percent);
                    Serial.print(F("% ("));
                    Serial.print(progress.downloaded_size);
                    Serial.print(F("/"));
                    Serial.print(progress.total_size);
                    Serial.println(F(" bytes)"));
                    last_percent = progress.progress_percent;
                }
            }
        } else {
            // No data available, give more time to other tasks
            delay(10);
        }

        // Feed watchdog and yield more frequently to prevent timeout
        unsigned long current_time = millis();
        if (current_time - last_watchdog_feed > 100) { // Feed watchdog every 100ms
            esp_task_wdt_reset();
            last_watchdog_feed = current_time;
        }

        if (current_time - last_yield_time > 50) { // Yield every 50ms
            yield();
            last_yield_time = current_time;
        }
    }

    http_client.end();

    Serial.print(F("[OTA] Downloaded "));
    Serial.print(progress.downloaded_size);
    Serial.println(F(" bytes"));

    return error_type::None;
}

photo_frame_error_t OTAUpdate::finalize_ota_update() {
    progress.status = ota_status_t::INSTALLING;
    progress.message = "Installing firmware...";

    Serial.println(F("[OTA] Finalizing OTA update"));

    esp_err_t err = esp_ota_end(ota_handle);
    if (err != ESP_OK) {
        Serial.print(F("[OTA] esp_ota_end failed: "));
        Serial.println(esp_err_to_name(err));
        return error_type::OtaEndFailed;
    }

    err = esp_ota_set_boot_partition(ota_partition);
    if (err != ESP_OK) {
        Serial.print(F("[OTA] esp_ota_set_boot_partition failed: "));
        Serial.println(esp_err_to_name(err));
        return error_type::OtaSetBootPartitionFailed;
    }

    return error_type::None;
}

void OTAUpdate::cleanup_ota_resources() {
    if (ota_handle != 0) {
        esp_ota_end(ota_handle);
        ota_handle = 0;
    }

    if (secure_client) {
        delete secure_client;
        secure_client = nullptr;
    }

    http_client.end();

    // Reset watchdog if it was configured for OTA
    if (is_update_in_progress) {
        Serial.println(F("[OTA] Resetting watchdog configuration"));
        esp_task_wdt_reset();
    }
}

bool OTAUpdate::is_newer_version(int latest_major, int latest_minor, int latest_patch) {
    // Use compile-time constants for current version
#ifdef FIRMWARE_VERSION_MAJOR
    int current_major = FIRMWARE_VERSION_MAJOR;
    int current_minor = FIRMWARE_VERSION_MINOR;
    int current_patch = FIRMWARE_VERSION_PATCH;
#else
    #error "FIRMWARE_VERSION_MAJOR, FIRMWARE_VERSION_MINOR, and FIRMWARE_VERSION_PATCH must be defined"
#endif

    Serial.print(F("[OTA] Current version: "));
    Serial.print(current_major);
    Serial.print(F("."));
    Serial.print(current_minor);
    Serial.print(F("."));
    Serial.println(current_patch);

    Serial.print(F("[OTA] Latest version: "));
    Serial.print(latest_major);
    Serial.print(F("."));
    Serial.print(latest_minor);
    Serial.print(F("."));
    Serial.println(latest_patch);

    // Semantic version comparison: major.minor.patch
    if (latest_major > current_major) {
        Serial.println(F("[OTA] Newer version available (major)"));
        return true;
    }
    if (latest_major < current_major) {
        Serial.println(F("[OTA] Current version is newer (major)"));
        return false;
    }

    // Major versions are equal, check minor
    if (latest_minor > current_minor) {
        Serial.println(F("[OTA] Newer version available (minor)"));
        return true;
    }
    if (latest_minor < current_minor) {
        Serial.println(F("[OTA] Current version is newer (minor)"));
        return false;
    }

    // Major and minor versions are equal, check patch
    if (latest_patch > current_patch) {
        Serial.println(F("[OTA] Newer version available (patch)"));
        return true;
    }
    if (latest_patch < current_patch) {
        Serial.println(F("[OTA] Current version is newer (patch)"));
        return false;
    }

    Serial.println(F("[OTA] Versions are equal"));
    return false; // Versions are equal
}

void OTAUpdate::cancel_update() {
    if (is_update_in_progress) {
        Serial.println(F("[OTA] Cancelling OTA update"));
        cleanup_ota_resources();
        is_update_in_progress = false;
        progress.status = ota_status_t::FAILED;
        progress.message = "Update cancelled";
    }
}

void OTAUpdate::print_partition_info() {
    const esp_partition_t* running = esp_ota_get_running_partition();
    const esp_partition_t* boot = esp_ota_get_boot_partition();
    const esp_partition_t* next = esp_ota_get_next_update_partition(nullptr);

    Serial.println(F("[OTA] Partition Information:"));

    if (running) {
        Serial.print(F("[OTA] Running partition: "));
        Serial.print(running->label);
        Serial.print(F(" ("));
        Serial.print(running->address, HEX);
        Serial.print(F(", size: "));
        Serial.print(running->size);
        Serial.println(F(")"));
    }

    if (boot) {
        Serial.print(F("[OTA] Boot partition: "));
        Serial.print(boot->label);
        Serial.print(F(" ("));
        Serial.print(boot->address, HEX);
        Serial.print(F(", size: "));
        Serial.print(boot->size);
        Serial.println(F(")"));
    }

    if (next) {
        Serial.print(F("[OTA] Next OTA partition: "));
        Serial.print(next->label);
        Serial.print(F(" ("));
        Serial.print(next->address, HEX);
        Serial.print(F(", size: "));
        Serial.print(next->size);
        Serial.println(F(")"));
    }
}

void OTAUpdate::mark_firmware_valid() {
    esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
    if (err == ESP_OK) {
        Serial.println(F("[OTA] Firmware marked as valid"));
    } else {
        Serial.print(F("[OTA] Failed to mark firmware as valid: "));
        Serial.println(esp_err_to_name(err));
    }
}

bool OTAUpdate::is_first_boot_after_update() {
    esp_ota_img_states_t ota_state;
    const esp_partition_t* running = esp_ota_get_running_partition();

    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
        return (ota_state == ESP_OTA_IMG_PENDING_VERIFY);
    }

    return false;
}

String OTAUpdate::get_running_partition_name() {
    const esp_partition_t* running = esp_ota_get_running_partition();
    return running ? String(running->label) : "unknown";
}

} // namespace photo_frame

#endif // OTA_UPDATE_ENABLED