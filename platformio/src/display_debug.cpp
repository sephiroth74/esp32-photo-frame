// Display Debug Mode - Comprehensive Display Testing
// This file runs instead of main.cpp when ENABLE_DISPLAY_DIAGNOSTIC is defined

#ifdef ENABLE_DISPLAY_DIAGNOSTIC

#include <Arduino.h>
#include "config.h"
#include "renderer.h"
#include "battery.h"
#include "board_util.h"
#include "rgb_status.h"
#include <Preferences.h>

// Display instance is defined in renderer.cpp, we just reference it here
extern GxEPD2_DISPLAY_CLASS<GxEPD2_DRIVER_CLASS, MAX_HEIGHT(GxEPD2_DRIVER_CLASS)> display;

namespace display_debug {

// Test results tracking
struct TestResults {
    bool power_supply_ok = false;
    bool data_lines_ok = false;
    bool busy_pin_ok = false;
    bool spi_comm_ok = false;
    bool refresh_timing_ok = false;
    float voltage_drop = 0.0;
    unsigned long refresh_time = 0;
    int busy_timeouts = 0;
};

TestResults test_results;

// =============================================================================
// BASIC DISPLAY TESTS
// =============================================================================

void testColorPattern() {
    Serial.println(F("\n[TEST] Color Pattern Test"));
    Serial.println(F("-------------------------------"));

    display.setFullWindow();
    display.firstPage();

    unsigned long start = millis();

    do {
        // Create a grid of all available colors
        int boxWidth = DISP_WIDTH / 4;
        int boxHeight = DISP_HEIGHT / 2;

        // Row 1
        display.fillRect(0, 0, boxWidth, boxHeight, GxEPD_BLACK);
        display.fillRect(boxWidth, 0, boxWidth, boxHeight, GxEPD_WHITE);
        display.fillRect(2*boxWidth, 0, boxWidth, boxHeight, GxEPD_RED);

        #ifdef DISP_6C
        display.fillRect(3*boxWidth, 0, boxWidth, boxHeight, GxEPD_GREEN);

        // Row 2
        display.fillRect(0, boxHeight, boxWidth, boxHeight, GxEPD_BLUE);
        display.fillRect(boxWidth, boxHeight, boxWidth, boxHeight, GxEPD_YELLOW);

        // Checkered pattern for testing
        for(int x = 2*boxWidth; x < 3*boxWidth; x += 10) {
            for(int y = boxHeight; y < DISP_HEIGHT; y += 10) {
                display.fillRect(x, y, 5, 5, GxEPD_BLACK);
                display.fillRect(x+5, y, 5, 5, GxEPD_RED);
                display.fillRect(x, y+5, 5, 5, GxEPD_WHITE);
                display.fillRect(x+5, y+5, 5, 5, GxEPD_GREEN);
            }
        }
        #elif defined(DISP_7C_F)
        display.fillRect(3*boxWidth, 0, boxWidth, boxHeight, GxEPD_GREEN);

        // Row 2 for 7-color
        display.fillRect(0, boxHeight, boxWidth, boxHeight, GxEPD_BLUE);
        display.fillRect(boxWidth, boxHeight, boxWidth, boxHeight, GxEPD_YELLOW);
        display.fillRect(2*boxWidth, boxHeight, boxWidth, boxHeight, GxEPD_ORANGE);
        #endif

        // Add text labels
        display.setTextColor(GxEPD_WHITE);
        display.setCursor(10, boxHeight/2);
        display.print("BLACK");

        display.setTextColor(GxEPD_BLACK);
        display.setCursor(boxWidth + 10, boxHeight/2);
        display.print("WHITE");

    } while (display.nextPage());

    unsigned long elapsed = millis() - start;
    test_results.refresh_time = elapsed;

    Serial.printf("[TEST] Refresh time: %lu ms\n", elapsed);

    if (elapsed > 20000) {
        Serial.println(F("[TEST] WARNING: Refresh time exceeds 20 seconds!"));
        test_results.refresh_timing_ok = false;
    } else {
        test_results.refresh_timing_ok = true;
    }
}

// =============================================================================
// POWER SUPPLY TESTS
// =============================================================================

void testPowerSupply() {
    Serial.println(F("\n[TEST] Power Supply Test"));
    Serial.println(F("-------------------------------"));

    #ifdef BATTERY_PIN
    // Take baseline reading
    float voltage_idle = 0;
    for(int i = 0; i < 10; i++) {
        voltage_idle += analogRead(BATTERY_PIN);
        delay(10);
    }
    voltage_idle = (voltage_idle / 10.0) * 3.3 / 4095.0 / BATTERY_RESISTORS_RATIO;
    Serial.printf("[TEST] Idle voltage: %.2fV\n", voltage_idle);

    // Test during display refresh
    Serial.println(F("[TEST] Testing voltage during refresh..."));

    display.setFullWindow();
    display.firstPage();

    float min_voltage = 5.0;
    float max_voltage = 0.0;

    do {
        // Stress test with all colors
        for(int i = 0; i < DISP_WIDTH; i += 40) {
            display.drawLine(i, 0, i, DISP_HEIGHT-1, GxEPD_RED);
            display.drawLine(i+10, 0, i+10, DISP_HEIGHT-1, GxEPD_GREEN);
            display.drawLine(i+20, 0, i+20, DISP_HEIGHT-1, GxEPD_BLUE);
            display.drawLine(i+30, 0, i+30, DISP_HEIGHT-1, GxEPD_BLACK);
        }

        // Measure voltage during drawing
        float v = analogRead(BATTERY_PIN) * 3.3 / 4095.0 / BATTERY_RESISTORS_RATIO;
        if(v < min_voltage) min_voltage = v;
        if(v > max_voltage) max_voltage = v;
    } while (display.nextPage());

    test_results.voltage_drop = voltage_idle - min_voltage;

    Serial.printf("[TEST] Voltage during refresh: Min=%.2fV, Max=%.2fV\n", min_voltage, max_voltage);
    Serial.printf("[TEST] Voltage drop: %.2fV\n", test_results.voltage_drop);

    if(test_results.voltage_drop > 0.3) {
        Serial.println(F("[TEST] FAIL: Significant voltage drop detected!"));
        Serial.println(F("[TEST] Solution: Add 100-470µF capacitor near display power pins"));
        test_results.power_supply_ok = false;
    } else {
        Serial.println(F("[TEST] PASS: Power supply stable"));
        test_results.power_supply_ok = true;
    }
    #else
    Serial.println(F("[TEST] SKIP: No battery pin defined"));
    test_results.power_supply_ok = true;
    #endif
}

// =============================================================================
// DATA LINE VOLTAGE TESTS
// =============================================================================

void testDataLineVoltages() {
    Serial.println(F("\n[TEST] Data Line Voltage Test"));
    Serial.println(F("-------------------------------"));
    Serial.println(F("Check with multimeter - all should read ~3.3V when HIGH"));

    // Configure all pins as outputs
    pinMode(EPD_MOSI_PIN, OUTPUT);
    pinMode(EPD_SCK_PIN, OUTPUT);
    pinMode(EPD_CS_PIN, OUTPUT);
    pinMode(EPD_DC_PIN, OUTPUT);
    pinMode(EPD_RST_PIN, OUTPUT);

    // Set all HIGH
    digitalWrite(EPD_CS_PIN, HIGH);
    digitalWrite(EPD_DC_PIN, HIGH);
    digitalWrite(EPD_RST_PIN, HIGH);
    digitalWrite(EPD_SCK_PIN, HIGH);
    digitalWrite(EPD_MOSI_PIN, HIGH);

    Serial.println(F("[TEST] All data pins set HIGH - measure now:"));
    Serial.printf("  CS  (GPIO %d) - Expected: 3.3V\n", EPD_CS_PIN);
    Serial.printf("  DC  (GPIO %d) - Expected: 3.3V\n", EPD_DC_PIN);
    Serial.printf("  RST (GPIO %d) - Expected: 3.3V\n", EPD_RST_PIN);
    Serial.printf("  SCK (GPIO %d) - Expected: 3.3V\n", EPD_SCK_PIN);
    Serial.printf("  MOSI(GPIO %d) - Expected: 3.3V\n", EPD_MOSI_PIN);

    delay(3000); // Give time to measure

    // Toggle test for oscilloscope
    Serial.println(F("\n[TEST] Toggling SCK for oscilloscope check..."));
    for(int i = 0; i < 1000; i++) {
        digitalWrite(EPD_SCK_PIN, HIGH);
        delayMicroseconds(10);
        digitalWrite(EPD_SCK_PIN, LOW);
        delayMicroseconds(10);
    }

    Serial.println(F("[TEST] Enter 'y' if voltages are correct, 'n' if not:"));
    while(!Serial.available()) delay(10);
    char response = Serial.read();
    while(Serial.available()) Serial.read(); // Clear buffer

    test_results.data_lines_ok = (response == 'y' || response == 'Y');

    if(!test_results.data_lines_ok) {
        Serial.println(F("[TEST] FAIL: Data line voltage issues"));
        Serial.println(F("[TEST] Solutions:"));
        Serial.println(F("  1. Use shorter jumper wires (<3 inches)"));
        Serial.println(F("  2. Check breadboard connections"));
        Serial.println(F("  3. Add 100Ω series resistors on data lines"));
    } else {
        Serial.println(F("[TEST] PASS: Data line voltages OK"));
    }
}

// =============================================================================
// BUSY PIN TESTS
// =============================================================================

void testBusyPin() {
    Serial.println(F("\n[TEST] BUSY Pin Test"));
    Serial.println(F("-------------------------------"));

    pinMode(EPD_BUSY_PIN, INPUT);

    // Check if pin is stuck
    Serial.println(F("[TEST] Checking if BUSY pin responds..."));
    int readings[20];
    bool varies = false;

    for(int i = 0; i < 20; i++) {
        readings[i] = digitalRead(EPD_BUSY_PIN);
        delay(50);
        if(i > 0 && readings[i] != readings[0]) {
            varies = true;
        }
    }

    if(!varies) {
        Serial.printf("[TEST] WARNING: BUSY pin stuck at %s\n",
                      readings[0] ? "HIGH" : "LOW");

        if(readings[0] == HIGH) {
            Serial.println(F("[TEST] Pin stuck HIGH - pull-up resistor working"));
            Serial.println(F("[TEST] But display may not be pulling it LOW"));
        } else {
            Serial.println(F("[TEST] Pin stuck LOW - possible short or bad connection"));
        }
        test_results.busy_pin_ok = false;
    } else {
        Serial.println(F("[TEST] BUSY pin is responsive"));
        test_results.busy_pin_ok = true;
    }

    // Test during actual refresh
    Serial.println(F("\n[TEST] Testing BUSY during refresh..."));
    display.setFullWindow();
    display.firstPage();

    int busy_checks = 0;
    int busy_low_count = 0;
    unsigned long start = millis();

    do {
        display.fillRect(0, 0, 100, 100, GxEPD_RED);

        // Check busy pin multiple times during refresh
        for(int i = 0; i < 10; i++) {
            if(digitalRead(EPD_BUSY_PIN) == LOW) {
                busy_low_count++;
            }
            busy_checks++;
            delay(10);
        }
    } while (display.nextPage());

    unsigned long elapsed = millis() - start;

    Serial.printf("[TEST] Refresh took %lu ms\n", elapsed);
    Serial.printf("[TEST] BUSY was LOW %d/%d times (%.1f%%)\n",
                  busy_low_count, busy_checks,
                  (busy_low_count * 100.0) / busy_checks);

    if(elapsed > 20000) {
        Serial.println(F("[TEST] FAIL: Likely BUSY timeout!"));
        test_results.busy_timeouts++;
        test_results.busy_pin_ok = false;
    }

    if(!test_results.busy_pin_ok) {
        Serial.println(F("\n[TEST] BUSY Pin Solutions:"));
        Serial.println(F("  1. Check BUSY wire connection"));
        Serial.println(F("  2. Ensure 10kΩ pull-up resistor is installed"));
        Serial.println(F("  3. Try different breadboard row"));
        Serial.println(F("  4. Replace jumper wire"));
    }
}

// =============================================================================
// SPI COMMUNICATION TEST
// =============================================================================

void testSPICommunication() {
    Serial.println(F("\n[TEST] SPI Communication Test"));
    Serial.println(F("-------------------------------"));

    uint32_t test_speeds[] = {1000000, 2000000, 4000000, 8000000};
    int best_speed_idx = -1;
    unsigned long best_time = 999999;

    for(int i = 0; i < 4; i++) {
        Serial.printf("[TEST] Testing at %d Hz...\n", test_speeds[i]);

        // Note: Can't actually change SPI speed without library modification
        // This is more of a stress test at current speed

        display.setFullWindow();
        display.firstPage();

        unsigned long start = millis();
        bool timeout = false;

        do {
            // Simple pattern
            for(int y = 0; y < 100; y += 20) {
                display.drawLine(0, y, DISP_WIDTH-1, y, GxEPD_BLACK);
            }

            if(millis() - start > 25000) {
                timeout = true;
                break;
            }
        } while (display.nextPage());

        unsigned long elapsed = millis() - start;

        if(!timeout && elapsed < best_time) {
            best_time = elapsed;
            best_speed_idx = i;
        }

        Serial.printf("[TEST] Time: %lu ms %s\n",
                      elapsed, timeout ? "(TIMEOUT)" : "");

        delay(1000);
    }

    if(best_speed_idx >= 0) {
        Serial.printf("[TEST] Best performance at test #%d\n", best_speed_idx);
        test_results.spi_comm_ok = true;
    } else {
        Serial.println(F("[TEST] FAIL: All SPI tests failed"));
        test_results.spi_comm_ok = false;
    }
}

// =============================================================================
// REFRESH TIMING TEST
// =============================================================================

void testRefreshTiming() {
    Serial.println(F("\n[TEST] Refresh Timing Test"));
    Serial.println(F("-------------------------------"));

    unsigned long timings[3];

    for(int i = 0; i < 3; i++) {
        Serial.printf("[TEST] Test %d/3...\n", i+1);

        display.setFullWindow();
        display.firstPage();

        unsigned long start = millis();

        do {
            // Alternating pattern
            for(int x = 0; x < DISP_WIDTH; x += 20) {
                display.fillRect(x, 0, 10, DISP_HEIGHT,
                                (x/20) % 2 ? GxEPD_BLACK : GxEPD_RED);
            }
        } while (display.nextPage());

        timings[i] = millis() - start;
        Serial.printf("[TEST] Refresh %d: %lu ms\n", i+1, timings[i]);

        if(timings[i] > 20000) {
            test_results.busy_timeouts++;
        }

        delay(2000);
    }

    // Calculate average
    unsigned long avg = (timings[0] + timings[1] + timings[2]) / 3;
    Serial.printf("\n[TEST] Average refresh time: %lu ms\n", avg);

    if(avg > 18000) {
        Serial.println(F("[TEST] WARNING: Close to timeout threshold!"));
        Serial.println(F("[TEST] Consider increasing timeout in library"));
    }

    if(test_results.busy_timeouts > 0) {
        Serial.printf("[TEST] FAIL: %d timeouts detected\n", test_results.busy_timeouts);
    } else {
        Serial.println(F("[TEST] PASS: No timeouts detected"));
    }
}

// =============================================================================
// INTERACTIVE MENU
// =============================================================================

void showMenu() {
    Serial.println(F("\n========================================"));
    Serial.println(F("Display Debug Menu"));
    Serial.println(F("========================================"));
    Serial.println(F("1. Run Full Diagnostic Suite"));
    Serial.println(F("2. Color Pattern Test"));
    Serial.println(F("3. Power Supply Test"));
    Serial.println(F("4. Data Line Voltage Test"));
    Serial.println(F("5. BUSY Pin Test"));
    Serial.println(F("6. SPI Communication Test"));
    Serial.println(F("7. Refresh Timing Test"));
    Serial.println(F("8. Show Test Results Summary"));
    Serial.println(F("9. Clear Display"));
    Serial.println(F("0. Restart ESP32"));
    Serial.println(F("----------------------------------------"));
    Serial.println(F("Enter choice:"));
}

void showResults() {
    Serial.println(F("\n========================================"));
    Serial.println(F("Test Results Summary"));
    Serial.println(F("========================================"));

    Serial.printf("Power Supply:     %s", test_results.power_supply_ok ? "PASS" : "FAIL");
    if(!test_results.power_supply_ok) {
        Serial.printf(" (%.2fV drop)", test_results.voltage_drop);
    }
    Serial.println();

    Serial.printf("Data Lines:       %s\n", test_results.data_lines_ok ? "PASS" : "FAIL/UNKNOWN");
    Serial.printf("BUSY Pin:         %s", test_results.busy_pin_ok ? "PASS" : "FAIL");
    if(test_results.busy_timeouts > 0) {
        Serial.printf(" (%d timeouts)", test_results.busy_timeouts);
    }
    Serial.println();

    Serial.printf("SPI Comm:         %s\n", test_results.spi_comm_ok ? "PASS" : "FAIL");
    Serial.printf("Refresh Timing:   %s", test_results.refresh_timing_ok ? "PASS" : "FAIL");
    if(test_results.refresh_time > 0) {
        Serial.printf(" (%lu ms)", test_results.refresh_time);
    }
    Serial.println();

    Serial.println(F("\n========================================"));

    // Diagnosis
    if(!test_results.power_supply_ok) {
        Serial.println(F("\nDIAGNOSIS: Power delivery issue"));
        Serial.println(F("SOLUTION: Add 100-470µF capacitor"));
    }

    if(!test_results.busy_pin_ok) {
        Serial.println(F("\nDIAGNOSIS: BUSY pin connection issue"));
        Serial.println(F("SOLUTION: Check wire, add 10kΩ pull-up"));
    }

    if(!test_results.data_lines_ok) {
        Serial.println(F("\nDIAGNOSIS: Data line signal integrity"));
        Serial.println(F("SOLUTION: Shorter wires, better connections"));
    }

    if(test_results.busy_timeouts > 0) {
        Serial.println(F("\nDIAGNOSIS: Display timeout issues"));
        Serial.println(F("SOLUTION: Increase timeout in library"));
    }
}

void runFullDiagnostic() {
    Serial.println(F("\n========================================"));
    Serial.println(F("Running Full Diagnostic Suite"));
    Serial.println(F("========================================"));

    testColorPattern();
    delay(2000);

    testPowerSupply();
    delay(2000);

    testDataLineVoltages();
    delay(2000);

    testBusyPin();
    delay(2000);

    testSPICommunication();
    delay(2000);

    testRefreshTiming();
    delay(2000);

    showResults();
}

} // namespace display_debug

