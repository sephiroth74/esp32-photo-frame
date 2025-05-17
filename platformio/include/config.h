#ifndef PHOTO_FRAME_CONFIG_H__
#define PHOTO_FRAME_CONFIG_H__

#define FONT_HEADER "fonts/Agave_Nerd_Font.h"

// Pin definitions for the SD card
#define SD_SCK_PIN               18
#define SD_MISO_PIN              22
#define SD_MOSI_PIN              23
#define SD_CS_PIN                17

#define SLEEP_DURATION_MINUTES   10 // board sleep duration in minutes

#define BATTERY_PIN              34     // ADC pin for battery voltage measurement
#define BATTERY_MONITORING_R1    100000 // 100kΩ
#define BATTERY_MONITORING_R2    100000 // 100kΩ

#define MAX_BATTERY_VOLTAGE      4200 // (millivolts)
#define MIN_BATTERY_VOLTAGE      3450 // (millivolts)

#define BATTERY_ALERT_VOLTAGE    3900 // (millivolts)
#define BATTERY_CRITICAL_VOLTAGE 3800 // (millivolts)

#define NO_BATTERY               2000

#define IMAGE_EXTENSION          ".bmp" // Image file extension

#define DISPLAY_GREY_SCALE       0
#define DISPLAY_WIDTH            800 // e-Paper display width
#define DISPLAY_HEIGHT           480 // e-Paper display height

#endif // PHOTO_FRAME_CONFIG_H__