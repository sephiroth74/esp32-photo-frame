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

#pragma once

#include "config.h"
#include "google_drive_client.h"
#include "sd_card.h"

namespace photo_frame {

/**
 * @brief Image source enumeration for tracking where an image was loaded from.
 */
typedef enum image_source {
    IMAGE_SOURCE_CLOUD,      ///< Image was downloaded from Google Drive
    IMAGE_SOURCE_LOCAL_CACHE ///< Image was loaded from local SD card cache
} image_source_t;


/**
 * @brief JSON-based configuration structure for Google Drive settings.
 *
 * Contains all configuration parameters that can be loaded from a JSON file.
 */
typedef struct {
    String serviceAccountEmail;
    String privateKeyPem;
    String clientId;
    String folderId;
    String rootCaPath;
    int listPageSize;
    bool useInsecureTls;
    String localPath;
    String tocFilename;
    unsigned long tocMaxAgeSeconds;
    int maxRequestsPerWindow;
    int rateLimitWindowSeconds;
    int minRequestDelayMs;
    int maxRetryAttempts;
    int backoffBaseDelayMs;
    int maxWaitTimeMs;
} google_drive_json_config;

/**
 * @brief Load Google Drive configuration from JSON file on SD card
 * @param sd_card Reference to the SD card instance
 * @param config_filepath Path to the JSON configuration file
 * @param config Output parameter for the loaded configuration
 * @return photo_frame_error_t indicating success or failure
 */
photo_frame_error_t load_google_drive_config_from_json(sd_card& sd_card, 
                                                      const char* config_filepath, 
                                                      google_drive_json_config& config);

/**
 * @brief High-level Google Drive interface for file management and caching.
 *
 * This class provides a convenient interface for interacting with Google Drive,
 * handling file list caching, downloading files to SD card, and managing local
 * storage. It acts as a wrapper around google_drive_client with additional
 * caching and optimization features.
 */
class google_drive {
  public:
    /**
     * @brief Default constructor for google_drive.
     */
    google_drive() : client(google_drive_client_config{}), config{}, last_image_source(IMAGE_SOURCE_LOCAL_CACHE) {}

    /**
     * @brief Initialize Google Drive from JSON configuration file.
     * @param sd_card Reference to the SD card instance
     * @param config_filepath Path to the JSON configuration file
     * @return photo_frame_error_t indicating success or failure
     */
    photo_frame_error_t initialize_from_json(sd_card& sd_card, const char* config_filepath);

    /**
     * @brief Create necessary directories on the sdcard for google drive local cache.
     * @param sdCard Reference to the SD card object
     * @return photo_frame_error_t indicating success or failure
     */
    photo_frame_error_t create_directories(sd_card& sdCard);

    /**
     * @brief Retrieve the Table of Contents (TOC) from Google Drive and store it locally.
     * If the local file (stored in the SD card) exists, it will be used instead of downloading
     * (unless is too old, or force it set to true).
     *
     * @param batteryConservationMode If true, uses cached TOC even if expired to save battery
     * power.
     * @return Total number of files in the TOC, or 0 if failed
     */
    size_t retrieve_toc(bool batteryConservationMode = false);

    /**
     * @brief Download a file from Google Drive to the SD card.
     *
     * @param file The google_drive_file object representing the file to download.
     * @param error Pointer to store the error code result.
     * @return fs::File object representing the downloaded file on the SD card, or empty File on
     * failure.
     */
    fs::File download_file(google_drive_file file, photo_frame_error_t* error);

    /**
     * @brief Download a file from Google Drive directly to LittleFS.
     *
     * @param file The google_drive_file object representing the file to download.
     * @param littlefs_path The path where to save the file in LittleFS.
     * @param error Pointer to store the error code result.
     * @return fs::File object representing the downloaded file in LittleFS, or empty File on
     * failure.
     */
    fs::File download_file_to_littlefs(google_drive_file file, const String& littlefs_path, photo_frame_error_t* error);

    /**
     * @brief Get the source of the last downloaded/accessed file.
     * @return image_source_t indicating whether the last file was from cloud or local cache
     */
    image_source_t get_last_image_source() const;