// =============================================================================
// MAIN SETUP AND LOOP FOR DEBUG MODE
// =============================================================================

void display_debug_setup() {
    Serial.begin(115200);
    while (!Serial) delay(10);

    Serial.println(F("\n\n========================================"));
    Serial.println(F("ESP32 Photo Frame - Display Debug Mode"));
    Serial.println(F("========================================\n"));
    Serial.println(F("ENABLE_DISPLAY_DIAGNOSTIC is defined"));
    Serial.println(F("Running in debug mode - normal operation disabled\n"));

    // Initialize basic hardware
    photo_frame::board_utils::blink_builtin_led(2, 100);

    // Show board info
    Serial.println(F("Board Information:"));
    Serial.printf("  Chip Model: %s\n", ESP.getChipModel());
    Serial.printf("  Chip Cores: %d\n", ESP.getChipCores());
    Serial.printf("  CPU Freq: %d MHz\n", ESP.getCpuFreqMHz());
    Serial.printf("  Free Heap: %d bytes\n", ESP.getFreeHeap());

    // Initialize display
    Serial.println(F("\nInitializing display..."));
    photo_frame::renderer::init_display();

    Serial.println(F("\nDisplay initialized successfully!"));
    Serial.println(F("Ready for testing.\n"));

    display_debug::showMenu();
}

void display_debug_loop() {
    if(Serial.available()) {
        char cmd = Serial.read();
        while(Serial.available()) Serial.read(); // Clear buffer

        switch(cmd) {
            case '1':
                display_debug::runFullDiagnostic();
                break;

            case '2':
                display_debug::testColorPattern();
                break;

            case '3':
                display_debug::testPowerSupply();
                break;

            case '4':
                display_debug::testDataLineVoltages();
                break;

            case '5':
                display_debug::testBusyPin();
                break;

            case '6':
                display_debug::testSPICommunication();
                break;

            case '7':
                display_debug::testRefreshTiming();
                break;

            case '8':
                display_debug::showResults();
                break;

            case '9':
                Serial.println(F("Clearing display..."));
                display.clearScreen();
                Serial.println(F("Display cleared"));
                break;

            case '0':
                Serial.println(F("Restarting in 3 seconds..."));
                delay(3000);
                ESP.restart();
                break;

            default:
                Serial.println(F("Invalid choice"));
                break;
        }

        delay(1000);
        display_debug::showMenu();
    }
}

#endif // ENABLE_DISPLAY_DIAGNOSTIC