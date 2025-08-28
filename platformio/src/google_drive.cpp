#include "google_drive.h"
#include "sd_card.h"
#include "string_utils.h"
#include <ArduinoJson.h>
#include <time.h>

extern photo_frame::sd_card sdCard;

// Memory monitoring utility
void logMemoryUsage(const char* context) {
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
bool checkMemoryAvailable(size_t requiredBytes) {
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

google_drive_files_list google_drive::retrieve_toc(bool force, bool batteryConservationMode) {
    google_drive_files_list result;
    
    logMemoryUsage("TOC Retrieve Start");

    // Build TOC path using optimized utility function
    String tocFullPath = photo_frame::string_utils::buildPath(config.localPath, config.localTocFilename);
    bool shouldFetchFromDrive = force;
    bool localTocExists = sdCard.file_exists(tocFullPath.c_str());
    
    if (!force && localTocExists) {
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
        
        // Get access token first
        photo_frame::photo_frame_error_t error = client.get_access_token();
        if (error != photo_frame::error_type::None) {
            Serial.print(F("Failed to get access token: "));
            Serial.println(error.code);
            
            // Fallback to local TOC if available and not forced
            if (!force && localTocExists) {
                Serial.println(F("Falling back to cached TOC file"));
                return load_toc_from_file(tocFullPath);
            }
            return result; // Return empty list
        }
        
        // Check memory before Google Drive operations
        if (!checkMemoryAvailable(4096)) {
            Serial.println(F("Insufficient memory for Google Drive operations"));
            if (!force && localTocExists) {
                Serial.println(F("Falling back to cached TOC file"));
                return load_toc_from_file(tocFullPath);
            }
            return result; // Return empty list
        }
        
        // Fetch files from Google Drive
        std::vector<google_drive_file> files;
        error = client.list_files(config.driveFolderId, files);
        
        if (error != photo_frame::error_type::None) {
            Serial.print(F("Failed to list files from Google Drive: "));
            Serial.println(error.code);
            
            // Fallback to local TOC if available and not forced
            if (!force && localTocExists) {
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

google_drive_files_list google_drive::load_toc_from_file(const String& filePath) {
    google_drive_files_list result;
    
    logMemoryUsage("TOC Load Start");
    
    fs::File file = sdCard.open(filePath.c_str(), FILE_READ);
    if (!file) {
        Serial.print(F("Failed to open TOC file: "));
        Serial.println(filePath);
        return result;
    }
    
    // Read JSON from file
    String jsonContent = file.readString();
    file.close();
    
    // Calculate required JSON document size based on estimated file count
    // Estimate: 200 bytes per file entry + 200 bytes overhead
    size_t estimatedSize = jsonContent.length() + 512;
    const size_t maxSize = 16384; // 16KB limit
    
    if (estimatedSize > maxSize) {
        Serial.print(F("TOC file too large: "));
        Serial.print(estimatedSize);
        Serial.println(F(" bytes. Using streaming parser."));
        return load_toc_from_file_streaming(filePath);
    }
    
    // Use static allocation for smaller files
    StaticJsonDocument<8192> doc;
    DeserializationError error = deserializeJson(doc, jsonContent);
    
    if (error) {
        Serial.print(F("Failed to parse TOC JSON: "));
        Serial.println(error.c_str());
        
        // Try streaming parser as fallback
        Serial.println(F("Trying streaming parser..."));
        return load_toc_from_file_streaming(filePath);
    }
    
    // Extract files array
    JsonArray filesArray = doc["files"];
    result.files.reserve(filesArray.size()); // Pre-allocate vector
    
    for (JsonObject fileObj : filesArray) {
        google_drive_file driveFile(
            fileObj["id"].as<String>(),
            fileObj["name"].as<String>(),
            fileObj["mimeType"].as<String>(),
            fileObj["modifiedTime"].as<String>()
        );
        result.files.push_back(driveFile);
    }
    
    Serial.print(F("Loaded "));
    Serial.print(result.files.size());
    Serial.println(F(" files from cached TOC"));
    
    logMemoryUsage("TOC Load End");
    return result;
}

google_drive_files_list google_drive::load_toc_from_file_streaming(const String& filePath) {
    google_drive_files_list result;
    
    fs::File file = sdCard.open(filePath.c_str(), FILE_READ);
    if (!file) {
        Serial.print(F("Failed to open TOC file for streaming: "));
        Serial.println(filePath);
        return result;
    }
    
    // Simple streaming parser - reads line by line
    // Expected format: each file entry on separate line as JSON object
    String line;
    bool inFilesArray = false;
    
    while (file.available()) {
        line = file.readStringUntil('\n');
        line.trim();
        
        // Look for start of files array
        if (line.indexOf("\"files\"") >= 0) {
            inFilesArray = true;
            continue;
        }
        
        // Skip non-file entries
        if (!inFilesArray || line.length() < 10) {
            continue;
        }
        
        // Parse individual file entry with minimal memory usage
        StaticJsonDocument<512> fileDoc; // Small buffer for single file
        DeserializationError error = deserializeJson(fileDoc, line);
        
        if (error == DeserializationError::Ok) {
            google_drive_file driveFile(
                fileDoc["id"].as<String>(),
                fileDoc["name"].as<String>(),
                fileDoc["mimeType"].as<String>(),
                fileDoc["modifiedTime"].as<String>()
            );
            result.files.push_back(driveFile);
        }
        
        // Yield to prevent watchdog reset
        if (result.files.size() % 10 == 0) {
            yield();
        }
    }
    
    file.close();
    
    Serial.print(F("Loaded "));
    Serial.print(result.files.size());
    Serial.println(F(" files using streaming parser"));
    
    return result;
}

bool google_drive::save_toc_to_file(const String& filePath, const google_drive_files_list& filesList) {
    const size_t maxFiles = filesList.files.size();
    
    logMemoryUsage("TOC Save Start");
    
    // Check if we have enough memory for the operation
    size_t estimatedMemory = maxFiles * 200 + 1024; // Estimate memory needed
    if (!checkMemoryAvailable(estimatedMemory)) {
        Serial.println(F("Insufficient memory for TOC save operation"));
        return false;
    }
    
    // Try to create the file directly
    fs::File file = sdCard.open(filePath.c_str(), FILE_WRITE);
    if (!file) {
        Serial.print(F("Failed to create TOC file: "));
        Serial.println(filePath);
        return false;
    }
    
    // Use chunked writing for large file lists to avoid memory issues
    const size_t chunkSize = 20; // Process 20 files at a time
    
    if (maxFiles > chunkSize) {
        return save_toc_to_file_chunked(file, filesList);
    }
    
    // For smaller lists, use optimized static allocation
    StaticJsonDocument<4096> doc; // Reduced from 8192
    doc["timestamp"] = time(NULL);
    doc["fileCount"] = maxFiles;
    
    JsonArray filesArray = doc.createNestedArray("files");
    
    for (const google_drive_file& driveFile : filesList.files) {
        JsonObject fileObj = filesArray.createNestedObject();
        fileObj["id"] = driveFile.id;
        fileObj["name"] = driveFile.name;
        fileObj["mimeType"] = driveFile.mimeType;
        fileObj["modifiedTime"] = driveFile.modifiedTime;
        
        // Check if we're running out of memory in the document
        if (doc.memoryUsage() > doc.capacity() * 0.9) {
            Serial.println(F("JSON document nearly full, switching to chunked writing"));
            file.close();
            file = sdCard.open(filePath.c_str(), FILE_WRITE); // Reopen for chunked writing
            return save_toc_to_file_chunked(file, filesList);
        }
    }
    
    // Write JSON to file
    if (serializeJson(doc, file) == 0) {
        Serial.println(F("Failed to write TOC JSON to file"));
        file.close();
        return false;
    }
    
    file.close();
    
    Serial.print(F("Saved TOC with "));
    Serial.print(maxFiles);
    Serial.print(F(" files to: "));
    Serial.println(filePath);
    
    logMemoryUsage("TOC Save End");
    return true;
}

bool google_drive::save_toc_to_file_chunked(fs::File& file, const google_drive_files_list& filesList) {
    const size_t chunkSize = 15; // Process 15 files at a time
    const size_t totalFiles = filesList.files.size();
    
    Serial.print(F("Using chunked writing for "));
    Serial.print(totalFiles);
    Serial.println(F(" files"));
    
    // Write header manually
    file.print(F("{\"timestamp\":"));
    file.print(time(NULL));
    file.print(F(",\"fileCount\":"));
    file.print(totalFiles);
    file.print(F(",\"files\":["));
    
    // Process files in chunks
    for (size_t i = 0; i < totalFiles; i += chunkSize) {
        size_t endIdx = min(i + chunkSize, totalFiles);
        
        // Create small buffer for this chunk
        StaticJsonDocument<2048> chunkDoc; // Smaller buffer for chunk
        JsonArray chunkArray = chunkDoc.to<JsonArray>();
        
        // Add files to chunk
        for (size_t j = i; j < endIdx; j++) {
            const google_drive_file& driveFile = filesList.files[j];
            JsonObject fileObj = chunkArray.createNestedObject();
            fileObj["id"] = driveFile.id;
            fileObj["name"] = driveFile.name;
            fileObj["mimeType"] = driveFile.mimeType;
            fileObj["modifiedTime"] = driveFile.modifiedTime;
        }
        
        // Serialize chunk to temporary string with pre-allocation
        String chunkJson;
        size_t estimatedChunkSize = (endIdx - i) * 150; // Estimate ~150 bytes per file entry
        chunkJson.reserve(estimatedChunkSize);
        serializeJson(chunkArray, chunkJson);
        
        // Remove array brackets and write content
        chunkJson = chunkJson.substring(1, chunkJson.length() - 1); // Remove [ ]
        
        if (i > 0) {
            file.print(F(","));
        }
        file.print(chunkJson);
        
        // Yield periodically to prevent watchdog reset
        yield();
        
        // Report progress
        Serial.print(F("Processed chunk "));
        Serial.print((i / chunkSize) + 1);
        Serial.print(F("/"));
        Serial.println((totalFiles + chunkSize - 1) / chunkSize);
    }
    
    // Close JSON structure
    file.print(F("]}"));
    file.close();
    
    Serial.println(F("Chunked TOC save completed"));
    
    // Report final memory usage
    size_t freeHeap = ESP.getFreeHeap();
    Serial.print(F("Free heap after chunked save: "));
    Serial.println(freeHeap);
    
    return true;
}

fs::File google_drive::download_file(google_drive_file file, photo_frame::photo_frame_error_t* error) {
    fs::File emptyFile;
    
    // Initialize error to None
    if (error) {
        *error = photo_frame::error_type::None;
    }
    
    // Create file paths using optimized utility functions
    String tempBasename = photo_frame::string_utils::buildString("temp_", config.localImageBasename);
    String tempPath = photo_frame::string_utils::buildPath(config.localPath, tempBasename, config.localImageExtension);
    String finalPath = photo_frame::string_utils::buildPath(config.localPath, config.localImageBasename, config.localImageExtension);
    
    Serial.print(F("Downloading file: "));
    Serial.println(file.name);
    Serial.print(F("Temp path: "));
    Serial.println(tempPath);
    Serial.print(F("Final path: "));
    Serial.println(finalPath);
    
    // Open temporary file for writing
    fs::File tempFile = sdCard.open(tempPath.c_str(), FILE_WRITE);
    if (!tempFile) {
        Serial.print(F("Failed to create temporary file: "));
        Serial.println(tempPath);
        if (error) {
            *error = photo_frame::error_type::SdCardFileCreateFailed;
        }
        return emptyFile;
    }
    
    // Download file from Google Drive to temporary location
    photo_frame::photo_frame_error_t downloadError = client.download_file(file.id, &tempFile);
    tempFile.close();
    
    if (downloadError != photo_frame::error_type::None) {
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
    
    // Move temporary file to final destination
    bool moveSuccess = sdCard.rename(tempPath.c_str(), finalPath.c_str(), true); // true = overwrite existing
    
    if (!moveSuccess) {
        Serial.print(F("Failed to move file from temp to final location"));
        
        // Clean up temporary file on move failure
        Serial.print(F("Cleaning up temporary file after move failure: "));
        Serial.println(tempPath);
        if (!sdCard.remove(tempPath.c_str())) {
            Serial.println(F("Warning: Failed to remove temporary file after move failure"));
        } else {
            Serial.println(F("Temporary file cleaned up after move failure"));
        }
        
        if (error) {
            *error = photo_frame::error_type::SdCardFileCreateFailed;
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
            *error = photo_frame::error_type::SdCardFileOpenFailed;
        }
        return emptyFile;
    }
    
    return finalFile;
}

#if !defined(USE_INSECURE_TLS)
String google_drive::load_root_ca_certificate(photo_frame::sd_card& sdCard)
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
    if (!certContent.startsWith("-----BEGIN CERTIFICATE-----") || 
        !certContent.endsWith("-----END CERTIFICATE-----")) {
        Serial.println(F("Invalid certificate format - missing PEM markers"));
        return String();
    }
    
    Serial.print(F("Successfully loaded root CA certificate ("));
    Serial.print(certContent.length());
    Serial.println(F(" bytes)"));
    
    return certContent;
}
#endif // !USE_INSECURE_TLS

uint32_t google_drive::cleanup_temporary_files(photo_frame::sd_card& sdCard, const google_drive_config& config)
{
    Serial.println(F("Cleaning up temporary files from previous incomplete downloads..."));
    
    uint32_t cleanedCount = 0;
    
    if (!sdCard.is_initialized()) {
        Serial.println(F("SD card not initialized, cannot clean up temporary files"));
        return 0;
    }
    
    // Construct temporary file path pattern: "temp_<basename><extension>"
    String tempBasename = photo_frame::string_utils::buildString("temp_", config.localImageBasename);
    String tempPath = photo_frame::string_utils::buildPath(config.localPath, tempBasename, config.localImageExtension);
    
    Serial.print(F("Checking for temporary file: "));
    Serial.println(tempPath);
    
    if (sdCard.file_exists(tempPath.c_str())) {
        Serial.print(F("Found temporary file: "));
        Serial.println(tempPath);
        
        // Get file size for validation
        size_t fileSize = sdCard.get_file_size(tempPath.c_str());
        Serial.print(F("Temporary file size: "));
        Serial.print(fileSize);
        Serial.println(F(" bytes"));
        
        bool shouldRemove = false;
        String removalReason;
        
        // Check for empty or corrupted files
        if (fileSize == 0) {
            shouldRemove = true;
            removalReason = "empty file (0 bytes)";
        }
        // Check for suspiciously small files (less than 1KB might indicate incomplete download)
        else if (fileSize < 1024) {
            shouldRemove = true;
            removalReason = photo_frame::string_utils::buildString("file too small (", String(fileSize), " bytes)");
        }
        // Check for unreasonably large files (greater than 5MB might indicate corruption)
        else if (fileSize > 5242880) { // 5MB limit
            shouldRemove = true;
            removalReason = photo_frame::string_utils::buildString("file too large (", String(fileSize), " bytes)");
        }
        // If file seems reasonable size, still remove it as it's a leftover temporary file
        else {
            shouldRemove = true;
            removalReason = "leftover temporary file from previous session";
        }
        
        if (shouldRemove) {
            Serial.print(F("Removing temporary file: "));
            Serial.println(removalReason);
            
            if (sdCard.remove(tempPath.c_str())) {
                Serial.println(F("Successfully removed temporary file"));
                cleanedCount++;
            } else {
                Serial.println(F("Failed to remove temporary file"));
            }
        } else {
            Serial.println(F("Temporary file appears valid, keeping it"));
        }
    }
    
    Serial.print(F("Cleaned up "));
    Serial.print(cleanedCount);
    Serial.println(F(" temporary files"));
    
    return cleanedCount;
}