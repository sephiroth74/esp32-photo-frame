#ifndef __CANVAS_RENDERER_H__
#define __CANVAS_RENDERER_H__

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <RTClib.h>
#include "config.h"
#include "display_driver.h" // For DISPLAY_COLOR_* definitions
#include "geometry.h"
#include "battery.h"
#include "errors.h"
#include "google_drive.h"
#include <assets/icons/icons.h>

// Include display library headers for canvas.width() and canvas.height()
#ifdef DISP_6C
#include "../lib/GDEP073E01/Display_EPD_GDEP073E01_W21.h"
#else
#include "../lib/GDEY075T7/Display_EPD_GDEY075T7_W21.h"
#endif

namespace photo_frame {
namespace canvas_renderer {

    /**
     * Draw overlay elements on the canvas (statusbar, battery, etc.)
     * @param canvas Canvas to draw on (typically GFXcanvas8)
     * @note Portrait mode is determined from canvas.getRotation()
     */
    void draw_overlay(GFXcanvas8& canvas);

    /**
     * Draw last update time on the canvas
     * @param canvas Canvas to draw on
     * @param lastUpdate DateTime object with last update time
     * @param refresh_seconds Refresh interval in seconds
     * @note Portrait mode is determined from canvas.getRotation()
     */
    void draw_last_update(GFXcanvas8& canvas, const DateTime& lastUpdate, long refresh_seconds = 0);

    /**
     * Draw battery status on the canvas
     * @param canvas Canvas to draw on
     * @param battery_info Battery information structure
     */
    void draw_battery_status(GFXcanvas8& canvas, battery_info_t battery_info);

    /**
     * Draw image information on the canvas
     * @param canvas Canvas to draw on
     * @param index Current image index
     * @param total_images Total number of images
     * @param image_source Source of the image (cloud/cache)
     */
    void draw_image_info(GFXcanvas8& canvas, uint32_t index, uint32_t total_images,
        image_source_t image_source);

    /**
     * Draw error message on the canvas
     * @param canvas Canvas to draw on
     * @param error Error information
     */
    void draw_error(GFXcanvas8& canvas, photo_frame_error_t error);

    /**
     * Draw error message with details on the canvas
     * @param canvas Canvas to draw on
     * @param errMsgLn1 First line of error message
     * @param errMsgLn2 Second line of error message
     * @param filename Filename that caused error (optional)
     * @param errorCode Error code
     */
    void draw_error_with_details(GFXcanvas8& canvas, const String& errMsgLn1,
        const String& errMsgLn2, const char* filename,
        uint16_t errorCode);

    /**
     * Draw a side message with icon on the canvas
     * @param canvas Canvas to draw on
     * @param gravity Position of the message
     * @param icon_name Icon to display
     * @param message Message text
     * @param x_offset X offset from default position
     * @param y_offset Y offset from default position
     */
    void draw_side_message_with_icon(GFXcanvas8& canvas, gravity_t gravity,
        icon_name_t icon_name, const char* message,
        int32_t x_offset = 0, int32_t y_offset = 0);

    /**
     * Draw a side message without icon on the canvas
     * @param canvas Canvas to draw on
     * @param gravity Position of the message
     * @param message Message text
     * @param x_offset X offset from default position
     * @param y_offset Y offset from default position
     */
    void draw_side_message(GFXcanvas8& canvas, gravity_t gravity, const char* message,
        int32_t x_offset = 0, int32_t y_offset = 0);

    /**
     * Get text bounds for a string
     * @param canvas Canvas to use for measurement
     * @param text Text to measure
     * @param x X position
     * @param y Y position
     * @return Rectangle with text bounds
     */
    rect_t get_text_bounds(GFXcanvas8& canvas, const char* text, int16_t x = 0, int16_t y = 0);

    /**
     * Get string width
     * @param canvas Canvas to use for measurement
     * @param text Text to measure
     * @return Width in pixels
     */
    uint16_t get_string_width(GFXcanvas8& canvas, const String& text);

    /**
     * Get string height
     * @param canvas Canvas to use for measurement
     * @param text Text to measure
     * @return Height in pixels
     */
    uint16_t get_string_height(GFXcanvas8& canvas, const String& text);

    /**
     * Draw a string with alignment on the canvas
     * @param canvas Canvas to draw on
     * @param x X position
     * @param y Y position
     * @param text Text to draw
     * @param alignment Text alignment
     * @param color Text color
     */
    void draw_string(GFXcanvas8& canvas, int16_t x, int16_t y, const char* text,
        alignment_t alignment, uint16_t color = DISPLAY_COLOR_BLACK);

    /**
     * Draw a multi-line string on the canvas
     * @param canvas Canvas to draw on
     * @param x X position
     * @param y Y position
     * @param text Multi-line text
     * @param alignment Text alignment
     * @param max_width Maximum width per line
     * @param max_lines Maximum number of lines
     * @param line_spacing Spacing between lines
     * @param color Text color
     */
    void draw_multiline_string(GFXcanvas8& canvas, int16_t x, int16_t y,
        const String& text, alignment_t alignment,
        uint16_t max_width, uint16_t max_lines,
        int16_t line_spacing, uint16_t color = DISPLAY_COLOR_BLACK);

    /**
     * Draw a rounded rectangle with transparency on the canvas
     * @param canvas Canvas to draw on
     * @param x X position
     * @param y Y position
     * @param width Rectangle width
     * @param height Rectangle height
     * @param radius Corner radius
     * @param color Fill color
     * @param transparency Transparency level (0=opaque, 255=invisible)
     */
    void draw_rounded_rect(GFXcanvas8& canvas, int16_t x, int16_t y,
        int16_t width, int16_t height, uint16_t radius,
        uint16_t color, uint8_t transparency = 128);

} // namespace canvas_renderer
} // namespace photo_frame

#endif // __CANVAS_RENDERER_H__