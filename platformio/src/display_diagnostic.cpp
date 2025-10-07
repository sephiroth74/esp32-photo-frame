// Display Diagnostic Tool for ESP32 Photo Frame
// This file helps diagnose display issues like washed-out colors

#include <Arduino.h>
#include "config.h"
#include "renderer.h"

// Test patterns for display diagnostics
namespace display_diagnostic {

/**
 * Measure and report voltage during display operations
 */
void measureVoltage() {
    #ifdef BATTERY_PIN
    // Take multiple readings during display refresh
    float voltage_idle = 0;
    float voltage_refresh = 0;

    // Measure idle voltage
    for(int i = 0; i < 10; i++) {
        voltage_idle += analogRead(BATTERY_PIN);
        delay(10);
    }
    voltage_idle = (voltage_idle / 10.0) * 3.3 / 4095.0 / BATTERY_RESISTORS_RATIO;

    Serial.printf("[DIAG] Idle voltage: %.2fV\n", voltage_idle);

    // Now trigger a display refresh and measure
    Serial.println("[DIAG] Starting display refresh test...");
    display.setFullWindow();
    display.firstPage();

    // Measure during refresh
    float min_voltage = 5.0;
    float max_voltage = 0.0;

    do {
        // Fill with different colors to stress power supply
        display.fillRect(0, 0, DISP_WIDTH/3, DISP_HEIGHT, GxEPD_BLACK);
        display.fillRect(DISP_WIDTH/3, 0, DISP_WIDTH/3, DISP_HEIGHT, GxEPD_RED);
        display.fillRect(2*DISP_WIDTH/3, 0, DISP_WIDTH/3, DISP_HEIGHT, GxEPD_WHITE);

        // Measure voltage during drawing
        float v = analogRead(BATTERY_PIN) * 3.3 / 4095.0 / BATTERY_RESISTORS_RATIO;
        if(v < min_voltage) min_voltage = v;
        if(v > max_voltage) max_voltage = v;
    } while (display.nextPage());

    Serial.printf("[DIAG] Voltage during refresh: Min=%.2fV, Max=%.2fV\n", min_voltage, max_voltage);
    Serial.printf("[DIAG] Voltage drop: %.2fV\n", voltage_idle - min_voltage);

    if(voltage_idle - min_voltage > 0.3) {
        Serial.println("[DIAG] WARNING: Significant voltage drop detected!");
        Serial.println("[DIAG] This indicates power delivery issues.");
    }
    #endif
}

/**
 * Test SPI communication integrity
 */
void testSPICommunication() {
    Serial.println("[DIAG] Testing SPI communication...");

    // Test at different SPI speeds
    uint32_t original_freq = 4000000; // Default 4MHz
    uint32_t test_frequencies[] = {1000000, 2000000, 4000000, 8000000};

    for(int i = 0; i < 4; i++) {
        Serial.printf("[DIAG] Testing at %d Hz...\n", test_frequencies[i]);

        // Note: This would need GxEPD2 library modification to actually change SPI speed
        // For now, just document the test concept

        display.setFullWindow();
        display.firstPage();
        do {
            // Draw test pattern
            for(int y = 0; y < 100; y += 20) {
                display.drawLine(0, y, DISP_WIDTH-1, y, GxEPD_BLACK);
            }
        } while (display.nextPage());

        delay(1000);
    }
}

/**
 * Display color test patterns
 */
void displayColorTestPattern() {
    Serial.println("[DIAG] Displaying color test pattern...");

    display.setFullWindow();
    display.firstPage();

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
        #endif

        // Add text labels
        display.setTextColor(GxEPD_WHITE);
        display.setCursor(10, boxHeight/2);
        display.print("BLACK");

        display.setTextColor(GxEPD_BLACK);
        display.setCursor(boxWidth + 10, boxHeight/2);
        display.print("WHITE");

    } while (display.nextPage());

    Serial.println("[DIAG] Test pattern displayed. Check for:");
    Serial.println("  - Uniform color in each section");
    Serial.println("  - No bleeding between colors");
    Serial.println("  - Correct color representation");
    Serial.println("  - No washed-out areas");
}

/**
 * Test different refresh modes
 */
void testRefreshModes() {
    Serial.println("[DIAG] Testing refresh modes...");

    // Test full refresh
    Serial.println("[DIAG] Full refresh test...");
    display.setFullWindow();
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        display.fillRect(100, 100, 200, 200, GxEPD_RED);
    } while (display.nextPage());

    delay(2000);

    // Test partial refresh (if supported)
    Serial.println("[DIAG] Partial refresh test...");
    display.setPartialWindow(100, 100, 200, 200);
    display.firstPage();
    do {
        display.fillRect(100, 100, 200, 200, GxEPD_BLACK);
    } while (display.nextPage());
}

/**
 * Run all diagnostic tests
 */
void runFullDiagnostic() {
    Serial.println("\n========================================");
    Serial.println("ESP32 Photo Frame - Display Diagnostics");
    Serial.println("========================================\n");

    // Test 1: Voltage measurement
    Serial.println("Test 1: Power Supply Analysis");
    Serial.println("------------------------------");
    measureVoltage();
    delay(2000);

    // Test 2: Color test pattern
    Serial.println("\nTest 2: Color Test Pattern");
    Serial.println("---------------------------");
    displayColorTestPattern();
    delay(5000);

    // Test 3: SPI Communication
    Serial.println("\nTest 3: SPI Communication");
    Serial.println("--------------------------");
    testSPICommunication();
    delay(2000);

    // Test 4: Refresh modes
    Serial.println("\nTest 4: Refresh Modes");
    Serial.println("----------------------");
    testRefreshModes();

    Serial.println("\n========================================");
    Serial.println("Diagnostic Complete!");
    Serial.println("========================================\n");

    Serial.println("Recommendations based on symptoms:");
    Serial.println("1. If voltage drops > 0.3V: Add capacitors near display");
    Serial.println("2. If colors are wrong: Check SPI connections");
    Serial.println("3. If partially washed out: Check ground connections");
    Serial.println("4. If completely washed out: Check power supply current rating");
}

} // namespace display_diagnostic