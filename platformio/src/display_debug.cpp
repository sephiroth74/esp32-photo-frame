#ifdef ENABLE_DISPLAY_DIAGNOSTIC

// New implementation using the display abstraction layer
#include "battery.h"
#include "display_manager.h"
#include "sd_card.h"
#include "test_photo_6c_portrait.h"
#include <Adafruit_GFX.h>
#include <RTClib.h>
#include <SPI.h>
#include <assets/fonts/Ubuntu_R.h>
#include <assets/icons/icons.h>
#include <esp_heap_caps.h> // For heap_caps_malloc

#define COLOR_WHITE  0xFF
#define COLOR_BLACK  0x00
#define COLOR_RED    0xE0
#define COLOR_GREEN  0x1C
#define COLOR_BLUE   0x03
#define COLOR_YELLOW 0xFC

// Define display power control pin (adjust for your board)
#define DISPLAY_POWER_PIN        17 // ProS3: GPIO17 controls LDO2 output
#define DISPLAY_POWER_ACTIVE_LOW 0  // ProS3 LDO2: HIGH = ON, LOW = OFF

const char images_directory[]      = "/6c/portrait/bin";
static const char image_filename[] = "/6c/bin/6c_MjAyMjA3MjBfMTk1NjEz__portrait.bin";
static photo_frame::DisplayManager g_display; // Use DisplayManager for all display operations
static photo_frame::SdCard sdCard;            // Use same SD card class as main.cpp

// Forward declarations
bool init_display_manager();
void cleanup_display_manager();

// Power control functions
void display_power_on() {
#ifdef DISPLAY_POWER_PIN
    log_i("[POWER] Turning display ON (GPIO %d -> %s)",
          DISPLAY_POWER_PIN,
          DISPLAY_POWER_ACTIVE_LOW ? "LOW" : "HIGH");

#ifdef DISPLAY_POWER_ACTIVE_LOW
    digitalWrite(DISPLAY_POWER_PIN, LOW); // P-MOSFET: LOW = ON
#else
    digitalWrite(DISPLAY_POWER_PIN, HIGH); // ProS3 LDO2 or N-MOSFET: HIGH = ON
#endif

    delay(200); // Allow power to stabilize (GDEP073E01 needs time)
    log_i("[POWER] Display power stabilized");
#endif
}

void display_power_off() {
#ifdef DISPLAY_POWER_PIN
    log_i("[POWER] Turning display OFF (GPIO %d -> %s)",
          DISPLAY_POWER_PIN,
          DISPLAY_POWER_ACTIVE_LOW ? "HIGH" : "LOW");

#ifdef DISPLAY_POWER_ACTIVE_LOW
    digitalWrite(DISPLAY_POWER_PIN, HIGH); // P-MOSFET: HIGH = OFF
#else
    digitalWrite(DISPLAY_POWER_PIN, LOW); // ProS3 LDO2 or N-MOSFET: LOW = OFF
#endif

    delay(50); // Short delay for clean shutdown
    log_i("[POWER] Display powered off");
#endif
}

void init_display_power() {
#ifdef DISPLAY_POWER_PIN
    log_i("[POWER] Initializing display power control on GPIO %d", DISPLAY_POWER_PIN);
    pinMode(DISPLAY_POWER_PIN, OUTPUT);

    // Start with display OFF
    display_power_off();
    delay(100);
#else
    log_i("[POWER] Display power control not configured (DISPLAY_POWER_PIN not defined)");
#endif
}

