// MIT License
//
// Copyright (c) 2025 Alessandro Crugnola
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef __PHOTO_FRAME_RENDERER_H__
#define __PHOTO_FRAME_RENDERER_H__

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <RTClib.h>

#include "FS.h"
#include "battery.h"
#include "config.h"
#include "errors.h"
#include "geometry.h"
#include "google_drive.h"
#include "display_driver.h"
#include <assets/icons/icons.h>

#define MAX_DISPLAY_BUFFER_SIZE 65536ul // e.g.

// Dynamic display dimensions (will be set based on portrait_mode)
extern uint16_t DISP_WIDTH;
extern uint16_t DISP_HEIGHT;

namespace photo_frame {

    /**
     * Initialize the renderer and display driver.
     * @return True if initialization successful, false otherwise
     */
    bool rendererInit();

    /**
     * Render an image from a buffer.
     * @param imageBuffer Pointer to image buffer (800x480 bytes) with image and overlays already drawn
     * @return True if rendering successful, false otherwise
     * @note The buffer should already contain the complete image with any overlays
     */
    bool renderImage(uint8_t* imageBuffer);

    /**
     * Put the display into sleep mode to save power.
     * Must be called after display operations to preserve screen lifespan.
     */
    void rendererSleep();

    /**
     * Clear the display to white.
     */
    void rendererClear();

    /**
     * Clean up renderer resources and shut down display.
     */
    void rendererCleanup();

    // ========== Display state functions ==========

    void rendererPowerOff();
    void rendererHibernate();
    void rendererEnd();
    void rendererRefresh(bool partial_update_mode = false);
    bool rendererHasPartialUpdate();
    bool rendererHasFastPartialUpdate();
    bool rendererHasColor();

    // ========== Error display functions ==========

    /**
     * Draws an error message on the canvas.
     * @param canvas Canvas to draw on
     * @param error The error to be displayed
     */
    void rendererDrawError(GFXcanvas8& canvas, photo_frame_error_t error);

    /**
     * Draws an error message with details on the canvas.
     * @param canvas Canvas to draw on
     * @param errMsgLn1 The first line of the error message
     * @param errMsgLn2 The second line of the error message (optional)
     * @param filename The filename that caused the error (optional)
     * @param errorCode The numeric error code
     */
    void rendererDrawErrorWithDetails(GFXcanvas8& canvas,
                                 const String& errMsgLn1,
                                 const String& errMsgLn2,
                                 const char* filename,
                                 uint16_t errorCode);

    // ========== Image loading and rendering functions ==========

    /**
     * Draws a multi-line string on the e-paper display.
     * @param x The x-coordinate of the text position.
     * @param y The y-coordinate of the text position.
     * @param text The multi-line text to be drawn.
     * @param alignment The alignment of the text (LEFT, RIGHT, CENTER).
     * @param max_width The maximum width of each line in pixels.
     * @param max_lines The maximum number of lines to draw.
     * @param line_spacing The spacing between lines in pixels.
     * @param color The color of the text (default is DISPLAY_COLOR_BLACK).
     */
    void rendererDrawMultilineString(GFXcanvas8 canvas, int16_t x,
        int16_t y,
        const String& text,
        alignment_t alignment,
        uint16_t max_width,
        uint16_t max_lines,
        int16_t line_spacing,
        uint16_t color = DISPLAY_COLOR_BLACK);

    /**
     * Draws a side message with an icon on a GFX canvas (for overlay composition).
     * @param gfx Reference to Adafruit_GFX object (can be display or canvas)
     * @param gravity The gravity of the message
     * @param icon_name The name of the icon to be displayed
     * @param message The message to be displayed next to the icon
     * @param x_offset The x-offset from the icon position (default is 0)
     * @param y_offset The y-offset from the icon position (default is 0)
     */
    void rendererDrawSideMessageWithIcon(Adafruit_GFX& gfx,
        gravity_t gravity,
        icon_name_t icon_name,
        const char* message,
        int32_t x_offset = 0,
        int32_t y_offset = 0);

    /**
     * Draws a side message without an icon on a GFX canvas (for overlay composition).
     * @param gfx Reference to Adafruit_GFX object (can be display or canvas)
     * @param gravity The gravity of the message
     * @param message The message to be displayed
     * @param x_offset The x-offset from the message position (default is 0)
     * @param y_offset The y-offset from the message position (default is 0)
     */
    void rendererDrawSideMessage(Adafruit_GFX& gfx,
        gravity_t gravity,
        const char* message,
        int32_t x_offset = 0,
        int32_t y_offset = 0);

    /**
     * Draws the last update time on a GFX canvas (for overlay composition).
     * @param gfx Reference to Adafruit_GFX object (can be display or canvas)
     * @param lastUpdate The DateTime object representing the last update time.
     * @param refresh_seconds The number of seconds since the last update (default is 0).
     */
    void rendererDrawLastUpdate(Adafruit_GFX& gfx, const DateTime& lastUpdate, long refresh_seconds = 0);

    /**
     * Draws the battery status on a GFX canvas (for overlay composition).
     * @param gfx Reference to Adafruit_GFX object (can be display or canvas)
     * @param battery_info Battery information structure
     */
    void rendererDrawBatteryStatus(Adafruit_GFX& gfx, photo_frame::battery_info_t battery_info);

    /**
     * Draws image information on a GFX canvas (for overlay composition).
     * @param gfx Reference to Adafruit_GFX object (can be display or canvas)
     * @param index The index of the current image.
     * @param total_images The total number of images.
     * @param image_source The source of the current image (cloud or local cache).
     */
    void rendererDrawImageInfo(Adafruit_GFX& gfx,
        uint32_t index,
        uint32_t total_images,
        photo_frame::image_source_t image_source);

    /**
     * @brief Load image file into buffer
     *
     * Reads Mode 1 format image (384KB, 1 byte per pixel) from SD card into the
     * provided buffer. This allows the SD card to be closed before display operations begin.
     *
     * @param buffer Pointer to buffer (must be at least width * height bytes)
     * @param file Open file handle (SD card or other filesystem)
     * @param filename Original filename for logging and validation
     * @param width Expected image width (should match display width, 800)
     * @param height Expected image height (should match display height, 480)
     * @return Error code (0 = success, non-zero = error)
     * @note File can be closed after this function returns
     */
    uint16_t loadImageToBuffer(uint8_t* buffer, File& file, const char* filename, int width, int height);

} // namespace photo_frame

#endif // __PHOTO_FRAME_RENDERER_H__