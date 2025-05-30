#include "sd_card.h"

namespace photo_frame {

photo_frame_error_t scan_directory(uint32_t* index,
                                   SdCardEntry* file_entry,
                                   const char* path,
                                   const char* extension,
                                   uint32_t& current_index) {
    Serial.print("scan_directory | path: ");
    Serial.print(path);
    Serial.print(", ext: ");
    Serial.print(extension);
    Serial.print(", current_index: ");
    Serial.print(current_index);
    Serial.print(", index: ");
    Serial.print(*index);
    Serial.println();

    bool found    = false;
    fs::File root = SD.open(path, FILE_READ); // Open the root directory

    if (!root) {
        Serial.println("Failed to open root directory");
        return error_type::CardOpenFileFailed;
    } else {
        while (true) {
            String entry = root.getNextFileName();

            if (!entry || entry.isEmpty()) {
                Serial.print(current_index);
                Serial.println(") No more files in directory");
                root.close();
                return error_type::SdCardFileNotFound;
            }

            String file_name = entry.substring(entry.lastIndexOf('/') + 1);

#if DEBUG_LOG
            Serial.print(current_index);
            Serial.print(F(") "));
            Serial.print(entry);
            Serial.print(F("... "));
#endif

            if (file_name.startsWith(".") || entry.endsWith(extension) == false) {
#if DEBUG_LOG
                Serial.println("skipping");
#endif
                continue;
            } else {
#if DEBUG_LOG
                Serial.println(F("ok"));
#endif
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

photo_frame_error_t SDCard::begin() {
    if (initialized) {
        Serial.println("SD card already initialized.");
        return error_type::None;
    }

    Serial.println("Initializing SD card...");
    hspi.begin(sckPin, misoPin, mosiPin, csPin);
    if (!SD.begin(SD_CS_PIN, hspi)) {
        hspi.end(); // End the SPI bus for SD card
        return error_type::CardMountFailed;
    }

    cardType = SD.cardType();

    if (cardType == CARD_NONE) {
        SD.end();
        hspi.end(); // End the SPI bus for SD card
        return error_type::NoSdCardAttached;
    } else if (cardType == CARD_UNKNOWN) {
        SD.end();
        hspi.end(); // End the SPI bus for SD card
        return error_type::UnknownSdCardType;
    }

    initialized = true;
    return error_type::None;
} // end begin

void SDCard::end() {
    Serial.println("Ending SD card...");
    if (initialized) {
        SD.end();
        hspi.end();
        initialized = false;
    }
} // end end

void SDCard::printStats() const {
    if (!initialized) {
        Serial.println("SD card not initialized.");
        return;
    }
    Serial.print("Card Type: ");

    switch (cardType) {
    case CARD_MMC:     Serial.println("MMC"); break;
    case CARD_SD:      Serial.println("SDSC"); break;
    case CARD_SDHC:    Serial.println("SDHC"); break;
    case CARD_UNKNOWN: Serial.println("Unknown"); break;
    case CARD_NONE:    Serial.println("No SD card attached!"); break;
    default:           Serial.println("Unknown card type!"); break;
    }

    Serial.print("Card Size: ");
    Serial.print(SD.cardSize() / (1024 * 1024));
    Serial.println(" MB");
    Serial.print("Total Size: ");
    Serial.print(SD.totalBytes() / (1024 * 1024));
    Serial.println(" MB");
    Serial.print("Used Size: ");
    Serial.print(SD.usedBytes() / (1024 * 1024));
    Serial.println(" MB");
} // end printStats

photo_frame_error_t
SDCard::findNextImage(uint32_t* index, const char* extension, SdCardEntry* file_entry) const {
    Serial.print("read_next_image | index: ");
    Serial.print(*index);
    Serial.print(", ext: ");
    Serial.println(extension);

    if (!extension || strlen(extension) == 0 || extension[0] != '.') {
        Serial.println("Invalid extension provided");
        return error_type::SdCardFileNotFound;
    }

    if (!initialized) {
        return error_type::NoSdCardAttached;
    }

    uint32_t current_index = 0;
    if (scan_directory(index, file_entry, "/", extension, current_index) != error_type::None) {
        if (index > 0) {
            Serial.println("No more files in directory");
            Serial.println("Resetting index to 0 and try again");

            *index        = 0;
            current_index = 0;
            return scan_directory(index, file_entry, "/", extension, current_index);
        }
    }
    return error_type::None;
} // end findNextImage

void SDCard::listFiles(const char* extension) const {
    if (!extension || strlen(extension) == 0 || extension[0] != '.') {
        Serial.println("Invalid extension provided");
        return;
    }

    Serial.print("Listing files on SD card with extension: ");
    Serial.println(extension);

    if (!initialized) {
        Serial.println("SD card not initialized.");
        return;
    }

    File root = SD.open("/", FILE_READ);
    if (!root) {
        Serial.println("Failed to open root directory");
        return;
    }

    uint32_t fileCount = 1;
    String entry       = root.getNextFileName();
    while (entry.length() > 0) {
        String file_name = entry.substring(entry.lastIndexOf('/') + 1);

        if (file_name.endsWith(extension) && !file_name.startsWith("/") &&
            !file_name.startsWith(".")) {
            Serial.print(fileCount);
            Serial.print(") entry: ");
            Serial.println(file_name);

            fileCount++;
        }
        entry = root.getNextFileName();
    }
    root.close();
} // end listFiles

uint32_t SDCard::getFileCount(const char* extension) const {
    Serial.print("getFileCount | extension: ");
    Serial.println(extension);

    if (!extension || strlen(extension) == 0) {
        Serial.println("Invalid extension provided");
        return 0;
    }
    if (extension[0] != '.') {
        Serial.println("Extension should start with a dot (.)");
        return 0;
    }

    if (!initialized) {
        return 0;
    }

    uint32_t count = 0;
    File root      = SD.open("/", FILE_READ);
    if (!root) {
        Serial.println("Failed to open root directory");
        return count;
    }

    auto start_time = millis();
    String path     = root.getNextFileName();
    while (path && !path.isEmpty()) {
        String file_name = path.substring(path.lastIndexOf('/') + 1);
        if (!file_name.startsWith(".") && !file_name.startsWith("/") && path.endsWith(extension)) {
            count++;
        }
        path = root.getNextFileName();
    }
    auto elapsed_time = millis() - start_time;
    Serial.print("Time taken to count files: ");
    Serial.print(elapsed_time);
    Serial.println(" ms");

    root.close();
    return count;
} // end getFileCount

fs::File SDCard::open(const char* path, const char* mode) {
    Serial.print("SDCard::open | path: ");
    Serial.print(path);
    Serial.print(", mode: ");
    Serial.println(mode);

    if (!initialized) {
        Serial.println("SD card not initialized.");
        return fs::File();
    }

    fs::File file = SD.open(path, mode);
    return file;
} // end open

} // namespace photo_frame