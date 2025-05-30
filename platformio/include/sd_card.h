#ifndef __PHOTO_FRAME_SD_CARD_H__
#define __PHOTO_FRAME_SD_CARD_H__

#include "FS.h"
#include "config.h"
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

    photo_frame_error_t begin();

    void end();

    bool isInitialized() const { return initialized; }

    sdcard_type_t getCardType() const {
        if (!initialized) {
            return CARD_NONE;
        }
        return cardType;
    }

    void listFiles(const char* extension) const;

    void printStats() const;

    photo_frame_error_t
    findNextImage(uint32_t* index, const char* extension, SdCardEntry* file_entry) const;

    uint32_t getFileCount(const char* extension) const;

    fs::File open(const char* path, const char* mode = FILE_READ);
};

} // namespace photo_frame

#endif // __PHOTO_FRAME_SD_CARD_H__