#ifndef __PHOTO_FRAME_CONFIG_H__
#define __PHOTO_FRAME_CONFIG_H__

#include "_locale.h"
#include <Arduino.h>

/**
 * LICENSE
 * This file is part of the Photo Frame project.
 */

// Pin definitions

// SD Card
extern const uint8_t SD_CS_PIN;
extern const uint8_t SD_MISO_PIN;
extern const uint8_t SD_MOSI_PIN;
extern const uint8_t SD_SCK_PIN;

// e-Paper Display
extern const uint8_t EPD_CS_PIN;
extern const uint8_t EPD_DC_PIN;
extern const uint8_t EPD_RST_PIN;
extern const uint8_t EPD_BUSY_PIN;
extern const uint8_t EPD_MISO_PIN;
extern const uint8_t EPD_MOSI_PIN;
extern const uint8_t EPD_SCK_PIN;
extern const uint8_t EPD_PWR_PIN;

// RTC
extern const uint8_t RTC_SDA_PIN;
extern const uint8_t RTC_SCL_PIN;
extern const uint8_t RTC_POWER_PIN;

// Level pin
extern const uint8_t LEVEL_PWR_PIN;   // Power pin for level shifter
extern const uint8_t LEVEL_INPUT_PIN; // SDA pin for level shifter
extern const uint16_t LEVEL_INPUT_MAX;

#ifndef LED_BUILTIN
#define LED_BUILTIN 2 // Built-in LED pin for ESP32
#endif                // LED_BUILTIN

// Wifi
// #define WIFI_SSID            "FiOS-RS1LV"
// #define WIFI_PASSWORD        "airs900pop4456chew"

extern const char* WIFI_SSID;
extern const char* WIFI_PASSWORD;

#define WIFI_CONNECT_TIMEOUT 10000 // 10 seconds
#define TIMEZONE             "CET-1CEST,M3.5.0,M10.5.0/3"
#define NTP_TIMEOUT          20000 // ms
#define NTP_SERVER1          "pool.ntp.org"
#define NTP_SERVER2          "time.nist.gov"

// e-Paper display

#define DISP_BW_V2 // Black and White e-Paper display (GDEH0154D67)
// #define DISP_7C_F    // 7-color e-Paper display (GDEY073C46)

#define USE_DESPI_DRIVER // Use Despi driver for e-Paper display
// #define USE_WAVESHARE_DRIVER // Use Waveshare driver for e-Paper display

extern const uint8_t ACCENT_COLOR; // Accent color for the display

// Miscellaneous

#define DEBUG_LOG              1 // Enable debug logging (0 = off, 1 = on)

#define MICROSECONDS_IN_SECOND 1000000
#define SECONDS_IN_MINUTE      60
#define SECONDS_IN_HOUR        3600

// Minimum time between refreshes in seconds (minimum 1 minute, maximum 2 hours)
#define REFRESH_MIN_INTERVAL_SECONDS 60

// Maximum time between refreshes in seconds (minimum 2 minutes, maximum 4 hours)
#define REFRESH_MAX_INTERVAL_SECONDS 2 * SECONDS_IN_HOUR

// time between refreshes in seconds when battery is low
#define REFRESH_INTERVAL_SECONDS_LOW_BATTERY 6 * SECONDS_IN_HOUR

#define DAY_START_HOUR                       06 // Hour when the day starts (6 AM)
#define DAY_END_HOUR                         22 // Hour when the day ends (10 PM) (midnight is 0)

#define FONT_HEADER                          "assets/fonts/Ubuntu_R.h"
// #define FONT_HEADER                          "assets/fonts/UbuntuMono_R.h"
// #define FONT_HEADER                          "assets/fonts/FreeMono.h"
// #define FONT_HEADER                          "assets/fonts/FreeSans.h"
// #define FONT_HEADER                          "assets/fonts/Lato_Regular.h"
// #define FONT_HEADER                          "assets/fonts/Montserrat_Regular.h"
// #define FONT_HEADER                          "assets/fonts/OpenSans_Regular.h"
// #define FONT_HEADER                          "assets/fonts/Poppins_Regular.h"
// #define FONT_HEADER                          "assets/fonts/Quicksand_Regular.h"
// #define FONT_HEADER                          "assets/fonts/Raleway_Regular.h"
// #define FONT_HEADER                          "assets/fonts/Roboto_Regular.h"
// #define FONT_HEADER                          "assets/fonts/RobotoMono_Regular.h"
// #define FONT_HEADER                          "assets/fonts/RobotoSlab_Regular.h"

// LOCALE
//   Language (Territory)            code
//   English (United States)         en_US
//   Italiano (Italia)               it_IT
#define LOCALE it_IT

typedef class photo_frame_error {
  public:
    const char* message;
    uint8_t code;
    uint8_t blink_count; // Number of times to blink the built-in LED for this error

    constexpr photo_frame_error(const char* msg, uint8_t err_code) :
        message(msg),
        code(err_code),
        blink_count(1) {}
    constexpr photo_frame_error(const char* msg, uint8_t err_code, uint8_t blink_count) :
        message(msg),
        code(err_code),
        blink_count(blink_count) {}

    constexpr bool operator==(const photo_frame_error& other) const {
        return (this->code == other.code) && (this->blink_count == other.blink_count);
    }
    constexpr bool operator!=(const photo_frame_error& other) const { return !(*this == other); }
} photo_frame_error_t;

namespace error_type {
const photo_frame_error None{TXT_NO_ERROR, 0, 0};
const photo_frame_error CardMountFailed{TXT_CARD_MOUNT_FAILED, 3, 3};
const photo_frame_error NoSdCardAttached{TXT_NO_SD_CARD_ATTACHED, 4, 4};
const photo_frame_error UnknownSdCardType{TXT_UNKNOWN_SD_CARD_TYPE, 5, 5};
const photo_frame_error CardOpenFileFailed{TXT_CARD_OPEN_FILE_FAILED, 6, 6};
const photo_frame_error SdCardFileNotFound{TXT_SD_CARD_FILE_NOT_FOUND, 7, 7};
const photo_frame_error SdCardFileOpenFailed{TXT_SD_CARD_FILE_OPEN_FAILED, 8, 8};
const photo_frame_error ImageFormatNotSupported{TXT_IMAGE_FORMAT_NOT_SUPPORTED, 9, 9};
// Add more errors here
} // namespace error_type

#endif // __PHOTO_FRAME_CONFIG_H__