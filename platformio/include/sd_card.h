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

#ifndef __PHOTO_FRAME_SD_CARD_H__
#define __PHOTO_FRAME_SD_CARD_H__

#include "FS.h"
#include "config.h"
#include "errors.h"
#include <Arduino.h>
#include <SD.h>
#include <SPI.h>

namespace photo_frame {

/**
 * @brief Represents an entry (file) on the SD card.
 *
 * This class encapsulates information about a file stored on the SD card,
 * including its name, full path, and index within a collection. It provides
 * utility methods for string representation and validity checking.
 */
class sd_card_entry {
  public:
    String name;    ///< Name of the file (without path)
    String path;    ///< Full path to the file on the SD card
    uint32_t index; ///< Index of this entry within a collection

    /**
     * @brief Default constructor creating an empty entry.
     */
    sd_card_entry() : name(""), path(""), index(0) {}

    /**
     * @brief Constructor with file information.
     * @param name Name of the file (without path)
     * @param path Full path to the file on the SD card
     * @param index Index of this entry within a collection
     */
    sd_card_entry(const char* name, const char* path, uint32_t index) :
        name(name),
        path(path),
        index(index) {}

    /**
     * @brief Converts the entry to a string representation.
     * @return String in format "name | path | index"
     */
    String to_string() const { return name + " | " + path + " | " + String(index); }

    /**
     * @brief Checks if the entry is valid (has non-empty name and path).
     * @return True if entry is valid, false otherwise
     */
    operator bool() const { return (!name.isEmpty() && !path.isEmpty()); }
};

/**
 * @brief SD card interface class for file operations.
 *
 * This class provides a comprehensive interface for interacting with SD cards,
 * including initialization, file operations, directory management, and metadata
 * operations. It supports both HSPI and standard SPI configurations.
 */
class sd_card {
  private:
#ifdef USE_HSPI_FOR_SD
    SPIClass hspi;          ///< SPI object for SD card when using HSPI
#endif                      // USE_HSPI_FOR_SD
    bool initialized;       ///< Flag indicating if SD card is initialized
    uint8_t csPin;          ///< Chip select pin for SD card
    uint8_t misoPin;        ///< MISO (Master In, Slave Out) pin
    uint8_t mosiPin;        ///< MOSI (Master Out, Slave In) pin
    uint8_t sckPin;         ///< Serial clock pin
    sdcard_type_t cardType; ///< Type of the SD card (MMC, SD, SDHC, etc.)

  public:
    /**
     * @brief Constructor for sd_card with configurable pins.
     * @param csPin Chip select pin (default from config)
     * @param misoPin MISO pin (default from config)
     * @param mosiPin MOSI pin (default from config)
     * @param sckPin Serial clock pin (default from config)
     */
    sd_card(uint8_t csPin   = SD_CS_PIN,
            uint8_t misoPin = SD_MISO_PIN,
            uint8_t mosiPin = SD_MOSI_PIN,
            uint8_t sckPin  = SD_SCK_PIN)
#ifdef USE_HSPI_FOR_SD
        :
        hspi(HSPI),
        initialized(false)
#else
        :
        initialized(false)
#endif // USE_HSPI_FOR_SD
        ,
        csPin(csPin),
        misoPin(misoPin),
        mosiPin(mosiPin),
        sckPin(sckPin),
        cardType(CARD_UNKNOWN) {
    }

    // Disable copy constructor and assignment operator
    sd_card(const sd_card&)            = delete;
    sd_card& operator=(const sd_card&) = delete;

    /**
     * Initializes the SD card.
     * @return photo_frame_error_t indicating success or failure.
     */
    photo_frame_error_t begin();

    /**
     * Ends the SD card session, releasing resources.
     * This should be called when the SD card is no longer needed.
     * It is important to call this to ensure that all data is written and the card is properly
     * unmounted. After calling this, the SD card cannot be used until `begin()` is called again.
     * @note This function should be called before the program ends or when the SD card is no longer
     * needed.
     * @note It is a good practice to call this function to ensure that the SD card is properly
     * unmounted.
     */
    void end();

    /**
     * Checks if the SD card is initialized.
     */
    bool is_initialized() const { return initialized; }

