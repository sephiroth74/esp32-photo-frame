// Display Debug Mode - Comprehensive Display Testing
// This file runs instead of main.cpp when ENABLE_DISPLAY_DIAGNOSTIC is defined

#ifdef ENABLE_DISPLAY_DIAGNOSTIC

#include <Arduino.h>
#include "config.h"
#include "renderer.h"
#include "battery.h"
#include "board_util.h"
#include "rgb_status.h"
#include "io_utils.h"
#include "spi_manager.h"
#include "sd_card.h"
#include "errors.h"
#include "weather.h"
#include "google_drive.h"
#include <RTClib.h>
#include <Preferences.h>
#include <SPI.h>
#include <SD_MMC.h>
#include <vector>

// Display instance is defined in renderer.cpp, we just reference it here
extern GxEPD2_DISPLAY_CLASS<GxEPD2_DRIVER_CLASS, MAX_HEIGHT(GxEPD2_DRIVER_CLASS)> display;

extern photo_frame::sd_card sdCard;

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

// Helper function for Floyd-Steinberg dithering
void applyFloydSteinbergDithering(int x, int y, int width, int height, uint8_t grayLevel)
{
    // Create error buffer for dithering
    int16_t* errorBuffer = new int16_t[width * height];
    memset(errorBuffer, 0, width * height * sizeof(int16_t));

    // Fill with initial gray level (0-255)
    for (int i = 0; i < width * height; i++) {
        errorBuffer[i] = grayLevel;
    }

    // Apply Floyd-Steinberg dithering
    for (int row = 0; row < height; row++) {
        // Yield every 10 rows to prevent watchdog timeout
        if (row % 10 == 0) {
            yield();
        }

        for (int col = 0; col < width; col++) {
            int idx = row * width + col;
            int oldPixel = errorBuffer[idx];
            int newPixel = (oldPixel > 127) ? 255 : 0;
            int error = oldPixel - newPixel;

            // Distribute error to neighboring pixels
            if (col + 1 < width) {
                errorBuffer[idx + 1] += (error * 7) / 16;
            }
            if (row + 1 < height) {
                if (col > 0) {
                    errorBuffer[idx + width - 1] += (error * 3) / 16;
                }
                errorBuffer[idx + width] += (error * 5) / 16;
                if (col + 1 < width) {
                    errorBuffer[idx + width + 1] += (error * 1) / 16;
                }
            }

            // Draw the dithered pixel
            display.drawPixel(x + col, y + row, newPixel > 0 ? GxEPD_WHITE : GxEPD_BLACK);
        }
    }

    delete[] errorBuffer;
}

void testColorPattern()
{
    Serial.println(F("\n[TEST] Color Pattern Test"));
    Serial.println(F("-------------------------------"));

    display.setFullWindow();
    display.firstPage();

    unsigned long start = millis();

    do {
#if defined(DISP_BW_V2) || (!defined(DISP_6C) && !defined(DISP_7C))
        // For B&W displays: Show grayscale patterns using Floyd-Steinberg dithering
        Serial.println(F("[TEST] B&W Display: Showing dithered grayscale patterns"));

        // Create 8 grayscale levels (0%, 14%, 28%, 42%, 57%, 71%, 85%, 100%)
        int boxWidth = DISP_WIDTH / 4;
        int boxHeight = DISP_HEIGHT / 2;

        // Top row: 0%, 25%, 50%, 75%
        applyFloydSteinbergDithering(0, 0, boxWidth, boxHeight, 0); // 0% (Black)
        applyFloydSteinbergDithering(boxWidth, 0, boxWidth, boxHeight, 64); // 25%
        applyFloydSteinbergDithering(2 * boxWidth, 0, boxWidth, boxHeight, 128); // 50%
        applyFloydSteinbergDithering(3 * boxWidth, 0, boxWidth, boxHeight, 192); // 75%

        // Bottom row: 100%, checkerboard, horizontal lines, vertical lines
        applyFloydSteinbergDithering(0, boxHeight, boxWidth, boxHeight, 255); // 100% (White)

        // Checkerboard pattern
        for (int x = boxWidth; x < 2 * boxWidth; x += 4) {
            for (int y = boxHeight; y < DISP_HEIGHT; y += 4) {
                display.fillRect(x, y, 2, 2, GxEPD_BLACK);
                display.fillRect(x + 2, y, 2, 2, GxEPD_WHITE);
                display.fillRect(x, y + 2, 2, 2, GxEPD_WHITE);
                display.fillRect(x + 2, y + 2, 2, 2, GxEPD_BLACK);
            }
        }

        // Horizontal lines pattern
        for (int y = boxHeight; y < DISP_HEIGHT; y += 4) {
            display.drawFastHLine(2 * boxWidth, y, boxWidth, GxEPD_BLACK);
            display.drawFastHLine(2 * boxWidth, y + 1, boxWidth, GxEPD_WHITE);
            display.drawFastHLine(2 * boxWidth, y + 2, boxWidth, GxEPD_WHITE);
            display.drawFastHLine(2 * boxWidth, y + 3, boxWidth, GxEPD_WHITE);
        }

        // Vertical lines pattern
        for (int x = 3 * boxWidth; x < DISP_WIDTH; x += 4) {
            display.drawFastVLine(x, boxHeight, boxHeight, GxEPD_BLACK);
            display.drawFastVLine(x + 1, boxHeight, boxHeight, GxEPD_WHITE);
            display.drawFastVLine(x + 2, boxHeight, boxHeight, GxEPD_WHITE);
            display.drawFastVLine(x + 3, boxHeight, boxHeight, GxEPD_WHITE);
        }

        // Add text labels
        display.setTextColor(GxEPD_WHITE);
        display.setCursor(10, boxHeight / 2);
        display.print("0%");

        display.setTextColor(GxEPD_BLACK);
        display.setCursor(boxWidth + 10, boxHeight / 2);
        display.print("25%");

        display.setTextColor(GxEPD_BLACK);
        display.setCursor(2 * boxWidth + 10, boxHeight / 2);
        display.print("50%");

        display.setTextColor(GxEPD_BLACK);
        display.setCursor(3 * boxWidth + 10, boxHeight / 2);
        display.print("75%");

        display.setTextColor(GxEPD_BLACK);
        display.setCursor(10, boxHeight + boxHeight / 2);
        display.print("100%");

#else
        // Original code for color displays
        // Create a grid of all available colors
        int boxWidth = DISP_WIDTH / 4;
        int boxHeight = DISP_HEIGHT / 2;

        // Row 1
        display.fillRect(0, 0, boxWidth, boxHeight, GxEPD_BLACK);
        display.fillRect(boxWidth, 0, boxWidth, boxHeight, GxEPD_WHITE);
        display.fillRect(2 * boxWidth, 0, boxWidth, boxHeight, GxEPD_RED);

#ifdef DISP_6C
        display.fillRect(3 * boxWidth, 0, boxWidth, boxHeight, GxEPD_GREEN);

        // Row 2
        display.fillRect(0, boxHeight, boxWidth, boxHeight, GxEPD_BLUE);
        display.fillRect(boxWidth, boxHeight, boxWidth, boxHeight, GxEPD_YELLOW);

        // Checkered pattern for testing
        for (int x = 2 * boxWidth; x < 3 * boxWidth; x += 10) {
            for (int y = boxHeight; y < DISP_HEIGHT; y += 10) {
                display.fillRect(x, y, 5, 5, GxEPD_BLACK);
                display.fillRect(x + 5, y, 5, 5, GxEPD_RED);
                display.fillRect(x, y + 5, 5, 5, GxEPD_WHITE);
                display.fillRect(x + 5, y + 5, 5, 5, GxEPD_GREEN);
            }
        }
#elif defined(DISP_7C)
        display.fillRect(3 * boxWidth, 0, boxWidth, boxHeight, GxEPD_GREEN);

        // Row 2 for 7-color
        display.fillRect(0, boxHeight, boxWidth, boxHeight, GxEPD_BLUE);
        display.fillRect(boxWidth, boxHeight, boxWidth, boxHeight, GxEPD_YELLOW);
        display.fillRect(2 * boxWidth, boxHeight, boxWidth, boxHeight, GxEPD_ORANGE);
#endif

        // Add text labels
        display.setTextColor(GxEPD_WHITE);
        display.setCursor(10, boxHeight / 2);
        display.print("BLACK");

        display.setTextColor(GxEPD_BLACK);
        display.setCursor(boxWidth + 10, boxHeight / 2);
        display.print("WHITE");
#endif

    } while (display.nextPage());

    unsigned long elapsed = millis() - start;
    test_results.refresh_time = elapsed;

    Serial.printf("[TEST] Refresh time: %lu ms\n", elapsed);

    if (elapsed > 30000) {
        Serial.println(F("[TEST] WARNING: Refresh time exceeds 30 seconds!"));
        test_results.refresh_timing_ok = false;
    } else {
        test_results.refresh_timing_ok = true;
    }
}

// =============================================================================
// COLOR BARS TEST - Display all available colors as vertical bars
// =============================================================================

