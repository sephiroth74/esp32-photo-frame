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

#include "FS.h"
#include "RTClib.h"
#include "config.h"
#include "errors.h"
#include "battery.h"
#include <Arduino.h>
#include <assets/icons/icons.h>

#define MAX_DISPLAY_BUFFER_SIZE 65536ul // e.g.

#if defined(DISP_BW_V2)
#define DISP_WIDTH 800
#define DISP_HEIGHT 480
#include <GxEPD2_BW.h>
#define GxEPD2_DISPLAY_CLASS GxEPD2_BW
#define GxEPD2_DRIVER_CLASS GxEPD2_750_GDEY075T7
#define MAX_HEIGHT(EPD) (EPD::HEIGHT <= MAX_DISPLAY_BUFFER_SIZE / (EPD::WIDTH / 8) ? EPD::HEIGHT : MAX_DISPLAY_BUFFER_SIZE / (EPD::WIDTH / 8))
extern GxEPD2_DISPLAY_CLASS<GxEPD2_DRIVER_CLASS, MAX_HEIGHT(GxEPD2_DRIVER_CLASS)> display;

#elif defined(DISP_7C_F)
#define DISP_WIDTH 800
#define DISP_HEIGHT 480
#include <GxEPD2_7C.h>
#define GxEPD2_DISPLAY_CLASS GxEPD2_7C
#define GxEPD2_DRIVER_CLASS GxEPD2_730c_GDEY073D46
#define MAX_HEIGHT(EPD) (EPD::HEIGHT <= (MAX_DISPLAY_BUFFER_SIZE) / (EPD::WIDTH / 2) ? EPD::HEIGHT : (MAX_DISPLAY_BUFFER_SIZE) / (EPD::WIDTH / 2))
extern GxEPD2_DISPLAY_CLASS<GxEPD2_DRIVER_CLASS, MAX_HEIGHT(GxEPD2_DRIVER_CLASS)> display;

#elif defined(DISP_6C)
#define DISP_WIDTH 800
#define DISP_HEIGHT 480
#include <GxEPD2_7C.h>
#define GxEPD2_DISPLAY_CLASS GxEPD2_7C
#define GxEPD2_DRIVER_CLASS GxEPD2_730c_GDEP073E01
#define MAX_HEIGHT(EPD) (EPD::HEIGHT <= (MAX_DISPLAY_BUFFER_SIZE) / (EPD::WIDTH / 2) ? EPD::HEIGHT : (MAX_DISPLAY_BUFFER_SIZE) / (EPD::WIDTH / 2))
extern GxEPD2_DISPLAY_CLASS<GxEPD2_DRIVER_CLASS, MAX_HEIGHT(GxEPD2_DRIVER_CLASS)> display;
#endif

typedef struct rect {
    int16_t x;
    int16_t y;
    uint16_t width;
    uint16_t height;

public:
    operator bool() const { return (width > 0 && height > 0); }

    inline constexpr bool is_empty() const { return (width == 0 || height == 0); }
    inline constexpr bool is_valid() const { return (x >= 0 && y >= 0 && width > 0 && height > 0); }
    inline constexpr bool contains(int16_t px, int16_t py) const
    {
        return (px >= x && px < x + width && py >= y && py < y + height);
    }
    inline constexpr bool intersects(const rect& other) const
    {
        return (x < other.x + other.width && x + width > other.x && y < other.y + other.height && y + height > other.y);
    }

    constexpr rect(int16_t x = 0, int16_t y = 0, uint16_t width = 0, uint16_t height = 0)
        : x(x)
        , y(y)
        , width(width)
        , height(height)
    {
    }

    static inline constexpr rect fromXYWH(int16_t x, int16_t y, uint16_t width, uint16_t height)
    {
        return rect(x, y, width, height);
    }

    static inline constexpr rect empty() { return rect(0, 0, 0, 0); }

} rect_t;

