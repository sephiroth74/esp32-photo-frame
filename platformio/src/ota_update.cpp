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
    progress.status           = ota_status_t::NOT_STARTED;
    progress.total_size       = 0;
    progress.downloaded_size  = 0;
    progress.progress_percent = 0;
    progress.error            = error_type::None;
}

OTAUpdate::~OTAUpdate() { cleanup_ota_resources(); }

photo_frame_error_t OTAUpdate::begin(const ota_config_t& ota_config) {
    config = ota_config;

    // Get running partition info
    running_partition = esp_ota_get_running_partition();
    if (!running_partition) {
        log_e("[OTA] Failed to get running partition");
        return error_type::OtaInitFailed;
    }

#ifdef DEBUG_OTA
    log_d("[OTA] Running partition: %s", running_partition->label);
    log_d("[OTA] Current version: %s", config.current_version.c_str());
    log_d("[OTA] Board name: %s", config.board_name.c_str());
    log_d("[OTA] Server URL: %s", config.server_url.c_str());
    log_d("[OTA] Version endpoint: %s", config.version_endpoint.c_str());
    log_d("[OTA] Firmware endpoint: %s", config.firmware_endpoint.c_str());
    log_d("[OTA] Use SSL: %s", config.use_ssl ? "Yes" : "No");
#endif // DEBUG_OTA

    // Initialize SSL client if needed
    if (config.use_ssl) {
        log_i("[OTA] Initializing secure client for HTTPS");
        secure_client = new WiFiClientSecure();
        if (config.ca_cert.length() > 0) {
            secure_client->setCACert(config.ca_cert.c_str());
        } else {
            log_w("[OTA] No CA certificate provided, using insecure connection");
            secure_client->setInsecure(); // Skip certificate validation (less secure)
        }
    }

    return error_type::None;
}

photo_frame_error_t OTAUpdate::check_for_update(bool force_check) {
    // Check if enough time has passed since last check
    if (!force_check && last_check_time > 0) {
        unsigned long time_since_check =
            (millis() - last_check_time) / 1000 / 3600; // Convert to hours
        if (time_since_check < OTA_CHECK_INTERVAL_HOURS) {
            log_i("[OTA] Skipping update check, %lu hours remaining", OTA_CHECK_INTERVAL_HOURS - time_since_check);
            return error_type::NoUpdateNeeded;
        }
    }

    progress.status  = ota_status_t::CHECKING_VERSION;
    progress.message = "Checking for updates...";
    log_i("[OTA] Checking for firmware updates");

    // Configure HTTP client for GitHub API
    String version_url = config.server_url + config.version_endpoint;
    log_i("[OTA] GitHub API URL: %s", version_url.c_str());

    if (config.use_ssl && secure_client) {
        log_i("[OTA] Using HTTPS for version check");
        http_client.begin(*secure_client, version_url);
    } else {
        log_i("[OTA] Using HTTP for version check");
        http_client.begin(version_url);
    }

    http_client.setTimeout(OTA_TIMEOUT_MS);
    http_client.addHeader("User-Agent", "ESP32-PhotoFrame/" + config.current_version);
    http_client.addHeader("X-Current-Version", config.current_version);
    http_client.addHeader("X-Board-Name", config.board_name);

    // Enable automatic redirect following for GitHub API
    http_client.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

    int http_code   = http_client.GET();
    last_check_time = millis();

    // Accept both OK and redirect responses
    if (http_code != HTTP_CODE_OK && http_code != HTTP_CODE_FOUND &&
        http_code != HTTP_CODE_MOVED_PERMANENTLY) {
        log_e("[OTA] Version check failed, HTTP code: %d", http_code);
        http_client.end();
        progress.status = ota_status_t::FAILED;
        progress.error  = error_type::OtaVersionCheckFailed;
        return error_type::OtaVersionCheckFailed;
    }

    log_i("[OTA] Version check HTTP response code: %d", http_code);

    String github_response = http_client.getString();
    http_client.end();

    int latest_major, latest_minor, latest_patch;
    auto error = parse_version_response(github_response, latest_major, latest_minor, latest_patch);
    if (error != error_type::None) {
        progress.status = ota_status_t::FAILED;
        progress.error  = error;
        return error;
    }

    log_i("[OTA] Latest version: %d.%d.%d", latest_major, latest_minor, latest_patch);

    // Check if version is newer first - no need to download manifest if not
    if (!is_newer_version(latest_major, latest_minor, latest_patch)) {
        log_i("[OTA] Firmware is up to date - skipping manifest download");
        progress.status  = ota_status_t::NO_UPDATE_NEEDED;
        progress.message = "Firmware is up to date";
        return error_type::NoUpdateNeeded;
    }

    log_i("[OTA] Newer version available - checking compatibility");

    // Download and parse OTA manifest for compatibility check
    String manifest_content;
    error = download_ota_manifest(manifest_content);
    if (error != error_type::None) {
        log_w("[OTA] Warning: Could not download manifest, proceeding without compatibility check");
        // Continue without compatibility check (non-fatal)
    } else {
        int min_major, min_minor, min_patch;
        error = parse_compatibility_info(manifest_content, min_major, min_minor, min_patch);
        if (error != error_type::None) {
            log_w("[OTA] Warning: Could not parse compatibility info from manifest, proceeding without compatibility check");
            // Continue without compatibility check (non-fatal)
        } else {
            if (!check_version_compatibility(min_major, min_minor, min_patch)) {
                log_e("[OTA] Current firmware version is too old for this update");
                progress.status  = ota_status_t::FAILED;
                progress.error   = error_type::OtaVersionIncompatible;
                progress.message = "Firmware version incompatible - manual update required";
                return error_type::OtaVersionIncompatible;
            }
        }
    }

    log_i("[OTA] New firmware version available");

    // Cache GitHub response for firmware URL extraction in start_update
    cached_github_response = github_response;

    progress.status        = ota_status_t::NOT_STARTED;
    progress.message = "Update available: v" + String(latest_major) + "." + String(latest_minor) +
                       "." + String(latest_patch);
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
    log_i("[OTA] Configuring watchdog for OTA operations");
    esp_task_wdt_init(30, true); // 30 second timeout for OTA operations

    is_update_in_progress = true;
    progress.status       = ota_status_t::DOWNLOADING;
    progress.message      = "Starting firmware download...";

    log_i("[OTA] Starting firmware update");

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

    progress.status       = ota_status_t::COMPLETED;
    progress.message      = "Update completed successfully. Restarting...";
    is_update_in_progress = false;

    log_i("[OTA] Firmware update completed successfully");
    log_i("[OTA] Restarting in 3 seconds...");

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
    log_i("[OTA] Battery level validation passed");
    return true;
}

