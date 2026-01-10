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

#include <ArduinoJson.h>
#include <time.h>
#include <unistd.h> // For fsync()

#include "datetime_utils.h"
#include "google_drive.h"
#include "google_drive_toc_parser.h"
#include "sd_card.h"
#include "string_utils.h"

// Memory monitoring utility
void logMemoryUsage(const char* context) {
    size_t freeHeap    = ESP.getFreeHeap();
    size_t totalHeap   = ESP.getHeapSize();
    size_t usedHeap    = totalHeap - freeHeap;
    float usagePercent = (float)usedHeap / totalHeap * 100.0;

#ifdef DEBUG_MEMORY_USAGE
    log_d("Memory [%s]: Free=%zu Used=%zu (%.1f%%)",
          context, freeHeap, usedHeap, usagePercent);
#endif // DEBUG_MEMORY_USAGE

    // Warning if memory usage is high
    if (usagePercent > 80.0) {
        log_w("WARNING: High memory usage detected!");
    }
}

// Check if we have enough memory for JSON operations
bool checkMemoryAvailable(size_t requiredBytes) {
    size_t freeHeap     = ESP.getFreeHeap();
    size_t safetyMargin = 2048; // Keep 2KB safety margin

    if (freeHeap < (requiredBytes + safetyMargin)) {
        log_w("Insufficient memory: need %zu, have %zu", requiredBytes, freeHeap);
        return false;
    }
    return true;
}

