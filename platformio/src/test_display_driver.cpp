/**
 * Test file for the new display abstraction layer
 * This file provides a simple test to verify the DisplayDriver implementation
 *
 * To use this test:
 * 1. Uncomment #define TEST_DISPLAY_DRIVER in your main.cpp
 * 2. Build and upload to your board
 * 3. Monitor serial output for test results
 */

#ifdef TEST_DISPLAY_DRIVER

#include "config.h"
#include "display_driver.h"
#include "display_driver_6c.h"
#include "display_driver_bw.h"
#include <Arduino.h>
#include <SD.h>
#include <SPI.h>

// Include display library headers for EPD_WIDTH and EPD_HEIGHT
// The pins are taken from config.h (EPD_BUSY_PIN, EPD_RST_PIN, EPD_DC_PIN, EPD_CS_PIN)
#ifdef DISP_6C
#include <GDEP073E01/Display_EPD_W21.h>
#else
#include <GDEY075T7/Display_EPD_W21.h>
#endif

// Test image buffer
static uint8_t* test_image_buffer     = nullptr;
static const size_t IMAGE_BUFFER_SIZE = EPD_WIDTH * EPD_HEIGHT;

// Display driver instance
static DisplayDriver* displayDriver = nullptr;

// SD card SPI instance for boards that need separate SPI
#ifdef USE_HSPI_FOR_SD
SPIClass hspi(HSPI);
#endif

