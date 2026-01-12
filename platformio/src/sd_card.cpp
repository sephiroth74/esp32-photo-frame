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
                                   SdCardEntry* file_entry,
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

photo_frame_error_t SdCard::begin() {
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

void SdCard::end() {
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

void SdCard::printStats() const {
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

time_t SdCard::getLastModified(const char* path) const {
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

time_t SdCard::getFileAge(const char* path) const {
    time_t lastModified = this->getLastModified(path);
    if (lastModified == 0) {
        return 0; // Return 0 if file cannot be accessed
    }
    return time(NULL) - lastModified;
}

void SdCard::listFiles() const {
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

bool SdCard::fileExists(const char* path) const {
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

uint32_t SdCard::countFiles() const {
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

fs::File SdCard::open(const char* path, const char* mode, bool create) {
    log_d("SdCard::open | path: %s, mode: %s", path, mode);

    if (!initialized) {
        log_e("SD card not initialized.");
        return fs::File();
    }

    fs::File file = SD_CARD_LIB.open(path, mode, create);
    return file;
} // end open

bool SdCard::rename(const char* pathFrom, const char* pathTo, bool overwrite) {
    log_d("SdCard::rename | pathFrom: %s, pathTo: %s", pathFrom, pathTo);

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

bool SdCard::cleanupDir(const char* path) {
    log_d("SdCard::cleanupDir | path: %s", path);

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
            if (!this->cleanupDir(entry.c_str())) {
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

bool SdCard::rmdir(const char* path) {
    log_d("SdCard::rmdir | path: %s", path);

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
    if (!this->cleanupDir(path)) {
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

bool SdCard::remove(const char* path) {
    log_d("SdCard::remove | path: %s", path);

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

size_t SdCard::getFileSize(const char* path) const {
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

bool SdCard::isDirectory(const char* path) const {
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

bool SdCard::isFile(const char* path) const {
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

bool SdCard::createDirectories(const char* path) {
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

uint64_t SdCard::totalBytes() const {
    if (!initialized) {
        log_e("SD card not initialized.");
        return 0;
    }

    return SD_CARD_LIB.totalBytes();
}

uint64_t SdCard::usedBytes() const {
    if (!initialized) {
        log_e("SD card not initialized.");
        return 0;
    }

    return SD_CARD_LIB.usedBytes();
}

uint64_t SdCard::cardSize() const {
    if (!initialized) {
        log_e("SD card not initialized.");
        return 0;
    }

    return SD_CARD_LIB.cardSize();
}

std::vector<String> SdCard::listFilesInDirectory(const char* dir_path,
                                                 const char* extension) const {
    std::vector<String> files;

    if (!initialized) {
        log_e("SD card not initialized.");
        return files;
    }

    if (!this->isDirectory(dir_path)) {
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

String SdCard::getFileAtIndex(const char* dir_path, uint32_t index, const char* extension) const {
    if (!initialized) {
        log_e("SD card not initialized.");
        return String();
    }

    if (!this->isDirectory(dir_path)) {
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

uint32_t SdCard::countFilesInDirectory(const char* dir_path, const char* extension) const {
    if (!initialized) {
        log_e("SD card not initialized.");
        return 0;
    }

    if (!this->isDirectory(dir_path)) {
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

bool SdCard::buildDirectoryToc(const char* dir_path,
                               const char* extension,
                               photo_frame_error_t* error) {
    if (!initialized) {
        log_e("SD card not initialized.");
        if (error)
            *error = error_type::NoSdCardAttached;
        return false;
    }

    if (!this->isDirectory(dir_path)) {
        log_e("Directory does not exist: %s", dir_path);
        if (error)
            *error = error_type::SdCardFileNotFound;
        return false;
    }

    log_i("Building TOC for directory: %s (extension: %s)", dir_path, extension);

    // Create timestamp for TOC
    time_t timestamp = time(nullptr);

    // Use temporary file names to avoid conflicts
    String tempDataPath = this->getTocDataPath() + ".tmp";
    String tempMetaPath = this->getTocMetaPath() + ".tmp";

    // First, open the temporary TOC data file for writing
    File tocDataFile = open(tempDataPath.c_str(), "w", true);
    if (!tocDataFile) {
        log_e("Failed to create temporary TOC data file");
        if (error)
            *error = error_type::TocWriteFailed;
        return false;
    }

    // No header in data file - it contains only the file list

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

    int errorCount = 0;
    const int MAX_ERRORS = 3;

    while (!fileName.isEmpty()) {
        if (!isDir && fileName.endsWith(extension)) {
            // Extract just the filename from the full path to check for hidden files
            int lastSlash   = fileName.lastIndexOf('/');
            String baseName = (lastSlash >= 0) ? fileName.substring(lastSlash + 1) : fileName;

            if (!baseName.startsWith(".")) {
                // Write path directly to TOC file (no size check for speed)
                tocDataFile.println(fileName);
                fileCount++;

                // Log progress and add delay every 10 files
                if (fileCount % 10 == 0) {
                    log_i("Building TOC: processed %u files...", fileCount);
                    tocDataFile.flush();  // Flush buffer to SD
                    delay(10);  // Give SD card time to recover
                    yield();    // Allow other tasks to run
                }
            }
        }

        // Get next file with error recovery
        String nextFile = dir.getNextFileName(&isDir);
        if (nextFile.isEmpty() && fileName != nextFile) {
            // Possible SD card error, try to recover
            errorCount++;
            log_w("SD card read error during iteration, attempt %d/%d", errorCount, MAX_ERRORS);

            if (errorCount >= MAX_ERRORS) {
                log_e("SD card failed after %d attempts, aborting iteration", MAX_ERRORS);
                break;
            }

            delay(100);  // Wait before retry
            continue;    // Try again
        }

        errorCount = 0;  // Reset error count on successful read
        fileName = nextFile;
    }
    dir.close();
    log_d("Directory iteration complete, found %u files", fileCount);

    // Close data file (contains only file list now)
    tocDataFile.flush();
    tocDataFile.close();
    log_d("Temporary TOC data file written and closed");

    // Get the size of the TOC data file for validation
    size_t tocFileSize = 0;
    File sizeCheckFile = open(tempDataPath.c_str(), "r");
    if (sizeCheckFile) {
        tocFileSize = sizeCheckFile.size();
        sizeCheckFile.close();
        log_d("TOC data file size: %u bytes", tocFileSize);
    } else {
        log_w("Could not get TOC data file size");
    }

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

    // Get directory modification time
    time_t dirModTime = this->getLastModified(dir_path);

    // Write metadata (all validation info in meta file)
    tocMetaFile.printf("directoryPath = %s\n", dir_path);
    tocMetaFile.printf("directoryModTime = %ld\n", dirModTime);
    tocMetaFile.printf("fileCount = %u\n", fileCount);
    tocMetaFile.printf("extension = %s\n", extension);
    tocMetaFile.printf("fileSize = %u\n", tocFileSize);
    tocMetaFile.flush();
    tocMetaFile.close();
    log_d("Temporary TOC meta file written and closed");

    // Remove old TOC files if they exist (rename won't overwrite)
    if (SD_CARD_LIB.exists(this->getTocDataPath().c_str())) {
        log_d("Removing existing TOC data file");
        SD_CARD_LIB.remove(this->getTocDataPath().c_str());
    }
    if (SD_CARD_LIB.exists(this->getTocMetaPath().c_str())) {
        log_d("Removing existing TOC meta file");
        SD_CARD_LIB.remove(this->getTocMetaPath().c_str());
    }

    // Now rename temporary files to final names
    if (!SD_CARD_LIB.rename(tempDataPath.c_str(), this->getTocDataPath().c_str())) {
        log_e("Failed to rename temporary TOC data file");
        remove(tempDataPath.c_str());
        remove(tempMetaPath.c_str());
        if (error)
            *error = error_type::TocWriteFailed;
        return false;
    }
    log_d("Renamed TOC data file successfully");

    if (!SD_CARD_LIB.rename(tempMetaPath.c_str(), this->getTocMetaPath().c_str())) {
        log_e("Failed to rename temporary TOC meta file");
        remove(this->getTocDataPath().c_str()); // Remove the renamed data file
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

bool SdCard::isTocValid(const char* dir_path, const char* extension) const {
    if (!initialized) {
        return false;
    }

    // First, check if TOC files exist on disk
    SdCardTocParser parser(
        const_cast<SdCard&>(*this), this->getTocDataPath().c_str(), this->getTocMetaPath().c_str());

    if (!parser.toc_exists()) {
        log_i("TOC rebuild reason: TOC files don't exist on SD card");
        return false;
    }

    // Validate that the TOC matches the requested path and extension
    if (!parser.validate_toc(dir_path, extension)) {
        log_i("TOC rebuild reason: TOC validation failed (path or extension mismatch)");
        return false;
    }

    // If we reach here, the TOC files exist and are valid for the requested directory
    // Update the cached values so subsequent calls in the same session are faster
    const_cast<SdCard*>(this)->toc_valid_            = true;
    const_cast<SdCard*>(this)->cached_toc_directory_ = String(dir_path);
    const_cast<SdCard*>(this)->cached_toc_extension_ = String(extension);

    log_v("TOC is valid for %s", dir_path);
    return true;
}

void SdCard::invalidateToc() {
    toc_valid_            = false;
    cached_toc_directory_ = "";
    cached_toc_extension_ = "";
    cached_toc_timestamp_ = 0;
    log_v("TOC invalidated");
}

bool SdCard::shouldUseToc(const char* dir_path, const char* extension) const {
    // Check if TOC is valid
    // NOTE: We don't attempt to build TOC here anymore to avoid double builds.
    // TOC building should be done explicitly by the caller when needed.
    if (!this->isTocValid(dir_path, extension)) {
        log_v("TOC not valid for %s (extension: %s)", dir_path, extension);
        return false;
    }
    return true;
}

uint32_t SdCard::countFilesCached(const char* dir_path, const char* extension, bool use_toc) const {
    if (use_toc && this->shouldUseToc(dir_path, extension)) {
        SdCardTocParser parser(const_cast<SdCard&>(*this),
                               this->getTocDataPath().c_str(),
                               this->getTocMetaPath().c_str());

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
    return this->countFilesInDirectory(dir_path, extension);
}

String SdCard::getFileAtIndexCached(const char* dir_path,
                                    uint32_t index,
                                    const char* extension,
                                    bool use_toc) const {
    if (use_toc && this->shouldUseToc(dir_path, extension)) {
        SdCardTocParser parser(const_cast<SdCard&>(*this),
                               this->getTocDataPath().c_str(),
                               this->getTocMetaPath().c_str());

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
    return this->getFileAtIndex(dir_path, index, extension);
}

} // namespace photo_frame