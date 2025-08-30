#include <time.h>
#include <ArduinoJson.h>

#include "google_drive.h"
#include "sd_card.h"
#include "string_utils.h"
#include "datetime_utils.h"

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
    String tocFullPath = string_utils::build_path(config.localPath, config.localTocFilename);
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
            Serial.println(config.tocMaxAge);

            if (fileAge <= config.tocMaxAge) {
                Serial.println(F("Using cached TOC file"));
                return load_toc_from_file(tocFullPath);
            } else if (batteryConservationMode) {
                Serial.println(F("TOC file is old but using cached version due to battery conservation mode"));
                return load_toc_from_file(tocFullPath);
            } else {
                Serial.println(F("TOC file is too old, will try to fetch from Google Drive"));
                shouldFetchFromDrive = true;
            }
        } else if (batteryConservationMode) {
            Serial.println(F("Could not determine TOC file timestamp, but using existing file due to battery conservation mode"));
            return load_toc_from_file(tocFullPath);
        } else {
            Serial.println(F("Could not determine TOC file timestamp, will try to fetch from Google Drive"));
            shouldFetchFromDrive = true;
        }
    } else if (!localTocExists) {
        if (batteryConservationMode) {
            Serial.println(F("TOC file does not exist locally and battery conservation mode is on - returning empty list"));
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
        error = client.list_files(config.driveFolderId, files, GOOGLE_DRIVE_LIST_PAGE_SIZE);

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

bool google_drive::save_toc_to_file(const String& filePath, const google_drive_files_list& filesList)
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

bool google_drive::save_toc_to_file_chunked(fs::File& file, const google_drive_files_list& filesList)
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
    return string_utils::build_path(config.localPath, config.localTocFilename);
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

google_drive_file google_drive::get_toc_file_by_index(const String& filePath, size_t index, photo_frame_error_t* error)
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

    // Create file paths using optimized utility functions
    String tempBasename = string_utils::build_string("temp_", file.name);
    String tempPath = string_utils::build_path(config.localPath, tempBasename);
    String finalPath = string_utils::build_path(config.localPath, file.name);

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

    return finalFile;
}

#if !defined(USE_INSECURE_TLS)
String google_drive::load_root_ca_certificate(sd_card& sdCard)
{
    Serial.print(F("Loading Google Drive root CA from: "));
    Serial.println(GOOGLE_DRIVE_ROOT_CA);

    if (!sdCard.is_initialized()) {
        Serial.println(F("SD card not initialized, cannot load root CA"));
        return String();
    }

    if (!sdCard.file_exists(GOOGLE_DRIVE_ROOT_CA)) {
        Serial.print(F("Root CA file not found: "));
        Serial.println(GOOGLE_DRIVE_ROOT_CA);
        return String();
    }

    fs::File certFile = sdCard.open(GOOGLE_DRIVE_ROOT_CA, FILE_READ);
    if (!certFile) {
        Serial.print(F("Failed to open root CA file: "));
        Serial.println(GOOGLE_DRIVE_ROOT_CA);
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
#endif // !USE_INSECURE_TLS

uint32_t google_drive::cleanup_temporary_files(sd_card& sdCard, const google_drive_config& config, boolean force)
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

    // first delete the toc
    String tocPath = string_utils::build_path(config.localPath, config.localTocFilename);
    bool tocDeleted = false;
    if (sdCard.file_exists(tocPath.c_str())) {
        Serial.print(F("Found TOC file: "));
        Serial.println(tocPath);

        time_t tocAge = sdCard.get_file_age(tocPath.c_str());
        Serial.print(F("TOC file age: "));
        Serial.print(tocAge);
        Serial.print(F(" seconds, max age: "));
        Serial.println(config.tocMaxAge);

        if (force || tocAge > config.tocMaxAge) {
            if (sdCard.remove(tocPath.c_str())) {
                Serial.println(F("Successfully removed TOC file"));
                cleanedCount++;
            } else {
                Serial.println(F("Failed to remove TOC file"));
            }
            tocDeleted = true;
        }
    }

    // if force is true, delete all the files inside the localPath folder
    // otherwise only those max age is greater than tocMaxAge, or the sdcard free space is under 20%

    if (!force && (freeBytes < SD_CARD_FREE_SPACE_THRESHOLD)) {
        Serial.print(F("SD card free space is under "));
        Serial.print(SD_CARD_FREE_SPACE_THRESHOLD / 1024 / 1024);
        Serial.println(F(" MB, forcing cleanup of all temporary files"));
        force = true;
    }

    // if force is true delete all files, otherwise delete those files
    // older than tocMaxAge
    if (force) {
        Serial.println(F("Force cleanup enabled, removing all temporary files"));
        if (sdCard.file_exists(config.localPath)) {
            cleanedCount += sdCard.rmdir(config.localPath);
        }
    } else if (tocDeleted) {
        Serial.println(F("Removing files older than max age"));
        fs::File root = sdCard.open(config.localPath, FILE_READ);
        if (!root || root.isDirectory() == false) {
            Serial.println(F("Failed to open directory"));
            return 0;
        }

        fs::File file = root.openNextFile();
        time_t currentTime = time(nullptr);

        while (file) {
            String fileName = file.name();
            time_t lastModified = file.getLastWrite();
            time_t elapsedSeconds = currentTime - lastModified;

            Serial.print(F("Checking file: "));
            Serial.print(fileName);
            Serial.print(F(", Last modified: "));
            Serial.print(lastModified);
            Serial.print(F(", Elapsed: "));
            Serial.print(elapsedSeconds);
            Serial.println();

            if (elapsedSeconds >= config.tocMaxAge) {
                Serial.print(F("Removing old file: "));
                Serial.println(fileName);
                sdCard.remove(fileName.c_str());
                cleanedCount++;
            }
            file = root.openNextFile();
        }

        root.close();
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

} // namespace photo_frame