// Display Debug Mode - Comprehensive Hardware Testing Suite
// This file runs instead of main.cpp when ENABLE_DISPLAY_DIAGNOSTIC is defined
//
// IMPORTANT NOTES (v0.11.0):
// - testProgmemImage() is DISABLED: requires test_image.h (24MB file removed from repository)
// - All other diagnostic tests remain fully functional
// - Use gallery test with SD card images for image rendering validation
//
// Available Tests:
// - Battery status test (MAX1704X or analog reading)
// - Google Drive integration test (WiFi/NTP/OAuth/file download)
// - Potentiometer test (continuous reading with real-time display)
// - Gallery test (BMP and BIN file support from SD card)
// - Stress test (Floyd-Steinberg dithering for B/W displays)
// - Hardware failure diagnostics (washout pattern detection)

#ifdef ENABLE_DISPLAY_DIAGNOSTIC

#include <Arduino.h>
#include "config.h"
#include "renderer.h"
#include "display_7c_helper.h"
#include "battery.h"
#include "board_util.h"
#include "string_utils.h"
#include "rgb_status.h"
#include "io_utils.h"
#include "littlefs_manager.h"
#include "sd_card.h"
#include "errors.h"
#include <Preferences.h>
#include <SPI.h>
#include <vector>
// #include "test_image.h"  // Removed: 24MB test image file not suitable for repository
#include <bitmaps/Bitmaps7c800x480.h>
#include "test_photo_6c_portrait.h"  // Portrait test image (480×800)
#include <Fonts/FreeSans9pt7b.h>
#include "google_drive_client.h"
#include "wifi_manager.h"
#include "unified_config.h"
#include <WiFi.h>

// External instances defined in main.cpp and renderer.cpp
extern GxEPD2_DISPLAY_CLASS<GxEPD2_DRIVER_CLASS, MAX_HEIGHT(GxEPD2_DRIVER_CLASS)> display;
extern photo_frame::sd_card sdCard;
extern photo_frame::battery_reader battery_reader;

namespace display_debug {

// Forward declarations
void printMenu();
void testPortraitImage();

static bool displayInitialized = false;
static bool batteryInitialized = false;
static bool sdCardInitialized = false;

// Static configuration variables (shared across display_debug functions)
static photo_frame::unified_config unifiedConfig;

// Static image buffer for Mode 1 format (allocated once in PSRAM)
static uint8_t* g_image_buffer = nullptr;
static const size_t IMAGE_BUFFER_SIZE = DISP_WIDTH * DISP_HEIGHT;  // 384,000 bytes for 800x480

/**
 * @brief Initialize the PSRAM image buffer for Mode 1 format rendering
 */
void init_image_buffer()
{
    if (g_image_buffer != nullptr) {
        log_w("[display_debug] Image buffer already initialized");
        return;
    }

    log_i("[display_debug] Initializing PSRAM image buffer...");
    log_i("[display_debug] Buffer size: %u bytes (%.2f KB)", IMAGE_BUFFER_SIZE, IMAGE_BUFFER_SIZE / 1024.0f);

#if CONFIG_SPIRAM_USE_CAPS_ALLOC || CONFIG_SPIRAM_USE_MALLOC
    // Try to allocate in PSRAM first
    g_image_buffer = (uint8_t*)heap_caps_malloc(IMAGE_BUFFER_SIZE, MALLOC_CAP_SPIRAM);

    if (g_image_buffer != nullptr) {
        log_i("[display_debug] Successfully allocated %u bytes in PSRAM for image buffer", IMAGE_BUFFER_SIZE);
        log_i("[display_debug] PSRAM free after allocation: %u bytes", ESP.getFreePsram());
    } else {
        log_w("[display_debug] Failed to allocate in PSRAM, falling back to regular heap");
        g_image_buffer = (uint8_t*)malloc(IMAGE_BUFFER_SIZE);
    }
#else
    // No PSRAM available, use regular heap
    log_w("[display_debug] PSRAM not available, using regular heap");
    g_image_buffer = (uint8_t*)malloc(IMAGE_BUFFER_SIZE);
#endif

    if (g_image_buffer == nullptr) {
        log_e("[display_debug] CRITICAL: Failed to allocate image buffer!");
        log_e("[display_debug] Free heap: %u bytes", ESP.getFreeHeap());
        log_e("[display_debug] Cannot continue without image buffer");
    } else {
        log_i("[display_debug] Image buffer initialized at address: %p", g_image_buffer);
    }
}

/**
 * @brief Free the PSRAM image buffer
 *
 * Deallocates the image buffer if it was allocated.
 * This is primarily for completeness in display_debug mode.
 */
void cleanup_image_buffer()
{
    if (g_image_buffer != nullptr) {
        log_i("[display_debug] Freeing image buffer at address: %p", g_image_buffer);
        free(g_image_buffer);
        g_image_buffer = nullptr;
        log_i("[display_debug] Image buffer freed");
    }
}

void ensureDisplayInitialized() {
    if (!displayInitialized) {
        photo_frame::renderer::init_display();
        init_image_buffer(); // Initialize PSRAM buffer
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

void ensureSDCardInitialized() {
    if (!sdCardInitialized) {
        auto error = sdCard.begin();
        if (error == photo_frame::error_type::None) {
            sdCardInitialized = true;
            log_d("SD card initialized");
        } else {
            log_e("SD card initialization failed: %d", error.code);
        }
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

    // Load configuration for refresh settings (needed for both potentiometer and default modes)
    ensureSDCardInitialized();
    if (!sdCardInitialized) {
        Serial.println(F("\nERROR: SD card not initialized - cannot load configuration"));
        return;
    }

    photo_frame::photo_frame_error_t error = photo_frame::load_unified_config_with_fallback(sdCard, CONFIG_FILEPATH, unifiedConfig);
    if (error != photo_frame::error_type::None) {
        Serial.println(F("Warning: Failed to load configuration, using defaults"));
    }

#ifdef USE_POTENTIOMETER
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
            long refreshSeconds = photo_frame::board_utils::read_refresh_seconds(unifiedConfig, false);

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
    Serial.println(F("\nPotentiometer not enabled (USE_POTENTIOMETER not defined)"));
    Serial.println(F("Using default refresh interval from config.json"));

    long defaultSeconds = unifiedConfig.board.refresh.default_seconds;
    Serial.println();
    Serial.print(F("Default refresh interval: "));
    Serial.print(defaultSeconds);
    Serial.println(F(" seconds"));

    char humanTime[64];
    photo_frame::string_utils::seconds_to_human(humanTime, sizeof(humanTime), defaultSeconds);
    Serial.print(F("                         "));
    Serial.println(humanTime);

    Serial.print(F("                         "));
    Serial.print(defaultSeconds / 60);
    Serial.println(F(" minutes"));
#endif

    Serial.println(F("\n========================================"));
}

// ============================================================================
// Test 4: SD Card Tests
// ============================================================================

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
// Test 5: Display Tests
// ============================================================================

// Test 5a: Display Color Grid Test
void testDisplayColorGrid() {
    log_i("\n========================================");
    log_i("Display Color Grid Test");
    log_i("========================================");

    ensureDisplayInitialized();

    display.setFullWindow();
    display.firstPage();

    do {
        display.fillScreen(GxEPD_WHITE);

        // Determine number of colors based on display type
        int numColors = 0;
        uint16_t colors[7];
        const char* colorNames[7];

#if defined(DISP_7C)
        numColors = 7;
        colors[0] = GxEPD_BLACK;   colorNames[0] = "Black";
        colors[1] = GxEPD_WHITE;   colorNames[1] = "White";
        colors[2] = GxEPD_RED;     colorNames[2] = "Red";
        colors[3] = GxEPD_GREEN;   colorNames[3] = "Green";
        colors[4] = GxEPD_BLUE;    colorNames[4] = "Blue";
        colors[5] = GxEPD_YELLOW;  colorNames[5] = "Yellow";
        colors[6] = GxEPD_ORANGE;  colorNames[6] = "Orange";
#elif defined(DISP_6C)
        numColors = 6;
        colors[0] = GxEPD_BLACK;   colorNames[0] = "Black";
        colors[1] = GxEPD_WHITE;   colorNames[1] = "White";
        colors[2] = GxEPD_RED;     colorNames[2] = "Red";
        colors[3] = GxEPD_GREEN;   colorNames[3] = "Green";
        colors[4] = GxEPD_BLUE;    colorNames[4] = "Blue";
        colors[5] = GxEPD_YELLOW;  colorNames[5] = "Yellow";
#else // BW
        numColors = 2;
        colors[0] = GxEPD_BLACK;   colorNames[0] = "Black";
        colors[1] = GxEPD_WHITE;   colorNames[1] = "White";
#endif

        // Calculate grid layout
        int cols = (numColors > 4) ? 3 : 2;
        int rows = (numColors + cols - 1) / cols;
        int cellWidth = DISP_WIDTH / cols;
        int cellHeight = DISP_HEIGHT / rows;

        // Draw color grid
        for (int i = 0; i < numColors; i++) {
            int col = i % cols;
            int row = i / cols;
            int x = col * cellWidth;
            int y = row * cellHeight;

            // Fill cell with color
            display.fillRect(x, y, cellWidth, cellHeight, colors[i]);

            // Draw border
            display.drawRect(x, y, cellWidth, cellHeight, GxEPD_BLACK);

            // Draw color name
            display.setTextColor(colors[i] == GxEPD_WHITE || colors[i] == GxEPD_YELLOW ? GxEPD_BLACK : GxEPD_WHITE);
            display.setCursor(x + 10, y + cellHeight / 2);
            display.print(colorNames[i]);
        }

    } while (display.nextPage());

    Serial.println(F("\nColor grid displayed on e-paper"));
    Serial.println(F("Press any key to continue..."));
}

// Test 5b: Display Pattern Test
void testDisplayPatterns() {
    log_i("\n========================================");
    log_i("Display Pattern Test");
    log_i("========================================");

    ensureDisplayInitialized();

    // Pattern 1: Horizontal stripes
    Serial.println(F("\nPattern 1: Horizontal Stripes"));
    display.setFullWindow();
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        for (int y = 0; y < DISP_HEIGHT; y += 20) {
            display.fillRect(0, y, DISP_WIDTH, 10, GxEPD_BLACK);
        }
    } while (display.nextPage());

    delay(2000);

    // Pattern 2: Vertical stripes
    Serial.println(F("Pattern 2: Vertical Stripes"));
    display.setFullWindow();
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        for (int x = 0; x < DISP_WIDTH; x += 20) {
            display.fillRect(x, 0, 10, DISP_HEIGHT, GxEPD_BLACK);
        }
    } while (display.nextPage());

    delay(2000);

    // Pattern 3: Checkerboard
    Serial.println(F("Pattern 3: Checkerboard"));
    display.setFullWindow();
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        int cellSize = 40;
        for (int y = 0; y < DISP_HEIGHT; y += cellSize) {
            for (int x = 0; x < DISP_WIDTH; x += cellSize) {
                if ((x / cellSize + y / cellSize) % 2 == 0) {
                    display.fillRect(x, y, cellSize, cellSize, GxEPD_BLACK);
                }
            }
        }
    } while (display.nextPage());

    delay(2000);

    // Pattern 4: Concentric circles
    Serial.println(F("Pattern 4: Concentric Circles"));
    display.setFullWindow();
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        int centerX = DISP_WIDTH / 2;
        int centerY = DISP_HEIGHT / 2;
        int maxRadius = min(centerX, centerY);
        for (int r = maxRadius; r > 0; r -= 20) {
            display.drawCircle(centerX, centerY, r, GxEPD_BLACK);
        }
    } while (display.nextPage());