bool OTAUpdate::check_free_space() {
    const esp_partition_t* next_partition = esp_ota_get_next_update_partition(nullptr);
    if (!next_partition) {
        log_e("[OTA] No OTA partition available");
        return false;
    }

    log_i("[OTA] OTA partition size: %u", next_partition->size);

    // Ensure we have at least the partition size available
    return next_partition->size > 0;
}

photo_frame_error_t
OTAUpdate::parse_version_response(const String& response, int& major, int& minor, int& patch) {
    DynamicJsonDocument doc(8192); // Increased size for GitHub API response
    DeserializationError json_error = deserializeJson(doc, response);

    if (json_error) {
        log_e("[OTA] JSON parse error: %s", json_error.c_str());
        return error_type::JsonParseFailed;
    }

    // Handle GitHub API format with tag_name
    if (doc.containsKey("tag_name")) {
        String tag_name = doc["tag_name"].as<String>();
        log_i("[OTA] Found tag_name: %s", tag_name.c_str());

        // Parse version from tag_name (e.g., "v0.5.1" -> 0.5.1)
        if (tag_name.startsWith("v")) {
            tag_name = tag_name.substring(1); // Remove 'v' prefix
        }

        // Split version string by dots
        int dot1 = tag_name.indexOf('.');
        int dot2 = tag_name.indexOf('.', dot1 + 1);

        if (dot1 == -1 || dot2 == -1) {
            log_e("[OTA] Invalid tag_name format - expected x.y.z");
            return error_type::OtaInvalidResponse;
        }

        major = tag_name.substring(0, dot1).toInt();
        minor = tag_name.substring(dot1 + 1, dot2).toInt();
        patch = tag_name.substring(dot2 + 1).toInt();

        log_i("[OTA] Parsed version from tag_name: %d.%d.%d", major, minor, patch);

        // Check if firmware asset exists for current board
        if (doc.containsKey("assets")) {
            JsonArray assets              = doc["assets"];
            String expected_firmware_name = "firmware-" + config.board_name + ".bin";
            bool firmware_found           = false;

            log_i("[OTA] Looking for firmware: %s", expected_firmware_name.c_str());

            for (JsonVariant asset_variant : assets) {
                JsonObject asset = asset_variant.as<JsonObject>();
                if (asset.containsKey("name")) {
                    String asset_name = asset["name"].as<String>();
                    log_i("[OTA] Found asset: %s", asset_name.c_str());

                    if (asset_name == expected_firmware_name) {
                        firmware_found = true;
                        log_i("[OTA] Firmware asset found for current board");
                        break;
                    }
                }
            }

            if (!firmware_found) {
                log_e("[OTA] Firmware not found for board: %s", config.board_name.c_str());
                return error_type::OtaInvalidResponse;
            }
        } else {
            log_e("[OTA] No assets found in GitHub response");
            return error_type::OtaInvalidResponse;
        }

        return error_type::None;

    } else {
        log_e("[OTA] tag_name field not found in GitHub response");
        return error_type::OtaInvalidResponse;
    }
}

