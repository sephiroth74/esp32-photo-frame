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

#ifndef OTA_UPDATE_H
#define OTA_UPDATE_H

#include "config.h"

#ifdef OTA_UPDATE_ENABLED

#include "errors.h"
#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>

namespace photo_frame {

/**
 * @brief OTA Update Manager for ESP32 Photo Frame
 *
 * Provides secure OTA firmware updates with:
 * - Version checking and validation
 * - Progressive download with progress tracking
 * - Automatic rollback on failure
 * - Secure signature verification (optional)
 * - Battery level validation before updates
 *
 * @version v0.5.0
 * @date 2025-01-22
 */

// OTA configuration constants are defined in config.h

// OTA Server Configuration
struct ota_config_t {
    String server_url;        // OTA server base URL (e.g., "https://updates.example.com")
    String version_endpoint;  // Version check endpoint (e.g., "/version")
    String firmware_endpoint; // Firmware download endpoint (e.g., "/firmware")
    String board_name;        // Board identifier for updates
    String current_version;   // Current firmware version
    bool use_ssl;             // Use HTTPS for secure updates
    String ca_cert;           // CA certificate for SSL verification (optional)
};

// OTA Update Status
enum class ota_status_t {
    NOT_STARTED,      // OTA not started
    CHECKING_VERSION, // Checking for new version
    DOWNLOADING,      // Downloading firmware
    INSTALLING,       // Installing firmware
    COMPLETED,        // Update completed successfully
    FAILED,           // Update failed
    ROLLBACK,         // Rolling back due to failure
    NO_UPDATE_NEEDED  // No update available
};

// OTA Progress Information
struct ota_progress_t {
    ota_status_t status;
    uint32_t total_size;
    uint32_t downloaded_size;
    uint8_t progress_percent;
    String message;
    photo_frame_error_t error;
};

/**
 * @brief OTA Update Manager Class
 */
class OTAUpdate {
  private:
    ota_config_t config;
    ota_progress_t progress;

    esp_ota_handle_t ota_handle              = 0;
    const esp_partition_t* ota_partition     = nullptr;
    const esp_partition_t* running_partition = nullptr;

    HTTPClient http_client;
    WiFiClientSecure* secure_client = nullptr;

    bool is_update_in_progress      = false;
    unsigned long last_check_time   = 0;
    String cached_github_response; // Store GitHub response for firmware URL extraction

    // Internal methods
    bool validate_battery_level();
    bool check_free_space();
    photo_frame_error_t
    parse_version_response(const String& response, int& major, int& minor, int& patch);
    photo_frame_error_t download_ota_manifest(String& manifest_content);
    photo_frame_error_t parse_compatibility_info(const String& manifest_content,
                                                 int& min_major,
                                                 int& min_minor,
                                                 int& min_patch);
    photo_frame_error_t get_firmware_download_url(const String& github_response,
                                                  String& firmware_url);
    bool check_version_compatibility(int min_major, int min_minor, int min_patch);
    photo_frame_error_t begin_ota_update();
    photo_frame_error_t download_and_install_firmware(const String& firmware_url);
    photo_frame_error_t finalize_ota_update();
    void cleanup_ota_resources();
    bool is_newer_version(int latest_major, int latest_minor, int latest_patch);

  public:
    OTAUpdate();
    ~OTAUpdate();

    /**
     * @brief Initialize OTA update system
     * @param config OTA configuration
     * @return Error state
     */
    photo_frame_error_t begin(const ota_config_t& ota_config);

    /**
     * @brief Check if update is needed and available
     * @param force_check Force version check even if recently checked
     * @return Error state (None if update available, NoUpdateNeeded if up to date)
     */
    photo_frame_error_t check_for_update(bool force_check = false);

    /**
     * @brief Start OTA update process
     * @return Error state
     */
    photo_frame_error_t start_update();

    /**
     * @brief Get current OTA progress
     * @return Progress information
     */
    const ota_progress_t& get_progress() const { return progress; }

    /**
     * @brief Check if OTA update is in progress
     * @return True if update is active
     */
    bool is_update_active() const { return is_update_in_progress; }

    /**
     * @brief Cancel ongoing OTA update
     */
    void cancel_update();

    /**
     * @brief Get partition information for diagnostics
     */
    void print_partition_info();

    /**
     * @brief Validate current firmware and mark as good
     * Call this after successful boot to prevent rollback
     */
    void mark_firmware_valid();

    /**
     * @brief Check if this is first boot after OTA update
     */
    bool is_first_boot_after_update();

    /**
     * @brief Get running partition name for diagnostics
     */
    String get_running_partition_name();
};

// Global OTA update instance
extern OTAUpdate ota_updater;

// Convenience macros for OTA operations
#define OTA_CHECK_UPDATE() ota_updater.check_for_update()
#define OTA_START_UPDATE() ota_updater.start_update()
#define OTA_GET_PROGRESS() ota_updater.get_progress()
#define OTA_IS_ACTIVE()    ota_updater.is_update_active()
#define OTA_CANCEL()       ota_updater.cancel_update()

} // namespace photo_frame

#else // OTA_UPDATE_ENABLED not defined

// OTA system disabled - provide stub definitions
namespace photo_frame {
// Empty namespace when OTA is disabled
}

// No-op macros when OTA is disabled
#define OTA_CHECK_UPDATE()                                                                         \
    do {                                                                                           \
    } while (0)
#define OTA_START_UPDATE()                                                                         \
    do {                                                                                           \
    } while (0)
#define OTA_GET_PROGRESS() nullptr
#define OTA_IS_ACTIVE()    false
#define OTA_CANCEL()                                                                               \
    do {                                                                                           \
    } while (0)

#endif // OTA_UPDATE_ENABLED

#endif // OTA_UPDATE_H