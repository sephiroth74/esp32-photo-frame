#define USE_ESP_IDF_LOG

#include <Arduino.h>
#include <EPD.h>
#include <Preferences.h>
#include <SDCard.h>
#include <Battery.h>
#include <stdint.h>
#include <stdio.h>

#include "board_util.h"
#include "config.h"
#include "display_util.h"
#include "messages.h"

#include "DEV_Config.h"
#include "GUI_Paint.h"

#include <icons/icons.h>

SDCard sdCard(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
Preferences preferences;

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

    if (battery.voltage > NO_BATTERY) {
        if (battery.voltage < BATTERY_CRITICAL_VOLTAGE) {
            Serial.println(F("Battery voltage too low, going to deep sleep..."));
            SD.end();  // End the SD library to free up resources
            SPI.end(); // End SPI to free up resources
            photo_frame::enter_deep_sleep();
            return;
        }
    }

    DEV_Module_Init();

#if DISPLAY_GREY_SCALE
    EPD_7IN5_V2_Init_4Gray();
#else
    EPD_7IN5_V2_Init();
#endif

    EPD_7IN5_V2_Clear();
    DEV_Delay_ms(500);

    // Create a new image cache
    UBYTE* BlackImage;
    /* you have to edit the startup_stm32fxxx.s file and set a big enough heap size */
    UWORD Imagesize =
        ((DISPLAY_WIDTH % 8 == 0) ? (DISPLAY_WIDTH / 8) : (DISPLAY_WIDTH / 8 + 1)) * DISPLAY_HEIGHT;
    if ((BlackImage = (UBYTE*)malloc(Imagesize)) == NULL) {
        Serial.println("Failed to apply for black memory...");
        return;
    }

    Paint_NewImage(BlackImage, DISPLAY_WIDTH, DISPLAY_HEIGHT, 0, WHITE);

#if DISPLAY_GREY_SCALE
    // Paint_SetScale(4);
#endif

    // Paint_SelectImage(BlackImage);
    Paint_Clear(WHITE);

    // read the next image file from the SD card
    if (sdcard_error == sd_card_error_code::NoError) {
        fs::File file = sdCard.open(file_entry.path.c_str(), FILE_READ);

        photo_frame::draw_bitmap_from_file(file);
        file.close();
    } else {
        photo_frame::draw_centered_warning_message(
            warning_icon_196x196, TXT_SDCARD_INIT_FAILED, 196, 196);
    }

    photo_frame::draw_battery_status(battery.voltage, battery.percent);

    // EPD_7IN5_V2_Clear();
    EPD_7IN5_V2_Display(BlackImage);
    DEV_Delay_ms(500);

    Serial.println("Turn off the screen...");
    EPD_7IN5_V2_Sleep();
    free(BlackImage);
    BlackImage = NULL;

    SD.end();  // End the SD library to free up resources
    SPI.end(); // End SPI to free up resources
    photo_frame::sleep_enable_timer_wakeup(battery);
    photo_frame::enter_deep_sleep();
}

void loop() {}
