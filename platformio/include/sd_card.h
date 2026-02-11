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

#ifndef __PHOTO_FRAME_SD_CARD_H__
#define __PHOTO_FRAME_SD_CARD_H__

#include "config.h"
#include "errors.h"
#include <Arduino.h>
#include <vector>

// Support both SDIO (SD_MMC) and SPI (SD) interfaces
#ifdef SD_USE_SPI
#include <SD.h>
#include <SPI.h>
#define SD_CARD_LIB SD // Use SPI SD library
#else
#include <SD_MMC.h>
#define SD_CARD_LIB SD_MMC // Use SDIO SD_MMC library
#endif

#define SD_TOC_BASE_PATH SD_CARD_CACHE_DIR "/sdcard/toc"

namespace photo_frame {

/**
 * @brief Represents an entry (file) on the SD card.
 *
 * This class encapsulates information about a file stored on the SD card,
 * including its name, full path, and index within a collection. It provides
 * utility methods for string representation and validity checking.
 */
class SdCardEntry {
public:
  String name;    ///< Name of the file (without path)
  String path;    ///< Full path to the file on the SD card
  uint32_t index; ///< Index of this entry within a collection

  /**
   * @brief Default constructor creating an empty entry.
   */
  SdCardEntry() : name(""), path(""), index(0) {}

  /**
   * @brief Constructor with file information.
   * @param name Name of the file (without path)
   * @param path Full path to the file on the SD card
   * @param index Index of this entry within a collection
   */
  SdCardEntry(const char *name, const char *path, uint32_t index) : name(name), path(path), index(index) {}

  /**
   * @brief Converts the entry to a string representation.
   * @return String in format "name | path | index"
   */
  String toString() const { return name + " | " + path + " | " + String(index); }

  /**
   * @brief Checks if the entry is valid (has non-empty name and path).
   * @return True if entry is valid, false otherwise
   */
  operator bool() const { return (!name.isEmpty() && !path.isEmpty()); }
};

/**
 * @brief SD card interface class for file operations using SD_MMC (SDIO).
 *
 * This class provides a comprehensive interface for interacting with SD cards,
 * including initialization, file operations, directory management, and metadata
 * operations. Uses the SD_MMC library with SDIO interface for better
 * performance than SPI mode. Uses custom ESP32 pins: CLK(14), CMD(15), D0(7),
 * D1(4), D2(12), D3(13). Note: D0 moved from GPIO2 to GPIO7 to avoid NeoPixel
 * LED conflict on Feather V2.
 */
class SdCard {
private:
  bool initialized;       ///< Flag indicating if SD card is initialized
  sdcard_type_t cardType; ///< Type of the SD card (MMC, SD, SDHC, etc.)

  // TOC caching system
  mutable String cached_toc_directory_; ///< Last directory used for TOC
  mutable String cached_toc_extension_; ///< Last extension used for TOC
  mutable time_t cached_toc_timestamp_; ///< When TOC was created
  mutable bool toc_valid_;              ///< Whether TOC is currently valid

public:
  /**
   * @brief Constructor for SdCard using SD_MMC (SDIO interface).
   * SD_MMC uses fixed pins that cannot be configured:
   * - CLK: GPIO14, CMD: GPIO15, D0: GPIO2, D1: GPIO4, D2: GPIO12, D3: GPIO13
   */
  SdCard() : initialized(false), cardType(CARD_UNKNOWN), toc_valid_(false) {}

  // Disable copy constructor and assignment operator
  SdCard(const SdCard &) = delete;
  SdCard &operator=(const SdCard &) = delete;

  /**
   * Initializes the SD card.
   * @return photo_frame_error_t indicating success or failure.
   */
  photo_frame_error_t begin();