    log_i("Pattern test complete");
    Serial.println(F("\nAll patterns displayed"));
    Serial.println(F("Press any key to continue..."));
}

// Test 5c: Display Images from SD Card
void testDisplayImages() {
    log_i("\n========================================");
    log_i("Display Images Test");
    log_i("========================================");

    ensureDisplayInitialized();

    // Initialize SD card if needed
    if (!sdCard.is_initialized()) {
        log_i("Initializing SD card...");
        auto initError = sdCard.begin();
        if (initError != photo_frame::error_type::None) {
            log_e("Failed to initialize SD card: %d", initError.code);
            Serial.println(F("ERROR: SD card initialization failed"));
            return;
        }
    }

    // Determine folder based on display type
    String basePath = "/";
#if defined(DISP_7C)
    basePath += "7c";
#elif defined(DISP_6C)
    basePath += "6c";
#else
    basePath += "bw";
#endif

    // Check if base directory exists
    if (!sdCard.is_directory(basePath.c_str())) {
        log_e("Base directory not found: %s", basePath.c_str());
        Serial.print(F("ERROR: Base directory not found: "));
        Serial.println(basePath);
        Serial.println(F("Please create the directory on the SD card"));
        return;
    }

    // Ask user for format
    Serial.println(F("\nSelect image format:"));
    Serial.println(F("1 - Binary (.bin)"));
    Serial.println(F("2 - Bitmap (.bmp)"));
    Serial.println(F("0 - Back"));
    Serial.print(F("Choice: "));

    while (!Serial.available()) delay(10);
    String choice = Serial.readStringUntil('\n');
    choice.trim();
    Serial.println(choice);

    if (choice == "0") return;

    String format = (choice == "1") ? "bin" : "bmp";
    String fullPath = basePath + "/" + format;

    Serial.print(F("Looking for images in: "));
    Serial.println(fullPath);

    // Check if format directory exists
    if (!sdCard.is_directory(fullPath.c_str())) {
        log_e("Format directory not found: %s", fullPath.c_str());
        Serial.print(F("ERROR: Directory not found: "));
        Serial.println(fullPath);
        Serial.println(F("Please create the directory on the SD card"));
        return;
    }

    // Scan directory for images
    fs::File dir = sdCard.open(fullPath.c_str(), FILE_READ);
    if (!dir || !dir.isDirectory()) {
        log_e("Failed to open directory");
        Serial.println(F("ERROR: Failed to open directory"));
        return;
    }

    // Count and collect image files
    std::vector<String> imageFiles;
    fs::File file = dir.openNextFile();
    while (file) {
        if (!file.isDirectory()) {
            String filename = String(file.name());

            // Skip macOS hidden files (starting with ._ or .)
            if (filename.startsWith("._") || filename.startsWith("/.")) {
                file = dir.openNextFile();
                continue;
            }

            // Extract just the filename without path
            int lastSlash = filename.lastIndexOf('/');
            if (lastSlash >= 0) {
                filename = filename.substring(lastSlash + 1);
            }

            if (filename.endsWith("." + format)) {
                imageFiles.push_back(filename);
            }
        }
        file = dir.openNextFile();
    }
    dir.close();

    if (imageFiles.empty()) {
        log_w("No %s files found in %s", format.c_str(), fullPath.c_str());
        Serial.print(F("No "));
        Serial.print(format);
        Serial.print(F(" files found in "));
        Serial.println(fullPath);
        return;
    }

    Serial.print(F("Found "));
    Serial.print(imageFiles.size());
    Serial.print(F(" "));
    Serial.print(format);
    Serial.println(F(" files"));
    Serial.println();

    // Render images one by one
    for (size_t i = 0; i < imageFiles.size(); i++) {
        String imagePath = fullPath + "/" + imageFiles[i];

        Serial.println(F("========================================"));
        Serial.print(F("Image "));
        Serial.print(i + 1);
        Serial.print(F(" of "));
        Serial.println(imageFiles.size());
        Serial.print(F("File: "));
        Serial.println(imageFiles[i]);
        Serial.println(F("========================================"));

        log_i("Rendering image %zu/%zu: %s", i + 1, imageFiles.size(), imagePath.c_str());

        // Open the file
        fs::File imgFile = sdCard.open(imagePath.c_str(), FILE_READ);
        if (!imgFile) {
            log_e("Failed to open file: %s", imagePath.c_str());
            Serial.print(F("ERROR: Failed to open file: "));
            Serial.println(imagePath);
            continue;
        }

        // Render the image using drawDemoBitmap for bin files on 6C displays
        bool success = false;
        display.setFullWindow();

        if (format == "bin") {
#if defined(DISP_6C)
            // For 6-color displays, use drawDemoBitmap() directly
            // Read entire file into buffer (expected: 384000 bytes, 1 byte per pixel)
            size_t expectedSize = DISP_WIDTH * DISP_HEIGHT;
            size_t fileSize = imgFile.size();

            log_i("File size: %d bytes, expected: %d bytes", fileSize, expectedSize);

            if (fileSize == expectedSize) {
                // Allocate buffer in PSRAM
                uint8_t* buffer = nullptr;
#ifdef BOARD_HAS_PSRAM
                buffer = (uint8_t*)ps_malloc(expectedSize);
                if (buffer) {
                    log_i("Allocated buffer in PSRAM");
                }
#endif
                if (!buffer) {
                    buffer = (uint8_t*)malloc(expectedSize);
                    if (buffer) {
                        log_i("Allocated buffer in regular heap");
                    }
                }

                if (buffer) {
                    // Read file
                    imgFile.seek(0);
                    size_t bytesRead = imgFile.read(buffer, expectedSize);

                    if (bytesRead == expectedSize) {
                        // Render using drawDemoBitmap with mode 1
                        auto renderStart = millis();
                        display.epd2.drawDemoBitmap(buffer, 0, 0, 0, DISP_WIDTH, DISP_HEIGHT, 1, false, false);
                        auto renderTime = millis() - renderStart;
                        log_i("Rendered in %lu ms using drawDemoBitmap mode 1", renderTime);
                        success = true;
                    } else {
                        log_e("Read %d bytes, expected %d", bytesRead, expectedSize);
                    }

                    free(buffer);
                } else {
                    log_e("Failed to allocate buffer");
                }
            } else {
                log_e("File size mismatch: %d bytes (expected %d for standard format)", fileSize, expectedSize);
            }
#else
            // For other displays, use standard binary renderer
            success = (photo_frame::renderer::draw_binary_from_file_paged(imgFile, imagePath.c_str(), DISP_WIDTH, DISP_HEIGHT) > 0);
#endif
        } else {
            // Bitmap format - DEPRECATED: draw_bitmap_from_file_buffered has been removed
            Serial.println(F("BMP format not supported in this test (use BIN format)"));
            success = false;
        }

        imgFile.close();

        if (!success) {
            log_e("Failed to render image");
            Serial.println(F("ERROR: Failed to render image"));
        } else {
            log_i("Image rendered successfully");
            Serial.println(F("Image rendered successfully"));
        }

        // Ask user to continue or stop
        Serial.println();
        Serial.println(F("n - Next image"));
        Serial.println(F("q - Quit test"));
        Serial.print(F("Choice: "));

        while (!Serial.available()) delay(10);
        String userChoice = Serial.readStringUntil('\n');
        userChoice.trim();
        userChoice.toLowerCase();
        Serial.println(userChoice);

        if (userChoice == "q" || userChoice == "quit") {
            log_i("Image test cancelled by user");
            Serial.println(F("\nTest cancelled"));
            break;
        }

        Serial.println();
    }

    log_i("Image test complete - rendered %zu images", imageFiles.size());
    Serial.println(F("\n========================================"));
    Serial.println(F("Image test complete"));
    Serial.println(F("========================================"));
}