// Test function for power cycling
void test_display_power_cycle() {
    log_i("========================================");
    log_i("Display Power Control Test");
    log_i("========================================");

#ifdef DISPLAY_POWER_PIN
    log_i("Testing display power cycling...");

    // Test 1: Basic ON/OFF cycle
    log_i("\n[TEST 1] Basic power cycle");
    display_power_on();
    delay(2000);
    display_power_off();
    delay(2000);

    // Test 2: Multiple rapid cycles
    log_i("\n[TEST 2] Rapid power cycling (5 cycles)");
    for (int i = 0; i < 5; i++) {
        log_i("Cycle %d/5", i + 1);
        display_power_on();
        delay(500);
        display_power_off();
        delay(500);
    }

    // Test 3: Power on, initialize display, power off
    log_i("\n[TEST 3] Power + Display init test");
    display_power_on();

    if (init_display_manager()) {
        log_i("Display initialized successfully with power control");

        // Try to draw something simple
        GFXcanvas8& canvas = g_display.getCanvas();
        canvas.fillScreen(COLOR_WHITE);
        canvas.fillRect(100, 100, 200, 200, COLOR_BLACK);
        canvas.fillRect(150, 150, 100, 100, COLOR_RED);
        log_i("Drew test pattern");

        // Render to display
        if (g_display.render()) {
            log_i("Test pattern rendered successfully");
        } else {
            log_e("Failed to render test pattern");
        }

        // Sleep display before power off
        g_display.sleep();
        delay(1000);
    }

    display_power_off();
    delay(2000);

    // Test 4: Power measurements simulation
    log_i("\n[TEST 4] Power consumption simulation");
    log_i("Display OFF - Simulated consumption: ~0mA");
    delay(1000);

    display_power_on();
    log_i("Display ON (idle) - Simulated consumption: ~5mA");
    delay(2000);

    log_i("Display ON (refresh) - Simulated consumption: ~35-50mA (peaks to 80mA)");
    delay(3000);

    display_power_off();
    log_i("Display OFF again - Simulated consumption: ~0mA");

    log_i("\n[POWER TEST] Complete!");
    log_i("Note: Connect multimeter to measure actual current consumption");

#else
    log_w("Display power control not available (DISPLAY_POWER_PIN not defined)");
    log_w("To enable: Define DISPLAY_POWER_PIN in your board config");
#endif
}

bool init_display_manager() {
    if (g_display.isInitialized()) {
        log_w("Display manager already initialized");
        return true;
    }

    log_i("Initializing display manager...");

    // First initialize the buffer
    if (!g_display.initBuffer(true)) { // true = prefer PSRAM
        log_e("[display_debug] CRITICAL: Failed to initialize display buffer!");
        log_e("[display_debug] Cannot continue without buffer");
        return false;
    }

    // Then initialize the display hardware
    if (!g_display.initDisplay()) {
        log_e("[display_debug] CRITICAL: Failed to initialize display hardware!");
        log_e("[display_debug] Cannot continue without display");
        return false;
    }

    log_i("[display_debug] Display manager initialized successfully");
    log_i("[display_debug] Buffer size: %u bytes", g_display.getBufferSize());
    return true;
}

void cleanup_display_manager() {
    if (g_display.isInitialized()) {
        log_i("[display_debug] Releasing display manager");
        g_display.release();
        log_i("[display_debug] Display manager released");
    }
}

// These functions are no longer needed as we use photo_frame::sd_card class

