#include <Arduino.h>

// Configuration for Unexpected Maker FeatherS3 (ESP32-S3)
// Pin definitions based on FeatherS3 pinout
// https://feathers3.io/pinout.html

// Pin definitions for FeatherS3 (ESP32-S3) - Based on actual pinout
// SD Card - using SD_MMC (SDIO interface) with available pins
#define SD_MMC_CLK_PIN 14 // SDIO CLK - IO14 (left side)
#define SD_MMC_D0_PIN 7   // SDIO D0 - IO7 (right side)
#define SD_MMC_CMD_PIN 17 // SDIO CMD - IO17 (left side)
#define SD_MMC_D3_PIN 11  // SDIO D3 - IO11 (right side)
#define SD_MMC_D1_PIN 3   // SDIO D1 - IO3 (right side)
#define SD_MMC_D2_PIN 12  // SDIO D2 - IO12 (left side)

// e-Paper Display - using separate SPI pins to avoid SDIO conflicts
#define EPD_BUSY_PIN 6   // A4 (IO6) - available digital pin
#define EPD_RST_PIN  5   // A5 (IO5) - available digital pin
#define EPD_DC_PIN   10  // IO10 - available digital pin (avoiding SD card pins)
#define EPD_CS_PIN   38   // IO8 - available digital pin (non-RTC, freeing GPIO1 for wakeup)
#define EPD_SCK_PIN  36  // SPI SCK pin from pinout
#define EPD_MOSI_PIN 35  // SPI MO pin from pinout
#define EPD_MISO_PIN 37  // SPI MI pin from pinout

// Potentiometer pin - using analog pins
#define POTENTIOMETER_PWR_PIN   33   // IO33 - available digital pin for power control
#define POTENTIOMETER_INPUT_PIN 18   // A0 (IO17) - Analog pin from pinout
#define POTENTIOMETER_INPUT_MAX 4095 // 12-bit ADC

// Battery monitoring - FeatherS3 has built-in battery voltage divider on GPIO2
#define BATTERY_PIN                    2 // Built-in battery voltage pin (GPIO2)
#define BATTERY_NUM_READINGS           100
#define BATTERY_DELAY_BETWEEN_READINGS 10
#define BATTERY_RESISTORS_RATIO 0.2574679943 // FeatherS3 built-in divider ratio

// Built-in LED - FeatherS3 uses RGB NeoPixel on GPIO40
#ifdef LED_BUILTIN
#undef LED_BUILTIN
#endif // LED_BUILTIN

#define LED_BUILTIN 13 // FeatherS3 LED pin

// RGB NeoPixel LED configuration - FeatherS3 built-in
#define RGB_LED_PIN     40  // GPIO40 - Built-in RGB NeoPixel
#define RGB_LED_COUNT   1   // Single RGB LED
#define RGB_LED_PWR_PIN 39  // GPIO39 - NeoPixel power control (LDO2)

// External wakeup configuration
#define WAKEUP_EXT0
// Use available RTC GPIO pin for wakeup (ESP32-S3 RTC pins: GPIO0-GPIO21)
#define WAKEUP_PIN GPIO_NUM_1 // GPIO1 is an RTC IO pin on ESP32-S3 (available)
#define WAKEUP_PIN_MODE INPUT_PULLUP   // Internal pull-up for button to GND
#define WAKEUP_LEVEL    LOW            // Button press pulls pin LOW

// ESP32-S3 has many RTC IO pins available for deep sleep wakeup (GPIO0-GPIO21)
// We're using GPIO3 which is confirmed as an RTC IO pin

// Power saving configuration
#define BATTERY_POWER_SAVING

// Delay before going to sleep in milliseconds
#define DELAY_BEFORE_SLEEP 8000 // Reduced since no I2C/WiFi conflicts

// WiFi configuration
#define WIFI_FILENAME "/wifi.txt"

// Network timeouts - can be more aggressive since ESP32-S3 is more stable
#define WIFI_CONNECT_TIMEOUT 8000 // Reduced timeout
#define TIMEZONE             "CET-1CEST,M3.5.0,M10.5.0"
#define NTP_TIMEOUT          10000 // Reduced timeout
#define NTP_SERVER1          "pool.ntp.org"
#define NTP_SERVER2          "time.nist.gov"

// e-Paper display configuration
#define DISP_6C // 6-color e-Paper display (GDEP073E01)
// #define DISP_BW_V2 // Black and White e-Paper display
#define USE_DESPI_DRIVER

// Accent color for the display
#define ACCENT_COLOR GxEPD_RED

// Miscellaneous
#define DEBUG_MODE 0
#define DEBUG_BATTERY_READER
#define DEBUG_BOARD

// Reset handling
#define RESET_INVALIDATES_DATE_TIME 1

// Refresh intervals - can be more frequent due to better power management
#define REFRESH_MIN_INTERVAL_SECONDS            (10 * SECONDS_IN_MINUTE)
#define REFRESH_MAX_INTERVAL_SECONDS            (4 * SECONDS_IN_HOUR)
#define REFRESH_STEP_SECONDS                    (10 * SECONDS_IN_MINUTE)
#define REFRESH_INTERVAL_SECONDS_LOW_BATTERY    (8 * SECONDS_IN_HOUR)

#define DAY_START_HOUR                       06 // Hour when the day starts (6 AM)
#define DAY_END_HOUR                         23 // Hour when the day ends (11 PM)

#define FONT_HEADER                          "assets/fonts/Ubuntu_R.h"

// LOCALE
#define LOCALE it_IT // Default to Italian for new configuration

// Google Drive integration - fully enabled since no I2C/WiFi conflicts
// Google Drive is now enabled globally in config.h
#define GOOGLE_DRIVE_CONFIG_FILEPATH "/google_drive_config.json"

// Service account cleanup interval
#define CLEANUP_TEMP_FILES_INTERVAL_SECONDS (24 * 60 * 60) // 24 hours

#define GOOGLE_DRIVE_STREAM_PARSER_THRESHOLD    2097152 // 2MB - avoid streaming for most responses
#define GOOGLE_DRIVE_JSON_DOC_SIZE              2097152 // 2MB JSON document buffer
#define GOOGLE_DRIVE_BODY_RESERVE_SIZE          2097152 // 2MB response body reserve
#define GOOGLE_DRIVE_SAFETY_LIMIT               2097152 // 2MB safety limit

#define GOOGLE_DRIVE_MAX_LIST_PAGE_SIZE 250 // Max 1000, but 250 is a good compromise

// FeatherS3 (ESP32-S3) advantages:
// 1. ESP32-S3 with dual core for excellent performance and larger IRAM (solves memory constraints)
// 2. 8MB PSRAM for improved Google Drive streaming and image processing performance
// 3. No I2C/WiFi coexistence issues (unlike ESP32-C6)
// 4. Built-in USB-C and excellent battery charging circuit with JST connector
// 5. RGB NeoPixel LED (GPIO40) for status indication
// 6. SDIO interface for faster SD card performance vs SPI mode
// 7. Built-in voltage divider for clean battery monitoring on A13 (GPIO18)
// 8. Stable network operations and excellent deep sleep reliability with multiple RTC GPIO pins
// 9. Native support for concurrent I2C and WiFi operations
// 10. Premium performance with sufficient memory for all features without IRAM overflow