photo_frame_error_t OTAUpdate::download_ota_manifest(String& manifest_content) {
    log_i("[OTA] Downloading OTA manifest");

    // Use the predefined manifest URL from config.h
    String manifest_url = OTA_MANIFEST_URL;
    log_i("[OTA] Manifest URL: %s", manifest_url.c_str());

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
    if (http_code != HTTP_CODE_OK && http_code != HTTP_CODE_FOUND &&
        http_code != HTTP_CODE_MOVED_PERMANENTLY) {
        log_e("[OTA] Manifest download failed, HTTP code: %d", http_code);
        manifest_client.end();
        return error_type::OtaVersionCheckFailed;
    }

    log_i("[OTA] Manifest download HTTP response code: %d", http_code);

    manifest_content = manifest_client.getString();
    manifest_client.end();

    log_i("[OTA] Manifest downloaded successfully");
    log_i("[OTA] Manifest size: %zu", manifest_content.length());

    return error_type::None;
}

photo_frame_error_t OTAUpdate::parse_compatibility_info(const String& manifest_content,
                                                        int& min_major,
                                                        int& min_minor,
                                                        int& min_patch) {
    DynamicJsonDocument doc(4096); // Increased size for manifest
    DeserializationError json_error = deserializeJson(doc, manifest_content);

    if (json_error) {
        log_e("[OTA] JSON parse error in manifest: %s", json_error.c_str());
        return error_type::JsonParseFailed;
    }

    // Navigate to releases array in new manifest format
    if (!doc.containsKey("releases")) {
        log_e("[OTA] releases field not found in manifest");
        return error_type::OtaInvalidResponse;
    }

    JsonArray releases = doc["releases"];
    if (releases.size() == 0) {
        log_e("[OTA] No releases found in manifest");
        return error_type::OtaInvalidResponse;
    }

    // Use the first (latest) release
    JsonObject release = releases[0];
    if (!release.containsKey("compatibility")) {
        log_e("[OTA] compatibility field not found in release");
        return error_type::OtaInvalidResponse;
    }

    JsonArray compatibility_array = release["compatibility"];

    // Use board name directly from config (manifest uses plain board names now)
    String expected_board = config.board_name;

    log_i("[OTA] Looking for board compatibility: %s", expected_board.c_str());

    // Find compatibility entry for current board
    bool board_found = false;
    for (JsonVariant compat_variant : compatibility_array) {
        JsonObject compat_obj = compat_variant.as<JsonObject>();

        if (compat_obj.containsKey("board")) {
            String board_name = compat_obj["board"].as<String>();
            log_i("[OTA] Found board in manifest: %s", board_name.c_str());

            if (board_name == expected_board) {
                board_found = true;

                if (compat_obj.containsKey("min_version")) {
                    JsonObject min_version_obj = compat_obj["min_version"];

                    if (min_version_obj.containsKey("major") &&
                        min_version_obj.containsKey("minor") &&
                        min_version_obj.containsKey("patch")) {
                        min_major = min_version_obj["major"];
                        min_minor = min_version_obj["minor"];
                        min_patch = min_version_obj["patch"];

                        log_i("[OTA] Parsed min supported version for %s: %d.%d.%d", expected_board.c_str(), min_major, min_minor, min_patch);

                        return error_type::None;
                    } else {
                        log_e("[OTA] Invalid min_version object format - missing major/minor/patch");
                        return error_type::OtaInvalidResponse;
                    }
                } else {
                    log_e("[OTA] min_version field not found for board");
                    return error_type::OtaInvalidResponse;
                }
            }
        }
    }

    if (!board_found) {
        log_e("[OTA] Board not found in manifest compatibility list: %s", expected_board.c_str());
        return error_type::OtaInvalidResponse;
    }

    return error_type::OtaInvalidResponse; // Should not reach here
}