// Test 5d: Clear Display Screen
void testClearScreen() {
    log_i("\n========================================");
    log_i("Clear Display Screen");
    log_i("========================================");

    ensureDisplayInitialized();

    Serial.println(F("\nClearing display..."));

    display.setFullWindow();
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
    } while (display.nextPage());

    log_i("Display cleared");
    Serial.println(F("Display cleared to white"));
}

// Test 5e: PROGMEM Image Test (with overlays like main.cpp)
// NOTE: Disabled - requires test_image.h (24MB file removed from repository)
void testProgmemImage() {
    Serial.println(F("\n========================================"));
    Serial.println(F("PROGMEM Image Test - DISABLED"));
    Serial.println(F("========================================"));
    Serial.println(F("This test requires test_image.h which was removed from the repository"));
    Serial.println(F("(24MB file not suitable for git repository)"));
    Serial.println(F("Press any key to continue..."));
    return;

    /* DISABLED CODE - requires test_image.h
    log_i("\n========================================");
    log_i("PROGMEM Image Test WITH OVERLAYS");
    log_i("========================================");

    ensureDisplayInitialized();

    Serial.println(F("\nTesting embedded PROGMEM image with overlays..."));
    Serial.print(F("Image size: "));
    Serial.print(test_image_data_size);
    Serial.println(F(" bytes (standard format, 1 byte per pixel)"));

    // NEW APPROACH: Use GFXcanvas8 with overloaded renderer functions (same as main.cpp)
    Serial.println(F("Using GFXcanvas8 with overloaded renderer functions..."));

    // Allocate and copy image to modifiable buffer
    uint8_t* modifiedBuffer = nullptr;
#if CONFIG_SPIRAM_USE_CAPS_ALLOC || CONFIG_SPIRAM_USE_MALLOC
    modifiedBuffer = (uint8_t*)heap_caps_malloc(IMAGE_BUFFER_SIZE, MALLOC_CAP_SPIRAM);
#else
    modifiedBuffer = (uint8_t*)malloc(IMAGE_BUFFER_SIZE);
#endif

    if (!modifiedBuffer) {
        Serial.println(F("ERROR: Failed to allocate buffer!"));
        return;
    }

    auto renderStart = millis();

    // Copy image from PROGMEM
    memcpy_P(modifiedBuffer, test_image_data, IMAGE_BUFFER_SIZE);

    // STEP 1: Create GFXcanvas8 pointing to our buffer
    GFXcanvas8 canvas(DISP_WIDTH, DISP_HEIGHT, false);
    uint8_t** bufferPtr = (uint8_t**)((uint8_t*)&canvas + sizeof(Adafruit_GFX));
    *bufferPtr = modifiedBuffer;

    // STEP 2: Draw white overlay bars on buffer for status areas
    Serial.println(F("Drawing white overlay bars..."));
    canvas.fillRect(0, 0, DISP_WIDTH, 16, 0xFF); // Top white bar

    // STEP 3: Create fake data for overlays
    Serial.println(F("Creating fake data for overlays..."));

    // Fake date/time (December 31, 2025 at 15:30)
    DateTime fake_now(2025, 12, 31, 15, 30, 0);
    long fake_refresh_seconds = 1800; // 30 minutes

    // Fake battery info
    photo_frame::battery_info_t fake_battery;
    fake_battery.percent = 85.5f;
    fake_battery.millivolts = 4150;
#ifndef USE_SENSOR_MAX1704X
    fake_battery.raw_value = 4150;
    fake_battery.raw_millivolts = 4150;
#else
    fake_battery.cell_voltage = 4.15f;
    fake_battery.charge_rate = 0.0f;
#endif

    // Fake image info
    uint32_t fake_image_index = 42;
    uint32_t fake_total_images = 123;
    photo_frame::image_source_t fake_source = photo_frame::IMAGE_SOURCE_CLOUD;


    // STEP 4: Draw overlays with icons using canvas-based renderer functions
    Serial.println(F("Drawing overlays with renderer functions..."));
    photo_frame::renderer::draw_last_update(canvas, fake_now, fake_refresh_seconds);
    photo_frame::renderer::draw_image_info(canvas, fake_image_index, fake_total_images, fake_source);
    photo_frame::renderer::draw_battery_status(canvas, fake_battery);

    // STEP 5: Render the modified buffer to display hardware
    Serial.println(F("Writing modified image to display..."));
    display.setFullWindow();
    delay(500);
    display.epd2.writeDemoBitmap(modifiedBuffer, 0, 0, 0, DISP_WIDTH, DISP_HEIGHT, 1, false, false);
    // STEP 6: Refresh display
    Serial.println(F("Refreshing display..."));
    display.refresh();

    auto renderTime = millis() - renderStart;

    // Cleanup (canvas destructor won't free buffer because buffer_owned=false)
    free(modifiedBuffer);

    log_i("Total render time: %lu ms", renderTime);

    Serial.println(F("\nPROGMEM image with GFXcanvas8 overlays rendered!"));
    Serial.println(F("Check if text is visible on the white bar!"));
    Serial.println(F("Press any key to continue..."));
    */ // End of DISABLED CODE
}

