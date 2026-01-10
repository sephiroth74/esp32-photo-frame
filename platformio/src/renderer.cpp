#include "renderer.h"
#include "config.h"
#include "display_driver_6c.h"
#include "display_driver_bw.h"
#include "canvas_renderer.h"
#include "image_buffer.h"
#include "errors.h"
#include <Adafruit_GFX.h>
#include <FS.h>

// Include display library headers for EPD_WIDTH and EPD_HEIGHT
// The pins are taken from config.h (EPD_BUSY_PIN, EPD_RST_PIN, EPD_DC_PIN, EPD_CS_PIN)
#ifdef DISP_6C
#include <Display_EPD_GDEP073E01_W21.h>
#else
#include <Display_EPD_GDEY075T7_W21.h>
#endif

// Dynamic display dimensions (exported for compatibility)
// These will be set based on portrait_mode in the future
uint16_t DISP_WIDTH = EPD_WIDTH; // 800
uint16_t DISP_HEIGHT = EPD_HEIGHT; // 480

// Static display driver instance - created at compile time based on configuration
#ifdef DISP_6C
static DisplayDriver6C displayDriver(EPD_CS_PIN, EPD_DC_PIN, EPD_RST_PIN,
    EPD_BUSY_PIN, EPD_SCK_PIN, EPD_MOSI_PIN);
#else
static DisplayDriverBW displayDriver(EPD_CS_PIN, EPD_DC_PIN, EPD_RST_PIN,
    EPD_BUSY_PIN, EPD_SCK_PIN, EPD_MOSI_PIN);
#endif

namespace photo_frame {
namespace renderer {

    bool init()
    {
        log_i("Initializing renderer...");

        // Initialize the display driver (already created at compile time)
        if (!displayDriver.init()) {
            log_e("Failed to initialize display");
            return false;
        }

        log_i("Renderer initialized successfully with %s display", displayDriver.getDisplayType());
        return true;
    }

    bool render_image(uint8_t* imageBuffer)
    {
        if (!displayDriver.isInitialized()) {
            log_e("Display not initialized");
            return false;
        }

        if (imageBuffer == nullptr) {
            log_e("Image buffer is NULL");
            return false;
        }

        // Display the image directly from the provided buffer
        // The buffer should already contain the image with any overlays drawn
        bool result = displayDriver.picDisplay(imageBuffer);

        if (result) {
            log_d("Image rendered successfully");
        } else {
            log_e("Failed to render image");
        }

        return result;
    }

    void sleep()
    {
        log_i("Putting display to sleep...");
        displayDriver.sleep();
        log_i("Display is now in sleep mode");
    }

    void clear()
    {
        if (displayDriver.isInitialized()) {
            log_i("Clearing display...");
            displayDriver.clear();
            log_i("Display cleared");
        }
    }

    void cleanup()
    {
        log_i("Cleaning up renderer...");

        // Put display to sleep if initialized
        if (displayDriver.isInitialized()) {
            displayDriver.sleep();
        }

        log_i("Renderer cleanup complete");
    }

    // ========== Legacy function wrappers using canvas_renderer ==========

    void draw_multiline_string(GFXcanvas8 canvas, int16_t x, int16_t y, const String& text,
        alignment_t alignment, uint16_t max_width,
        uint16_t max_lines, int16_t line_spacing, uint16_t color)
    {

        // Draw on canvas using canvas_renderer
        canvas_renderer::draw_multiline_string(canvas, x, y, text, alignment,
            max_width, max_lines, line_spacing, color);
    }

    void draw_side_message_with_icon(Adafruit_GFX& gfx, gravity_t gravity,
        icon_name_t icon_name, const char* message,
        int32_t x_offset, int32_t y_offset)
    {
        // This version draws directly on the provided GFX object (could be a canvas)
        // We assume it's a GFXcanvas8 - caller must ensure this
        GFXcanvas8& canvas = static_cast<GFXcanvas8&>(gfx);
        canvas_renderer::draw_side_message_with_icon(canvas, gravity, icon_name,
            message, x_offset, y_offset);
    }