    /**
     * Returns the type of the SD card.
     * @return sdcard_type_t representing the type of the SD card.
     * If the SD card is not initialized, it returns CARD_NONE.
     */
    sdcard_type_t get_card_type() const {
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
    void print_card_type() const {
        Serial.print("Card Type: ");
        switch (cardType) {
        case CARD_MMC:     Serial.println("MMC"); break;
        case CARD_SD:      Serial.println("SDSC"); break;
        case CARD_SDHC:    Serial.println("SDHC"); break;
        case CARD_UNKNOWN: Serial.println("Unknown"); break;
        case CARD_NONE:    Serial.println("No SD card attached!"); break;
        default:           Serial.println("Unknown card type!"); break;
        }
    }

    /**
     * Gets the last modified time of a file on the SD card.
     * @param path The path to the file.
     * @return The last modified time as a time_t value.
     */
    time_t get_last_modified(const char* path) const;

    /**
     * Get the age of a file on the SD card.
     * @param path The path to the file.
     * @return The age of the file in seconds.
     */
    time_t get_file_age(const char* path) const;

    /**
     * Checks if a file exists on the SD card.
     * @param path The path to the file to check.
     * @return true if the file exists, false otherwise.
     */
    bool file_exists(const char* path) const;

    /**
     * Checks if a directory exists on the SD card.
     * @param path The path to the directory to check.
     * @return true if the directory exists, false otherwise.
     */
    bool is_directory(const char* path) const;

    /**
     * Checks if a file exists on the SD card.
     * @param path The path to the file to check.
     * @return true if the file exists, false otherwise.
     */
    bool is_file(const char* path) const;

    /**
     * List all files in the root directory of the SD card with the specified extension.
     * Then writes a table of contents (TOC) file named "toc.txt" in the root directory.
     *
     * @param total_files Pointer to a variable that will hold the total number of files written to
     * the TOC.
     * @param toc_file_name The name of the TOC file to be created (default is "/toc.txt").
     * @param root_dir The root directory to search for image files (default is "/").
     * @param extension The file extension to filter files (default is ".bmp").
     * @return photo_frame_error_t indicating success or failure.
     */
    photo_frame_error_t write_images_toc(uint32_t* total_files,
                                         const char* toc_file_name,
                                         const char* root_dir,
                                         const char* extension) const;

    /**
     * Gets the file path of the TOC file.
     * @param index The index of the TOC file.
     * @param toc_file_name The name of the TOC file (default is "toc.txt").
     * @return The file path of the TOC file as a String.
     * If the index is out of bounds, it returns an empty String.
     */
    String get_toc_file_path(uint32_t index, const char* toc_file_name = TOC_FILENAME) const;

    /**
     * Lists all files in the root directory of the SD card with the specified extension.
     * @param extension The file extension to filter files (e.g., ".jpg", ".png").
     * If the extension is null or empty, it lists all files.
     * @note This function only lists files in the root directory, not in subdirectories.
     * @note If the SD card is not initialized, it will not list any files.
     */
    void list_files(const char* extension) const;

    /**
     * Prints statistics about the SD card, including total space, used space, and free space.
     * @note This function provides a summary of the SD card's storage status.
     * It is useful for monitoring the SD card's capacity and available space.
     * @note If the SD card is not initialized, it will not print any statistics.
     * @note This function is useful for debugging and monitoring the SD card's status.
     * It can help identify issues related to storage capacity or file management.
     */
    void print_stats() const;

    /**
     * Finds the next image file on the SD card with the specified extension.
     * @param index Pointer to a variable that will hold the index of the found image.
     * @param extension The file extension to filter files (e.g., ".jpg", ".png").
     * If the extension is null or empty, it searches for all files.
     * @param file_entry Pointer to a sd_card_entry object that will be filled with the file
     * details.
     * @return photo_frame_error_t indicating success or failure.
     * If no image is found, it returns an error indicating that no image was found.
     * @note This function searches for image files in the root directory of the SD card.
     * It does not search in subdirectories.
     * @note If the SD card is not initialized, it will return an error indicating that the SD card
     * is not initialized.
     */
    photo_frame_error_t
    find_next_image(uint32_t* index, const char* extension, sd_card_entry* file_entry) const;

    /**
     * Counts the number of files with the specified extension in the root directory of the SD card.
     * @param extension The file extension to filter files (e.g., ".jpg", ".png").
     * If the extension is null or empty, it counts all files.
     * @return The number of files with the specified extension.
     * If the SD card is not initialized, it returns 0.
     * @note This function only counts files in the root directory, not in subdirectories.
     */
    uint32_t count_files(const char* extension) const;

    /**
     * Opens a file on the SD card.
     * @param path The path to the file to open.
     * @param mode The mode in which to open the file (e.g., FILE_READ, FILE_WRITE).
     * @return A File object representing the opened file.
     * If the SD card is not initialized or the file cannot be opened, it returns an empty File
     * object.
     */
    fs::File open(const char* path, const char* mode = FILE_READ);

    /**
     * Renames a file on the SD card.
     * @param pathFrom The current path of the file to rename.
     * @param pathTo The new path for the renamed file.
     * @param overwrite If true, allows overwriting an existing file.
     * @return true if the file was renamed successfully, false otherwise.
     * @note If the SD card is not initialized, it will return false.
     */
    bool rename(const char* pathFrom, const char* pathTo, bool overwrite = false);

    /**
     * Removes (deletes) a file from the SD card.
     * @param path The path to the file to remove.
     * @return true if the file was removed successfully, false otherwise.
     * @note If the SD card is not initialized or the file doesn't exist, it will return false.
     */
    bool remove(const char* path);

    /**
     * Recursively removes all files and subdirectories from a directory.
     * @param path The path to the directory to clean up.
     * @return true if all contents were removed successfully, false otherwise.
     * @note If the SD card is not initialized or the directory doesn't exist, it will return false.
     */
    bool cleanup_dir(const char* path);

    /**
     * Removes a directory from the SD card.
     * @param path The path to the directory to remove.
     * @return true if the directory was removed successfully, false otherwise.
     * @note If the SD card is not initialized or the directory doesn't exist, it will return false.
     */
    bool rmdir(const char* path);

    /**
     * Gets the size of a file on the SD card.
     * @param path The path to the file.
     * @return The size of the file in bytes, or 0 if the file doesn't exist or SD card is not
     * initialized.
     */
    size_t get_file_size(const char* path) const;

    /**
     * Creates directories recursively if they don't exist.
     * @param path The directory path to create (can be nested like "/dir1/dir2/dir3").
     * @return true if directories were created successfully or already exist, false otherwise.
     * @note If the SD card is not initialized, it will return false.
     */
    bool create_directories(const char* path);

    /**
     * Gets the amount of used space in bytes on the SD card.
     * @return The amount of used space in bytes, or 0 if the SD card is not initialized.
     */
    uint64_t used_bytes() const;

    /**
     * Gets the total bytes on the SD card.
     * @return The total space in bytes, or 0 if the SD card is not initialized.
     */
    uint64_t total_bytes() const;

    /**
     * Gets the card size in bytes.
     * @return The card size in bytes, or 0 if the SD card is not initialized.
     */
    uint64_t card_size() const;
};

} // namespace photo_frame

#endif // __PHOTO_FRAME_SD_CARD_H__