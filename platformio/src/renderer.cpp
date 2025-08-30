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

#include "renderer.h"
#include "datetime_utils.h"

#include FONT_HEADER

#if !defined(USE_DESPI_DRIVER) && !defined(USE_WAVESHARE_DRIVER)
#error "Please define USE_DESPI_DRIVER or USE_WAVESHARE_DRIVER."
#endif

#if defined(USE_DESPI_DRIVER) && defined(USE_WAVESHARE_DRIVER)
#error "Please define only one display driver: either USE_DESPI_DRIVER or USE_WAVESHARE_DRIVER."
#endif

GxEPD2_DISPLAY_CLASS<GxEPD2_DRIVER_CLASS, MAX_HEIGHT(GxEPD2_DRIVER_CLASS)> display(GxEPD2_DRIVER_CLASS(EPD_CS_PIN, EPD_DC_PIN, EPD_RST_PIN, EPD_BUSY_PIN));

#ifdef USE_HSPI_FOR_EPD
SPIClass hspi(HSPI); // SPI object for e-Paper display
SPISettings device_settings(4000000, MSBFIRST, SPI_MODE0);
#endif // USE_HSPI_FOR_EPD

namespace photo_frame {
namespace renderer {

    void color2Epd(uint8_t color, uint16_t& pixelColor, int x, int y);

    // static const uint16_t input_buffer_pixels = 20; // may affect performance
    static const uint16_t input_buffer_pixels
        = 800; // may affect performance

    static const uint16_t max_row_width = 1448; // for up to 6" display 1448x1072
    static const uint16_t max_palette_pixels = 256; // for depth <= 8

    uint8_t input_buffer[3 * input_buffer_pixels]; // up to depth 24
    uint8_t output_row_mono_buffer[max_row_width / 8]; // buffer for at least one row of b/w bits
    uint8_t output_row_color_buffer[max_row_width / 8]; // buffer for at least one row of color bits
    uint8_t mono_palette_buffer[max_palette_pixels / 8]; // palette buffer for depth <= 8 b/w
    uint8_t color_palette_buffer[max_palette_pixels / 8]; // palette buffer for depth <= 8 c/w
    uint16_t rgb_palette_buffer[max_palette_pixels]; // palette buffer for depth <= 8 for buffered
                                                     // graphics, needed for 7-color display


    uint16_t read8(File& f)
    {
        uint8_t result;
        f.read(&result, 1); // read the next byte to move the file pointer
        return result;
    }

    uint16_t read16(File& f)
    {
        // BMP data is stored little-endian, same as Arduino.
        uint16_t result;
        ((uint8_t*)&result)[0] = f.read(); // LSB
        ((uint8_t*)&result)[1] = f.read(); // MSB
        return result;
    }

    uint32_t read32(File& f)
    {
        // BMP data is stored little-endian, same as Arduino.
        uint32_t result;
        ((uint8_t*)&result)[0] = f.read(); // LSB
        ((uint8_t*)&result)[1] = f.read();
        ((uint8_t*)&result)[2] = f.read();
        ((uint8_t*)&result)[3] = f.read(); // MSB
        return result;
    }

    /*
     * Initialize e-paper display
     */
    void init_display()
    {
        Serial.println("Initializing e-paper display...");

        if (!digitalPinIsValid(EPD_BUSY_PIN) || !digitalPinIsValid(EPD_RST_PIN) || !digitalPinIsValid(EPD_DC_PIN) || !digitalPinIsValid(EPD_CS_PIN)) {
            Serial.println("Invalid e-paper display pins!");
            return;
        }

        pinMode(EPD_RST_PIN, OUTPUT);
        pinMode(EPD_DC_PIN, OUTPUT);
        pinMode(EPD_CS_PIN, OUTPUT);


#if defined(USE_HSPI_FOR_EPD)
        hspi.begin(EPD_SCK_PIN, EPD_MOSI_PIN, EPD_MOSI_PIN, EPD_CS_PIN); // remap SPI for EPD
        display.epd2.selectSPI(hspi, device_settings);
#elif defined(USE_HSPI_FOR_EPD)
        SPI.end();
        SPI.begin(EPD_SCK_PIN, -1, EPD_MOSI_PIN, EPD_CS_PIN); // remap SPI for EPD
#endif // USE_HSPI_FOR_EPD

#ifdef USE_DESPI_DRIVER
        display.init(115200);
#else // USE_WAVESHARE_DRIVER
        display.init(115200, true, 2, false);
#endif // USE_DESPI_DRIVER

        display.setRotation(0);
        display.setTextSize(1);
        // display.clearScreen(); // black
        display.setTextColor(GxEPD_BLACK);
        display.setTextWrap(false);

        auto pages = display.pages();
        Serial.printf("Display pages: %d\n", pages);
        if (pages > 1) {
            Serial.printf("Display page height: %d\n", display.pageHeight());
        }
        Serial.printf("Display width: %d\n", display.width());
        Serial.printf("Display height: %d\n", display.height());
        Serial.printf("Full refresh time: %d\n", display.epd2.full_refresh_time);
        Serial.printf("Display has color: %s\n", display.epd2.hasColor ? "true" : "false");
        Serial.printf("Display has partial update: %s\n",
            display.epd2.hasPartialUpdate ? "true" : "false");
        Serial.printf("Display has fast partial update: %s\n",
            display.epd2.hasFastPartialUpdate ? "true" : "false");

        // display.setFullWindow();
        // display.fillScreen(GxEPD_WHITE);
        // display.firstPage(); // use paged drawing mode, sets fillScreen(GxEPD_WHITE)
        return;
    } // end initDisplay

    /*
     * Power-off e-paper display
     */
    void power_off()
    {
        Serial.println("Powering off e-paper display...");
        // display.hibernate();
        display.powerOff(); // turns off the display
        delay(100); // wait for the display to power off
        display.end(); // release SPI and control pins

#ifdef USE_HSPI_FOR_EPD
        hspi.end(); // release SPI pins
#else
        SPI.end(); // release SPI pins
#endif // USE_HSPI_FOR_EPD

        return;
    } // end initDisplay

