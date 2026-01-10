#ifdef ENABLE_DISPLAY_DIAGNOSTIC

// New implementation using the display abstraction layer
#include <Adafruit_GFX.h>
#include <SPI.h>
#include "sd_card.h"
#include "renderer.h"
#include "canvas_renderer.h"
#include "display_driver.h"
#include "image_buffer.h"
#include "test_photo_6c_portrait.h"
#include <assets/icons/icons.h>
#include <assets/fonts/Ubuntu_R.h>
#include "battery.h"
#include <RTClib.h>

#define COLOR_WHITE 0xFF
#define COLOR_BLACK 0x00
#define COLOR_RED 0xE0
#define COLOR_GREEN 0x1C
#define COLOR_BLUE 0x03
#define COLOR_YELLOW 0xFC


const char images_directory[] = "/6c/bin";
static const char image_filename[] = "/6c/bin/6c_MjAyMjA3MjBfMTk1NjEz__portrait.bin";
static photo_frame::ImageBuffer g_imageBuffer;  // RAII-based image buffer management
static photo_frame::sd_card sdCard;  // Use same SD card class as main.cpp

bool init_image_buffer()
{
    if (g_imageBuffer.isInitialized()) {
        log_w("Image buffer already initialized");
        return true;
    }

    log_i("Initializing image buffer...");

    if (!g_imageBuffer.init(EPD_WIDTH, EPD_HEIGHT, true)) {
        log_e("[display_debug] CRITICAL: Failed to initialize image buffer!");
        log_e("[display_debug] Cannot continue without image buffer");
        return false;
    } else {
        log_i("[display_debug] Image buffer initialized successfully");
        log_i("[display_debug] Buffer size: %u bytes, in %s",
              g_imageBuffer.getSize(),
              g_imageBuffer.isInPsram() ? "PSRAM" : "heap");
        return true;
    }
}

void cleanup_image_buffer()
{
    if (g_imageBuffer.isInitialized()) {
        log_i("[display_debug] Releasing image buffer");
        g_imageBuffer.release();
        log_i("[display_debug] Image buffer released");
    }
}

// These functions are no longer needed as we use photo_frame::sd_card class

bool pickImageFromSdCard(const char* images_directory, uint8_t* image_buffer, size_t buffer_size)
{
    log_i("========================================");
    log_i("Portrait Image Test (From SD Card)");
    log_i("========================================");

    if (!image_buffer) {
        log_w("image_buffer is NULL");
        return false;
    }

    File root = SD_CARD_LIB.open(images_directory, "r");
    if (!root) {
        log_e("Failed to open '%s' from Sd Card", images_directory);
        return false;
    }

    if (!root.isDirectory()) {
        log_e("'%s' is not a directory!", images_directory);
        root.close();
        return false;
    }

    uint32_t fileCount = 0;
    File entry = root.openNextFile();

    // Count valid .bin files
    while (entry) {
        if (!entry.isDirectory()) {
            String fileName = String(entry.name());
            if (fileName.endsWith(".bin") && !fileName.startsWith(".")) {
                fileCount++;
                log_v("Found file #%lu: %s (%lu bytes)", fileCount, entry.name(), entry.size());
            }
        }
        entry.close();
        entry = root.openNextFile();
    }
    root.close();

    log_v("Total .bin file count inside %s is %lu", images_directory, fileCount);

    if (fileCount < 1) {
        log_w("Could not find any image inside the %s directory", images_directory);
        return false;
    }

    // Pick a random file
    uint32_t targetIndex = random(0, fileCount);
    log_v("Picking file at index %lu (0-based)", targetIndex);

    // Reopen directory to iterate again
    root = SD_CARD_LIB.open(images_directory, "r");
    if (!root) {
        log_e("Failed to reopen '%s'", images_directory);
        // SD card will be closed by caller
        return false;
    }

    uint32_t currentIndex = 0;
    entry = root.openNextFile();
    bool success = false;

    // Find the target file
    while (entry) {
        if (!entry.isDirectory()) {
            String fileName = String(entry.name());
            if (fileName.endsWith(".bin") && !fileName.startsWith(".")) {
                if (currentIndex == targetIndex) {
                    // Found our target file!
                    log_i("Selected file: %s", entry.name());

                    size_t fileSize = entry.size();
                    log_v("File size: %u bytes (expected: %u bytes)", fileSize, buffer_size);

                    // Validate file size
                    if (fileSize != buffer_size) {
                        log_e("File size mismatch! File: %u bytes, Buffer: %u bytes", fileSize, buffer_size);
                        entry.close();
                        break;
                    }

                    // Build full path - check if fileName already contains the full path
                    String fullPath;
                    if (fileName.startsWith("/")) {
                        fullPath = fileName;
                    } else {
                        fullPath = String(images_directory) + "/" + fileName;
                    }
                    log_v("Full path: %s", fullPath.c_str());
                    log_v("image_buffer address: %p", image_buffer);
                    entry.close();

                    File imageFile = SD_CARD_LIB.open(fullPath.c_str(), "r");
                    if (!imageFile) {
                        log_e("Failed to open file: %s", fullPath.c_str());
                        break;
                    }

                    log_v("File opened successfully, available bytes: %u", imageFile.available());

                    // Read file content into image_buffer
                    log_v("Reading file into image buffer...");
                    size_t bytesRead = imageFile.read(image_buffer, buffer_size);
                    imageFile.close();

                    if (bytesRead != buffer_size) {
                        log_e("Failed to read complete file! Read: %u bytes, Expected: %u bytes", bytesRead, buffer_size);
                        break;
                    }

                    log_i("Successfully loaded %u bytes from SD card", bytesRead);
                    success = true;
                    break;
                }
                currentIndex++;
            }
        }
        entry.close();
        entry = root.openNextFile();
    }
    root.close();
    return success;
}