// Test 5g: Portrait Image Test with Rotation
void testPortraitImage() {
    log_i("\n========================================");
    log_i("Portrait Image Test WITH ROTATION AND STATUSBAR");
    log_i("========================================");

    ensureDisplayInitialized();
    ensureBatteryInitialized();

    Serial.println(F("\nTesting PRE-ROTATED portrait image (800×480 already rotated)..."));
    Serial.print(F("Image size: "));
    Serial.print(sizeof(test_photo_6c_portrait));
    Serial.println(F(" bytes"));
    Serial.print(F("Expected: "));
    Serial.print(800 * 480);
    Serial.println(F(" bytes (800×480 pre-rotated for portrait display)"));

    // Copy from PROGMEM to RAM buffer
    // Image is ALREADY 800×480 (pre-rotated by Rust processor)
    uint8_t* imageBuffer = nullptr;
#if CONFIG_SPIRAM_USE_CAPS_ALLOC || CONFIG_SPIRAM_USE_MALLOC
    imageBuffer = (uint8_t*)heap_caps_malloc(800 * 480, MALLOC_CAP_SPIRAM);
    if (imageBuffer) {
        Serial.println(F("Allocated image buffer in PSRAM"));
    }
#else
    imageBuffer = (uint8_t*)malloc(800 * 480);
#endif

    if (!imageBuffer) {
        Serial.println(F("ERROR: Failed to allocate image buffer!"));
        return;
    }

    Serial.println(F("Copying PRE-ROTATED image from PROGMEM to RAM..."));
    memcpy_P(imageBuffer, test_photo_6c_portrait, 800 * 480);

    // Create fake data for statusbar overlays
    Serial.println(F("Creating fake data for statusbar overlays..."));

    // Fake date/time (January 3, 2026 at 14:30)
    DateTime fake_now(2026, 1, 3, 14, 30, 0);
    long fake_refresh_seconds = 1800; // 30 minutes

    // Fake battery info
    photo_frame::battery_info_t fake_battery;
    fake_battery.percent = 78.5f;
    fake_battery.millivolts = 4050;
#ifndef USE_SENSOR_MAX1704X
    fake_battery.raw_value = 4050;
    fake_battery.raw_millivolts = 4050;
#else
    fake_battery.cell_voltage = 4.05f;
    fake_battery.charge_rate = 0.0f;
#endif

    // Fake image info
    uint32_t fake_image_index = 12;
    uint32_t fake_total_images = 87;
    photo_frame::image_source_t fake_source = photo_frame::IMAGE_SOURCE_CLOUD;

    Serial.println(F("Test parameters:"));
    Serial.print(F("  Battery: ")); Serial.print(fake_battery.percent); Serial.println(F("%"));
    Serial.print(F("  Time: ")); Serial.println(fake_now.timestamp(DateTime::TIMESTAMP_FULL).c_str());
    Serial.print(F("  Refresh: ")); Serial.print(fake_refresh_seconds); Serial.println(F(" seconds"));
    Serial.print(F("  Images: ")); Serial.print(fake_image_index); Serial.print(F(" / ")); Serial.println(fake_total_images);

    // Image is ALREADY pre-rotated to 800×480 by Rust processor
    // Apply overlay directly on landscape buffer (RIGHT side statusbar)
    Serial.println(F("Creating canvas for landscape overlay (image already rotated)..."));
    GFXcanvas8 landscapeCanvas(HW_WIDTH, HW_HEIGHT, false);
    uint8_t** landscapeBufferPtr = (uint8_t**)((uint8_t*)&landscapeCanvas + sizeof(Adafruit_GFX));
    *landscapeBufferPtr = imageBuffer;

    // Draw white bar on RIGHT side of landscape (16 pixels wide, 480 pixels tall)
    // This is where the statusbar appears on portrait-mode displays
    Serial.println(F("Drawing statusbar on RIGHT side of landscape..."));
    landscapeCanvas.fillRect(HW_WIDTH - 16, 0, 16, HW_HEIGHT, 0xFF);

    // Set rotation for vertical text rendering
    // Rotation 1 = 90° CW makes text read from bottom to top (correct for RIGHT side)
    landscapeCanvas.setRotation(1);

    // Draw overlays - they will appear vertically on the right side
    Serial.println(F("Drawing statusbar overlays on landscape canvas..."));
    photo_frame::renderer::draw_last_update(landscapeCanvas, fake_now, fake_refresh_seconds);
    photo_frame::renderer::draw_image_info(landscapeCanvas, fake_image_index, fake_total_images, fake_source);
    photo_frame::renderer::draw_battery_status(landscapeCanvas, fake_battery);

    // Clear display
    display.setFullWindow();
    delay(500);

    // Write the pre-rotated buffer with overlays directly to display
    Serial.println(F("Writing pre-rotated image with overlays to display..."));
    auto renderStart = millis();

#if defined(DISP_6C) || defined(DISP_7C)
    display.epd2.writeDemoBitmap(imageBuffer, 0, 0, 0, HW_WIDTH, HW_HEIGHT, 1, false, false);
#else
    Serial.println(F("ERROR: This test requires 6C or 7C display!"));
    free(imageBuffer);
    return;
#endif

    // Refresh display
    Serial.println(F("Refreshing display..."));
    display.refresh();

    auto renderTime = millis() - renderStart;

    // Cleanup
    free(imageBuffer);

    log_i("Total render time: %lu ms", renderTime);

    Serial.println(F("\nPre-rotated portrait image rendered with statusbar!"));
    Serial.println(F("The image was already rotated 90° CW by Rust processor"));
    Serial.println(F("No runtime rotation needed - direct display!"));
    Serial.println(F("Check if statusbar text is visible on the white bar!"));
    Serial.println(F("Press any key to continue..."));
}