    void refresh(bool partial_update_mode) { display.refresh(partial_update_mode); }

    void fill_screen(uint16_t color) { display.fillScreen(color); }

    bool has_partial_update()
    {
        // check if the display supports partial update
        return display.epd2.hasPartialUpdate || display.epd2.hasFastPartialUpdate;
    } // end hasPartialUpdate

    /*
     * Returns the string width in pixels
     */
    uint16_t get_string_width(const String& text)
    {
        int16_t x1, y1;
        uint16_t w, h;
        display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
        return w;
    }

    /*
     * Returns the string height in pixels
     */
    uint16_t get_string_height(const String& text)
    {
        int16_t x1, y1;
        uint16_t w, h;
        display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
        return h;
    }

    rect_t get_text_bounds(const char* text, int16_t x, int16_t y)
    {
        int16_t x1, y1;
        uint16_t w, h;
        display.getTextBounds(text, x, y, &x1, &y1, &w, &h);
        return rect_t::from_xywh(x1, y1, w, h);
    }

    /*
     * Draws a string with alignment
     */
    void draw_string(int16_t x, int16_t y, const char* text, alignment_t alignment, uint16_t color)
    {
        display.setTextColor(color);
        rect_t rect = get_text_bounds(text, x, y);
        if (alignment == RIGHT) {
            x = x - rect.width;
        }
        if (alignment == CENTER) {
            x = x - rect.width / 2;
        }
        display.setCursor(x, y);
        display.print(text);
        return;
    } // end drawString

    /*
     * Draws a string without alignment
     */
    void draw_string(int16_t x, int16_t y, const char* text, uint16_t color)
    {
        display.setTextColor(color);
        display.setCursor(x, y);
        display.print(text);
        return;
    } // end drawString

    /*
     * Draws a string that will flow into the next line when max_width is reached.
     * If a string exceeds max_lines an ellipsis (...) will terminate the last word.
     * Lines will break at spaces(' ') and dashes('-').
     *
     * Note: max_width should be big enough to accommodate the largest word that
     *       will be displayed. If an unbroken string of characters longer than
     *       max_width exist in text, then the string will be printed beyond
     *       max_width.
     */
    void draw_multiline_string(int16_t x,
        int16_t y,
        const String& text,
        alignment_t alignment,
        uint16_t max_width,
        uint16_t max_lines,
        int16_t line_spacing,
        uint16_t color)
    {
        uint16_t current_line = 0;
        String textRemaining = text;
        // print until we reach max_lines or no more text remains
        while (current_line < max_lines && !textRemaining.isEmpty()) {
            // Yield periodically during complex text processing
            if (current_line % 5 == 0) {
                yield();
            }

            int16_t x1, y1;
            uint16_t w, h;

            display.getTextBounds(textRemaining, 0, 0, &x1, &y1, &w, &h);

            int endIndex = textRemaining.length();
            // check if remaining text is to wide, if it is then print what we can
            String subStr = textRemaining;
            int splitAt = 0;
            int keepLastChar = 0;
            while (w > max_width && splitAt != -1) {
                if (keepLastChar) {
                    // if we kept the last character during the last iteration of this while
                    // loop, remove it now so we don't get stuck in an infinite loop.
                    subStr.remove(subStr.length() - 1);
                }

                // find the last place in the string that we can break it.
                if (current_line < max_lines - 1) {
                    splitAt = std::max(subStr.lastIndexOf(" "), subStr.lastIndexOf("-"));
                } else {
                    // this is the last line, only break at spaces so we can add ellipsis
                    splitAt = subStr.lastIndexOf(" ");
                }

                // if splitAt == -1 then there is an unbroken set of characters that is
                // longer than max_width. Otherwise if splitAt != -1 then we can continue
                // the loop until the string is <= max_width
                if (splitAt != -1) {
                    endIndex = splitAt;
                    subStr = subStr.substring(0, endIndex + 1);

                    char lastChar = subStr.charAt(endIndex);
                    if (lastChar == ' ') {
                        // remove this char now so it is not counted towards line width
                        keepLastChar = 0;
                        subStr.remove(endIndex);
                        --endIndex;
                    } else if (lastChar == '-') {
                        // this char will be printed on this line and removed next iteration
                        keepLastChar = 1;
                    }

                    if (current_line < max_lines - 1) {
                        // this is not the last line
                        display.getTextBounds(subStr, 0, 0, &x1, &y1, &w, &h);
                    } else {
                        // this is the last line, we need to make sure there is space for
                        // ellipsis
                        display.getTextBounds(subStr + "...", 0, 0, &x1, &y1, &w, &h);
                        if (w <= max_width) {
                            // ellipsis fit, add them to subStr
                            subStr = subStr + "...";
                        }
                    }

                } // end if (splitAt != -1)
            } // end inner while

            draw_string(x, y + (current_line * line_spacing), subStr.c_str(), alignment, color);

            // update textRemaining to no longer include what was printed
            // +1 for exclusive bounds, +1 to get passed space/dash
            textRemaining = textRemaining.substring(endIndex + 2 - keepLastChar);

            ++current_line;
        } // end outer while

        return;
    } // end drawMultiLnString

    void draw_error(photo_frame_error error)
    {
        if (error == error_type::None) {
            Serial.println("drawError: No error to display");
            return;
        }

        const unsigned char* bitmap_196x196;

        if (error == error_type::NoSdCardAttached) {
            bitmap_196x196 = getBitmap(icon_name::micro_sd_card_0deg, 196);
        } else if (error == error_type::BatteryLevelCritical) {
            bitmap_196x196 = getBitmap(icon_name::battery_alert_0deg, 196);
        } else {
            bitmap_196x196 = getBitmap(icon_name::error_icon, 196);
        }

        draw_error(bitmap_196x196, String(error.message), "");
    }

