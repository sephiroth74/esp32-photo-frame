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
#include <LittleFS.h>
#include <time.h>
#include <unistd.h> // For fsync()

#include "datetime_utils.h"
#include "google_drive.h"
#include "google_drive_toc_parser.h"
#include "sd_card.h"
#include "spi_manager.h"
#include "string_utils.h"

// Memory monitoring utility
void logMemoryUsage(const char* context) {
    size_t freeHeap    = ESP.getFreeHeap();
    size_t totalHeap   = ESP.getHeapSize();
    size_t usedHeap    = totalHeap - freeHeap;
    float usagePercent = (float)usedHeap / totalHeap * 100.0;

#ifdef DEBUG_MEMORY_USAGE
    Serial.print(F("[google_drive] Memory ["));
    Serial.print(context);
    Serial.print(F("]: Free="));
    Serial.print(freeHeap);
    Serial.print(F(" Used="));
    Serial.print(usedHeap);
    Serial.print(F(" ("));
    Serial.print(usagePercent, 1);
    Serial.println(F("%)"));
#endif // DEBUG_MEMORY_USAGE

    // Warning if memory usage is high
    if (usagePercent > 80.0) {
        Serial.println(F("[google_drive] WARNING: High memory usage detected!"));
    }
}

// Check if we have enough memory for JSON operations
bool checkMemoryAvailable(size_t requiredBytes) {
    size_t freeHeap     = ESP.getFreeHeap();
    size_t safetyMargin = 2048; // Keep 2KB safety margin

    if (freeHeap < (requiredBytes + safetyMargin)) {
        Serial.print(F("[google_drive] Insufficient memory: need "));
        Serial.print(requiredBytes);
        Serial.print(F(", have "));
        Serial.println(freeHeap);
        return false;
    }
    return true;
}