// Test 5f: Official Library Image Test
void testOfficialLibraryImage() {
    log_i("\n========================================");
    log_i("Official Library Image Test");
    log_i("========================================");

    ensureDisplayInitialized();

    Serial.println(F("\nTesting official GxEPD2 library image..."));
    Serial.println(F("Image: Bitmap7c800x480 (384000 bytes, standard format)"));

    // Render using drawDemoBitmap() as used in the official example
    // This is the special format for 6/7-color ACeP displays
    Serial.println(F("Rendering image using drawDemoBitmap()..."));

    auto renderStart = millis();
    // drawDemoBitmap(bitmap, x1, y1, x2, w, h, mode, invert, pgm)
    // mode 0 = special format, pgm = true for PROGMEM
    display.epd2.drawDemoBitmap(Bitmap7c800x480, 0, 0, 0, 800, 480, 0, false, true);
    auto renderTime = millis() - renderStart;

    log_i("Official library image test complete!");
    log_i("  Render time: %lu ms", renderTime);

    Serial.println(F("\nOfficial library image rendered successfully"));
    Serial.println(F("Press any key to continue..."));


}

// ============================================================================
// Test 6.7: Error Rendering Test
// ============================================================================
void testErrorRendering() {
    log_i("\n========================================");
    log_i("Error Rendering Test");
    log_i("========================================");

    ensureDisplayInitialized();
    ensureBatteryInitialized();

    Serial.println(F("\nThis test simulates error screen rendering"));
    Serial.println(F("It will test the display without any SD card or Google Drive operations"));
    Serial.println(F("\nRendering error screen..."));

    // Create fake data for testing
    photo_frame::battery_info_t fake_battery;
    fake_battery.percent = 75.0f;
    fake_battery.millivolts = 3950;
#ifdef USE_SENSOR_MAX1704X
    fake_battery.cell_voltage = 3.95f;
    fake_battery.charge_rate = 0.0f;
#else
    fake_battery.raw_value = 0;
    fake_battery.raw_millivolts = 3950;
#endif

    DateTime fake_now(2025, 12, 30, 19, 14, 6);
    long fake_refresh_seconds = 3600; // 1 hour

    uint32_t fake_image_index = 5;
    uint32_t fake_total_files = 142;

    // Test error: HTTP POST Failed (same as in your log)
    photo_frame::photo_frame_error_t test_error = photo_frame::error_type::HttpPostFailed;

    Serial.println(F("Test parameters:"));
    Serial.print(F("  Error: ")); Serial.println(test_error.message);
    Serial.print(F("  Error code: ")); Serial.println(test_error.code);
    Serial.print(F("  Battery: ")); Serial.print(fake_battery.percent); Serial.println(F("%"));
    Serial.print(F("  Time: ")); Serial.println(fake_now.timestamp(DateTime::TIMESTAMP_FULL).c_str());
    Serial.print(F("  Refresh: ")); Serial.print(fake_refresh_seconds); Serial.println(F(" seconds"));
    Serial.print(F("  Images: ")); Serial.print(fake_image_index); Serial.print(F(" / ")); Serial.println(fake_total_files);

    // Clear screen first
    display.setFullWindow();
    display.fillScreen(GxEPD_WHITE);
    delay(500);

    Serial.println(F("\nRendering error page (with paging)..."));

    // Render error screen using the same method as main.cpp
    display.firstPage();
    do {
        photo_frame::renderer::draw_error(test_error);
        photo_frame::renderer::draw_error_message(gravity_t::TOP_RIGHT, test_error.code);

        display.writeFillRect(0, 0, display.width(), 16, GxEPD_WHITE);
        photo_frame::renderer::draw_last_update(fake_now, fake_refresh_seconds);
        photo_frame::renderer::draw_image_info(fake_image_index, fake_total_files, photo_frame::IMAGE_SOURCE_CLOUD);
        photo_frame::renderer::draw_battery_status(fake_battery);
    } while (display.nextPage());

    Serial.println(F("\nError screen rendered!"));
    Serial.println(F("Check the display - you should see:"));
    Serial.println(F("  - Error icon and message"));
    Serial.println(F("  - Error code in top-right"));
    Serial.println(F("  - Last update time in top-left"));
    Serial.println(F("  - Image info and battery status"));
    Serial.println(F("\nPress any key to continue..."));
}