    void draw_error_message(const gravity_t gravity, const uint8_t code)
    {
        String errMsgLn1 = String(TXT_ERROR_CODE) + code;

        display.setFont(&FONT_6pt8b);
        display.setTextColor(GxEPD_BLACK);

        rect_t rect = get_text_bounds(errMsgLn1.c_str(), 0, 0);

        switch (gravity) {
        case TOP_LEFT: {
            draw_string(10, 10, errMsgLn1.c_str(), GxEPD_BLACK);
            break;
        }
        case TOP_RIGHT: {
            draw_string(DISP_WIDTH - rect.width - 10, 10, errMsgLn1.c_str(), GxEPD_BLACK);
            break;
        }
        case BOTTOM_LEFT: {
            draw_string(10, DISP_HEIGHT - rect.height - 10, errMsgLn1.c_str(), GxEPD_BLACK);
            break;
        }
        case BOTTOM_RIGHT: {
            draw_string(DISP_WIDTH - rect.width - 10,
                DISP_HEIGHT - rect.height - 10,
                errMsgLn1.c_str(),
                GxEPD_BLACK);
            break;
        }
        default: {
            Serial.println("drawErrorMessage: Invalid gravity type");
            return;
        }
        }
    }

    void draw_error(const uint8_t* bitmap_196x196, const String& errMsgLn1, const String& errMsgLn2)
    {
        Serial.println("drawError: " + errMsgLn1);

        display.setFont(&FONT_26pt8b);
        if (!errMsgLn2.isEmpty()) {
            draw_string(DISP_WIDTH / 2, DISP_HEIGHT / 2 + 196 / 2 + 21, errMsgLn1.c_str(), CENTER);
            draw_string(DISP_WIDTH / 2, DISP_HEIGHT / 2 + 196 / 2 + 21 + 55, errMsgLn2.c_str(), CENTER);
        } else {
            draw_multiline_string(DISP_WIDTH / 2,
                DISP_HEIGHT / 2 + 196 / 2 + 21,
                errMsgLn1,
                CENTER,
                DISP_WIDTH - 200,
                2,
                55);
        }

        if (bitmap_196x196) {
            display.drawInvertedBitmap(DISP_WIDTH / 2 - 196 / 2,
                DISP_HEIGHT / 2 - 196 / 2 - 21,
                bitmap_196x196,
                196,
                196,
                ACCENT_COLOR);
        } else {
            Serial.println("Error: No bitmap provided for drawError");
        }
        return;
    } // end drawError

    void draw_image_info(uint32_t index, uint32_t total_images)
    {
        Serial.println("drawImageInfo: " + String(index) + " / " + String(total_images));
        String message = String(index + 1) + " / " + String(total_images);
        draw_side_message(gravity::TOP_CENTER, message.c_str(), 0, 0);
    }

    void draw_battery_status(photo_frame::battery_info_t battery_info)
    {
        Serial.println("drawBatteryStatus");

        auto battery_voltage = battery_info.millivolts;

        auto battery_percentage = battery_info.percent;

        // Draw battery icon
        icon_name_t icon_name;

        if (battery_info.is_charging()) {
            icon_name = icon_name::battery_charging_full_90deg;
        } else {

            if (battery_percentage >= 100) {
                icon_name = icon_name::battery_full_90deg;
            } else if (battery_percentage >= 80) {
                icon_name = icon_name::battery_6_bar_90deg;
            } else if (battery_percentage >= 60) {
                icon_name = icon_name::battery_5_bar_90deg;
            } else if (battery_percentage >= 40) {
                icon_name = icon_name::battery_4_bar_90deg;
            } else if (battery_percentage >= 20) {
                icon_name = icon_name::battery_3_bar_90deg;
            } else if (battery_percentage >= 10) {
                icon_name = icon_name::battery_2_bar_90deg;
            } else {
                icon_name = icon_name::battery_alert_90deg; // Critical battery
            }
        }

        String message = String((uint16_t)battery_percentage) + "% (" + String((float)battery_voltage / 1000, 2) + "V)";

#if DEBUG_MODE
#ifdef USE_SENSOR_MAX1704X
        message += " - " + String(battery_info.charge_rate) + "mA";
#else
        message += " / " + String(raw_value) + "r - " + String(raw_millivolts) + "m";
#endif // USE_SENSOR_MAX1704X
#endif

        draw_side_message_with_icon(gravity::TOP_RIGHT, icon_name, message.c_str(), 0, -2);
    }

    void draw_last_update(const DateTime& lastUpdate, long refresh_seconds)
    {
        Serial.println("drawLastUpdate: " + lastUpdate.timestamp());
        char dateTimeBuffer[32] = { 0 }; // Buffer to hold formatted date and time
        photo_frame::datetime_utils::format_datetime(dateTimeBuffer, sizeof(dateTimeBuffer), lastUpdate);

        // format the refresh time string in the format "Refresh: 0' 0" (e.g. "Refresh: 10' 30")
        String refreshStr = "";
        if (refresh_seconds > 0) {
            long hours = refresh_seconds / 3600; // Calculate hours
            long minutes = refresh_seconds / 60; // Calculate minutes
            long seconds = refresh_seconds % 60; // Calculate remaining seconds

            if (hours > 0) {
                refreshStr += String(hours) + "h"; // Append hours with a single quote
                minutes -= hours * 60; // Adjust minutes after calculating hours
                seconds -= hours * 3600; // Adjust seconds after calculating hours
            }

            if (minutes > 0) {
                if (refreshStr.length() > 0) {
                    refreshStr += " "; // Add space if hours were added
                }
                refreshStr += String(minutes) + "m"; // Append minutes with a single quote
                seconds -= minutes * 60; // Adjust seconds after calculating minutes
            }

            if (seconds > 0) {
                if (refreshStr.length() > 0) {
                    refreshStr += " "; // Add space if minutes were added
                }
                refreshStr += String(seconds) + "s"; // Append seconds with a double quote
            }
        }

        String lastUpdateStr = String(dateTimeBuffer) + (refreshStr ? " (" + String(refreshStr) + ")" : "");

        draw_side_message_with_icon(gravity::TOP_LEFT, icon_name::wi_time_10, lastUpdateStr.c_str(), 0, -2);
    } // end drawLastUpdate