namespace photo_frame {

size_t google_drive::retrieve_toc(sd_card& sdCard, bool batteryConservationMode) {
    logMemoryUsage("TOC Retrieve Start");

    // Clear any previous error
    last_error = error_type::None;

    // Build TOC path using optimized utility function
    String tocFullPath        = get_toc_file_path();
    String tocDataPath        = get_toc_file_path();
    bool shouldFetchFromDrive = false;
    bool localTocExists =
        sdCard.file_exists(tocFullPath.c_str()) && sdCard.get_file_age(tocDataPath.c_str()) >= 0;

    if (localTocExists) {
        // Check file age
        time_t fileTime = sdCard.get_last_modified(tocFullPath.c_str());
        log_i("Local TOC file found: %s", tocFullPath.c_str());
        log_i("TOC file last modified: %ld", fileTime);

        time_t currentTime = time(NULL);

        if (fileTime > 0 && currentTime > 0) {
            unsigned long fileAge = currentTime - fileTime;

            log_i("TOC file age: %lu seconds, max age: %lu",
                  fileAge, config.caching.toc_max_age_seconds);

            if (fileAge <= config.caching.toc_max_age_seconds) {
                log_i("Using cached TOC file");
                return get_toc_file_count(sdCard, tocFullPath);
            } else if (batteryConservationMode) {
                log_i("TOC file is old but using cached version due to battery conservation mode");
                return get_toc_file_count(sdCard, tocFullPath);
            } else {
                log_i("TOC file is too old, will try to fetch from Google Drive");
                shouldFetchFromDrive = true;
            }
        } else if (batteryConservationMode) {
            log_i("Could not determine TOC file timestamp, but using existing file due to battery conservation mode");
            return get_toc_file_count(sdCard, tocFullPath);
        } else {
            log_w("Could not determine TOC file timestamp, will try to fetch from Google Drive");
            shouldFetchFromDrive = true;
        }
    } else if (!localTocExists) {
        if (batteryConservationMode) {
            log_i("TOC file does not exist locally and battery conservation mode is on - returning 0");
            return 0; // Return 0 to avoid network operation
        } else {
            log_i("TOC file does not exist locally, will fetch from Google Drive");
            shouldFetchFromDrive = true;
        }
    }

    if (shouldFetchFromDrive) {
        log_i("Fetching TOC from Google Drive...");

        // Try to load cached access token first
        photo_frame_error_t error = load_access_token_from_file();
        if (error != error_type::None) {
            log_i("No valid cached token, getting new access token...");
            error = client.get_access_token();
            if (error != error_type::None) {
                log_e("Failed to get access token: %d", error.code);
            } else {
                // Save the new token for future use
                save_access_token_to_file();
            }
        } else {
            log_i("Using cached access token");
        }

        if (error != error_type::None) {
            // Store the original access token error
            photo_frame_error_t original_error = error;

            // Fallback to local TOC if available and not forced
            if (localTocExists) {
                log_i("Falling back to cached TOC file");
                photo_frame_error_t toc_error = error_type::None;
                size_t count = get_toc_file_count(sdCard, tocFullPath, &toc_error);

                if (count == 0 && toc_error != error_type::None) {
                    // Local TOC also failed - report the original access token error instead
                    log_e("Both access token and local TOC failed. Original error: %d", original_error.code);
                    // Store the original error so it can be retrieved later
                    last_error = original_error;
                }
                return count;
            }
            // Store the error so it can be retrieved later
            last_error = error;
            return 0; // Return 0 on error
        }

        // Check memory before Google Drive operations
        if (!checkMemoryAvailable(4096)) {
            log_w("Insufficient memory for Google Drive operations");
            if (localTocExists) {
                log_i("Falling back to cached TOC file");
                return get_toc_file_count(sdCard, tocFullPath);
            }
            return 0; // Return 0 on error
        }

        // Stream files directly from Google Drive to TOC file to save memory
        log_i("Fetching TOC from Google Drive...");

        // Create paths for the 2-file TOC system
        String tocDataPath = get_toc_file_path();
        String tocMetaPath = get_toc_meta_file_path();

        log_i("Using 2-file TOC system: data=%s, meta=%s", tocDataPath.c_str(), tocMetaPath.c_str());

        // Remove existing data file to start fresh
        if (sdCard.file_exists(tocDataPath.c_str())) {
            sdCard.remove(tocDataPath.c_str());
            log_i("Removed existing TOC data file");
        }

        // Stream files directly from Google Drive to TOC file to save memory
        size_t totalFiles = client.list_files_streaming(
            config.drive.folder_id.c_str(), sdCard, tocDataPath.c_str(), config.drive.list_page_size);

        if (totalFiles == 0) {
            log_e("Failed to retrieve files from Google Drive");
            // Remove incomplete data file
            sdCard.remove(tocDataPath.c_str());

            // Fallback to local TOC if available and not forced
            if (localTocExists) {
                log_i("Falling back to cached TOC file");
                return get_toc_file_count(sdCard, tocFullPath);
            }
            return 0; // Return 0 on error
        }

        // Verify the file was actually written
        log_i("Verifying data file exists and has content...");
        if (sdCard.file_exists(tocDataPath.c_str())) {
            fs::File verifyFile = sdCard.open(tocDataPath.c_str(), FILE_READ);
            if (verifyFile) {
                size_t actualSize = verifyFile.size();
                verifyFile.close();
                log_i(" Size: %zu bytes", actualSize);

                if (actualSize == 0) {
                    log_e("ERROR: Data file is empty after write!");
                    if (localTocExists) {
                        log_i("Falling back to cached TOC file");
                        return get_toc_file_count(sdCard, tocFullPath);
                    }
                    return 0;
                }
            } else {
                log_e(" ERROR: Cannot open for verification");
            }
        } else {
            log_e(" ERROR: File does not exist");
        }

        if (totalFiles == 0) {
            log_e("Failed to retrieve files from Google Drive");
            sdCard.remove(tocDataPath.c_str()); // Remove incomplete data file

            // Fallback to local TOC if available and not forced
            if (localTocExists) {
                log_i("Falling back to cached TOC file");
                return get_toc_file_count(sdCard, tocFullPath);
            }
            return 0; // Return 0 on error
        }

        // Get data file size for integrity verification
        size_t dataFileSize = 0;
        if (sdCard.file_exists(tocDataPath.c_str())) {
            fs::File sizeCheckFile = sdCard.open(tocDataPath.c_str(), FILE_READ);
            if (sizeCheckFile) {
                dataFileSize = sizeCheckFile.size();
                sizeCheckFile.close();
            }
        }

        // Now write the metadata file with correct file count and data file size
        fs::File tocMetaFile = sdCard.open(tocMetaPath.c_str(), FILE_WRITE);
        if (!tocMetaFile) {
            log_e("Failed to open TOC meta file for writing");
            sdCard.remove(tocDataPath.c_str()); // Clean up data file
            if (localTocExists) {
                log_i("Falling back to cached TOC file");
                return get_toc_file_count(sdCard, tocFullPath);
            }
            return 0; // Return 0 on error
        }

        // Write metadata (timestamp, file count, and data file size)
        time_t now = time(NULL);
        tocMetaFile.print(F("timestamp = "));
        tocMetaFile.println(now);
        tocMetaFile.print(F("fileCount = "));
        tocMetaFile.println(totalFiles);
        tocMetaFile.print(F("dataFileSize = "));
        tocMetaFile.println(dataFileSize);
        tocMetaFile.close();

        log_i("TOC system complete: %zu files written to data file, metadata saved", totalFiles);
        log_i("TOC file closed: %s", tocFullPath.c_str());
        log_i("Retrieved %zu files from Google Drive", totalFiles);

        logMemoryUsage("TOC Retrieve End");
        return totalFiles;
    }

    logMemoryUsage("TOC Retrieve End");
    return 0;
}

photo_frame_error_t google_drive::create_directories(sd_card& sdCard) {
    // Create necessary directories on the SD card for Google Drive local cache
    // first make sure the directory doesn't exists and it's not a file, in which case we will try
    // to delete the file and continue otherwise, if the directory already exists, continue to check
    // subdirectories.
    if (sdCard.file_exists(config.caching.local_path.c_str())) {
        if (sdCard.is_file(config.caching.local_path.c_str())) {
            log_i("Removing file at path: %s", config.caching.local_path.c_str());
            if (!sdCard.remove(config.caching.local_path.c_str())) {
                log_e("Failed to remove file at path: %s", config.caching.local_path.c_str());
                return photo_frame::error_type::SdCardDirCreateFailed;
            }
        } else {
            log_i("Root directory already exists: %s", config.caching.local_path.c_str());
        }
    }

    // Create root directory
    if (!sdCard.create_directories(config.caching.local_path.c_str())) {
        log_e("Failed to create root directory for Google Drive local cache: %s", config.caching.local_path.c_str());
        return photo_frame::error_type::SdCardDirCreateFailed;
    }

    // Create temp subdirectory
    String tempDir = get_temp_dir_path();
    if (!sdCard.create_directories(tempDir.c_str())) {
        log_e("Failed to create temp directory: %s", tempDir.c_str());
        return photo_frame::error_type::SdCardDirCreateFailed;
    }
    log_i("Created temp directory: %s", tempDir.c_str());

    // Create cache subdirectory
    String cacheDir = get_cache_dir_path();
    if (!sdCard.create_directories(cacheDir.c_str())) {
        log_e("Failed to create cache directory: %s", cacheDir.c_str());
        return photo_frame::error_type::SdCardDirCreateFailed;
    }
    log_i("Created cache directory: %s", cacheDir.c_str());

    return photo_frame::error_type::None;
}

String google_drive::get_toc_file_path() const {
    return string_utils::build_path(config.caching.local_path, TOC_DATA_FILENAME);
}

String google_drive::get_toc_meta_file_path() const {
    return string_utils::build_path(config.caching.local_path, TOC_META_FILENAME);
}

String google_drive::get_temp_dir_path() const {
    return string_utils::build_path(config.caching.local_path, GOOGLE_DRIVE_TEMP_DIR);
}

String google_drive::get_cache_dir_path() const {
    return string_utils::build_path(config.caching.local_path, GOOGLE_DRIVE_CACHE_DIR);
}

String google_drive::get_cached_file_path(const String& filename) const {
    return string_utils::build_path(get_cache_dir_path(), filename);
}

String google_drive::get_temp_file_path(const String& filename) const {
    return string_utils::build_path(get_temp_dir_path(), filename);
}

size_t google_drive::get_toc_file_count(sd_card& sdCard,
                                        const String& filePath,
                                        photo_frame_error_t* error) {
    if (error) {
        *error = error_type::None;
    }

    // Build path to metadata file
    String tocMetaPath = get_toc_meta_file_path();

    log_i("Reading TOC metadata from: %s", tocMetaPath.c_str());

    fs::File metaFile = sdCard.open(tocMetaPath.c_str(), FILE_READ);
    if (!metaFile) {
        log_e("Failed to open TOC metadata file: %s", tocMetaPath.c_str());
        if (error) {
            *error = error_type::SdCardFileOpenFailed;
        }
        return 0;
    }

    // Read line 1 (timestamp)
    String line = metaFile.readStringUntil('\n');
    if (line.length() == 0) {
        log_e("TOC metadata file is empty or invalid");
        metaFile.close();
        if (error) {
            *error = error_type::JsonParseFailed;
        }
        return 0;
    }

    // Read line 2 (fileCount)
    line = metaFile.readStringUntil('\n');
    if (line.length() == 0) {
        log_e("TOC metadata file missing fileCount line");
        metaFile.close();
        if (error) {
            *error = error_type::JsonParseFailed;
        }
        return 0;
    }

    // Parse "fileCount = <number>"
    int equalPos = line.indexOf('=');
    if (equalPos == -1) {
        log_e("Invalid TOC metadata format: missing '=' in fileCount line");
        metaFile.close();
        if (error) {
            *error = error_type::JsonParseFailed;
        }
        return 0;
    }

    String countStr = line.substring(equalPos + 1);
    countStr.trim();

    size_t fileCount = countStr.toInt();
    if (fileCount == 0 && countStr != "0") {
        log_e("Invalid fileCount value in TOC metadata");
        metaFile.close();
        if (error) {
            *error = error_type::JsonParseFailed;
        }
        return 0;
    }

    // Read line 3 (dataFileSize) for integrity verification
    line = metaFile.readStringUntil('\n');
    metaFile.close();

    size_t expectedDataFileSize = 0;
    if (line.length() > 0) {
        int equalPos2 = line.indexOf('=');
        if (equalPos2 != -1) {
            String sizeStr = line.substring(equalPos2 + 1);
            sizeStr.trim();
            expectedDataFileSize = sizeStr.toInt();
        }
    }

    // Verify data file integrity if size information is available
    if (expectedDataFileSize > 0) {
        String tocDataPath = get_toc_file_path();
        if (sdCard.file_exists(tocDataPath.c_str())) {
            fs::File dataFile = sdCard.open(tocDataPath.c_str(), FILE_READ);
            if (dataFile) {
                size_t actualSize = dataFile.size();
                dataFile.close();

                if (actualSize != expectedDataFileSize) {
                    log_e("Data file integrity check failed: expected %zu bytes, found %zu", expectedDataFileSize, actualSize);
                    if (error) {
                        *error = error_type::JsonParseFailed;
                    }
                    return 0;
                }
                log_i("Data file integrity verified");
            }
        } else {
            log_e("Data file missing, cannot verify integrity");
            if (error) {
                *error = error_type::SdCardFileNotFound;
            }
            return 0;
        }
    }

    log_i("Found %zu files in TOC metadata", fileCount);

    return fileCount;
}

google_drive_file google_drive::get_toc_file_by_index(sd_card& sdCard,
                                                      const String& filePath,
                                                      size_t index,
                                                      photo_frame_error_t* error) {
    if (error) {
        *error = error_type::None;
    }

    // Build path to data file
    String tocDataPath = get_toc_file_path();

    log_i("Reading TOC data from: %s", tocDataPath.c_str());

    fs::File dataFile = sdCard.open(tocDataPath.c_str(), FILE_READ);
    if (!dataFile) {
        log_e("Failed to open TOC data file: %s", tocDataPath.c_str());
        if (error) {
            *error = error_type::SdCardFileOpenFailed;
        }
        return google_drive_file();
    }

    // Skip to the desired index (no header lines in data file, just entries)
    for (size_t i = 0; i < index; i++) {
        String skipLine = dataFile.readStringUntil('\n');
        if (skipLine.length() == 0) {
            log_e("TOC data file ended before reaching index %zu", index);
            dataFile.close();
            if (error) {
                *error = error_type::JsonParseFailed;
            }
            return google_drive_file();
        }
    }

    // Read the target line
    String targetLine = dataFile.readStringUntil('\n');
    dataFile.close();

    if (targetLine.length() == 0) {
        log_e("No file entry found at index %zu", index);
        if (error) {
            *error = error_type::JsonParseFailed;
        }
        return google_drive_file("", "");
    }

    // Parse the line: id|name
    int pos1 = targetLine.indexOf('|');
    if (pos1 == -1) {
        log_e("Invalid file entry format: missing separator");
        if (error) {
            *error = error_type::JsonParseFailed;
        }
        return google_drive_file("", "");
    }

    String id   = targetLine.substring(0, pos1);
    String name = targetLine.substring(pos1 + 1);

    log_i("Retrieved file at index %zu: %s", index, name.c_str());

    return google_drive_file(id, name);
}

fs::File
google_drive::download_file(sd_card& sdCard, google_drive_file file, photo_frame_error_t* error) {
    fs::File emptyFile;

    // Initialize error to None
    if (error) {
        *error = error_type::None;
    }

    // Create final file path (download directly to cache, no temp file needed)
    String finalPath = get_cached_file_path(file.name);

    log_i("Downloading file: %s", file.name.c_str());
    log_i("Downloading directly to: %s", finalPath.c_str());

    // Ensure cache directory exists
    int lastSlash = finalPath.lastIndexOf('/');
    if (lastSlash > 0) {
        String dirPath = finalPath.substring(0, lastSlash);
        if (!sdCard.create_directories(dirPath.c_str())) {
            log_e("Failed to create cache directory: %s", dirPath.c_str());
            if (error) {
                *error = error_type::SdCardFileCreateFailed;
            }
            return emptyFile;
        }
    }

    // check if the current access token is still valid
    bool token_expired = client.is_token_expired(300);

    if (token_expired) {
        // Try to load cached access token first
        photo_frame_error_t access_token_error = load_access_token_from_file();
        if (access_token_error != error_type::None) {
            log_i("No valid cached token, getting new access token...");
            access_token_error = client.get_access_token();
            if (access_token_error != error_type::None) {
                log_e("Failed to get access token: %d", access_token_error.code);
            } else {
                // Save the new token for future use
                save_access_token_to_file();
            }
        } else {
            log_i("Using cached access token");
        }
    }

    // Open final file for writing (download directly, no temp file)
    fs::File finalFile = sdCard.open(finalPath.c_str(), FILE_WRITE, true);
    if (!finalFile) {
        log_e("Failed to create file: %s", finalPath.c_str());
        if (error) {
            *error = error_type::SdCardFileCreateFailed;
        }
        return emptyFile;
    }

    // Download file from Google Drive directly to final location
    photo_frame_error_t downloadError = client.download_file(file.id, &finalFile);

    // Ensure file is fully written to SD card before proceeding
    finalFile.flush();
    finalFile.close();

    // Small delay to ensure file system write completion
    delay(25);

    if (downloadError != error_type::None) {
        log_e("Failed to download file from Google Drive: %d", downloadError.code);

        // Clean up failed download file
        log_w("Cleaning up failed download: %s", finalPath.c_str());
        if (!sdCard.remove(finalPath.c_str())) {
            log_w("Warning: Failed to remove failed download");
        } else {
            log_i("Failed download cleaned up successfully");
        }

        if (error) {
            *error = downloadError;
        }

        return emptyFile;
    }

    log_i("File downloaded successfully");

    // Verify final file size (no rename needed - downloaded directly to final location)
    size_t finalFileSize = sdCard.get_file_size(finalPath.c_str());
    log_i("Downloaded file size: %zu bytes", finalFileSize);

    // Open and return the final file for reading
    fs::File readFile = sdCard.open(finalPath.c_str(), FILE_READ);
    if (!readFile) {
        log_e("Failed to open final file: %s", finalPath.c_str());
        if (error) {
            *error = error_type::SdCardFileOpenFailed;
        }
        return emptyFile;
    }

    // Double-check file size consistency
    size_t openedFileSize = readFile.size();
    log_i("Opened file size: %zu bytes", openedFileSize);

    if (finalFileSize != openedFileSize) {
        log_w("File size inconsistency! get_file_size=%zu, file.size()=%zu", finalFileSize, openedFileSize);
    }

    // Mark as downloaded from cloud since we actually downloaded it
    last_image_source = IMAGE_SOURCE_CLOUD;

    return readFile;
}

image_source_t google_drive::get_last_image_source() const { return last_image_source; }

void google_drive::set_last_image_source(image_source_t source) { last_image_source = source; }

String google_drive::load_root_ca_certificate(sd_card& sdCard, const char* rootCaPath) {
    log_i("Loading Google Drive root CA from: %s", rootCaPath);

    if (!sdCard.is_initialized()) {
        log_w("SD card not initialized, cannot load root CA");
        return String();
    }

    if (!sdCard.file_exists(rootCaPath)) {
        log_w("Root CA file not found: %s", rootCaPath);
        return String();
    }

    fs::File certFile = sdCard.open(rootCaPath, FILE_READ);
    if (!certFile) {
        log_e("Failed to open root CA file: %s", rootCaPath);
        return String();
    }

    size_t fileSize = certFile.size();
    if (fileSize > 4096) { // Reasonable limit for a certificate
        log_e("Root CA file too large: %zu bytes (max 4096)", fileSize);
        certFile.close();
        return String();
    }

    if (fileSize == 0) {
        log_w("Root CA file is empty");
        certFile.close();
        return String();
    }

    // Pre-allocate string for certificate content
    String certContent;
    certContent.reserve(fileSize + 1);
    certContent = certFile.readString();
    certFile.close();

    // Basic validation - check for PEM format markers
    if (!certContent.startsWith("-----BEGIN CERTIFICATE-----") ||
        !certContent.endsWith("-----END CERTIFICATE-----")) {
        log_e("Invalid certificate format - missing PEM markers");
        return String();
    }

#ifdef DEBUG_GOOGLE_DRIVE
    log_d("Successfully loaded root CA certificate (%zu bytes)", certContent.length());
#endif // DEBUG_GOOGLE_DRIVE

    return certContent;
}

uint32_t google_drive::cleanup_temporary_files(sd_card& sdCard, boolean force) {
    log_i("Cleaning up temporary files... (Local path: %s, Force: %d)", config.caching.local_path.c_str(), force);

    uint32_t cleanedCount = 0;

    if (!sdCard.is_initialized()) {
        log_w("SD card not initialized, cannot clean up");
        return 0;
    }

    // Get space information
    uint64_t usedBytes              = sdCard.used_bytes();
    uint64_t totalBytes             = sdCard.total_bytes();
    uint64_t freeBytes              = totalBytes - usedBytes;
    uint64_t twentyPercentThreshold = totalBytes * 20 / 100;
    bool lowSpace                   = freeBytes < twentyPercentThreshold;

    log_i("SD card space - Total: %llu MB, Free: %llu MB (%llu%%), Threshold: %llu MB",
          totalBytes / 1024 / 1024, freeBytes / 1024 / 1024, (freeBytes * 100) / totalBytes, twentyPercentThreshold / 1024 / 1024);

    // Always clean up temporary files from temp directory first
    log_i("Cleaning temp directory...");
    String tempDir = string_utils::build_path(config.caching.local_path, GOOGLE_DRIVE_TEMP_DIR);
    if (sdCard.file_exists(tempDir.c_str())) {
        fs::File tempRoot = sdCard.open(tempDir.c_str(), FILE_READ);
        if (tempRoot && tempRoot.isDirectory()) {
            fs::File tempFile = tempRoot.openNextFile();
            while (tempFile) {
                String tempFileName = tempFile.name();
                String tempFilePath = string_utils::build_path(tempDir, tempFileName);
                if (sdCard.remove(tempFilePath.c_str())) {
                    cleanedCount++;
                    log_i("Removed temp file: %s", tempFileName.c_str());
                }
                tempFile = tempRoot.openNextFile();
            }
            tempRoot.close();
        }
    }

    // Decision tree for cleanup strategy
    if (force) {
        log_i("FORCE CLEANUP: Removing everything");

        // 1. Remove access token
        String accessTokenPath = string_utils::build_path(config.caching.local_path, ACCESS_TOKEN_FILENAME);
        if (sdCard.file_exists(accessTokenPath.c_str()) && sdCard.remove(accessTokenPath.c_str())) {
            cleanedCount++;
            log_i("Removed access token");
        }

        // 2. Remove TOC files
        String tocDataPath = get_toc_file_path();
        String tocMetaPath = get_toc_meta_file_path();
        if (sdCard.file_exists(tocDataPath.c_str()) && sdCard.remove(tocDataPath.c_str())) {
            cleanedCount++;
            log_i("Removed TOC data file");
        }
        if (sdCard.file_exists(tocMetaPath.c_str()) && sdCard.remove(tocMetaPath.c_str())) {
            cleanedCount++;
            log_i("Removed TOC meta file");
        }

        // 3. Remove all cached images
        cleanedCount += cleanup_all_cached_images(sdCard);

    } else if (lowSpace) {
        log_i("LOW SPACE: Removing cached images only");
        cleanedCount += cleanup_all_cached_images(sdCard);

    } else {
        log_i("NORMAL CLEANUP: Checking TOC file conditions");

        String tocDataPath = get_toc_file_path();
        String tocMetaPath = get_toc_meta_file_path();
        bool tocDataExists = sdCard.file_exists(tocDataPath.c_str());
        bool tocMetaExists = sdCard.file_exists(tocMetaPath.c_str());

        if (!tocDataExists || !tocMetaExists) {
            log_i("TOC INTEGRITY: One file missing, removing both");
            if (tocDataExists && sdCard.remove(tocDataPath.c_str())) {
                cleanedCount++;
                log_i("Removed TOC data file");
            }
            if (tocMetaExists && sdCard.remove(tocMetaPath.c_str())) {
                cleanedCount++;
                log_i("Removed TOC meta file");
            }
        } else {
            // Both TOC files exist - check if expired
            time_t tocAge = sdCard.get_file_age(tocMetaPath.c_str());
            log_i("TOC age: %ld seconds, max: %lu", tocAge, config.caching.toc_max_age_seconds);

            if (tocAge > config.caching.toc_max_age_seconds) {
                log_i("TOC EXPIRED: Removing TOC files");
                if (sdCard.remove(tocDataPath.c_str())) {
                    cleanedCount++;
                    log_i("Removed expired TOC data file");
                }
                if (sdCard.remove(tocMetaPath.c_str())) {
                    cleanedCount++;
                    log_i("Removed expired TOC meta file");
                }
            } else {
                log_i("TOC files are fresh, keeping them");
            }
        }
    }

    log_i("Cleanup complete: %u files removed", cleanedCount);

    return cleanedCount;
}

uint32_t google_drive::cleanup_all_cached_images(sd_card& sdCard) {
    uint32_t cleanedCount = 0;
    String cacheDir       = string_utils::build_path(config.caching.local_path, GOOGLE_DRIVE_CACHE_DIR);

    if (!sdCard.file_exists(cacheDir.c_str())) {
        log_i("Cache directory doesn't exist");
        return 0;
    }

    fs::File cacheRoot = sdCard.open(cacheDir.c_str(), FILE_READ);
    if (!cacheRoot || !cacheRoot.isDirectory()) {
        log_e("Failed to open cache directory");
        return 0;
    }

    fs::File cacheFile = cacheRoot.openNextFile();
    while (cacheFile) {
        String fileName = cacheFile.name();
        String filePath = string_utils::build_path(cacheDir, fileName);

        if (sdCard.remove(filePath.c_str())) {
            cleanedCount++;
            log_i("Removed cached image: %s", fileName.c_str());
        } else {
            log_w("Failed to remove: %s", fileName.c_str());
        }
        cacheFile = cacheRoot.openNextFile();
        yield(); // Prevent watchdog timeout
    }
    cacheRoot.close();

    return cleanedCount;
}

photo_frame_error_t google_drive::save_access_token_to_file() {
    log_i("Saving access token to file...");

    const google_drive_access_token* token = client.get_access_token_value();
    if (!token || strlen(token->accessToken) == 0) {
        log_w("No valid access token to save");
        return error_type::TokenMissing;
    }

    String tokenPath   = string_utils::build_path(config.caching.local_path, ACCESS_TOKEN_FILENAME);

    fs::File tokenFile = SD_CARD_LIB.open(tokenPath.c_str(), FILE_WRITE);
    if (!tokenFile) {
        log_e("Failed to create token file: %s", tokenPath.c_str());
        return error_type::SdCardFileCreateFailed;
    }

    // Write token as JSON
    tokenFile.print(F("{\"access_token\":\""));
    tokenFile.print(token->accessToken);
    tokenFile.print(F("\",\"expires_at\":"));
    tokenFile.print(token->expiresAt);
    tokenFile.print(F(",\"obtained_at\":"));
    tokenFile.print(token->obtainedAt);
    tokenFile.println(F("}"));

    tokenFile.close();

    log_i("Access token saved to: %s", tokenPath.c_str());

    return error_type::None;
}

photo_frame_error_t google_drive::load_access_token_from_file() {
    log_i("Loading access token from file...");

    String tokenPath = string_utils::build_path(config.caching.local_path, ACCESS_TOKEN_FILENAME);

    if (!SD_CARD_LIB.exists(tokenPath.c_str())) {
        log_i("Token file does not exist: %s", tokenPath.c_str());
        return error_type::SdCardFileNotFound;
    }

    fs::File tokenFile = SD_CARD_LIB.open(tokenPath.c_str(), FILE_READ);
    if (!tokenFile) {
        log_e("Failed to open token file: %s", tokenPath.c_str());
        return error_type::SdCardFileOpenFailed;
    }

    String tokenContent = tokenFile.readString();
    tokenFile.close();

    // Parse JSON with static allocation
    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, tokenContent);

