#include <Arduino.h>

// Configuration for Unexpected Maker ProS3(d) (ESP32-S3)
// Pin definitions based on ProS3 pinout
// https://unexpectedmaker.com/shop.html#!/ProS3-D/p/759221737

// Pin definitions for ProS3(d) (ESP32-S3) - Based on actual pinout

// ==========================================================================
// SPI Pin Configuration
// ==========================================================================

// Enable separate SPI bus for SD card
// Using HSPI for SD to avoid conflicts when BLE/BT diagnostics are active
#define USE_HSPI_FOR_SD

// #define SD_MMC_CLK_PIN  12   // SDIO CLK
// #define SD_MMC_D0_PIN   13   // SDIO D0
// #define SD_MMC_CMD_PIN  14   // SDIO CMD
// #define SD_MMC_D3_PIN   15   // SDIO D3
// #define SD_MMC_D1_PIN   21   // SDIO D1
// #define SD_MMC_D2_PIN   5    // SDIO D2

// SD Card - sharing default SPI bus (VSPI) with display
#define SD_USE_SPI // Use SPI instead of SDIO for SD card
#define SD_SCK_PIN 12 // SD SPI Clock - IO36 (VSPI, shared with display)
#define SD_MISO_PIN 13 // SD SPI MISO - IO37 (VSPI)
#define SD_MOSI_PIN 14 // SD SPI MOSI - IO35 (VSPI, shared with display)
#define SD_CS_PIN 15 // SD Card Chip Select (CS)

// ==========================================================================
// E-Paper Display Pin Configuration
// ==========================================================================

#define DISP_6C
#define ACCENT_COLOR DISPLAY_COLOR_RED
#define FONT_HEADER "assets/fonts/Ubuntu_R.h"

// e-Paper Display - using default SPI (VSPI)
#define EPD_BUSY_PIN 6 // IO6 - available digital pin
#define EPD_RST_PIN 4 // IO5 - available digital pin
#define EPD_DC_PIN 16 // IO13 - available digital pin
#define EPD_CS_PIN 38 // IO38 - SPI CS for e-paper
#define EPD_SCK_PIN 36 // IO36 - SPI Clock for e-paper (VSPI)
#define EPD_MOSI_PIN 35 // IO35 - SPI MOSI for e-paper (VSPI)

#define DISPLAY_TIMEOUT_MS 60000

// ===========================================================================
// Battery Monitoring Configuration
// ===========================================================================

// Battery monitoring - ProS3 has MAX1704X fuel gauge over I2C
#define BATTERY_NUM_READINGS 100
#define BATTERY_DELAY_BETWEEN_READINGS 10
#define BATTERY_RESISTORS_RATIO 0.2574679943 // ProS3 built-in divider ratio

// MAX1704X I2C fuel gauge - primary battery monitoring method
// Note: USE_SENSOR_MAX1704X is defined in platformio.ini build_flags
#define MAX1704X_SDA_PIN 8 // IO8 - I2C SDA
#define MAX1704X_SCL_PIN 9 // IO9 - I2C SCL

// ===========================================================================
// User Interface Hardware
// ===========================================================================

// RGB NeoPixel LED configuration - ProS3 built-in
#define RGB_LED_PIN 18 // GPIO18 - Built-in RGB NeoPixel on ProS3
#define RGB_LED_COUNT 1 // Single RGB LED

// External wakeup configuration
#define WAKEUP_EXT0
#define WAKEUP_PIN GPIO_NUM_1 // GPIO1 is an RTC IO pin on ESP32-S3
#define WAKEUP_PIN_MODE INPUT_PULLUP // Internal pull-up for button to GND
#define WAKEUP_LEVEL LOW // Button press pulls pin LOW

#define DELAY_BEFORE_SLEEP 8000 // Reduced since no I2C/WiFi conflicts

// Display power control using ProS3 LDO2 feature
// GPIO17 controls the LDO2 output (3.3V controllable power rail)
// HIGH = LDO2 ON (display powered), LOW = LDO2 OFF (display unpowered)
#define DISPLAY_POWER_PIN 17
#define DISPLAY_POWER_ACTIVE_LOW 0 // ProS3 LDO2: HIGH = ON, LOW = OFF

#if defined(ENABLE_WEBSERVER_DATAPROVIDER)
#define WS_LISTEN_TIMEOUT_MS (60 * 60 * 1000) // 10 minutes timeout for subsequent boots
#endif // ENABLE_WEBSERVER_DATAPROVIDER

// ===========================================================================
// Timezone Configuration
// ===========================================================================
#define TIMEZONE "CET-1CEST,M3.5.0,M10.5.0"

#define LOCALE it_IT