    void draw_side_message(gravity_t gravity, const char* message, int32_t x_offset, int32_t y_offset)
    {
        display.setFont(&FONT_7pt8b);
        display.setTextColor(GxEPD_BLACK);
        rect_t rect = get_text_bounds(message, 0, 0);

        int16_t x = 0, y = 0;
        switch (gravity) {
        case TOP_CENTER: {
            x = (DISP_WIDTH - rect.width) / 2;
            y = -rect.y;
            break;
        }

        case TOP_LEFT: {
            x = 0;
            y = -rect.y;
            break;
        }
        case TOP_RIGHT: {
            x = DISP_WIDTH - rect.width - 20;
            y = -rect.y;
            break;
        }
        case BOTTOM_LEFT: {
            x = 0;
            y = DISP_HEIGHT - rect.y;
            break;
        }
        case BOTTOM_RIGHT: {
            x = DISP_WIDTH - rect.width - 20;
            y = DISP_HEIGHT - rect.y;
            break;
        }
        default: {
            Serial.println("drawSideMessage: Invalid gravity type");
            return;
        }
        }

        // Draw the message
        draw_string(x + x_offset, y + y_offset, message, GxEPD_BLACK);
    } // end drawSideMessage

    void draw_side_message_with_icon(gravity_t gravity,
        icon_name_t icon_name,
        const char* message,
        int32_t x_offset,
        int32_t y_offset)
    {
        const uint16_t icon_size = 16;
        const unsigned char* icon = getBitmap(icon_name, icon_size); // ensure the icon is loaded
        if (!icon) {
            Serial.println("drawSizeMessageWithIcon: Icon not found");
            return;
        }

        display.setFont(&FONT_7pt8b);
        display.setTextColor(GxEPD_BLACK);
        rect_t rect = get_text_bounds(message, 0, 0);

        int16_t x = 0, y = 0;
        switch (gravity) {
        case TOP_CENTER: {
            x = (DISP_WIDTH - rect.width - icon_size - 2) / 2; // leave space for icon
            y = -rect.y;
            break;
        }

        case TOP_LEFT: {
            x = 0;
            y = -rect.y;
            break;
        }
        case TOP_RIGHT: {
            x = DISP_WIDTH - rect.width - 20;
            y = -rect.y;
            break;
        }
        case BOTTOM_LEFT: {
            x = 0;
            y = DISP_HEIGHT - rect.y;
            break;
        }
        case BOTTOM_RIGHT: {
            x = DISP_WIDTH - rect.width - 20;
            y = DISP_HEIGHT - rect.y;
            break;
        }
        default: {
            Serial.println("drawSizeMessageWithIcon: Invalid gravity type");
            return;
        }
        }

        // Draw the icon
        display.drawInvertedBitmap(
            x + x_offset, y - y + y_offset, icon, icon_size, icon_size, GxEPD_BLACK);
        // Draw the message next to the icon
        draw_string(x + icon_size + 2, y, message, GxEPD_BLACK);
    }