// Display Tests Menu
void displayTestsMenu() {
    while (true) {
        Serial.println(F("\n========================================"));
        Serial.println(F("Display Tests Menu"));
        Serial.println(F("========================================"));
        Serial.println(F("1 - Color Grid Test"));
        Serial.println(F("2 - Pattern Test"));
        Serial.println(F("3 - Image Test (from SD)"));
        Serial.println(F("4 - Clear Screen"));
        Serial.println(F("5 - PROGMEM Image Test (our native format)"));
        Serial.println(F("6 - Official Library Image Test (standard format)"));
        Serial.println(F("7 - Error Rendering Test (no SD/Drive needed)"));
        Serial.println(F("8 - Portrait Image Test (480×800 with rotation)"));
        Serial.println(F(""));
        Serial.println(F("0 - Back to Main Menu"));
        Serial.println(F("========================================"));
        Serial.println(F("Enter your choice:"));

        while (!Serial.available()) delay(10);
        String input = Serial.readStringUntil('\n');
        input.trim();
        Serial.println(input);

        if (input == "1") {
            testDisplayColorGrid();
            while (!Serial.available()) delay(10);
            Serial.readStringUntil('\n'); // Clear input
        } else if (input == "2") {
            testDisplayPatterns();
            while (!Serial.available()) delay(10);
            Serial.readStringUntil('\n'); // Clear input
        } else if (input == "3") {
            testDisplayImages();
        } else if (input == "4") {
            testClearScreen();
        } else if (input == "5") {
            testProgmemImage();
            while (!Serial.available()) delay(10);
            Serial.readStringUntil('\n'); // Clear input
        } else if (input == "6") {
            testOfficialLibraryImage();
            while (!Serial.available()) delay(10);
            Serial.readStringUntil('\n'); // Clear input
        } else if (input == "7") {
            testErrorRendering();
            while (!Serial.available()) delay(10);
            Serial.readStringUntil('\n'); // Clear input
        } else if (input == "8") {
            testPortraitImage();
            while (!Serial.available()) delay(10);
            Serial.readStringUntil('\n'); // Clear input
        } else if (input == "0") {
            return;
        } else {
            Serial.print(F("Unknown command: "));
            Serial.println(input);
            Serial.println(F("Press '0' for main menu"));
        }
    }
}

// ============================================================================
// Test 6: Deep Sleep Test
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
// Test 7: Google Drive Integration Test
// ============================================================================

static photo_frame::google_drive_client* gDriveClient = nullptr;
static photo_frame::wifi_manager wifiManager;
static photo_frame::google_drive_client_config clientConfig;  // Must be static to persist
static bool googleDriveClientInitialized = false;

bool ensureGoogleDriveClientInitialized() {
    if (!googleDriveClientInitialized) {
        log_i("[Setup] Initializing Google Drive Client test...");

        // Initialize SD card if not already initialized
        ensureSDCardInitialized();
        if (!sdCard.is_initialized()) {
            log_e("[Setup] ERROR: SD card not initialized");
            return false;
        }

        // Load unified configuration
        log_i("[Setup] Loading configuration...");
        auto error = photo_frame::load_unified_config_with_fallback(sdCard, CONFIG_FILEPATH, unifiedConfig);
        if (error != photo_frame::error_type::None) {
            log_e("[Setup] ERROR: Failed to load configuration");
            return false;
        }

        // Validate WiFi credentials
        if (unifiedConfig.wifi.network_count == 0 ||
            unifiedConfig.wifi.networks[0].ssid.isEmpty() ||
            unifiedConfig.wifi.networks[0].password.isEmpty()) {
            log_e("[WiFi] ERROR: WiFi credentials not found in config.json");
            log_e("[WiFi] Please configure WiFi in /config.json on SD card");
            return false;
        }

        // Initialize WiFi manager
        log_i("[WiFi] Initializing WiFi manager...");
        error = wifiManager.init_with_config(unifiedConfig.wifi.networks[0].ssid, unifiedConfig.wifi.networks[0].password);
        if (error != photo_frame::error_type::None) {
            log_e("[WiFi] ERROR: Failed to initialize WiFi manager");
            return false;
        }

        // Connect to WiFi
        log_i("[WiFi] Connecting to: %s", unifiedConfig.wifi.networks[0].ssid.c_str());

        error = wifiManager.connect();
        if (error != photo_frame::error_type::None) {
            log_e("[WiFi] ERROR: Failed to connect");
            log_e("[WiFi] Error code: %d", error.code);
            return false;
        }

        log_i("[WiFi] Connected successfully");
        log_i("[WiFi] IP Address: %s", WiFi.localIP().toString().c_str());

        // Synchronize time via NTP (required for JWT token generation)
        log_i("[NTP] Synchronizing time...");
        photo_frame::photo_frame_error_t ntpError;
        wifiManager.fetch_datetime(&ntpError);
        if (ntpError != photo_frame::error_type::None) {
            log_w("[NTP] WARNING: Failed to sync time via NTP");
            log_w("[NTP] JWT token generation may fail");
        } else {
            log_i("[NTP] Time synchronized successfully");
            // Display current time
            time_t now = time(NULL);
            log_i("[NTP] Current time (Unix): %ld", now);
        }

        // Validate Google Drive credentials
        if (unifiedConfig.google_drive.auth.service_account_email.isEmpty()) {
            log_e("[GDrive] ERROR: Service account email is empty");
            return false;
        }
        if (unifiedConfig.google_drive.auth.private_key_pem.isEmpty()) {
            log_e("[GDrive] ERROR: Private key PEM is empty");
            return false;
        }
        if (unifiedConfig.google_drive.auth.client_id.isEmpty()) {
            log_e("[GDrive] ERROR: Client ID is empty");
            return false;
        }

        // Populate Google Drive client config (using static variable to ensure it persists)
        clientConfig.serviceAccountEmail = unifiedConfig.google_drive.auth.service_account_email.c_str();
        clientConfig.privateKeyPem = unifiedConfig.google_drive.auth.private_key_pem.c_str();
        clientConfig.clientId = unifiedConfig.google_drive.auth.client_id.c_str();
        clientConfig.useInsecureTls = unifiedConfig.google_drive.drive.use_insecure_tls;
        clientConfig.rateLimitWindowSeconds = unifiedConfig.google_drive.rate_limiting.rate_limit_window_seconds;
        clientConfig.minRequestDelayMs = unifiedConfig.google_drive.rate_limiting.min_request_delay_ms;
        clientConfig.maxRetryAttempts = unifiedConfig.google_drive.rate_limiting.max_retry_attempts;
        clientConfig.backoffBaseDelayMs = unifiedConfig.google_drive.rate_limiting.backoff_base_delay_ms;
        clientConfig.maxWaitTimeMs = unifiedConfig.google_drive.rate_limiting.max_wait_time_ms;

        // Validate config pointers
        if (!clientConfig.serviceAccountEmail || strlen(clientConfig.serviceAccountEmail) == 0) {
            log_e("[GDrive Client] ERROR: Service account email pointer is NULL or empty");
            return false;
        }
        if (!clientConfig.privateKeyPem || strlen(clientConfig.privateKeyPem) == 0) {
            log_e("[GDrive Client] ERROR: Private key PEM pointer is NULL or empty");
            return false;
        }
        if (!clientConfig.clientId || strlen(clientConfig.clientId) == 0) {
            log_e("[GDrive Client] ERROR: Client ID pointer is NULL or empty");
            return false;
        }

        log_d("[GDrive Client] Service account: %s", clientConfig.serviceAccountEmail);
        log_d("[GDrive Client] Client ID: %s", clientConfig.clientId);
        log_d("[GDrive Client] Private key length: %zu bytes", strlen(clientConfig.privateKeyPem));
        log_d("[GDrive Client] Private key first 60 chars: %.60s", clientConfig.privateKeyPem);
        log_d("[GDrive Client] Config pointers - email: %p, key: %p, clientId: %p",
              clientConfig.serviceAccountEmail, clientConfig.privateKeyPem, clientConfig.clientId);

        // Create Google Drive client instance
        log_i("[GDrive Client] Creating client...");
        gDriveClient = new photo_frame::google_drive_client(clientConfig);

        log_i("[GDrive Client] Initialized successfully");
        googleDriveClientInitialized = true;
    }

    return googleDriveClientInitialized;
}