    /**
     * @brief Set the source of the current image for tracking purposes.
     * @param source The source type (cloud or local cache)
     */
    void set_last_image_source(image_source_t source);

    /**
     * @brief Clean up temporary files left from previous incomplete downloads
     * @param sdCard Reference to the SD card object
     * @param config Google Drive configuration
     * @param force If true, forces the cleanup of temporary files
     * @return Number of temporary files cleaned up
     */
    static uint32_t
    cleanup_temporary_files(sd_card& sdCard, const google_drive_json_config& config, boolean force);

    /**
     * @brief Clean up temporary files left from previous incomplete downloads
     * @param force If true, forces the cleanup of temporary files
     * @return Number of temporary files cleaned up
     */
    uint32_t cleanup_temporary_files(boolean force);

    /**
     * @brief Load Google Drive root CA certificate from SD card
     * @param sdCard Reference to the SD card object
     * @param rootCaPath Path to the root CA certificate file
     * @return String containing the certificate in PEM format, empty if failed
     */
    static String load_root_ca_certificate(sd_card& sdCard, const char* rootCaPath);

    /**
     * @brief Get the full path to the TOC file on SD card
     * @return String containing the full path to the TOC file
     */
    String get_toc_file_path() const;

    /**
     * @brief Get the full path to the temp directory on SD card
     * @return String containing the full path to the temp directory
     */
    String get_temp_dir_path() const;

    /**
     * @brief Get the full path to the cache directory on SD card
     * @return String containing the full path to the cache directory
     */
    String get_cache_dir_path() const;

    /**
     * @brief Get the full path for a cached file on SD card
     * @param filename Name of the file
     * @return String containing the full path to the cached file
     */
    String get_cached_file_path(const String& filename) const;

    /**
     * @brief Get the full path for a temporary file on SD card
     * @param filename Name of the file
     * @return String containing the full path to the temporary file
     */
    String get_temp_file_path(const String& filename) const;

    /**
     * @brief Get the file count from a plain text TOC file efficiently
     * @param filePath Path to the TOC file on SD card
     * @param error Pointer to error code (optional)
     * @return Number of files in the TOC, or 0 if error
     */
    size_t get_toc_file_count(const String& filePath, photo_frame_error_t* error = nullptr);

    /**
     * @brief Get a specific file entry by index from a plain text TOC file
     * @param filePath Path to the TOC file on SD card
     * @param index Zero-based index of the file to retrieve
     * @param error Pointer to error code (optional)
     * @return google_drive_file at the specified index, or empty file if error
     */
    google_drive_file get_toc_file_by_index(const String& filePath,
                                            size_t index,
                                            photo_frame_error_t* error = nullptr);

    /**
     * @brief Get the file count from the TOC file efficiently
     * @param error Pointer to error code (optional)
     * @return Number of files in the TOC, or 0 if error
     */
    size_t get_toc_file_count(photo_frame_error_t* error = nullptr);

    /**
     * @brief Get a specific file entry by index from the TOC file
     * @param index Zero-based index of the file to retrieve
     * @param error Pointer to error code (optional)
     * @return google_drive_file at the specified index, or empty file if error
     */
    google_drive_file get_toc_file_by_index(size_t index, photo_frame_error_t* error = nullptr);

    /**
     * @brief Find a file by name in the TOC file
     * @param filename Name of the file to search for
     * @param error Pointer to error code (optional)
     * @return google_drive_file with the specified name, or empty file if not found
     */
    google_drive_file get_toc_file_by_name(const char* filename, photo_frame_error_t* error = nullptr);

    /**
     * @brief Save the current access token to SD card
     * @return Error code indicating success or failure
     */
    photo_frame_error_t save_access_token_to_file();

    /**
     * @brief Load access token from SD card and set it in the client
     * @return Error code indicating success or failure
     */
    photo_frame_error_t load_access_token_from_file();

  private:
    google_drive_client client; ///< Google Drive client for API operations
    google_drive_json_config config;  ///< Configuration settings for this Google Drive instance
    image_source_t last_image_source; ///< Source of the last accessed/downloaded image


};

} // namespace photo_frame