    bool draw_bitmap_from_file(File& file, const char* filename, int16_t x, int16_t y, bool with_color)
    {
        if (!file) {
            Serial.println("File not open or invalid");
            return false;
        }

        Serial.println("drawBitmapFromFile: " + String(filename));

        bool valid = false; // valid format to be handled
        bool flip = true; // bitmap is stored bottom-to-top
        uint32_t startTime = millis();
        if ((x >= display.epd2.WIDTH) || (y >= display.epd2.HEIGHT))
            return false;

        // Parse BMP header
        auto signature = read16(file);
        Serial.print("BMP signature: ");
        Serial.println(signature, HEX);

        if (signature == 0x4D42) // BMP signature
        {
            uint32_t fileSize = read32(file);
            uint32_t creatorBytes = read32(file);
            (void)creatorBytes; // unused
            uint32_t imageOffset = read32(file); // Start of image data
            uint32_t headerSize = read32(file);
            uint32_t width = read32(file);
            int32_t height = (int32_t)read32(file);
            uint16_t planes = read16(file);
            uint16_t depth = read16(file); // bits per pixel
            uint32_t format = read32(file);
            Serial.println("planes: " + String(planes));
            Serial.println("format: " + String(format));
            if ((planes == 1) && ((format == 0) || (format == 3))) // uncompressed is handled, 565 also
            {
                Serial.print("File size: ");
                Serial.println(fileSize);
                Serial.print("Image Offset: ");
                Serial.println(imageOffset);
                Serial.print("Header size: ");
                Serial.println(headerSize);
                Serial.print("Bit Depth: ");
                Serial.println(depth);
                Serial.print("Image size: ");
                Serial.print(width);
                Serial.print('x');
                Serial.println(height);
                // BMP rows are padded (if needed) to 4-byte boundary
                uint32_t rowSize = (width * depth / 8 + 3) & ~3;
                if (depth < 8)
                    rowSize = ((width * depth + 8 - depth) / 8 + 3) & ~3;
                if (height < 0) {
                    height = -height;
                    flip = false;
                }
                uint16_t w = width;
                uint16_t h = height;
                if ((x + w - 1) >= display.epd2.WIDTH)
                    w = display.epd2.WIDTH - x;
                if ((y + h - 1) >= display.epd2.HEIGHT)
                    h = display.epd2.HEIGHT - y;
                if (w <= max_row_width) // handle with direct drawing
                {
                    valid = true;
                    uint8_t bitmask = 0xFF;
                    uint8_t bitshift = 8 - depth;
                    uint16_t red, green, blue;
                    bool whitish = false;
                    bool colored = false;
                    if (depth == 1)
                        with_color = false;
                    if (depth <= 8) {
                        if (depth < 8)
                            bitmask >>= depth;
                        // file.seek(54); //palette is always @ 54
                        file.seek(imageOffset - (4 << depth)); // 54 for regular, diff for colorsimportant
                        for (uint16_t pn = 0; pn < (1 << depth); pn++) {
                            blue = file.read();
                            green = file.read();
                            red = file.read();
                            file.read();
                            // Serial.print(red); Serial.print(" "); Serial.print(green); Serial.print("
                            // "); Serial.println(blue);
                            whitish = with_color ? ((red > 0x80) && (green > 0x80) && (blue > 0x80))
                                                 : ((red + green + blue) > 3 * 0x80); // whitish
                            colored = (red > 0xF0) || ((green > 0xF0) && (blue > 0xF0)); // reddish or yellowish?
                            if (0 == pn % 8)
                                mono_palette_buffer[pn / 8] = 0;
                            mono_palette_buffer[pn / 8] |= whitish << pn % 8;
                            if (0 == pn % 8)
                                color_palette_buffer[pn / 8] = 0;
                            color_palette_buffer[pn / 8] |= colored << pn % 8;

                            // Yield every 16 palette entries to prevent watchdog timeout
                            if (pn % 16 == 0) {
                                yield();
                            }
                        }
                    }
                    // display.clearScreen();
                    uint32_t rowPosition = flip ? imageOffset + (height - h) * rowSize : imageOffset;
                    for (uint16_t row = 0; row < h; row++, rowPosition += rowSize) // for each line
                    {
                        // Yield every 10 rows to prevent watchdog timeout
                        if (row % 10 == 0) {
                            yield();
                        }

                        uint32_t in_remain = rowSize;
                        uint32_t in_idx = 0;
                        uint32_t in_bytes = 0;
                        uint8_t in_byte = 0; // for depth <= 8
                        uint8_t in_bits = 0; // for depth <= 8
                        uint8_t out_byte = 0xFF; // white (for w%8!=0 border)
                        uint8_t out_color_byte = 0xFF; // white (for w%8!=0 border)
                        uint32_t out_idx = 0;
                        file.seek(rowPosition);
                        for (uint16_t col = 0; col < w; col++) // for each pixel
                        {
                            // Time to read more pixel data?
                            if (in_idx >= in_bytes) // ok, exact match for 24bit also (size IS multiple of 3)
                            {
                                in_bytes = file.read(input_buffer,
                                    in_remain > sizeof(input_buffer) ? sizeof(input_buffer)
                                                                     : in_remain);
                                in_remain -= in_bytes;
                                in_idx = 0;
                            }
                            switch (depth) {
                            case 32:
                                blue = input_buffer[in_idx++];
                                green = input_buffer[in_idx++];
                                red = input_buffer[in_idx++];
                                in_idx++; // skip alpha
                                whitish = with_color ? ((red > 0x80) && (green > 0x80) && (blue > 0x80))
                                                     : ((red + green + blue) > 3 * 0x80); // whitish
                                colored = (red > 0xF0) || ((green > 0xF0) && (blue > 0xF0)); // reddish or yellowish?
                                break;
                            case 24:
                                blue = input_buffer[in_idx++];
                                green = input_buffer[in_idx++];
                                red = input_buffer[in_idx++];
                                whitish = with_color ? ((red > 0x80) && (green > 0x80) && (blue > 0x80))
                                                     : ((red + green + blue) > 3 * 0x80); // whitish
                                colored = (red > 0xF0) || ((green > 0xF0) && (blue > 0xF0)); // reddish or yellowish?
                                break;
                            case 16: {
                                uint8_t lsb = input_buffer[in_idx++];
                                uint8_t msb = input_buffer[in_idx++];
                                if (format == 0) // 555
                                {
                                    blue = (lsb & 0x1F) << 3;
                                    green = ((msb & 0x03) << 6) | ((lsb & 0xE0) >> 2);
                                    red = (msb & 0x7C) << 1;
                                } else // 565
                                {
                                    blue = (lsb & 0x1F) << 3;
                                    green = ((msb & 0x07) << 5) | ((lsb & 0xE0) >> 3);
                                    red = (msb & 0xF8);
                                }
                                whitish = with_color ? ((red > 0x80) && (green > 0x80) && (blue > 0x80))
                                                     : ((red + green + blue) > 3 * 0x80); // whitish
                                colored = (red > 0xF0) || ((green > 0xF0) && (blue > 0xF0)); // reddish or yellowish?
                            } break;
                            case 1:
                            case 2:
                            case 4:
                            case 8: {
                                if (0 == in_bits) {
                                    in_byte = input_buffer[in_idx++];
                                    in_bits = 8;
                                }
                                uint16_t pn = (in_byte >> bitshift) & bitmask;
                                whitish = mono_palette_buffer[pn / 8] & (0x1 << pn % 8);
                                colored = color_palette_buffer[pn / 8] & (0x1 << pn % 8);
                                in_byte <<= depth;
                                in_bits -= depth;
                            } break;
                            }
                            if (whitish) {
                                // keep white
                            } else if (colored && with_color) {
                                out_color_byte &= ~(0x80 >> col % 8); // colored
                            } else {
                                out_byte &= ~(0x80 >> col % 8); // black
                            }
                            if ((7 == col % 8) || (col == w - 1)) // write that last byte! (for w%8!=0 border)
                            {
                                output_row_color_buffer[out_idx] = out_color_byte;
                                output_row_mono_buffer[out_idx++] = out_byte;
                                out_byte = 0xFF; // white (for w%8!=0 border)
                                out_color_byte = 0xFF; // white (for w%8!=0 border)
                            }

                            // Yield every 100 pixels to prevent watchdog timeout
                            if (col % 100 == 0) {
                                yield();
                            }
                        } // end pixel
                        uint16_t yrow = y + (flip ? h - row - 1 : row);
                        display.writeImage(
                            output_row_mono_buffer, output_row_color_buffer, x, yrow, w, 1);
                    } // end line
                    Serial.print("loaded in ");
                    Serial.print(millis() - startTime);
                    Serial.println(" ms");
                    // display.refresh();
                    // display.refresh(true); // use partial update
                }
            }
        }
        if (!valid) {
            Serial.println("bitmap format not handled.");
            return false;
        } else {
            Serial.println("bitmap loaded successfully.");
            return true;
        }
    } // end drawBitmapFromFile

