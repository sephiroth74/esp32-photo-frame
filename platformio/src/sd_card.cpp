// ESP32 Photo Frame
// Copyright (C) 2025 Alessandro Crugnola
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

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
        delay(1);
#elif defined(SD_USE_SPI)
        log_i("Ending SPI bus");
        SPI.end();
        delay(1);
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

String SdCard::getTocDirectoryPath(const char* dir_path) const {
    String base = SD_TOC_BASE_PATH;
    String dir  = String(dir_path);
    if (!dir.startsWith("/")) {
        dir = "/" + dir;
    }
    return base + dir;
}

String SdCard::getTocDataPath(const char* dir_path) const {
    return this->getTocDirectoryPath(dir_path) + "/" + TOC_DATA_FILENAME;
}

String SdCard::getTocMetaPath(const char* dir_path) const {
    return this->getTocDirectoryPath(dir_path) + "/" + TOC_META_FILENAME;
}

void SdCard::collectTocDirectoriesWithFiles(const char* base_path,
                                            std::vector<String>& out_paths) const {
    File dir = SD_CARD_LIB.open(base_path, FILE_READ);
    if (!dir || !dir.isDirectory()) {
        return;
    }

    bool isDir      = false;
    String fileName = dir.getNextFileName(&isDir);

    while (!fileName.isEmpty()) {
        if (isDir) {
            String dataPath = fileName + "/toc_data.txt";
            String metaPath = fileName + "/toc_meta.txt";
            if (SD_CARD_LIB.exists(dataPath.c_str()) && SD_CARD_LIB.exists(metaPath.c_str())) {
                out_paths.push_back(fileName);
            }
            collectTocDirectoriesWithFiles(fileName.c_str(), out_paths);
        }
        fileName = dir.getNextFileName(&isDir);
    }
    dir.close();
}

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

    // Ensure TOC directory exists
    String tocDir = this->getTocDirectoryPath(dir_path);
    if (!this->createDirectories(tocDir.c_str())) {
        log_e("Failed to create TOC directory: %s", tocDir.c_str());
        if (error)
            *error = error_type::TocBuildFailed;
        return false;
    }

    // Use temporary file names to avoid conflicts
    String tempDataPath = this->getTocDataPath(dir_path) + ".tmp";
    String tempMetaPath = this->getTocMetaPath(dir_path) + ".tmp";

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
    uint32_t fileCount   = 0;
    bool isDir           = false;
    String fileName      = dir.getNextFileName(&isDir);

    int errorCount       = 0;
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
                    tocDataFile.flush(); // Flush buffer to SD
                    delay(20);           // Increased delay for better stability with many files
                    yield();             // Allow other tasks to run

                    // Extra delay every 100 files for large directories
                    if (fileCount % 100 == 0) {
                        log_i("Large directory: extra delay at %u files", fileCount);
                        delay(100);
                    }
                }
            }
        }

        // Get next file with error recovery
        String nextFile = dir.getNextFileName(&isDir);

        // Check if we've reached the end of directory normally
        if (nextFile.isEmpty()) {
            // If we have processed files and get empty, it's likely end of directory
            if (fileCount > 0) {
                log_i("Reached end of directory after %u files", fileCount);
                break; // Normal end of directory
            }

            // If no files processed and empty, might be an error
            errorCount++;
            log_w(
                "SD card may have failed during iteration, attempt %d/%d", errorCount, MAX_ERRORS);

            if (errorCount >= MAX_ERRORS) {
                log_e("SD card failed after %d attempts, aborting iteration", MAX_ERRORS);
                break;
            }

            delay(100); // Wait before retry

            // Try to re-read current position
            nextFile = dir.getNextFileName(&isDir);
            if (nextFile.isEmpty()) {
                continue; // Still empty, try again
            }
        }

        errorCount = 0; // Reset error count on successful read
        fileName   = nextFile;
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
    if (SD_CARD_LIB.exists(this->getTocDataPath(dir_path).c_str())) {
        log_d("Removing existing TOC data file");
        SD_CARD_LIB.remove(this->getTocDataPath(dir_path).c_str());
    }
    if (SD_CARD_LIB.exists(this->getTocMetaPath(dir_path).c_str())) {
        log_d("Removing existing TOC meta file");
        SD_CARD_LIB.remove(this->getTocMetaPath(dir_path).c_str());
    }

    // Now rename temporary files to final names
    if (!SD_CARD_LIB.rename(tempDataPath.c_str(), this->getTocDataPath(dir_path).c_str())) {
        log_e("Failed to rename temporary TOC data file");
        remove(tempDataPath.c_str());
        remove(tempMetaPath.c_str());
        if (error)
            *error = error_type::TocWriteFailed;
        return false;
    }
    log_d("Renamed TOC data file successfully");

    if (!SD_CARD_LIB.rename(tempMetaPath.c_str(), this->getTocMetaPath(dir_path).c_str())) {
        log_e("Failed to rename temporary TOC meta file");
        remove(this->getTocDataPath(dir_path).c_str()); // Remove the renamed data file
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
    SdCardTocParser parser(const_cast<SdCard&>(*this),
                           this->getTocDataPath(dir_path).c_str(),
                           this->getTocMetaPath(dir_path).c_str());

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
                               this->getTocDataPath(dir_path).c_str(),
                               this->getTocMetaPath(dir_path).c_str());

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
                               this->getTocDataPath(dir_path).c_str(),
                               this->getTocMetaPath(dir_path).c_str());

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

