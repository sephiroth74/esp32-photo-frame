#include <Arduino.h>

// Configuration for Unexpected Maker ProS3(d) (ESP32-S3)
// Pin definitions based on ProS3(d) pinout
// https://unexpectedmaker.com/shop.html#!/ProS3-D/p/759221737

// -------------------------------------------
// OTA Update Configuration
// -------------------------------------------

// #define OTA_SERVER_URL "http://192.168.188.100:8000/"
// #define OTA_VERSION_ENDPOINT "version.json"

#define OTA_USE_SSL true // Use HTTPS for secure updates

// Pin definitions for ProS3(d) (ESP32-S3) - Based on pinout diagram
// SD Card - using SD_MMC (SDIO interface) with available pins
#define SD_MMC_CLK_PIN 39  // SDIO CLK - IO39 (MTCK available pin)
#define SD_MMC_D0_PIN  40  // SDIO D0 - IO40 (MTDO available pin)
#define SD_MMC_CMD_PIN 41  // SDIO CMD - IO41 (MTDI available pin)
#define SD_MMC_D3_PIN  42  // SDIO D3 - IO42 (MTMS available pin)
#define SD_MMC_D1_PIN  2   // SDIO D1 - IO2 (available pin)
#define SD_MMC_D2_PIN  3   // SDIO D2 - IO3 (available pin)

// e-Paper Display - using separate SPI pins to avoid SDIO conflicts
#define EPD_BUSY_PIN 12   // IO21 - available RTC pin
#define EPD_RST_PIN  13   // IO16 - available pin
#define EPD_DC_PIN   14   // IO15 - available pin
#define EPD_CS_PIN   15   // IO14 - available pin
#define EPD_SCK_PIN  36   // SPI SCK pin from pinout (D13)
#define EPD_MOSI_PIN 35   // SPI MOSI pin from pinout (D12)

// Potentiometer pin - using analog pins
#define POTENTIOMETER_PWR_PIN   38   // IO38 - available pin for power control
#define POTENTIOMETER_INPUT_PIN 4    // IO4 (A3) - Analog pin from pinout
#define POTENTIOMETER_INPUT_MAX 4095 // 12-bit ADC

// I2C Bus Configuration - ProS3(d) specific feature
// Shared I2C bus for MAX1704X Battery Fuel Gauge and PCF8523 RTC
#define MAX1704X_SDA_PIN 8  // IO8 - I2C SDA for battery fuel gauge (from pinout)
#define MAX1704X_SCL_PIN 9  // IO9 - I2C SCL for battery fuel gauge (from pinout)

// PCF8523 RTC - shares I2C bus with battery fuel gauge
#define RTC_SDA_PIN 8  // IO8 - I2C SDA for RTC (shared with battery fuel gauge)
#define RTC_SCL_PIN 9  // IO9 - I2C SCL for RTC (shared with battery fuel gauge)
// RTC Class definition for PCF8523
#define RTC_CLASS RTC_PCF8523 

// Built-in LED - ProS3(d) uses standard LED on IO13
#ifdef LED_BUILTIN
#undef LED_BUILTIN
#endif // LED_BUILTIN

#define LED_BUILTIN 13 // ProS3(d) LED pin

// RGB NeoPixel LED configuration - ProS3(d) built-in
#define RGB_LED_PIN     18  // GPIO18 - Built-in RGB NeoPixel (from pinout)
#define RGB_LED_COUNT   1   // Single RGB LED
#define LDO2_PWR_PIN    17  // GPIO17 - LDO2 power enable pin (from pinout)

// External wakeup configuration - more RTC pins available on ProS3(d)
#define WAKEUP_EXT0
#define WAKEUP_PIN GPIO_NUM_1 // GPIO1 is an RTC IO pin on ESP32-S3 (available)
#define WAKEUP_PIN_MODE INPUT_PULLUP   // Internal pull-up for button to GND
#define WAKEUP_LEVEL    LOW            // Button press pulls pin LOW

#define DELAY_BEFORE_SLEEP 8000 // Reduced since no I2C/WiFi conflicts

#define TIMEZONE             "CET-1CEST,M3.5.0,M10.5.0"

#define DISP_6C              // 6-color e-Paper display (GDEP073E01)
#define USE_DESPI_DRIVER

#define ACCENT_COLOR GxEPD_RED

#define FONT_HEADER                          "assets/fonts/Ubuntu_R.h"

#define LOCALE it_IT