    bool draw_binary_from_file(File& file, const char* filename, int width, int height)
    {
        Serial.print("draw_binary_from_file: ");
        Serial.print(filename);
        Serial.print(", width: ");
        Serial.print(width);
        Serial.print(", height: ");
        Serial.println(height);

        if (width <= 0 || height <= 0) {
            Serial.println("Invalid width or height");
            return false;
        } else if (width > display.width() || height > display.height()) {
            Serial.println("Width or height exceeds display dimensions");
            return false;
        }

        if (!file) {
            Serial.println("File not open or invalid");
            return false;
        }

        display.fillScreen(GxEPD_WHITE);
        auto startTime = millis();
        uint32_t idx = 0;

        for (int y = 0; y < height; y++) {
            // Yield every 10 rows to prevent watchdog timeout
            if (y % 10 == 0) {
                yield();
            }

            for (int x = 0; x < width; x++) {
                uint8_t color;
                uint16_t pixelColor;
                idx = (y * width + x);

                int byteRead = 0;

                // Move to the pixel position in the file
                if (file.seek(idx)) {
                    byteRead = file.read();
                }

                if (byteRead >= 0) {
                    color = static_cast<uint8_t>(byteRead); // Read the byte as color value
                    photo_frame::renderer::color2Epd(color, pixelColor, x, y);
                    display.writePixel(x, y, pixelColor);
                } else {
                    Serial.print("Error reading pixel data at position (");
                    Serial.print(x);
                    Serial.print(", ");
                    Serial.print(y);
                    Serial.print(") - index: ");
                    Serial.print(idx);
                    Serial.print(" - file position: ");
                    Serial.println(file.position());
                    Serial.println("Exiting early due to read error.");
                    return false; // Exit early if read error occurs
                }

                // Yield every 100 pixels to prevent watchdog timeout
                if (x % 100 == 0) {
                    yield();
                }
            }
        }
        auto elapsedTime = millis() - startTime;
        Serial.print("Image rendered in: ");
        Serial.print(elapsedTime);
        Serial.println(" ms");
        Serial.println("Binary file processed successfully");
        return true;
    }

    bool draw_binary_from_file_buffered(File& file, const char* filename, int width, int height)
    {
        Serial.print("draw_binary_from_file_buffered: ");
        Serial.print(filename);
        Serial.print(", width: ");
        Serial.print(width);
        Serial.print(", height: ");
        Serial.println(height);

        if (width <= 0 || height <= 0) {
            Serial.println("Invalid width or height");
            return false;
        } else if (width > display.width() || height > display.height()) {
            Serial.println("Width or height exceeds display dimensions");
            return false;
        }

        if (!file) {
            Serial.println("File not open or invalid");
            return false;
        }

        display.setFullWindow();
        display.firstPage();

        do {
            display.fillScreen(GxEPD_WHITE);
            auto startTime = millis();
            uint32_t idx = 0;

            for (int y = 0; y < height; y++) {
                // Yield every 10 rows to prevent watchdog timeout
                if (y % 10 == 0) {
                    yield();
                }

                for (int x = 0; x < width; x++) {
                    uint8_t color;
                    uint16_t pixelColor;
                    idx = (y * width + x);

                    int byteRead = 0;

                    // Move to the pixel position in the file
                    if (file.seek(idx)) {
                        byteRead = file.read();
                    }

                    if (byteRead >= 0) {
                        color = static_cast<uint8_t>(byteRead); // Read the byte as color value
                        photo_frame::renderer::color2Epd(color, pixelColor, x, y);
                        display.drawPixel(x, y, pixelColor);
                    } else {
                        Serial.print("Error reading pixel data at position (");
                        Serial.print(x);
                        Serial.print(", ");
                        Serial.print(y);
                        Serial.print(") - index: ");
                        Serial.print(idx);
                        Serial.print(" - file position: ");
                        Serial.println(file.position());
                        Serial.println("Exiting early due to read error.");
                        return false; // Exit early if read error occurs
                    }

                    // Yield every 100 pixels to prevent watchdog timeout
                    if (x % 100 == 0) {
                        yield();
                    }
                }
            }
            auto elapsedTime = millis() - startTime;
            Serial.print("Page rendered in: ");
            Serial.print(elapsedTime);
            Serial.println(" ms");
        } while (display.nextPage());
        Serial.println("Binary file processed successfully");
        return true; // Return true if the file was processed successfully
    }

