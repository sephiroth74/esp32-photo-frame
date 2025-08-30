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
 * @brief Configuration structure for Google Drive operations.
 * 
 * Contains settings and paths for managing Google Drive file caching,
 * local storage locations, and file naming conventions.
 */
typedef struct {
    unsigned long tocMaxAge;        ///< Maximum age (in seconds) of the local TOC file before refresh
    const char* localPath;          ///< Path to the google drive files on the SD card (toc and images)
    const char* localTocFilename;   ///< Filename of the TOC file on the SD card
    const char* driveFolderId;      ///< Google Drive folder ID to access

} google_drive_config;

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
     * @brief Constructor for google_drive.
     * @param client Reference to the google_drive_client for API operations
     * @param config Configuration settings for Google Drive operations
     */
    google_drive(google_drive_client& client, const google_drive_config& config)
        : client(client)
        , config(config)
    {
    }

    /**
     * @brief Retrieve the Table of Contents (TOC) from Google Drive and store it locally.
     * If the local file (stored in the SD card) exists, it will be used instead of downloading (unless is too old, or force it set to true).
     *
     * @param batteryConservationMode If true, uses cached TOC even if expired to save battery power.
     */
    google_drive_files_list retrieve_toc(bool batteryConservationMode = false);

    /**
     * @brief Download a file from Google Drive to the SD card.
     *
     * @param file The google_drive_file object representing the file to download.
     * @param error Pointer to store the error code result.
     * @return fs::File object representing the downloaded file on the SD card, or empty File on failure.
     */
    fs::File download_file(google_drive_file file, photo_frame_error_t* error);

    /**
     * @brief Clean up temporary files left from previous incomplete downloads
     * @param sdCard Reference to the SD card object
     * @param config Google Drive configuration
     * @param force If true, forces the cleanup of temporary files
     * @return Number of temporary files cleaned up
     */
    static uint32_t cleanup_temporary_files(sd_card& sdCard, const google_drive_config& config, boolean force);

#if !defined(USE_INSECURE_TLS)
    /**
     * @brief Load Google Drive root CA certificate from SD card
     * @param sdCard Reference to the SD card object
     * @return String containing the certificate in PEM format, empty if failed
     */
    static String load_root_ca_certificate(sd_card& sdCard);
#endif

    /**
     * @brief Get the full path to the TOC file on SD card
     * @return String containing the full path to the TOC file
     */
    String get_toc_file_path() const;
    
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
    google_drive_file get_toc_file_by_index(const String& filePath, size_t index, photo_frame_error_t* error = nullptr);

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
    google_drive_client& client;  ///< Reference to the Google Drive client for API operations
    google_drive_config config;   ///< Configuration settings for this Google Drive instance
    
    /**
     * @brief Load TOC from local SD card file
     * @param filePath Path to the TOC file on SD card
     * @return google_drive_files_list loaded from file
     */
    google_drive_files_list load_toc_from_file(const String& filePath);
    
    /**
     * @brief Load TOC from local SD card file using streaming parser (for large files)
     * @param filePath Path to the TOC file on SD card
     * @return google_drive_files_list loaded from file
     */
    google_drive_files_list load_toc_from_file_streaming(const String& filePath);
    
    /**
     * @brief Save TOC to local SD card file with memory optimization
     * @param filePath Path where to save the TOC file
     * @param filesList Files list to save
     * @return true if successful, false otherwise
     */
    bool save_toc_to_file(const String& filePath, const google_drive_files_list& filesList);
    
    /**
     * @brief Save TOC using chunked writing for large file lists
     * @param file Open file handle
     * @param filesList Files list to save
     * @return true if successful, false otherwise
     */
    bool save_toc_to_file_chunked(fs::File& file, const google_drive_files_list& filesList);
};

} // namespace photo_frame