    void draw_side_message(Adafruit_GFX& gfx, gravity_t gravity, const char* message,
        int32_t x_offset, int32_t y_offset)
    {
        // This version draws directly on the provided GFX object
        // We assume it's a GFXcanvas8 - caller must ensure this
        GFXcanvas8& canvas = static_cast<GFXcanvas8&>(gfx);
        canvas_renderer::draw_side_message(canvas, gravity, message, x_offset, y_offset);
    }

    void draw_last_update(Adafruit_GFX& gfx, const DateTime& lastUpdate, long refresh_seconds)
    {
        // This version draws directly on the provided GFX object
        // We assume it's a GFXcanvas8 - caller must ensure this
        GFXcanvas8& canvas = static_cast<GFXcanvas8&>(gfx);
        canvas_renderer::draw_last_update(canvas, lastUpdate, refresh_seconds);
    }

    void draw_battery_status(Adafruit_GFX& gfx, photo_frame::battery_info_t battery_info)
    {
        // This version draws directly on the provided GFX object
        // We assume it's a GFXcanvas8 - caller must ensure this
        GFXcanvas8& canvas = static_cast<GFXcanvas8&>(gfx);
        canvas_renderer::draw_battery_status(canvas, battery_info);
    }

    void draw_image_info(Adafruit_GFX& gfx, uint32_t index, uint32_t total_images,
        photo_frame::image_source_t image_source)
    {
        // This version draws directly on the provided GFX object
        // We assume it's a GFXcanvas8 - caller must ensure this
        GFXcanvas8& canvas = static_cast<GFXcanvas8&>(gfx);
        canvas_renderer::draw_image_info(canvas, index, total_images, image_source);
    }

    // ========== Display state functions ==========

    void power_off()
    {
        log_i("Powering off display...");
        if (displayDriver.isInitialized()) {
            displayDriver.sleep();
        }
    }

    void end()
    {
        cleanup();
    }

    void refresh(bool partial_update_mode)
    {
        log_i("Refreshing display (partial=%d)...", partial_update_mode);
        if (displayDriver.isInitialized()) {
            displayDriver.refresh(partial_update_mode);
        }
    }

    bool has_partial_update()
    {
        return displayDriver.has_partial_update();
    }

    bool has_color()
    {
        return displayDriver.has_color();
    }

    // ========== Error display functions ==========

    void draw_error(GFXcanvas8& canvas, photo_frame_error_t error)
    {
        // Forward to canvas_renderer
        photo_frame::canvas_renderer::draw_error(canvas, error);
    }

    void draw_error_with_details(GFXcanvas8& canvas,
                                 const String& errMsgLn1,
                                 const String& errMsgLn2,
                                 const char* filename,
                                 uint16_t errorCode)
    {
        // Forward to canvas_renderer
        photo_frame::canvas_renderer::draw_error_with_details(canvas, errMsgLn1, errMsgLn2, filename, errorCode);
    }

    // ========== Image loading functions ==========

    uint16_t load_image_to_buffer(uint8_t* buffer, File& file, const char* filename, int width, int height)
    {
        log_i("load_image_to_buffer: %s, width: %d, height: %d",
              filename, width, height);

        // Validate basic parameters
        if (!file) {
            log_e("File not open or invalid");
            return 1;
        }

        if (!buffer) {
            log_e("ERROR: Image buffer is null!");
            return 2;
        }

        // Mode 1 format file size validation: width * height bytes (1 byte per pixel)
        size_t expectedSize = width * height;
        if (file.size() != expectedSize) {
            log_e("ERROR: Mode 1 format file size mismatch");
            log_e("Expected: %d bytes, Got: %d bytes", expectedSize, file.size());
            return 3;
        }

        log_i("Mode 1 format file size: %d bytes (1 byte per pixel)", expectedSize);

        // Read the entire file into provided buffer
        auto startTime = millis();
        file.seek(0);
        size_t bytesRead = file.read(buffer, expectedSize);
        auto readTime = millis() - startTime;

        if (bytesRead != expectedSize) {
            log_e("ERROR: Failed to read file (got %d bytes, expected %d)",
                  bytesRead, expectedSize);
            return 4;
        }

        log_i("Image loaded to buffer in %lu ms", readTime);
        return 0; // Success
    }

} // namespace renderer
} // namespace photo_frame