void drawInvertedBitmap(GFXcanvas8& canvas, int16_t x, int16_t y, const uint8_t bitmap[], int16_t w, int16_t h, uint16_t color)
{
    int16_t byteWidth = (w + 7) / 8;
    uint8_t byte = 0;
    for (int16_t j = 0; j < h; j++) {
        for (int16_t i = 0; i < w; i++) {
            if (i & 7)
                byte <<= 1;
            else {
#if defined(__AVR) || defined(ESP8266) || defined(ESP32)
                byte = pgm_read_byte(&bitmap[j * byteWidth + i / 8]);
#else
                byte = bitmap[j * byteWidth + i / 8];
#endif
            }
            if (!(byte & 0x80)) {
                canvas.drawPixel(x + i, y + j, color);
            }
        }
    }
}

void setup()
{
    Serial.begin(115200);
    delay(2000);

    log_i("Display Debug - Using new renderer abstraction");

    // Initialize the new renderer (this handles all display initialization)
    if (!photo_frame::renderer::init()) {
        log_e("Failed to initialize renderer!");
        return;
    }

    log_i("Renderer initialized successfully");

    // Initialize image buffer
    if(!init_image_buffer()) {
        log_e("Failed to initialize image buffer");
        return;
    }

    // Initialize SD card using photo_frame::sd_card class
    photo_frame::photo_frame_error_t sdError = sdCard.begin();
    if (sdError != photo_frame::error_type::None) {
        log_w("Failed to initialize the SD Card: %s", sdError.message);
        cleanup_image_buffer();
        return;
    }

    bool success = pickImageFromSdCard(images_directory, g_imageBuffer.getBuffer(), g_imageBuffer.getSize());
    sdCard.end();  // Properly close SD card

    if (!success) {
        log_w("Failed to pick image from SD Card");
        return;
    }

    // Check buffer is valid
    if (!g_imageBuffer.isInitialized()) {
        log_e("Image buffer is not initialized!");
        return;
    }

    log_v("Image buffer address: %p", g_imageBuffer.getBuffer());
    delay(1000);

    log_i("Displaying SD Card image");

    // Get canvas from ImageBuffer
    GFXcanvas8& canvas = g_imageBuffer.getCanvas();

    // Draw overlay using the new canvas_renderer
    bool portrait_mode = true; // Change to true if testing portrait mode

    if (portrait_mode) {
        canvas.setRotation(1);
    }

    log_v("Drawing overlay elements...");
    photo_frame::canvas_renderer::draw_overlay(canvas);

    // Draw battery status
    photo_frame::battery_info_t battery_info;
    battery_info.percent = 100;
    battery_info.millivolts = 4120;
    photo_frame::canvas_renderer::draw_battery_status(canvas, battery_info);

    // Draw last update time
    DateTime now = DateTime(2024, 1, 15, 12, 30, 0);
    long refresh_seconds = 3600; // 1 hour
    photo_frame::canvas_renderer::draw_last_update(canvas, now, refresh_seconds);

    // Draw image info
    photo_frame::canvas_renderer::draw_image_info(canvas, 0, 10,
        photo_frame::image_source_t::IMAGE_SOURCE_LOCAL_CACHE);

    // Render the image with overlays to the display
    log_i("Rendering image to display...");
    if (!photo_frame::renderer::render_image(g_imageBuffer.getBuffer())) {
        log_e("Failed to render image!");
    }

    // Put display to sleep to preserve lifespan
    photo_frame::renderer::sleep();

    delay(5000); // Delay for 5s to view the result

    cleanup_image_buffer();
}

void loop()
{
    // empty
}

#endif // ENABLE_DISPLAY_DIAGNOSTIC