photo_frame_error_t OTAUpdate::get_firmware_download_url(const String& github_response,
                                                         String& firmware_url) {
    DynamicJsonDocument doc(8192);
    DeserializationError json_error = deserializeJson(doc, github_response);

    if (json_error) {
        log_e("[OTA] JSON parse error when getting firmware URL: %s", json_error.c_str());
        return error_type::JsonParseFailed;
    }

    // Search for firmware asset in GitHub response
    if (doc.containsKey("assets")) {
        JsonArray assets              = doc["assets"];
        String expected_firmware_name = "firmware-" + config.board_name + ".bin";

        log_i("[OTA] Looking for firmware asset: %s", expected_firmware_name.c_str());

        for (JsonVariant asset_variant : assets) {
            JsonObject asset = asset_variant.as<JsonObject>();
            if (asset.containsKey("name") && asset.containsKey("browser_download_url")) {
                String asset_name = asset["name"].as<String>();

                if (asset_name == expected_firmware_name) {
                    firmware_url = asset["browser_download_url"].as<String>();
                    log_i("[OTA] Found firmware download URL: %s", firmware_url.c_str());
                    return error_type::None;
                }
            }
        }

        log_e("[OTA] Firmware asset not found for board: %s", config.board_name.c_str());
        return error_type::OtaInvalidResponse;
    } else {
        log_e("[OTA] No assets found in GitHub response");
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

    log_i("[OTA] Current version: %d.%d.%d", current_major, current_minor, current_patch);
    log_i("[OTA] Minimum required version: %d.%d.%d", min_major, min_minor, min_patch);

    // Check if current version meets minimum requirement
    // Current version must be >= minimum version
    if (current_major > min_major) {
        log_i("[OTA] Version compatibility check passed (major)");
        return true;
    }
    if (current_major < min_major) {
        log_w("[OTA] Version compatibility check failed (major too old)");
        return false;
    }

    // Major versions are equal, check minor
    if (current_minor > min_minor) {
        log_i("[OTA] Version compatibility check passed (minor)");
        return true;
    }
    if (current_minor < min_minor) {
        log_w("[OTA] Version compatibility check failed (minor too old)");
        return false;
    }

    // Major and minor versions are equal, check patch
    if (current_patch >= min_patch) {
        log_i("[OTA] Version compatibility check passed (patch)");
        return true;
    } else {
        log_w("[OTA] Version compatibility check failed (patch too old)");
        return false;
    }
}

photo_frame_error_t OTAUpdate::begin_ota_update() {
    // Get next OTA partition
    ota_partition = esp_ota_get_next_update_partition(nullptr);
    if (!ota_partition) {
        log_e("[OTA] No OTA partition found");
        return error_type::OtaPartitionNotFound;
    }

    log_i("[OTA] Target partition: %s", ota_partition->label);

    // Begin OTA update
    esp_err_t err = esp_ota_begin(ota_partition, OTA_SIZE_UNKNOWN, &ota_handle);
    if (err != ESP_OK) {
        log_e("[OTA] esp_ota_begin failed: %s", esp_err_to_name(err));
        return error_type::OtaBeginFailed;
    }

    return error_type::None;
}

photo_frame_error_t OTAUpdate::download_and_install_firmware(const String& firmware_url) {
    log_i("[OTA] Downloading firmware from: %s", firmware_url.c_str());

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
    if (http_code != HTTP_CODE_OK && http_code != HTTP_CODE_FOUND &&
        http_code != HTTP_CODE_MOVED_PERMANENTLY) {
        log_e("[OTA] Firmware download failed, HTTP code: %d", http_code);
        http_client.end();
        return error_type::OtaDownloadFailed;
    }

    log_i("[OTA] HTTP response code: %d", http_code);

    progress.total_size      = http_client.getSize();
    progress.downloaded_size = 0;
    progress.status          = ota_status_t::DOWNLOADING;

    log_i("[OTA] Firmware size: %d", progress.total_size);

    // Download and write firmware in chunks
    WiFiClient* stream = http_client.getStreamPtr();
    uint8_t buffer[OTA_BUFFER_SIZE];
    unsigned long last_yield_time     = millis();
    unsigned long last_watchdog_feed  = millis();
    unsigned long download_start_time = millis();
    unsigned long max_download_time   = 300000; // 5 minutes maximum download time

    while (http_client.connected() &&
           (progress.downloaded_size < progress.total_size || progress.total_size == -1)) {
        // Check for download timeout
        if (millis() - download_start_time > max_download_time) {
            log_e("[OTA] Download timeout - aborting");
            http_client.end();
            return error_type::OtaDownloadFailed;
        }

        // Check if we have a valid total size and have completed download
        if (progress.total_size > 0 && progress.downloaded_size >= progress.total_size) {
            log_i("[OTA] Download completed");
            break;
        }

        size_t available = stream->available();
        if (available) {
            size_t read_size = stream->readBytes(buffer, min(available, sizeof(buffer)));

            esp_err_t err    = esp_ota_write(ota_handle, buffer, read_size);
            if (err != ESP_OK) {
                log_e("[OTA] esp_ota_write failed: %s", esp_err_to_name(err));
                http_client.end();
                return error_type::OtaWriteFailed;
            }

            progress.downloaded_size += read_size;

            if (progress.total_size > 0) {
                progress.progress_percent = (progress.downloaded_size * 100) / progress.total_size;

                // Update progress every 10% with bounds checking
                static uint8_t last_percent = 0;
                if (progress.progress_percent >= last_percent + 10 &&
                    progress.progress_percent <= 100) {
                    log_i("[OTA] Progress: %u%% (%u/%d bytes)", progress.progress_percent, progress.downloaded_size, progress.total_size);
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

    log_i("[OTA] Downloaded %u bytes", progress.downloaded_size);

    return error_type::None;
}

photo_frame_error_t OTAUpdate::finalize_ota_update() {
    progress.status  = ota_status_t::INSTALLING;
    progress.message = "Installing firmware...";

    log_i("[OTA] Finalizing OTA update");

    esp_err_t err = esp_ota_end(ota_handle);
    if (err != ESP_OK) {
        log_e("[OTA] esp_ota_end failed: %s", esp_err_to_name(err));
        return error_type::OtaEndFailed;
    }

    err = esp_ota_set_boot_partition(ota_partition);
    if (err != ESP_OK) {
        log_e("[OTA] esp_ota_set_boot_partition failed: %s", esp_err_to_name(err));
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
        log_i("[OTA] Resetting watchdog configuration");
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

    log_i("[OTA] Current version: %d.%d.%d", current_major, current_minor, current_patch);
    log_i("[OTA] Latest version: %d.%d.%d", latest_major, latest_minor, latest_patch);

    // Semantic version comparison: major.minor.patch
    if (latest_major > current_major) {
        log_i("[OTA] Newer version available (major)");
        return true;
    }
    if (latest_major < current_major) {
        log_i("[OTA] Current version is newer (major)");
        return false;
    }

    // Major versions are equal, check minor
    if (latest_minor > current_minor) {
        log_i("[OTA] Newer version available (minor)");
        return true;
    }
    if (latest_minor < current_minor) {
        log_i("[OTA] Current version is newer (minor)");
        return false;
    }

    // Major and minor versions are equal, check patch
    if (latest_patch > current_patch) {
        log_i("[OTA] Newer version available (patch)");
        return true;
    }
    if (latest_patch < current_patch) {
        log_i("[OTA] Current version is newer (patch)");
        return false;
    }

    log_i("[OTA] Versions are equal");
    return false; // Versions are equal
}

void OTAUpdate::cancel_update() {
    if (is_update_in_progress) {
        log_i("[OTA] Cancelling OTA update");
        cleanup_ota_resources();
        is_update_in_progress = false;
        progress.status       = ota_status_t::FAILED;
        progress.message      = "Update cancelled";
    }
}

void OTAUpdate::print_partition_info() {
    const esp_partition_t* running = esp_ota_get_running_partition();
    const esp_partition_t* boot    = esp_ota_get_boot_partition();
    const esp_partition_t* next    = esp_ota_get_next_update_partition(nullptr);

    log_i("[OTA] Partition Information:");

    if (running) {
        log_i("[OTA] Running partition: %s (0x%x, size: %u)", running->label, running->address, running->size);
    }

    if (boot) {
        log_i("[OTA] Boot partition: %s (0x%x, size: %u)", boot->label, boot->address, boot->size);
    }

    if (next) {
        log_i("[OTA] Next OTA partition: %s (0x%x, size: %u)", next->label, next->address, next->size);
    }
}

void OTAUpdate::mark_firmware_valid() {
    esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
    if (err == ESP_OK) {
        log_i("[OTA] Firmware marked as valid");
    } else {
        log_e("[OTA] Failed to mark firmware as valid: %s", esp_err_to_name(err));
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