    bool draw_bitmap_from_file_buffered(File& file,
        const char* filename,
        int16_t x,
        int16_t y,
        bool with_color,
        bool partial_update)
    {

        if (!file) {
            Serial.println("File not open or invalid");
            return false;
        }
        Serial.println("drawBitmapFromSD_Buffered: " + String(filename) + ", x: " + String(x) + ", y: " + String(y) + ", with_color: " + String(with_color) + ", partial_update: " + String(partial_update));

        bool valid = false; // valid format to be handled
        bool flip = true; // bitmap is stored bottom-to-top
        bool has_multicolors = (display.epd2.panel == GxEPD2::ACeP565) || (display.epd2.panel == GxEPD2::GDEY073D46);
        uint32_t startTime = millis();
        if ((x >= display.width()) || (y >= display.height())) {
            Serial.println("drawBitmapFromSD_Buffered: x or y out of bounds");
            return false;
        }
        // Parse BMP header

        uint16_t signature = read16(file);
        if (signature == 0x4D42) // BMP signature
        {
            Serial.println("BMP signature: " + String(signature));

            uint32_t fileSize = read32(file);
            uint32_t creatorBytes = read32(file);
            (void)creatorBytes; // unused
            uint32_t imageOffset = read32(file); // Start of image data
            uint32_t headerSize = read32(file);
            uint32_t width = read32(file);
            int32_t height = (int32_t)read32(file);
            uint16_t planes = read16(file);
            uint16_t depth = read16(file); // bits per pixel
            uint32_t format = read32(file);

            Serial.println("planes: " + String(planes));
            Serial.println("format: " + String(format));

            if ((planes == 1) && ((format == 0) || (format == 3))) // uncompressed is handled, 565 also
            {
                Serial.print("File size: ");
                Serial.println(fileSize);
                Serial.print("Image Offset: ");
                Serial.println(imageOffset);
                Serial.print("Header size: ");
                Serial.println(headerSize);
                Serial.print("Bit Depth: ");
                Serial.println(depth);
                Serial.print("Image size: ");
                Serial.print(width);
                Serial.print('x');
                Serial.println(height);
                // BMP rows are padded (if needed) to 4-byte boundary
                uint32_t rowSize = (width * depth / 8 + 3) & ~3;
                if (depth < 8)
                    rowSize = ((width * depth + 8 - depth) / 8 + 3) & ~3;
                if (height < 0) {
                    height = -height;
                    flip = false;
                }
                uint16_t w = width;
                uint16_t h = height;
                if ((x + w - 1) >= display.width())
                    w = display.width() - x;
                if ((y + h - 1) >= display.height())
                    h = display.height() - y;
                // if (w <= max_row_width) // handle with direct drawing
                {
                    valid = true;
                    uint8_t bitmask = 0xFF;
                    uint8_t bitshift = 8 - depth;
                    uint16_t red, green, blue;
                    bool whitish = false;
                    bool colored = false;
                    if (depth == 1)
                        with_color = false;
                    if (depth <= 8) {
                        if (depth < 8)
                            bitmask >>= depth;
                        // file.seek(54); //palette is always @ 54
                        file.seek(imageOffset - (4 << depth)); // 54 for regular, diff for colorsimportant
                        for (uint16_t pn = 0; pn < (1 << depth); pn++) {
                            blue = file.read();
                            green = file.read();
                            red = file.read();
                            file.read();
                            whitish = with_color ? ((red > 0x80) && (green > 0x80) && (blue > 0x80))
                                                 : ((red + green + blue) > 3 * 0x80); // whitish
                            colored = (red > 0xF0) || ((green > 0xF0) && (blue > 0xF0)); // reddish or yellowish?
                            if (0 == pn % 8)
                                mono_palette_buffer[pn / 8] = 0;
                            mono_palette_buffer[pn / 8] |= whitish << pn % 8;
                            if (0 == pn % 8)
                                color_palette_buffer[pn / 8] = 0;
                            color_palette_buffer[pn / 8] |= colored << pn % 8;
                            rgb_palette_buffer[pn] = ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | ((blue & 0xF8) >> 3);

                            // Yield every 16 palette entries to prevent watchdog timeout
                            if (pn % 16 == 0) {
                                yield();
                            }
                        }
                    }
                    if (partial_update)
                        display.setPartialWindow(x, y, w, h);
                    else
                        display.setFullWindow();
                    display.firstPage();
                    display.fillScreen(GxEPD_WHITE); // clear screen if not overwriting
                    do {
                        uint32_t rowPosition = flip ? imageOffset + (height - h) * rowSize : imageOffset;
                        for (uint16_t row = 0; row < h; row++, rowPosition += rowSize) // for each line
                        {
                            // Yield every 10 rows to prevent watchdog timeout
                            if (row % 10 == 0) {
                                yield();
                            }

                            uint32_t in_remain = rowSize;
                            uint32_t in_idx = 0;
                            uint32_t in_bytes = 0;
                            uint8_t in_byte = 0; // for depth <= 8
                            uint8_t in_bits = 0; // for depth <= 8
                            uint16_t color = GxEPD_WHITE;
                            file.seek(rowPosition);
                            for (uint16_t col = 0; col < w; col++) // for each pixel
                            {
                                // Time to read more pixel data?
                                if (in_idx >= in_bytes) // ok, exact match for 24bit also (size IS multiple of 3)
                                {
                                    in_bytes = file.read(input_buffer,
                                        in_remain > sizeof(input_buffer)
                                            ? sizeof(input_buffer)
                                            : in_remain);
                                    in_remain -= in_bytes;
                                    in_idx = 0;
                                }
                                switch (depth) {
                                case 32:
                                    blue = input_buffer[in_idx++];
                                    green = input_buffer[in_idx++];
                                    red = input_buffer[in_idx++];
                                    in_idx++; // skip alpha
                                    whitish = with_color
                                        ? ((red > 0x80) && (green > 0x80) && (blue > 0x80))
                                        : ((red + green + blue) > 3 * 0x80); // whitish
                                    colored = (red > 0xF0) || ((green > 0xF0) && (blue > 0xF0)); // reddish or yellowish?
                                    color = ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | ((blue & 0xF8) >> 3);
                                    break;
                                case 24:
                                    blue = input_buffer[in_idx++];
                                    green = input_buffer[in_idx++];
                                    red = input_buffer[in_idx++];
                                    whitish = with_color
                                        ? ((red > 0x80) && (green > 0x80) && (blue > 0x80))
                                        : ((red + green + blue) > 3 * 0x80); // whitish
                                    colored = (red > 0xF0) || ((green > 0xF0) && (blue > 0xF0)); // reddish or yellowish?
                                    color = ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | ((blue & 0xF8) >> 3);
                                    break;
                                case 16: {
                                    uint8_t lsb = input_buffer[in_idx++];
                                    uint8_t msb = input_buffer[in_idx++];
                                    if (format == 0) // 555
                                    {
                                        blue = (lsb & 0x1F) << 3;
                                        green = ((msb & 0x03) << 6) | ((lsb & 0xE0) >> 2);
                                        red = (msb & 0x7C) << 1;
                                        color = ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | ((blue & 0xF8) >> 3);
                                    } else // 565
                                    {
                                        blue = (lsb & 0x1F) << 3;
                                        green = ((msb & 0x07) << 5) | ((lsb & 0xE0) >> 3);
                                        red = (msb & 0xF8);
                                        color = (msb << 8) | lsb;
                                    }
                                    whitish = with_color
                                        ? ((red > 0x80) && (green > 0x80) && (blue > 0x80))
                                        : ((red + green + blue) > 3 * 0x80); // whitish
                                    colored = (red > 0xF0) || ((green > 0xF0) && (blue > 0xF0)); // reddish or yellowish?
                                } break;
                                case 1:
                                case 2:
                                case 4:
                                case 8: {
                                    if (0 == in_bits) {
                                        in_byte = input_buffer[in_idx++];
                                        in_bits = 8;
                                    }
                                    uint16_t pn = (in_byte >> bitshift) & bitmask;
                                    whitish = mono_palette_buffer[pn / 8] & (0x1 << pn % 8);
                                    colored = color_palette_buffer[pn / 8] & (0x1 << pn % 8);
                                    in_byte <<= depth;
                                    in_bits -= depth;
                                    color = rgb_palette_buffer[pn];
                                } break;
                                }
                                if (with_color && has_multicolors) {
                                    // keep color
                                } else if (whitish) {
                                    color = GxEPD_WHITE;
                                } else if (colored && with_color) {
                                    // color = GxEPD_COLORED;
                                } else {
                                    // color = GxEPD_BLACK;
                                }
                                uint16_t yrow = y + (flip ? h - row - 1 : row);
                                display.drawPixel(x + col, yrow, color);

                                // Yield every 100 pixels to prevent watchdog timeout
                                if (col % 100 == 0) {
                                    yield();
                                }
                            } // end pixel
                        } // end line
                        Serial.print("page loaded in ");
                        Serial.print(millis() - startTime);
                        Serial.println(" ms");
                    } while (display.nextPage());
                    Serial.print("loaded in ");
                    Serial.print(millis() - startTime);
                    Serial.println(" ms");
                }
            }
        }
        if (!valid) {
            Serial.println("bitmap format not handled (signature: " + String(signature) + ").");
            return false;
        } else {
            Serial.println("bitmap loaded successfully.");
            return true;
        }
    } // end drawBitmapFromFile_Buffered

#if defined(DISP_6C) || defined(DISP_7C_F)

