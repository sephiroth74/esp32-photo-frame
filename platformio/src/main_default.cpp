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

// ============================================================================
// DEFAULT MAIN - Normal Mode (Google Drive + SD Card)
// ============================================================================
// This is the main entry point for normal photo frame operation.
// Handles image loading from Google Drive or SD Card and display rendering.

#ifndef ENABLE_BT_IMAGE

#include <Arduino.h>

#include "battery.h"
#include "binary_utils.h"
#include "board_util.h"
#include "config.h"
#include "errors.h"
#include "google_drive.h"
#include "google_drive_client.h"
#include "io_utils.h"
#include "littlefs_manager.h"
#include "preferences_helper.h"
#include "renderer.h"
#include "rgb_status.h"
#include "sd_card.h"
#include "string_utils.h"
#include "unified_config.h"
#include "wifi_manager.h"

#include "main_common.h"
#include "main_default.h"

// ============================================================================
// LOCAL GLOBALS (Mode-specific)
// ============================================================================

// Google Drive instance (initialized from JSON config)
photo_frame::GoogleDrive drive;
photo_frame::SdCard sdCard; // SD_MMC uses fixed SDIO pins
photo_frame::WifiManager wifiManager;
photo_frame::unified_config systemConfig; // Unified configuration system

// ============================================================================
// FORWARD DECLARATIONS (from original main.cpp)
// ============================================================================

photo_frame::photo_frame_error_t
setup_time_and_connectivity(const photo_frame::battery_info_t& battery_info,
                            bool is_reset,
                            DateTime& now);

photo_frame::photo_frame_error_t
handle_google_drive_operations(bool is_reset,
                               fs::File& file,
                               String& original_filename,
                               uint32_t& image_index,
                               uint32_t& total_files,
                               const photo_frame::battery_info_t& battery_info,
                               bool& file_ready);

photo_frame::photo_frame_error_t
handle_sd_card_operations(bool is_reset,
                          fs::File& file,
                          String& original_filename,
                          uint32_t& image_index,
                          uint32_t& total_files,
                          const photo_frame::battery_info_t& battery_info,
                          bool& file_ready);

// ============================================================================
// MODE-SPECIFIC IMPLEMENTATIONS
// ============================================================================

photo_frame::photo_frame_error_t
setup_time_and_connectivity(const photo_frame::battery_info_t& battery_info,
                            bool is_reset,
                            DateTime& now) {
    photo_frame::photo_frame_error_t error = photo_frame::error_type::None;

    log_i("--------------------------------------");
    log_i("- Initialize SD card and load configuration...");
    log_i("--------------------------------------");

    // PHASE 1: SD Card Operations - Display OFF to avoid SPI conflicts
    photo_frame::board_utils::display_power_off();

    RGB_SET_STATE(SD_READING); // Show SD card operations
    error = sdCard.begin();

    // Reduce RGB brightness if battery is low to save power
    if (battery_info.is_low()) {
#ifdef RGB_STATUS_ENABLED
        rgbStatus.setBrightness(32); // Reduce brightness to 50% of normal for low battery
#endif                               // RGB_STATUS_ENABLED
    }

    // Load unified configuration from SD card
    if (error == photo_frame::error_type::None) {
        log_i("Loading unified configuration...");
        error =
            photo_frame::load_unified_config_with_fallback(sdCard, CONFIG_FILEPATH, systemConfig);

        if (error != photo_frame::error_type::None) {
            log_w("Failed to load unified configuration: %d", error.code);
            // Configuration loading failed, but fallback values are loaded
            // Continue with fallback configuration
            error = photo_frame::error_type::None;
        }

        // Validate essential configuration
        if (!systemConfig.wifi.is_valid()) {
            log_w("WARNING: WiFi configuration is missing or invalid!");
            log_w("Please ensure CONFIG_FILEPATH contains valid WiFi credentials");
            error = photo_frame::error_type::WifiCredentialsNotFound;
        }
    } else {
        log_w("SD card initialization failed - using fallback configuration");

        // SD card failed, load fallback configuration and calculate extended sleep
        load_fallback_config(systemConfig);

        // Enter deep sleep immediately with extended duration
        // photo_frame::board_utils::enter_deep_sleep(ESP_SLEEP_WAKEUP_UNDEFINED,
        // fallback_sleep_microseconds);
        return error; // This line won't be reached, but included for completeness
    }

    // WiFi is optional for SD card mode, required for Google Drive
    bool wifiRequired = systemConfig.GoogleDrive.enabled;

    if (error == photo_frame::error_type::None) {
        log_i("Initializing WiFi manager with unified configuration...");
        RGB_SET_STATE(WIFI_CONNECTING); // Show WiFi connecting status

        // Initialize WiFi manager with multiple networks from unified config
        error = wifiManager.initWithNetworks(systemConfig.wifi);

        if (error == photo_frame::error_type::None) {
            // NTP-only time fetching
            log_i("Fetching time from NTP servers...");
            error = wifiManager.connect();
            if (error == photo_frame::error_type::None) {
                now = wifiManager.fetchDatetime(&error);
                if (!now.isValid() || error != photo_frame::error_type::None) {
                    log_e("Failed to fetch time from NTP!");
                    if (error == photo_frame::error_type::None) {
                        error = photo_frame::error_type::NTPSyncFailed;
                    }
                } else {
                    log_i("Successfully fetched time from NTP: %s",
                          now.timestamp(DateTime::TIMESTAMP_FULL).c_str());
                }
            }
        } else {
            log_e("WiFi initialization failed");
            RGB_SET_STATE_TIMED(WIFI_FAILED, 2000); // Show WiFi failed status
        }

        // If WiFi is not required (SD card mode) and it failed, clear the error
        if (!wifiRequired && error != photo_frame::error_type::None) {
            log_w("WiFi failed but not required for SD card mode, continuing without time sync");
            error = photo_frame::error_type::None;
            // Set a default time if WiFi failed
            now = DateTime(2024, 1, 1, 12, 0, 0);
        }
    }

    if (error == photo_frame::error_type::None) {
        log_i("Current time is valid: %s", now.isValid() ? "Yes" : "No");
    } else {
        log_e("Failed to fetch current time! Error code: %d", error.code);
    }

    return error;
}