bool pickImageFromSdCard(const char* images_directory, uint8_t* image_buffer, size_t buffer_size) {
    log_i("========================================");
    log_i("Portrait Image Test (From SD Card)");
    log_i("========================================");

    if (!image_buffer) {
        log_w("image_buffer is NULL");
        return false;
    }

    // First, try to use TOC cache if available
    bool use_toc       = true; // Enable TOC by default
    uint32_t fileCount = 0;
    auto ts            = millis();

    // Build or validate TOC
    if (use_toc) {
        if (!sdCard.isTocValid(images_directory, ".bin")) {
            log_i("Building SD card TOC for directory: %s", images_directory);
            photo_frame::photo_frame_error_t tocError;
            if (sdCard.buildDirectoryToc(images_directory, ".bin", &tocError)) {
                log_i("SD card TOC built successfully");
            } else {
                log_w(
                    "Failed to build SD card TOC: %s (code: %u), falling back to direct iteration",
                    tocError.message,
                    tocError.code);
                use_toc = false;
            }
        } else {
            log_i("Using existing SD card TOC cache");
        }
    }

    // Count files using TOC or direct iteration
    if (use_toc) {
        fileCount = sdCard.countFilesCached(images_directory, ".bin", true);
        log_i("Found %lu files using TOC cache", fileCount);
    } else {
        fileCount = sdCard.countFilesInDirectory(images_directory, ".bin");
        log_i("Found %lu files using direct iteration", fileCount);
    }

    auto elapsed = millis() - ts;
    log_v("Total .bin file count inside %s is %lu", images_directory, fileCount);
    log_v("Elapsed time for counting: %lu ms", elapsed);

    if (fileCount < 1) {
        log_w("Could not find any image inside the %s directory", images_directory);
        return false;
    }

    // Pick a random file
    uint32_t targetIndex = random(0, fileCount);
    log_v("Picking file at index %lu (0-based)", targetIndex);

    // Get file path using TOC or direct iteration
    String fullPath;
    ts = millis();

    if (use_toc) {
        fullPath = sdCard.getFileAtIndexCached(images_directory, targetIndex, ".bin", true);
        log_i("Selected file from TOC: %s", fullPath.c_str());
    } else {
        fullPath = sdCard.getFileAtIndex(images_directory, targetIndex, ".bin");
        log_i("Selected file from iteration: %s", fullPath.c_str());
    }

    elapsed = millis() - ts;
    log_v("Elapsed time for file selection: %lu ms", elapsed);

    if (fullPath.isEmpty()) {
        log_e("Failed to get file at index %lu", targetIndex);
        return false;
    }

    log_v("Full path: %s", fullPath.c_str());
    log_v("image_buffer address: %p", image_buffer);

    // Open and read the file
    File imageFile = sdCard.open(fullPath.c_str(), "r");
    if (!imageFile) {
        log_e("Failed to open file: %s", fullPath.c_str());
        return false;
    }

    size_t fileSize = imageFile.size();
    log_v("File size: %u bytes (expected: %u bytes)", fileSize, buffer_size);

    // Validate file size
    if (fileSize != buffer_size) {
        log_e("File size mismatch! File: %u bytes, Buffer: %u bytes", fileSize, buffer_size);
        imageFile.close();
        return false;
    }

    log_v("File opened successfully, available bytes: %u", imageFile.available());

    // Read file content into image_buffer
    log_v("Reading file into image buffer...");
    size_t bytesRead = imageFile.read(image_buffer, buffer_size);
    imageFile.close();

    if (bytesRead != buffer_size) {
        log_e("Failed to read complete file! Read: %u bytes, Expected: %u bytes",
              bytesRead,
              buffer_size);
        return false;
    }

    log_i("Successfully loaded %u bytes from SD card", bytesRead);
    return true;
}

