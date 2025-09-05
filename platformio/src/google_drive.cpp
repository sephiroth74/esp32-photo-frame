#include <ArduinoJson.h>
#include <time.h>

#include "datetime_utils.h"
#include "google_drive.h"
#include "sd_card.h"
#include "string_utils.h"

extern photo_frame::sd_card sdCard;

// Memory monitoring utility
void logMemoryUsage(const char* context)
{
    size_t freeHeap = ESP.getFreeHeap();
    size_t totalHeap = ESP.getHeapSize();
    size_t usedHeap = totalHeap - freeHeap;
    float usagePercent = (float)usedHeap / totalHeap * 100.0;

    Serial.print(F("Memory ["));
    Serial.print(context);
    Serial.print(F("]: Free="));
    Serial.print(freeHeap);
    Serial.print(F(" Used="));
    Serial.print(usedHeap);
    Serial.print(F(" ("));
    Serial.print(usagePercent, 1);
    Serial.println(F("%)"));

    // Warning if memory usage is high
    if (usagePercent > 80.0) {
        Serial.println(F("WARNING: High memory usage detected!"));
    }
}

// Check if we have enough memory for JSON operations
bool checkMemoryAvailable(size_t requiredBytes)
{
    size_t freeHeap = ESP.getFreeHeap();
    size_t safetyMargin = 2048; // Keep 2KB safety margin

    if (freeHeap < (requiredBytes + safetyMargin)) {
        Serial.print(F("Insufficient memory: need "));
        Serial.print(requiredBytes);
        Serial.print(F(", have "));
        Serial.println(freeHeap);
        return false;
    }
    return true;
}