bool SdCard::buildMultiDirectoryToc(const std::vector<String>& directories,
                                    const char* extension,
                                    photo_frame_error_t* error) {
    if (!initialized) {
        if (error)
            *error = error_type::NoSdCardAttached;
        return false;
    }

    if (directories.empty()) {
        if (error)
            *error = error_type::NoImagesFound;
        return false;
    }

    if (!this->createDirectories(SD_TOC_BASE_PATH)) {
        log_e("Failed to create TOC base directory: %s", SD_TOC_BASE_PATH);
        if (error)
            *error = error_type::TocBuildFailed;
        return false;
    }

    for (const auto& dir : directories) {
        photo_frame_error_t tocError;
        if (!this->buildDirectoryToc(dir.c_str(), extension, &tocError)) {
            log_e("Failed to build TOC for directory: %s", dir.c_str());
            if (error)
                *error = tocError;
            return false;
        }
    }

    if (error)
        *error = error_type::None;

    return true;
}

bool SdCard::isMultiDirectoryTocValid(const std::vector<String>& directories,
                                      const char* extension) const {
    if (!initialized) {
        return false;
    }

    if (directories.empty()) {
        return false;
    }

    if (!this->isDirectory(SD_TOC_BASE_PATH)) {
        log_i("TOC base directory missing: %s", SD_TOC_BASE_PATH);
        return false;
    }

    std::vector<String> expectedPaths;
    expectedPaths.reserve(directories.size());
    for (const auto& dir : directories) {
        expectedPaths.push_back(this->getTocDirectoryPath(dir.c_str()));
    }

    std::vector<String> cachedPaths;
    collectTocDirectoriesWithFiles(SD_TOC_BASE_PATH, cachedPaths);

    if (cachedPaths.size() != expectedPaths.size()) {
        log_i("TOC cache mismatch: expected %u directories, found %u",
              (unsigned int)expectedPaths.size(),
              (unsigned int)cachedPaths.size());
        return false;
    }

    for (const auto& expected : expectedPaths) {
        bool found = false;
        for (const auto& cached : cachedPaths) {
            if (cached == expected) {
                found = true;
                break;
            }
        }

        if (!found) {
            log_i("TOC cache mismatch: missing directory %s", expected.c_str());
            return false;
        }
    }

    for (const auto& dir : directories) {
        if (!this->isTocValid(dir.c_str(), extension)) {
            log_i("TOC invalid for directory: %s", dir.c_str());
            return false;
        }
    }

    return true;
}

bool SdCard::selectRandomImageFromDirectories(const std::vector<String>& directories,
                                              String& out_directory,
                                              String& out_file_path,
                                              uint32_t& out_total_files,
                                              uint32_t& out_selected_index,
                                              const char* extension) const {
    out_directory      = "";
    out_file_path      = "";
    out_total_files    = 0;
    out_selected_index = 0;

    if (!initialized || directories.empty()) {
        return false;
    }

    std::vector<String> shuffled = directories;
    for (int i = (int)shuffled.size() - 1; i > 0; --i) {
        int j = random(0, i + 1);
        std::swap(shuffled[i], shuffled[j]);
    }

    for (const auto& dir : shuffled) {
        uint32_t fileCount = this->countFilesCached(dir.c_str(), extension, true);
        if (fileCount == 0) {
            continue;
        }

        out_total_files    = fileCount;
        out_selected_index = random(0, fileCount);
        String file_path =
            this->getFileAtIndexCached(dir.c_str(), out_selected_index, extension, true);
        if (!file_path.isEmpty()) {
            out_directory = dir;
            out_file_path = file_path;
            return true;
        }
    }

    return false;
}

} // namespace photo_frame