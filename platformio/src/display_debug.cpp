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

static bool displayInitialized = false;
static bool batteryInitialized = false;

void ensureDisplayInitialized() {
    if (!displayInitialized) {
        photo_frame::renderer::init_display();
        displayInitialized = true;
        Serial.println(F("[DEBUG] Display initialized"));
    }
}

void ensureBatteryInitialized() {
    if (!batteryInitialized) {
        battery_reader.init();
        batteryInitialized = true;
        Serial.println(F("[DEBUG] Battery initialized"));
    }
}

// ============================================================================
// Test 1: Battery Status Test
// ============================================================================
void testBatteryStatus() {
    Serial.println(F("\n========================================"));
    Serial.println(F("[TEST] Battery Status"));
    Serial.println(F("========================================"));

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
    Serial.println(F("\n========================================"));
    Serial.println(F("[TEST] LED Test"));
    Serial.println(F("========================================"));

#ifdef RGB_STATUS_ENABLED
    Serial.println(F("\nTesting RGB LED..."));
    Serial.println(F("This will cycle through colors"));

    // Initialize RGB LED
    if (!rgbStatus.begin()) {
        Serial.println(F("ERROR: Failed to initialize RGB LED"));
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
    Serial.println(F("\n========================================"));
    Serial.println(F("[TEST] Potentiometer Test"));
    Serial.println(F("========================================"));

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
// Test 4: Deep Sleep Test
// ============================================================================
void testDeepSleep() {
    Serial.println(F("\n========================================"));
    Serial.println(F("[TEST] Deep Sleep Test"));
    Serial.println(F("========================================"));

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
    Serial.println(F("\n========================================"));
    Serial.println(F("[RESTART] Restarting board..."));
    Serial.println(F("========================================"));
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
    Serial.println(F("4 - Deep Sleep Test"));
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
    delay(1000); // Give time to open serial monitor

    Serial.println(F("\n\n========================================"));
    Serial.println(F("Display Debug Mode - STARTED"));
    Serial.println(F("========================================"));

    // Check wake-up reason
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    Serial.print(F("Wake-up reason: "));
    switch (wakeup_reason) {
        case ESP_SLEEP_WAKEUP_EXT0:
            Serial.println(F("EXT0 (Button)"));
            break;
        case ESP_SLEEP_WAKEUP_EXT1:
            Serial.println(F("EXT1"));
            break;
        case ESP_SLEEP_WAKEUP_TIMER:
            Serial.println(F("Timer"));
            break;
        case ESP_SLEEP_WAKEUP_TOUCHPAD:
            Serial.println(F("Touchpad"));
            break;
        case ESP_SLEEP_WAKEUP_ULP:
            Serial.println(F("ULP"));
            break;
        case ESP_SLEEP_WAKEUP_GPIO:
            Serial.println(F("GPIO"));
            break;
        case ESP_SLEEP_WAKEUP_UART:
            Serial.println(F("UART"));
            break;
        case ESP_SLEEP_WAKEUP_WIFI:
            Serial.println(F("WiFi"));
            break;
        case ESP_SLEEP_WAKEUP_COCPU:
            Serial.println(F("COCPU"));
            break;
        case ESP_SLEEP_WAKEUP_COCPU_TRAP_TRIG:
            Serial.println(F("COCPU Trap"));
            break;
        case ESP_SLEEP_WAKEUP_BT:
            Serial.println(F("Bluetooth"));
            break;
        default:
            Serial.println(F("Power-on or Reset"));
            break;
    }

    // Print board info
    Serial.print(F("Board: "));
    Serial.println(ARDUINO_BOARD);
    Serial.print(F("Display: "));
    Serial.print(DISP_WIDTH);
    Serial.print(F("x"));
    Serial.println(DISP_HEIGHT);

#if defined(DISP_BW)
    Serial.println(F("Display Type: Black & White"));
#elif defined(DISP_6C)
    Serial.println(F("Display Type: 6-Color"));
#elif defined(DISP_7C)
    Serial.println(F("Display Type: 7-Color"));
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
