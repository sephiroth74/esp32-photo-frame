#ifndef __PHOTO_FRAME_SD_CARD_H__
#define __PHOTO_FRAME_SD_CARD_H__

#include "FS.h"
#include "config.h"
#include "errors.h"
#include <Arduino.h>
#include <SD.h>
#include <SPI.h>

namespace photo_frame {

class SdCardEntry {
  public:
    String name;
    String path;
    uint32_t index;

    SdCardEntry() : name(""), path(""), index(0) {}

    SdCardEntry(const char* name, const char* path, uint32_t index) :
        name(name),
        path(path),
        index(index) {}

    String to_string() const { return name + " | " + path + " | " + String(index); }

    operator bool() const { return (!name.isEmpty() && !path.isEmpty()); }
};

class SDCard {
  private:
    SPIClass hspi; // SPI object for SD card
    bool initialized;
    uint8_t csPin;
    uint8_t misoPin;
    uint8_t mosiPin;
    uint8_t sckPin;
    sdcard_type_t cardType;

  public:
    SDCard(uint8_t csPin   = SD_CS_PIN,
           uint8_t misoPin = SD_MISO_PIN,
           uint8_t mosiPin = SD_MOSI_PIN,
           uint8_t sckPin  = SD_SCK_PIN) :
        hspi(HSPI),
        initialized(false),
        csPin(csPin),
        misoPin(misoPin),
        mosiPin(mosiPin),
        sckPin(sckPin),
        cardType(CARD_UNKNOWN) {}

    // Disable copy constructor and assignment operator
    SDCard(const SDCard&)            = delete;
    SDCard& operator=(const SDCard&) = delete;

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
     * Lists all files in the root directory of the SD card with the specified extension.
     * @param extension The file extension to filter files (e.g., ".jpg", ".png").
     * If the extension is null or empty, it lists all files.
     * @note This function only lists files in the root directory, not in subdirectories.
     * @note If the SD card is not initialized, it will not list any files.
     */
    void listFiles(const char* extension) const;

    /**
     * Prints statistics about the SD card, including total space, used space, and free space.
     * @note This function provides a summary of the SD card's storage status.
     * It is useful for monitoring the SD card's capacity and available space.
     * @note If the SD card is not initialized, it will not print any statistics.
     * @note This function is useful for debugging and monitoring the SD card's status.
     * It can help identify issues related to storage capacity or file management.
     */
    void printStats() const;

    /**
     * Finds the next image file on the SD card with the specified extension.
     * @param index Pointer to a variable that will hold the index of the found image.
     * @param extension The file extension to filter files (e.g., ".jpg", ".png").
     * If the extension is null or empty, it searches for all files.
     * @param file_entry Pointer to a SdCardEntry object that will be filled with the file details.
     * @return photo_frame_error_t indicating success or failure.
     * If no image is found, it returns an error indicating that no image was found.
     * @note This function searches for image files in the root directory of the SD card.
     * It does not search in subdirectories.
     * @note If the SD card is not initialized, it will return an error indicating that the SD card
     * is not initialized.
     */
    photo_frame_error_t
    findNextImage(uint32_t* index, const char* extension, SdCardEntry* file_entry) const;

    /**
     * Counts the number of files with the specified extension in the root directory of the SD card.
     * @param extension The file extension to filter files (e.g., ".jpg", ".png").
     * If the extension is null or empty, it counts all files.
     * @return The number of files with the specified extension.
     * If the SD card is not initialized, it returns 0.
     * @note This function only counts files in the root directory, not in subdirectories.
     */
    uint32_t getFileCount(const char* extension) const;

    /**
     * Opens a file on the SD card.
     * @param path The path to the file to open.
     * @param mode The mode in which to open the file (e.g., FILE_READ, FILE_WRITE).
     * @return A File object representing the opened file.
     * If the SD card is not initialized or the file cannot be opened, it returns an empty File
     * object.
     */
    fs::File open(const char* path, const char* mode = FILE_READ);
};

} // namespace photo_frame

#endif // __PHOTO_FRAME_SD_CARD_H__