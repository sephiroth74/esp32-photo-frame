#include <Arduino.h>

// Configuration for Unexpected Maker ProS3(d) (ESP32-S3)
// Pin definitions based on ProS3 pinout
// https://unexpectedmaker.com/shop.html#!/ProS3-D/p/759221737

// Pin definitions for ProS3(d) (ESP32-S3) - Based on actual pinout
// Using SEPARATE SPI buses for SD card and e-Paper display to avoid conflicts
//
// SPI Bus Configuration:
// - SD Card: Uses HSPI (secondary SPI bus) - initialized first in sd_card.cpp
// - Display: Uses VSPI (default SPI bus) - initialized later in renderer.cpp
//
// This configuration follows the GxEPD2_SD_Example pattern:
// 1. SD card initialization creates SPIClass hspi(HSPI) and passes it to SD.begin()
// 2. Display initialization calls SPI.end() then SPI.begin() with display pins
// 3. Both devices can operate independently on their dedicated SPI buses
//
// This approach resolves SPI conflicts that occur when both devices share the same bus

// Enable separate SPI bus for SD card
#define USE_HSPI_FOR_SD

// #define SD_MMC_CLK_PIN 14 // SDIO CLK
// #define SD_MMC_D0_PIN 6 // SDIO D0
// #define SD_MMC_CMD_PIN 16 // SDIO CMD
// #define SD_MMC_D3_PIN 12 // SDIO D3
// #define SD_MMC_D1_PIN 21 // SDIO D1
// #define SD_MMC_D2_PIN 15 // SDIO D2

// SD Card - using custom HSPI bus
#define SD_USE_SPI          // Use SPI instead of SDIO for SD card
#define SD_CS_PIN      12   // SD Card Chip Select (CS) - IO12
#define SD_SCK_PIN     14   // SD SPI Clock - IO14 (HSPI)
#define SD_MOSI_PIN    16   // SD SPI MOSI - IO16 (HSPI)
#define SD_MISO_PIN    15   // SD SPI MISO - IO15 (HSPI)

// e-Paper Display - using default SPI (VSPI)
#define EPD_BUSY_PIN 6   // IO6 - available digital pin
#define EPD_RST_PIN  5   // IO5 - available digital pin
#define EPD_DC_PIN   13  // IO13 - available digital pin
#define EPD_CS_PIN   38  // IO38 - SPI CS for e-paper
#define EPD_SCK_PIN  36  // IO36 - SPI Clock for e-paper (VSPI)
#define EPD_MOSI_PIN 35  // IO35 - SPI MOSI for e-paper (VSPI)

// Potentiometer pin - using analog pins
#define POTENTIOMETER_PWR_PIN   7    // Disabled - direct 3.3V power
#define POTENTIOMETER_INPUT_PIN 3    // IO3 - Analog pin (ADC1_CH2)
#define POTENTIOMETER_INPUT_MAX 4095 // 12-bit ADC

// Battery monitoring - ProS3 has MAX1704X fuel gauge over I2C
#define BATTERY_PIN                    2 // Built-in battery voltage pin (GPIO2) - backup only
#define BATTERY_NUM_READINGS           100
#define BATTERY_DELAY_BETWEEN_READINGS 10
#define BATTERY_RESISTORS_RATIO 0.2574679943 // ProS3 built-in divider ratio

// MAX1704X I2C fuel gauge - primary battery monitoring method
// Note: USE_SENSOR_MAX1704X is defined in platformio.ini build_flags
#define MAX1704X_SDA_PIN   8  // IO8 - I2C SDA (shared with RTC)
#define MAX1704X_SCL_PIN   9  // IO9 - I2C SCL (shared with RTC)

// Built-in LED - ProS3 uses RGB NeoPixel on GPIO18
#ifdef LED_BUILTIN
#undef LED_BUILTIN
#endif // LED_BUILTIN

// #define LED_BUILTIN 21 // ProS3 LED pin

// RGB NeoPixel LED configuration - ProS3 built-in
#define RGB_LED_PIN     18  // GPIO18 - Built-in RGB NeoPixel on ProS3
#define RGB_LED_COUNT   1   // Single RGB LED
#define LED_PWR_PIN     17  // GPIO17 - RGB LED power control pin

// PCF8523 RTC - shares I2C bus with battery fuel gauge
// Note: USE_RTC is defined in platformio.ini build_flags
#define RTC_SDA_PIN 8 // IO8 - I2C SDA for RTC (shared with battery fuel gauge)
#define RTC_SCL_PIN 9 // IO9 - I2C SCL for RTC (shared with battery fuel gauge)
// RTC Class definition for PCF8523
#define RTC_CLASS_PCF8523

// External wakeup configuration
#define WAKEUP_EXT0
#define WAKEUP_PIN GPIO_NUM_1 // GPIO1 is an RTC IO pin on ESP32-S3
#define WAKEUP_PIN_MODE INPUT_PULLUP   // Internal pull-up for button to GND
#define WAKEUP_LEVEL    LOW            // Button press pulls pin LOW

#define DELAY_BEFORE_SLEEP 8000 // Reduced since no I2C/WiFi conflicts

#define TIMEZONE             "CET-1CEST,M3.5.0,M10.5.0"

#define DISP_6C
#define USE_DESPI_DRIVER

// #define ACCENT_COLOR GxEPD_RED

#define FONT_HEADER                          "assets/fonts/Ubuntu_R.h"

#define LOCALE it_IT