    bool is_dominant(uint8_t r, uint8_t g, uint8_t b)
    {
        return (r >= g && r >= b) || (g >= r && g >= b) || (b >= r && b >= g);
    };

    bool is_light_grey(uint8_t r, uint8_t g, uint8_t b)
    {
        return (r >= 5 && g >= 5 && b >= 2);
    };

    bool is_dark_grey(uint8_t r, uint8_t g, uint8_t b)
    {
        return (r < 3 && g < 3 && b < 2);
    };

    bool is_yellow(uint8_t r, uint8_t g, uint8_t b)
    {
        return (r > 3 && g > 3 && b < 2);
    };

#endif // DISP_6C

    void color2Epd(uint8_t color, uint16_t& pixelColor, int x, int y)
    {
#ifdef DISP_6C
        switch (color) {
        case 0xff: // 255
            pixelColor = GxEPD_WHITE;
            break;
        case 0x00: // 0
            pixelColor = GxEPD_BLACK;
            break;
        case 0xe0: // 224
            pixelColor = GxEPD_RED;
            break;
        case 0x1c: // 28
            pixelColor = GxEPD_GREEN;
            break;
        case 0xfc: // 252
            pixelColor = GxEPD_YELLOW;
            break;
        case 0x03: // 3
            pixelColor = GxEPD_BLUE;
            break;
        default:
            // extract the rgb values from the color byte
            uint8_t r = (color >> 5) & 0x07; // from 0 to 7
            uint8_t g = (color >> 2) & 0x07; // from 0 to 7
            uint8_t b = color & 0x03; // from 0 to 3

            // Improved color mapping: use the dominant channel to determine color
            if (is_dominant(r, g, b)) {
                // Use the dominant channel to determine the color
                if (r > g && r > b) {
                    pixelColor = GxEPD_RED;
                } else if (g > r && g > b) {
                    pixelColor = GxEPD_GREEN;
                } else if (b > r && b > g) {
                    pixelColor = GxEPD_BLUE;
                } else {
                    pixelColor = GxEPD_WHITE; // Fallback to white if no dominant channel
                }
            } else {
                if (is_yellow(r, g, b)) {
                    pixelColor = GxEPD_YELLOW;
                } else if (is_light_grey(r, g, b)) {
                    pixelColor = GxEPD_WHITE; // Light grey
                } else if (is_dark_grey(r, g, b)) {
                    pixelColor = GxEPD_BLACK; // Dark grey
                } else {
                    // Fallback to white if no match
                    pixelColor = GxEPD_WHITE;

                    uint8_t r8 = (r * 255) / 7; // scale to 0-255
                    uint8_t g8 = (g * 255) / 7; // scale to 0-255
                    uint8_t b8 = (b * 255) / 3; // scale to 0-255

                    Serial.print("Unknown color: ");
                    Serial.print(color, HEX);
                    Serial.print(" - RGB values: R=");
                    Serial.print(r);
                    Serial.print(", G=");
                    Serial.print(g);
                    Serial.print(", B=");
                    Serial.print(b);

                    Serial.print(" - RGB8 values: R=");
                    Serial.print(r8);
                    Serial.print(", G=");
                    Serial.print(g8);
                    Serial.print(", B=");
                    Serial.print(b8);

                    Serial.print(" - at position (");
                    Serial.print(x);
                    Serial.print(", ");
                    Serial.print(y);
                    Serial.println(")");
                }
            }
        }

#elif defined(DISP_7C_F)
#error "DISP_7C_F is not supported in this code snippet. Please define the color2Epd function for DISP_7C_F."

#elif defined(DISP_BW_V2)
        if (color > 127) {
            pixelColor = GxEPD_WHITE; // 255
        } else {
            pixelColor = GxEPD_BLACK; // 0
        }
#endif
    }

} // namespace renderer

} // namespace photo_frame