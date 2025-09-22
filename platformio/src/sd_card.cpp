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

#include "sd_card.h"

namespace photo_frame {

photo_frame_error_t scan_directory(uint32_t* index,
                                   sd_card_entry* file_entry,
                                   const char* path,
                                   const char* extension,
                                   uint32_t& current_index) {
    Serial.print("[sdcard] scan_directory | path: ");
    Serial.print(path);
    Serial.print(", ext: ");
    Serial.print(extension);
    Serial.print(", current_index: ");
    Serial.print(current_index);
    Serial.print(", index: ");
    Serial.print(*index);
    Serial.println();

    bool found    = false;
    fs::File root = SD_MMC.open(path, FILE_READ); // Open the root directory

    if (!root) {
        Serial.println("[sdcard] Failed to open root directory");
        return error_type::CardOpenFileFailed;
    } else {
        while (true) {
            String entry = root.getNextFileName();

            if (!entry || entry.isEmpty()) {
                Serial.print(current_index);
                Serial.println("[sdcard] No more files in directory");
                root.close();
                return error_type::SdCardFileNotFound;
            }

            String file_name = entry.substring(entry.lastIndexOf('/') + 1);

#ifdef DEBUG_SD_CARD
            Serial.print(current_index);
            Serial.print(F(") "));
            Serial.print(entry);
            Serial.print(F("... "));
#endif // DEBUG_SD_CARD

            if (file_name.startsWith(".") || entry.endsWith(extension) == false) {
#ifdef DEBUG_SD_CARD
                Serial.println("[sdcard] skipping");
#endif // DEBUG_SD_CARD
                continue;
            } else {
#ifdef DEBUG_SD_CARD
                Serial.println(F("[sdcard] ok"));
#endif // DEBUG_SD_CARD
            }

            if (current_index == *index) {
                file_entry->name  = file_name.c_str();
                file_entry->path  = entry.c_str();
                file_entry->index = current_index;

                *index            = current_index;
                found             = true;
                break;
            }

            current_index++;
        }
    }
    root.close(); // Close the root directory

    if (found) {
        return error_type::None;
    } else {
        return error_type::SdCardFileNotFound;
    }
} // end scan_directory

photo_frame_error_t sd_card::begin() {
    if (initialized) {
        Serial.println("[sdcard] already initialized.");
        return error_type::None;
    }

    Serial.println("[sdcard] Initializing SD card using SD_MMC (SDIO)...");
    Serial.println("[sdcard] Using SD_MMC (SDIO) interface with custom pins:");
    Serial.println("[sdcard] CLK: GPIO14, CMD: GPIO17, D0: GPIO7, D1: GPIO3, D2: GPIO12, D3: GPIO11");
    Serial.println("[sdcard] Note: D0 moved from GPIO2 to GPIO7 to avoid NeoPixel LED conflict");

    // Configure custom SDIO pins before initialization
    SD_MMC.setPins(SD_MMC_CLK_PIN, SD_MMC_CMD_PIN, SD_MMC_D0_PIN, SD_MMC_D1_PIN, SD_MMC_D2_PIN, SD_MMC_D3_PIN);

    if (!SD_MMC.begin()) {
        return error_type::CardMountFailed;
    }

    cardType = SD_MMC.cardType();

    if (cardType == CARD_NONE) {
        SD_MMC.end();
        return error_type::NoSdCardAttached;
    } else if (cardType == CARD_UNKNOWN) {
        SD_MMC.end();
        return error_type::UnknownSdCardType;
    }

    initialized = true;
    return error_type::None;
} // end begin

void sd_card::end() {
    Serial.println("[sdcard] Ending SD card...");
    if (initialized) {
        SD_MMC.end();
        initialized = false;
    } else {
        Serial.println("[sdcard] SD card not initialized, nothing to end.");
    }
} // end end

void sd_card::print_stats() const {
    if (!initialized) {
        Serial.println("[sdcard] not initialized.");
        return;
    }
    Serial.print("[sdcard] Card Type: ");

    switch (cardType) {
    case CARD_MMC:     Serial.println("MMC"); break;
    case CARD_SD:      Serial.println("SDSC"); break;
    case CARD_SDHC:    Serial.println("SDHC"); break;
    case CARD_UNKNOWN: Serial.println("Unknown"); break;
    case CARD_NONE:    Serial.println("No SD card attached!"); break;
    default:           Serial.println("Unknown card type!"); break;
    }

    Serial.print("[sdcard] Card Size: ");
    Serial.print(SD_MMC.cardSize() / (1024 * 1024));
    Serial.println(" MB");
    Serial.print("[sdcard] Total Size: ");
    Serial.print(SD_MMC.totalBytes() / (1024 * 1024));
    Serial.println(" MB");
    Serial.print("[sdcard] Used Size: ");
    Serial.print(SD_MMC.usedBytes() / (1024 * 1024));
    Serial.println(" MB");
} // end printStats

time_t sd_card::get_last_modified(const char* path) const {
    if (!initialized) {
        return 0; // Return 0 if SD card is not initialized
    }
    fs::File file = SD_MMC.open(path, FILE_READ);
    if (!file) {
        return 0; // Return 0 if file cannot be opened
    }
    time_t lastModified = file.getLastWrite();
    file.close();
    return lastModified;
}

time_t sd_card::get_file_age(const char* path) const {
    time_t lastModified = get_last_modified(path);
    if (lastModified == 0) {
        return 0; // Return 0 if file cannot be accessed
    }
    return time(NULL) - lastModified;
}

void sd_card::list_files() const {
#ifdef DEBUG_SD_CARD
    Serial.println("[sdcard] Listing files on SD card with allowed extensions");
#endif // DEBUG_SD_CARD

    if (!initialized) {
        Serial.println("[sdcard] not initialized.");
        return;
    }

    File root = SD_MMC.open("/", FILE_READ);
    if (!root) {
        Serial.println("[sdcard] Failed to open root directory");
        return;
    }

    uint32_t fileCount = 1;
    String entry       = root.getNextFileName();
    while (entry.length() > 0) {
        String file_name = entry.substring(entry.lastIndexOf('/') + 1);
        bool hasAllowedExtension = false;

        // Check if file has any of the allowed extensions
        for (size_t i = 0; i < ALLOWED_EXTENSIONS_COUNT; i++) {
            if (file_name.endsWith(ALLOWED_FILE_EXTENSIONS[i])) {
                hasAllowedExtension = true;
                break;
            }
        }

        if (hasAllowedExtension && !file_name.startsWith("/") && !file_name.startsWith(".")) {
#ifdef DEBUG_SD_CARD
            Serial.print(fileCount);
            Serial.print(") entry: ");
            Serial.println(file_name);
#endif // DEBUG_SD_CARD

            fileCount++;
        }
        entry = root.getNextFileName();
    }
    root.close();
} // end listFiles

bool sd_card::file_exists(const char* path) const {
    Serial.print("[sdcard] file_exists | path: ");
    Serial.println(path);

    if (!initialized) {
        Serial.println("[sdcard] SD card not initialized.");
        return false;
    }

    if (!path || strlen(path) == 0) {
        Serial.println("[sdcard] Invalid path provided");
        return false;
    }

    return SD_MMC.exists(path);
} // end fileExists

uint32_t sd_card::count_files() const {
    Serial.println("[sdcard] count_files with allowed extensions");

    if (!initialized) {
        return 0;
    }

    uint32_t count = 0;
    File root      = SD_MMC.open("/", FILE_READ);
    if (!root) {
        Serial.println("[sdcard] Failed to open root directory");
        return count;
    }

    auto start_time __attribute__((unused)) = millis();
    bool is_dir                             = false;
    String path                             = root.getNextFileName(&is_dir);
    while (path && !path.isEmpty()) {
        String file_name = path.substring(path.lastIndexOf('/') + 1);
        bool hasAllowedExtension = false;

        // Check if file has any of the allowed extensions
        for (size_t i = 0; i < ALLOWED_EXTENSIONS_COUNT; i++) {
            if (path.endsWith(ALLOWED_FILE_EXTENSIONS[i])) {
                hasAllowedExtension = true;
                break;
            }
        }

        if (!is_dir && !file_name.startsWith(".") && !file_name.startsWith("/") && hasAllowedExtension) {
            count++;
        }
        path = root.getNextFileName(&is_dir);
    }

#ifdef DEBUG_SD_CARD
    auto elapsed_time = millis() - start_time;
    Serial.print("[sdcard] Time taken to count files: ");
    Serial.print(elapsed_time);
    Serial.println(" ms");
#endif // DEBUG_SD_CARD

    root.close();
    return count;
} // end getFileCount

fs::File sd_card::open(const char* path, const char* mode, bool create) {
    Serial.print("[sdcard] sd_card::open | path: ");
    Serial.print(path);
    Serial.print(", mode: ");
    Serial.println(mode);

    if (!initialized) {
        Serial.println("[sdcard] SD card not initialized.");
        return fs::File();
    }

    fs::File file = SD_MMC.open(path, mode, create);
    return file;
} // end open

bool sd_card::rename(const char* pathFrom, const char* pathTo, bool overwrite) {
    Serial.print("[sdcard] sd_card::rename | pathFrom: ");
    Serial.print(pathFrom);
    Serial.print(", pathTo: ");
    Serial.println(pathTo);

    if (!initialized) {
        Serial.println("[sdcard] SD card not initialized.");
        return false;
    }

    if (!pathFrom || !pathTo) {
        Serial.println("[sdcard] Invalid file paths provided.");
        return false;
    }

    if (SD_MMC.exists(pathTo)) {
        Serial.println("[sdcard] Destination file already exists.");
        if (overwrite) {
            Serial.println("[sdcard] Overwriting the existing file.");
            if (!SD_MMC.remove(pathTo)) {
                Serial.println("[sdcard] Failed to remove existing file.");
                return false;
            }
        } else {
            Serial.println("[sdcard] Overwrite not allowed. Rename operation aborted.");
            return false;
        }
    }

    if (SD_MMC.rename(pathFrom, pathTo)) {
        Serial.println("[sdcard] File renamed successfully.");
        return true;
    } else {
        Serial.println("[sdcard] Failed to rename file.");
        return false;
    }
}

bool sd_card::cleanup_dir(const char* path) {
    Serial.print("[sdcard] sd_card::cleanup_dir | path: ");
    Serial.println(path);

    if (!initialized) {
        Serial.println("[sdcard] SD card not initialized.");
        return false;
    }

    if (!path) {
        Serial.println("[sdcard] Invalid directory path provided.");
        return false;
    }

    if (!SD_MMC.exists(path)) {
        Serial.println("[sdcard] Directory does not exist.");
        return false;
    }

    fs::File dir = SD_MMC.open(path, FILE_READ);
    if (!dir) {
        Serial.println("[sdcard] Failed to open directory for cleanup.");
        return false;
    }

    if (!dir.isDirectory()) {
        Serial.println("[sdcard] Provided path is not a directory.");
        return false;
    }

    bool allRemoved = true;
    bool isDir      = false;
    String entry    = dir.getNextFileName(&isDir);

    while (entry && !entry.isEmpty()) {
#ifdef DEBUG_SD_CARD
        Serial.print("[sdcard] Processing entry: ");
        Serial.print(entry);
        Serial.print(" (isDir: ");
        Serial.print(isDir);
        Serial.println(")");
#endif // DEBUG_SD_CARD

        if (isDir) {
            // Recursively cleanup and remove subdirectory
            if (!cleanup_dir(entry.c_str())) {
                Serial.print("[sdcard] Failed to cleanup subdirectory: ");
                Serial.println(entry);
                allRemoved = false;
            } else {
                // After cleanup, remove the empty subdirectory
                if (!SD_MMC.rmdir(entry.c_str())) {
                    Serial.print("[sdcard] Failed to remove empty subdirectory: ");
                    Serial.println(entry);
                    allRemoved = false;
                }
            }
        } else {
            // Remove file
            if (!SD_MMC.remove(entry.c_str())) {
                Serial.print("[sdcard] Failed to remove file: ");
                Serial.println(entry);
                allRemoved = false;
            }
        }

        entry = dir.getNextFileName(&isDir);
    }

    dir.close();

    if (!allRemoved) {
        Serial.println("[sdcard] Not all files/directories could be removed during cleanup.");
        return false;
    }

#ifdef DEBUG_SD_CARD
    Serial.println("[sdcard] Directory cleanup completed successfully.");
#endif // DEBUG_SD_CARD
    return true;
}

bool sd_card::rmdir(const char* path) {
    Serial.print("[sdcard] sd_card::rmdir | path: ");
    Serial.println(path);

    if (!initialized) {
        Serial.println("[sdcard] SD card not initialized.");
        return false;
    }

    if (!path) {
        Serial.println("[sdcard] Invalid directory path provided.");
        return false;
    }

    if (!SD_MMC.exists(path)) {
        Serial.println("[sdcard] Directory does not exist.");
        return false;
    }

    // First cleanup all files and subdirectories recursively
    if (!cleanup_dir(path)) {
        Serial.println("[sdcard] Failed to cleanup directory contents.");
        return false;
    }

    // Now remove the empty directory
    if (SD_MMC.rmdir(path)) {
        Serial.println("[sdcard] Directory removed successfully.");
        return true;
    } else {
        Serial.println("[sdcard] Failed to remove directory.");
        return false;
    }
}

bool sd_card::remove(const char* path) {
    Serial.print("[sdcard] sd_card::remove | path: ");
    Serial.println(path);

    if (!initialized) {
        Serial.println("[sdcard] SD card not initialized.");
        return false;
    }

    if (!path) {
        Serial.println("[sdcard] Invalid file path provided.");
        return false;
    }

    if (!SD_MMC.exists(path)) {
        Serial.println("[sdcard] File does not exist, nothing to remove.");
        return true; // Consider it successful if file doesn't exist
    }

    if (SD_MMC.remove(path)) {
        Serial.println("[sdcard] File removed successfully.");
        return true;
    } else {
        Serial.println("[sdcard] Failed to remove file.");
        return false;
    }
}

size_t sd_card::get_file_size(const char* path) const {
    if (!initialized) {
        Serial.println("[sdcard] SD card not initialized.");
        return 0;
    }

    if (!path) {
        Serial.println("[sdcard] Invalid file path provided.");
        return 0;
    }

    if (!SD_MMC.exists(path)) {
        return 0; // File doesn't exist
    }

    fs::File file = SD_MMC.open(path, FILE_READ);
    if (!file) {
        Serial.print("[sdcard] Failed to open file for size check: ");
        Serial.println(path);
        return 0;
    }

    size_t fileSize = file.size();
    file.close();

    return fileSize;
}

bool sd_card::is_directory(const char* path) const {
    if (!initialized) {
        Serial.println("[sdcard] SD card not initialized.");
        return false;
    }

    if (!path) {
        Serial.println("[sdcard] Invalid directory path provided.");
        return false;
    }

    if (!SD_MMC.exists(path)) {
        Serial.println("[sdcard] Directory does not exist.");
        return false;
    }

    fs::File file = SD_MMC.open(path, FILE_READ);
    if (!file) {
        Serial.print("[sdcard] Failed to open directory for reading: ");
        Serial.println(path);
        return false;
    }

    bool isDir = file.isDirectory();
    file.close();
    return isDir;
}

bool sd_card::is_file(const char* path) const {
    if (!initialized) {
        Serial.println("[sdcard] SD card not initialized.");
        return false;
    }

    if (!path) {
        Serial.println("[sdcard] Invalid file path provided.");
        return false;
    }

    fs::File file = SD_MMC.open(path, FILE_READ);
    if (!file) {
        Serial.print("[sdcard] Failed to open file for reading: ");
        Serial.println(path);
        return false;
    }

    bool isFile = !file.isDirectory();
    file.close();
    return isFile;
}

bool sd_card::create_directories(const char* path) {
    if (!initialized) {
        Serial.println(F("[sdcard] SD card not initialized"));
        return false;
    }

    if (!path || strlen(path) == 0) {
        return true; // Empty path is considered success
    }

    String dirPath = String(path);

    // Remove trailing slash if present
    if (dirPath.endsWith("/")) {
        dirPath = dirPath.substring(0, dirPath.length() - 1);
    }

    // If directory already exists, return success
    if (SD_MMC.exists(dirPath.c_str())) {
        return true;
    }

    // Split path into components and create each directory level
    String currentPath = "";
    int start          = 0;
    if (dirPath.startsWith("/")) {
        start       = 1; // Skip leading slash
        currentPath = "/";
    }

    while (start < dirPath.length()) {
        int end = dirPath.indexOf('/', start);
        if (end == -1) {
            end = dirPath.length();
        }

        String dirName = dirPath.substring(start, end);
        if (dirName.length() > 0) {
            if (currentPath != "/" && !currentPath.endsWith("/")) {
                currentPath += "/";
            }
            currentPath += dirName;

            // Check if this directory level exists
            if (!SD_MMC.exists(currentPath.c_str())) {
#ifdef DEBUG_SD_CARD
                Serial.print(F("[sdcard] Creating directory: "));
                Serial.println(currentPath);
#endif // DEBUG_SD_CARD

                if (!SD_MMC.mkdir(currentPath.c_str())) {
                    Serial.print(F("[sdcard] Failed to create directory: "));
                    Serial.println(currentPath);
                    return false;
                }
            }
        }

        start = end + 1;
    }

    return true;
}

uint64_t sd_card::total_bytes() const {
    if (!initialized) {
        Serial.println("[sdcard] SD card not initialized.");
        return 0;
    }

    return SD_MMC.totalBytes();
}

uint64_t sd_card::used_bytes() const {
    if (!initialized) {
        Serial.println("[sdcard] SD card not initialized.");
        return 0;
    }

    return SD_MMC.usedBytes();
}

uint64_t sd_card::card_size() const {
    if (!initialized) {
        Serial.println("[sdcard] SD card not initialized.");
        return 0;
    }

    return SD_MMC.cardSize();
}

} // namespace photo_frame