void testGoogleDriveAuth() {
    Serial.println(F("\n========================================"));
    Serial.println(F("Google Drive Authentication Test"));
    Serial.println(F("========================================"));

    if (!ensureGoogleDriveClientInitialized()) {
        Serial.println(F("\nERROR: Google Drive client initialization failed"));
        return;
    }

    // Test authentication by acquiring an access token
    Serial.println(F("\n[GDrive] Testing authentication..."));
    Serial.println(F("[GDrive] Acquiring OAuth2 access token via JWT..."));

    auto startTime = millis();

    // Call get_access_token which will create JWT and exchange it for an access token
    photo_frame::photo_frame_error_t error = gDriveClient->get_access_token();

    auto elapsedTime = millis() - startTime;

    if (error == photo_frame::error_type::None) {
        Serial.println(F("\n✓ Authentication successful!"));
        Serial.println(F("  Access token acquired successfully"));
        Serial.print(F("  Time taken: "));
        Serial.print(elapsedTime);
        Serial.println(F(" ms"));

        // Get token info
        const photo_frame::google_drive_access_token* token = gDriveClient->get_access_token_value();
        if (token && token->accessToken[0] != '\0') {
            Serial.print(F("  Token obtained at: "));
            Serial.println(token->obtainedAt);
            Serial.print(F("  Token expires at: "));
            Serial.println(token->expiresAt);
            Serial.print(F("  Token valid for: "));
            Serial.print((token->expiresAt - token->obtainedAt) / 60);
            Serial.println(F(" minutes"));

            // Show first/last few characters of token for verification (security)
            String tokenStr = String(token->accessToken);
            if (tokenStr.length() > 20) {
                Serial.print(F("  Token preview: "));
                Serial.print(tokenStr.substring(0, 10));
                Serial.print(F("..."));
                Serial.println(tokenStr.substring(tokenStr.length() - 10));
            }
        }
    } else {
        Serial.println(F("\n✗ Authentication failed!"));
        Serial.print(F("  Error code: "));
        Serial.println(error.code);
        if (error.message && strlen(error.message) > 0) {
            Serial.print(F("  Error message: "));
            Serial.println(error.message);
        }
        Serial.println(F("\n[Troubleshooting]"));
        Serial.println(F("  - Check service account credentials in config.json"));
        Serial.println(F("  - Verify NTP time synchronization"));
        Serial.println(F("  - Ensure service account has Drive API access"));
    }
}

void testGoogleDriveListFiles() {
    Serial.println(F("\n========================================"));
    Serial.println(F("Google Drive File List Test"));
    Serial.println(F("========================================"));

    if (!ensureGoogleDriveClientInitialized()) {
        Serial.println(F("\nERROR: Google Drive client initialization failed"));
        return;
    }

    // Create a temporary file for TOC
    String tempTocPath = "/test_toc.txt";

    Serial.println(F("\n[GDrive] Fetching file list..."));
    Serial.print(F("[GDrive] Folder ID: "));
    Serial.println(unifiedConfig.google_drive.drive.folder_id);

    auto startTime = millis();
    size_t fileCount = gDriveClient->list_files_streaming(
        unifiedConfig.google_drive.drive.folder_id.c_str(),
        sdCard,
        tempTocPath.c_str(),
        unifiedConfig.google_drive.drive.list_page_size
    );
    auto elapsedTime = millis() - startTime;

    if (fileCount > 0) {
        Serial.println(F("\n✓ File list fetched successfully!"));
        Serial.print(F("  Total files: "));
        Serial.println(fileCount);
        Serial.print(F("  Time taken: "));
        Serial.print(elapsedTime);
        Serial.println(F(" ms"));
        Serial.print(F("  TOC saved to: "));
        Serial.println(tempTocPath);

        // Read and display the TOC file contents
        Serial.println(F("\n[File List]"));
        fs::File tocFile = sdCard.open(tempTocPath.c_str(), FILE_READ);
        if (tocFile) {
            size_t displayCount = 0;
            size_t maxDisplay = 20;  // Show first 20 files

            while (tocFile.available() && displayCount < maxDisplay) {
                String line = tocFile.readStringUntil('\n');
                line.trim();

                if (line.length() > 0) {
                    // Parse line format: fileId|fileName
                    int separatorPos = line.indexOf('|');
                    if (separatorPos > 0) {
                        String fileName = line.substring(separatorPos + 1);
                        Serial.print(F("  "));
                        Serial.print(displayCount + 1);
                        Serial.print(F(". "));
                        Serial.println(fileName);
                        displayCount++;
                    }
                }
            }

            if (fileCount > maxDisplay) {
                Serial.print(F("  ... and "));
                Serial.print(fileCount - maxDisplay);
                Serial.println(F(" more files"));
            }

            tocFile.close();
        } else {
            Serial.println(F("  ERROR: Could not open TOC file for reading"));
        }

        // Clean up temporary TOC file
        sdCard.remove(tempTocPath.c_str());
        Serial.println(F("\n[Cleanup] Temporary TOC file removed"));
    } else {
        Serial.println(F("\n✗ Failed to fetch file list or folder is empty"));
    }
}

