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
    SdCardEntry* file_entry,
    const char* path,
    const char* extension,
    uint32_t& current_index)
{
    Serial.print("scan_directory | path: ");
    Serial.print(path);
    Serial.print(", ext: ");
    Serial.print(extension);
    Serial.print(", current_index: ");
    Serial.print(current_index);
    Serial.print(", index: ");
    Serial.print(*index);
    Serial.println();

    bool found = false;
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

#if DEBUG_MODE
            Serial.print(current_index);
            Serial.print(F(") "));
            Serial.print(entry);
            Serial.print(F("... "));
#endif

            if (file_name.startsWith(".") || entry.endsWith(extension) == false) {
#if DEBUG_MODE
                Serial.println("skipping");
#endif
                continue;
            } else {
#if DEBUG_MODE
                Serial.println(F("ok"));
#endif
            }

            if (current_index == *index) {
                file_entry->name = file_name.c_str();
                file_entry->path = entry.c_str();
                file_entry->index = current_index;

                *index = current_index;
                found = true;
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

photo_frame_error_t SDCard::begin()
{
    if (initialized) {
        Serial.println("SD card already initialized.");
        return error_type::None;
    }

    Serial.println("Initializing SD card...");
#ifdef USE_HSPI_FOR_SD
    Serial.println("Using HSPI for SD card communication.");
    hspi.begin(sckPin, misoPin, mosiPin, csPin);
#else
    Serial.println("Using default SPI for SD card communication.");
    SPI.begin(sckPin, misoPin, mosiPin, csPin);
#endif // USE_HSPI_FOR_SD

#if defined(USE_HSPI_FOR_SD)
    if (!SD.begin(csPin, hspi)) {
        hspi.end(); // End the SPI bus for SD card
        return error_type::CardMountFailed;
    }
#else
    if (!SD.begin(csPin)) {
        SPI.end(); // End the SPI bus for SD card
        return error_type::CardMountFailed;
    }
#endif // USE_HSPI_FOR_SD

    cardType = SD.cardType();

    if (cardType == CARD_NONE) {
        SD.end();

#ifdef USE_HSPI_FOR_SD
        hspi.end(); // End the SPI bus for SD card
#else
        SPI.end(); // End the SPI bus for SD card
#endif
        return error_type::NoSdCardAttached;
    } else if (cardType == CARD_UNKNOWN) {
        SD.end();

#ifdef USE_HSPI_FOR_SD
        hspi.end(); // End the SPI bus for SD card
#else
        SPI.end(); // End the SPI bus for SD card
#endif
        return error_type::UnknownSdCardType;
    }

    initialized = true;
    return error_type::None;
} // end begin

void SDCard::end()
{
    Serial.println("Ending SD card...");
    if (initialized) {
        SD.end();
#ifdef USE_HSPI_FOR_SD
        hspi.end();
#else
        SPI.end();
#endif
        initialized = false;
    } else {
        Serial.println("SD card not initialized, nothing to end.");
    }
} // end end

void SDCard::print_stats() const
{
    if (!initialized) {
        Serial.println("SD card not initialized.");
        return;
    }
    Serial.print("Card Type: ");

    switch (cardType) {
    case CARD_MMC:
        Serial.println("MMC");
        break;
    case CARD_SD:
        Serial.println("SDSC");
        break;
    case CARD_SDHC:
        Serial.println("SDHC");
        break;
    case CARD_UNKNOWN:
        Serial.println("Unknown");
        break;
    case CARD_NONE:
        Serial.println("No SD card attached!");
        break;
    default:
        Serial.println("Unknown card type!");
        break;
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

time_t SDCard::get_last_modified(const char* path) const
{
    if (!initialized) {
        return 0; // Return 0 if SD card is not initialized
    }
    fs::File file = SD.open(path, FILE_READ);
    if (!file) {
        return 0; // Return 0 if file cannot be opened
    }
    time_t lastModified = file.getLastWrite();
    file.close();
    return lastModified;
}

photo_frame_error_t
SDCard::find_next_image(uint32_t* index, const char* extension, SdCardEntry* file_entry) const
{
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

            *index = 0;
            current_index = 0;
            return scan_directory(index, file_entry, "/", extension, current_index);
        }
    }
    return error_type::None;
} // end findNextImage

void SDCard::list_files(const char* extension) const
{
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
    String entry = root.getNextFileName();
    while (entry.length() > 0) {
        String file_name = entry.substring(entry.lastIndexOf('/') + 1);

        if (file_name.endsWith(extension) && !file_name.startsWith("/") && !file_name.startsWith(".")) {
            Serial.print(fileCount);
            Serial.print(") entry: ");
            Serial.println(file_name);

            fileCount++;
        }
        entry = root.getNextFileName();
    }
    root.close();
} // end listFiles

bool SDCard::file_exists(const char* path) const
{
    Serial.print("file_exists | path: ");
    Serial.println(path);

    if (!initialized) {
        Serial.println("SD card not initialized.");
        return false;
    }

    if (!path || strlen(path) == 0) {
        Serial.println("Invalid path provided");
        return false;
    }

    return SD.exists(path);
} // end fileExists

String SDCard::get_toc_file_path(uint32_t index, const char* toc_file_name) const
{
    Serial.print("get_toc_file_path | index: ");
    Serial.println(index);

    if (!initialized) {
        Serial.println("SD card not initialized.");
        return "";
    }

    if (!toc_file_name || strlen(toc_file_name) == 0) {
        Serial.println("Invalid TOC file name provided");
        return "";
    }

    File toc_file = SD.open(toc_file_name, FILE_READ);
    if (!toc_file) {
        Serial.println("Failed to open TOC file");
        return "";
    }

    // now read the TOC file and find the entry at the specified index
    uint32_t current_index = 0;
    while (toc_file.available()) {
        String line = toc_file.readStringUntil('\n');
        if (line.isEmpty()) {
            continue; // Skip empty lines
        }

        if (current_index == index) {
            toc_file.close();
            return line; // Return the file path at the specified index
        }
        current_index++;
    }

    toc_file.close();

#if DEBUG_MODE
    Serial.println("Index out of bounds for TOC file");
#endif
    return ""; // If index is out of bounds, return an empty string

} // end getTocFilePath

photo_frame_error_t SDCard::write_images_toc(uint32_t* total_files,
    const char* toc_file_name,
    const char* root_dir,
    const char* extension) const
{
    Serial.print("write_images_toc | toc_file_name: ");
    Serial.print(toc_file_name);
    Serial.print(", root_dir: ");
    Serial.print(root_dir);
    Serial.print(", extension: ");
    Serial.println(extension);

    if (!initialized) {
        Serial.println("SD card not initialized.");
        return error_type::NoSdCardAttached;
    }

    if (!total_files) {
        Serial.println("Total files pointer is null");
        return error_type::SdCardFileNotFound;
    }

    if (!toc_file_name || strlen(toc_file_name) == 0) {
        Serial.println("Invalid TOC file name provided");
        return error_type::SdCardFileNotFound;
    }

    if (!extension || strlen(extension) == 0 || extension[0] != '.') {
        Serial.println("Invalid extension provided");
        return error_type::SdCardFileNotFound;
    }

    File toc_file = SD.open(toc_file_name, FILE_WRITE);
    if (!toc_file) {
        Serial.println("Failed to open TOC file for writing");
        return error_type::CardTocOpenFileFailed;
    }

    File root = SD.open(root_dir, FILE_READ);
    if (!root) {
        Serial.println("Failed to open root directory");
        toc_file.close();
        return error_type::CardOpenFileFailed;
    }

    unsigned long start_time = millis();
    uint32_t file_count = 0;
    bool is_dir = false;
    String entry = root.getNextFileName(&is_dir);
    while (entry && !entry.isEmpty()) {
        String file_name = entry.substring(entry.lastIndexOf('/') + 1);
        if (!is_dir && file_name.endsWith(extension) && !file_name.startsWith("/") && !file_name.startsWith(".")) {
#if DEBUG_MODE
            Serial.print("[");
            Serial.print(file_count + 1);
            Serial.print("] ");
            Serial.print("Writing file to TOC: ");
            Serial.println(entry);
#endif
            toc_file.println(entry.c_str());
            file_count++;
        }
        entry = root.getNextFileName(&is_dir);
    }
    toc_file.close();
    root.close();

    unsigned long elapsed_time = millis() - start_time;
    Serial.print("Time taken to write TOC: ");
    Serial.print(elapsed_time);
    Serial.println(" ms");

    Serial.print("Total files written to TOC: ");
    Serial.println(file_count);

    // Set the total files count
    *total_files = file_count;

    return photo_frame::error_type::None;
}

uint32_t SDCard::count_files(const char* extension) const
{
    Serial.print("count_files | extension: ");
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
    File root = SD.open("/", FILE_READ);
    if (!root) {
        Serial.println("Failed to open root directory");
        return count;
    }

    auto start_time = millis();
    bool is_dir = false;
    String path = root.getNextFileName(&is_dir);
    while (path && !path.isEmpty()) {
        String file_name = path.substring(path.lastIndexOf('/') + 1);
        if (!is_dir && !file_name.startsWith(".") && !file_name.startsWith("/") && path.endsWith(extension)) {
            count++;
        }
        path = root.getNextFileName(&is_dir);
    }
    auto elapsed_time = millis() - start_time;
    Serial.print("Time taken to count files: ");
    Serial.print(elapsed_time);
    Serial.println(" ms");

    root.close();
    return count;
} // end getFileCount

fs::File SDCard::open(const char* path, const char* mode)
{
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