#!/usr/bin/env python3
"""
Create simplified display_debug.cpp with only essential tests
and compiled bitmap rendering.
"""

# Read the original file to extract test functions we want to keep
original_file = "../src/display_debug.cpp.backup"
output_file = "../src/display_debug.cpp"

with open(original_file, 'r') as f:
    original_content = f.read()

# Extract specific test functions (potentiometer, LED, battery, color pattern, high stress)
# We'll manually construct the new file with simplified structure

new_content = '''// Display Debug Mode - Simplified Testing Suite
// This file runs instead of main.cpp when ENABLE_DISPLAY_DIAGNOSTIC is defined
//
// Simplified version includes only:
// - Potentiometer test
// - LED test (builtin + RGB)
// - Battery status test
// - Color pattern test (6-color/7-color displays only)
// - High stress color test (6-color/7-color displays only)
// - Compiled bitmap test (from GxEPD2 library)
// - Custom color test patterns (compiled into firmware)

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
#include <Preferences.h>
#include <SPI.h>

// Include compiled bitmaps from GxEPD2 library
#include "Bitmaps800x480.h"
#include "Bitmaps7c800x480.h"

// Include custom color test patterns
#include "test_bitmaps_color.h"

// Display instance is defined in renderer.cpp
extern GxEPD2_DISPLAY_CLASS<GxEPD2_DRIVER_CLASS, MAX_HEIGHT(GxEPD2_DRIVER_CLASS)> display;
extern photo_frame::sd_card sdCard;

namespace display_debug {

// Track if display has been initialized
bool display_initialized = false;

// Helper to ensure display is initialized before tests that need it
void ensureDisplayInitialized() {
    if (!display_initialized) {
        Serial.println(F("\\n[INIT] Initializing display for this test..."));
        photo_frame::renderer::init_display();
        display_initialized = true;
        Serial.println(F("[INIT] Display initialized successfully!\\n"));
    }
}

'''

# Add potentiometer test
pot_test_start = original_content.find("void testPotentiometer()")
pot_test_end = original_content.find("void testHardwareFailure()", pot_test_start)
if pot_test_start != -1 and pot_test_end != -1:
    new_content += original_content[pot_test_start:pot_test_end]
    new_content += "\n"

# Add LED test
led_test_start = original_content.find("void testLEDs()")
led_test_end = original_content.find("// =============================================================================\n// INTERACTIVE MENU", led_test_start)
if led_test_start != -1 and led_test_end != -1:
    new_content += "// =============================================================================\n// LED TEST\n// =============================================================================\n\n"
    new_content += original_content[led_test_start:led_test_end]
    new_content += "\n"

# Add battery test
battery_test_start = original_content.find("void testBatteryStatus()")
battery_test_end = original_content.find("void testGoogleDrive()", battery_test_start)
if battery_test_start != -1 and battery_test_end != -1:
    new_content += "// =============================================================================\n// BATTERY STATUS TEST\n// =============================================================================\n\n"
    new_content += original_content[battery_test_start:battery_test_end]
    new_content += "\n"

# Add color pattern test
color_test_start = original_content.find("void testColorPattern()")
color_test_end = original_content.find("void testColorBars()", color_test_start)
if color_test_start != -1 and color_test_end != -1:
    new_content += "// =============================================================================\n// COLOR PATTERN TEST\n// =============================================================================\n\n"
    new_content += original_content[color_test_start:color_test_end]
    new_content += "\n"

# Add high stress test
stress_test_start = original_content.find("void testHighStressColor()")
stress_test_end = original_content.find("void testPowerSupply()", stress_test_start)
if stress_test_end == -1:
    # Try alternative end marker
    stress_test_end = original_content.find("// =============================================================================\n// POWER", stress_test_start)
if stress_test_start != -1 and stress_test_end != -1:
    new_content += "// =============================================================================\n// HIGH STRESS COLOR TEST\n// =============================================================================\n\n"
    new_content += original_content[stress_test_start:stress_test_end]
    new_content += "\n"

