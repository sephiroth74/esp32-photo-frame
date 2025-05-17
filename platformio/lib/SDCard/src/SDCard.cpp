#include "SDCard.h"

sd_card_error_code_t SDCard::begin() {
    Serial.println("SDCard::begin");

    SPI.end(); // Ensure SPI is not in use
    SPI.begin(sck, miso, mosi, cs);
    SPI.setDataMode(SPI_MODE0);
    pinMode(cs, OUTPUT);
    digitalWrite(cs, HIGH); // Set CS high to deselect the card

    if (!SD.begin(cs)) {
        return sd_card_error_code::SDCardInitializationFailed;
    }
    return sd_card_error_code::NoError;
}

sd_card_error_code_t scan_directory(uint32_t* index,
                                    FileEntry* file_entry,
                                    const char* path,
                                    const char* extension,
                                    uint32_t& current_index) {
    Serial.print("scan_directory: ");
    Serial.print(path);
    Serial.print(", ");
    Serial.print(extension);
    Serial.print(", ");
    Serial.print(current_index);
    Serial.print(", ");
    Serial.print(*index);
    Serial.println();

    bool found    = false;
    fs::File root = SD.open(path, FILE_READ); // Open the root directory

    if (!root) {
        Serial.println("Failed to open root directory");
        return sd_card_error_code::SDCardOpenFileFailed;
    } else {
        while (true) {
            String entry = root.getNextFileName();

            if (!entry || entry.isEmpty()) {
                Serial.println("No more files in directory");
                root.close();
                return sd_card_error_code::SDCardFileNotFound; // No more files
            }

            String file_name = entry.substring(entry.lastIndexOf('/') + 1);

            Serial.print(current_index);
            Serial.print(", file name: ");
            Serial.print(file_name);
            Serial.print(", path: ");
            Serial.println(entry);

            if (file_name.startsWith(".") || entry.endsWith(extension) == false) {
                Serial.println("Skipping file");
                continue;
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
        return sd_card_error_code::NoError;
    } else {
        return sd_card_error_code::SDCardFileNotFound;
    }
}

sd_card_error_code_t
SDCard::read_next_image(uint32_t* index, const char* extension, FileEntry* file_entry) {
    uint32_t current_index = 0;
    if (scan_directory(index, file_entry, "/", extension, current_index) !=
        sd_card_error_code::NoError) {
        if (index > 0) {
            Serial.println("No more files in directory");
            Serial.println("Resetting index to 0 and try again");

            *index        = 0;
            current_index = 0;
            return scan_directory(index, file_entry, "/", extension, current_index);
        }
    }
    return sd_card_error_code::NoError;
}

fs::File SDCard::open(const char* path, const char* mode) {
    fs::File file = SD.open(path, mode);
    return file;
}

void SDCard::end() {
    Serial.println("SDCard::end");
    SD.end();
    SPI.end();
}