photo_frame::photo_frame_error_t
handle_google_drive_operations(bool is_reset,
                               fs::File& file,
                               String& original_filename,
                               uint32_t& image_index,
                               uint32_t& total_files,
                               const photo_frame::battery_info_t& battery_info,
                               bool& file_ready) {
    photo_frame::photo_frame_error_t error    = photo_frame::error_type::None;
    photo_frame::photo_frame_error_t tocError = photo_frame::error_type::JsonParseFailed;
    file_ready                                = false; // Initialize to false
    bool write_toc                            = is_reset;

    log_i("--------------------------------------");
    log_i(" - Find the next image from the SD...");
    log_i("--------------------------------------");

    // Ensure display is OFF for SD card operations
    photo_frame::board_utils::display_power_off();

    error = sdCard.begin(); // Initialize the SD card

    if (error == photo_frame::error_type::None) {
#if DEBUG_MODE
        sdCard.printStats(); // Print SD card statistics
#endif

        // Initialize Google Drive from unified configuration
        log_i("Initializing Google Drive from unified config...");
        error = drive.initialize_from_unified_config(systemConfig.GoogleDrive);

        if (error != photo_frame::error_type::None) {
            log_e("Failed to initialize Google Drive from unified config! Error: %d", error.code);
        } else {
            log_i("Google Drive initialized successfully from unified config");
        }

        if (error == photo_frame::error_type::None) {
            // Clean up any temporary files from previous incomplete downloads
            // Only run cleanup once per day to save battery
            bool shouldCleanup = write_toc; // Always cleanup if forced

            if (!shouldCleanup) {
                // Check if we need to run cleanup based on time interval
                auto& prefs        = photo_frame::PreferencesHelper::getInstance();
                time_t now         = time(NULL);
                time_t lastCleanup = prefs.getLastCleanup();

                if (now - lastCleanup >= CLEANUP_TEMP_FILES_INTERVAL_SECONDS) {
                    shouldCleanup = true;
                    log_i("Time since last cleanup: %ld seconds", now - lastCleanup);
                } else {
                    log_i("Skipping cleanup, only %ld seconds since last cleanup (need %d seconds)",
                          now - lastCleanup,
                          CLEANUP_TEMP_FILES_INTERVAL_SECONDS);
                }
            }

            if (shouldCleanup) {
                uint32_t cleanedFiles = drive.cleanup_temporary_files(sdCard, write_toc);
                if (cleanedFiles > 0) {
                    log_i("Cleaned up %u temporary files from previous session", cleanedFiles);
                }

                // Update last cleanup time
                auto& prefs = photo_frame::PreferencesHelper::getInstance();
                if (prefs.setLastCleanup(time(NULL))) {
                    log_i("Updated last cleanup time");
                } else {
                    log_w("Failed to save cleanup time to preferences");
                }
            }

            // Retrieve Table of Contents (with caching)
            // If battery is critical, use cached TOC even if expired to save power
            bool batteryConservationMode = battery_info.is_critical();
            if (batteryConservationMode) {
                log_w("Battery critical (%d%%) - using cached TOC to preserve power",
                      battery_info.percent);
            }

            if (error == photo_frame::error_type::None) {
                error = drive.create_directories(sdCard);
            }

            if (error == photo_frame::error_type::None) {
                // Retrieve TOC and get file count directly
                total_files = drive.retrieve_toc(sdCard, batteryConservationMode);
            } else {
                log_w("Google Drive not initialized - skipping");
                total_files = 0;
            }

            if (total_files > 0) {
                log_i("Total files in Google Drive folder: %u", total_files);

                photo_frame::GoogleDriveFile selectedFile;

#ifdef GOOGLE_DRIVE_TEST_FILE
                // Use specific test file if defined
                log_i("Using test file: %s", GOOGLE_DRIVE_TEST_FILE);
                selectedFile = drive.get_toc_file_by_name(GOOGLE_DRIVE_TEST_FILE, &tocError);
                if (tocError != photo_frame::error_type::None) {
                    log_w("Test file not found in TOC, falling back to random selection. Error: %d",
                          tocError.code);
                    // Fallback to random selection
                    image_index  = random(0, drive.get_toc_file_count());
                    selectedFile = drive.get_toc_file_by_index(image_index, &tocError);
                }
#else
                // Generate a random index to start from
                image_index = random(0, drive.get_toc_file_count(sdCard));

                // Get the specific file by index efficiently
                selectedFile = drive.get_toc_file_by_index(sdCard, image_index, &tocError);
#endif // GOOGLE_DRIVE_TEST_FILE

                // Track if file was successfully processed (binary loaded to PSRAM buffer)
                bool fileProcessedSuccessfully = false;

                if (tocError == photo_frame::error_type::None && selectedFile.id.length() > 0) {
                    log_i("Selected file: %s", selectedFile.name.c_str());

                    // Always download to SD card first for better caching
                    String localFilePath = drive.get_cached_file_path(selectedFile.name);
                    if (sdCard.fileExists(localFilePath.c_str()) &&
                        sdCard.getFileSize(localFilePath.c_str()) > 0) {
                        log_i("File already exists in SD card, using cached version");
                        file = sdCard.open(localFilePath.c_str(), FILE_READ);
                        drive.set_last_image_source(photo_frame::IMAGE_SOURCE_LOCAL_CACHE);
                    } else {
                        // Check battery level before downloading file
                        if (batteryConservationMode) {
                            log_w("Skipping file download due to critical battery level (%d%%) - "
                                  "will use cached files if available",
                                  battery_info.percent);
                            error = photo_frame::error_type::BatteryLevelCritical;
                        } else {
                            // Download the selected file to SD card
                            file = drive.download_file(sdCard, selectedFile, &error);
                        }
                    }

                    // Validate and load image file directly to PSRAM buffer
                    if (error == photo_frame::error_type::None && file) {
                        const char* filename = file.name();
                        String filePath      = String(filename);
                        original_filename    = String(filename);

                        log_i("Validating downloaded image file...");
                        photo_frame::binary_utils::PFR1BinaryFile wrapper(DISP_WIDTH, DISP_HEIGHT);
                        auto validationError =
                            photo_frame::binary_utils::validatePFR1File(file, wrapper);

                        if (validationError != photo_frame::error_type::None) {
                            log_e("Image validation FAILED: %s", validationError.message);

                            // Close file before deletion
                            file.close();

                            // Delete corrupted file from SD card
                            if (sdCard.fileExists(filePath.c_str())) {
                                log_w("Deleting corrupted file from SD card: %s", filePath.c_str());
                                if (sdCard.remove(filePath.c_str())) {
                                    log_i("Corrupted file successfully deleted");
                                } else {
                                    log_e("Failed to delete corrupted file");
                                }
                            }

                            error = validationError;

                            // Close SD card after validation error to prevent SPI conflicts
                            log_i("Closing SD card after validation error");
                            sdCard.end();
                        } else {
                            log_i("Image validation PASSED");

                            // Binary format only: Load to PSRAM buffer, then close SD card
                            log_i("Loading binary image to PSRAM buffer from SD card...");
                            uint16_t loadError = photo_frame::loadImageToBuffer(
                                photo_frame::DisplayManager::getInstance().getBuffer(),
                                file,
                                filename,
                                DISP_WIDTH,
                                DISP_HEIGHT);

                            // Close the SD card file after loading
                            file.close();

                            // Sample first few bytes to verify buffer has data
                            auto& display = photo_frame::DisplayManager::getInstance();
                            log_d("Buffer check - First 8 bytes: %02X %02X %02X %02X %02X %02X "
                                  "%02X %02X",
                                  display.getBuffer()[0],
                                  display.getBuffer()[1],
                                  display.getBuffer()[2],
                                  display.getBuffer()[3],
                                  display.getBuffer()[4],
                                  display.getBuffer()[5],
                                  display.getBuffer()[6],
                                  display.getBuffer()[7]);

                            if (loadError != 0) {
                                log_e("Failed to load image to buffer, error code: %d", loadError);
                                error = photo_frame::error_type::BinaryRenderingFailed;
                            } else {
                                log_i("Binary image loaded to PSRAM buffer");
                                // Mark as successfully processed
                                fileProcessedSuccessfully = true;
                                file_ready                = true; // Binary image loaded to buffer
                            }
                            sdCard.end();
                        }
                    }
                } else {
                    log_e("Failed to get file by index. Error code: %d", tocError.code);
                    error = tocError;
                }

                // Check if file was successfully processed (binary in buffer)
                if (error == photo_frame::error_type::None && (fileProcessedSuccessfully || file)) {
                    log_i("File downloaded and ready for display!");
                } else {
                    log_e("Failed to download file from Google Drive! Error code: %d", error.code);
                }
            } else {
                // Check if there was an error during TOC retrieval (e.g., access token failure)
                photo_frame::photo_frame_error_t lastDriveError = drive.get_last_error();
                if (lastDriveError != photo_frame::error_type::None) {
                    log_e("Failed to retrieve TOC. Error code: %d", lastDriveError.code);
                    error = lastDriveError;
                } else if (tocError != photo_frame::error_type::None) {
                    log_e("Failed to read TOC file count. Error code: %d", tocError.code);
                    error = tocError;
                } else {
                    log_w("No files found in Google Drive folder!");
                    error = photo_frame::error_type::NoImagesFound;
                }

                // CRITICAL: Close SD card on error to prevent SPI conflicts with display
                log_i("Closing SD card after Google Drive error to prevent SPI conflicts");
                sdCard.end();
            }
        }
    } else {
        log_e("Failed to initialize SD card. Error code: %d", error.code);
    }

    // CRITICAL: Ensure SD card is closed on any error before display initialization
    // This prevents SPI bus conflicts that cause "Busy Timeout!" on the display
    if (error != photo_frame::error_type::None && sdCard.isInitialized()) {
        log_w("Closing SD card due to error (code %d) to prevent SPI conflicts", error.code);
        sdCard.end();
    }

    return error;
}