namespace photo_frame {

google_drive_files_list google_drive::retrieve_toc(bool batteryConservationMode)
{
    google_drive_files_list result;

    logMemoryUsage("TOC Retrieve Start");

    // Build TOC path using optimized utility function
    String tocFullPath = string_utils::build_path(config.localPath, config.tocFilename);
    bool shouldFetchFromDrive = false;
    bool localTocExists = sdCard.file_exists(tocFullPath.c_str());

    if (localTocExists) {
        // Check file age
        time_t fileTime = sdCard.get_last_modified(tocFullPath.c_str());
        Serial.print(F("Local TOC file found: "));
        Serial.println(tocFullPath);
        Serial.print(F("TOC file last modified: "));
        Serial.println(fileTime);

        time_t currentTime = time(NULL);

        if (fileTime > 0 && currentTime > 0) {
            unsigned long fileAge = currentTime - fileTime;

            Serial.print(F("TOC file age: "));
            Serial.print(fileAge);
            Serial.print(F(" seconds, max age: "));
            Serial.println(config.tocMaxAgeSeconds);

            if (fileAge <= config.tocMaxAgeSeconds) {
                Serial.println(F("Using cached TOC file"));
                return load_toc_from_file(tocFullPath);
            } else if (batteryConservationMode) {
                Serial.println(
                    F("TOC file is old but using cached version due to battery conservation mode"));
                return load_toc_from_file(tocFullPath);
            } else {
                Serial.println(F("TOC file is too old, will try to fetch from Google Drive"));
                shouldFetchFromDrive = true;
            }
        } else if (batteryConservationMode) {
            Serial.println(F("Could not determine TOC file timestamp, but using existing file due "
                             "to battery conservation mode"));
            return load_toc_from_file(tocFullPath);
        } else {
            Serial.println(
                F("Could not determine TOC file timestamp, will try to fetch from Google Drive"));
            shouldFetchFromDrive = true;
        }
    } else if (!localTocExists) {
        if (batteryConservationMode) {
            Serial.println(F("TOC file does not exist locally and battery conservation mode is on "
                             "- returning empty list"));
            return result; // Return empty list to avoid network operation
        } else {
            Serial.println(F("TOC file does not exist locally, will fetch from Google Drive"));
            shouldFetchFromDrive = true;
        }
    }

    if (shouldFetchFromDrive) {
        Serial.println(F("Fetching TOC from Google Drive..."));

        // Try to load cached access token first
        photo_frame_error_t error = load_access_token_from_file();
        if (error != error_type::None) {
            Serial.println(F("No valid cached token, getting new access token..."));
            error = client.get_access_token();
            if (error != error_type::None) {
                Serial.print(F("Failed to get access token: "));
                Serial.println(error.code);
            } else {
                // Save the new token for future use
                save_access_token_to_file();
            }
        } else {
            Serial.println(F("Using cached access token"));
        }

        if (error != error_type::None) {

            // Fallback to local TOC if available and not forced
            if (localTocExists) {
                Serial.println(F("Falling back to cached TOC file"));
                return load_toc_from_file(tocFullPath);
            }
            return result; // Return empty list
        }

        // Check memory before Google Drive operations
        if (!checkMemoryAvailable(4096)) {
            Serial.println(F("Insufficient memory for Google Drive operations"));
            if (localTocExists) {
                Serial.println(F("Falling back to cached TOC file"));
                return load_toc_from_file(tocFullPath);
            }
            return result; // Return empty list
        }

        // Fetch files from Google Drive
        std::vector<google_drive_file> files;
        error = client.list_files(config.folderId.c_str(), files, config.listPageSize);

        if (error != error_type::None) {
            Serial.print(F("Failed to list files from Google Drive: "));
            Serial.println(error.code);

            // Fallback to local TOC if available and not forced
            if (localTocExists) {
                Serial.println(F("Falling back to cached TOC file"));
                return load_toc_from_file(tocFullPath);
            }
            return result; // Return empty list
        }

        result.files = files;

        // Save to local file for caching
        save_toc_to_file(tocFullPath, result);

        Serial.print(F("Retrieved "));
        Serial.print(result.files.size());
        Serial.println(F(" files from Google Drive"));
    }

    logMemoryUsage("TOC Retrieve End");
    return result;
}

google_drive_files_list google_drive::load_toc_from_file(const String& filePath)
{
    google_drive_files_list result;

    logMemoryUsage("TOC Load Start");

    fs::File file = sdCard.open(filePath.c_str(), FILE_READ);
    if (!file) {
        Serial.print(F("Failed to open TOC file: "));
        Serial.println(filePath);
        return result;
    }

    // Skip line 1 (timestamp) and line 2 (fileCount)
    file.readStringUntil('\n');
    String fileCountLine = file.readStringUntil('\n');

    // Parse fileCount to pre-allocate vector
    int equalPos = fileCountLine.indexOf('=');
    if (equalPos != -1) {
        String countStr = fileCountLine.substring(equalPos + 1);
        countStr.trim();
        size_t fileCount = countStr.toInt();
        result.files.reserve(fileCount);

        Serial.print(F("Loading "));
        Serial.print(fileCount);
        Serial.println(F(" files from plain text TOC"));
    }

    // Read file entries line by line
    while (file.available()) {
        String line = file.readStringUntil('\n');
        line.trim();

        if (line.length() == 0)
            continue;

        // Parse line: id|name|mimeType|modifiedTime
        int pos1 = line.indexOf('|');
        if (pos1 == -1)
            continue;

        int pos2 = line.indexOf('|', pos1 + 1);
        if (pos2 == -1)
            continue;

        int pos3 = line.indexOf('|', pos2 + 1);
        if (pos3 == -1)
            continue;

        String id = line.substring(0, pos1);
        String name = line.substring(pos1 + 1, pos2);
        String mimeType = line.substring(pos2 + 1, pos3);
        String modifiedTime = line.substring(pos3 + 1);

        google_drive_file driveFile(id, name, mimeType, modifiedTime);
        result.files.push_back(driveFile);

        // Yield periodically for large files
        if (result.files.size() % 50 == 0) {
            yield();
        }
    }

    file.close();

    Serial.print(F("Loaded "));
    Serial.print(result.files.size());
    Serial.println(F(" files from plain text TOC"));

    logMemoryUsage("TOC Load End");
    return result;
}

google_drive_files_list google_drive::load_toc_from_file_streaming(const String& filePath)
{
    // With plain text format, streaming is not needed - just use the regular function
    Serial.println(F("Streaming parser not needed for plain text format, using regular parser"));
    return load_toc_from_file(filePath);
}

photo_frame_error_t google_drive::create_directories(sd_card& sdCard)
{
    // Create necessary directories on the SD card for Google Drive local cache
    // first make sure the directory doesn't exists and it's not a file, in which case we will try to delete the file and continue
    // otherwise, if the directory already exists, continue to check subdirectories.
    if (sdCard.file_exists(config.localPath.c_str())) {
        if (sdCard.is_file(config.localPath.c_str())) {
            Serial.print(F("Removing file at path: "));
            Serial.println(config.localPath);
            if (!sdCard.remove(config.localPath.c_str())) {
                Serial.print(F("Failed to remove file at path: "));
                Serial.println(config.localPath);
                return photo_frame::error_type::SdCardDirCreateFailed;
            }
        } else {
            Serial.print(F("Root directory already exists: "));
            Serial.println(config.localPath);
        }
    }

    // Create root directory
    if (!sdCard.create_directories(config.localPath.c_str())) {
        Serial.print(F("Failed to create root directory for Google Drive local cache: "));
        Serial.println(config.localPath);
        return photo_frame::error_type::SdCardDirCreateFailed;
    }

    // Create temp subdirectory
    String tempDir = get_temp_dir_path();
    if (!sdCard.create_directories(tempDir.c_str())) {
        Serial.print(F("Failed to create temp directory: "));
        Serial.println(tempDir);
        return photo_frame::error_type::SdCardDirCreateFailed;
    }
    Serial.print(F("Created temp directory: "));
    Serial.println(tempDir);

    // Create cache subdirectory
    String cacheDir = get_cache_dir_path();
    if (!sdCard.create_directories(cacheDir.c_str())) {
        Serial.print(F("Failed to create cache directory: "));
        Serial.println(cacheDir);
        return photo_frame::error_type::SdCardDirCreateFailed;
    }
    Serial.print(F("Created cache directory: "));
    Serial.println(cacheDir);

    return photo_frame::error_type::None;
}

bool google_drive::save_toc_to_file(const String& filePath,
    const google_drive_files_list& filesList)
{
    const size_t maxFiles = filesList.files.size();

    logMemoryUsage("TOC Save Start");

    // Plain text format uses much less memory than JSON
    size_t estimatedMemory = 1024; // Much smaller memory requirement
    if (!checkMemoryAvailable(estimatedMemory)) {
        Serial.println(F("Insufficient memory for TOC save operation"));
        return false;
    }

    // Extract directory path from file path and create directories if needed
    int lastSlash = filePath.lastIndexOf('/');
    if (lastSlash > 0) {
        String dirPath = filePath.substring(0, lastSlash);
        Serial.print(F("Creating directories for path: "));
        Serial.println(dirPath);

        if (!sdCard.create_directories(dirPath.c_str())) {
            Serial.print(F("Failed to create directories for TOC file: "));
            Serial.println(dirPath);
            return false;
        }
    }

    // Now try to create the file
    fs::File file = sdCard.open(filePath.c_str(), FILE_WRITE);
    if (!file) {
        Serial.print(F("Failed to create TOC file: "));
        Serial.println(filePath);
        return false;
    }

    // Write plain text format
    // Line 1: timestamp = <value>
    file.print(F("timestamp = "));
    file.println(time(NULL));

    // Line 2: fileCount = <value>
    file.print(F("fileCount = "));
    file.println(maxFiles);

    // Line 3+: File entries in format: id|name|mimeType|modifiedTime
    for (const google_drive_file& driveFile : filesList.files) {
        file.print(driveFile.id);
        file.print(F("|"));
        file.print(driveFile.name);
        file.print(F("|"));
        file.print(driveFile.mimeType);
        file.print(F("|"));
        file.println(driveFile.modifiedTime);

        // Periodically yield to prevent watchdog reset for large lists
        if (filesList.files.size() > 100) {
            yield();
        }
    }

    file.close();

    Serial.print(F("Saved TOC with "));
    Serial.print(maxFiles);
    Serial.print(F(" files to: "));
    Serial.println(filePath);

    logMemoryUsage("TOC Save End");
    return true;
}

bool google_drive::save_toc_to_file_chunked(fs::File& file,
    const google_drive_files_list& filesList)
{
    const size_t totalFiles = filesList.files.size();

    Serial.print(F("Using chunked writing for "));
    Serial.print(totalFiles);
    Serial.println(F(" files"));

    // Write plain text header
    file.print(F("timestamp = "));
    file.println(time(NULL));

    file.print(F("fileCount = "));
    file.println(totalFiles);

    // Write files one by one to minimize memory usage
    for (size_t i = 0; i < totalFiles; i++) {
        const google_drive_file& driveFile = filesList.files[i];

        file.print(driveFile.id);
        file.print(F("|"));
        file.print(driveFile.name);
        file.print(F("|"));
        file.print(driveFile.mimeType);
        file.print(F("|"));
        file.println(driveFile.modifiedTime);

        // Yield periodically to prevent watchdog reset
        if (i % 50 == 0) {
            yield();
        }

        // Report progress every 100 files
        if (i % 100 == 0) {
            Serial.print(F("Processed "));
            Serial.print(i + 1);
            Serial.print(F("/"));
            Serial.println(totalFiles);
        }
    }

    file.close();

    Serial.println(F("Chunked TOC save completed"));

    // Report final memory usage
    size_t freeHeap = ESP.getFreeHeap();
    Serial.print(F("Free heap after chunked save: "));
    Serial.println(freeHeap);

    return true;
}

String google_drive::get_toc_file_path() const
{
    return string_utils::build_path(config.localPath, config.tocFilename);
}

String google_drive::get_temp_dir_path() const
{
    return string_utils::build_path(config.localPath, GOOGLE_DRIVE_TEMP_DIR);
}

String google_drive::get_cache_dir_path() const
{
    return string_utils::build_path(config.localPath, GOOGLE_DRIVE_CACHE_DIR);
}

String google_drive::get_cached_file_path(const String& filename) const
{
    return string_utils::build_path(get_cache_dir_path(), filename);
}

String google_drive::get_temp_file_path(const String& filename) const
{
    return string_utils::build_path(get_temp_dir_path(), filename);
}

size_t google_drive::get_toc_file_count(const String& filePath, photo_frame_error_t* error)
{
    if (error) {
        *error = error_type::None;
    }

    fs::File file = sdCard.open(filePath.c_str(), FILE_READ);
    if (!file) {
        Serial.print(F("Failed to open TOC file for file count: "));
        Serial.println(filePath);
        if (error) {
            *error = error_type::SdCardFileOpenFailed;
        }
        return 0;
    }

    // Skip line 1 (timestamp)
    String line = file.readStringUntil('\n');
    if (line.length() == 0) {
        Serial.println(F("TOC file is empty or invalid"));
        file.close();
        if (error) {
            *error = error_type::JsonParseFailed;
        }
        return 0;
    }

    // Read line 2 (fileCount)
    line = file.readStringUntil('\n');
    file.close();

    if (line.length() == 0) {
        Serial.println(F("TOC file missing fileCount line"));
        if (error) {
            *error = error_type::JsonParseFailed;
        }
        return 0;
    }

    // Parse "fileCount = <number>"
    int equalPos = line.indexOf('=');
    if (equalPos == -1) {
        Serial.println(F("Invalid TOC format: missing '=' in fileCount line"));
        if (error) {
            *error = error_type::JsonParseFailed;
        }
        return 0;
    }

    String countStr = line.substring(equalPos + 1);
    countStr.trim();

    size_t fileCount = countStr.toInt();
    if (fileCount == 0 && countStr != "0") {
        Serial.println(F("Invalid fileCount value in TOC"));
        if (error) {
            *error = error_type::JsonParseFailed;
        }
        return 0;
    }

    Serial.print(F("Found "));
    Serial.print(fileCount);
    Serial.println(F(" files"));

    return fileCount;
}

google_drive_file google_drive::get_toc_file_by_index(const String& filePath,
    size_t index,
    photo_frame_error_t* error)
{
    if (error) {
        *error = error_type::None;
    }

    fs::File file = sdCard.open(filePath.c_str(), FILE_READ);
    if (!file) {
        Serial.print(F("Failed to open TOC file for file entry: "));
        Serial.println(filePath);
        if (error) {
            *error = error_type::SdCardFileOpenFailed;
        }
        return google_drive_file("", "", "", "");
    }

    // Skip line 1 (timestamp) and line 2 (fileCount)
    String line1 = file.readStringUntil('\n');
    String line2 = file.readStringUntil('\n');

    if (line1.length() == 0 || line2.length() == 0) {
        Serial.println(F("TOC file missing header lines"));
        file.close();
        if (error) {
            *error = error_type::JsonParseFailed;
        }
        return google_drive_file("", "", "", "");
    }

    // Skip to the desired index (line index + 3)
    for (size_t i = 0; i < index; i++) {
        String skipLine = file.readStringUntil('\n');
        if (skipLine.length() == 0) {
            Serial.print(F("TOC file ended before reaching index "));
            Serial.println(index);
            file.close();
            if (error) {
                *error = error_type::JsonParseFailed;
            }
            return google_drive_file("", "", "", "");
        }
    }

    // Read the target line
    String targetLine = file.readStringUntil('\n');
    file.close();

    if (targetLine.length() == 0) {
        Serial.print(F("No file entry found at index "));
        Serial.println(index);
        if (error) {
            *error = error_type::JsonParseFailed;
        }
        return google_drive_file("", "", "", "");
    }

    // Parse the line: id|name|mimeType|modifiedTime
    int pos1 = targetLine.indexOf('|');
    if (pos1 == -1) {
        Serial.println(F("Invalid file entry format: missing first separator"));
        if (error) {
            *error = error_type::JsonParseFailed;
        }
        return google_drive_file("", "", "", "");
    }

    int pos2 = targetLine.indexOf('|', pos1 + 1);
    if (pos2 == -1) {
        Serial.println(F("Invalid file entry format: missing second separator"));
        if (error) {
            *error = error_type::JsonParseFailed;
        }
        return google_drive_file("", "", "", "");
    }

    int pos3 = targetLine.indexOf('|', pos2 + 1);
    if (pos3 == -1) {
        Serial.println(F("Invalid file entry format: missing third separator"));
        if (error) {
            *error = error_type::JsonParseFailed;
        }
        return google_drive_file("", "", "", "");
    }

    String id = targetLine.substring(0, pos1);
    String name = targetLine.substring(pos1 + 1, pos2);
    String mimeType = targetLine.substring(pos2 + 1, pos3);
    String modifiedTime = targetLine.substring(pos3 + 1);

    Serial.print(F("Retrieved file at index "));
    Serial.print(index);
    Serial.print(F(": "));
    Serial.println(name);

    return google_drive_file(id, name, mimeType, modifiedTime);
}

fs::File google_drive::download_file(google_drive_file file, photo_frame_error_t* error)
{
    fs::File emptyFile;

    // Initialize error to None
    if (error) {
        *error = error_type::None;
    }

    // Create file paths using new directory structure
    String tempPath = get_temp_file_path(file.name);
    String finalPath = get_cached_file_path(file.name);

    Serial.print(F("Downloading file: "));
    Serial.println(file.name);
    Serial.print(F("Temp path: "));
    Serial.println(tempPath);
    Serial.print(F("Final path: "));
    Serial.println(finalPath);

    // Ensure directories exist before creating temporary file
    int lastSlash = tempPath.lastIndexOf('/');
    if (lastSlash > 0) {
        String dirPath = tempPath.substring(0, lastSlash);
        if (!sdCard.create_directories(dirPath.c_str())) {
            Serial.print(F("Failed to create directories for temp file: "));
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
            Serial.println(F("No valid cached token, getting new access token..."));
            access_token_error = client.get_access_token();
            if (access_token_error != error_type::None) {
                Serial.print(F("Failed to get access token: "));
                Serial.println(access_token_error.code);
            } else {
                // Save the new token for future use
                save_access_token_to_file();
            }
        } else {
            Serial.println(F("Using cached access token"));
        }
    }

    // Open temporary file for writing
    fs::File tempFile = sdCard.open(tempPath.c_str(), FILE_WRITE);
    if (!tempFile) {
        Serial.print(F("Failed to create temporary file: "));
        Serial.println(tempPath);
        if (error) {
            *error = error_type::SdCardFileCreateFailed;
        }
        return emptyFile;
    }

    // Download file from Google Drive to temporary location
    photo_frame_error_t downloadError = client.download_file(file.id, &tempFile);
    tempFile.flush();
    tempFile.close();

    if (downloadError != error_type::None) {
        Serial.print(F("Failed to download file from Google Drive: "));
        Serial.println(downloadError.code);

        // Clean up temporary file on download failure
        Serial.print(F("Cleaning up temporary file: "));
        Serial.println(tempPath);
        if (!sdCard.remove(tempPath.c_str())) {
            Serial.println(F("Warning: Failed to remove temporary file"));
        } else {
            Serial.println(F("Temporary file cleaned up successfully"));
        }

        if (error) {
            *error = downloadError;
        }

        return emptyFile;
    }

    Serial.println(F("File downloaded successfully to temporary location"));

#if DEBUG_MODE
    // print the tempFile size
    tempFile = sdCard.open(tempPath.c_str(), FILE_READ);
    if (tempFile) {
        Serial.print(F("Temporary file size: "));
        Serial.print(tempFile.size());
        Serial.println(F(" bytes"));
        tempFile.close();
    } else {
        Serial.print(F("Failed to open temporary file: "));
        Serial.println(tempPath);
    }
#endif // DEBUG_MODE

    // Move temporary file to final destination
    bool moveSuccess = sdCard.rename(tempPath.c_str(), finalPath.c_str(), true); // true = overwrite existing

    if (!moveSuccess) {
        Serial.print(F("Failed to move file from temp to final location"));

        // Clean up temporary file on move failure
        // Serial.print(F("Cleaning up temporary file after move failure: "));
        // Serial.println(tempPath);
        // if (!sdCard.remove(tempPath.c_str())) {
        //     Serial.println(F("Warning: Failed to remove temporary file after move failure"));
        // } else {
        //     Serial.println(F("Temporary file cleaned up after move failure"));
        // }

        if (error) {
            *error = error_type::SdCardFileCreateFailed;
        }
        return emptyFile;
    }

    Serial.print(F("File moved successfully to: "));
    Serial.println(finalPath);

    // Open and return the final file
    fs::File finalFile = sdCard.open(finalPath.c_str(), FILE_READ);
    if (!finalFile) {
        Serial.print(F("Failed to open final file: "));
        Serial.println(finalPath);
        if (error) {
            *error = error_type::SdCardFileOpenFailed;
        }
        return emptyFile;
    }

    // Mark as downloaded from cloud since we actually downloaded it
    last_image_source = IMAGE_SOURCE_CLOUD;

    return finalFile;
}

image_source_t google_drive::get_last_image_source() const
{
    return last_image_source;
}

void google_drive::set_last_image_source(image_source_t source)
{
    last_image_source = source;
}

String google_drive::load_root_ca_certificate(sd_card& sdCard, const char* rootCaPath)
{
    Serial.print(F("Loading Google Drive root CA from: "));
    Serial.println(rootCaPath);

    if (!sdCard.is_initialized()) {
        Serial.println(F("SD card not initialized, cannot load root CA"));
        return String();
    }

    if (!sdCard.file_exists(rootCaPath)) {
        Serial.print(F("Root CA file not found: "));
        Serial.println(rootCaPath);
        return String();
    }

    fs::File certFile = sdCard.open(rootCaPath, FILE_READ);
    if (!certFile) {
        Serial.print(F("Failed to open root CA file: "));
        Serial.println(rootCaPath);
        return String();
    }

    size_t fileSize = certFile.size();
    if (fileSize > 4096) { // Reasonable limit for a certificate
        Serial.print(F("Root CA file too large: "));
        Serial.print(fileSize);
        Serial.println(F(" bytes (max 4096)"));
        certFile.close();
        return String();
    }

    if (fileSize == 0) {
        Serial.println(F("Root CA file is empty"));
        certFile.close();
        return String();
    }

    // Pre-allocate string for certificate content
    String certContent;
    certContent.reserve(fileSize + 1);
    certContent = certFile.readString();
    certFile.close();

    // Basic validation - check for PEM format markers
    if (!certContent.startsWith("-----BEGIN CERTIFICATE-----") || !certContent.endsWith("-----END CERTIFICATE-----")) {
        Serial.println(F("Invalid certificate format - missing PEM markers"));
        return String();
    }

    Serial.print(F("Successfully loaded root CA certificate ("));
    Serial.print(certContent.length());
    Serial.println(F(" bytes)"));

    return certContent;
}

uint32_t google_drive::cleanup_temporary_files(sd_card& sdCard,
    const google_drive_json_config& config,
    boolean force)
{
    Serial.print(F("Cleaning up temporary files from previous incomplete downloads... ("));
    Serial.print(F("Local path: "));
    Serial.print(config.localPath);
    Serial.print(F(", Force cleanup: "));
    Serial.print(force);
    Serial.println(F(")"));

    uint32_t cleanedCount = 0;

    if (!sdCard.is_initialized()) {
        Serial.println(F("SD card not initialized, cannot clean up temporary files"));
        return 0;
    }

    uint64_t usedBytes = sdCard.used_bytes();
    uint64_t totalBytes = sdCard.total_bytes();
    uint64_t cardSize = sdCard.card_size();
    uint64_t freeBytes = totalBytes - usedBytes;

#if DEBUG_MODE

    Serial.print(F("SD card size: "));
    Serial.println(cardSize / 1024 / 1024);

    Serial.print(F("Total size: "));
    Serial.println(totalBytes / 1024 / 1024);

    Serial.print(F("Used size: "));
    Serial.println(usedBytes / 1024 / 1024);

    Serial.print(F("SD card free space: "));
    Serial.println(freeBytes / 1024 / 1024);

#endif // DEBUG_MODE

    // Calculate 20% threshold of total SD card space first
    uint64_t twentyPercentThreshold = totalBytes * 20 / 100;
    bool lowSpace = freeBytes < twentyPercentThreshold;
    bool shouldCleanupImages = force || lowSpace;

    // Handle TOC deletion and orphaned file cleanup
    String tocPath = string_utils::build_path(config.localPath, config.tocFilename);
    std::vector<String> validFilenames;

    if (sdCard.file_exists(tocPath.c_str())) {
        Serial.print(F("Found TOC file: "));
        Serial.println(tocPath);

        time_t tocAge = sdCard.get_file_age(tocPath.c_str());
        Serial.print(F("TOC file age: "));
        Serial.print(tocAge);
        Serial.print(F(" seconds, max age: "));
        Serial.println(config.tocMaxAgeSeconds);

        // If TOC will be deleted AND we're not doing full cleanup, read filenames for orphaned cleanup
        if ((force || tocAge > config.tocMaxAgeSeconds) && !shouldCleanupImages) {
            Serial.println(F("Reading TOC file for orphaned cache cleanup..."));
            fs::File tocFile = sdCard.open(tocPath.c_str(), FILE_READ);
            if (tocFile) {
                // Skip timestamp line
                String line = tocFile.readStringUntil('\n');
                // Skip fileCount line
                line = tocFile.readStringUntil('\n');

                // Read all file entries
                while (tocFile.available()) {
                    line = tocFile.readStringUntil('\n');
                    line.trim();
                    if (line.length() == 0)
                        continue;

                    // Parse line: id|name|mimeType|modifiedTime
                    int pos1 = line.indexOf('|');
                    if (pos1 == -1)
                        continue;
                    int pos2 = line.indexOf('|', pos1 + 1);
                    if (pos2 == -1)
                        continue;

                    String filename = line.substring(pos1 + 1, pos2);
                    if (filename.length() > 0) {
                        validFilenames.push_back(filename);
                    }
                }
                tocFile.close();

                Serial.print(F("Found "));
                Serial.print(validFilenames.size());
                Serial.println(F(" valid filenames in TOC"));
            }
        }

        if (force || tocAge > config.tocMaxAgeSeconds) {
            if (sdCard.remove(tocPath.c_str())) {
                Serial.println(F("Successfully removed TOC file"));
                cleanedCount++;
            } else {
                Serial.println(F("Failed to remove TOC file"));
            }
        }
    }

    // Always clean up temporary files from temp directory
    Serial.println(F("Cleaning up temporary files..."));
    String tempDir = string_utils::build_path(config.localPath, GOOGLE_DRIVE_TEMP_DIR);
    if (sdCard.file_exists(tempDir.c_str())) {
        fs::File tempRoot = sdCard.open(tempDir.c_str(), FILE_READ);
        if (tempRoot && tempRoot.isDirectory()) {
            fs::File tempFile = tempRoot.openNextFile();
            while (tempFile) {
                String tempFileName = tempFile.name();
                String tempFilePath = string_utils::build_path(tempDir, tempFileName);

                Serial.print(F("Removing temp file: "));
                Serial.println(tempFileName);

                if (sdCard.remove(tempFilePath.c_str())) {
                    cleanedCount++;
                } else {
                    Serial.print(F("Failed to remove temp file: "));
                    Serial.println(tempFileName);
                }
                tempFile = tempRoot.openNextFile();
            }
            tempRoot.close();
        }
    }

    // Clean up orphaned cache files only if we're NOT doing full cleanup
    if (validFilenames.size() > 0 && !shouldCleanupImages) {
        Serial.println(F("Cleaning up cached images not in TOC..."));
        String cacheDir = string_utils::build_path(config.localPath, GOOGLE_DRIVE_CACHE_DIR);
        if (sdCard.file_exists(cacheDir.c_str())) {
            fs::File cacheRoot = sdCard.open(cacheDir.c_str(), FILE_READ);
            if (cacheRoot && cacheRoot.isDirectory()) {
                fs::File cacheFile = cacheRoot.openNextFile();
                while (cacheFile) {
                    String fileName = cacheFile.name();

                    // Check if this cached file is in the valid filenames list
                    bool foundInToc = false;
                    for (const String& validName : validFilenames) {
                        if (fileName.equals(validName)) {
                            foundInToc = true;
                            break;
                        }
                    }

                    if (!foundInToc) {
                        String filePath = string_utils::build_path(cacheDir, fileName);
                        Serial.print(F("Removing orphaned cached file: "));
                        Serial.println(fileName);

                        if (sdCard.remove(filePath.c_str())) {
                            cleanedCount++;
                        } else {
                            Serial.print(F("Failed to remove orphaned file: "));
                            Serial.println(fileName);
                        }
                    }
                    cacheFile = cacheRoot.openNextFile();
                }
                cacheRoot.close();
            }
        }
    }

    Serial.print(F("SD card space - Total: "));
    Serial.print(totalBytes / 1024 / 1024);
    Serial.print(F(" MB, Free: "));
    Serial.print(freeBytes / 1024 / 1024);
    Serial.print(F(" MB ("));
    Serial.print((freeBytes * 100) / totalBytes);
    Serial.print(F("%), 20% threshold: "));
    Serial.print(twentyPercentThreshold / 1024 / 1024);
    Serial.println(F(" MB"));

    if (lowSpace && !force) {
        Serial.println(F("SD card free space is under 20%, enabling image cleanup"));
    }

    if (shouldCleanupImages) {
        if (force) {
            Serial.println(F("Force cleanup enabled, removing all cached images"));
        } else {
            Serial.println(F("Low disk space detected, removing cached images to free space"));
        }

        // Remove all cached images from cache directory
        String cacheDir = string_utils::build_path(config.localPath, GOOGLE_DRIVE_CACHE_DIR);
        if (sdCard.file_exists(cacheDir.c_str())) {
            fs::File cacheRoot = sdCard.open(cacheDir.c_str(), FILE_READ);
            if (!cacheRoot || cacheRoot.isDirectory() == false) {
                Serial.println(F("Failed to open cache directory for cleanup"));
                return cleanedCount;
            }

            fs::File cacheFile = cacheRoot.openNextFile();
            while (cacheFile) {
                String fileName = cacheFile.name();
                String filePath = string_utils::build_path(cacheDir, fileName);

                Serial.print(F("Removing cached image: "));
                Serial.println(fileName);

                if (sdCard.remove(filePath.c_str())) {
                    cleanedCount++;
                } else {
                    Serial.print(F("Failed to remove: "));
                    Serial.println(fileName);
                }
                cacheFile = cacheRoot.openNextFile();
            }
            cacheRoot.close();
        }
    } else {
        Serial.println(F("Sufficient disk space available, keeping cached images"));
    }

    Serial.print(F("Cleaned up "));
    Serial.print(cleanedCount);
    Serial.println(F(" temporary files"));

    return cleanedCount;
}

photo_frame_error_t google_drive::save_access_token_to_file()
{
    Serial.println(F("Saving access token to file..."));

    const google_drive_access_token* token = client.get_access_token_value();
    if (!token || strlen(token->accessToken) == 0) {
        Serial.println(F("No valid access token to save"));
        return error_type::TokenMissing;
    }

    String tokenPath = string_utils::build_path(config.localPath, "access_token.json");

    fs::File tokenFile = SD.open(tokenPath.c_str(), FILE_WRITE);
    if (!tokenFile) {
        Serial.print(F("Failed to create token file: "));
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

    Serial.print(F("Access token saved to: "));
    Serial.println(tokenPath);

    return error_type::None;
}

photo_frame_error_t google_drive::load_access_token_from_file()
{
    Serial.println(F("Loading access token from file..."));

    String tokenPath = string_utils::build_path(config.localPath, "access_token.json");

    if (!SD.exists(tokenPath.c_str())) {
        Serial.print(F("Token file does not exist: "));
        Serial.println(tokenPath);
        return error_type::SdCardFileNotFound;
    }

    fs::File tokenFile = SD.open(tokenPath.c_str(), FILE_READ);
    if (!tokenFile) {
        Serial.print(F("Failed to open token file: "));
        Serial.println(tokenPath);
        return error_type::SdCardFileOpenFailed;
    }

    String tokenContent = tokenFile.readString();
    tokenFile.close();

    // Parse JSON
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, tokenContent);

    if (error) {
        Serial.print(F("Failed to parse token file: "));
        Serial.println(error.c_str());
        return error_type::JsonParseFailed;
    }

    if (!doc.containsKey("access_token") || !doc.containsKey("expires_at") || !doc.containsKey("obtained_at")) {
        Serial.println(F("Invalid token file format"));
        return error_type::JsonParseFailed;
    }

    google_drive_access_token token;
    strncpy(token.accessToken, doc["access_token"], sizeof(token.accessToken) - 1);
    token.accessToken[sizeof(token.accessToken) - 1] = '\0';
    token.expiresAt = doc["expires_at"];
    token.obtainedAt = doc["obtained_at"];

    // Check if token is still valid (with 5 minute margin)
    if (token.expired(300)) {
        Serial.println(F("Cached token has expired"));
        return error_type::TokenMissing;
    }

    client.set_access_token(token);

    Serial.print(F("Access token loaded from: "));
    Serial.println(tokenPath);
    Serial.print(F("Token expires in: "));
    Serial.print(token.expires_in());
    Serial.println(F(" seconds"));

    return error_type::None;
}

photo_frame_error_t load_google_drive_config_from_json(sd_card& sd_card,
    const char* config_filepath,
    google_drive_json_config& config)
{
    if (!sd_card.is_initialized()) {
        Serial.println(F("SD card not initialized"));
        return error_type::CardMountFailed;
    }

    fs::File configFile = sd_card.open(config_filepath, FILE_READ);
    if (!configFile) {
        Serial.print(F("Failed to open Google Drive config file: "));
        Serial.println(config_filepath);
        return error_type::FileOpenFailed;
    }

    size_t fileSize = configFile.size();
    if (fileSize == 0) {
        Serial.println(F("Google Drive config file is empty"));
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
        Serial.print(F("Failed to parse Google Drive config JSON: "));
        Serial.println(jsonError.c_str());
        return error_type::JsonParseFailed;
    }

    // Validate and extract authentication settings
    if (!doc["authentication"].isNull()) {
        JsonObject auth = doc["authentication"];
        config.serviceAccountEmail = auth["service_account_email"].as<String>();
        config.privateKeyPem = auth["private_key_pem"].as<String>();
        config.clientId = auth["client_id"].as<String>();

        // Validate service account email
        if (config.serviceAccountEmail.isEmpty()) {
            Serial.println(F("Validation failed: service_account_email is required"));
            return error_type::ConfigMissingField;
        }
        if (config.serviceAccountEmail.indexOf('@') == -1 || config.serviceAccountEmail.indexOf('.') == -1) {
            Serial.println(F("Validation failed: service_account_email must be a valid email address"));
            return error_type::ConfigInvalidEmail;
        }

        // Validate private key PEM format
        if (config.privateKeyPem.isEmpty()) {
            Serial.println(F("Validation failed: private_key_pem is required"));
            return error_type::ConfigMissingField;
        }
        if (!config.privateKeyPem.startsWith("-----BEGIN PRIVATE KEY-----")) {
            Serial.println(F("Validation failed: private_key_pem must be in PEM format"));
            return error_type::ConfigInvalidPemKey;
        }
        if (!config.privateKeyPem.endsWith("-----END PRIVATE KEY-----") && !config.privateKeyPem.endsWith("-----END PRIVATE KEY-----\n")) {
            Serial.println(F("Validation failed: private_key_pem must end with proper PEM footer"));
            return error_type::ConfigInvalidPemKey;
        }

        // Validate client ID
        if (config.clientId.isEmpty()) {
            Serial.println(F("Validation failed: client_id is required"));
            return error_type::ConfigMissingField;
        }
        if (config.clientId.length() < 10) {
            Serial.println(F("Validation failed: client_id appears to be too short"));
            return error_type::ConfigValueOutOfRange;
        }
    } else {
        Serial.println(F("Validation failed: authentication section is required"));
        return error_type::ConfigMissingSection;
    }

    // Validate and extract drive settings
    if (!doc["drive"].isNull()) {
        JsonObject drive = doc["drive"];
        config.folderId = drive["folder_id"].as<String>();
        config.rootCaPath = drive["root_ca_path"].as<String>();
        config.listPageSize = drive["list_page_size"].as<int>();
        config.useInsecureTls = drive["use_insecure_tls"].as<bool>();

        // Validate folder ID
        if (config.folderId.isEmpty()) {
            Serial.println(F("Validation failed: folder_id is required"));
            return error_type::ConfigMissingField;
        }
        if (config.folderId.length() < 20) {
            Serial.println(F("Validation failed: folder_id appears to be too short"));
            return error_type::ConfigValueOutOfRange;
        }

        // Validate root CA path
        if (!config.rootCaPath.isEmpty()) {
            if (!config.rootCaPath.startsWith("/")) {
                Serial.println(F("Validation failed: root_ca_path must be an absolute path"));
                return error_type::ConfigInvalidPath;
            }
            if (!config.rootCaPath.endsWith(".pem")) {
                Serial.println(F("Validation failed: root_ca_path must point to a .pem file"));
                return error_type::ConfigInvalidPath;
            }
        } else if (!config.useInsecureTls) {
            Serial.println(F("Validation failed: root_ca_path is required when use_insecure_tls is false"));
            return error_type::ConfigMissingField;
        }

        // Validate list page size
        if (config.listPageSize <= 0 || config.listPageSize > GOOGLE_DRIVE_MAX_LIST_PAGE_SIZE) {
            Serial.print(F("Validation failed: list_page_size must be between 1 and "));
            Serial.println(GOOGLE_DRIVE_MAX_LIST_PAGE_SIZE);

            // do not throw an exception, just update the config with GOOGLE_DRIVE_MAX_LIST_PAGE_SIZE
            config.listPageSize = constrain(config.listPageSize, 1, GOOGLE_DRIVE_MAX_LIST_PAGE_SIZE);
            Serial.print(F("Updated list_page_size to "));
            Serial.println(config.listPageSize);
        }

        // use_insecure_tls is automatically validated as boolean by ArduinoJson
    } else {
        Serial.println(F("Validation failed: drive section is required"));
        return error_type::ConfigMissingSection;
    }

    // Validate and extract caching settings
    if (!doc["caching"].isNull()) {
        JsonObject caching = doc["caching"];
        config.localPath = caching["local_path"].as<String>();
        config.tocFilename = caching["toc_filename"].as<String>();
        config.tocMaxAgeSeconds = caching["toc_max_age_seconds"].as<unsigned long>();

        // Validate local path
        if (config.localPath.isEmpty()) {
            Serial.println(F("Validation failed: local_path is required"));
            return error_type::ConfigMissingField;
        }
        if (!config.localPath.startsWith("/")) {
            Serial.println(F("Validation failed: local_path must be an absolute path"));
            return error_type::ConfigInvalidPath;
        }
        if (config.localPath.endsWith("/")) {
            Serial.println(F("Validation failed: local_path must not end with '/'"));
            return error_type::ConfigInvalidPath;
        }

        // Validate TOC filename
        if (config.tocFilename.isEmpty()) {
            Serial.println(F("Validation failed: toc_filename is required"));
            return error_type::ConfigMissingField;
        }
        if (config.tocFilename.indexOf('/') != -1 || config.tocFilename.indexOf('\\') != -1) {
            Serial.println(F("Validation failed: toc_filename must be a filename, not a path"));
            return error_type::ConfigInvalidFilename;
        }
        if (config.tocFilename.startsWith(".")) {
            Serial.println(F("Validation failed: toc_filename cannot start with '.'"));
            return error_type::ConfigInvalidFilename;
        }

        // Validate TOC max age (1 second to 30 days)
        if (config.tocMaxAgeSeconds == 0 || config.tocMaxAgeSeconds > GOOGLE_DRIVE_TOC_MAX_AGE_SECONDS) {
            Serial.println(F("Validation failed: toc_max_age_seconds must be between 1 and 2592000 (30 days)"));
            return error_type::ConfigValueOutOfRange;
        }
    } else {
        Serial.println(F("Validation failed: caching section is required"));
        return error_type::ConfigMissingSection;
    }

    // Validate and extract rate limiting settings
    if (!doc["rate_limiting"].isNull()) {
        JsonObject rateLimiting = doc["rate_limiting"];
        config.maxRequestsPerWindow = rateLimiting["max_requests_per_window"].as<int>();
        config.rateLimitWindowSeconds = rateLimiting["rate_limit_window_seconds"].as<int>();
        config.minRequestDelayMs = rateLimiting["min_request_delay_ms"].as<int>();
        config.maxRetryAttempts = rateLimiting["max_retry_attempts"].as<int>();
        config.backoffBaseDelayMs = rateLimiting["backoff_base_delay_ms"].as<int>();
        config.maxWaitTimeMs = rateLimiting["max_wait_time_ms"].as<int>();

        // Validate max requests per window
        if (config.maxRequestsPerWindow <= 0 || config.maxRequestsPerWindow > GOOGLE_DRIVE_MAX_REQUESTS_PER_WINDOW) {
            Serial.print(F("Validation failed: max_requests_per_window must be between 1 and "));
            Serial.println(GOOGLE_DRIVE_MAX_REQUESTS_PER_WINDOW);
            return error_type::ConfigValueOutOfRange;
        }

        // Validate rate limit window seconds
        if (config.rateLimitWindowSeconds <= 0 || config.rateLimitWindowSeconds > GOOGLE_DRIVE_RATE_LIMIT_WINDOW_SECONDS) {
            Serial.print(F("Validation failed: rate_limit_window_seconds must be between 1 and "));
            Serial.println(GOOGLE_DRIVE_RATE_LIMIT_WINDOW_SECONDS);
            return error_type::ConfigValueOutOfRange;
        }

        // Validate minimum request delay
        if (config.minRequestDelayMs < 0 || config.minRequestDelayMs > GOOGLE_DRIVE_MIN_REQUEST_DELAY_MS) {
            Serial.print(F("Validation failed: min_request_delay_ms must be between 0 and "));
            Serial.println(GOOGLE_DRIVE_MIN_REQUEST_DELAY_MS);
            return error_type::ConfigValueOutOfRange;
        }

        // Validate max retry attempts
        if (config.maxRetryAttempts < 0 || config.maxRetryAttempts > GOOGLE_DRIVE_MAX_RETRY_ATTEMPTS) {
            Serial.print(F("Validation failed: max_retry_attempts must be between 0 and "));
            Serial.println(GOOGLE_DRIVE_MAX_RETRY_ATTEMPTS);
            return error_type::ConfigValueOutOfRange;
        }

        // Validate backoff base delay
        if (config.backoffBaseDelayMs <= 0 || config.backoffBaseDelayMs > GOOGLE_DRIVE_BACKOFF_BASE_DELAY_MS) {
            Serial.print(F("Validation failed: backoff_base_delay_ms must be between 1 and "));
            Serial.println(GOOGLE_DRIVE_BACKOFF_BASE_DELAY_MS);
            return error_type::ConfigValueOutOfRange;
        }

        // Validate max wait time
        if (config.maxWaitTimeMs <= 0 || config.maxWaitTimeMs > GOOGLE_DRIVE_MAX_WAIT_TIME_MS) {
            Serial.print(F("Validation failed: max_wait_time_ms must be between 1 and "));
            Serial.println(GOOGLE_DRIVE_MAX_WAIT_TIME_MS);
            return error_type::ConfigValueOutOfRange;
        }

        // Cross-validation: backoff base delay should be less than max wait time
        if (config.backoffBaseDelayMs >= config.maxWaitTimeMs) {
            Serial.println(F("Validation failed: backoff_base_delay_ms must be less than max_wait_time_ms"));
            return error_type::ConfigValueOutOfRange;
        }
    } else {
        Serial.println(F("Validation failed: rate_limiting section is required"));
        return error_type::ConfigMissingSection;
    }

    Serial.print(F("Successfully loaded and validated Google Drive config from: "));
    Serial.println(config_filepath);
    Serial.print(F("Service Account: "));
    Serial.println(config.serviceAccountEmail);
    Serial.print(F("Folder ID: "));
    Serial.println(config.folderId);
    Serial.print(F("Use Insecure TLS: "));
    Serial.println(config.useInsecureTls ? "true" : "false");
    Serial.print(F("Local Cache Path: "));
    Serial.println(config.localPath);
    Serial.print(F("TOC File Path: "));
    Serial.println(config.tocFilename);
    Serial.print(F("TOC Max Age (seconds): "));
    Serial.println(config.tocMaxAgeSeconds);

    logMemoryUsage("Config Load End");
    return error_type::None;
}

photo_frame_error_t google_drive::initialize_from_json(sd_card& sd_card, const char* config_filepath)
{
    // Load JSON configuration
    google_drive_json_config json_config;
    photo_frame_error_t load_result = load_google_drive_config_from_json(sd_card, config_filepath, json_config);
    if (load_result != error_type::None) {
        Serial.print(F("Failed to load Google Drive config: "));
        Serial.println(load_result.code);
        return load_result;
    }

    // Create client configuration from JSON
    static google_drive_client_config client_config;
    // Convert String to const char* for compatibility
    static String email_str = json_config.serviceAccountEmail;
    static String key_str = json_config.privateKeyPem;
    static String client_id_str = json_config.clientId;

    client_config.serviceAccountEmail = email_str.c_str();
    client_config.privateKeyPem = key_str.c_str();
    client_config.clientId = client_id_str.c_str();
    client_config.useInsecureTls = json_config.useInsecureTls;

    // Pass rate limiting settings to client
    client_config.rateLimitWindowSeconds = json_config.rateLimitWindowSeconds;
    client_config.minRequestDelayMs = json_config.minRequestDelayMs;
    client_config.maxRetryAttempts = json_config.maxRetryAttempts;
    client_config.backoffBaseDelayMs = json_config.backoffBaseDelayMs;
    client_config.maxWaitTimeMs = json_config.maxWaitTimeMs;

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
            Serial.print(F("Loaded SSL certificate from: "));
            Serial.println(json_config.rootCaPath);
        } else {
            Serial.print(F("Warning: Could not load SSL certificate from: "));
            Serial.println(json_config.rootCaPath);
        }
    }

    // Create necessary directories for Google Drive cache structure
    photo_frame_error_t dir_result = create_directories(sd_card);
    if (dir_result != error_type::None) {
        Serial.print(F("Failed to create Google Drive directories: "));
        Serial.println(dir_result.code);
        return dir_result;
    }

    Serial.println(F("Google Drive initialized from JSON configuration"));
    return error_type::None;
}

// Overloaded methods that use internal TOC path
size_t google_drive::get_toc_file_count(photo_frame_error_t* error)
{
    String tocPath = get_toc_file_path();
    return get_toc_file_count(tocPath, error);
}

google_drive_file google_drive::get_toc_file_by_index(size_t index, photo_frame_error_t* error)
{
    String tocPath = get_toc_file_path();
    return get_toc_file_by_index(tocPath, index, error);
}

// Member function version of cleanup_temporary_files
uint32_t google_drive::cleanup_temporary_files(boolean force)
{
    return cleanup_temporary_files(sdCard, config, force);
}

} // namespace photo_frame