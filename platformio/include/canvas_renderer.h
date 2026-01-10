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

// Canvas rendering functions - all in photo_frame namespace
// Using camelCase for functions as per C++ conventions

    /**
     * Draw overlay elements on the canvas (statusbar, battery, etc.)
     * @param canvas Canvas to draw on (typically GFXcanvas8)
     * @note Portrait mode is determined from canvas.getRotation()
     */
    void drawOverlay(GFXcanvas8& canvas);

    /**
     * Draw last update time on the canvas
     * @param canvas Canvas to draw on
     * @param lastUpdate DateTime object with last update time
     * @param refreshSeconds Refresh interval in seconds
     * @note Portrait mode is determined from canvas.getRotation()
     */
    void drawLastUpdate(GFXcanvas8& canvas, const DateTime& lastUpdate, long refreshSeconds = 0);

    /**
     * Draw battery status on the canvas
     * @param canvas Canvas to draw on
     * @param batteryInfo Battery information structure
     */
    void drawBatteryStatus(GFXcanvas8& canvas, battery_info_t batteryInfo);

    /**
     * Draw image information on the canvas
     * @param canvas Canvas to draw on
     * @param index Current image index
     * @param totalImages Total number of images
     * @param imageSource Source of the image (cloud/cache)
     */
    void drawImageInfo(GFXcanvas8& canvas, uint32_t index, uint32_t totalImages,
        image_source_t imageSource);

    /**
     * Draw error message on the canvas
     * @param canvas Canvas to draw on
     * @param error Error information
     */
    void drawError(GFXcanvas8& canvas, photo_frame_error_t error);

    /**
     * Draw error message with details on the canvas
     * @param canvas Canvas to draw on
     * @param errMsgLn1 First line of error message
     * @param errMsgLn2 Second line of error message
     * @param filename Filename that caused error (optional)
     * @param errorCode Error code
     */
    void drawErrorWithDetails(GFXcanvas8& canvas, const String& errMsgLn1,
        const String& errMsgLn2, const char* filename,
        uint16_t errorCode);

    /**
     * Draw a side message with icon on the canvas
     * @param canvas Canvas to draw on
     * @param gravity Position of the message
     * @param iconName Icon to display
     * @param message Message text
     * @param xOffset X offset from default position
     * @param yOffset Y offset from default position
     */
    void drawSideMessageWithIcon(GFXcanvas8& canvas, gravity_t gravity,
        icon_name_t iconName, const char* message,
        int32_t xOffset = 0, int32_t yOffset = 0);

    /**
     * Draw a side message without icon on the canvas
     * @param canvas Canvas to draw on
     * @param gravity Position of the message
     * @param message Message text
     * @param xOffset X offset from default position
     * @param yOffset Y offset from default position
     */
    void drawSideMessage(GFXcanvas8& canvas, gravity_t gravity, const char* message,
        int32_t xOffset = 0, int32_t yOffset = 0);

    /**
     * Get text bounds for a string
     * @param canvas Canvas to use for measurement
     * @param text Text to measure
     * @param x X position
     * @param y Y position
     * @return Rectangle with text bounds
     */
    rect_t getTextBounds(GFXcanvas8& canvas, const char* text, int16_t x = 0, int16_t y = 0);

    /**
     * Get string width
     * @param canvas Canvas to use for measurement
     * @param text Text to measure
     * @return Width in pixels
     */
    uint16_t getStringWidth(GFXcanvas8& canvas, const String& text);

    /**
     * Get string height
     * @param canvas Canvas to use for measurement
     * @param text Text to measure
     * @return Height in pixels
     */
    uint16_t getStringHeight(GFXcanvas8& canvas, const String& text);

    /**
     * Draw a string with alignment on the canvas
     * @param canvas Canvas to draw on
     * @param x X position
     * @param y Y position
     * @param text Text to draw
     * @param alignment Text alignment
     * @param color Text color
     */
    void drawString(GFXcanvas8& canvas, int16_t x, int16_t y, const char* text,
        alignment_t alignment, uint16_t color = DISPLAY_COLOR_BLACK);

    /**
     * Draw a multi-line string on the canvas
     * @param canvas Canvas to draw on
     * @param x X position
     * @param y Y position
     * @param text Multi-line text
     * @param alignment Text alignment
     * @param maxWidth Maximum width per line
     * @param maxLines Maximum number of lines
     * @param lineSpacing Spacing between lines
     * @param color Text color
     */
    void drawMultilineString(GFXcanvas8& canvas, int16_t x, int16_t y,
        const String& text, alignment_t alignment,
        uint16_t maxWidth, uint16_t maxLines,
        int16_t lineSpacing, uint16_t color = DISPLAY_COLOR_BLACK);

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
    void drawRoundedRect(GFXcanvas8& canvas, int16_t x, int16_t y,
        int16_t width, int16_t height, uint16_t radius,
        uint16_t color, uint8_t transparency = 128);
} // namespace photo_frame

#endif // __CANVAS_RENDERER_H__