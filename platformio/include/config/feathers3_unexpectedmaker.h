#include <Arduino.h>

// Configuration for Unexpected Maker FeatherS3 (ESP32-S3)
// Pin definitions based on FeatherS3 pinout
// https://esp32s3.com/feathers3.html

// Pin definitions for FeatherS3 (ESP32-S3) - Based on actual pinout
// SD Card - using SD_MMC (SDIO interface) with available pins
#define SD_MMC_CLK_PIN 14 // SDIO CLK - IO14 (left side)
#define SD_MMC_D0_PIN 7   // SDIO D0 - IO7 (right side)
#define SD_MMC_CMD_PIN 17 // SDIO CMD - IO17 (left side)
#define SD_MMC_D3_PIN 11  // SDIO D3 - IO11 (right side)
#define SD_MMC_D1_PIN 3   // SDIO D1 - IO3 (right side)
#define SD_MMC_D2_PIN 12  // SDIO D2 - IO12 (left side)

// e-Paper Display - using separate SPI pins to avoid SDIO conflicts
// Using available output-capable pins that don't conflict with SD card or other peripherals
#define EPD_BUSY_PIN 6   // A4 (IO6) - available digital pin
#define EPD_RST_PIN  5   // A5 (IO5) - available digital pin
#define EPD_DC_PIN   10  // IO10 - available digital pin (avoiding SD card pins)
#define EPD_CS_PIN   38  
#define EPD_SCK_PIN  36  
#define EPD_MOSI_PIN 35  

// Potentiometer pin - using analog pins
#define POTENTIOMETER_PWR_PIN   33   // IO33 - available digital pin for power control
#define POTENTIOMETER_INPUT_PIN 18   // A0 (IO17) - Analog pin from pinout
#define POTENTIOMETER_INPUT_MAX 4095 // 12-bit ADC

// Battery monitoring - FeatherS3 has built-in battery voltage divider on GPIO2
#define BATTERY_PIN                    2 // Built-in battery voltage pin (GPIO2)
#define BATTERY_NUM_READINGS           100
#define BATTERY_DELAY_BETWEEN_READINGS 10
#define BATTERY_RESISTORS_RATIO 0.2574679943 // FeatherS3 built-in divider ratio

#define USE_SENSOR_MAX1704X
#define MAX1704X_SDA_PIN   SDA
#define MAX1704X_SCL_PIN   SCL

// Built-in LED - FeatherS3 uses RGB NeoPixel on GPIO40
#ifdef LED_BUILTIN
#undef LED_BUILTIN
#endif // LED_BUILTIN

#define LED_BUILTIN 13 // FeatherS3 LED pin

// RGB NeoPixel LED configuration - FeatherS3 built-in
#define RGB_LED_PIN     40  // GPIO40 - Built-in RGB NeoPixel
#define RGB_LED_COUNT   1   // Single RGB LED
// #define LED_PWR_PIN 39      // Led power control pin (GPIO39)

// PCF8523 RTC - shares I2C bus with battery fuel gauge
#define RTC_SDA_PIN 8 // IO8 - I2C SDA for RTC (shared with battery fuel gauge)
#define RTC_SCL_PIN 9 // IO9 - I2C SCL for RTC (shared with battery fuel gauge)
// RTC Class definition for DS3231
#define RTC_CLASS_DS3231

// External wakeup configuration
#define WAKEUP_EXT0
#define WAKEUP_PIN GPIO_NUM_1 // GPIO1 is an RTC IO pin on ESP32-S3 (available)
#define WAKEUP_PIN_MODE INPUT_PULLUP   // Internal pull-up for button to GND
#define WAKEUP_LEVEL    LOW            // Button press pulls pin LOW

#define DELAY_BEFORE_SLEEP 8000 // Reduced since no I2C/WiFi conflicts

#define TIMEZONE             "CET-1CEST,M3.5.0,M10.5.0"

#define DISP_BW_V2              
#define USE_DESPI_DRIVER

// #define ACCENT_COLOR GxEPD_RED

#define FONT_HEADER                          "assets/fonts/Ubuntu_R.h"

#define LOCALE it_IT