photo_frame::photo_frame_error_t
handle_sd_card_operations(bool is_reset,
                          fs::File& file,
                          String& original_filename,
                          uint32_t& image_index,
                          uint32_t& total_files,
                          const photo_frame::battery_info_t& battery_info,
                          bool& file_ready) {
    photo_frame::photo_frame_error_t error = photo_frame::error_type::None;
    file_ready                             = false;

    log_i("--------------------------------------");
    log_i(" - SD Card Only Mode - Local Images");
    log_i("--------------------------------------");

    // Ensure display is OFF for SD card operations
    photo_frame::board_utils::display_power_off();

    // Initialize SD card
    error = sdCard.begin();
    if (error != photo_frame::error_type::None) {
        log_e("Failed to initialize SD card for image source");
        return error;
    }

#if DEBUG_MODE
    sdCard.printStats(); // Print SD card statistics
#endif

    // Check if the configured directory exists
    const char* images_dir = systemConfig.sd_card.images_directory.c_str();
    if (!sdCard.isDirectory(images_dir)) {
        log_e("Images directory does not exist: %s", images_dir);
        sdCard.end();
        return photo_frame::error_type::NoImagesFound;
    }

    // Build or validate TOC if caching is enabled
    if (systemConfig.sd_card.use_toc_cache) {
        // Only rebuild TOC on reset or if TOC doesn't exist/is invalid
        bool rebuild_toc = is_reset || !sdCard.isTocValid(images_dir, ".bin");

        if (rebuild_toc) {
            if (is_reset) {
                log_i("Reset detected - rebuilding SD card TOC for directory: %s", images_dir);
            } else {
                log_i("TOC invalid - building SD card TOC for directory: %s", images_dir);
            }
            photo_frame::photo_frame_error_t tocError;
            if (sdCard.buildDirectoryToc(images_dir, ".bin", &tocError)) {
                log_i("SD card TOC built successfully");
            } else {
                log_w(
                    "Failed to build SD card TOC: %s (code: %u), falling back to direct iteration",
                    tocError.message,
                    tocError.code);
            }
        } else {
            log_i("Using existing SD card TOC cache (no reset, TOC valid)");
        }
    }

    // Count total files in directory (uses TOC cache if enabled)
    total_files = systemConfig.sd_card.use_toc_cache
                      ? sdCard.countFilesCached(images_dir, ".bin", true)
                      : sdCard.countFilesInDirectory(images_dir, ".bin");
    if (total_files == 0) {
        log_e("No .bin files found in directory: %s", images_dir);
        sdCard.end();
        return photo_frame::error_type::NoImagesFound;
    }

    log_i("Found %d image files in %s", total_files, images_dir);

    // Select a random image
    image_index = random(0, total_files);
    log_i("Selected random image index: %d", image_index);

    // Get the file path at the selected index (uses TOC cache if enabled)
    String file_path = systemConfig.sd_card.use_toc_cache
                           ? sdCard.getFileAtIndexCached(images_dir, image_index, ".bin", true)
                           : sdCard.getFileAtIndex(images_dir, image_index, ".bin");
    if (file_path.isEmpty()) {
        log_e("Failed to get file at index %d", image_index);
        sdCard.end();
        return photo_frame::error_type::SdCardFileNotFound;
    }

    // Store the original filename for display
    int lastSlash = file_path.lastIndexOf('/');
    if (lastSlash >= 0) {
        original_filename = file_path.substring(lastSlash + 1);
    } else {
        original_filename = file_path;
    }

    log_i("Selected image: %s", original_filename.c_str());

    // Open the file
    file = sdCard.open(file_path.c_str());
    if (!file) {
        log_e("Failed to open image file: %s", file_path.c_str());
        sdCard.end();
        return photo_frame::error_type::SdCardFileOpenFailed;
    }

    // Validate the binary file
    log_i("Validating image file dimensions and size...");
    photo_frame::binary_utils::PFR1BinaryFile wrapper(DISP_WIDTH, DISP_HEIGHT);
    auto validationError = photo_frame::binary_utils::validatePFR1File(file, wrapper);

    if (validationError != photo_frame::error_type::None) {
        log_e("Image validation failed for: %s - %s",
              original_filename.c_str(),
              validationError.message);
        file.close();
        sdCard.end();
        return validationError;
    }

    log_i("✓ Image validation PASSED");

    // Load binary image to PSRAM buffer
    log_i("Loading binary image to PSRAM buffer...");
    uint16_t loadError =
        photo_frame::loadImageToBuffer(photo_frame::DisplayManager::getInstance().getBuffer(),
                                       file,
                                       original_filename.c_str(),
                                       DISP_WIDTH,
                                       DISP_HEIGHT);

    file.close();

    if (loadError != 0) {
        log_e("Failed to load image to buffer, error code: %d", loadError);

        // If read failed, try to reinitialize SD card once
        if (loadError == 4) { // Read error
            log_w("SD card read error detected, attempting to reinitialize SD card...");
            sdCard.end();
            delay(100);

            // Try to reinitialize SD card
            photo_frame::photo_frame_error_t reinit_error = sdCard.begin();
            if (reinit_error != photo_frame::error_type::None) {
                log_e("Failed to reinitialize SD card");
                return photo_frame::error_type::CardMountFailed;
            }

            // Try to open and read the file again
            log_i("Retrying file read after SD card reinitialization...");
            file = sdCard.open(original_filename.c_str(), FILE_READ);
            if (!file) {
                log_e("Failed to reopen file after SD reinitialization");
                sdCard.end();
                return photo_frame::error_type::CardOpenFileFailed;
            }

            loadError = photo_frame::loadImageToBuffer(
                photo_frame::DisplayManager::getInstance().getBuffer(),
                file,
                original_filename.c_str(),
                DISP_WIDTH,
                DISP_HEIGHT);
            file.close();

            if (loadError != 0) {
                log_e("Failed to load image after SD reinitialization, error code: %d", loadError);
                sdCard.end();
                return photo_frame::error_type::BinaryRenderingFailed;
            }

            log_i("Successfully loaded image after SD reinitialization");
        } else {
            sdCard.end();
            return photo_frame::error_type::BinaryRenderingFailed;
        }
    }

    log_i("Binary image loaded to PSRAM buffer");
    file_ready = true;

    // Store last displayed index in preferences
    auto& prefs = photo_frame::PreferencesHelper::getInstance();
    prefs.setImageIndex(image_index);

    // Keep SD card mounted for potential next image
    // It will be closed later if needed

    return error;
}

