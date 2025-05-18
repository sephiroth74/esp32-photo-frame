#ifndef PHOTO_FRAME_CONFIG_H__
#define PHOTO_FRAME_CONFIG_H__

// #define LOCALE en_US
#define LOCALE it_IT

// #define FONT_HEADER "fonts/Agave_Nerd.h"
// #define FONT_HEADER "fonts/RobotoMono_Nerd.h"
#define FONT_HEADER "fonts/Ubuntu_Nerd_Font_Mono.h"

// Pin definitions for the SD card
#define SD_SCK_PIN               18
#define SD_MISO_PIN              22
#define SD_MOSI_PIN              23
#define SD_CS_PIN                17

#define SLEEP_TIMEOUT_MINUTES_ON_BATTERY   60 // board sleep timeout in minutes (on battery)
#define SLEEP_TIMEOUT_MINUTES_ON_USB       10 // board sleep timeout in minutes (on USB power)

#define BATTERY_PIN              34     // ADC pin for battery voltage measurement
#define BATTERY_MONITORING_R1    100000 // 100kΩ
#define BATTERY_MONITORING_R2    100000 // 100kΩ

#define MAX_BATTERY_VOLTAGE      4200 // (millivolts)
#define MIN_BATTERY_VOLTAGE      3500 // (millivolts)

#define BATTERY_ALERT_VOLTAGE    3850 // (millivolts)
#define BATTERY_CRITICAL_VOLTAGE 3550 // (millivolts)
#define NO_BATTERY               1000 // (millivolts) - no battery connected - wired power supply

#define IMAGE_EXTENSION          ".bmp" // Image file extension

#define DISPLAY_GREY_SCALE       0   // 1 = Grey scale, 0 = Black and white (not working)
#define DISPLAY_WIDTH            800 // e-Paper display width
#define DISPLAY_HEIGHT           480 // e-Paper display height

#define DEBUG_LOG                1 // 1 = Debug log enabled, 0 = Debug log disabled

#endif // PHOTO_FRAME_CONFIG_H__