void setup() {
    Serial.begin(115200);
    delay(3000);

    log_i("========================================");
    log_i("Display Driver Test - Starting");
    log_i("========================================");

    // Print board info
    log_i("Board: %s", ARDUINO_BOARD);
    log_i("Chip Model: %s", ESP.getChipModel());

    // Check PSRAM
    if (psramFound()) {
        log_i("PSRAM: FOUND - Size: %.2f MB", ESP.getPsramSize() / 1048576.0f);
        log_i("Free PSRAM: %.2f MB", ESP.getFreePsram() / 1048576.0f);
    } else {
        log_w("PSRAM: NOT FOUND");
    }

    // Allocate image buffer
    log_i("Allocating image buffer...");
#if CONFIG_SPIRAM_USE_CAPS_ALLOC || CONFIG_SPIRAM_USE_MALLOC
    test_image_buffer = (uint8_t*)heap_caps_malloc(IMAGE_BUFFER_SIZE, MALLOC_CAP_SPIRAM);
    if (test_image_buffer) {
        log_i("Buffer allocated in PSRAM");
    } else {
        test_image_buffer = (uint8_t*)malloc(IMAGE_BUFFER_SIZE);
        log_w("Buffer allocated in regular heap");
    }
#else
    test_image_buffer = (uint8_t*)malloc(IMAGE_BUFFER_SIZE);
#endif

    if (!test_image_buffer) {
        log_e("Failed to allocate image buffer!");
        return;
    }

    // Create display driver based on configuration
#ifdef DISP_6C
    log_i("Creating 6-color display driver...");
    displayDriver = new DisplayDriver6C(
        EPD_CS_PIN, EPD_DC_PIN, EPD_RST_PIN, EPD_BUSY_PIN, EPD_SCK_PIN, EPD_MOSI_PIN);
#else
    log_i("Creating B&W display driver...");
    displayDriver = new DisplayDriverBW(
        EPD_CS_PIN, EPD_DC_PIN, EPD_RST_PIN, EPD_BUSY_PIN, EPD_SCK_PIN, EPD_MOSI_PIN);
#endif

    if (!displayDriver) {
        log_e("Failed to create display driver!");
        free(test_image_buffer);
        return;
    }

    // Initialize display
    log_i("Initializing display...");
    if (!displayDriver->init()) {
        log_e("Failed to initialize display!");
        delete displayDriver;
        free(test_image_buffer);
        return;
    }

    log_i("Display initialized: %s", displayDriver->getDisplayType());

    // Test 1: Clear display
    log_i("Test 1: Clearing display...");
    displayDriver->clear();
    delay(2000);

    // Test 2: Fill with test pattern
    log_i("Test 2: Creating test pattern...");

    // Create a simple test pattern
    for (size_t i = 0; i < IMAGE_BUFFER_SIZE; i++) {
        // Create alternating stripes
        int x = i % EPD_WIDTH;
        int y = i / EPD_WIDTH;

        if ((x / 100) % 2 == 0) {
            test_image_buffer[i] = DISPLAY_COLOR_WHITE;
        } else if ((x / 100) % 3 == 0) {
            test_image_buffer[i] = DISPLAY_COLOR_BLACK;
        } else {
#ifdef DISP_6C
            // For 6-color display, use colors
            switch ((x / 100) % 6) {
            case 0:  test_image_buffer[i] = DISPLAY_COLOR_RED; break;
            case 1:  test_image_buffer[i] = DISPLAY_COLOR_GREEN; break;
            case 2:  test_image_buffer[i] = DISPLAY_COLOR_BLUE; break;
            case 3:  test_image_buffer[i] = DISPLAY_COLOR_YELLOW; break;
            default: test_image_buffer[i] = DISPLAY_COLOR_WHITE; break;
            }
#else
            // For B&W display, use black and white
            test_image_buffer[i] = (x / 100) % 2 ? DISPLAY_COLOR_BLACK : DISPLAY_COLOR_WHITE;
#endif
        }
    }

    log_i("Displaying test pattern...");
    if (!displayDriver->picDisplay(test_image_buffer)) {
        log_e("Failed to display test pattern!");
    } else {
        log_i("Test pattern displayed successfully");
    }

    delay(5000);

    // Test 3: Try to load image from SD card if available
    log_i("Test 3: Attempting to load image from SD card...");

#ifdef SD_USE_SPI
#ifdef USE_HSPI_FOR_SD
    hspi.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
    if (SD.begin(SD_CS_PIN, hspi)) {
#else
    SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
    if (SD.begin(SD_CS_PIN, SPI)) {
#endif
#else
    if (SD_MMC.begin("/sdcard", false, true)) {
#endif
        log_i("SD card initialized");

        // Try to find an image file
#ifdef DISP_6C
        const char* testPath = "/6c/bin";
#else
        const char* testPath = "/bw/bin";
#endif

        File root = SD.open(testPath);
        if (root && root.isDirectory()) {
            File entry = root.openNextFile();
            while (entry) {
                String name = entry.name();
                if (name.endsWith(".bin") && entry.size() == IMAGE_BUFFER_SIZE) {
                    log_i("Found test image: %s", name.c_str());

                    size_t bytesRead = entry.read(test_image_buffer, IMAGE_BUFFER_SIZE);
                    if (bytesRead == IMAGE_BUFFER_SIZE) {
                        log_i("Image loaded, displaying...");

                        if (displayDriver->picDisplay(test_image_buffer)) {
                            log_i("Image displayed successfully!");
                        } else {
                            log_e("Failed to display image");
                        }
                    } else {
                        log_e("Failed to read complete image");
                    }

                    entry.close();
                    break;
                }
                entry.close();
                entry = root.openNextFile();
            }
            root.close();
        } else {
            log_w("Test directory %s not found", testPath);
        }

        SD.end();
#ifdef USE_HSPI_FOR_SD
        hspi.end();
#endif
    } else {
        log_w("SD card not available, skipping image test");
    }

    delay(10000);

    // Put display to sleep
    log_i("Putting display to sleep...");
    displayDriver->sleep();

    // Cleanup
    delete displayDriver;
    free(test_image_buffer);

    log_i("========================================");
    log_i("Display Driver Test - Complete");
    log_i("========================================");

    log_i("Going to deep sleep in 5 seconds...");
    delay(5000);
    esp_deep_sleep_start();
}

void loop() {
    // Not used in test
}

#endif // TEST_DISPLAY_DRIVER