void testColorBars()
{
    Serial.println(F("\n[TEST] Color Bars Test"));
    Serial.println(F("-------------------------------"));

#ifdef DISP_6C
    Serial.println(F("Drawing 6 vertical bars with all available colors"));
    int numColors = 6;
#elif defined(DISP_7C)
    Serial.println(F("Drawing 7 vertical bars with all available colors"));
    int numColors = 7;
#else
    Serial.println(F("Drawing 2 bars (Black and White)"));
    int numColors = 2;
#endif

    display.setFullWindow();
    display.firstPage();

    unsigned long start = millis();

    do {
        // Fill background with white first
        display.fillScreen(GxEPD_WHITE);

        // Calculate bar width based on number of colors
        int barWidth = DISP_WIDTH / numColors;
        int barHeight = DISP_HEIGHT;
        int lastBarWidth = DISP_WIDTH - ((numColors - 1) * barWidth); // Handle any remainder pixels

#ifdef DISP_6C
        // 6-color display: Black, White, Green, Blue, Red, Yellow (NO Orange)
        display.fillRect(0 * barWidth, 0, barWidth, barHeight, GxEPD_BLACK);
        display.fillRect(1 * barWidth, 0, barWidth, barHeight, GxEPD_WHITE);
        display.fillRect(2 * barWidth, 0, barWidth, barHeight, GxEPD_GREEN);
        display.fillRect(3 * barWidth, 0, barWidth, barHeight, GxEPD_BLUE);
        display.fillRect(4 * barWidth, 0, barWidth, barHeight, GxEPD_RED);
        display.fillRect(5 * barWidth, 0, lastBarWidth, barHeight, GxEPD_YELLOW);
#elif defined(DISP_7C)
        // 7-color display: Black, White, Green, Blue, Red, Yellow, Orange
        display.fillRect(0 * barWidth, 0, barWidth, barHeight, GxEPD_BLACK);
        display.fillRect(1 * barWidth, 0, barWidth, barHeight, GxEPD_WHITE);
        display.fillRect(2 * barWidth, 0, barWidth, barHeight, GxEPD_GREEN);
        display.fillRect(3 * barWidth, 0, barWidth, barHeight, GxEPD_BLUE);
        display.fillRect(4 * barWidth, 0, barWidth, barHeight, GxEPD_RED);
        display.fillRect(5 * barWidth, 0, barWidth, barHeight, GxEPD_YELLOW);
        display.fillRect(6 * barWidth, 0, lastBarWidth, barHeight, GxEPD_ORANGE);
#else
        // B/W display: Black and White only
        display.fillRect(0 * barWidth, 0, barWidth, barHeight, GxEPD_BLACK);
        display.fillRect(1 * barWidth, 0, lastBarWidth, barHeight, GxEPD_WHITE);
#endif

        // Add text labels for each color
        display.setTextSize(2);
        int textY = barHeight / 2 - 8; // Center vertically

        // Black bar - white text
        display.setTextColor(GxEPD_WHITE);
        display.setCursor((0 * barWidth) + 10, textY);
        display.print("BLK");

        // White bar - black text
        display.setTextColor(GxEPD_BLACK);
        display.setCursor((1 * barWidth) + 10, textY);
        display.print("WHT");

#ifdef DISP_6C
        // Green bar - black text
        display.setTextColor(GxEPD_BLACK);
        display.setCursor((2 * barWidth) + 10, textY);
        display.print("GRN");

        // Blue bar - white text
        display.setTextColor(GxEPD_WHITE);
        display.setCursor((3 * barWidth) + 10, textY);
        display.print("BLU");

        // Red bar - white text
        display.setTextColor(GxEPD_WHITE);
        display.setCursor((4 * barWidth) + 10, textY);
        display.print("RED");

        // Yellow bar - black text
        display.setTextColor(GxEPD_BLACK);
        display.setCursor((5 * barWidth) + 10, textY);
        display.print("YEL");

#elif defined(DISP_7C)
        // Green bar - black text
        display.setTextColor(GxEPD_BLACK);
        display.setCursor((2 * barWidth) + 10, textY);
        display.print("GRN");

        // Blue bar - white text
        display.setTextColor(GxEPD_WHITE);
        display.setCursor((3 * barWidth) + 10, textY);
        display.print("BLU");

        // Red bar - white text
        display.setTextColor(GxEPD_WHITE);
        display.setCursor((4 * barWidth) + 10, textY);
        display.print("RED");

        // Yellow bar - black text
        display.setTextColor(GxEPD_BLACK);
        display.setCursor((5 * barWidth) + 10, textY);
        display.print("YEL");

        // Orange bar - black text
        display.setTextColor(GxEPD_BLACK);
        display.setCursor((6 * barWidth) + 10, textY);
        display.print("ORG");
#endif

    } while (display.nextPage());

    unsigned long elapsed = millis() - start;

    Serial.printf("[TEST] Color bars refresh time: %lu ms\n", elapsed);
    Serial.println(F("[TEST] Color bars complete"));

#ifdef DISP_6C
    Serial.println(F("[TEST] All 6 colors displayed: Black, White, Green, Blue, Red, Yellow"));
#elif defined(DISP_7C)
    Serial.println(F("[TEST] All 7 colors displayed: Black, White, Green, Blue, Red, Yellow, Orange"));
#else
    Serial.println(F("[TEST] Black and White bars displayed"));
#endif

    Serial.println(F("[TEST] Check display for:"));
    Serial.println(F("  - Each color should be distinct and clear"));
    Serial.println(F("  - No color bleeding between bars"));
    Serial.println(F("  - Uniform color within each bar"));
    Serial.println(F("  - No washout or fading"));
}

// =============================================================================
// HIGH STRESS COLOR TEST
// =============================================================================

void testHighStressColor()
{
    Serial.println(F("\n[TEST] High Stress Color Test"));
    Serial.println(F("-------------------------------"));
    Serial.println(F("This test pushes the display to its limits"));
    Serial.println(F("Expect refresh time of 17-30 seconds for 6-color displays"));

    unsigned long start = millis();
    bool timeout_detected = false;

    display.setFullWindow();
    display.firstPage();

    do {
        // Clear screen for this page
        display.fillScreen(GxEPD_WHITE);

        // Create maximum color transitions to stress the display
        // This pattern requires the display to switch colors frequently
        // which takes more time and power

#ifdef DISP_6C
        // For 6-color displays - create a complex pattern using all colors

        // Create vertical stripes with all colors (maximum transitions)
        int stripeWidth = 4; // Narrow stripes = more transitions = more stress
        for (int x = 0; x < DISP_WIDTH; x += stripeWidth * 6) {
            // Use all 6 colors in sequence
            display.fillRect(x, 0, stripeWidth, DISP_HEIGHT, GxEPD_BLACK);
            display.fillRect(x + stripeWidth, 0, stripeWidth, DISP_HEIGHT, GxEPD_WHITE);
            display.fillRect(x + stripeWidth * 2, 0, stripeWidth, DISP_HEIGHT, GxEPD_RED);
            display.fillRect(x + stripeWidth * 3, 0, stripeWidth, DISP_HEIGHT, GxEPD_GREEN);
            display.fillRect(x + stripeWidth * 4, 0, stripeWidth, DISP_HEIGHT, GxEPD_BLUE);
            display.fillRect(x + stripeWidth * 5, 0, stripeWidth, DISP_HEIGHT, GxEPD_YELLOW);
        }

        // Add horizontal stripes overlaying for maximum complexity
        for (int y = 0; y < DISP_HEIGHT; y += 40) {
            // Alternating horizontal lines with different colors
            display.drawLine(0, y, DISP_WIDTH - 1, y, GxEPD_RED);
            display.drawLine(0, y + 10, DISP_WIDTH - 1, y + 10, GxEPD_BLACK);
            display.drawLine(0, y + 20, DISP_WIDTH - 1, y + 20, GxEPD_GREEN);
            display.drawLine(0, y + 30, DISP_WIDTH - 1, y + 30, GxEPD_BLUE);
        }

        // Add diagonal patterns for extra complexity
        for (int i = 0; i < DISP_WIDTH; i += 20) {
            display.drawLine(i, 0, 0, i * DISP_HEIGHT / DISP_WIDTH, GxEPD_YELLOW);
            display.drawLine(DISP_WIDTH - i, 0, DISP_WIDTH, i * DISP_HEIGHT / DISP_WIDTH, GxEPD_RED);
        }

        // Add scattered pixels of different colors (most stressful)
        for (int i = 0; i < 100; i++) {
            int x = random(DISP_WIDTH);
            int y = random(DISP_HEIGHT);
            uint16_t color = random(6);
            switch (color) {
            case 0:
                display.drawPixel(x, y, GxEPD_BLACK);
                break;
            case 1:
                display.drawPixel(x, y, GxEPD_WHITE);
                break;
            case 2:
                display.drawPixel(x, y, GxEPD_RED);
                break;
            case 3:
                display.drawPixel(x, y, GxEPD_GREEN);
                break;
            case 4:
                display.drawPixel(x, y, GxEPD_BLUE);
                break;
            case 5:
                display.drawPixel(x, y, GxEPD_YELLOW);
                break;
            }
        }

#elif defined(DISP_7C)
        // For 7-color displays - use all 7 colors
        int stripeWidth = 4;
        for (int x = 0; x < DISP_WIDTH; x += stripeWidth * 7) {
            display.fillRect(x, 0, stripeWidth, DISP_HEIGHT, GxEPD_BLACK);
            display.fillRect(x + stripeWidth, 0, stripeWidth, DISP_HEIGHT, GxEPD_WHITE);
            display.fillRect(x + stripeWidth * 2, 0, stripeWidth, DISP_HEIGHT, GxEPD_RED);
            display.fillRect(x + stripeWidth * 3, 0, stripeWidth, DISP_HEIGHT, GxEPD_GREEN);
            display.fillRect(x + stripeWidth * 4, 0, stripeWidth, DISP_HEIGHT, GxEPD_BLUE);
            display.fillRect(x + stripeWidth * 5, 0, stripeWidth, DISP_HEIGHT, GxEPD_YELLOW);
            display.fillRect(x + stripeWidth * 6, 0, stripeWidth, DISP_HEIGHT, GxEPD_ORANGE);
        }
#else
        // For B&W displays - create complex pattern to stress the display
        Serial.println(F("[TEST] Creating B&W stress pattern..."));

        // Create alternating vertical stripes (more transitions = more stress)
        int stripeWidth = 2;  // Very narrow stripes for maximum transitions
        for (int x = 0; x < DISP_WIDTH; x += stripeWidth * 2) {
            display.fillRect(x, 0, stripeWidth, DISP_HEIGHT, GxEPD_BLACK);
            display.fillRect(x + stripeWidth, 0, stripeWidth, DISP_HEIGHT, GxEPD_WHITE);
        }

        // Add horizontal lines for complexity
        for (int y = 0; y < DISP_HEIGHT; y += 20) {
            display.drawFastHLine(0, y, DISP_WIDTH, GxEPD_BLACK);
            display.drawFastHLine(0, y + 1, DISP_WIDTH, GxEPD_WHITE);
        }

        // Add diagonal lines
        for (int i = 0; i < DISP_WIDTH; i += 40) {
            display.drawLine(i, 0, 0, i * DISP_HEIGHT / DISP_WIDTH, GxEPD_BLACK);
            display.drawLine(DISP_WIDTH - i, 0, DISP_WIDTH, i * DISP_HEIGHT / DISP_WIDTH, GxEPD_WHITE);
        }

        // Add some filled shapes for variety
        for (int i = 0; i < 5; i++) {
            int x = (i * DISP_WIDTH) / 5 + 20;
            int y = DISP_HEIGHT / 2;
            display.fillCircle(x, y, 15, (i % 2) ? GxEPD_BLACK : GxEPD_WHITE);
            display.drawCircle(x, y, 20, (i % 2) ? GxEPD_WHITE : GxEPD_BLACK);
        }

        // Add checkerboard pattern in corners (most stressful pattern)
        int checkSize = 1;  // 1x1 pixel checkerboard is most stressful
        // Top-left corner
        for (int y = 0; y < 50; y++) {
            for (int x = 0; x < 50; x++) {
                if ((x / checkSize + y / checkSize) % 2 == 0) {
                    display.drawPixel(x, y, GxEPD_BLACK);
                }
            }
        }
        // Bottom-right corner
        for (int y = DISP_HEIGHT - 50; y < DISP_HEIGHT; y++) {
            for (int x = DISP_WIDTH - 50; x < DISP_WIDTH; x++) {
                if ((x / checkSize + y / checkSize) % 2 == 0) {
                    display.drawPixel(x, y, GxEPD_BLACK);
                }
            }
        }

        // Add text to show it's working
        display.setTextColor(GxEPD_BLACK);
        display.setTextSize(2);
        display.setCursor(DISP_WIDTH / 2 - 80, 10);
        display.print("B&W STRESS");
#endif

        // Check for timeout
        if (millis() - start > 25000) {
            timeout_detected = true;
            Serial.println(F("[TEST] WARNING: Approaching timeout threshold!"));
        }

    } while (display.nextPage());

    unsigned long elapsed = millis() - start;

    Serial.printf("[TEST] High stress refresh time: %lu ms\n", elapsed);

    // Detailed analysis
    if (elapsed < 15000) {
        Serial.println(F("[TEST] EXCELLENT: Fast refresh even under stress"));
    } else if (elapsed < 18000) {
        Serial.println(F("[TEST] GOOD: Normal refresh time under stress"));
    } else if (elapsed < 30000) {
        Serial.println(F("[TEST] WARNING: Close to timeout limit"));
        Serial.println(F("[TEST] Consider increasing library timeout"));
    } else if (elapsed < 32000) {
        Serial.println(F("[TEST] CRITICAL: Very close to timeout!"));
        Serial.println(F("[TEST] Timeout increase recommended"));
    } else {
        Serial.println(F("[TEST] FAIL: Likely hit timeout!"));
        Serial.println(F("[TEST] Check BUSY pin and increase timeout"));
        test_results.busy_timeouts++;
    }

    // Check for timeout symptoms
    if (timeout_detected || elapsed > 30000) {
        Serial.println(F("\n[TEST] Timeout Risk Assessment:"));
        Serial.println(F("  - Simple images may work fine"));
        Serial.println(F("  - Complex/portrait images may timeout"));
        Serial.println(F("  - Washout likely on complex patterns"));
        Serial.println(F("\n[TEST] Recommended Actions:"));
        Serial.println(F("  1. Increase timeout to 30 seconds"));
        Serial.println(F("  2. Ensure BUSY pin has 10kΩ pull-up"));
        Serial.println(F("  3. Check all connections"));
    }

    // Update test results
    if (elapsed > 30000) {
        test_results.refresh_timing_ok = false;
    }
}

