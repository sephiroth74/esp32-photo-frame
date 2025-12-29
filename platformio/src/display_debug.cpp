// Display Debug Mode - Clean Testing Suite
// This file runs instead of main.cpp when ENABLE_DISPLAY_DIAGNOSTIC is defined

#ifdef ENABLE_DISPLAY_DIAGNOSTIC

#include <Arduino.h>
#include "config.h"
#include "renderer.h"
#include "battery.h"
#include "board_util.h"
#include "string_utils.h"
#include "rgb_status.h"
#include "io_utils.h"
#include "spi_manager.h"
#include "sd_card.h"
#include "errors.h"
#include <Preferences.h>
#include <SPI.h>

// External instances defined in main.cpp and renderer.cpp
extern GxEPD2_DISPLAY_CLASS<GxEPD2_DRIVER_CLASS, MAX_HEIGHT(GxEPD2_DRIVER_CLASS)> display;
extern photo_frame::sd_card sdCard;
extern photo_frame::battery_reader battery_reader;

namespace display_debug {

// Forward declarations
void printMenu();

static bool displayInitialized = false;
static bool batteryInitialized = false;

void ensureDisplayInitialized() {
    if (!displayInitialized) {
        photo_frame::renderer::init_display();
        displayInitialized = true;
        log_d("Display initialized");
    }
}

void ensureBatteryInitialized() {
    if (!batteryInitialized) {
        battery_reader.init();
        batteryInitialized = true;
        log_d("Battery initialized");
    }
}

// ============================================================================
// Test 1: Battery Status Test
// ============================================================================
void testBatteryStatus() {
    log_i("\n========================================");
    log_i("Battery Status");
    log_i("========================================");

    ensureBatteryInitialized();
    photo_frame::battery_info_t info = battery_reader.read();

    Serial.println(F("\nBattery Information:"));

#ifdef USE_SENSOR_MAX1704X
    Serial.print(F("  Cell Voltage: "));
    Serial.print(info.cell_voltage);
    Serial.println(F(" V"));

    Serial.print(F("  Charge Rate: "));
    Serial.print(info.charge_rate);
    Serial.println(F(" mA"));
#else
    Serial.print(F("  Raw Value: "));
    Serial.println(info.raw_value);

    Serial.print(F("  Raw Millivolts: "));
    Serial.print(info.raw_millivolts);
    Serial.println(F(" mV"));

    Serial.print(F("  Millivolts: "));
    Serial.print(info.millivolts);
    Serial.println(F(" mV"));
#endif

    Serial.print(F("  Percentage: "));
    Serial.print(info.percent);
    Serial.println(F(" %"));

    Serial.print(F("  Status: "));
    if (info.is_critical()) {
        Serial.println(F("CRITICAL"));
    } else if (info.is_low()) {
        Serial.println(F("LOW"));
    } else if (info.is_charging()) {
        Serial.println(F("CHARGING"));
    } else {
        Serial.println(F("NORMAL"));
    }

    Serial.println(F("\n========================================"));
}

// ============================================================================
// Test 2: LED Test
// ============================================================================
void testLED() {
    log_i("\n========================================");
    log_i("LED Test");
    log_i("========================================");

#ifdef RGB_STATUS_ENABLED
    Serial.println(F("\nTesting RGB LED..."));
    Serial.println(F("This will cycle through colors"));

    // Initialize RGB LED
    if (!rgbStatus.begin()) {
        log_e("Failed to initialize RGB LED");
        return;
    }

    // Test sequence with different colors
    Serial.println(F("  Color: RED"));
    rgbStatus.setCustomColor({255, 0, 0}, RGBEffect::SOLID, 0, 128);
    delay(800);

    Serial.println(F("  Color: GREEN"));
    rgbStatus.setCustomColor({0, 255, 0}, RGBEffect::SOLID, 0, 128);
    delay(800);

    Serial.println(F("  Color: BLUE"));
    rgbStatus.setCustomColor({0, 0, 255}, RGBEffect::SOLID, 0, 128);
    delay(800);

    Serial.println(F("  Color: YELLOW"));
    rgbStatus.setCustomColor({255, 255, 0}, RGBEffect::SOLID, 0, 128);
    delay(800);

    Serial.println(F("  Color: CYAN"));
    rgbStatus.setCustomColor({0, 255, 255}, RGBEffect::SOLID, 0, 128);
    delay(800);

    Serial.println(F("  Color: MAGENTA"));
    rgbStatus.setCustomColor({255, 0, 255}, RGBEffect::SOLID, 0, 128);
    delay(800);

    Serial.println(F("  Color: WHITE"));
    rgbStatus.setCustomColor({255, 255, 255}, RGBEffect::SOLID, 0, 128);
    delay(800);

    // Test pulse effect
    Serial.println(F("  Effect: PULSE (White)"));
    rgbStatus.setCustomColor({255, 255, 255}, RGBEffect::PULSE, 0, 128);
    delay(2000);

    // Turn off
    Serial.println(F("  Turning off..."));
    rgbStatus.end();

    Serial.println(F("\nRGB LED test complete!"));
#else
    #ifdef LED_BUILTIN
        Serial.println(F("\nTesting built-in LED..."));
        pinMode(LED_BUILTIN, OUTPUT);

        for (int i = 0; i < 5; i++) {
            Serial.print(F("  Blink "));
            Serial.println(i + 1);
            digitalWrite(LED_BUILTIN, HIGH);
            delay(200);
            digitalWrite(LED_BUILTIN, LOW);
            delay(200);
        }

        Serial.println(F("\nBuilt-in LED test complete!"));
    #else
        Serial.println(F("\nNo LED configured for this board"));
    #endif
#endif

    Serial.println(F("\n========================================"));
}

// ============================================================================
// Test 3: Potentiometer Test
// ============================================================================
void testPotentiometer() {
    log_i("\n========================================");
    log_i("Potentiometer Test");
    log_i("========================================");

#ifdef POTENTIOMETER_INPUT_PIN
    Serial.println(F("\nReading potentiometer for 10 seconds..."));
    Serial.print(F("  Pin: GPIO"));
    Serial.println(POTENTIOMETER_INPUT_PIN);
    Serial.print(F("  ADC Range: 0-"));
    Serial.println(POTENTIOMETER_INPUT_MAX);
    Serial.println(F("  Turn the potentiometer to see values change"));
    Serial.println(F("  Using read_refresh_seconds() from main application"));
    Serial.println();

    // Configure ADC
    analogReadResolution(12); // 12-bit resolution (0-4095)

    unsigned long startTime = millis();
    unsigned long lastReadTime = 0;
    long lastRefreshSeconds = -1;

    // Header
    Serial.println(F("Time | Refresh Interval (using main app logic)"));
    Serial.println(F("-----|----------------------------------------"));

    while (millis() - startTime < 10000) { // 10 seconds
        unsigned long currentTime = millis() - startTime;

        // Read every 500ms using the main application function
        if (currentTime - lastReadTime >= 500) {
            // Use the actual function from board_util.cpp
            // This tests the REAL logic used by the main application
            long refreshSeconds = photo_frame::board_utils::read_refresh_seconds(false);

            // Only print if value changed
            if (refreshSeconds != lastRefreshSeconds) {
                lastRefreshSeconds = refreshSeconds;

                // Format time display
                char timeStr[8];
                sprintf(timeStr, "%4lu", currentTime / 1000);
                Serial.print(timeStr);
                Serial.print(F(" | "));

                // Convert seconds to human-readable format using utility from main app
                char humanTime[64];
                photo_frame::string_utils::seconds_to_human(humanTime, sizeof(humanTime), refreshSeconds);
                Serial.print(humanTime);

                // Also show in minutes for quick reference
                Serial.print(F(" ("));
                Serial.print(refreshSeconds / 60);
                Serial.println(F(" min)"));
            }

            lastReadTime = currentTime;
        }

        delay(10); // Small delay to prevent watchdog issues
    }

    Serial.println(F("-----|----------------------------------------"));

    // Show final value with full details
    if (lastRefreshSeconds > 0) {
        Serial.println(F("\nFinal reading:"));
        Serial.print(F("  Refresh interval: "));
        char humanTime[64];
        photo_frame::string_utils::seconds_to_human(humanTime, sizeof(humanTime), lastRefreshSeconds);
        Serial.println(humanTime);
        Serial.print(F("  In seconds: "));
        Serial.println(lastRefreshSeconds);
        Serial.print(F("  In minutes: "));
        Serial.println(lastRefreshSeconds / 60);
    }

    Serial.println(F("\n✓ Test uses the SAME logic as main application"));
    Serial.println(F("✓ This validates the actual refresh interval calculation"));
    Serial.println(F("\nPotentiometer test complete!"));
#else
    Serial.println(F("\nNo potentiometer configured for this board"));
#endif

    Serial.println(F("\n========================================"));
}

// ============================================================================
// Test 4: SD Card Tests
// ============================================================================
static bool sdCardInitialized = false;

void ensureSDCardInitialized() {
    if (!sdCardInitialized) {
        log_d("Initializing SD card...");
        photo_frame::photo_frame_error_t error = sdCard.begin();
        if (error == photo_frame::error_type::None) {
            sdCardInitialized = true;
            log_d("SD card initialized successfully");
        } else {
            log_e("Failed to initialize SD card");
        }
    }
}

void testSDCardInfo() {
    log_i("\n========================================");
    log_i("SD Card Information");
    log_i("========================================\n");

    ensureSDCardInitialized();

    if (!sdCardInitialized) {
        log_e("ERROR: SD card not initialized!");
        log_e("Please check:");
        log_e("  - SD card is inserted");
        log_e("  - SD card is formatted (FAT32)");
        log_e("  - Connections are correct");
        return;
    }

    // Get card info
    Serial.println(F("SD Card Details:"));
    Serial.print(F("  Card Type: "));

    uint8_t cardType = sdCard.get_card_type();
    switch (cardType) {
        case CARD_NONE:
            Serial.println(F("None"));
            break;
        case CARD_MMC:
            Serial.println(F("MMC"));
            break;
        case CARD_SD:
            Serial.println(F("SD"));
            break;
        case CARD_SDHC:
            Serial.println(F("SDHC"));
            break;
        default:
            Serial.println(F("Unknown"));
    }

    // Get size info
    uint64_t cardSize = sdCard.card_size();
    Serial.print(F("  Total Size: "));
    Serial.print(cardSize / (1024 * 1024));
    Serial.println(F(" MB"));

    uint64_t usedBytes = sdCard.used_bytes();
    Serial.print(F("  Used Space: "));
    Serial.print(usedBytes / (1024 * 1024));
    Serial.println(F(" MB"));

    uint64_t freeBytes = cardSize - usedBytes;
    Serial.print(F("  Free Space: "));
    Serial.print(freeBytes / (1024 * 1024));
    Serial.println(F(" MB"));

    float usedPercent = (usedBytes * 100.0) / cardSize;
    Serial.print(F("  Usage: "));
    Serial.print(usedPercent, 1);
    Serial.println(F("%"));

    Serial.println(F("\n✓ SD card is working correctly!"));
    Serial.println(F("\n========================================"));
}

void testSDCardListFiles() {
    log_i("\n========================================");
    log_i("List Files in Root");
    log_i("========================================\n");

    ensureSDCardInitialized();

    if (!sdCardInitialized) {
        log_e("SD card not initialized!");
        return;
    }

    Serial.println(F("Files in root directory:"));
    Serial.println(F("Size (KB) | Type | Name"));
    Serial.println(F("----------|------|---------------------"));

    fs::File root = sdCard.open("/");
    if (!root) {
        log_e("Failed to open root directory");
        return;
    }

    if (!root.isDirectory()) {
        log_e("Root is not a directory");
        return;
    }

    int fileCount = 0;
    int dirCount = 0;
    uint64_t totalSize = 0;

    fs::File file = root.openNextFile();
    while (file) {
        if (file.isDirectory()) {
            Serial.print(F("   <DIR>  |  DIR | "));
            Serial.println(file.name());
            dirCount++;
        } else {
            size_t fileSize = file.size();
            totalSize += fileSize;

            char sizeStr[12];
            sprintf(sizeStr, "%8u", (unsigned int)(fileSize / 1024));
            Serial.print(sizeStr);
            Serial.print(F(" | FILE | "));
            Serial.println(file.name());
            fileCount++;
        }
        file = root.openNextFile();
    }

    Serial.println(F("----------|------|---------------------"));
    Serial.print(F("\nSummary:\n"));
    Serial.print(F("  Files: "));
    Serial.println(fileCount);
    Serial.print(F("  Directories: "));
    Serial.println(dirCount);
    Serial.print(F("  Total size: "));
    Serial.print(totalSize / 1024);
    Serial.println(F(" KB"));

    Serial.println(F("\n========================================"));
}

void testSDCardConfig() {
    log_i("\n========================================");
    log_i("Load and Validate config.json");
    log_i("========================================\n");

    ensureSDCardInitialized();

    if (!sdCardInitialized) {
        log_e("ERROR: SD card not initialized!");
        return;
    }

    photo_frame::unified_config systemConfig; // Unified configuration system
    photo_frame::photo_frame_error_t error = photo_frame::load_unified_config_with_fallback(sdCard, CONFIG_FILEPATH, systemConfig);

    if (error != photo_frame::error_type::None) {
        log_e("Failed to load unified configuration - Error code: %d", error.code);
    }

    // Validate essential configuration
    if (!systemConfig.wifi.is_valid()) {
        log_w("WiFi configuration is missing or invalid!");
        log_w("Please ensure CONFIG_FILEPATH contains valid WiFi credentials");
    }

    log_d("json: %s", systemConfig.to_json().c_str());

}

void showSDCardMenu() {
    Serial.println(F("\n========================================"));
    Serial.println(F("SD Card Test Menu"));
    Serial.println(F("========================================"));
    Serial.println(F("a - SD Card Information"));
    Serial.println(F("b - List Files in Root"));
    Serial.println(F("c - Load and Validate config.json"));
    Serial.println(F(""));
    Serial.println(F("0 - Back to Main Menu"));
    Serial.println(F("========================================"));
    Serial.println(F("Enter your choice:"));
}

void handleSDCardMenu() {
    showSDCardMenu();

    bool inSubmenu = true;
    while (inSubmenu) {
        if (Serial.available() > 0) {
            char input = Serial.read();

            // Clear any remaining input
            while (Serial.available() > 0) {
                Serial.read();
                delay(10);
            }

            switch (input) {
                case 'a':
                case 'A':
                    testSDCardInfo();
                    showSDCardMenu();
                    break;

                case 'b':
                case 'B':
                    testSDCardListFiles();
                    showSDCardMenu();
                    break;

                case 'c':
                case 'C':
                    testSDCardConfig();
                    showSDCardMenu();
                    break;

                case '0':
                    inSubmenu = false;
                    printMenu();
                    break;

                case '\n':
                case '\r':
                    // Ignore newlines
                    break;

                default:
                    Serial.print(F("Unknown command: "));
                    Serial.println(input);
                    Serial.println(F("Press '0' for main menu"));
                    break;
            }
        }
        delay(100);
    }
}

// ============================================================================
// Test 5: Deep Sleep Test
// ============================================================================
void testDeepSleep() {
    log_i("\n========================================");
    log_i("Deep Sleep Test");
    log_i("========================================");

    // Configure wake-up button
    Serial.println(F("\nConfiguring wake-up source..."));
    Serial.print(F("  Wake-up pin: GPIO"));
    Serial.println((int)WAKEUP_PIN);
    Serial.println(F("  Mode: Button to GND (internal pull-up)"));

    // Configure the RTC GPIO for EXT0 wakeup
    // The pin needs to be configured as RTC GPIO before deep sleep
    pinMode(WAKEUP_PIN, WAKEUP_PIN_MODE);

    // Configure EXT0 wakeup (single RTC GPIO)
    // Wake when pin goes LOW (button pressed, connecting to GND)
    esp_sleep_enable_ext0_wakeup(WAKEUP_PIN, LOW);

    Serial.println(F("\nDeep sleep configuration:"));
    Serial.println(F("  - EXT0 wake-up enabled"));
    Serial.println(F("  - Press button to wake up"));
    Serial.println(F("  - All peripherals will be powered off"));

    // Turn off RGB LED before sleep
#ifdef RGB_STATUS_ENABLED
    Serial.println(F("  - RGB LED: OFF"));
    rgbStatus.end();
#endif

    Serial.println(F("\n========================================"));
    Serial.println(F("Entering deep sleep in 3 seconds..."));
    Serial.println(F("Press the button to wake up"));
    Serial.println(F("========================================"));
    Serial.flush();

    delay(3000);

    // Enter deep sleep
    Serial.println(F("Going to sleep now..."));
    Serial.flush();
    delay(100);

    esp_deep_sleep_start();

    // This line will never be reached
}

// ============================================================================
// Restart Command
// ============================================================================
void restartBoard() {
    log_i("\n========================================");
    log_i("Restarting board...");
    log_i("========================================");
    Serial.flush();
    delay(100);
    ESP.restart();
}

// ============================================================================
// Interactive Menu
// ============================================================================
void printMenu() {
    Serial.println(F("\n========================================"));
    Serial.println(F("Display Debug Menu"));
    Serial.println(F("========================================"));
    Serial.println(F("1 - Battery Status Test"));
    Serial.println(F("2 - LED Test"));
    Serial.println(F("3 - Potentiometer Test"));
    Serial.println(F("4 - SD Card Tests"));
    Serial.println(F("5 - Deep Sleep Test"));
    Serial.println(F(""));
    Serial.println(F("r - Restart Board"));
    Serial.println(F("0 - Show this menu"));
    Serial.println(F("========================================"));
    Serial.println(F("Enter your choice:"));
}

void handleSerialInput() {
    if (Serial.available() > 0) {
        char input = Serial.read();

        // Clear any remaining input
        while (Serial.available() > 0) {
            Serial.read();
            delay(10);
        }

        switch (input) {
            case '0':
                printMenu();
                break;

            case '1':
                testBatteryStatus();
                break;

            case '2':
                testLED();
                break;

            case '3':
                testPotentiometer();
                break;

            case '4':
                handleSDCardMenu();
                break;

            case '5':
                testDeepSleep();
                break;

            case 'r':
            case 'R':
                restartBoard();
                break;

            case '\n':
            case '\r':
                // Ignore newlines
                break;

            default:
                Serial.print(F("Unknown command: "));
                Serial.println(input);
                Serial.println(F("Press '0' for menu"));
                break;
        }
    }
}

} // namespace display_debug