namespace photo_frame {

size_t google_drive::retrieve_toc(sd_card& sdCard, bool batteryConservationMode) {
    logMemoryUsage("TOC Retrieve Start");

    // Build TOC path using optimized utility function
    String tocFullPath        = get_toc_file_path();
    String tocDataPath        = get_toc_file_path();
    bool shouldFetchFromDrive = false;
    bool localTocExists =
        sdCard.file_exists(tocFullPath.c_str()) && sdCard.get_file_age(tocDataPath.c_str()) >= 0;

    if (localTocExists) {
        // Check file age
        time_t fileTime = sdCard.get_last_modified(tocFullPath.c_str());
        Serial.print(F("[google_drive] Local TOC file found: "));
        Serial.println(tocFullPath);
        Serial.print(F("[google_drive] TOC file last modified: "));
        Serial.println(fileTime);

        time_t currentTime = time(NULL);

        if (fileTime > 0 && currentTime > 0) {
            unsigned long fileAge = currentTime - fileTime;

            Serial.print(F("[google_drive] TOC file age: "));
            Serial.print(fileAge);
            Serial.print(F(" seconds, max age: "));
            Serial.println(config.tocMaxAgeSeconds);

            if (fileAge <= config.tocMaxAgeSeconds) {
                Serial.println(F("[google_drive] Using cached TOC file"));
                return get_toc_file_count(sdCard, tocFullPath);
            } else if (batteryConservationMode) {
                Serial.println(F("[google_drive] TOC file is old but using cached version due to "
                                 "battery conservation mode"));
                return get_toc_file_count(sdCard, tocFullPath);
            } else {
                Serial.println(
                    F("[google_drive] TOC file is too old, will try to fetch from Google Drive"));
                shouldFetchFromDrive = true;
            }
        } else if (batteryConservationMode) {
            Serial.println(F("[google_drive] Could not determine TOC file timestamp, but using "
                             "existing file due "
                             "to battery conservation mode"));
            return get_toc_file_count(sdCard, tocFullPath);
        } else {
            Serial.println(F("[google_drive] Could not determine TOC file timestamp, will try to "
                             "fetch from Google Drive"));
            shouldFetchFromDrive = true;
        }
    } else if (!localTocExists) {
        if (batteryConservationMode) {
            Serial.println(F("[google_drive] TOC file does not exist locally and battery "
                             "conservation mode is on "
                             "- returning 0"));
            return 0; // Return 0 to avoid network operation
        } else {
            Serial.println(
                F("[google_drive] TOC file does not exist locally, will fetch from Google Drive"));
            shouldFetchFromDrive = true;
        }
    }

    if (shouldFetchFromDrive) {
        Serial.println(F("[google_drive] Fetching TOC from Google Drive..."));

        // Try to load cached access token first
        photo_frame_error_t error = load_access_token_from_file();
        if (error != error_type::None) {
            Serial.println(F("[google_drive] No valid cached token, getting new access token..."));
            error = client.get_access_token();
            if (error != error_type::None) {
                Serial.print(F("[google_drive] Failed to get access token: "));
                Serial.println(error.code);
            } else {
                // Save the new token for future use
                save_access_token_to_file();
            }
        } else {
            Serial.println(F("[google_drive] Using cached access token"));
        }

        if (error != error_type::None) {

            // Fallback to local TOC if available and not forced
            if (localTocExists) {
                Serial.println(F("[google_drive] Falling back to cached TOC file"));
                return get_toc_file_count(sdCard, tocFullPath);
            }
            return 0; // Return 0 on error
        }

        // Check memory before Google Drive operations
        if (!checkMemoryAvailable(4096)) {
            Serial.println(F("[google_drive] Insufficient memory for Google Drive operations"));
            if (localTocExists) {
                Serial.println(F("[google_drive] Falling back to cached TOC file"));
                return get_toc_file_count(sdCard, tocFullPath);
            }
            return 0; // Return 0 on error
        }

        // Stream files directly from Google Drive to TOC file to save memory
        Serial.println(F("[google_drive] Fetching TOC from Google Drive..."));

        // Create paths for the 2-file TOC system
        String tocDataPath = get_toc_file_path();
        String tocMetaPath = get_toc_meta_file_path();

        Serial.print(F("[google_drive] Using 2-file TOC system: data="));
        Serial.print(tocDataPath);
        Serial.print(F(", meta="));
        Serial.println(tocMetaPath);

        // Remove existing data file to start fresh
        if (sdCard.file_exists(tocDataPath.c_str())) {
            sdCard.remove(tocDataPath.c_str());
            Serial.println(F("[google_drive] Removed existing TOC data file"));
        }

        // Stream files directly from Google Drive to TOC file to save memory
        size_t totalFiles = client.list_files_streaming(
            config.folderId.c_str(), sdCard, tocDataPath.c_str(), config.listPageSize);

        if (totalFiles == 0) {
            Serial.println(F("[google_drive] Failed to retrieve files from Google Drive"));
            // Remove incomplete data file
            sdCard.remove(tocDataPath.c_str());

            // Fallback to local TOC if available and not forced
            if (localTocExists) {
                Serial.println(F("[google_drive] Falling back to cached TOC file"));
                return get_toc_file_count(sdCard, tocFullPath);
            }
            return 0; // Return 0 on error
        }

        // Verify the file was actually written
        Serial.print(F("[google_drive] Verifying data file exists and has content..."));
        if (sdCard.file_exists(tocDataPath.c_str())) {
            fs::File verifyFile = sdCard.open(tocDataPath.c_str(), FILE_READ);
            if (verifyFile) {
                size_t actualSize = verifyFile.size();
                verifyFile.close();
                Serial.print(F(" Size: "));
                Serial.print(actualSize);
                Serial.println(F(" bytes"));

                if (actualSize == 0) {
                    Serial.println(F("[google_drive] ERROR: Data file is empty after write!"));
                    if (localTocExists) {
                        Serial.println(F("[google_drive] Falling back to cached TOC file"));
                        return get_toc_file_count(sdCard, tocFullPath);
                    }
                    return 0;
                }
            } else {
                Serial.println(F(" ERROR: Cannot open for verification"));
            }
        } else {
            Serial.println(F(" ERROR: File does not exist"));
        }

        if (totalFiles == 0) {
            Serial.println(F("[google_drive] Failed to retrieve files from Google Drive"));
            sdCard.remove(tocDataPath.c_str()); // Remove incomplete data file

            // Fallback to local TOC if available and not forced
            if (localTocExists) {
                Serial.println(F("[google_drive] Falling back to cached TOC file"));
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
            Serial.println(F("[google_drive] Failed to open TOC meta file for writing"));
            sdCard.remove(tocDataPath.c_str()); // Clean up data file
            if (localTocExists) {
                Serial.println(F("[google_drive] Falling back to cached TOC file"));
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

        Serial.print(F("[google_drive] TOC system complete: "));
        Serial.print(totalFiles);
        Serial.println(F(" files written to data file, metadata saved"));

        Serial.print(F("[google_drive] TOC file closed: "));
        Serial.println(tocFullPath);

        Serial.print(F("[google_drive] Retrieved "));
        Serial.print(totalFiles);
        Serial.println(F(" files from Google Drive"));

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
    if (sdCard.file_exists(config.localPath.c_str())) {
        if (sdCard.is_file(config.localPath.c_str())) {
            Serial.print(F("[google_drive] Removing file at path: "));
            Serial.println(config.localPath);
            if (!sdCard.remove(config.localPath.c_str())) {
                Serial.print(F("[google_drive] Failed to remove file at path: "));
                Serial.println(config.localPath);
                return photo_frame::error_type::SdCardDirCreateFailed;
            }
        } else {
            Serial.print(F("[google_drive] Root directory already exists: "));
            Serial.println(config.localPath);
        }
    }

    // Create root directory
    if (!sdCard.create_directories(config.localPath.c_str())) {
        Serial.print(
            F("[google_drive] Failed to create root directory for Google Drive local cache: "));
        Serial.println(config.localPath);
        return photo_frame::error_type::SdCardDirCreateFailed;
    }

    // Create temp subdirectory
    String tempDir = get_temp_dir_path();
    if (!sdCard.create_directories(tempDir.c_str())) {
        Serial.print(F("[google_drive] Failed to create temp directory: "));
        Serial.println(tempDir);
        return photo_frame::error_type::SdCardDirCreateFailed;
    }
    Serial.print(F("[google_drive] Created temp directory: "));
    Serial.println(tempDir);

    // Create cache subdirectory
    String cacheDir = get_cache_dir_path();
    if (!sdCard.create_directories(cacheDir.c_str())) {
        Serial.print(F("[google_drive] Failed to create cache directory: "));
        Serial.println(cacheDir);
        return photo_frame::error_type::SdCardDirCreateFailed;
    }
    Serial.print(F("[google_drive] Created cache directory: "));
    Serial.println(cacheDir);

    return photo_frame::error_type::None;
}

String google_drive::get_toc_file_path() const {
    return string_utils::build_path(config.localPath, TOC_DATA_FILENAME);
}

String google_drive::get_toc_meta_file_path() const {
    return string_utils::build_path(config.localPath, TOC_META_FILENAME);
}

String google_drive::get_temp_dir_path() const {
    return string_utils::build_path(config.localPath, GOOGLE_DRIVE_TEMP_DIR);
}

String google_drive::get_cache_dir_path() const {
    return string_utils::build_path(config.localPath, GOOGLE_DRIVE_CACHE_DIR);
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

    Serial.print(F("[google_drive] Reading TOC metadata from: "));
    Serial.println(tocMetaPath);

    fs::File metaFile = sdCard.open(tocMetaPath.c_str(), FILE_READ);
    if (!metaFile) {
        Serial.print(F("[google_drive] Failed to open TOC metadata file: "));
        Serial.println(tocMetaPath);
        if (error) {
            *error = error_type::SdCardFileOpenFailed;
        }
        return 0;
    }

    // Read line 1 (timestamp)
    String line = metaFile.readStringUntil('\n');
    if (line.length() == 0) {
        Serial.println(F("[google_drive] TOC metadata file is empty or invalid"));
        metaFile.close();
        if (error) {
            *error = error_type::JsonParseFailed;
        }
        return 0;
    }

    // Read line 2 (fileCount)
    line = metaFile.readStringUntil('\n');
    if (line.length() == 0) {
        Serial.println(F("[google_drive] TOC metadata file missing fileCount line"));
        metaFile.close();
        if (error) {
            *error = error_type::JsonParseFailed;
        }
        return 0;
    }

    // Parse "fileCount = <number>"
    int equalPos = line.indexOf('=');
    if (equalPos == -1) {
        Serial.println(
            F("[google_drive] Invalid TOC metadata format: missing '=' in fileCount line"));
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
        Serial.println(F("[google_drive] Invalid fileCount value in TOC metadata"));
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
                    Serial.print(F("[google_drive] Data file integrity check failed: expected "));
                    Serial.print(expectedDataFileSize);
                    Serial.print(F(" bytes, found "));
                    Serial.println(actualSize);
                    if (error) {
                        *error = error_type::JsonParseFailed;
                    }
                    return 0;
                }
                Serial.println(F("[google_drive] Data file integrity verified"));
            }
        } else {
            Serial.println(F("[google_drive] Data file missing, cannot verify integrity"));
            if (error) {
                *error = error_type::SdCardFileNotFound;
            }
            return 0;
        }
    }

    Serial.print(F("[google_drive] Found "));
    Serial.print(fileCount);
    Serial.println(F(" files in TOC metadata"));

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

    Serial.print(F("[google_drive] Reading TOC data from: "));
    Serial.println(tocDataPath);

    fs::File dataFile = sdCard.open(tocDataPath.c_str(), FILE_READ);
    if (!dataFile) {
        Serial.print(F("[google_drive] Failed to open TOC data file: "));
        Serial.println(tocDataPath);
        if (error) {
            *error = error_type::SdCardFileOpenFailed;
        }
        return google_drive_file();
    }

    // Skip to the desired index (no header lines in data file, just entries)
    for (size_t i = 0; i < index; i++) {
        String skipLine = dataFile.readStringUntil('\n');
        if (skipLine.length() == 0) {
            Serial.print(F("[google_drive] TOC data file ended before reaching index "));
            Serial.println(index);
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
        Serial.print(F("[google_drive] No file entry found at index "));
        Serial.println(index);
        if (error) {
            *error = error_type::JsonParseFailed;
        }
        return google_drive_file("", "");
    }

    // Parse the line: id|name
    int pos1 = targetLine.indexOf('|');
    if (pos1 == -1) {
        Serial.println(F("[google_drive] Invalid file entry format: missing separator"));
        if (error) {
            *error = error_type::JsonParseFailed;
        }
        return google_drive_file("", "");
    }

    String id   = targetLine.substring(0, pos1);
    String name = targetLine.substring(pos1 + 1);

    Serial.print(F("[google_drive] Retrieved file at index "));
    Serial.print(index);
    Serial.print(F(": "));
    Serial.println(name);

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

    Serial.print(F("[google_drive] Downloading file: "));
    Serial.println(file.name);
    Serial.print(F("[google_drive] Downloading directly to: "));
    Serial.println(finalPath);

    // Ensure cache directory exists
    int lastSlash = finalPath.lastIndexOf('/');
    if (lastSlash > 0) {
        String dirPath = finalPath.substring(0, lastSlash);
        if (!sdCard.create_directories(dirPath.c_str())) {
            Serial.print(F("[google_drive] Failed to create cache directory: "));
            Serial.println(dirPath);
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
            Serial.println(F("[google_drive] No valid cached token, getting new access token..."));
            access_token_error = client.get_access_token();
            if (access_token_error != error_type::None) {
                Serial.print(F("[google_drive] Failed to get access token: "));
                Serial.println(access_token_error.code);
            } else {
                // Save the new token for future use
                save_access_token_to_file();
            }
        } else {
            Serial.println(F("[google_drive] Using cached access token"));
        }
    }

    // Open final file for writing (download directly, no temp file)
    fs::File finalFile = sdCard.open(finalPath.c_str(), FILE_WRITE, true);
    if (!finalFile) {
        Serial.print(F("[google_drive] Failed to create file: "));
        Serial.println(finalPath);
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
        Serial.print(F("[google_drive] Failed to download file from Google Drive: "));
        Serial.println(downloadError.code);

        // Clean up failed download file
        Serial.print(F("[google_drive] Cleaning up failed download: "));
        Serial.println(finalPath);
        if (!sdCard.remove(finalPath.c_str())) {
            Serial.println(F("[google_drive] Warning: Failed to remove failed download"));
        } else {
            Serial.println(F("[google_drive] Failed download cleaned up successfully"));
        }

        if (error) {
            *error = downloadError;
        }

        return emptyFile;
    }

    Serial.println(F("[google_drive] File downloaded successfully"));

    // Verify final file size (no rename needed - downloaded directly to final location)
    size_t finalFileSize = sdCard.get_file_size(finalPath.c_str());
    Serial.print(F("[google_drive] Downloaded file size: "));
    Serial.print(finalFileSize);
    Serial.println(F(" bytes"));

    // Open and return the final file for reading
    fs::File readFile = sdCard.open(finalPath.c_str(), FILE_READ);
    if (!readFile) {
        Serial.print(F("[google_drive] Failed to open final file: "));
        Serial.println(finalPath);
        if (error) {
            *error = error_type::SdCardFileOpenFailed;
        }
        return emptyFile;
    }

    // Double-check file size consistency
    size_t openedFileSize = readFile.size();
    Serial.print(F("[google_drive] Opened file size: "));
    Serial.print(openedFileSize);
    Serial.println(F(" bytes"));

    if (finalFileSize != openedFileSize) {
        Serial.printf(
            "[google_drive] ⚠️ File size inconsistency! get_file_size=%zu, file.size()=%zu\n",
            finalFileSize,
            openedFileSize);
    }

    // Mark as downloaded from cloud since we actually downloaded it
    last_image_source = IMAGE_SOURCE_CLOUD;

    return readFile;
}

image_source_t google_drive::get_last_image_source() const { return last_image_source; }

void google_drive::set_last_image_source(image_source_t source) { last_image_source = source; }

String google_drive::load_root_ca_certificate(sd_card& sdCard, const char* rootCaPath) {
    Serial.print(F("[google_drive] Loading Google Drive root CA from: "));
    Serial.println(rootCaPath);

    if (!sdCard.is_initialized()) {
        Serial.println(F("[google_drive] SD card not initialized, cannot load root CA"));
        return String();
    }

    if (!sdCard.file_exists(rootCaPath)) {
        Serial.print(F("[google_drive] Root CA file not found: "));
        Serial.println(rootCaPath);
        return String();
    }

    fs::File certFile = sdCard.open(rootCaPath, FILE_READ);
    if (!certFile) {
        Serial.print(F("[google_drive] Failed to open root CA file: "));
        Serial.println(rootCaPath);
        return String();
    }

    size_t fileSize = certFile.size();
    if (fileSize > 4096) { // Reasonable limit for a certificate
        Serial.print(F("[google_drive] Root CA file too large: "));
        Serial.print(fileSize);
        Serial.println(F(" bytes (max 4096)"));
        certFile.close();
        return String();
    }

    if (fileSize == 0) {
        Serial.println(F("[google_drive] Root CA file is empty"));
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
        Serial.println(F("[google_drive] Invalid certificate format - missing PEM markers"));
        return String();
    }

#ifdef DEBUG_GOOGLE_DRIVE
    Serial.print(F("[google_drive] Successfully loaded root CA certificate ("));
    Serial.print(certContent.length());
    Serial.println(F(" bytes)"));
#endif // DEBUG_GOOGLE_DRIVE

    return certContent;
}

uint32_t google_drive::cleanup_temporary_files(sd_card& sdCard,
                                               const google_drive_json_config& config,
                                               boolean force) {
    Serial.print(F("[google_drive] Cleaning up temporary files... ("));
    Serial.print(F("Local path: "));
    Serial.print(config.localPath);
    Serial.print(F(", Force: "));
    Serial.print(force);
    Serial.println(F(")"));

    uint32_t cleanedCount = 0;

    if (!sdCard.is_initialized()) {
        Serial.println(F("[google_drive] SD card not initialized, cannot clean up"));
        return 0;
    }

    // Get space information
    uint64_t usedBytes              = sdCard.used_bytes();
    uint64_t totalBytes             = sdCard.total_bytes();
    uint64_t freeBytes              = totalBytes - usedBytes;
    uint64_t twentyPercentThreshold = totalBytes * 20 / 100;
    bool lowSpace                   = freeBytes < twentyPercentThreshold;

    Serial.print(F("[google_drive] SD card space - Total: "));
    Serial.print(totalBytes / 1024 / 1024);
    Serial.print(F(" MB, Free: "));
    Serial.print(freeBytes / 1024 / 1024);
    Serial.print(F(" MB ("));
    Serial.print((freeBytes * 100) / totalBytes);
    Serial.print(F("%), Threshold: "));
    Serial.print(twentyPercentThreshold / 1024 / 1024);
    Serial.println(F(" MB"));

    // Always clean up temporary files from temp directory first
    Serial.println(F("[google_drive] Cleaning temp directory..."));
    String tempDir = string_utils::build_path(config.localPath, GOOGLE_DRIVE_TEMP_DIR);
    if (sdCard.file_exists(tempDir.c_str())) {
        fs::File tempRoot = sdCard.open(tempDir.c_str(), FILE_READ);
        if (tempRoot && tempRoot.isDirectory()) {
            fs::File tempFile = tempRoot.openNextFile();
            while (tempFile) {
                String tempFileName = tempFile.name();
                String tempFilePath = string_utils::build_path(tempDir, tempFileName);
                if (sdCard.remove(tempFilePath.c_str())) {
                    cleanedCount++;
                    Serial.print(F("[google_drive] Removed temp file: "));
                    Serial.println(tempFileName);
                }
                tempFile = tempRoot.openNextFile();
            }
            tempRoot.close();
        }
    }

    // Decision tree for cleanup strategy
    if (force) {
        Serial.println(F("[google_drive] FORCE CLEANUP: Removing everything"));

        // 1. Remove access token
        String accessTokenPath = string_utils::build_path(config.localPath, ACCESS_TOKEN_FILENAME);
        if (sdCard.file_exists(accessTokenPath.c_str()) && sdCard.remove(accessTokenPath.c_str())) {
            cleanedCount++;
            Serial.println(F("[google_drive] Removed access token"));
        }

        // 2. Remove TOC files
        String tocDataPath = get_toc_file_path();
        String tocMetaPath = get_toc_meta_file_path();
        if (sdCard.file_exists(tocDataPath.c_str()) && sdCard.remove(tocDataPath.c_str())) {
            cleanedCount++;
            Serial.println(F("[google_drive] Removed TOC data file"));
        }
        if (sdCard.file_exists(tocMetaPath.c_str()) && sdCard.remove(tocMetaPath.c_str())) {
            cleanedCount++;
            Serial.println(F("[google_drive] Removed TOC meta file"));
        }

        // 3. Remove all cached images
        cleanedCount += cleanup_all_cached_images(sdCard, config);

    } else if (lowSpace) {
        Serial.println(F("[google_drive] LOW SPACE: Removing cached images only"));
        cleanedCount += cleanup_all_cached_images(sdCard, config);

    } else {
        Serial.println(F("[google_drive] NORMAL CLEANUP: Checking TOC file conditions"));

        String tocDataPath = get_toc_file_path();
        String tocMetaPath = get_toc_meta_file_path();
        bool tocDataExists = sdCard.file_exists(tocDataPath.c_str());
        bool tocMetaExists = sdCard.file_exists(tocMetaPath.c_str());

        if (!tocDataExists || !tocMetaExists) {
            Serial.println(F("[google_drive] TOC INTEGRITY: One file missing, removing both"));
            if (tocDataExists && sdCard.remove(tocDataPath.c_str())) {
                cleanedCount++;
                Serial.println(F("[google_drive] Removed TOC data file"));
            }
            if (tocMetaExists && sdCard.remove(tocMetaPath.c_str())) {
                cleanedCount++;
                Serial.println(F("[google_drive] Removed TOC meta file"));
            }
        } else {
            // Both TOC files exist - check if expired
            time_t tocAge = sdCard.get_file_age(tocMetaPath.c_str());
            Serial.print(F("[google_drive] TOC age: "));
            Serial.print(tocAge);
            Serial.print(F(" seconds, max: "));
            Serial.println(config.tocMaxAgeSeconds);

            if (tocAge > config.tocMaxAgeSeconds) {
                Serial.println(F("[google_drive] TOC EXPIRED: Removing TOC files"));
                if (sdCard.remove(tocDataPath.c_str())) {
                    cleanedCount++;
                    Serial.println(F("[google_drive] Removed expired TOC data file"));
                }
                if (sdCard.remove(tocMetaPath.c_str())) {
                    cleanedCount++;
                    Serial.println(F("[google_drive] Removed expired TOC meta file"));
                }
            } else {
                Serial.println(F("[google_drive] TOC files are fresh, keeping them"));
            }
        }
    }

    Serial.print(F("[google_drive] Cleanup complete: "));
    Serial.print(cleanedCount);
    Serial.println(F(" files removed"));

    return cleanedCount;
}

uint32_t google_drive::cleanup_all_cached_images(sd_card& sdCard,
                                                 const google_drive_json_config& config) {
    uint32_t cleanedCount = 0;
    String cacheDir       = string_utils::build_path(config.localPath, GOOGLE_DRIVE_CACHE_DIR);

    if (!sdCard.file_exists(cacheDir.c_str())) {
        Serial.println(F("[google_drive] Cache directory doesn't exist"));
        return 0;
    }

    fs::File cacheRoot = sdCard.open(cacheDir.c_str(), FILE_READ);
    if (!cacheRoot || !cacheRoot.isDirectory()) {
        Serial.println(F("[google_drive] Failed to open cache directory"));
        return 0;
    }

    fs::File cacheFile = cacheRoot.openNextFile();
    while (cacheFile) {
        String fileName = cacheFile.name();
        String filePath = string_utils::build_path(cacheDir, fileName);

        if (sdCard.remove(filePath.c_str())) {
            cleanedCount++;
            Serial.print(F("[google_drive] Removed cached image: "));
            Serial.println(fileName);
        } else {
            Serial.print(F("[google_drive] Failed to remove: "));
            Serial.println(fileName);
        }
        cacheFile = cacheRoot.openNextFile();
        yield(); // Prevent watchdog timeout
    }
    cacheRoot.close();

    return cleanedCount;
}

photo_frame_error_t google_drive::save_access_token_to_file() {
    Serial.println(F("[google_drive] Saving access token to file..."));

    const google_drive_access_token* token = client.get_access_token_value();
    if (!token || strlen(token->accessToken) == 0) {
        Serial.println(F("[google_drive] No valid access token to save"));
        return error_type::TokenMissing;
    }

    String tokenPath   = string_utils::build_path(config.localPath, ACCESS_TOKEN_FILENAME);

    fs::File tokenFile = SD_MMC.open(tokenPath.c_str(), FILE_WRITE);
    if (!tokenFile) {
        Serial.print(F("[google_drive] Failed to create token file: "));
        Serial.println(tokenPath);
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

    Serial.print(F("[google_drive] Access token saved to: "));
    Serial.println(tokenPath);

    return error_type::None;
}

photo_frame_error_t google_drive::load_access_token_from_file() {
    Serial.println(F("[google_drive] Loading access token from file..."));

    String tokenPath = string_utils::build_path(config.localPath, ACCESS_TOKEN_FILENAME);

    if (!SD_MMC.exists(tokenPath.c_str())) {
        Serial.print(F("[google_drive] Token file does not exist: "));
        Serial.println(tokenPath);
        return error_type::SdCardFileNotFound;
    }

    fs::File tokenFile = SD_MMC.open(tokenPath.c_str(), FILE_READ);
    if (!tokenFile) {
        Serial.print(F("[google_drive] Failed to open token file: "));
        Serial.println(tokenPath);
        return error_type::SdCardFileOpenFailed;
    }

    String tokenContent = tokenFile.readString();
    tokenFile.close();

    // Parse JSON with static allocation
    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, tokenContent);

    if (error) {
        Serial.print(F("[google_drive] Failed to parse token file: "));
        Serial.println(error.c_str());
        return error_type::JsonParseFailed;
    }

    if (!doc.containsKey("access_token") || !doc.containsKey("expires_at") ||
        !doc.containsKey("obtained_at")) {
        Serial.println(F("[google_drive] Invalid token file format"));
        return error_type::JsonParseFailed;
    }

    google_drive_access_token token;
    strncpy(token.accessToken, doc["access_token"], sizeof(token.accessToken) - 1);
    token.accessToken[sizeof(token.accessToken) - 1] = '\0';
    token.expiresAt                                  = doc["expires_at"];
    token.obtainedAt                                 = doc["obtained_at"];

    // Check if token is still valid (with 5 minute margin)
    if (token.expired(300)) {
        Serial.println(F("[google_drive] Cached token has expired"));
        return error_type::TokenMissing;
    }

    client.set_access_token(token);

    Serial.print(F("[google_drive] Access token loaded from: "));
    Serial.println(tokenPath);
    Serial.print(F("[google_drive] Token expires in: "));
    Serial.print(token.expires_in());
    Serial.println(F(" seconds"));

    return error_type::None;
}

photo_frame_error_t load_google_drive_config_from_json(sd_card& sd_card,
                                                       const char* config_filepath,
                                                       google_drive_json_config& config) {
    if (!sd_card.is_initialized()) {
        Serial.println(F("[google_drive] SD card not initialized"));
        return error_type::CardMountFailed;
    }

    fs::File configFile = sd_card.open(config_filepath, FILE_READ);
    if (!configFile) {
        Serial.print(F("[google_drive] Failed to open Google Drive config file: "));
        Serial.println(config_filepath);
        return error_type::FileOpenFailed;
    }

    size_t fileSize = configFile.size();
    if (fileSize == 0) {
        Serial.println(F("[google_drive] Google Drive config file is empty"));
        configFile.close();
        return error_type::JsonParseFailed;
    }

    // Check if we have enough memory for JSON operations
    size_t requiredMemory = fileSize + 2048; // File size + JSON parsing overhead
    if (!checkMemoryAvailable(requiredMemory)) {
        configFile.close();
        return error_type::JsonParseFailed;
    }

    logMemoryUsage("Config Load Start");

    StaticJsonDocument<2048> doc;

    DeserializationError jsonError = deserializeJson(doc, configFile);
    configFile.close();

    if (jsonError) {
        Serial.print(F("[google_drive] Failed to parse Google Drive config JSON: "));
        Serial.println(jsonError.c_str());
        return error_type::JsonParseFailed;
    }

    // Validate and extract authentication settings
    if (!doc["authentication"].isNull()) {
        JsonObject auth            = doc["authentication"];
        config.serviceAccountEmail = auth["service_account_email"].as<String>();
        config.privateKeyPem       = auth["private_key_pem"].as<String>();
        config.clientId            = auth["client_id"].as<String>();

        // Validate service account email
        if (config.serviceAccountEmail.isEmpty()) {
            Serial.println(
                F("[google_drive] Validation failed: service_account_email is required"));
            return error_type::ConfigMissingField;
        }
        if (config.serviceAccountEmail.indexOf('@') == -1 ||
            config.serviceAccountEmail.indexOf('.') == -1) {
            Serial.println(F("[google_drive] Validation failed: service_account_email must be a "
                             "valid email address"));
            return error_type::ConfigInvalidEmail;
        }

        // Validate private key PEM format
        if (config.privateKeyPem.isEmpty()) {
            Serial.println(F("[google_drive] Validation failed: private_key_pem is required"));
            return error_type::ConfigMissingField;
        }
        if (!config.privateKeyPem.startsWith("-----BEGIN PRIVATE KEY-----")) {
            Serial.println(
                F("[google_drive] Validation failed: private_key_pem must be in PEM format"));
            return error_type::ConfigInvalidPemKey;
        }
        if (!config.privateKeyPem.endsWith("-----END PRIVATE KEY-----") &&
            !config.privateKeyPem.endsWith("-----END PRIVATE KEY-----\n")) {
            Serial.println(F("[google_drive] Validation failed: private_key_pem must end with "
                             "proper PEM footer"));
            return error_type::ConfigInvalidPemKey;
        }

        // Validate client ID
        if (config.clientId.isEmpty()) {
            Serial.println(F("[google_drive] Validation failed: client_id is required"));
            return error_type::ConfigMissingField;
        }
        if (config.clientId.length() < 10) {
            Serial.println(
                F("[google_drive] Validation failed: client_id appears to be too short"));
            return error_type::ConfigValueOutOfRange;
        }
    } else {
        Serial.println(F("[google_drive] Validation failed: authentication section is required"));
        return error_type::ConfigMissingSection;
    }

    // Validate and extract drive settings
    if (!doc["drive"].isNull()) {
        JsonObject drive      = doc["drive"];
        config.folderId       = drive["folder_id"].as<String>();
        config.rootCaPath     = drive["root_ca_path"].as<String>();
        config.listPageSize   = drive["list_page_size"].as<int>();
        config.useInsecureTls = drive["use_insecure_tls"].as<bool>();

        // Validate folder ID
        if (config.folderId.isEmpty()) {
            Serial.println(F("[google_drive] Validation failed: folder_id is required"));
            return error_type::ConfigMissingField;
        }
        if (config.folderId.length() < 20) {
            Serial.println(
                F("[google_drive] Validation failed: folder_id appears to be too short"));
            return error_type::ConfigValueOutOfRange;
        }

        // Validate root CA path
        if (!config.rootCaPath.isEmpty()) {
            if (!config.rootCaPath.startsWith("/")) {
                Serial.println(
                    F("[google_drive] Validation failed: root_ca_path must be an absolute path"));
                return error_type::ConfigInvalidPath;
            }
            if (!config.rootCaPath.endsWith(".pem")) {
                Serial.println(
                    F("[google_drive] Validation failed: root_ca_path must point to a .pem file"));
                return error_type::ConfigInvalidPath;
            }
        } else if (!config.useInsecureTls) {
            Serial.println(F("[google_drive] Validation failed: root_ca_path is required when "
                             "use_insecure_tls is false"));
            return error_type::ConfigMissingField;
        }

        // Validate list page size
        if (config.listPageSize <= 0 || config.listPageSize > GOOGLE_DRIVE_MAX_LIST_PAGE_SIZE) {
            Serial.print(
                F("[google_drive] Validation failed: list_page_size must be between 1 and "));
            Serial.println(GOOGLE_DRIVE_MAX_LIST_PAGE_SIZE);

            // do not throw an exception, just update the config with
            // GOOGLE_DRIVE_MAX_LIST_PAGE_SIZE
            config.listPageSize =
                constrain(config.listPageSize, 1, GOOGLE_DRIVE_MAX_LIST_PAGE_SIZE);
            Serial.print(F("[google_drive] Updated list_page_size to "));
            Serial.println(config.listPageSize);
        }

        // use_insecure_tls is automatically validated as boolean by ArduinoJson
    } else {
        Serial.println(F("[google_drive] Validation failed: drive section is required"));
        return error_type::ConfigMissingSection;
    }

    // Validate and extract caching settings
    if (!doc["caching"].isNull()) {
        JsonObject caching      = doc["caching"];
        config.localPath        = caching["local_path"].as<String>();
        config.tocMaxAgeSeconds = caching["toc_max_age_seconds"].as<unsigned long>();

        // Validate local path
        if (config.localPath.isEmpty()) {
            Serial.println(F("[google_drive] Validation failed: local_path is required"));
            return error_type::ConfigMissingField;
        }
        if (!config.localPath.startsWith("/")) {
            Serial.println(
                F("[google_drive] Validation failed: local_path must be an absolute path"));
            return error_type::ConfigInvalidPath;
        }
        if (config.localPath.endsWith("/")) {
            Serial.println(F("[google_drive] Validation failed: local_path must not end with '/'"));
            return error_type::ConfigInvalidPath;
        }

        // Validate TOC max age (1 second to 30 days)
        if (config.tocMaxAgeSeconds == 0 ||
            config.tocMaxAgeSeconds > GOOGLE_DRIVE_TOC_MAX_AGE_SECONDS) {
            Serial.println(F("[google_drive] Validation failed: toc_max_age_seconds must be "
                             "between 1 and 2592000 (30 days)"));
            return error_type::ConfigValueOutOfRange;
        }
    } else {
        Serial.println(F("[google_drive] Validation failed: caching section is required"));
        return error_type::ConfigMissingSection;
    }

    // Validate and extract rate limiting settings
    if (!doc["rate_limiting"].isNull()) {
        JsonObject rateLimiting       = doc["rate_limiting"];
        config.maxRequestsPerWindow   = rateLimiting["max_requests_per_window"].as<int>();
        config.rateLimitWindowSeconds = rateLimiting["rate_limit_window_seconds"].as<int>();
        config.minRequestDelayMs      = rateLimiting["min_request_delay_ms"].as<int>();
        config.maxRetryAttempts       = rateLimiting["max_retry_attempts"].as<int>();
        config.backoffBaseDelayMs     = rateLimiting["backoff_base_delay_ms"].as<int>();
        config.maxWaitTimeMs          = rateLimiting["max_wait_time_ms"].as<int>();

        // Validate max requests per window
        if (config.maxRequestsPerWindow <= 0 ||
            config.maxRequestsPerWindow > GOOGLE_DRIVE_MAX_REQUESTS_PER_WINDOW) {
            Serial.print(F("[google_drive] Validation failed: max_requests_per_window must be "
                           "between 1 and "));
            Serial.println(GOOGLE_DRIVE_MAX_REQUESTS_PER_WINDOW);
            return error_type::ConfigValueOutOfRange;
        }

        // Validate rate limit window seconds
        if (config.rateLimitWindowSeconds <= 0 ||
            config.rateLimitWindowSeconds > GOOGLE_DRIVE_RATE_LIMIT_WINDOW_SECONDS) {
            Serial.print(F("[google_drive] Validation failed: rate_limit_window_seconds must be "
                           "between 1 and "));
            Serial.println(GOOGLE_DRIVE_RATE_LIMIT_WINDOW_SECONDS);
            return error_type::ConfigValueOutOfRange;
        }

        // Validate minimum request delay
        if (config.minRequestDelayMs < 0 ||
            config.minRequestDelayMs > GOOGLE_DRIVE_MIN_REQUEST_DELAY_MS) {
            Serial.print(
                F("[google_drive] Validation failed: min_request_delay_ms must be between 0 and "));
            Serial.println(GOOGLE_DRIVE_MIN_REQUEST_DELAY_MS);
            return error_type::ConfigValueOutOfRange;
        }

        // Validate max retry attempts
        if (config.maxRetryAttempts < 0 ||
            config.maxRetryAttempts > GOOGLE_DRIVE_MAX_RETRY_ATTEMPTS) {
            Serial.print(
                F("[google_drive] Validation failed: max_retry_attempts must be between 0 and "));
            Serial.println(GOOGLE_DRIVE_MAX_RETRY_ATTEMPTS);
            return error_type::ConfigValueOutOfRange;
        }

        // Validate backoff base delay
        if (config.backoffBaseDelayMs <= 0 ||
            config.backoffBaseDelayMs > GOOGLE_DRIVE_BACKOFF_BASE_DELAY_MS) {
            Serial.print(F(
                "[google_drive] Validation failed: backoff_base_delay_ms must be between 1 and "));
            Serial.println(GOOGLE_DRIVE_BACKOFF_BASE_DELAY_MS);
            return error_type::ConfigValueOutOfRange;
        }

        // Validate max wait time
        if (config.maxWaitTimeMs <= 0 || config.maxWaitTimeMs > GOOGLE_DRIVE_MAX_WAIT_TIME_MS) {
            Serial.print(
                F("[google_drive] Validation failed: max_wait_time_ms must be between 1 and "));
            Serial.println(GOOGLE_DRIVE_MAX_WAIT_TIME_MS);
            return error_type::ConfigValueOutOfRange;
        }

        // Cross-validation: backoff base delay should be less than max wait time
        if (config.backoffBaseDelayMs >= config.maxWaitTimeMs) {
            Serial.println(F("[google_drive] Validation failed: backoff_base_delay_ms must be less "
                             "than max_wait_time_ms"));
            return error_type::ConfigValueOutOfRange;
        }
    } else {
        Serial.println(F("[google_drive] Validation failed: rate_limiting section is required"));
        return error_type::ConfigMissingSection;
    }

    Serial.print(F("[google_drive] Successfully loaded and validated Google Drive config from: "));
    Serial.println(config_filepath);
    Serial.print(F("[google_drive] Service Account: "));
    Serial.println(config.serviceAccountEmail);
    Serial.print(F("[google_drive] Folder ID: "));
    Serial.println(config.folderId);
    Serial.print(F("[google_drive] Use Insecure TLS: "));
    Serial.println(config.useInsecureTls ? "true" : "false");
    Serial.print(F("[google_drive] Local Cache Path: "));
    Serial.println(config.localPath);
    Serial.print(F("[google_drive] TOC Max Age (seconds): "));
    Serial.println(config.tocMaxAgeSeconds);

    logMemoryUsage("Config Load End");
    return error_type::None;
}

photo_frame_error_t google_drive::initialize_from_json(sd_card& sd_card,
                                                       const char* config_filepath) {
    // Load JSON configuration
    google_drive_json_config json_config;
    photo_frame_error_t load_result =
        load_google_drive_config_from_json(sd_card, config_filepath, json_config);
    if (load_result != error_type::None) {
        Serial.print(F("[google_drive] Failed to load Google Drive config: "));
        Serial.println(load_result.code);
        return load_result;
    }

    // Create client configuration from JSON
    static google_drive_client_config client_config;
    // Convert String to const char* for compatibility
    static String email_str           = json_config.serviceAccountEmail;
    static String key_str             = json_config.privateKeyPem;
    static String client_id_str       = json_config.clientId;

    client_config.serviceAccountEmail = email_str.c_str();
    client_config.privateKeyPem       = key_str.c_str();
    client_config.clientId            = client_id_str.c_str();
    client_config.useInsecureTls      = json_config.useInsecureTls;

    // Pass rate limiting settings to client
    client_config.rateLimitWindowSeconds = json_config.rateLimitWindowSeconds;
    client_config.minRequestDelayMs      = json_config.minRequestDelayMs;
    client_config.maxRetryAttempts       = json_config.maxRetryAttempts;
    client_config.backoffBaseDelayMs     = json_config.backoffBaseDelayMs;
    client_config.maxWaitTimeMs          = json_config.maxWaitTimeMs;

    // Initialize this instance's client and config
    client = google_drive_client(client_config);
    config = json_config;

    // Load SSL certificate if not using insecure TLS
    if (!json_config.useInsecureTls && !json_config.rootCaPath.isEmpty()) {
        fs::File certFile = sd_card.open(json_config.rootCaPath.c_str(), FILE_READ);
        if (certFile) {
            String cert = certFile.readString();
            certFile.close();
            if (!json_config.useInsecureTls) {
                client.set_root_ca_certificate(cert);
            }
            Serial.print(F("[google_drive] Loaded SSL certificate from: "));
            Serial.println(json_config.rootCaPath);
        } else {
            Serial.print(F("[google_drive] Warning: Could not load SSL certificate from: "));
            Serial.println(json_config.rootCaPath);
        }
    }

    // Create necessary directories for Google Drive cache structure
    photo_frame_error_t dir_result = create_directories(sd_card);
    if (dir_result != error_type::None) {
        Serial.print(F("[google_drive] Failed to create Google Drive directories: "));
        Serial.println(dir_result.code);
        return dir_result;
    }

    Serial.println(F("[google_drive] Google Drive initialized from JSON configuration"));
    return error_type::None;
}

photo_frame_error_t google_drive::initialize_from_unified_config(const unified_config::google_drive_config& gd_config) {
    // Validate essential configuration
    if (!gd_config.is_valid()) {
        Serial.println(F("[google_drive] Invalid Google Drive configuration in unified config"));
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

    // Create legacy config structure for backward compatibility
    config.serviceAccountEmail = gd_config.auth.service_account_email;
    config.privateKeyPem = gd_config.auth.private_key_pem;
    config.clientId = gd_config.auth.client_id;
    config.folderId = gd_config.drive.folder_id;
    config.rootCaPath = gd_config.drive.root_ca_path;
    config.listPageSize = gd_config.drive.list_page_size;
    config.useInsecureTls = gd_config.drive.use_insecure_tls;
    config.localPath = gd_config.caching.local_path;
    config.tocMaxAgeSeconds = gd_config.caching.toc_max_age_seconds;
    config.maxRequestsPerWindow = gd_config.rate_limiting.max_requests_per_window;
    config.rateLimitWindowSeconds = gd_config.rate_limiting.rate_limit_window_seconds;
    config.minRequestDelayMs = gd_config.rate_limiting.min_request_delay_ms;
    config.maxRetryAttempts = gd_config.rate_limiting.max_retry_attempts;
    config.backoffBaseDelayMs = gd_config.rate_limiting.backoff_base_delay_ms;
    config.maxWaitTimeMs = gd_config.rate_limiting.max_wait_time_ms;

    // SSL certificate handling: Note that this method doesn't have sd_card access
    // The certificate loading would need to be handled separately if needed
    if (!gd_config.drive.use_insecure_tls && !gd_config.drive.root_ca_path.isEmpty()) {
        Serial.println(F("[google_drive] Warning: SSL certificate path specified but cannot load without SD card access"));
        Serial.println(F("[google_drive] Consider using the JSON-based initialization for SSL certificate support"));
    }

    Serial.println(F("[google_drive] Google Drive initialized from unified configuration"));
    return error_type::None;
}

// Overloaded methods that use internal TOC path
size_t google_drive::get_toc_file_count(sd_card& sdCard, photo_frame_error_t* error) {
    String tocPath = get_toc_file_path();
    return get_toc_file_count(sdCard, tocPath, error);
}

google_drive_file
google_drive::get_toc_file_by_index(sd_card& sdCard, size_t index, photo_frame_error_t* error) {
    Serial.print(F("[google_drive] get_toc_file_by_index: "));
    Serial.println(index);

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

// Member function version of cleanup_temporary_files
uint32_t google_drive::cleanup_temporary_files(sd_card& sdCard, boolean force) {
    return cleanup_temporary_files(sdCard, config, force);
}

} // namespace photo_frame