void testGoogleDriveDownload() {
    Serial.println(F("\n========================================"));
    Serial.println(F("Google Drive Download & Display Test"));
    Serial.println(F("========================================"));

    if (!ensureGoogleDriveClientInitialized()) {
        Serial.println(F("\nERROR: Google Drive client initialization failed"));
        return;
    }

    // First, fetch the file list
    String tempTocPath = "/test_download_toc.txt";
    Serial.println(F("\n[GDrive] Fetching file list..."));

    size_t fileCount = gDriveClient->list_files_streaming(
        unifiedConfig.google_drive.drive.folder_id.c_str(),
        sdCard,
        tempTocPath.c_str(),
        unifiedConfig.google_drive.drive.list_page_size
    );

    if (fileCount == 0) {
        Serial.println(F("\n✗ No files found in folder or fetch failed"));
        return;
    }

    Serial.print(F("  Found "));
    Serial.print(fileCount);
    Serial.println(F(" files"));

    // Select a random file from the list
    randomSeed(millis());
    size_t randomIndex = random(0, fileCount);

    Serial.print(F("\n[Selection] Randomly selected file #"));
    Serial.println(randomIndex + 1);

    // Read the TOC file to get the selected file
    fs::File tocFile = sdCard.open(tempTocPath.c_str(), FILE_READ);
    if (!tocFile) {
        Serial.println(F("  ERROR: Could not open TOC file"));
        sdCard.remove(tempTocPath.c_str());
        return;
    }

    String selectedFileId;
    String selectedFileName;
    size_t currentIndex = 0;

    while (tocFile.available() && currentIndex <= randomIndex) {
        String line = tocFile.readStringUntil('\n');
        line.trim();

        if (line.length() > 0) {
            int separatorPos = line.indexOf('|');
            if (separatorPos > 0 && currentIndex == randomIndex) {
                selectedFileId = line.substring(0, separatorPos);
                selectedFileName = line.substring(separatorPos + 1);
                break;
            }
            currentIndex++;
        }
    }
    tocFile.close();
    sdCard.remove(tempTocPath.c_str());

    if (selectedFileId.length() == 0) {
        Serial.println(F("  ERROR: Could not select random file"));
        return;
    }

    Serial.print(F("  File: "));
    Serial.println(selectedFileName);
    Serial.print(F("  ID: "));
    Serial.println(selectedFileId);

    // Download the file
    String downloadPath = "/test_download.bin";
    Serial.println(F("\n[Download] Downloading file..."));

    fs::File downloadFile = sdCard.open(downloadPath.c_str(), FILE_WRITE);
    if (!downloadFile) {
        Serial.println(F("  ERROR: Could not create download file"));
        return;
    }

    auto startTime = millis();
    photo_frame::photo_frame_error_t error = gDriveClient->download_file(selectedFileId, &downloadFile);
    auto elapsedTime = millis() - startTime;

    size_t downloadedSize = downloadFile.size();
    downloadFile.close();

    if (error == photo_frame::error_type::None) {
        Serial.println(F("\n✓ Download successful!"));
        Serial.print(F("  File size: "));
        Serial.print(downloadedSize);
        Serial.println(F(" bytes"));
        Serial.print(F("  Time taken: "));
        Serial.print(elapsedTime);
        Serial.println(F(" ms"));

        // Display the image on the e-paper display
        Serial.println(F("\n[Display] Rendering image on e-paper display..."));
        ensureDisplayInitialized();

        fs::File imageFile = sdCard.open(downloadPath.c_str(), FILE_READ);
        if (imageFile) {
            auto displayStartTime = millis();

            // Detect file format and render
            uint16_t renderError = 0;
            size_t fileSize = imageFile.size();

            // Check if it's a BMP file (based on extension or size)
            bool isBmp = selectedFileName.endsWith(".bmp") || selectedFileName.endsWith(".BMP");

            if (isBmp) {
                Serial.println(F("  Format: BMP (DEPRECATED - not supported)"));
                Serial.println(F("  Use BIN format instead"));
                renderError = photo_frame::error_type::ImageFormatNotSupported.code;
            } else {
                // Binary format - only Mode 1 format supported (384KB)
                size_t expectedSize = DISP_WIDTH * DISP_HEIGHT;  // 384000 bytes for 800x480

                if (fileSize == expectedSize) {
                    Serial.println(F("  Format: Mode 1 binary (1 byte/pixel)"));
#if defined(DISP_6C) || defined(DISP_7C)
                    renderError = photo_frame::renderer::draw_demo_bitmap_mode1_from_file(
                        g_image_buffer, imageFile, selectedFileName.c_str(), DISP_WIDTH, DISP_HEIGHT);
#else
                    Serial.println(F("  ERROR: Mode 1 format not supported on this display"));
                    renderError = 1;
#endif
                } else {
                    Serial.print(F("  ERROR: Invalid file size: "));
                    Serial.println(fileSize);
                    Serial.print(F("  Expected: "));
                    Serial.print(expectedSize);
                    Serial.println(F(" bytes (Mode 1 format)"));
                    renderError = 1;
                }
            }

            auto displayElapsed = millis() - displayStartTime;
            imageFile.close();

            if (renderError == 0) {
                Serial.println(F("✓ Image displayed successfully!"));
                Serial.print(F("  Render time: "));
                Serial.print(displayElapsed);
                Serial.println(F(" ms"));
            } else {
                Serial.print(F("✗ Display failed with error: "));
                Serial.println(renderError);
            }
        }

        // Clean up
        sdCard.remove(downloadPath.c_str());
        Serial.println(F("\n[Cleanup] Downloaded file removed"));
    } else {
        Serial.println(F("\n✗ Download failed!"));
        Serial.print(F("  Error code: "));
        Serial.println(error.code);
        if (error.message && strlen(error.message) > 0) {
            Serial.print(F("  Error message: "));
            Serial.println(error.message);
        }
        sdCard.remove(downloadPath.c_str());
    }
}

void googleDriveTestMenu() {
    while (true) {
        Serial.println(F("\n========================================"));
        Serial.println(F("Google Drive Client Tests Menu"));
        Serial.println(F("========================================"));
        Serial.println(F("1 - Test Authentication (get_access_token)"));
        Serial.println(F("2 - List Files (list_files_streaming)"));
        Serial.println(F("3 - Download Random & Display"));
        Serial.println(F(""));
        Serial.println(F("0 - Back to Main Menu"));
        Serial.println(F("========================================"));
        Serial.println(F("Enter your choice:"));

        while (!Serial.available()) delay(10);
        String input = Serial.readStringUntil('\n');
        input.trim();
        Serial.println(input);

        if (input == "1") {
            testGoogleDriveAuth();
            Serial.println(F("\nPress any key to continue..."));
            while (!Serial.available()) delay(10);
            Serial.readStringUntil('\n');
        } else if (input == "2") {
            testGoogleDriveListFiles();
            Serial.println(F("\nPress any key to continue..."));
            while (!Serial.available()) delay(10);
            Serial.readStringUntil('\n');
        } else if (input == "3") {
            testGoogleDriveDownload();
            Serial.println(F("\nPress any key to continue..."));
            while (!Serial.available()) delay(10);
            Serial.readStringUntil('\n');
        } else if (input == "0") {
            return;
        } else {
            Serial.print(F("Unknown command: "));
            Serial.println(input);
            Serial.println(F("Press '0' for main menu"));
        }
    }
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
    Serial.println(F("5 - Display Tests"));
    Serial.println(F("6 - Deep Sleep Test"));
    Serial.println(F("7 - Google Drive Tests"));
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
                displayTestsMenu();
                break;

            case '6':
                testDeepSleep();
                break;

            case '7':
                googleDriveTestMenu();
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