// ============================================================================
// Public Functions (called from main.cpp when ENABLE_DISPLAY_DIAGNOSTIC is set)
// ============================================================================
void display_debug_setup() {
    // Inizializza la seriale
    Serial.begin(115200);

    // --- BLOCCA FINCHÉ NON APRI IL MONITOR ---
    // Questo è vitale sulla Nano ESP32 per non perdere i messaggi iniziali
    // Rimuovilo se devi alimentare la scheda con batterie senza PC!
    while (!Serial) {
        delay(100);
    }

    log_i("\n\n========================================");
    log_i("Display Debug Mode - STARTED");
    log_i("========================================");

    // Check wake-up reason
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    const char* wakeup_reason_str;
    switch (wakeup_reason) {
        case ESP_SLEEP_WAKEUP_EXT0:
            wakeup_reason_str = "EXT0 (Button)";
            break;
        case ESP_SLEEP_WAKEUP_EXT1:
            wakeup_reason_str = "EXT1";
            break;
        case ESP_SLEEP_WAKEUP_TIMER:
            wakeup_reason_str = "Timer";
            break;
        case ESP_SLEEP_WAKEUP_TOUCHPAD:
            wakeup_reason_str = "Touchpad";
            break;
        case ESP_SLEEP_WAKEUP_ULP:
            wakeup_reason_str = "ULP";
            break;
        case ESP_SLEEP_WAKEUP_GPIO:
            wakeup_reason_str = "GPIO";
            break;
        case ESP_SLEEP_WAKEUP_UART:
            wakeup_reason_str = "UART";
            break;
        case ESP_SLEEP_WAKEUP_WIFI:
            wakeup_reason_str = "WiFi";
            break;
        case ESP_SLEEP_WAKEUP_COCPU:
            wakeup_reason_str = "COCPU";
            break;
        case ESP_SLEEP_WAKEUP_COCPU_TRAP_TRIG:
            wakeup_reason_str = "COCPU Trap";
            break;
        case ESP_SLEEP_WAKEUP_BT:
            wakeup_reason_str = "Bluetooth";
            break;
        default:
            wakeup_reason_str = "Power-on or Reset";
            break;
    }
    log_i("Wake-up reason: %s", wakeup_reason_str);

    // Print board info
    log_i("Board: %s", ARDUINO_BOARD);
    log_i("Display: %dx%d", DISP_WIDTH, DISP_HEIGHT);

#if defined(DISP_BW)
    log_i("Display Type: Black & White");
#elif defined(DISP_6C)
    log_i("Display Type: 6-Color");
#elif defined(DISP_7C)
    log_i("Display Type: 7-Color");
#endif

    Serial.println();

    // Show menu
    display_debug::printMenu();
}

void display_debug_loop() {
    display_debug::handleSerialInput();
    delay(100);
}

#endif // ENABLE_DISPLAY_DIAGNOSTIC
