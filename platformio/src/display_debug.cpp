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

const char images_directory[]      = "/6c/portrait/bin";
static const char image_filename[] = "/6c/bin/6c_MjAyMjA3MjBfMTk1NjEz__portrait.bin";
static photo_frame::DisplayManager g_display; // Use DisplayManager for all display operations
static photo_frame::sd_card sdCard;           // Use same SD card class as main.cpp

bool init_display_manager() {
    if (g_display.isInitialized()) {
        log_w("Display manager already initialized");
        return true;
    }

    log_i("Initializing display manager...");

    if (!g_display.init(true)) { // true = prefer PSRAM
        log_e("[display_debug] CRITICAL: Failed to initialize display manager!");
        log_e("[display_debug] Cannot continue without display");
        return false;
    } else {
        log_i("[display_debug] Display manager initialized successfully");
        log_i("[display_debug] Buffer size: %u bytes", g_display.getBufferSize());
        return true;
    }
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
        if (!sdCard.is_toc_valid(images_directory, ".bin")) {
            log_i("Building SD card TOC for directory: %s", images_directory);
            photo_frame::photo_frame_error_t tocError;
            if (sdCard.build_directory_toc(images_directory, ".bin", &tocError)) {
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
        fileCount = sdCard.count_files_cached(images_directory, ".bin", true);
        log_i("Found %lu files using TOC cache", fileCount);
    } else {
        fileCount = sdCard.count_files_in_directory(images_directory, ".bin");
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
        fullPath = sdCard.get_file_at_index_cached(images_directory, targetIndex, ".bin", true);
        log_i("Selected file from TOC: %s", fullPath.c_str());
    } else {
        fullPath = sdCard.get_file_at_index(images_directory, targetIndex, ".bin");
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

void setup() {
    Serial.begin(115200);
    delay(2000);

    log_i("Display Debug - Using DisplayManager with two-phase operation");

    // ========== PHASE 1: SD Card Operations ==========
    // First, allocate a temporary buffer for loading the image
    size_t bufferSize = photo_frame::DisplayManager::getNativeWidth() *
                        photo_frame::DisplayManager::getNativeHeight();
    uint8_t* tempBuffer = (uint8_t*)heap_caps_malloc(bufferSize, MALLOC_CAP_SPIRAM);
    if (!tempBuffer) {
        tempBuffer = (uint8_t*)malloc(bufferSize);
        if (!tempBuffer) {
            log_e("Failed to allocate temporary buffer!");
            return;
        }
    }
    log_i("Temporary buffer allocated: %u bytes", bufferSize);

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
        return;
    }

    // ========== PHASE 2: Display Operations ==========
    // Now that SD card is closed, we can initialize the display

    // Initialize display manager (this handles all display initialization)
    if (!init_display_manager()) {
        log_e("Failed to initialize display manager");
        free(tempBuffer);
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

    delay(5000); // Delay for 5s to view the result

    cleanup_display_manager();
}

void loop() {
    // empty
}

#endif // ENABLE_DISPLAY_DIAGNOSTIC