    if (error) {
        log_e("Failed to parse token file: %s", error.c_str());
        return error_type::JsonParseFailed;
    }

    if (!doc.containsKey("access_token") || !doc.containsKey("expires_at") ||
        !doc.containsKey("obtained_at")) {
        log_e("Invalid token file format");
        return error_type::JsonParseFailed;
    }

    google_drive_access_token token;
    strncpy(token.accessToken, doc["access_token"], sizeof(token.accessToken) - 1);
    token.accessToken[sizeof(token.accessToken) - 1] = '\0';
    token.expiresAt                                  = doc["expires_at"];
    token.obtainedAt                                 = doc["obtained_at"];

    // Check if token is still valid (with 5 minute margin)
    if (token.expired(300)) {
        log_i("Cached token has expired");
        return error_type::TokenMissing;
    }

    client.set_access_token(token);

    log_i("Access token loaded from: %s", tokenPath.c_str());
    log_i("Token expires in: %ld seconds", token.expires_in());

    return error_type::None;
}



photo_frame_error_t google_drive::initialize_from_unified_config(const unified_config::google_drive_config& gd_config) {
    // Validate essential configuration
    if (!gd_config.is_valid()) {
        log_e("Invalid Google Drive configuration in unified config");
        return error_type::ConfigMissingField;
    }

    // Create client configuration from unified config
    static google_drive_client_config client_config;

    // Convert String to const char* for compatibility
    static String email_str = gd_config.auth.service_account_email;
    static String key_str = gd_config.auth.private_key_pem;
    static String client_id_str = gd_config.auth.client_id;

    client_config.serviceAccountEmail = email_str.c_str();
    client_config.privateKeyPem = key_str.c_str();
    client_config.clientId = client_id_str.c_str();
    client_config.useInsecureTls = gd_config.drive.use_insecure_tls;

    // Pass rate limiting settings to client
    client_config.rateLimitWindowSeconds = gd_config.rate_limiting.rate_limit_window_seconds;
    client_config.minRequestDelayMs = gd_config.rate_limiting.min_request_delay_ms;
    client_config.maxRetryAttempts = gd_config.rate_limiting.max_retry_attempts;
    client_config.backoffBaseDelayMs = gd_config.rate_limiting.backoff_base_delay_ms;
    client_config.maxWaitTimeMs = gd_config.rate_limiting.max_wait_time_ms;

    // Initialize this instance's client
    client = google_drive_client(client_config);

    // Store unified configuration
    config = gd_config;

    // SSL certificate handling: Note that this method doesn't have sd_card access
    // The certificate loading would need to be handled separately if needed
    if (!gd_config.drive.use_insecure_tls && !gd_config.drive.root_ca_path.isEmpty()) {
        log_w("Warning: SSL certificate path specified but cannot load without SD card access");
        log_w("Consider using the JSON-based initialization for SSL certificate support");
    }

    log_i("Google Drive initialized from unified configuration");
    return error_type::None;
}

// Overloaded methods that use internal TOC path
size_t google_drive::get_toc_file_count(sd_card& sdCard, photo_frame_error_t* error) {
    String tocPath = get_toc_file_path();
    return get_toc_file_count(sdCard, tocPath, error);
}

google_drive_file
google_drive::get_toc_file_by_index(sd_card& sdCard, size_t index, photo_frame_error_t* error) {
    log_i("get_toc_file_by_index: %zu", index);

    String tocPath = get_toc_file_path();
    return get_toc_file_by_index(sdCard, tocPath, index, error);
}

google_drive_file google_drive::get_toc_file_by_name(sd_card& sdCard,
                                                     const char* filename,
                                                     photo_frame_error_t* error) {
    String tocPath = get_toc_file_path();
    google_drive_toc_parser parser(sdCard, tocPath.c_str());
    return parser.get_file_by_name(filename, error);
}


} // namespace photo_frame