void run_display_tests() {
    log_i("\n========================================");
    log_i("Display Debug Menu");
    log_i("========================================");
    log_i("1. Power Control Test");
    log_i("2. SD Card Image Test (with power control)");
    log_i("3. Full Test (Power + Image + Overlays)");
    log_i("========================================");
    log_i("Running test 1: Power Control Test");
    log_i("========================================\n");

    // Run power control test
    test_display_power_cycle();

    log_i("\nPress any key to continue to SD Card Image Test...");
    while (!Serial.available()) {
        delay(100);
    }
    while (Serial.available()) {
        Serial.read();
    }

    log_i("\n========================================");
    log_i("Running test 2: SD Card Image Test");
    log_i("========================================\n");

    // Initialize power control
    init_display_power();

    // ========== PHASE 1: SD Card Operations (Display OFF) ==========
    // Display is OFF during SD card operations to avoid SPI conflicts

    size_t bufferSize = photo_frame::DisplayManager::getNativeWidth() *
                        photo_frame::DisplayManager::getNativeHeight();
    uint8_t* tempBuffer = (uint8_t*)heap_caps_malloc(bufferSize, MALLOC_CAP_SPIRAM);
    if (!tempBuffer) {
        tempBuffer = (uint8_t*)malloc(bufferSize);
        if (!tempBuffer) {
            log_e("Failed to allocate temporary buffer!");
            display_power_off();
            return;
        }
    }
    log_i("Temporary buffer allocated: %u bytes", bufferSize);

    // Turn OFF display during SD card operations
    display_power_off();
    log_i("[PHASE 1] Display powered OFF for SD card operations");

    // Initialize SD card using photo_frame::sd_card class
    photo_frame::photo_frame_error_t sdError = sdCard.begin();
    if (sdError != photo_frame::error_type::None) {
        log_w("Failed to initialize the SD Card: %s", sdError.message);
        free(tempBuffer);
        return;
    }

    // Load image from SD card into temporary buffer
    bool success = pickImageFromSdCard(images_directory, tempBuffer, bufferSize);

    // IMPORTANT: Close SD card before initializing display (SPI conflict avoidance)
    sdCard.end();
    log_i("SD card closed");

    if (!success) {
        log_w("Failed to pick image from SD Card");
        free(tempBuffer);
        display_power_off();
        return;
    }

    // ========== PHASE 2: Display Operations (Display ON) ==========
    // Now that SD card is closed, we can power on and initialize the display

    log_i("[PHASE 2] Powering ON display for rendering");
    display_power_on();

    // Initialize display manager (this handles all display initialization)
    if (!init_display_manager()) {
        log_e("Failed to initialize display manager");
        free(tempBuffer);
        display_power_off();
        return;
    }

    // Copy image from temporary buffer to display buffer
    memcpy(g_display.getBuffer(), tempBuffer, bufferSize);
    free(tempBuffer);
    log_i("Image copied to display buffer");

    // Check display is valid
    if (!g_display.isInitialized()) {
        log_e("Display manager is not initialized!");
        return;
    }

    log_v("Image buffer address: %p", g_display.getBuffer());
    delay(1000);

    log_i("Displaying SD Card image");

    // Set rotation for portrait mode if needed
    bool portrait_mode = true; // Change to true if testing portrait mode

    if (portrait_mode) {
        g_display.setRotation(1);
    }

    log_v("Drawing overlay elements...");
    g_display.drawOverlay();

    // Draw battery status
    photo_frame::battery_info_t battery_info;
    battery_info.percent    = 100;
    battery_info.millivolts = 4120;
    g_display.drawBatteryStatus(battery_info);

    // Draw last update time
    DateTime now         = DateTime(2024, 1, 15, 12, 30, 0);
    long refresh_seconds = 3600; // 1 hour
    g_display.drawLastUpdate(now, refresh_seconds);

    // Draw image info
    g_display.drawImageInfo(0, 10, photo_frame::image_source_t::IMAGE_SOURCE_CLOUD);

    // Render the image with overlays to the display
    log_i("Rendering image to display...");
    if (!g_display.render()) {
        log_e("Failed to render image!");
    }

    // Put display to sleep to preserve lifespan
    g_display.sleep();

    log_i("Display rendered and put to sleep mode");
    delay(5000); // Delay for 5s to view the result

    // Power off display completely
    log_i("[PHASE 3] Powering OFF display for deep sleep");
    display_power_off();

    cleanup_display_manager();

    log_i("\n========================================");
    log_i("Test Complete!");
    log_i("Display is now powered OFF");
    log_i("Power consumption should be ~0mA");
    log_i("========================================");
}

void setup() {
    Serial.begin(115200);
    delay(2000);

    log_i("========================================");
    log_i("Display Power Control Test Suite");
    log_i("========================================");
    log_i("This test will demonstrate display power control");
    log_i("using GPIO pin for GDEP073E01 display");
    log_i("========================================\n");

    // Run all display tests
    run_display_tests();
}

void loop() {
    // empty
}

#endif // ENABLE_DISPLAY_DIAGNOSTIC