typedef enum alignment { LEFT,
    RIGHT,
    CENTER } alignment_t;

typedef enum gravity {
    TOP_LEFT,
    TOP_RIGHT,
    BOTTOM_LEFT,
    BOTTOM_RIGHT,
    TOP_CENTER,
} gravity_t;

namespace photo_frame {
namespace renderer {

    /**
     * Calculates the bounds of the text to be rendered.
     * @param text The text to calculate bounds for.
     * @param x The x-coordinate of the text position (default is 0).
     * @param y The y-coordinate of the text position (default is 0).
     * @return A rect_t object representing the bounds of the text.
     * @note This function calculates the width and height of the text based on the current font size
     * and style. It does not render the text on the display.
     */
    rect_t get_text_bounds(const char* text, int16_t x = 0, int16_t y = 0);

    /**
     * Calculates the width of the given text string.
     * @param text The text string to calculate the width for.
     * @return The width of the text in pixels.
     * @note This function uses the current font size and style to calculate the width.
     */
    uint16_t get_string_width(const String& text);

    /**
     * Calculates the height of the given text string.
     * @param text The text string to calculate the height for.
     * @return The height of the text in pixels.
     * @note This function uses the current font size and style to calculate the height.
     */
    uint16_t get_string_height(const String& text);

    /**
     * Initializes the e-paper display.
     * Sets up the SPI communication and configures the display settings.
     * Should be called before any drawing operations.
     */
    void init_display();

    /**
     * Powers off the e-paper display.
     * This function should be called when the display is no longer needed.
     * It releases the SPI and control pins and turns off the display.
     */
    void power_off();

    void refresh(bool partial_update_mode = false);

    void fill_screen(uint16_t color = GxEPD_WHITE);

    bool has_partial_update();

    /**
     * Draws a string on the e-paper display at the specified position with alignment.
     * @param x The x-coordinate of the text position.
     * @param y The y-coordinate of the text position.
     * @param text The text to be drawn.
     * @param alignment The alignment of the text (LEFT, RIGHT, CENTER).
     * @param color The color of the text (default is GxEPD_BLACK).
     */
    void draw_string(int16_t x,
        int16_t y,
        const char* text,
        alignment_t alignment,
        uint16_t color = GxEPD_BLACK);

    /**
     * Draws a string on the e-paper display at the specified position.
     * @param x The x-coordinate of the text position.
     * @param y The y-coordinate of the text position.
     * @param text The text to be drawn.
     * @param color The color of the text (default is GxEPD_BLACK).
     * @note This function does not support text alignment. It draws the text at the specified position
     * without any alignment adjustments.
     */
    void draw_string(int16_t x, int16_t y, const char* text, uint16_t color = GxEPD_BLACK);

    /**
     * Draws a multi-line string on the e-paper display.
     * @param x The x-coordinate of the text position.
     * @param y The y-coordinate of the text position.
     * @param text The multi-line text to be drawn.
     * @param alignment The alignment of the text (LEFT, RIGHT, CENTER).
     * @param max_width The maximum width of each line in pixels.
     * @param max_lines The maximum number of lines to draw.
     * @param line_spacing The spacing between lines in pixels.
     * @param color The color of the text (default is GxEPD_BLACK).
     */
    void draw_multiline_string(int16_t x,
        int16_t y,
        const String& text,
        alignment_t alignment,
        uint16_t max_width,
        uint16_t max_lines,
        int16_t line_spacing,
        uint16_t color = GxEPD_BLACK);

    /**
     * Draws an error message on the e-paper display.
     * @param error_type The type of error to be displayed.
     * @note This function displays a predefined error message based on the error type.
     */
    void draw_error(photo_frame_error error_type);

    /**
     * Draws an error message with a bitmap icon.
     * @param bitmap_196x196 The bitmap to be displayed (196x196 pixels).
     * @param errMsgLn1 The first line of the error message.
     * @param errMsgLn2 The second line of the error message (optional).
     */
    void draw_error(const uint8_t* bitmap_196x196,
        const String& errMsgLn1,
        const String& errMsgLn2 = "");