# Add new compiled bitmap tests
new_content += '''// =============================================================================
// COMPILED BITMAP TEST - GxEPD2 Library Bitmaps
// =============================================================================

void testCompiledBitmaps() {
    ensureDisplayInitialized();

    Serial.println(F("\\n[TEST] Compiled Bitmap Test"));
    Serial.println(F("-------------------------------"));
    Serial.println(F("Rendering bitmaps compiled into firmware"));
    Serial.println(F("No SD card required for this test\\n"));

    display.setFullWindow();

    // Draw first bitmap from GxEPD2 library
    Serial.println(F("[TEST] Drawing Bitmap800x480_1..."));
    display.firstPage();
    unsigned long start = millis();
    do {
        display.fillScreen(GxEPD_WHITE);
        display.drawBitmap(0, 0, Bitmap800x480_1, DISP_WIDTH, DISP_HEIGHT, GxEPD_BLACK);
    } while (display.nextPage());
    unsigned long elapsed = millis() - start;
    Serial.printf("[TEST] Rendering time: %lu ms\\n", elapsed);

    delay(3000);

    // Draw second bitmap
    Serial.println(F("[TEST] Drawing Bitmap800x480_2..."));
    display.firstPage();
    start = millis();
    do {
        display.fillScreen(GxEPD_WHITE);
        display.drawBitmap(0, 0, Bitmap800x480_2, DISP_WIDTH, DISP_HEIGHT, GxEPD_BLACK);
    } while (display.nextPage());
    elapsed = millis() - start;
    Serial.printf("[TEST] Rendering time: %lu ms\\n", elapsed);

    delay(3000);

    // Draw third bitmap
    Serial.println(F("[TEST] Drawing Bitmap800x480_3..."));
    display.firstPage();
    start = millis();
    do {
        display.fillScreen(GxEPD_WHITE);
        display.drawBitmap(0, 0, Bitmap800x480_3, DISP_WIDTH, DISP_HEIGHT, GxEPD_BLACK);
    } while (display.nextPage());
    elapsed = millis() - start;
    Serial.printf("[TEST] Rendering time: %lu ms\\n", elapsed);

    Serial.println(F("[TEST] Compiled bitmap test complete!"));
}

// =============================================================================
// COLOR TEST PATTERNS - Compiled Into Firmware
// =============================================================================

void testColorPatterns6C() {
#if defined(DISP_6C)
    ensureDisplayInitialized();

    Serial.println(F("\\n[TEST] 6-Color Test Patterns (Compiled)"));
    Serial.println(F("-------------------------------"));
    Serial.println(F("Testing 6-color rendering with compiled test patterns\\n"));

    // Test Pattern 1: Colored Squares
    Serial.println(F("[TEST] Pattern 1/4: Colored Squares (2x3 grid)"));
    display.setFullWindow();
    display.firstPage();
    unsigned long start = millis();
    do {
        // Render from compiled binary data
        for (int y = 0; y < DISP_HEIGHT; y++) {
            for (int x = 0; x < DISP_WIDTH; x++) {
                int idx = y * DISP_WIDTH + x;
                uint8_t color_byte = pgm_read_byte(&test_6c_squares[idx]);

                // Convert byte to GxEPD color
                uint16_t color = GxEPD_WHITE;
                if (color_byte == 0xE0) color = GxEPD_RED;
                else if (color_byte == 0x1C) color = GxEPD_GREEN;
                else if (color_byte == 0x03) color = GxEPD_BLUE;
                else if (color_byte == 0xFC) color = GxEPD_YELLOW;
                else if (color_byte == 0x00) color = GxEPD_BLACK;
                else if (color_byte == 0xFF) color = GxEPD_WHITE;

                display.drawPixel(x, y, color);
            }
        }
    } while (display.nextPage());
    unsigned long elapsed = millis() - start;
    Serial.printf("[TEST] Rendering time: %lu ms\\n", elapsed);
    delay(3000);

    // Test Pattern 2: Vertical Bars
    Serial.println(F("[TEST] Pattern 2/4: Vertical Color Bars"));
    display.firstPage();
    start = millis();
    do {
        for (int y = 0; y < DISP_HEIGHT; y++) {
            for (int x = 0; x < DISP_WIDTH; x++) {
                int idx = y * DISP_WIDTH + x;
                uint8_t color_byte = pgm_read_byte(&test_6c_vertical_bars[idx]);

                uint16_t color = GxEPD_WHITE;
                if (color_byte == 0xE0) color = GxEPD_RED;
                else if (color_byte == 0x1C) color = GxEPD_GREEN;
                else if (color_byte == 0x03) color = GxEPD_BLUE;
                else if (color_byte == 0xFC) color = GxEPD_YELLOW;
                else if (color_byte == 0x00) color = GxEPD_BLACK;
                else if (color_byte == 0xFF) color = GxEPD_WHITE;

                display.drawPixel(x, y, color);
            }
        }
    } while (display.nextPage());
    elapsed = millis() - start;
    Serial.printf("[TEST] Rendering time: %lu ms\\n", elapsed);
    delay(3000);

    // Test Pattern 3: Horizontal Bands
    Serial.println(F("[TEST] Pattern 3/4: Horizontal Color Bands"));
    display.firstPage();
    start = millis();
    do {
        for (int y = 0; y < DISP_HEIGHT; y++) {
            for (int x = 0; x < DISP_WIDTH; x++) {
                int idx = y * DISP_WIDTH + x;
                uint8_t color_byte = pgm_read_byte(&test_6c_horizontal_bands[idx]);

                uint16_t color = GxEPD_WHITE;
                if (color_byte == 0xE0) color = GxEPD_RED;
                else if (color_byte == 0x1C) color = GxEPD_GREEN;
                else if (color_byte == 0x03) color = GxEPD_BLUE;
                else if (color_byte == 0xFC) color = GxEPD_YELLOW;
                else if (color_byte == 0x00) color = GxEPD_BLACK;
                else if (color_byte == 0xFF) color = GxEPD_WHITE;

                display.drawPixel(x, y, color);
            }
        }
    } while (display.nextPage());
    elapsed = millis() - start;
    Serial.printf("[TEST] Rendering time: %lu ms\\n", elapsed);
    delay(3000);

    // Test Pattern 4: Checkerboard
    Serial.println(F("[TEST] Pattern 4/4: Checkerboard Pattern"));
    display.firstPage();
    start = millis();
    do {
        for (int y = 0; y < DISP_HEIGHT; y++) {
            for (int x = 0; x < DISP_WIDTH; x++) {
                int idx = y * DISP_WIDTH + x;
                uint8_t color_byte = pgm_read_byte(&test_6c_checkerboard[idx]);

                uint16_t color = GxEPD_WHITE;
                if (color_byte == 0xE0) color = GxEPD_RED;
                else if (color_byte == 0x1C) color = GxEPD_GREEN;
                else if (color_byte == 0x03) color = GxEPD_BLUE;
                else if (color_byte == 0xFC) color = GxEPD_YELLOW;
                else if (color_byte == 0x00) color = GxEPD_BLACK;
                else if (color_byte == 0xFF) color = GxEPD_WHITE;

                display.drawPixel(x, y, color);
            }
        }
    } while (display.nextPage());
    elapsed = millis() - start;
    Serial.printf("[TEST] Rendering time: %lu ms\\n", elapsed);

    Serial.println(F("\\n[TEST] All 6-color patterns displayed successfully!"));
#else
    Serial.println(F("[TEST] SKIP: This test requires a 6-color display"));
    Serial.println(F("[TEST] Current display is not configured for 6 colors"));
#endif
}

void testColorPatterns7C() {
#if defined(DISP_7C)
    ensureDisplayInitialized();

    Serial.println(F("\\n[TEST] 7-Color Test Patterns (Compiled)"));
    Serial.println(F("-------------------------------"));
    Serial.println(F("Testing 7-color rendering with compiled test patterns\\n"));

    // Test Pattern 1: Colored Squares (includes ORANGE)
    Serial.println(F("[TEST] Pattern 1/2: Colored Squares (7 colors)"));
    display.setFullWindow();
    display.firstPage();
    unsigned long start = millis();
    do {
        display.fillScreen(GxEPD_WHITE);
        display.drawImage(test_7c_squares, 0, 0, DISP_WIDTH, DISP_HEIGHT, false, false, true);
    } while (display.nextPage());
    unsigned long elapsed = millis() - start;
    Serial.printf("[TEST] Rendering time: %lu ms\\n", elapsed);
    delay(3000);

    // Test Pattern 2: Vertical Bars
    Serial.println(F("[TEST] Pattern 2/2: Vertical Color Bars (7 colors)"));
    display.firstPage();
    start = millis();
    do {
        display.fillScreen(GxEPD_WHITE);
        display.drawImage(test_7c_vertical_bars, 0, 0, DISP_WIDTH, DISP_HEIGHT, false, false, true);
    } while (display.nextPage());
    elapsed = millis() - start;
    Serial.printf("[TEST] Rendering time: %lu ms\\n", elapsed);

    Serial.println(F("\\n[TEST] All 7-color patterns displayed successfully!"));
#else
    Serial.println(F("[TEST] SKIP: This test requires a 7-color display"));
    Serial.println(F("[TEST] Current display is not configured for 7 colors"));
#endif
}

// =============================================================================
// MENU
// =============================================================================

void showMenu() {
    Serial.println(F("\\n========================================"));
    Serial.println(F("Display Debug Menu - Simplified"));
    Serial.println(F("========================================"));
    Serial.println(F("T. Test Potentiometer (Continuous Reading)"));
    Serial.println(F("L. Test LEDs (Builtin + RGB NeoPixel)"));
    Serial.println(F("B. Battery Status Test"));

#if defined(DISP_6C) || defined(DISP_7C)
    Serial.println(F("2. Color Pattern Test (Color Displays Only)"));
    Serial.println(F("3. High Stress Color Test (Color Displays Only)"));
    Serial.println(F("6. 6-Color Test Patterns (Compiled)"));
    Serial.println(F("7. 7-Color Test Patterns (Compiled)"));
#endif

    Serial.println(F("M. Compiled Bitmap Test (GxEPD2 Library)"));
    Serial.println(F("C. Clear Display"));
    Serial.println(F("0. Restart ESP32"));
    Serial.println(F("----------------------------------------"));
    Serial.println(F("Enter choice:"));
}

} // namespace display_debug

// =============================================================================
// MAIN SETUP AND LOOP FOR DEBUG MODE
// =============================================================================

void display_debug_setup() {
    Serial.begin(115200);
    while (!Serial.isConnected())
        delay(10);

    Serial.println(F("\\n\\n========================================"));
    Serial.println(F("ESP32 Photo Frame - Display Debug Mode"));
    Serial.println(F("========================================\\n"));
    Serial.println(F("ENABLE_DISPLAY_DIAGNOSTIC is defined"));
    Serial.println(F("Running in simplified debug mode\\n"));

    // Initialize basic hardware
    photo_frame::board_utils::blink_builtin_led(2, 100);

    // Show board info
    Serial.println(F("Board Information:"));
    Serial.printf("  Chip Model: %s\\n", ESP.getChipModel());
    Serial.printf("  Chip Cores: %d\\n", ESP.getChipCores());
    Serial.printf("  CPU Freq: %d MHz\\n", ESP.getCpuFreqMHz());
    Serial.printf("  Free Heap: %d bytes\\n", ESP.getFreeHeap());

    Serial.println(F("\\n========================================"));
    Serial.println(F("Display initialization is ON-DEMAND"));
    Serial.println(F("Display will be initialized when needed"));
    Serial.println(F("========================================\\n"));

    display_debug::showMenu();
}

void display_debug_loop() {
    if (Serial.available()) {
        char cmd = Serial.read();
        while (Serial.available())
            Serial.read(); // Clear buffer

        switch (cmd) {
        case 'T':
        case 't':
            display_debug::testPotentiometer();
            break;

        case 'L':
        case 'l':
            display_debug::testLEDs();
            break;

        case 'B':
        case 'b':
            display_debug::testBatteryStatus();
            break;

        case '2':
#if defined(DISP_6C) || defined(DISP_7C)
            display_debug::testColorPattern();
#else
            Serial.println(F("Color pattern test requires 6-color or 7-color display"));
#endif
            break;

        case '3':
#if defined(DISP_6C) || defined(DISP_7C)
            display_debug::testHighStressColor();
#else
            Serial.println(F("High stress test requires 6-color or 7-color display"));
#endif
            break;

        case '6':
            display_debug::testColorPatterns6C();
            break;

        case '7':
            display_debug::testColorPatterns7C();
            break;

        case 'M':
        case 'm':
            display_debug::testCompiledBitmaps();
            break;

        case 'C':
        case 'c':
            display_debug::ensureDisplayInitialized();
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
'''

# Write the new file
with open(output_file, 'w') as f:
    f.write(new_content)

print("✓ Created simplified display_debug.cpp")
print("✓ Backup saved as display_debug.cpp.backup")
print()
print("Tests included:")
print("  - Potentiometer test")
print("  - LED test (builtin + RGB)")
print("  - Battery status test")
print("  - Color pattern test (6C/7C only)")
print("  - High stress color test (6C/7C only)")
print("  - GxEPD2 compiled bitmaps test")
print("  - Custom 6-color test patterns (compiled)")
print("  - Custom 7-color test patterns (compiled)")
