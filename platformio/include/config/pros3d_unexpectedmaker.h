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
// Disabled: Using default SPI bus (VSPI) for better stability
// SD card and display are never used simultaneously, so sharing the bus is safe
// #define USE_HSPI_FOR_SD

// #define SD_MMC_CLK_PIN  12   // SDIO CLK
// #define SD_MMC_D0_PIN   13   // SDIO D0
// #define SD_MMC_CMD_PIN  14   // SDIO CMD
// #define SD_MMC_D3_PIN   15   // SDIO D3
// #define SD_MMC_D1_PIN   21   // SDIO D1
// #define SD_MMC_D2_PIN   5    // SDIO D2

// SD Card - sharing default SPI bus (VSPI) with display
#define SD_USE_SPI          // Use SPI instead of SDIO for SD card
#define SD_CS_PIN      15   // SD Card Chip Select (CS)
#define SD_SCK_PIN     12   // SD SPI Clock - IO36 (VSPI, shared with display)
#define SD_MOSI_PIN    14   // SD SPI MOSI - IO35 (VSPI, shared with display)
#define SD_MISO_PIN    13   // SD SPI MISO - IO37 (VSPI)

// e-Paper Display - using default SPI (VSPI)
#define EPD_BUSY_PIN 6   // IO6 - available digital pin
#define EPD_RST_PIN  4   // IO5 - available digital pin
#define EPD_DC_PIN   16  // IO13 - available digital pin
#define EPD_CS_PIN   38  // IO38 - SPI CS for e-paper
#define EPD_SCK_PIN  36  // IO36 - SPI Clock for e-paper (VSPI)
#define EPD_MOSI_PIN 35  // IO35 - SPI MOSI for e-paper (VSPI)

// Battery monitoring - ProS3 has MAX1704X fuel gauge over I2C
#define BATTERY_NUM_READINGS           100
#define BATTERY_DELAY_BETWEEN_READINGS 10
#define BATTERY_RESISTORS_RATIO 0.2574679943 // ProS3 built-in divider ratio

// MAX1704X I2C fuel gauge - primary battery monitoring method
// Note: USE_SENSOR_MAX1704X is defined in platformio.ini build_flags
// Note: RTC hardware support removed - time synchronization uses NTP only
#define MAX1704X_SDA_PIN   8  // IO8 - I2C SDA
#define MAX1704X_SCL_PIN   9  // IO9 - I2C SCL

// Built-in LED - ProS3 uses RGB NeoPixel on GPIO18
#ifdef LED_BUILTIN
#undef LED_BUILTIN
#endif // LED_BUILTIN

// #define LED_BUILTIN 21 // ProS3 LED pin

// RGB NeoPixel LED configuration - ProS3 built-in
#define RGB_LED_PIN     18  // GPIO18 - Built-in RGB NeoPixel on ProS3
#define RGB_LED_COUNT   1   // Single RGB LED
#define LED_PWR_PIN     17  // GPIO17 - RGB LED power control pin


// External wakeup configuration
#define WAKEUP_EXT0
#define WAKEUP_PIN GPIO_NUM_1 // GPIO1 is an RTC IO pin on ESP32-S3
#define WAKEUP_PIN_MODE INPUT_PULLUP   // Internal pull-up for button to GND
#define WAKEUP_LEVEL    LOW            // Button press pulls pin LOW

#define DELAY_BEFORE_SLEEP 8000 // Reduced since no I2C/WiFi conflicts

#define TIMEZONE             "CET-1CEST,M3.5.0,M10.5.0"

#define DISP_6C

// Display orientation is now configured dynamically via config.json (board_config.portrait_mode)
// Old compile-time constants ORIENTATION_PORTRAIT and ORIENTATION_LANDSCAPE are deprecated

#define ACCENT_COLOR COLOR_DISPLAY_RED

#define FONT_HEADER                          "assets/fonts/Ubuntu_R.h"

#define LOCALE it_IT