// ============================================================================
// SETUP & LOOP
// ============================================================================

void default_main_setup() {
    Serial.begin(115200);
    delay(5000);

    // Initialize display power control (if configured)
    photo_frame::board_utils::init_display_power();

    log_i("\n==================================");
    log_i("*** NORMAL MODE ***");
    log_i("==================================");

    // Initialize hardware components
    if (!initialize_hardware()) {
        log_e("Failed to initialize hardware!");
        return;
    }

    // Determine wakeup reason and setup basic state
    esp_sleep_wakeup_cause_t wakeup_reason = photo_frame::board_utils::get_wakeup_reason();

    // Consider it a reset if it's an undefined wakeup (power on/reset)
    // EXT1 wakeup (button press) goes through normal TOC validation
    bool is_reset = wakeup_reason == ESP_SLEEP_WAKEUP_UNDEFINED;

    char wakeup_reason_string[32];
    photo_frame::board_utils::get_wakeup_reason_string(
        wakeup_reason, wakeup_reason_string, sizeof(wakeup_reason_string));

    log_i("Wakeup reason: %s (%d)", wakeup_reason_string, wakeup_reason);
    log_i("Is reset: %s", is_reset ? "Yes" : "No");

    // Setup battery and power management
    photo_frame::battery_info_t battery_info;
    photo_frame::photo_frame_error_t error = setup_battery_and_power(battery_info, wakeup_reason);

    // Setup time synchronization and connectivity
    DateTime now = DateTime((uint32_t)0);

    if (error == photo_frame::error_type::None && !battery_info.is_critical()) {
        error = setup_time_and_connectivity(battery_info, is_reset, now);
    }

    // Set rotation from config BEFORE initializing buffer
    display_rotation = systemConfig.board.display_rotation;

    // If config loading failed, try to get from preferences
    if (!systemConfig.is_valid()) {
        auto& prefs      = photo_frame::PreferencesHelper::getInstance();
        display_rotation = prefs.getDisplayRotation(); // Default to 0 (landscape) if not set
        log_w("Config invalid, using rotation from preferences: %u", display_rotation);
    }

    log_i("Display rotation: %u", display_rotation);

    // Phase 1: Initialize PSRAM image buffer BEFORE SD card operations
    // This allocates the buffer but does NOT initialize display hardware
    log_i("--------------------------------------");
    log_i("- Phase 1: Initializing image buffer...");
    log_i("--------------------------------------");
    if (!init_image_buffer()) {
        // Critical failure - cannot continue without buffer
        log_e("[main] FATAL: Buffer initialization failed!");
        log_e("[main] Entering deep sleep mode");

        const uint64_t emergency_sleep_duration = 60 * 60 * 1000000ULL; // 1 hour
        photo_frame::board_utils::enter_deep_sleep(ESP_SLEEP_WAKEUP_UNDEFINED,
                                                   emergency_sleep_duration);
        return;
    }

    // Handle Google Drive operations
    fs::File file;
    uint32_t image_index = 0, total_files = 0;
    String original_filename; // Store original filename for format detection
    bool file_ready = false;  // Track if file is ready (buffer loaded or file open)

    if (error == photo_frame::error_type::None && !battery_info.is_critical()) {
        // Choose image source based on configuration
        if (systemConfig.GoogleDrive.enabled) {
            log_i("Using Google Drive as image source");
            RGB_SET_STATE(GOOGLE_DRIVE); // Show Google Drive operations
            error = handle_google_drive_operations(is_reset,
                                                   file,
                                                   original_filename,
                                                   image_index,
                                                   total_files,
                                                   battery_info,
                                                   file_ready);
        } else if (systemConfig.sd_card.enabled) {
            log_i("Using SD Card as image source (Google Drive disabled)");
            RGB_SET_STATE(SD_READING); // Show SD Card operations
            error = handle_sd_card_operations(is_reset,
                                              file,
                                              original_filename,
                                              image_index,
                                              total_files,
                                              battery_info,
                                              file_ready);
        } else {
            // This should not happen as config validation ensures at least one source is enabled
            log_e("No image source enabled!");
            error = photo_frame::error_type::InvalidConfigNoImageSource;
        }
    }

    // Safely disconnect WiFi with proper cleanup
    if (wifiManager.isConnected()) {
        log_i("Disconnecting WiFi to save power...");
        // Add small delay to ensure pending WiFi operations complete
        delay(100);
        wifiManager.disconnect();
        // Wait for disconnect to complete
        delay(200);
    }

    log_i("WiFi operations complete - using NTP-only time");

    // Calculate refresh delay
    log_i("--------------------------------------");
    log_i("- Calculating refresh rate");
    log_i("--------------------------------------");
    refresh_delay_t refresh_delay = calculate_wakeup_delay(battery_info, now);

    // Phase 2: Initialize E-Paper display hardware (after SD card operations are complete)
    log_i("--------------------------------------");
    log_i("- Phase 2: Initializing display hardware...");
    log_i("--------------------------------------");

    // PHASE 2: Display Operations - Power ON display now that SD card is closed
    photo_frame::board_utils::display_power_on();

    delay(100);
    RGB_SET_STATE(RENDERING); // Show display rendering

    // Initialize the display hardware now that SD card is closed
    if (!init_display_hardware()) {
        // Critical failure - cannot continue without display
        log_e("[main] FATAL: Display hardware initialization failed!");
        log_e("[main] Entering deep sleep mode to preserve battery");

        const uint64_t emergency_sleep_duration = 60 * 60 * 1000000ULL; // 1 hour
        photo_frame::board_utils::enter_deep_sleep(ESP_SLEEP_WAKEUP_UNDEFINED,
                                                   emergency_sleep_duration);
        return;
    }

    // Allow time for display SPI bus initialization to complete
    delay(300);
    log_i("Display initialization complete");

    // Prepare display for rendering
    // Note: fillScreen() removed in v0.11.0 to eliminate race condition causing white vertical
    // stripes The full-screen image write will overwrite the entire display buffer, making
    // fillScreen() redundant
    log_i("Preparing display for rendering...");

    // Check if file is ready (binary image loaded to buffer)
    if (error == photo_frame::error_type::None && !file_ready) {
        log_e("File is not ready! (buffer not loaded)");
        error = photo_frame::error_type::SdCardFileOpenFailed;
    }

    // Handle errors or process image file
    if (error != photo_frame::error_type::None) {
        RGB_SET_STATE(ERROR); // Show error status

        auto& display = photo_frame::DisplayManager::getInstance();
        // Clear display and draw error (include filename if available)
        display.clear(DISPLAY_COLOR_WHITE);
        display.drawError(error, original_filename.isEmpty() ? nullptr : original_filename.c_str());

        if (error != photo_frame::error_type::BatteryLevelCritical && now.isValid()) {
            display.drawLastUpdate(now, refresh_delay.refresh_seconds);
        }

        // Render to display
        display.render();
    } else {
        // Render the image if it was successfully loaded
        if (error == photo_frame::error_type::None && file_ready) {
            log_i("Rendering validated binary image from buffer...");
            error = render_image(file,
                                 original_filename.c_str(),
                                 error,
                                 now,
                                 refresh_delay,
                                 image_index,
                                 total_files,
                                 drive,
                                 battery_info);
        }

        // Ensure file is closed (should already be closed after buffer load)
        if (file) {
            file.close();
        }
    }

    // Finalize and enter sleep - show sleep preparation with delay
    RGB_SET_STATE(SLEEP_PREP); // Show sleep preparation
    delay(2500);               // Allow sleep preparation animation to complete
    finalize_and_enter_sleep(battery_info, now, wakeup_reason, refresh_delay);
}

void default_main_loop() {
    delay(1000); // Just to avoid watchdog reset
}

#endif // ENABLE_BT_IMAGE