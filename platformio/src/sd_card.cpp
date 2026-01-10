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
#include "sd_card_toc_parser.h"
#include <algorithm>
#include <vector>

// Create HSPI instance for SD card when using separate SPI buses
// Only needed when SD_USE_SPI is defined (SPI mode, not SDIO)
#if defined(SD_USE_SPI) && defined(USE_HSPI_FOR_SD)
SPIClass hspi(HSPI);
#endif

namespace photo_frame {

photo_frame_error_t scan_directory(uint32_t* index,
                                   sd_card_entry* file_entry,
                                   const char* path,
                                   const char* extension,
                                   uint32_t& current_index) {
    log_d("scan_directory | path: %s, ext: %s, current_index: %u, index: %u",
          path,
          extension,
          current_index,
          *index);

    bool found    = false;
    fs::File root = SD_CARD_LIB.open(path, FILE_READ); // Open the root directory

    if (!root) {
        log_e("Failed to open root directory");
        return error_type::CardOpenFileFailed;
    } else {
        while (true) {
            String entry = root.getNextFileName();

            if (!entry || entry.isEmpty()) {
                log_d("%u [sdcard] No more files in directory", current_index);
                root.close();
                return error_type::SdCardFileNotFound;
            }

            String file_name = entry.substring(entry.lastIndexOf('/') + 1);

#ifdef DEBUG_SD_CARD
            log_d("%u) %s... ", current_index, entry.c_str());
#endif // DEBUG_SD_CARD

            if (file_name.startsWith(".") || entry.endsWith(extension) == false) {
#ifdef DEBUG_SD_CARD
                log_d("skipping");
#endif // DEBUG_SD_CARD
                continue;
            } else {
#ifdef DEBUG_SD_CARD
                log_d("ok");
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
        log_i("already initialized.");
        return error_type::None;
    }

#ifdef SD_USE_SPI
    log_i("Initializing SD card using SPI...");

#ifdef USE_HSPI_FOR_SD
    // Using separate HSPI bus for SD card
    log_i("Initializing HSPI bus for SD card");
    hspi.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
    delay(250); // Increased delay for SD card power-up and stabilization

    // Use 1MHz for more stable operation (balance between speed and reliability)
    // Too fast (2MHz+) causes timeouts, too slow (400kHz) may have issues during operations
    if (!SD_CARD_LIB.begin(SD_CS_PIN, hspi, 1000000U)) {
        log_e("Failed to initialize SD card on HSPI at 1MHz");
        return error_type::CardMountFailed;
    }
    log_i("SD card initialized successfully at 1MHz");
#else
    // Using default SPI bus for SD card
    log_i("Initializing default SPI bus for SD card");
    SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);

    if (!SD_CARD_LIB.begin(SD_CS_PIN, SPI)) {
        log_e("Failed to initialize SD card on default SPI");
        return error_type::CardMountFailed;
    }
#endif
#else
    log_i("Initializing SD card using SD_MMC (SDIO)...");

    // Configure custom SDIO pins before initialization
    SD_CARD_LIB.setPins(
        SD_MMC_CLK_PIN, SD_MMC_CMD_PIN, SD_MMC_D0_PIN, SD_MMC_D1_PIN, SD_MMC_D2_PIN, SD_MMC_D3_PIN);

    if (!SD_CARD_LIB.begin()) {
        return error_type::CardMountFailed;
    }
#endif

    cardType = SD_CARD_LIB.cardType();

    if (cardType == CARD_NONE) {
        SD_CARD_LIB.end();
        return error_type::NoSdCardAttached;
    } else if (cardType == CARD_UNKNOWN) {
        SD_CARD_LIB.end();
        return error_type::UnknownSdCardType;
    }

    initialized = true;
    return error_type::None;
} // end begin

void sd_card::end() {
    log_i("Ending SD card...");
    if (initialized) {
        SD_CARD_LIB.end();

#if defined(SD_USE_SPI) && defined(USE_HSPI_FOR_SD)
        // Also end the HSPI bus to fully release SPI resources
        log_i("Ending HSPI bus");
        hspi.end();
#endif

        initialized = false;

        // Allow time for SPI bus to fully shutdown and release resources
        delay(200);
        log_i("SD card and SPI bus shutdown complete");
    } else {
        log_i("SD card not initialized, nothing to end.");
    }
} // end end

void sd_card::print_stats() const {
    if (!initialized) {
        log_i("not initialized.");
        return;
    }

    const char* card_type_str;
    switch (cardType) {
    case CARD_MMC:     card_type_str = "MMC"; break;
    case CARD_SD:      card_type_str = "SDSC"; break;
    case CARD_SDHC:    card_type_str = "SDHC"; break;
    case CARD_UNKNOWN: card_type_str = "Unknown"; break;
    case CARD_NONE:    card_type_str = "No SD card attached!"; break;
    default:           card_type_str = "Unknown card type!"; break;
    }
    log_i("Card Type: %s", card_type_str);

    log_i("Card Size: %llu MB", SD_CARD_LIB.cardSize() / (1024 * 1024));
    log_i("Total Size: %llu MB", SD_CARD_LIB.totalBytes() / (1024 * 1024));
    log_i("Used Size: %llu MB", SD_CARD_LIB.usedBytes() / (1024 * 1024));
} // end printStats

time_t sd_card::get_last_modified(const char* path) const {
    if (!initialized) {
        return 0; // Return 0 if SD card is not initialized
    }
    fs::File file = SD_CARD_LIB.open(path, FILE_READ);
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
    log_d("Listing files on SD card with allowed extensions");
#endif // DEBUG_SD_CARD

    if (!initialized) {
        log_i("not initialized.");
        return;
    }

    File root = SD_CARD_LIB.open("/", FILE_READ);
    if (!root) {
        log_e("Failed to open root directory");
        return;
    }

    uint32_t fileCount = 1;
    String entry       = root.getNextFileName();
    while (entry.length() > 0) {
        String file_name         = entry.substring(entry.lastIndexOf('/') + 1);
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
            log_d("%u) entry: %s", fileCount, file_name.c_str());
#endif // DEBUG_SD_CARD

            fileCount++;
        }
        entry = root.getNextFileName();
    }
    root.close();
} // end listFiles

bool sd_card::file_exists(const char* path) const {
    if (!initialized) {
        log_e("SD card not initialized.");
        return false;
    }

    if (!path || strlen(path) == 0) {
        log_e("Invalid path provided");
        return false;
    }

    log_d("file_exists | path: %s", path);
    return SD_CARD_LIB.exists(path);
} // end fileExists

uint32_t sd_card::count_files() const {
    log_d("count_files with allowed extensions");

    if (!initialized) {
        return 0;
    }

    uint32_t count = 0;
    File root      = SD_CARD_LIB.open("/", FILE_READ);
    if (!root) {
        log_e("Failed to open root directory");
        return count;
    }

    auto start_time __attribute__((unused)) = millis();
    bool is_dir                             = false;
    String path                             = root.getNextFileName(&is_dir);
    while (path && !path.isEmpty()) {
        String file_name         = path.substring(path.lastIndexOf('/') + 1);
        bool hasAllowedExtension = false;

        // Check if file has any of the allowed extensions
        for (size_t i = 0; i < ALLOWED_EXTENSIONS_COUNT; i++) {
            if (path.endsWith(ALLOWED_FILE_EXTENSIONS[i])) {
                hasAllowedExtension = true;
                break;
            }
        }

        if (!is_dir && !file_name.startsWith(".") && !file_name.startsWith("/") &&
            hasAllowedExtension) {
            count++;
        }
        path = root.getNextFileName(&is_dir);
    }

#ifdef DEBUG_SD_CARD
    auto elapsed_time = millis() - start_time;
    log_d("Time taken to count files: %lu ms", elapsed_time);
#endif // DEBUG_SD_CARD

    root.close();
    return count;
} // end getFileCount

fs::File sd_card::open(const char* path, const char* mode, bool create) {
    log_d("sd_card::open | path: %s, mode: %s", path, mode);

    if (!initialized) {
        log_e("SD card not initialized.");
        return fs::File();
    }

    fs::File file = SD_CARD_LIB.open(path, mode, create);
    return file;
} // end open

bool sd_card::rename(const char* pathFrom, const char* pathTo, bool overwrite) {
    log_d("sd_card::rename | pathFrom: %s, pathTo: %s", pathFrom, pathTo);

    if (!initialized) {
        log_e("SD card not initialized.");
        return false;
    }

    if (!pathFrom || !pathTo) {
        log_e("Invalid file paths provided.");
        return false;
    }

    if (SD_CARD_LIB.exists(pathTo)) {
        log_w("Destination file already exists.");
        if (overwrite) {
            log_i("Overwriting the existing file.");
            if (!SD_CARD_LIB.remove(pathTo)) {
                log_e("Failed to remove existing file.");
                return false;
            }
        } else {
            log_w("Overwrite not allowed. Rename operation aborted.");
            return false;
        }
    }

    if (SD_CARD_LIB.rename(pathFrom, pathTo)) {
        log_i("File renamed successfully.");
        return true;
    } else {
        log_e("Failed to rename file.");
        return false;
    }
}

bool sd_card::cleanup_dir(const char* path) {
    log_d("sd_card::cleanup_dir | path: %s", path);

    if (!initialized) {
        log_e("SD card not initialized.");
        return false;
    }

    if (!path) {
        log_e("Invalid directory path provided.");
        return false;
    }

    if (!SD_CARD_LIB.exists(path)) {
        log_e("Directory does not exist.");
        return false;
    }

    fs::File dir = SD_CARD_LIB.open(path, FILE_READ);
    if (!dir) {
        log_e("Failed to open directory for cleanup.");
        return false;
    }

    if (!dir.isDirectory()) {
        log_e("Provided path is not a directory.");
        return false;
    }

    bool allRemoved = true;
    bool isDir      = false;
    String entry    = dir.getNextFileName(&isDir);

    while (entry && !entry.isEmpty()) {
#ifdef DEBUG_SD_CARD
        log_d("Processing entry: %s (isDir: %d)", entry.c_str(), isDir);
#endif // DEBUG_SD_CARD

        if (isDir) {
            // Recursively cleanup and remove subdirectory
            if (!cleanup_dir(entry.c_str())) {
                log_e("Failed to cleanup subdirectory: %s", entry.c_str());
                allRemoved = false;
            } else {
                // After cleanup, remove the empty subdirectory
                if (!SD_CARD_LIB.rmdir(entry.c_str())) {
                    log_e("Failed to remove empty subdirectory: %s", entry.c_str());
                    allRemoved = false;
                }
            }
        } else {
            // Remove file
            if (!SD_CARD_LIB.remove(entry.c_str())) {
                log_e("Failed to remove file: %s", entry.c_str());
                allRemoved = false;
            }
        }

        entry = dir.getNextFileName(&isDir);
    }

    dir.close();

    if (!allRemoved) {
        log_e("Not all files/directories could be removed during cleanup.");
        return false;
    }

#ifdef DEBUG_SD_CARD
    log_d("Directory cleanup completed successfully.");
#endif // DEBUG_SD_CARD
    return true;
}

bool sd_card::rmdir(const char* path) {
    log_d("sd_card::rmdir | path: %s", path);

    if (!initialized) {
        log_e("SD card not initialized.");
        return false;
    }

    if (!path) {
        log_e("Invalid directory path provided.");
        return false;
    }

    if (!SD_CARD_LIB.exists(path)) {
        log_e("Directory does not exist.");
        return false;
    }

    // First cleanup all files and subdirectories recursively
    if (!cleanup_dir(path)) {
        log_e("Failed to cleanup directory contents.");
        return false;
    }

    // Now remove the empty directory
    if (SD_CARD_LIB.rmdir(path)) {
        log_i("Directory removed successfully.");
        return true;
    } else {
        log_e("Failed to remove directory.");
        return false;
    }
}

bool sd_card::remove(const char* path) {
    log_d("sd_card::remove | path: %s", path);

    if (!initialized) {
        log_e("SD card not initialized.");
        return false;
    }

    if (!path) {
        log_e("Invalid file path provided.");
        return false;
    }

    if (!SD_CARD_LIB.exists(path)) {
        log_i("File does not exist, nothing to remove.");
        return true; // Consider it successful if file doesn't exist
    }

    if (SD_CARD_LIB.remove(path)) {
        log_i("File removed successfully.");
        return true;
    } else {
        log_e("Failed to remove file.");
        return false;
    }
}

size_t sd_card::get_file_size(const char* path) const {
    if (!initialized) {
        log_e("SD card not initialized.");
        return 0;
    }

    if (!path) {
        log_e("Invalid file path provided.");
        return 0;
    }

    if (!SD_CARD_LIB.exists(path)) {
        return 0; // File doesn't exist
    }

    fs::File file = SD_CARD_LIB.open(path, FILE_READ);
    if (!file) {
        log_e("Failed to open file for size check: %s", path);
        return 0;
    }

    size_t fileSize = file.size();
    file.close();

    return fileSize;
}

bool sd_card::is_directory(const char* path) const {
    if (!initialized) {
        log_e("SD card not initialized.");
        return false;
    }

    if (!path) {
        log_e("Invalid directory path provided.");
        return false;
    }

    if (!SD_CARD_LIB.exists(path)) {
        log_e("Directory does not exist.");
        return false;
    }

    fs::File file = SD_CARD_LIB.open(path, FILE_READ);
    if (!file) {
        log_e("Failed to open directory for reading: %s", path);
        return false;
    }

    bool isDir = file.isDirectory();
    file.close();
    return isDir;
}

bool sd_card::is_file(const char* path) const {
    if (!initialized) {
        log_e("SD card not initialized.");
        return false;
    }

    if (!path) {
        log_e("Invalid file path provided.");
        return false;
    }

    fs::File file = SD_CARD_LIB.open(path, FILE_READ);
    if (!file) {
        log_e("Failed to open file for reading: %s", path);
        return false;
    }

    bool isFile = !file.isDirectory();
    file.close();
    return isFile;
}

bool sd_card::create_directories(const char* path) {
    if (!initialized) {
        log_e("SD card not initialized");
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
    if (SD_CARD_LIB.exists(dirPath.c_str())) {
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
            if (!SD_CARD_LIB.exists(currentPath.c_str())) {
#ifdef DEBUG_SD_CARD
                log_d("Creating directory: %s", currentPath.c_str());
#endif // DEBUG_SD_CARD

                if (!SD_CARD_LIB.mkdir(currentPath.c_str())) {
                    log_e("Failed to create directory: %s", currentPath.c_str());
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
        log_e("SD card not initialized.");
        return 0;
    }

    return SD_CARD_LIB.totalBytes();
}

uint64_t sd_card::used_bytes() const {
    if (!initialized) {
        log_e("SD card not initialized.");
        return 0;
    }

    return SD_CARD_LIB.usedBytes();
}

uint64_t sd_card::card_size() const {
    if (!initialized) {
        log_e("SD card not initialized.");
        return 0;
    }

    return SD_CARD_LIB.cardSize();
}

std::vector<String> sd_card::list_files_in_directory(const char* dir_path,
                                                     const char* extension) const {
    std::vector<String> files;

    if (!initialized) {
        log_e("SD card not initialized.");
        return files;
    }

    if (!is_directory(dir_path)) {
        log_e("Directory does not exist: %s", dir_path);
        return files;
    }

    File dir = SD_CARD_LIB.open(dir_path);
    if (!dir) {
        log_e("Failed to open directory: %s", dir_path);
        return files;
    }

    bool isDir      = false;
    String fileName = dir.getNextFileName(&isDir);

    while (!fileName.isEmpty()) {
        if (!isDir && fileName.endsWith(extension)) {
            // Extract just the filename from the full path to check for hidden files
            int lastSlash   = fileName.lastIndexOf('/');
            String baseName = (lastSlash >= 0) ? fileName.substring(lastSlash + 1) : fileName;

            if (!baseName.startsWith(".")) {
                // fileName already contains the full path from getNextFileName
                files.push_back(fileName);
            }
        }
        fileName = dir.getNextFileName(&isDir);
    }
    dir.close();

    // Sort files alphabetically
    std::sort(files.begin(), files.end());

    return files;
}

String
sd_card::get_file_at_index(const char* dir_path, uint32_t index, const char* extension) const {
    if (!initialized) {
        log_e("SD card not initialized.");
        return String();
    }

    if (!is_directory(dir_path)) {
        log_e("Directory does not exist: %s", dir_path);
        return String();
    }

    File dir = SD_CARD_LIB.open(dir_path);
    if (!dir) {
        log_e("Failed to open directory: %s", dir_path);
        return String();
    }

    uint32_t currentIndex = 0;
    bool isDir            = false;
    String fileName       = dir.getNextFileName(&isDir);
    String result;

    // Find the file at the target index
    while (!fileName.isEmpty()) {
        if (!isDir && fileName.endsWith(extension)) {
            // Extract just the filename from the full path to check for hidden files
            int lastSlash   = fileName.lastIndexOf('/');
            String baseName = (lastSlash >= 0) ? fileName.substring(lastSlash + 1) : fileName;

            if (!baseName.startsWith(".")) {
                if (currentIndex == index) {
                    // Found our target file!
                    result = fileName; // fileName already contains the full path
                    break;
                }
                currentIndex++;
            }
        }
        fileName = dir.getNextFileName(&isDir);
    }
    dir.close();

    if (result.isEmpty()) {
        log_w("Index %d out of range (total files: %d)", index, currentIndex);
    }

    return result;
}

uint32_t sd_card::count_files_in_directory(const char* dir_path, const char* extension) const {
    if (!initialized) {
        log_e("SD card not initialized.");
        return 0;
    }

    if (!is_directory(dir_path)) {
        log_e("Directory does not exist: %s", dir_path);
        return 0;
    }

    uint32_t count = 0;
    File dir       = SD_CARD_LIB.open(dir_path);
    if (!dir) {
        log_e("Failed to open directory: %s", dir_path);
        return 0;
    }

    bool isDir      = false;
    String fileName = dir.getNextFileName(&isDir);

    while (!fileName.isEmpty()) {
        if (!isDir && fileName.endsWith(extension)) {
            // Extract just the filename from the full path to check for hidden files
            int lastSlash   = fileName.lastIndexOf('/');
            String baseName = (lastSlash >= 0) ? fileName.substring(lastSlash + 1) : fileName;

            if (!baseName.startsWith(".")) {
                count++;
            }
        }
        fileName = dir.getNextFileName(&isDir);
    }
    dir.close();

    return count;
}

// ========== TOC (Table of Contents) Caching System Implementation ==========

bool sd_card::build_directory_toc(const char* dir_path,
                                  const char* extension,
                                  photo_frame_error_t* error) {
    if (!initialized) {
        log_e("SD card not initialized.");
        if (error)
            *error = error_type::NoSdCardAttached;
        return false;
    }

    if (!is_directory(dir_path)) {
        log_e("Directory does not exist: %s", dir_path);
        if (error)
            *error = error_type::SdCardFileNotFound;
        return false;
    }

    log_i("Building TOC for directory: %s (extension: %s)", dir_path, extension);

    // Get directory modification time BEFORE opening any files
    time_t dirModTime = get_last_modified(dir_path);
    log_v("Directory last modified: %lu", dirModTime);

    // Create timestamp for TOC
    time_t timestamp = time(nullptr);

    // Use temporary file names to avoid conflicts
    String tempDataPath = get_toc_data_path() + ".tmp";
    String tempMetaPath = get_toc_meta_path() + ".tmp";

    // First, open the temporary TOC data file for writing
    File tocDataFile = open(tempDataPath.c_str(), "w", true);
    if (!tocDataFile) {
        log_e("Failed to create temporary TOC data file");
        if (error)
            *error = error_type::TocWriteFailed;
        return false;
    }

    // Write header with placeholder for file count
    tocDataFile.printf("timestamp = %ld\n", timestamp);
    tocDataFile.printf("fileCount = 0000000000\n"); // Reserve 10 digits for count
    tocDataFile.flush();

    // Now open directory for scanning
    log_d("Opening directory for scanning: %s", dir_path);
    File dir = SD_CARD_LIB.open(dir_path, FILE_READ);
    if (!dir) {
        tocDataFile.close();
        remove(tempDataPath.c_str());
        log_e("Failed to open directory for TOC building: %s", dir_path);
        if (error)
            *error = error_type::TocBuildFailed;
        return false;
    }

    if (!dir.isDirectory()) {
        dir.close();
        tocDataFile.close();
        remove(tempDataPath.c_str());
        log_e("Path is not a directory: %s", dir_path);
        if (error)
            *error = error_type::TocBuildFailed;
        return false;
    }

    log_d("Directory opened successfully, starting file iteration");
    uint32_t fileCount = 0;
    bool isDir         = false;
    String fileName    = dir.getNextFileName(&isDir);

    while (!fileName.isEmpty()) {
        log_v("Processing entry: %s (isDir: %d)", fileName.c_str(), isDir);

        if (!isDir && fileName.endsWith(extension)) {
            // Extract just the filename from the full path to check for hidden files
            int lastSlash   = fileName.lastIndexOf('/');
            String baseName = (lastSlash >= 0) ? fileName.substring(lastSlash + 1) : fileName;

            if (!baseName.startsWith(".")) {
                // Write path directly to TOC file (no size check for speed)
                tocDataFile.println(fileName);
                fileCount++;
                log_v("Added file #%u: %s", fileCount, fileName.c_str());
            }
        }
        fileName = dir.getNextFileName(&isDir);
    }
    dir.close();
    log_d("Directory iteration complete, found %u files", fileCount);

    // Update file count in header
    tocDataFile.flush();
    tocDataFile.seek(0);
    tocDataFile.printf("timestamp = %ld\n", timestamp);
    tocDataFile.printf("fileCount = %10lu\n", (unsigned long)fileCount); // Use 10 digits
    tocDataFile.flush();
    tocDataFile.close();
    log_d("Temporary TOC data file written and closed");

    // Open and write temporary TOC meta file
    File tocMetaFile = open(tempMetaPath.c_str(), "w", true);
    if (!tocMetaFile) {
        // Clean up temp data file if meta creation fails
        remove(tempDataPath.c_str());
        log_e("Failed to create temporary TOC meta file");
        if (error)
            *error = error_type::TocWriteFailed;
        return false;
    }

    // Write metadata (now includes directory path and extension)
    tocMetaFile.printf("directoryModTime = %ld\n", dirModTime);
    tocMetaFile.printf("directoryPath = %s\n", dir_path);
    tocMetaFile.printf("extension = %s\n", extension);
    tocMetaFile.flush();
    tocMetaFile.close();
    log_d("Temporary TOC meta file written and closed");

    // Remove old TOC files if they exist (rename won't overwrite)
    if (SD_CARD_LIB.exists(get_toc_data_path().c_str())) {
        log_d("Removing existing TOC data file");
        SD_CARD_LIB.remove(get_toc_data_path().c_str());
    }
    if (SD_CARD_LIB.exists(get_toc_meta_path().c_str())) {
        log_d("Removing existing TOC meta file");
        SD_CARD_LIB.remove(get_toc_meta_path().c_str());
    }

    // Now rename temporary files to final names
    if (!SD_CARD_LIB.rename(tempDataPath.c_str(), get_toc_data_path().c_str())) {
        log_e("Failed to rename temporary TOC data file");
        remove(tempDataPath.c_str());
        remove(tempMetaPath.c_str());
        if (error)
            *error = error_type::TocWriteFailed;
        return false;
    }
    log_d("Renamed TOC data file successfully");

    if (!SD_CARD_LIB.rename(tempMetaPath.c_str(), get_toc_meta_path().c_str())) {
        log_e("Failed to rename temporary TOC meta file");
        remove(get_toc_data_path().c_str()); // Remove the renamed data file
        remove(tempMetaPath.c_str());
        if (error)
            *error = error_type::TocWriteFailed;
        return false;
    }
    log_d("Renamed TOC meta file successfully");

    // Update cached values
    cached_toc_directory_ = String(dir_path);
    cached_toc_extension_ = String(extension);
    cached_toc_timestamp_ = timestamp;
    toc_valid_            = true;

    log_i("TOC built successfully: %lu files indexed", (unsigned long)fileCount);
    if (error) {
        *error = error_type::None;
    }
    return true;
}

bool sd_card::is_toc_valid(const char* dir_path, const char* extension) const {
    if (!initialized) {
        return false;
    }

    // Check if cached values match
    if (toc_valid_ && cached_toc_directory_ == String(dir_path) &&
        cached_toc_extension_ == String(extension)) {

        // Check if TOC files exist
        sd_card_toc_parser parser(
            const_cast<sd_card&>(*this), get_toc_data_path().c_str(), get_toc_meta_path().c_str());

        if (!parser.toc_exists()) {
            log_v("TOC files don't exist");
            return false;
        }

        // Check directory modification time and validate path/extension
        time_t currentModTime = get_last_modified(dir_path);
        if (!parser.validate_toc(currentModTime, dir_path, extension)) {
            log_v("TOC validation failed");
            return false;
        }

        log_v("TOC is valid for %s", dir_path);
        return true;
    }

    log_v("TOC cache miss or invalid");
    return false;
}

void sd_card::invalidate_toc() {
    toc_valid_            = false;
    cached_toc_directory_ = "";
    cached_toc_extension_ = "";
    cached_toc_timestamp_ = 0;
    log_v("TOC invalidated");
}

bool sd_card::should_use_toc(const char* dir_path, const char* extension) const {
    // Check if TOC is valid, if not try to build it
    if (!is_toc_valid(dir_path, extension)) {
        log_v("TOC not valid, attempting to build...");
        photo_frame_error_t buildError;
        if (!const_cast<sd_card*>(this)->build_directory_toc(dir_path, extension, &buildError)) {
            log_w("Failed to build TOC: %s (code: %u), falling back to direct iteration",
                  buildError.message,
                  buildError.code);
            return false;
        }
    }
    return true;
}

uint32_t
sd_card::count_files_cached(const char* dir_path, const char* extension, bool use_toc) const {
    if (use_toc && should_use_toc(dir_path, extension)) {
        sd_card_toc_parser parser(
            const_cast<sd_card&>(*this), get_toc_data_path().c_str(), get_toc_meta_path().c_str());

        photo_frame_error_t error; // Default constructor sets code to 0 (no error)
        size_t count = parser.get_file_count(&error);

        if (error.code == 0) { // No error
            log_v("Using TOC: found %u files", count);
            return count;
        } else {
            log_w("TOC read failed: %s (code: %u), falling back to direct iteration",
                  error.message,
                  error.code);
        }
    }

    // Fall back to original implementation
    return count_files_in_directory(dir_path, extension);
}

String sd_card::get_file_at_index_cached(const char* dir_path,
                                         uint32_t index,
                                         const char* extension,
                                         bool use_toc) const {
    if (use_toc && should_use_toc(dir_path, extension)) {
        sd_card_toc_parser parser(
            const_cast<sd_card&>(*this), get_toc_data_path().c_str(), get_toc_meta_path().c_str());

        photo_frame_error_t error; // Default constructor sets code to 0 (no error)
        String filePath = parser.get_file_by_index(index, &error);

        if (error.code == 0 && !filePath.isEmpty()) { // No error
            log_v("Using TOC: got file at index %u: %s", index, filePath.c_str());
            return filePath;
        } else {
            log_w("TOC read failed: %s (code: %u), falling back to direct iteration",
                  error.message,
                  error.code);
        }
    }

    // Fall back to original implementation
    return get_file_at_index(dir_path, index, extension);
}

} // namespace photo_frame