  /**
   * Ends the SD card session, releasing resources.
   * This should be called when the SD card is no longer needed.
   * It is important to call this to ensure that all data is written and the
   * card is properly unmounted. After calling this, the SD card cannot be used
   * until `begin()` is called again.
   * @note This function should be called before the program ends or when the SD
   * card is no longer needed.
   * @note It is a good practice to call this function to ensure that the SD
   * card is properly unmounted.
   */
  void end();

  /**
   * Checks if the SD card is initialized.
   */
  bool isInitialized() const { return initialized; }

  /**
   * Returns the type of the SD card.
   * @return sdcard_type_t representing the type of the SD card.
   * If the SD card is not initialized, it returns CARD_NONE.
   */
  sdcard_type_t getCardType() const {
    if (!initialized) {
      return CARD_NONE;
    }
    return cardType;
  }

  /**
   * @brief Prints the SD card type to Serial output.
   *
   * Outputs a human-readable description of the detected SD card type
   * (MMC, SDSC, SDHC, etc.) for debugging and informational purposes.
   */
  void printCardType() const {
    const char *card_type_str;
    switch (cardType) {
    case CARD_MMC:
      card_type_str = "MMC";
      break;
    case CARD_SD:
      card_type_str = "SDSC";
      break;
    case CARD_SDHC:
      card_type_str = "SDHC";
      break;
    case CARD_UNKNOWN:
      card_type_str = "Unknown";
      break;
    case CARD_NONE:
      card_type_str = "No SD card attached!";
      break;
    default:
      card_type_str = "Unknown card type!";
      break;
    }
    log_i("Card Type: %s", card_type_str);
  }

  /**
   * Gets the last modified time of a file on the SD card.
   * @param path The path to the file.
   * @return The last modified time as a time_t value.
   */
  time_t getLastModified(const char *path) const;

  /**
   * Get the age of a file on the SD card.
   * @param path The path to the file.
   * @return The age of the file in seconds.
   */
  time_t getFileAge(const char *path) const;

  /**
   * Checks if a file exists on the SD card.
   * @param path The path to the file to check.
   * @return true if the file exists, false otherwise.
   */
  bool fileExists(const char *path) const;

  /**
   * Checks if a directory exists on the SD card.
   * @param path The path to the directory to check.
   * @return true if the directory exists, false otherwise.
   */
  bool isDirectory(const char *path) const;

  /**
   * Checks if a file exists on the SD card.
   * @param path The path to the file to check.
   * @return true if the file exists, false otherwise.
   */
  bool isFile(const char *path) const;

  /**
   * Lists all files in the root directory of the SD card with allowed
   * extensions (.pfr1, .bmp).
   * @note This function only lists files in the root directory, not in
   * subdirectories.
   * @note If the SD card is not initialized, it will not list any files.
   */
  void listFiles() const;

  /**
   * Prints statistics about the SD card, including total space, used space, and
   * free space.
   * @note This function provides a summary of the SD card's storage status.
   * It is useful for monitoring the SD card's capacity and available space.
   * @note If the SD card is not initialized, it will not print any statistics.
   * @note This function is useful for debugging and monitoring the SD card's
   * status. It can help identify issues related to storage capacity or file
   * management.
   */
  void printStats() const;

  /**
   * Counts the number of files with allowed extensions (.pfr1, .bmp) in the
   * root directory of the SD card.
   * @return The number of files with allowed extensions.
   * If the SD card is not initialized, it returns 0.
   * @note This function only counts files in the root directory, not in
   * subdirectories.
   */
  uint32_t countFiles() const;

  /**
   * Opens a file on the SD card.
   * @param path The path to the file to open.
   * @param mode The mode in which to open the file (e.g., FILE_READ,
   * FILE_WRITE).
   * @param create If true, creates the file if it does not exist (only
   * applicable for write modes).
   * @return A File object representing the opened file.
   * If the SD card is not initialized or the file cannot be opened, it returns
   * an empty File object.
   */
  fs::File open(const char *path, const char *mode = FILE_READ, bool create = false);