    /**
     * Draws an error message with a specific gravity and code.
     * @param gravity The gravity of the message (TOP_LEFT, TOP_RIGHT, BOTTOM_LEFT, BOTTOM_RIGHT).
     * @param code The error code to be displayed.
     */
    void draw_error_message(const gravity_t gravity, const uint8_t code);

    /**
     * Draws a side message with an icon.
     * @param gravity The gravity of the message (TOP_LEFT, TOP_RIGHT, BOTTOM_LEFT, BOTTOM_RIGHT).
     * @param icon_name The name of the icon to be displayed.
     * @param message The message to be displayed next to the icon.
     * @param x_offset The x-offset from the icon position (default is 0).
     * @param y_offset The y-offset from the icon position (default is 0).
     * @note The icon will be drawn next to the message, and the message will be aligned based on the
     * specified gravity. The icon size is fixed at 16x16 pixels.
     */
    void draw_side_message_with_icon(gravity_t gravity,
        icon_name_t icon_name,
        const char* message,
        int32_t x_offset = 0,
        int32_t y_offset = 0);

    /**
     * Draws a side message without an icon.
     * @param gravity The gravity of the message (TOP_LEFT, TOP_RIGHT, BOTTOM_LEFT, BOTTOM_RIGHT).
     * @param message The message to be displayed.
     * @param x_offset The x-offset from the message position (default is 0).
     * @param y_offset The y-offset from the message position (default is 0).
     * @note This function displays a message on the side of the display without any icon. The
     * message will be aligned based on the specified gravity. The text will be drawn in the default
     * font and color (GxEPD_BLACK).
     */
    void draw_side_message(gravity_t gravity,
        const char* message,
        int32_t x_offset = 0,
        int32_t y_offset = 0);

    /**
     * Draws the last update time on the e-paper display.
     * @param lastUpdate The DateTime object representing the last update time.
     * @param refresh_seconds The number of seconds since the last update (default is 0).
     * @note This function formats the last update time and displays it on the top left corner of the
     * display. If refresh_seconds is bigger than 0, it will be displayed in parentheses next to the time.
     */
    void draw_last_update(const DateTime& lastUpdate, long refresh_seconds = 0);

    /**
     * Draws the battery status on the e-paper display.
     * @param raw_value The raw battery value (used for debugging).
     * @param battery_voltage The battery voltage in millivolts.
     * @param battery_percentage The battery percentage (0-100).
     * @note This function displays the battery icon and its status on the top right corner of the
     * display. It uses predefined icons based on the battery percentage.
     */
    void draw_battery_status(battery::battery_info battery_info);

    /**
     * Draws image information on the e-paper display.
     * @param index The index of the current image.
     * @param total_images The total number of images.
     * @note This function displays the current image index and total number of images on the
     * top center corner of the display. It is useful for providing context when viewing images.
     */
    void draw_image_info(uint32_t index, uint32_t total_images);

    /**
     * Draws a bitmap image from a file on the e-paper display.
     */
    bool draw_bitmap_from_file(File& file, const char* filename, int16_t x = 0, int16_t y = 0, bool with_color = true);

    /**
     * Draws a bitmap image from a file on the e-paper display with buffering.
     * This function is optimized for partial updates and uses a buffered approach.
     */
    bool draw_bitmap_from_file_buffered(File& file,
        const char* filename,
        int16_t x = 0,
        int16_t y = 0,
        bool with_color = true,
        bool partial_update = false);

    bool draw_binary_from_file_buffered(File& filen, const char* filename, int width, int height);

    bool draw_binary_from_file(File& file, const char* filename, int width, int height);

} // namespace renderer

} // namespace photo_frame

#endif // __PHOTO_FRAME_RENDERER_H__