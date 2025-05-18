#define USE_ESP_IDF_LOG

#include <Arduino.h>
#include <Battery.h>
#include <EPD.h>
#include <Preferences.h>
#include <SDCard.h>
#include <stdint.h>
#include <stdio.h>

#include "_locale.h"
#include "board_util.h"
#include "config.h"
#include "renderer.h"

#include "DEV_Config.h"
#include "GUI_Paint.h"

#include <icons/icons.h>

SDCard sdCard(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
Preferences preferences;

photo_frame::Renderer* renderer = nullptr;

void setup() {
    Serial.begin(115200);
    delay(2500); // Give time for Serial to initialize

    Battery battery;
    FileEntry file_entry;
    uint32_t image_index              = 0;
    sd_card_error_code_t sdcard_error = sd_card_error_code_t::NoError;

    // Disable the built-in LED to save power
    photo_frame::disable_builtin_led();

    // first read battery voltage
    Serial.print(F("Reading battery voltage... "));
    read_battery_with_resistor(&battery,
                               BATTERY_PIN,
                               BATTERY_MONITORING_R1,
                               BATTERY_MONITORING_R2,
                               MIN_BATTERY_VOLTAGE,
                               MAX_BATTERY_VOLTAGE);

    Serial.println(F("done."));
    Serial.print(F("Battery rawADC: "));
    Serial.print(battery.raw);
    Serial.print(F(" | voltage: "));
    Serial.print(battery.voltage);
    Serial.print(F(" mV | percent: "));
    Serial.print(battery.percent);
    Serial.println(F(" %"));

    if (battery) {
        if (battery.voltage < BATTERY_CRITICAL_VOLTAGE) {
            Serial.println(F("Battery voltage too low, going to deep sleep..."));
            SD.end();  // End the SD library to free up resources
            SPI.end(); // End SPI to free up resources
            photo_frame::enter_deep_sleep();
            return;
        }
    }

    Serial.print("Initializing SD card... ");
    sdcard_error = sdCard.begin();

    if (sdcard_error != sd_card_error_code::NoError) {
        Serial.println(F("failed"));
    } else {
        Serial.println(F("done."));
    }

    if (sdcard_error == sd_card_error_code::NoError) {
        Serial.print("Reading next index from preferences... ");
        if (!preferences.begin("photo_frame", true)) {
            Serial.println(F("failed"));
        } else {
            Serial.println(F("done."));
            image_index = preferences.getUInt("index", 0);
            preferences.end();
        }

        Serial.print(F("Next index: "));
        Serial.println(image_index);

        // read the next image file from the SD card
        Serial.println(F("Reading next image from SD card..."));

        unsigned long startTime = millis();

        sdcard_error = sdCard.read_next_image(&image_index, IMAGE_EXTENSION, &file_entry);

        if (sdcard_error != sd_card_error_code::NoError) {
            Serial.println(F("failed"));
            Serial.print("Index: ");
            Serial.println(image_index);
            Serial.print("File entry: ");
            Serial.println(file_entry.to_string());
        } else {
            Serial.println(F("done."));
        }

        unsigned long elapsedTime = millis() - startTime;
        Serial.print(F("Elapsed time: "));
        Serial.print(elapsedTime);
        Serial.println(F(" ms"));

        Serial.print(F("File entry: "));
        Serial.println(file_entry.to_string());
        Serial.print(F("Index: "));
        Serial.println(image_index);

        if (preferences.begin("photo_frame", false)) {
            Serial.print(F("Saving index to preferences... "));
            Serial.println(image_index + 1);
            preferences.putUInt("index", (image_index + 1));
            preferences.end();
        }
    }

    renderer = new photo_frame::Renderer();
    if (renderer == nullptr) {
        Serial.println(F("Failed to allocate memory for renderer"));
        SD.end();  // End the SD library to free up resources
        SPI.end(); // End SPI to free up resources
        return;
    }

    Serial.print(F("Initializing renderer... "));

    if (!renderer->init()) {
        Serial.println(F("failed."));
        SD.end();  // End the SD library to free up resources
        SPI.end(); // End SPI to free up resources
        return;
    } else {
        Serial.println(F("done."));
    }

    Serial.println(F("Clearing the screen..."));
    renderer->clear();

    // read the next image file from the SD card
    if (sdcard_error == sd_card_error_code::NoError) {
        Serial.println(F("Opening file from the SD card..."));
        fs::File file = sdCard.open(file_entry.path.c_str(), FILE_READ);
        if (!file) {
            Serial.println(F("Failed to open file"));
            renderer->drawCenteredWarningMessage(TXT_SDCARD_OPEN_FAILED);
            SD.end();  // End the SD library to free up resources
            SPI.end(); // End SPI to free up resources
            return;
        }

        Serial.print(F("Drawing image from file... "));

        if (renderer->drawBitmapFromFile(file) !=
            photo_frame::renderer_error_code::OperatoionComplete) {
            Serial.println(F("failed."));
        } else {
            Serial.println(F("done."));
        }

        file.close();

    } else {
        const char* error_message = TXT_SDCARD_FILE_NOT_FOUND;

        switch (sdcard_error) {
        case sd_card_error_code::SDCardInitializationFailed:
            error_message = TXT_SDCARD_INIT_FAILED;
            break;
        case sd_card_error_code::SDCardOpenFileFailed:
            error_message = TXT_SDCARD_OPEN_FAILED;
            break;
        case sd_card_error_code::SDCardFileNotFound:
            error_message = TXT_SDCARD_FILE_NOT_FOUND;
            break;
        default: break;
        }

        renderer->drawCenteredWarningMessage(error_message);
    }

    renderer->drawCentererMessage(
        battery.toString().c_str(), photo_frame::font_size::FontSize14pt, BLACK, WHITE);

    Serial.println(F("Drawing battery status"));
    renderer->drawBatteryStatus(battery);

    Serial.println(F("Display the screen"));
    renderer->display();

    Serial.println("Turning off the screen");
    renderer->sleep();

    Serial.println(F("Freeing up resources"));
    if (renderer) {
        delete renderer;
        renderer = nullptr;
    }

    SD.end();  // End the SD library to free up resources
    SPI.end(); // End SPI to free up resources

    Serial.println(F("Going to deep sleep..."));

    photo_frame::sleep_enable_timer_wakeup(battery);
    photo_frame::enter_deep_sleep();
}

void loop() {}