  /**
   * Renames a file on the SD card.
   * @param pathFrom The current path of the file to rename.
   * @param pathTo The new path for the renamed file.
   * @param overwrite If true, allows overwriting an existing file.
   * @return true if the file was renamed successfully, false otherwise.
   * @note If the SD card is not initialized, it will return false.
   */
  bool rename(const char *pathFrom, const char *pathTo, bool overwrite = false);

  /**
   * Removes (deletes) a file from the SD card.
   * @param path The path to the file to remove.
   * @return true if the file was removed successfully, false otherwise.
   * @note If the SD card is not initialized or the file doesn't exist, it will
   * return false.
   */
  bool remove(const char *path);

  /**
   * Recursively removes all files and subdirectories from a directory.
   * @param path The path to the directory to clean up.
   * @return true if all contents were removed successfully, false otherwise.
   * @note If the SD card is not initialized or the directory doesn't exist, it
   * will return false.
   */
  bool cleanupDir(const char *path);

  /**
   * Removes a directory from the SD card.
   * @param path The path to the directory to remove.
   * @return true if the directory was removed successfully, false otherwise.
   * @note If the SD card is not initialized or the directory doesn't exist, it
   * will return false.
   */
  bool rmdir(const char *path);

  /**
   * Gets the size of a file on the SD card.
   * @param path The path to the file.
   * @return The size of the file in bytes, or 0 if the file doesn't exist or SD
   * card is not initialized.
   */
  size_t getFileSize(const char *path) const;

  /**
   * Creates directories recursively if they don't exist.
   * @param path The directory path to create (can be nested like
   * "/dir1/dir2/dir3").
   * @return true if directories were created successfully or already exist,
   * false otherwise.
   * @note If the SD card is not initialized, it will return false.
   */
  bool createDirectories(const char *path);

  /**
   * Lists all files in a directory with the specified extension.
   * @param dir_path The directory path to list files from.
   * @param extension The file extension to filter (e.g., ".pfr1"). Default is
   * ".pfr1".
   * @return Vector of file paths matching the extension.
   * @note If the SD card is not initialized or directory doesn't exist, returns
   * empty vector.
   */
  std::vector<String> listFilesInDirectory(const char *dir_path, const char *extension = BINARY_FILE_EXTENSION) const;

  /**
   * Gets a file at a specific index from a directory.
   * @param dir_path The directory path to get file from.
   * @param index The index of the file to get (0-based).
   * @param extension The file extension to filter (e.g., ".pfr1"). Default is
   * ".pfr1".
   * @return Full path to the file, or empty string if not found.
   * @note Files are sorted alphabetically before indexing.
   */
  String getFileAtIndex(const char *dir_path, uint32_t index, const char *extension = BINARY_FILE_EXTENSION) const;

  /**
   * Counts the number of files with specified extension in a directory.
   * @param dir_path The directory path to count files in.
   * @param extension The file extension to filter (e.g., ".pfr1"). Default is
   * ".pfr1".
   * @return Number of files with the specified extension.
   * @note If the SD card is not initialized or directory doesn't exist, returns
   * 0.
   */
  uint32_t countFilesInDirectory(const char *dir_path, const char *extension = BINARY_FILE_EXTENSION) const;

  /**
   * Gets the amount of used space in bytes on the SD card.
   * @return The amount of used space in bytes, or 0 if the SD card is not
   * initialized.
   */
  uint64_t usedBytes() const;

  /**
   * Gets the total bytes on the SD card.
   * @return The total space in bytes, or 0 if the SD card is not initialized.
   */
  uint64_t totalBytes() const;

  /**
   * Gets the card size in bytes.
   * @return The card size in bytes, or 0 if the SD card is not initialized.
   */
  uint64_t cardSize() const;

  // ========== TOC (Table of Contents) Caching System ==========