// =============================================================================
// POWER SUPPLY TESTS
// =============================================================================

void testPowerSupply()
{
    Serial.println(F("\n[TEST] Power Supply Test"));
    Serial.println(F("-------------------------------"));

#ifdef BATTERY_PIN
    // Take baseline reading
    float voltage_idle = 0;
    for (int i = 0; i < 10; i++) {
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
        for (int i = 0; i < DISP_WIDTH; i += 40) {
            display.drawLine(i, 0, i, DISP_HEIGHT - 1, GxEPD_RED);
            display.drawLine(i + 10, 0, i + 10, DISP_HEIGHT - 1, GxEPD_GREEN);
            display.drawLine(i + 20, 0, i + 20, DISP_HEIGHT - 1, GxEPD_BLUE);
            display.drawLine(i + 30, 0, i + 30, DISP_HEIGHT - 1, GxEPD_BLACK);
        }

        // Measure voltage during drawing
        float v = analogRead(BATTERY_PIN) * 3.3 / 4095.0 / BATTERY_RESISTORS_RATIO;
        if (v < min_voltage)
            min_voltage = v;
        if (v > max_voltage)
            max_voltage = v;
    } while (display.nextPage());

    test_results.voltage_drop = voltage_idle - min_voltage;

    Serial.printf("[TEST] Voltage during refresh: Min=%.2fV, Max=%.2fV\n", min_voltage, max_voltage);
    Serial.printf("[TEST] Voltage drop: %.2fV\n", test_results.voltage_drop);

    if (test_results.voltage_drop > 0.3) {
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

void testDataLineVoltages()
{
    Serial.println(F("\n[TEST] Data Line Voltage Test"));
    Serial.println(F("-------------------------------"));

    Serial.println(F("Testing each pin individually - measure with multimeter"));
    Serial.println(F("Press any key to test next pin, or 'q' to quit\n"));

    // Configure all pins as outputs first
    pinMode(EPD_MOSI_PIN, OUTPUT);
    pinMode(EPD_SCK_PIN, OUTPUT);
    pinMode(EPD_CS_PIN, OUTPUT);
    pinMode(EPD_DC_PIN, OUTPUT);
    pinMode(EPD_RST_PIN, OUTPUT);

    // Set all LOW initially
    digitalWrite(EPD_CS_PIN, LOW);
    digitalWrite(EPD_DC_PIN, LOW);
    digitalWrite(EPD_RST_PIN, LOW);
    digitalWrite(EPD_SCK_PIN, LOW);
    digitalWrite(EPD_MOSI_PIN, LOW);

    // Test each pin individually
    struct PinTest {
        uint8_t pin;
        const char* name;
        bool passed;
    };

    PinTest pins[] = {
        { EPD_CS_PIN, "CS  ", false },
        { EPD_DC_PIN, "DC  ", false },
        { EPD_RST_PIN, "RST ", false },
        { EPD_SCK_PIN, "SCK ", false },
        { EPD_MOSI_PIN, "MOSI", false }
    };

    bool all_passed = true;

    for (int i = 0; i < 5; i++) {
        Serial.printf("\n[TEST %d/5] Testing %s (GPIO %d)\n", i + 1, pins[i].name, pins[i].pin);

        // Set this pin HIGH
        digitalWrite(pins[i].pin, HIGH);
        Serial.println(F("[TEST] Pin set HIGH - measure now (should read ~3.3V)"));

        // Wait for user to press a key
        Serial.println(F("[TEST] Press 'p' if passed, 'f' if failed, 'q' to quit:"));
        while (!Serial.available())
            delay(10);
        char response = Serial.read();
        while (Serial.available())
            Serial.read(); // Clear buffer

        if (response == 'q' || response == 'Q') {
            Serial.println(F("[TEST] Test aborted by user"));
            digitalWrite(pins[i].pin, LOW);
            test_results.data_lines_ok = false;
            return;
        }

        pins[i].passed = (response == 'p' || response == 'P');
        if (!pins[i].passed) {
            all_passed = false;
            Serial.printf("[TEST] FAIL: %s pin not working correctly\n", pins[i].name);
        } else {
            Serial.printf("[TEST] PASS: %s pin OK\n", pins[i].name);
        }

        // Test toggling for this pin
        Serial.printf("[TEST] Toggling %s pin rapidly...\n", pins[i].name);
        for (int j = 0; j < 500; j++) {
            digitalWrite(pins[i].pin, HIGH);
            delayMicroseconds(10);
            digitalWrite(pins[i].pin, LOW);
            delayMicroseconds(10);
        }

        // Set back to LOW before testing next pin
        digitalWrite(pins[i].pin, LOW);
    }

    Serial.println(F("\n[TEST] Pin Test Summary:"));
    Serial.println(F("-----------------------------"));
    for (int i = 0; i < 5; i++) {
        Serial.printf("[TEST] %s (GPIO %d): %s\n",
            pins[i].name,
            pins[i].pin,
            pins[i].passed ? "PASSED" : "FAILED");
    }

    test_results.data_lines_ok = all_passed;

    if (!test_results.data_lines_ok) {
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

void testBusyPin()
{
    Serial.println(F("\n[TEST] BUSY Pin Test"));
    Serial.println(F("-------------------------------"));

    pinMode(EPD_BUSY_PIN, INPUT);

    // Check if pin is stuck
    Serial.println(F("[TEST] Checking if BUSY pin responds..."));
    int readings[20];
    bool varies = false;

    for (int i = 0; i < 20; i++) {
        readings[i] = digitalRead(EPD_BUSY_PIN);
        delay(50);
        if (i > 0 && readings[i] != readings[0]) {
            varies = true;
        }
    }

    if (!varies) {
        Serial.printf("[TEST] WARNING: BUSY pin stuck at %s\n",
            readings[0] ? "HIGH" : "LOW");

        if (readings[0] == HIGH) {
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
        for (int i = 0; i < 10; i++) {
            if (digitalRead(EPD_BUSY_PIN) == LOW) {
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

    if (elapsed > 30000) {
        Serial.println(F("[TEST] FAIL: Likely BUSY timeout!"));
        test_results.busy_timeouts++;
        test_results.busy_pin_ok = false;
    }

    if (!test_results.busy_pin_ok) {
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

void testSPICommunication()
{
    Serial.println(F("\n[TEST] SPI Communication Test"));
    Serial.println(F("-------------------------------"));

    uint32_t test_speeds[] = { 1000000, 2000000, 4000000, 8000000 };
    int best_speed_idx = -1;
    unsigned long best_time = 999999;

    for (int i = 0; i < 4; i++) {
        Serial.printf("[TEST] Testing at %d Hz...\n", test_speeds[i]);

        // Note: Can't actually change SPI speed without library modification
        // This is more of a stress test at current speed

        display.setFullWindow();
        display.firstPage();

        unsigned long start = millis();
        bool timeout = false;

        do {
            // Simple pattern
            for (int y = 0; y < 100; y += 20) {
                display.drawLine(0, y, DISP_WIDTH - 1, y, GxEPD_BLACK);
            }

            if (millis() - start > 25000) {
                timeout = true;
                break;
            }
        } while (display.nextPage());

        unsigned long elapsed = millis() - start;

        if (!timeout && elapsed < best_time) {
            best_time = elapsed;
            best_speed_idx = i;
        }

        Serial.printf("[TEST] Time: %lu ms %s\n",
            elapsed, timeout ? "(TIMEOUT)" : "");

        delay(1000);
    }

    if (best_speed_idx >= 0) {
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

void testRefreshTiming()
{
    Serial.println(F("\n[TEST] Refresh Timing Test"));
    Serial.println(F("-------------------------------"));

    unsigned long timings[3];

    for (int i = 0; i < 3; i++) {
        Serial.printf("[TEST] Test %d/3...\n", i + 1);

        display.setFullWindow();
        display.firstPage();

        unsigned long start = millis();

        do {
            // Alternating pattern
            for (int x = 0; x < DISP_WIDTH; x += 20) {
                display.fillRect(x, 0, 10, DISP_HEIGHT,
                    (x / 20) % 2 ? GxEPD_BLACK : GxEPD_RED);
            }
        } while (display.nextPage());

        timings[i] = millis() - start;
        Serial.printf("[TEST] Refresh %d: %lu ms\n", i + 1, timings[i]);

        if (timings[i] > 20000) {
            test_results.busy_timeouts++;
        }

        delay(2000);
    }

    // Calculate average
    unsigned long avg = (timings[0] + timings[1] + timings[2]) / 3;
    Serial.printf("\n[TEST] Average refresh time: %lu ms\n", avg);

    if (avg > 18000) {
        Serial.println(F("[TEST] WARNING: Close to timeout threshold!"));
        Serial.println(F("[TEST] Consider increasing timeout in library"));
    }

    if (test_results.busy_timeouts > 0) {
        Serial.printf("[TEST] FAIL: %d timeouts detected\n", test_results.busy_timeouts);
    } else {
        Serial.println(F("[TEST] PASS: No timeouts detected"));
    }
}

// =============================================================================
// BATTERY STATUS TEST
// =============================================================================

void testBatteryStatus()
{
    Serial.println(F("\n[TEST] Battery Status Test"));
    Serial.println(F("-------------------------------"));

    // Initialize battery reader
#ifndef USE_SENSOR_MAX1704X
    photo_frame::battery_reader batteryReader(
        BATTERY_PIN,
        BATTERY_RESISTORS_RATIO,
        BATTERY_NUM_READINGS,
        BATTERY_DELAY_BETWEEN_READINGS);
#else
    photo_frame::battery_reader batteryReader;
#endif

    batteryReader.init();

    Serial.println(F("[TEST] Taking battery readings..."));

    // Take multiple readings
    const int NUM_READINGS = 5;
    photo_frame::battery_info_t readings[NUM_READINGS];
    float avgVoltage = 0;
    float avgPercent = 0;

    for (int i = 0; i < NUM_READINGS; i++) {
        Serial.printf("[TEST] Reading %d/%d...\n", i + 1, NUM_READINGS);
        readings[i] = batteryReader.read();

#ifdef USE_SENSOR_MAX1704X
        Serial.printf("[TEST]   Cell Voltage: %.3fV\n", readings[i].cell_voltage);
        Serial.printf("[TEST]   Charge Rate: %.1fmA\n", readings[i].charge_rate);
        Serial.printf("[TEST]   Percentage: %.1f%%\n", readings[i].percent);
        Serial.printf("[TEST]   Millivolts: %lumV\n", readings[i].millivolts);
        avgVoltage += readings[i].cell_voltage;
#else
        Serial.printf("[TEST]   Raw ADC: %lu\n", readings[i].raw_value);
        Serial.printf("[TEST]   Raw mV: %lumV\n", readings[i].raw_millivolts);
        Serial.printf("[TEST]   Corrected mV: %lumV\n", readings[i].millivolts);
        Serial.printf("[TEST]   Percentage: %.1f%%\n", readings[i].percent);
        avgVoltage += readings[i].millivolts / 1000.0f;
#endif
        avgPercent += readings[i].percent;

        delay(500); // Short delay between readings
    }

    avgVoltage /= NUM_READINGS;
    avgPercent /= NUM_READINGS;

    // Get final reading for display
    photo_frame::battery_info_t finalReading = batteryReader.read();

    // Analyze battery status
    Serial.println(F("\n[TEST] Battery Status Analysis:"));
    Serial.println(F("-------------------------------"));
    Serial.printf("[TEST] Average Voltage: %.3fV\n", avgVoltage);
    Serial.printf("[TEST] Average Percentage: %.1f%%\n", avgPercent);
    Serial.printf("[TEST] Is Charging: %s\n", finalReading.is_charging() ? "YES" : "NO");
    Serial.printf("[TEST] Is Low: %s (%d%%)\n", finalReading.is_low() ? "YES" : "NO", BATTERY_PERCENT_LOW);
    Serial.printf("[TEST] Is Critical: %s (%d%%)\n", finalReading.is_critical() ? "YES" : "NO", BATTERY_PERCENT_CRITICAL);
    Serial.printf("[TEST] Is Empty: %s (%d%%)\n", finalReading.is_empty() ? "YES" : "NO", BATTERY_PERCENT_EMPTY);

    // Display results on screen
    Serial.println(F("\n[TEST] Displaying battery status on screen..."));

    display.setFullWindow();
    display.firstPage();

    do {
        display.fillScreen(GxEPD_WHITE);

        // Title
        display.setTextSize(2);
        display.setTextColor(GxEPD_BLACK);
        display.setCursor(20, 20);
        display.println(F("Battery Status Test"));

        // Draw separator line
        display.drawFastHLine(20, 45, DISP_WIDTH - 40, GxEPD_BLACK);

        // Battery icon visualization
        int iconX = 20;
        int iconY = 65;
        int iconWidth = 80;
        int iconHeight = 40;

        // Draw battery outline
        display.drawRect(iconX, iconY, iconWidth, iconHeight, GxEPD_BLACK);
        display.drawRect(iconX + 1, iconY + 1, iconWidth - 2, iconHeight - 2, GxEPD_BLACK);

        // Draw battery terminal
        display.fillRect(iconX + iconWidth, iconY + 10, 8, 20, GxEPD_BLACK);

        // Fill battery based on percentage
        int fillWidth = (iconWidth - 6) * finalReading.percent / 100;
        if (fillWidth > 0) {
            uint16_t fillColor = GxEPD_BLACK;

#if defined(DISP_6C) || defined(DISP_7C)
            // Use colors on color displays
            if (finalReading.is_critical()) {
                fillColor = GxEPD_RED;
            } else if (finalReading.is_low()) {
                fillColor = GxEPD_YELLOW;
            } else {
                fillColor = GxEPD_GREEN;
            }
#endif

            display.fillRect(iconX + 3, iconY + 3, fillWidth, iconHeight - 6, fillColor);
        }

        // Display percentage inside battery icon
        display.setTextSize(2);
        display.setTextColor(fillWidth > 40 ? GxEPD_WHITE : GxEPD_BLACK);
        display.setCursor(iconX + 20, iconY + 13);
        display.printf("%.0f%%", finalReading.percent);

        // Voltage information
        display.setTextSize(2);
        display.setTextColor(GxEPD_BLACK);
        display.setCursor(iconX + iconWidth + 30, iconY + 13);
#ifdef USE_SENSOR_MAX1704X
        display.printf("%.2fV", finalReading.cell_voltage);
#else
        display.printf("%.2fV", finalReading.millivolts / 1000.0f);
#endif

        // Detailed readings
        display.setTextSize(1);
        display.setCursor(20, iconY + iconHeight + 20);
        display.println(F("Detailed Readings:"));

        int yPos = iconY + iconHeight + 40;

#ifdef USE_SENSOR_MAX1704X
        display.setCursor(20, yPos);
        display.printf("Cell Voltage:  %.3fV", finalReading.cell_voltage);
        yPos += 15;

        display.setCursor(20, yPos);
        display.printf("Charge Rate:   %.1fmA", finalReading.charge_rate);
        yPos += 15;

        display.setCursor(20, yPos);
        display.printf("Percentage:    %.1f%%", finalReading.percent);
        yPos += 15;

        display.setCursor(20, yPos);
        display.printf("Millivolts:    %lumV", finalReading.millivolts);
        yPos += 15;

        display.setCursor(20, yPos);
        display.println(F("Sensor Type:   MAX1704X"));
#else
        display.setCursor(20, yPos);
        display.printf("Raw ADC:       %lu", finalReading.raw_value);
        yPos += 15;

        display.setCursor(20, yPos);
        display.printf("Raw Voltage:   %lumV", finalReading.raw_millivolts);
        yPos += 15;

        display.setCursor(20, yPos);
        display.printf("Corrected:     %lumV", finalReading.millivolts);
        yPos += 15;

        display.setCursor(20, yPos);
        display.printf("Percentage:    %.1f%%", finalReading.percent);
        yPos += 15;

        display.setCursor(20, yPos);
        display.printf("ADC Pin:       GPIO%d", BATTERY_PIN);
        yPos += 15;

        display.setCursor(20, yPos);
        display.printf("Divider Ratio: %.3f", BATTERY_RESISTORS_RATIO);
        yPos += 15;

        display.setCursor(20, yPos);
        display.println(F("Sensor Type:   Analog"));
#endif

        yPos += 25;

        // Status indicators
        display.setTextSize(2);
        display.setCursor(20, yPos);
        display.print(F("Status: "));

        // Determine status text and color
        String statusText;
        uint16_t statusColor = GxEPD_BLACK;

        if (finalReading.is_charging()) {
            statusText = "CHARGING";
#if defined(DISP_6C) || defined(DISP_7C)
            statusColor = GxEPD_BLUE;
#endif
        } else if (finalReading.is_empty()) {
            statusText = "EMPTY";
#if defined(DISP_6C) || defined(DISP_7C)
            statusColor = GxEPD_RED;
#endif
        } else if (finalReading.is_critical()) {
            statusText = "CRITICAL";
#if defined(DISP_6C) || defined(DISP_7C)
            statusColor = GxEPD_RED;
#endif
        } else if (finalReading.is_low()) {
            statusText = "LOW";
#if defined(DISP_6C) || defined(DISP_7C)
            statusColor = GxEPD_YELLOW;
#elif defined(DISP_7C)
            statusColor = GxEPD_ORANGE;
#endif
        } else {
            statusText = "GOOD";
#if defined(DISP_6C) || defined(DISP_7C)
            statusColor = GxEPD_GREEN;
#endif
        }

        display.setTextColor(statusColor);
        display.print(statusText);

        yPos += 30;

        // Thresholds information
        display.setTextSize(1);
        display.setTextColor(GxEPD_BLACK);
        display.setCursor(20, yPos);
        display.println(F("Battery Thresholds:"));
        yPos += 15;

        display.setCursor(20, yPos);
        display.printf("Low:      < %d%%", BATTERY_PERCENT_LOW);
        yPos += 15;

        display.setCursor(20, yPos);
        display.printf("Critical: < %d%%", BATTERY_PERCENT_CRITICAL);
        yPos += 15;

        display.setCursor(20, yPos);
        display.printf("Empty:    < %d%%", BATTERY_PERCENT_EMPTY);
        yPos += 15;

        display.setCursor(20, yPos);
        display.printf("Charging: > %dmV", BATTERY_CHARGING_MILLIVOLTS);
        yPos += 20;

        // Test result
        display.drawFastHLine(20, yPos, DISP_WIDTH - 40, GxEPD_BLACK);
        yPos += 10;

        display.setTextSize(2);
        bool testPassed = (finalReading.percent >= 0 && finalReading.percent <= 120) &&
                         (finalReading.millivolts > 0 && finalReading.millivolts < 5000);

        display.setCursor(20, yPos);
        display.print(F("Test Result: "));

#if defined(DISP_6C) || defined(DISP_7C)
        display.setTextColor(testPassed ? GxEPD_GREEN : GxEPD_RED);
#else
        display.setTextColor(GxEPD_BLACK);
#endif
        display.print(testPassed ? F("PASS") : F("FAIL"));

        // Footer
        display.setTextSize(1);
        display.setTextColor(GxEPD_BLACK);
        display.setCursor(20, DISP_HEIGHT - 30);
        display.println(F("Press any key to continue..."));

    } while (display.nextPage());

    // Update test results
    test_results.power_supply_ok = (finalReading.millivolts > 3000 && finalReading.millivolts < 4500);

    Serial.println(F("[TEST] Battery status displayed on screen"));
    Serial.println(F("[TEST] Press any key to continue..."));

    // Wait for user input
    while (!Serial.available())
        delay(10);
    while (Serial.available())
        Serial.read(); // Clear buffer
}

// =============================================================================
// HARDWARE FAILURE TEST
// =============================================================================

void testHardwareFailure()
{
    Serial.println(F("\n[TEST] Hardware Failure Diagnostic"));
    Serial.println(F("-------------------------------"));
    Serial.println(F("This test checks for display/DESPI hardware issues\n"));

    // Test 1: Black and White Only
    Serial.println(F("[TEST 1/4] Black & White Pattern Test"));
    Serial.println(F("If this works but colors fail = Color circuit issue"));

    display.setFullWindow();
    display.firstPage();
    unsigned long bwStart = millis();

    do {
        // Simple B&W checkerboard
        for (int y = 0; y < DISP_HEIGHT; y += 20) {
            for (int x = 0; x < DISP_WIDTH; x += 20) {
                if (((x / 20) + (y / 20)) % 2 == 0) {
                    display.fillRect(x, y, 20, 20, GxEPD_BLACK);
                } else {
                    display.fillRect(x, y, 20, 20, GxEPD_WHITE);
                }
            }
        }
    } while (display.nextPage());

    unsigned long bwTime = millis() - bwStart;
    Serial.printf("[TEST 1] B&W refresh time: %lu ms\n", bwTime);
    Serial.println(F("[TEST 1] Check display - is B&W pattern clear? (y/n)"));

    while (!Serial.available())
        delay(10);
    char bwResult = Serial.read();
    while (Serial.available())
        Serial.read();

    bool bwPass = (bwResult == 'y' || bwResult == 'Y');

    delay(2000);

    // Test 2: Single Color Test
#ifdef DISP_6C
    Serial.println(F("\n[TEST 2/4] Individual Color Test"));
    Serial.println(F("Testing each color separately..."));

    uint16_t colors[] = { GxEPD_BLACK, GxEPD_WHITE, GxEPD_RED,
        GxEPD_GREEN, GxEPD_BLUE, GxEPD_YELLOW };
    const char* colorNames[] = { "BLACK", "WHITE", "RED",
        "GREEN", "BLUE", "YELLOW" };
    bool colorResults[6];

    for (int i = 0; i < 6; i++) {
        Serial.printf("[TEST 2] Testing %s...\n", colorNames[i]);

        display.setFullWindow();
        display.firstPage();
        unsigned long colorStart = millis();

        do {
            display.fillScreen(colors[i]);
            // Add text label
            display.setTextColor(i == 0 ? GxEPD_WHITE : GxEPD_BLACK);
            display.setCursor(DISP_WIDTH / 2 - 50, DISP_HEIGHT / 2);
            display.setTextSize(3);
            display.print(colorNames[i]);
        } while (display.nextPage());

        unsigned long colorTime = millis() - colorStart;
        Serial.printf("[TEST 2] %s refresh time: %lu ms\n", colorNames[i], colorTime);
        Serial.printf("[TEST 2] Is %s displayed correctly? (y/n): ", colorNames[i]);

        while (!Serial.available())
            delay(10);
        char result = Serial.read();
        while (Serial.available())
            Serial.read();

        colorResults[i] = (result == 'y' || result == 'Y');
        delay(1500);
    }

    // Analyze color results
    Serial.println(F("\n[TEST 2] Color Test Results:"));
    for (int i = 0; i < 6; i++) {
        Serial.printf("  %s: %s\n", colorNames[i],
            colorResults[i] ? "PASS" : "FAIL");
        if (!colorResults[i])
            failedColors++;
    }
#else
    Serial.println(F("\n[TEST 2/4] Skipped - B&W display"));
#endif

    // Test 3: Gradient Test
    Serial.println(F("\n[TEST 3/4] Gradient/Transition Test"));
    Serial.println(F("Looking for dead zones or line defects..."));

    display.setFullWindow();
    display.firstPage();
    unsigned long gradStart = millis();

    do {
        // Vertical gradient bars
        for (int x = 0; x < DISP_WIDTH; x++) {
            uint16_t color;
#ifdef DISP_6C
            if (x < DISP_WIDTH / 6)
                color = GxEPD_BLACK;
            else if (x < DISP_WIDTH * 2 / 6)
                color = GxEPD_RED;
            else if (x < DISP_WIDTH * 3 / 6)
                color = GxEPD_GREEN;
            else if (x < DISP_WIDTH * 4 / 6)
                color = GxEPD_BLUE;
            else if (x < DISP_WIDTH * 5 / 6)
                color = GxEPD_YELLOW;
            else
                color = GxEPD_WHITE;
#else
            color = (x < DISP_WIDTH / 2) ? GxEPD_BLACK : GxEPD_WHITE;
#endif

            display.drawLine(x, 0, x, DISP_HEIGHT - 1, color);
        }
    } while (display.nextPage());

    unsigned long gradTime = millis() - gradStart;
    Serial.printf("[TEST 3] Gradient refresh time: %lu ms\n", gradTime);
    Serial.println(F("[TEST 3] Check for:"));
    Serial.println(F("  - Dead zones (areas that don't change)"));
    Serial.println(F("  - Line defects (permanent lines)"));
    Serial.println(F("  - Color bleeding between sections"));
    Serial.println(F("[TEST 3] Any defects visible? (y=defects, n=clear): "));

    while (!Serial.available())
        delay(10);
    char gradResult = Serial.read();
    while (Serial.available())
        Serial.read();

    bool hasDefects = (gradResult == 'y' || gradResult == 'Y');

    // Test 4: Connection Test
    Serial.println(F("\n[TEST 4/4] Connection Stability Test"));
    Serial.println(F("Gently flex ribbon cable and press DESPI board"));
    Serial.println(F("Watch for display changes/glitches"));
    Serial.println(F("Press any key when done testing..."));

    // Show test pattern during flex test
    display.setFullWindow();
    display.firstPage();
    do {
        for (int i = 0; i < 10; i++) {
            display.drawLine(0, i * 48, DISP_WIDTH - 1, i * 48, GxEPD_BLACK);
            display.drawLine(i * 80, 0, i * 80, DISP_HEIGHT - 1, GxEPD_BLACK);
        }
    } while (display.nextPage());

    while (!Serial.available())
        delay(10);
    while (Serial.available())
        Serial.read();

    Serial.println(F("[TEST 4] Did display change during flexing? (y/n): "));
    while (!Serial.available())
        delay(10);
    char flexResult = Serial.read();
    while (Serial.available())
        Serial.read();

    bool connectionIssue = (flexResult == 'y' || flexResult == 'Y');

    // Final Analysis
    Serial.println(F("\n========================================"));
    Serial.println(F("Hardware Diagnostic Results"));
    Serial.println(F("========================================"));

    if (bwPass && !hasDefects && !connectionIssue) {
#ifdef DISP_6C
        if (failedColors == 0) {
            Serial.println(F("✅ Hardware appears GOOD"));
            Serial.println(F("Issue is likely power delivery"));
        } else if (failedColors <= 2) {
            Serial.println(F("⚠️ Partial hardware issue"));
            Serial.println(F("Some colors failing - check DESPI board"));
        } else {
            Serial.println(F("❌ Hardware FAILURE likely"));
            Serial.println(F("Multiple colors failing - DESPI or display issue"));
        }
#else
        Serial.println(F("✅ Hardware appears GOOD"));
#endif
    } else {
        Serial.println(F("❌ Hardware issues detected:"));
        if (!bwPass)
            Serial.println(F("  - Basic display function failed"));
        if (hasDefects)
            Serial.println(F("  - Dead zones or line defects found"));
        if (connectionIssue)
            Serial.println(F("  - Connection stability problem"));
    }

    Serial.println(F("\nRecommendations:"));
    if (connectionIssue) {
        Serial.println(F("1. Check ribbon cable and connectors"));
        Serial.println(F("2. Reseat all connections"));
    }
    if (!bwPass || hasDefects) {
        Serial.println(F("1. Inspect for physical damage"));
        Serial.println(F("2. Try different display or DESPI board"));
    }
#ifdef DISP_6C
    if (failedColors > 0 && failedColors < 6) {
        Serial.println(F("1. DESPI voltage regulator may be weak"));
        Serial.println(F("2. Add capacitors for power stability"));
    }
#endif
}

// =============================================================================
// SD CARD IMAGE GALLERY TEST
// =============================================================================

void testSDCardImages()
{
    Serial.println(F("\n[TEST] SD Card Image Gallery Test"));
    Serial.println(F("-------------------------------"));
    Serial.println(F("This will render all BMP and BIN images from SD root"));

    auto sdError = sdCard.begin();
    if (sdError != photo_frame::error_type::None) {
        Serial.println(F("[TEST] FAIL: Could not initialize SD card"));
        return;
    }

    // Initialize display if not already done
    if (!display.epd2.WIDTH) {
        photo_frame::renderer::init_display();
    }

    // Open SD root directory
    File root = sdCard.open("/examples");
    if (!root || !root.isDirectory()) {
        Serial.println(F("[TEST] Failed to open SD root directory"));
        return;
    }

    bool hasColors = photo_frame::renderer::has_color();
    bool hasPartialUpdate = photo_frame::renderer::has_partial_update();

    // Count and list images first
    Serial.println(F("\n[TEST] Scanning for images..."));

    // Limit max images to prevent stack overflow
    // Ensure we test both BMP and BIN files
    const int MAX_TEST_IMAGES = 10;
    const int MAX_PER_TYPE = MAX_TEST_IMAGES / 2;  // 5 of each type max
    String* imageFiles = new String[MAX_TEST_IMAGES];
    int imageCount = 0;
    int bmpCount = 0;
    int binCount = 0;

    File file = root.openNextFile();

    while (file) {
        String filename = String(file.name());

        // Skip hidden files and subdirectories
        if (!file.isDirectory() && !filename.startsWith(".")) {
            String lowerName = filename;
            lowerName.toLowerCase();

            // Check for supported formats
            if (lowerName.endsWith(".bmp") || lowerName.endsWith(".bin")) {
                bool is_valid = false;
                if (hasColors) {
                    is_valid = lowerName.startsWith("6c_");
                } else {
                    is_valid = lowerName.startsWith("bw_");
                }

                if (is_valid && imageCount < MAX_TEST_IMAGES) {
                    // Balance between BMP and BIN files
                    bool isBin = photo_frame::io_utils::is_binary_format(lowerName.c_str());
                    bool isBmp = !isBin;

                    bool canAdd = false;
                    if (isBmp && bmpCount < MAX_PER_TYPE) {
                        bmpCount++;
                        canAdd = true;
                    } else if (isBin && binCount < MAX_PER_TYPE) {
                        binCount++;
                        canAdd = true;
                    }

                    if (canAdd) {
                        imageFiles[imageCount] = String("/examples/") + filename;
                        Serial.printf("[TEST] Found: %s (%d bytes) [BMP:%d BIN:%d]\n",
                                     filename.c_str(), file.size(), bmpCount, binCount);
                        imageCount++;
                    }
                }
            }
        }
        file.close();
        file = root.openNextFile();
    }
    root.close();

    // Simple randomization without std::mt19937 to save stack space
    if (imageCount > 1) {
        Serial.println(F("[TEST] Randomizing image order..."));

        // Simple Fisher-Yates shuffle using ESP32's hardware RNG
        for (int i = imageCount - 1; i > 0; i--) {
            int j = esp_random() % (i + 1);
            String temp = imageFiles[i];
            imageFiles[i] = imageFiles[j];
            imageFiles[j] = temp;
        }

        Serial.println(F("[TEST] Images randomized"));
    }

    int totalImages = imageCount;
    if (totalImages == 0) {
        Serial.println(F("[TEST] No BMP or BIN files found in SD root"));
        delete[] imageFiles;
        return;
    }

    Serial.printf("\n[TEST] Found %d images total (BMP: %d, BIN: %d)\n", imageCount, bmpCount, binCount);

    if (imageCount >= MAX_TEST_IMAGES) {
        Serial.printf("[TEST] WARNING: Limited to %d images to prevent memory issues\n", MAX_TEST_IMAGES);
        Serial.printf("[TEST] Selected mix: %d BMP files, %d BIN files\n", bmpCount, binCount);
    }

    Serial.printf("\n[TEST] Ready to test %d images. Press 'Y' to start rendering, 'N' to cancel: ", totalImages);
    while (!Serial.available())
        delay(10);

    char confirm = Serial.read();
    while (Serial.available())
        Serial.read(); // Clear buffer

    if (confirm != 'Y' && confirm != 'y') {
        Serial.println(F("\n[TEST] Cancelled by user"));
        delete[] imageFiles;
        return;
    }

    // Ask about overlay testing
    Serial.print(F("\n[TEST] Test overlays (last update, image info, weather)? (Y/N)"));
    while (!Serial.available())
        delay(10);

    char testOverlays = Serial.read();
    while (Serial.available())
        Serial.read(); // Clear buffer

    bool shouldTestOverlays = (testOverlays == 'Y' || testOverlays == 'y');

    // Render each image
    Serial.println(F("\n[TEST] Starting image rendering..."));
    int successCount = 0;
    int failCount = 0;
    unsigned long totalTestTime = 0;

    Serial.printf("\n[TEST] Display has colors: %d", hasColors);
    Serial.printf("\n[TEST] Display has partial updates: %d", hasPartialUpdate);

    for (int i = 0; i < totalImages; i++) {
        String& filename = imageFiles[i];
        Serial.printf("\n[TEST] ========================================\n");
        Serial.printf("[TEST] Image %d/%d: %s\n", i + 1, totalImages, filename.c_str());
        Serial.printf("[TEST] ========================================\n");

        File file = sdCard.open(filename.c_str(), FILE_READ);

        bool isBinary = photo_frame::io_utils::is_binary_format(filename.c_str());

        uint16_t error_code = 0;

        unsigned long startTime = millis();

        if (hasPartialUpdate) {
            Serial.println("\n[TEST] drawing buffered");
            if (isBinary) {
                error_code = photo_frame::renderer::draw_binary_from_file_buffered(file, filename.c_str(), DISP_WIDTH, DISP_HEIGHT);
            } else {
                error_code = photo_frame::renderer::draw_bitmap_from_file_buffered(file, filename.c_str(), 0, 0, hasColors);
            }

            if (error_code == 0 && shouldTestOverlays) {
                Serial.println(F("[TEST] Creating fake overlay data..."));

                // Create fake DateTime for last update
                DateTime fakeLastUpdate(2024, 3, 15, 14, 30, 45);
                long fakeRefreshSeconds = 3600; // 1 hour refresh interval
                uint32_t fakeImageIndex = i + 1;
                uint32_t fakeTotalImages = totalImages;

                // Create fake battery info
                photo_frame::battery_info_t fakeBatteryInfo;
#ifdef USE_SENSOR_MAX1704X
                fakeBatteryInfo.cell_voltage = 3.8f;
                fakeBatteryInfo.charge_rate = 0.0f;
                fakeBatteryInfo.percent = 75.0f;
                fakeBatteryInfo.millivolts = 3800;
#else
                fakeBatteryInfo.raw_value = 2800;
                fakeBatteryInfo.raw_millivolts = 3800;
                fakeBatteryInfo.millivolts = 3800;
                fakeBatteryInfo.percent = 75.0f;
#endif

                // Create fake weather data
                photo_frame::weather::WeatherData fakeWeather;
                fakeWeather.temperature = 22.5f;
                fakeWeather.weather_code = 2; // Partly cloudy
                fakeWeather.icon = photo_frame::weather::weather_icon_t::cloudy_day;
                fakeWeather.description = "Partly Cloudy";
                // Use a fixed recent timestamp that will always be considered fresh
                // Set to 2024-12-15 12:00:00 UTC (recent enough to not be stale)
                fakeWeather.last_update = time(NULL);
                fakeWeather.humidity = 65;
                fakeWeather.is_day = true;
                fakeWeather.wind_speed = 12.5f;
                fakeWeather.valid = true;
                fakeWeather.temperature_unit = "°C";
                fakeWeather.temp_min = 18.0f;
                fakeWeather.temp_max = 26.0f;
                fakeWeather.sunrise_time = 1734264000 - 7200;  // 2 hours before
                fakeWeather.sunset_time = 1734264000 + 36000;  // 10 hours after
                fakeWeather.has_daily_data = true;

                display.setPartialWindow(0, 0, display.width(), 16);
                display.firstPage();
                do {
                    display.fillScreen(GxEPD_WHITE);
                    Serial.println(F("[TEST] Drawing overlays with fake data..."));
                    // Draw last update overlay
                    Serial.println(F("[TEST] Drawing last update overlay..."));
                    photo_frame::renderer::draw_last_update(fakeLastUpdate, fakeRefreshSeconds);

                    // Draw image info overlay
                    Serial.println(F("[TEST] Drawing image info overlay..."));
                    photo_frame::image_source_t fakeImageSource = (i % 2 == 0) ? photo_frame::IMAGE_SOURCE_CLOUD
                                                                               : photo_frame::IMAGE_SOURCE_LOCAL_CACHE;
                    photo_frame::renderer::draw_image_info(fakeImageIndex, fakeTotalImages, fakeImageSource);
                    photo_frame::renderer::draw_battery_status(fakeBatteryInfo);
                } while (display.nextPage()); // Clear the display

                // Draw weather info overlay - always draw for testing
                Serial.println(F("[TEST] Drawing weather info overlay..."));
                Serial.printf("[TEST] Weather valid: %s, displayable: %s\n",
                             fakeWeather.valid ? "true" : "false",
                             fakeWeather.is_displayable() ? "true" : "false");

                rect_t box = photo_frame::renderer::get_weather_info_rect();
                Serial.printf("[TEST] Weather box: x=%d, y=%d, w=%d, h=%d\n",
                             box.x, box.y, box.width, box.height);

                display.setPartialWindow(box.x, box.y, box.width, box.height);
                display.firstPage();
                do {
                    // Still try to call the real function in case it works
                    photo_frame::renderer::draw_weather_info(fakeWeather, box);
                } while (display.nextPage());

                Serial.println(F("[TEST] Weather overlay drawn"));
            }

        } else {
            Serial.println("\n[TEST] drawing unbuffered");
            display.clearScreen();
            if (isBinary) {
                Serial.println("[TEST] Calling draw_binary_from_file_paged");
                error_code = photo_frame::renderer::draw_binary_from_file_paged(file, filename.c_str(), DISP_WIDTH, DISP_HEIGHT, -1);
            } else {
                Serial.println("[TEST] Calling draw_bitmap_from_file (unbuffered - may be slow)");
                Serial.printf("[TEST] File size: %d bytes\n", file.size());
                error_code = photo_frame::renderer::draw_bitmap_from_file(file, filename.c_str());
                error_code = 0; // drawBitmapFromSD doesn't return error code
            }
            Serial.println("[TEST] Calling display.refresh()");
            unsigned long refreshStart = millis();
            display.refresh();
            unsigned long refreshTime = millis() - refreshStart;
            Serial.printf("[TEST] display.refresh() completed in %lu ms\n", refreshTime);

            if (error_code == 0 && shouldTestOverlays) {
                Serial.println(F("[TEST] Creating fake overlay data..."));

                // Create fake DateTime for last update
                DateTime fakeLastUpdate(2024, 3, 15, 14, 30, 45);
                long fakeRefreshSeconds = 3600; // 1 hour refresh interval
                uint32_t fakeImageIndex = i + 1;
                uint32_t fakeTotalImages = totalImages;

                // Create fake battery info
                photo_frame::battery_info_t fakeBatteryInfo;
#ifdef USE_SENSOR_MAX1704X
                fakeBatteryInfo.cell_voltage = 3.8f;
                fakeBatteryInfo.charge_rate = 0.0f;
                fakeBatteryInfo.percent = 75.0f;
                fakeBatteryInfo.millivolts = 3800;
#else
                fakeBatteryInfo.raw_value = 2800;
                fakeBatteryInfo.raw_millivolts = 3800;
                fakeBatteryInfo.millivolts = 3800;
                fakeBatteryInfo.percent = 75.0f;
#endif

                // Create fake weather data
                photo_frame::weather::WeatherData fakeWeather;
                fakeWeather.temperature = 22.5f;
                fakeWeather.weather_code = 2; // Partly cloudy
                fakeWeather.icon = photo_frame::weather::weather_icon_t::cloudy_day;
                fakeWeather.description = "Partly Cloudy";
                // Use a fixed recent timestamp that will always be considered fresh
                // Set to 2024-12-15 12:00:00 UTC (recent enough to not be stale)
                fakeWeather.last_update = 1734264000; // December 15, 2024
                fakeWeather.humidity = 65;
                fakeWeather.is_day = true;
                fakeWeather.wind_speed = 12.5f;
                fakeWeather.valid = true;
                fakeWeather.temperature_unit = "°C";
                fakeWeather.temp_min = 18.0f;
                fakeWeather.temp_max = 26.0f;
                fakeWeather.sunrise_time = 1734264000 - 7200;  // 2 hours before
                fakeWeather.sunset_time = 1734264000 + 36000;  // 10 hours after
                fakeWeather.has_daily_data = true;

                Serial.println(F("[TEST] Drawing overlays with fake data..."));
                // Draw last update overlay
                Serial.println(F("[TEST] Drawing last update overlay..."));
                photo_frame::renderer::draw_last_update(fakeLastUpdate, fakeRefreshSeconds);

                // Draw image info overlay
                Serial.println(F("[TEST] Drawing image info overlay..."));
                photo_frame::image_source_t fakeImageSource = (i % 2 == 0) ? photo_frame::IMAGE_SOURCE_CLOUD
                                                                           : photo_frame::IMAGE_SOURCE_LOCAL_CACHE;
                photo_frame::renderer::draw_image_info(fakeImageIndex, fakeTotalImages, fakeImageSource);
                photo_frame::renderer::draw_battery_status(fakeBatteryInfo);

                // Draw weather info overlay - always draw for testing
                Serial.println(F("[TEST] Drawing weather info overlay..."));
                Serial.printf("[TEST] Weather valid: %s, displayable: %s\n",
                             fakeWeather.valid ? "true" : "false",
                             fakeWeather.is_displayable() ? "true" : "false");

                rect_t box = photo_frame::renderer::get_weather_info_rect();
                Serial.printf("[TEST] Weather box: x=%d, y=%d, w=%d, h=%d\n",
                             box.x, box.y, box.width, box.height);

                // Draw black background box with white border for testing
                display.fillRect(box.x, box.y, box.width, box.height, GxEPD_BLACK);
                display.drawRect(box.x, box.y, box.width, box.height, GxEPD_WHITE);

                // Draw test weather info manually since draw_weather_info might skip due to staleness
                display.setTextColor(GxEPD_WHITE);
                display.setTextSize(2);
                display.setCursor(box.x + 10, box.y + 20);
                display.printf("%.1f%s", fakeWeather.temperature, fakeWeather.temperature_unit.c_str());

                display.setTextSize(1);
                display.setCursor(box.x + 10, box.y + 45);
                display.print(fakeWeather.description);

                display.setCursor(box.x + 10, box.y + 60);
                display.printf("H:%d%% W:%.1fkm/h", fakeWeather.humidity, fakeWeather.wind_speed);

                // Still try to call the real function in case it works
                photo_frame::renderer::draw_weather_info(fakeWeather, box);
                Serial.println(F("[TEST] Weather overlay drawn"));

                // Refresh display to show overlays
                Serial.println(F("[TEST] Refreshing display with overlays..."));
                display.refresh();
                Serial.println(F("[TEST] Overlays drawn successfully"));
            }
        }

        file.close();

        unsigned long renderTime = millis() - startTime;
        totalTestTime += renderTime;

        if (error_code == 0) {
            Serial.printf("[TEST] SUCCESS: Rendered in %lu ms\n", renderTime);
            successCount++;
        } else {
            Serial.printf("[TEST] FAIL: Error code %d\n", error_code);
            failCount++;
        }

        // Wait between images
        delay(4000);
    }

    // Final summary
    Serial.println(F("\n========================================"));
    Serial.println(F("SD Card Image Gallery Test Complete"));
    Serial.println(F("========================================"));
    Serial.printf("Total images: %d\n", totalImages);
    Serial.printf("Successful: %d\n", successCount);
    Serial.printf("Failed: %d\n", failCount);
    Serial.printf("Total test time: %lu ms\n", totalTestTime);
    if (totalImages > 0) {
        Serial.printf("Average time per image: %lu ms\n", totalTestTime / totalImages);
    }

    String resultText;
    if (successCount == totalImages) {
        Serial.println(F("Result: PASS - All images rendered successfully"));
        resultText = "PASS";
    } else if (successCount > 0) {
        Serial.println(F("Result: PARTIAL - Some images failed"));
        resultText = "PARTIAL";
    } else {
        Serial.println(F("Result: FAIL - No images rendered successfully"));
        resultText = "FAIL";
    }

    delay(5000);

    // Display summary on screen
    Serial.println(F("\n[TEST] Displaying summary on screen..."));

    display.setFullWindow();
    display.firstPage();

    do {
        display.fillScreen(GxEPD_WHITE);

        // Title
        display.setTextSize(2);
        display.setTextColor(GxEPD_BLACK);
        display.setCursor(20, 20);
        display.println(F("Gallery Test Results"));

        // Draw a separator line
        display.drawFastHLine(20, 45, DISP_WIDTH - 40, GxEPD_BLACK);

        // Statistics - moved down to avoid line overlap
        display.setTextSize(2);
        display.setCursor(20, 70);  // Moved from 60 to 70
        display.printf("Total Images: %d", totalImages);

        display.setCursor(20, 95);  // Moved from 85 to 95
        display.printf("Successful:   %d", successCount);

        display.setCursor(20, 120);  // Moved from 110 to 120
        display.printf("Failed:       %d", failCount);

        // Success rate
        if (totalImages > 0) {
            float successRate = (float)successCount / totalImages * 100.0;
            display.setCursor(20, 145);  // Moved from 135 to 145
            display.printf("Success Rate: %.1f%%", successRate);
        }

        // Timing statistics
        display.setCursor(20, 170);
        display.printf("Total Time:   %lu ms", totalTestTime);

        if (totalImages > 0) {
            unsigned long avgTime = totalTestTime / totalImages;
            display.setCursor(20, 195);
            display.printf("Avg per Image: %lu ms", avgTime);
        }

        // Draw another separator line
        display.drawFastHLine(20, 225, DISP_WIDTH - 40, GxEPD_BLACK);  // Moved from 165 to 225

        // Result status - larger text, moved down more
        display.setTextSize(3);
        display.setCursor(20, 255);  // Moved from 245 to 255 (another 10 pixels down)
        display.print("Result: ");

        // Color code the result if color display
#ifdef DISP_6C
        if (resultText == "PASS") {
            display.setTextColor(GxEPD_GREEN);
        } else if (resultText == "FAIL") {
            display.setTextColor(GxEPD_RED);
        } else {
            display.setTextColor(GxEPD_YELLOW);
        }
#elif defined(DISP_7C)
        if (resultText == "PASS") {
            display.setTextColor(GxEPD_GREEN);
        } else if (resultText == "FAIL") {
            display.setTextColor(GxEPD_RED);
        } else {
            display.setTextColor(GxEPD_ORANGE);
        }
#endif
        display.print(resultText);

        // Additional info
        display.setTextSize(1);
        display.setTextColor(GxEPD_BLACK);
        display.setCursor(20, 300);  // Moved from 290 to 300

        if (shouldTestOverlays) {
            display.println(F("Overlay testing: ENABLED"));
        } else {
            display.println(F("Overlay testing: DISABLED"));
        }

        // Display type info
        display.setCursor(20, 315);  // Moved from 305 to 315
#ifdef DISP_6C
        display.println(F("Display: 6-Color E-Paper"));
#elif defined(DISP_7C)
        display.println(F("Display: 7-Color E-Paper"));
#elif defined(DISP_BW_V2)
        display.println(F("Display: Black & White E-Paper"));
#else
        display.println(F("Display: B&W E-Paper"));
#endif

        // Test completion time
        display.setCursor(20, 330);  // Moved from 320 to 330
        display.println(F("Test completed successfully"));

        // Bottom message
        display.setTextSize(1);
        display.setCursor(20, DISP_HEIGHT - 30);
        display.println(F("Press any key to continue..."));

    } while (display.nextPage());

    Serial.println(F("[TEST] Summary displayed on screen"));
    Serial.println(F("[TEST] Press any key to continue..."));

    // Wait for user input before returning
    while (!Serial.available())
        delay(10);
    while (Serial.available())
        Serial.read(); // Clear buffer

    // Clean up dynamically allocated memory
    delete[] imageFiles;
}

// =============================================================================
// INTERACTIVE MENU
// =============================================================================

void showMenu()
{
    Serial.println(F("\n========================================"));
    Serial.println(F("Display Debug Menu"));
    Serial.println(F("========================================"));
    Serial.println(F("1. Run Full Diagnostic Suite"));
    Serial.println(F("2. Color Pattern Test"));
    Serial.println(F("3. High Stress Color Test (Timeout Test)"));
    Serial.println(F("4. Power Supply Test"));
    Serial.println(F("5. Data Line Voltage Test"));
    Serial.println(F("6. BUSY Pin Test"));
    Serial.println(F("7. SPI Communication Test"));
    Serial.println(F("8. Refresh Timing Test"));
    Serial.println(F("9. Battery Status Test"));
    Serial.println(F("B. Color Bars Test (All Display Colors)"));
    Serial.println(F("H. Hardware Failure Test"));
    Serial.println(F("G. Gallery - Render All SD Card Images"));
    Serial.println(F("P. Test Pins (Release SPI First)"));
    Serial.println(F("R. Show Test Results Summary"));
    Serial.println(F("C. Clear Display"));
    Serial.println(F("0. Restart ESP32"));
    Serial.println(F("----------------------------------------"));
    Serial.println(F("Enter choice:"));
}

void showResults()
{
    Serial.println(F("\n========================================"));
    Serial.println(F("Test Results Summary"));
    Serial.println(F("========================================"));

    Serial.printf("Power Supply:     %s", test_results.power_supply_ok ? "PASS" : "FAIL");
    if (!test_results.power_supply_ok) {
        Serial.printf(" (%.2fV drop)", test_results.voltage_drop);
    }
    Serial.println();

    Serial.printf("Data Lines:       %s\n", test_results.data_lines_ok ? "PASS" : "FAIL/UNKNOWN");
    Serial.printf("BUSY Pin:         %s", test_results.busy_pin_ok ? "PASS" : "FAIL");
    if (test_results.busy_timeouts > 0) {
        Serial.printf(" (%d timeouts)", test_results.busy_timeouts);
    }
    Serial.println();

    Serial.printf("SPI Comm:         %s\n", test_results.spi_comm_ok ? "PASS" : "FAIL");
    Serial.printf("Refresh Timing:   %s", test_results.refresh_timing_ok ? "PASS" : "FAIL");
    if (test_results.refresh_time > 0) {
        Serial.printf(" (%lu ms)", test_results.refresh_time);
    }
    Serial.println();

    Serial.println(F("\n========================================"));

    // Diagnosis
    if (!test_results.power_supply_ok) {
        Serial.println(F("\nDIAGNOSIS: Power delivery issue"));
        Serial.println(F("SOLUTION: Add 100-470µF capacitor"));
    }

    if (!test_results.busy_pin_ok) {
        Serial.println(F("\nDIAGNOSIS: BUSY pin connection issue"));
        Serial.println(F("SOLUTION: Check wire, add 10kΩ pull-up"));
    }

    if (!test_results.data_lines_ok) {
        Serial.println(F("\nDIAGNOSIS: Data line signal integrity"));
        Serial.println(F("SOLUTION: Shorter wires, better connections"));
    }

    if (test_results.busy_timeouts > 0) {
        Serial.println(F("\nDIAGNOSIS: Display timeout issues"));
        Serial.println(F("SOLUTION: Increase timeout in library"));
    }
}

void runFullDiagnostic()
{
    Serial.println(F("\n========================================"));
    Serial.println(F("Running Full Diagnostic Suite"));
    Serial.println(F("========================================"));

    testColorPattern();
    delay(2000);

    testHighStressColor(); // Add the stress test
    delay(2000);

    testPowerSupply();
    delay(2000);

    // testDataLineVoltages();
    // delay(2000);

    testBusyPin();
    delay(2000);

    testSPICommunication();
    delay(2000);

    testRefreshTiming();
    delay(2000);

    testBatteryStatus(); // Add battery test
    delay(2000);

    showResults();
}

} // namespace display_debug

// =============================================================================
// MAIN SETUP AND LOOP FOR DEBUG MODE
// =============================================================================

void display_debug_setup()
{
    Serial.begin(115200);
    while (!Serial.isConnected())
        delay(10);

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

    // Ask if user wants to test pins first
    Serial.println(F("\n========================================"));
    Serial.println(F("IMPORTANT: Pin Test Options"));
    Serial.println(F("========================================"));
    Serial.println(F("Do you want to test pins BEFORE display initialization?"));
    Serial.println(F("(Once display is initialized, SPI takes control of pins)"));
    Serial.println(F("\nPress 'P' to test pins first (recommended)"));
    Serial.println(F("Press 'D' to initialize display and continue"));
    Serial.println(F("========================================"));

    while (!Serial.available())
        delay(10);
    char choice = Serial.read();
    while (Serial.available())
        Serial.read(); // Clear buffer

    if (choice == 'P' || choice == 'p') {
        Serial.println(F("\n[TEST] Testing pins WITHOUT display initialization"));
        Serial.println(F("[TEST] This allows manual control of all pins"));
        display_debug::testDataLineVoltages();
        Serial.println(F("\n[TEST] Pin test complete."));
        Serial.println(F("[TEST] Press any key to continue with display init..."));
        while (!Serial.available())
            delay(10);
        while (Serial.available())
            Serial.read(); // Clear buffer
    }

    // Initialize display
    Serial.println(F("\nInitializing display..."));
    photo_frame::renderer::init_display();

    Serial.println(F("\nDisplay initialized successfully!"));
    Serial.println(F("Ready for testing.\n"));

    display_debug::showMenu();
}

void display_debug_loop()
{
    if (Serial.available()) {
        char cmd = Serial.read();
        while (Serial.available())
            Serial.read(); // Clear buffer

        switch (cmd) {
        case '1':
            display_debug::runFullDiagnostic();
            break;

        case '2':
            display_debug::testColorPattern();
            break;

        case '3':
            display_debug::testHighStressColor();
            break;

        case '4':
            display_debug::testPowerSupply();
            break;

        case '5':
            display_debug::testDataLineVoltages();
            break;

        case '6':
            display_debug::testBusyPin();
            break;

        case '7':
            display_debug::testSPICommunication();
            break;

        case '8':
            display_debug::testRefreshTiming();
            break;

        case '9':
            display_debug::testBatteryStatus();
            break;

        case 'B':
        case 'b':
            display_debug::testColorBars();
            break;

        case 'H':
        case 'h':
            display_debug::testHardwareFailure();
            break;

        case 'G':
        case 'g':
            display_debug::testSDCardImages();
            break;

        case 'P':
        case 'p':
            Serial.println(F("\n[TEST] Releasing SPI control..."));
            // End SPI to release pin control
            SPI.end();
            // Set CS high to deselect display
            digitalWrite(EPD_CS_PIN, HIGH);
            pinMode(EPD_CS_PIN, OUTPUT);
            Serial.println(F("[TEST] SPI released. Testing pins manually..."));
            display_debug::testDataLineVoltages();
            Serial.println(F("\n[TEST] Note: Display will need restart after this test"));
            break;

        case 'R':
        case 'r':
            display_debug::showResults();
            break;

        case 'C':
        case 'c':
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