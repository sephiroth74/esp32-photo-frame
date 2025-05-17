#ifndef SD_CARD_H__
#define SD_CARD_H__

#include <Arduino.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>

typedef enum sd_card_error_code {
    NoError = 0,
    SDCardInitializationFailed, // SD card initialization failed
    SDCardOpenFileFailed,       // Failed to open file or directory on the SD card
    SDCardFileNotFound,         // File not found on SD card
} sd_card_error_code_t;

class FileEntry {
  public:
    FileEntry() : name(""), path(""), index(0) {}

    FileEntry(const char* name, const char* path, uint32_t index) :
        name(name),
        path(path),
        index(index) {}

    String name;
    String path;
    uint32_t index;

    String to_string() const { return name + " | " + path + " | " + String(index); }

    operator bool() const { return (!name.isEmpty() && !path.isEmpty()); }
};

class SDCard {
  public:
    SDCard(uint8_t sck, uint8_t miso, uint8_t mosi, uint8_t cs) :
        sck(sck),
        miso(miso),
        mosi(mosi),
        cs(cs) {}

    // Setup the SD card
    sd_card_error_code_t begin();

    // Read the next image from the SD card
    sd_card_error_code_t read_next_image(uint32_t* index, const char* extension, FileEntry* file_entry);

    // Open a file on the SD card
    fs::File open(const char* path, const char* mode = FILE_READ);

    // End the SD card
    void end();

  private:
    uint8_t sck;
    uint8_t miso;
    uint8_t mosi;
    uint8_t cs;
};

#endif