  /**
   * Builds a TOC (Table of Contents) file for a directory.
   * This creates an index of all files matching the extension, allowing
   * for fast random access without directory iteration.
   * @param dir_path The directory path to build TOC for.
   * @param extension The file extension to include (e.g., ".pfr1"). Default is
   * ".pfr1".
   * @param error Optional pointer to store error details if build fails.
   * @return true if TOC was built successfully, false otherwise.
   */
  bool buildDirectoryToc(const char *dir_path, const char *extension = BINARY_FILE_EXTENSION, photo_frame_error_t *error = nullptr);

  /**
   * Checks if the TOC is valid for the given directory.
   * Validates that TOC exists, matches the directory, and is up-to-date.
   * @param dir_path The directory path to check.
   * @param extension The file extension filter.
   * @return true if TOC is valid and can be used, false if it needs rebuilding.
   */
  bool isTocValid(const char *dir_path, const char *extension = BINARY_FILE_EXTENSION) const;

  /**
   * Invalidates the cached TOC, forcing a rebuild on next access.
   * Call this when directory contents may have changed.
   */
  void invalidateToc();

  /**
   * Gets the number of files using the cached TOC if available.
   * If TOC is not valid, falls back to directory iteration.
   * @param dir_path The directory path.
   * @param extension The file extension filter.
   * @param use_toc If true, attempts to use TOC cache. Default is true.
   * @return Number of files matching the extension.
   */
  uint32_t countFilesCached(const char *dir_path, const char *extension = BINARY_FILE_EXTENSION, bool use_toc = true) const;

  /**
   * Gets a file at index using the cached TOC if available.
   * If TOC is not valid, falls back to directory iteration.
   * @param dir_path The directory path.
   * @param index The file index (0-based).
   * @param extension The file extension filter.
   * @param use_toc If true, attempts to use TOC cache. Default is true.
   * @return Full path to the file, or empty string if not found.
   */
  String getFileAtIndexCached(const char *dir_path, uint32_t index, const char *extension = BINARY_FILE_EXTENSION,
                              bool use_toc = true) const;

  /**
   * Builds TOC files for all configured directories.
   * @param directories List of directories to build TOC for.
   * @param extension File extension filter.
   * @param error Optional pointer to store error details.
   * @return true if all TOCs were built successfully.
   */
  bool buildMultiDirectoryToc(const std::vector<String> &directories, const char *extension = BINARY_FILE_EXTENSION,
                              photo_frame_error_t *error = nullptr);

  /**
   * Validates TOC cache consistency for all configured directories.
   * @param directories List of directories configured.
   * @param extension File extension filter.
   * @return true if all cached TOCs are valid and match the config list.
   */
  bool isMultiDirectoryTocValid(const std::vector<String> &directories, const char *extension = BINARY_FILE_EXTENSION) const;

  /**
   * Select a random image from the configured directories.
   * The directory list is shuffled before selection. If a directory has
   * fileCount == 0, it moves to the next element.
   * @param directories Configured directories list.
   * @param out_directory Selected directory.
   * @param out_file_path Selected file path.
   * @param out_total_files Total files counted in the selected directory.
   * @param out_selected_index Selected index within the directory.
   * @param extension File extension filter.
   * @return true if an image was selected.
   */
  bool selectRandomImageFromDirectories(const std::vector<String> &directories, String &out_directory, String &out_file_path,
                                        uint32_t &out_total_files, uint32_t &out_selected_index,
                                        const char *extension = BINARY_FILE_EXTENSION) const;

private:
  // Helper methods for TOC operations
  String getTocDirectoryPath(const char *dir_path) const;
  String getTocDataPath(const char *dir_path) const;
  String getTocMetaPath(const char *dir_path) const;
  bool shouldUseToc(const char *dir_path, const char *extension) const;
  void collectTocDirectoriesWithFiles(const char *base_path, std::vector<String> &out_paths) const;
};

} // namespace photo_frame

#endif // __PHOTO_FRAME_SD_CARD_H__