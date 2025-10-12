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
#include "google_drive.h"

#include FONT_HEADER

#if !defined(USE_DESPI_DRIVER) && !defined(USE_WAVESHARE_DRIVER)
#error "Please define USE_DESPI_DRIVER or USE_WAVESHARE_DRIVER."
#endif

#if defined(USE_DESPI_DRIVER) && defined(USE_WAVESHARE_DRIVER)
#error "Please define only one display driver: either USE_DESPI_DRIVER or USE_WAVESHARE_DRIVER."
#endif

GxEPD2_DISPLAY_CLASS<GxEPD2_DRIVER_CLASS, MAX_HEIGHT(GxEPD2_DRIVER_CLASS)>
    display(GxEPD2_DRIVER_CLASS(EPD_CS_PIN, EPD_DC_PIN, EPD_RST_PIN, EPD_BUSY_PIN));

namespace photo_frame {
namespace renderer {

void color2Epd(uint8_t color, uint16_t& pixelColor, int x, int y);

// static const uint16_t input_buffer_pixels = 20; // may affect performance
static const uint16_t input_buffer_pixels = 200; // Reduced to save DRAM (was 800, 600 bytes saved)

static const uint16_t max_row_width       = 800; // Optimized for 800x480 display (was 1448)
static const uint16_t max_palette_pixels  = 256; // for depth <= 8

// Static buffers - using plenty of available memory
uint8_t input_buffer[3 * input_buffer_pixels];        // up to depth 24
uint8_t output_row_mono_buffer[max_row_width / 8];    // buffer for at least one row of b/w bits
uint8_t output_row_color_buffer[max_row_width / 8];   // buffer for at least one row of color bits
uint8_t mono_palette_buffer[max_palette_pixels / 8];  // palette buffer for depth <= 8 b/w
uint8_t color_palette_buffer[max_palette_pixels / 8]; // palette buffer for depth <= 8 c/w
uint16_t rgb_palette_buffer[max_palette_pixels];      // palette buffer for depth <= 8 for buffered
                                                      // graphics, needed for 7-color display

uint16_t read8(File& f) {
    uint8_t result;
    f.read(&result, 1); // read the next byte to move the file pointer
    return result;
}

uint16_t read16(File& f) {
    // BMP data is stored little-endian, same as Arduino.
    uint16_t result;
    ((uint8_t*)&result)[0] = f.read(); // LSB
    ((uint8_t*)&result)[1] = f.read(); // MSB
    return result;
}

uint32_t read32(File& f) {
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
void init_display() {
    Serial.println("[renderer] Initializing e-paper display...");

    if (!digitalPinIsValid(EPD_BUSY_PIN) || !digitalPinIsValid(EPD_RST_PIN) ||
        !digitalPinIsValid(EPD_DC_PIN) || !digitalPinIsValid(EPD_CS_PIN)) {
        Serial.println("[renderer] Invalid e-paper display pins!");
        return;
    }

    SPI.end();
    SPI.begin(EPD_SCK_PIN, -1, EPD_MOSI_PIN, EPD_CS_PIN); // remap SPI for EPD, -1 for MISO (not used)

#ifdef USE_DESPI_DRIVER
    display.init(115200);
#else  // USE_WAVESHARE_DRIVER
    display.init(115200, true, 2, false);
#endif // USE_DESPI_DRIVER

    display.setRotation(0);
    display.setTextSize(1);
    // display.clearScreen(); // black
    display.setTextColor(GxEPD_BLACK);
    display.setTextWrap(false);

#if defined(DISP_6C) || defined(DISP_7C)
    // For GDEP073E01 6-color display: power stabilization only
    Serial.println("[renderer] GDEP073E01 6-color display detected - applying power stabilization...");
    delay(500);
    Serial.println("[renderer] Power stabilization delay applied");

#endif

    auto pages = display.pages();
    Serial.printf("[renderer] Display pages: %d\n", pages);
    if (pages > 1) {
        Serial.printf("[renderer] Display page height: %d\n", display.pageHeight());
    }
    Serial.printf("[renderer] Display width: %d\n", display.width());
    Serial.printf("[renderer] Display height: %d\n", display.height());
    Serial.printf("[renderer] Full refresh time: %d\n", display.epd2.full_refresh_time);
    Serial.printf("[renderer] Display has color: %s\n", display.epd2.hasColor ? "true" : "false");
    Serial.printf("[renderer] Display has partial update: %s\n",
                  display.epd2.hasPartialUpdate ? "true" : "false");
    Serial.printf("[renderer] Display has fast partial update: %s\n",
                  display.epd2.hasFastPartialUpdate ? "true" : "false");

    // Static buffers are ready to use - no allocation needed

    // display.setFullWindow();
    // display.fillScreen(GxEPD_WHITE);
    // display.firstPage(); // use paged drawing mode, sets fillScreen(GxEPD_WHITE)
    return;
} // end initDisplay

/*
 * Power-off e-paper display
 */
void power_off() {
    Serial.println("[renderer] Powering off e-paper display...");

    // Static buffers - no need to free

    // display.hibernate();
    display.powerOff(); // turns off the display
    delay(100);         // wait for the display to power off
    display.end();      // release SPI and control pins
    SPI.end();          // release SPI pins
    return;
} // end powerOff

void refresh(bool partial_update_mode) { display.refresh(partial_update_mode); }

void fill_screen(uint16_t color) { display.fillScreen(color); }

bool has_partial_update() {
    // check if the display supports partial update
    return display.epd2.hasPartialUpdate;
} // end hasPartialUpdate

bool has_color() {
    return display.epd2.hasColor;
} // end has_color

bool has_fast_partial_update() {
    // check if the display supports fast partial update
    return display.epd2.hasFastPartialUpdate;
} // end hasFastPartialUpdate

/*
 * Returns the string width in pixels
 */
uint16_t get_string_width(const String& text) {
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
    return w;
}

/*
 * Returns the string height in pixels
 */
uint16_t get_string_height(const String& text) {
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
    return h;
}

rect_t get_text_bounds(const char* text, int16_t x, int16_t y) {
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(text, x, y, &x1, &y1, &w, &h);
    return rect_t::from_xywh(x1, y1, w, h);
}

/*
 * Draws a string with alignment
 */
void draw_string(int16_t x, int16_t y, const char* text, alignment_t alignment, uint16_t color) {
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
void draw_string(int16_t x, int16_t y, const char* text, uint16_t color) {
#ifdef DEBUG_RENDERER
    Serial.printf("[renderer] drawString: x=%d y=%d text='%s' color=0x%04X\n", x, y, text, color);
#endif // DEBUG_RENDERER
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
                           uint16_t color) {
    uint16_t current_line = 0;
    String textRemaining  = text;
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
        String subStr    = textRemaining;
        int splitAt      = 0;
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
                endIndex      = splitAt;
                subStr        = subStr.substring(0, endIndex + 1);

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

void draw_error(photo_frame_error error) {
    if (error == error_type::None) {
        Serial.println("[renderer] drawError: No error to display");
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

    draw_error(bitmap_196x196, error.format_for_display(), "");
}

void draw_error_message(const gravity_t gravity, const uint8_t code) {
    String errMsgLn1 = String(TXT_ERROR_CODE) + code;
    errMsgLn1 += " - ";
    errMsgLn1 += String(FIRMWARE_VERSION_STRING);

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

void draw_error(const uint8_t* bitmap_196x196, const String& errMsgLn1, const String& errMsgLn2) {
#ifdef DEBUG_RENDERER
    Serial.println("[renderer] drawError[1]: " + errMsgLn1);
    if (!errMsgLn2.isEmpty()) {
        Serial.println("[renderer] drawError[2]: " + errMsgLn2);
    }
#endif // DEBUG_RENDERER

    // Position error text higher on screen (no icon space needed)
    // int16_t baseY = DISP_HEIGHT / 2 - 60; // Higher position without icon
    int16_t baseY = DISP_HEIGHT / 2 + 196 / 2 - 21;

    display.setFont(&FONT_26pt8b);
    if (!errMsgLn2.isEmpty()) {
        draw_string(DISP_WIDTH / 2, baseY, errMsgLn1.c_str(), CENTER);
        draw_string(DISP_WIDTH / 2, baseY + 55, errMsgLn2.c_str(), CENTER);
    } else {
        draw_multiline_string(DISP_WIDTH / 2, baseY, errMsgLn1, CENTER, DISP_WIDTH - 200, 2, 55);
    }

    display.drawInvertedBitmap(DISP_WIDTH / 2 - 196 / 2,
                               DISP_HEIGHT / 2 - 196 / 2 - 42,
                               bitmap_196x196,
                               196,
                               196,
                               ACCENT_COLOR);

    return;
} // end drawError

void draw_error_with_details(const String& errMsgLn1,
                             const String& errMsgLn2,
                             const char* filename,
                             uint16_t errorCode) {
#ifdef DEBUG_RENDERER
    Serial.println("[renderer] drawErrorWithDetails[1]: " + errMsgLn1);
    if (!errMsgLn2.isEmpty()) {
        Serial.println("[renderer] drawErrorWithDetails[2]: " + errMsgLn2);
    }
    Serial.printf("[renderer] drawErrorWithDetails filename: %s, code: %d\n",
                  filename ? filename : "unknown",
                  errorCode);
#endif // DEBUG_RENDERER

    // Position error text higher on screen (no icon space needed)
    int16_t baseY = DISP_HEIGHT / 2 - 80; // Higher position for more content

    // Draw main error messages
    display.setFont(&FONT_26pt8b);
    if (!errMsgLn2.isEmpty()) {
        draw_string(DISP_WIDTH / 2, baseY, errMsgLn1.c_str(), CENTER);
        draw_string(DISP_WIDTH / 2, baseY + 55, errMsgLn2.c_str(), CENTER);
        baseY += 110; // Move to next line after 2 large lines
    } else {
        draw_multiline_string(DISP_WIDTH / 2, baseY, errMsgLn1, CENTER, DISP_WIDTH - 200, 2, 55);
        baseY += 55; // Move to next line after 1 large line
    }

    // Add small line with filename and error code
    display.setFont(&FONT_9pt8b);
    String detailLine = "File: ";
    if (filename && strlen(filename) > 0) {
        // Extract just the filename from the path
        const char* baseName = strrchr(filename, '/');
        if (baseName) {
            baseName++; // Skip the '/'
        } else {
            baseName = filename;
        }
        detailLine += String(baseName);
    } else {
        detailLine += "unknown";
    }
    detailLine += " | Error: " + String(errorCode);

    draw_string(DISP_WIDTH / 2, baseY + 30, detailLine.c_str(), CENTER, GxEPD_BLACK);

    return;
} // end drawErrorWithDetails

void draw_image_info(uint32_t index,
                     uint32_t total_images,
                     photo_frame::image_source_t image_source) {
#ifdef DEBUG_RENDERER
    Serial.println("[renderer] drawImageInfo: " + String(index) + " / " + String(total_images));
#endif // DEBUG_RENDERER
    String message = String(index + 1) + " / " + String(total_images);
    draw_side_message_with_icon(gravity::TOP_CENTER,
                                image_source == photo_frame::image_source_t::IMAGE_SOURCE_CLOUD
                                    ? icon_name::cloud_0deg
                                    : icon_name::micro_sd_card_0deg,
                                message.c_str(),
                                -4,
                                -2);
}


rect_t get_weather_info_rect() { return {16 - 8, DISP_HEIGHT - 88 - 8, 184, 88}; }

void draw_weather_info(const photo_frame::weather::WeatherData& weather_data, rect_t box_rect) {
    // Only display if weather data is valid and not stale (max 3 hours old)
    if (!weather_data.is_displayable(WEATHER_MAX_AGE_SECONDS)) {
        Serial.println(
            "[renderer] drawWeatherInfo: Weather data invalid or stale, skipping display");
        return;
    }

#ifdef DEBUG_RENDERER
    Serial.printf("[renderer] drawWeatherInfo: %.1f°C, %s\n",
                  weather_data.temperature,
                  weather_data.description.c_str());
#endif // DEBUG_RENDERER

    // Calculate position based on gravity (positioned in bottom-left area like the image)
    int16_t box_x         = box_rect.x;
    int16_t box_y         = box_rect.y;
    uint16_t box_width    = box_rect.width;
    uint16_t box_height   = box_rect.height;

    int16_t base_x        = box_x + 8;
    int16_t base_y        = box_y + 8;

    auto text_color       = GxEPD_WHITE;
    auto icon_color       = GxEPD_WHITE;
    auto background_color = GxEPD_BLACK;

    // Draw rounded background box using reusable function with 50% transparency
    // draw_rounded_rect(box_x, box_y, box_width, box_height, 10, background_color, 128);
    display.fillRoundRect(box_x, box_y, box_width, box_height, 4, background_color);

    // Get weather icon mapped to system icon
    icon_name_t weather_icon = photo_frame::weather::weather_icon_to_system_icon(weather_data.icon);

    // Draw weather icon
    const unsigned char* icon_bitmap = getBitmap(weather_icon, 48);
    if (icon_bitmap) {
        // draw the weather icon centered vertically
        display.drawInvertedBitmap(base_x, base_y + 10, icon_bitmap, 48, 48, icon_color);
    }

    // Position text to the right of icon
    int16_t text_x        = base_x + 60;
    int16_t text_y        = base_y;

    auto temperature_unit = weather_data.temperature_unit;
    temperature_unit.replace("°", "\260"); // remove degree symbol for display

    // Draw main temperature with unit (large text)
    String main_temp = String((int)weather_data.temperature) + " " + temperature_unit;

    display.setFont(&FONT_18pt8b);
    draw_string(text_x, text_y + 22, main_temp.c_str(), text_color);
    text_y += 44;

    // Draw high/low temperatures with units if available
    if (weather_data.has_daily_data && weather_data.temp_min != 0.0f &&
        weather_data.temp_max != 0.0f) {
        String temp_range = String((int)weather_data.temp_min) + "\260" + " - " +
                            String((int)weather_data.temp_max) + "\260";
        display.setFont(&FONT_9pt8b);
        draw_string(text_x, text_y, temp_range.c_str(), text_color);
        text_y += 24;
    }

    // Draw sunrise time if available
    if (weather_data.has_daily_data) {
        char sunrise_buffer[16];
        char sunset_buffer[16];

#ifdef DEBUG_RENDERER
        Serial.print(F("[renderer] drawWeatherInfo: sunrise time: "));
        Serial.println(weather_data.sunrise_time);
#endif // DEBUG_RENDERER

        // if both sunrise and sunset are available, reserve space for the icon
        if (photo_frame::datetime_utils::format_datetime(
                sunrise_buffer, sizeof(sunrise_buffer), weather_data.sunrise_time, "%H:%M") > 0 &&
            photo_frame::datetime_utils::format_datetime(
                sunset_buffer, sizeof(sunset_buffer), weather_data.sunset_time, "%H:%M") > 0) {

            display.setFont(&FONT_7pt8b);

            // first draw the sunrise icon
            const unsigned char* sunrise_icon = getBitmap(icon_name::sunrise_0deg, 16);
            const unsigned char* sunset_icon  = getBitmap(icon_name::sunset_0deg, 16);

            if (sunrise_icon && sunset_icon) {
                display.drawInvertedBitmap(text_x, text_y - 12, sunrise_icon, 16, 16, icon_color);
                text_x += 18; // leave space for the icon
            }

            draw_string(text_x, text_y, sunrise_buffer, text_color);

            if (sunset_icon) {
                // draw the sunset icon after the sunrise time
                text_x += get_string_width(sunrise_buffer) + 4; // space after sunrise time
                display.drawInvertedBitmap(text_x, text_y - 12, sunset_icon, 16, 16, icon_color);
                text_x += 18; // leave space for the icon
            }

            draw_string(text_x, text_y, sunset_buffer, text_color);
        }
    }
}

void draw_rounded_rect(int16_t x,
                       int16_t y,
                       int16_t width,
                       int16_t height,
                       uint16_t radius,
                       uint16_t color,
                       uint8_t transparency) {
    // Input validation
#ifdef DEBUG_RENDERER
    Serial.print(F("[renderer] draw_rounded_rect: "));
    Serial.print(x);
    Serial.print(", ");
    Serial.print(y);
    Serial.print(", ");
    Serial.print(width);
    Serial.print(", ");
    Serial.print(height);
    Serial.print(", ");
    Serial.print(radius);
    Serial.print(", ");
    Serial.print(color);
    Serial.print(", transparency=");
    Serial.println(transparency);
#endif // DEBUG_RENDERER

    if (width <= 0 || height <= 0)
        return;

    // Calculate corner radius (about 1/8 of the smaller dimension)
    if (radius < 2)
        radius = 2; // Minimum radius
    if (radius > 10)
        radius = 10; // Maximum radius for performance

    // Draw rounded rectangle with dithering for semi-transparency
    for (int16_t py = 0; py < height; py++) {
        // Yield every 10 rows to prevent watchdog timeout
        if (py % 10 == 0) {
            yield();
        }

        for (int16_t px = 0; px < width; px++) {
            // Check if we're in a corner region
            bool inCorner    = false;
            int16_t corner_x = 0, corner_y = 0;

            // Top-left corner
            if (px < radius && py < radius) {
                corner_x = radius - px;
                corner_y = radius - py;
                inCorner = true;
            }
            // Top-right corner
            else if (px >= width - radius && py < radius) {
                corner_x = px - (width - radius - 1);
                corner_y = radius - py;
                inCorner = true;
            }
            // Bottom-left corner
            else if (px < radius && py >= height - radius) {
                corner_x = radius - px;
                corner_y = py - (height - radius - 1);
                inCorner = true;
            }
            // Bottom-right corner
            else if (px >= width - radius && py >= height - radius) {
                corner_x = px - (width - radius - 1);
                corner_y = py - (height - radius - 1);
                inCorner = true;
            }

            // If in corner, check if we're within the rounded area
            bool drawPixel = true;
            if (inCorner) {
                // Calculate distance from corner center
                int16_t dist_squared   = corner_x * corner_x + corner_y * corner_y;
                int16_t radius_squared = radius * radius;

                // Skip pixel if outside rounded corner
                if (dist_squared > radius_squared) {
                    drawPixel = false;
                }
            }

            if (drawPixel) {
                // Create dithering pattern based on transparency level
                bool draw_pixel = false;

                if (transparency == 0) {
                    // Fully opaque - draw all pixels
                    draw_pixel = true;
                } else if (transparency == 255) {
                    // Fully transparent - draw no pixels
                    draw_pixel = false;
                } else {
                    // Semi-transparent - use dithering patterns based on transparency level

                    if (transparency <= 64) {
                        // ~75% opacity: Draw 3 out of 4 pixels (sparse transparency)
                        draw_pixel = !((px % 2 == 1) && (py % 2 == 1));
                    } else if (transparency <= 128) {
                        // ~50% opacity: Standard checkerboard pattern
                        draw_pixel = ((px + py) % 2) == 0;
                    } else if (transparency <= 192) {
                        // ~25% opacity: Draw 1 out of 4 pixels
                        draw_pixel = (px % 2 == 0) && (py % 2 == 0);
                    } else {
                        // ~12% opacity: Very sparse pattern
                        draw_pixel = (px % 4 == 0) && (py % 4 == 0);
                    }
                }

                // Draw pixel if pattern allows it
                if (draw_pixel) {
                    display.drawPixel(x + px, y + py, color);
                }
                // Background pixels are left as-is (creating transparency effect)
            }

            // Yield every 100 pixels to prevent watchdog timeout
            if (px % 100 == 0) {
                yield();
            }
        }
    }
}

void draw_battery_status(photo_frame::battery_info_t battery_info) {
#ifdef DEBUG_RENDERER
    Serial.println("[renderer] drawBatteryStatus");
#endif // DEBUG_RENDERER

    auto battery_voltage    = battery_info.millivolts;

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

    String message = String((uint16_t)battery_percentage) + "% (" +
                     String((float)battery_voltage / 1000, 2) + "V)";

#ifdef USE_SENSOR_MAX1704X
    if (battery_info.charge_rate != 0.0) {
        message += " - " + String(battery_info.charge_rate, 2) + "mA";
    }
#else

#ifdef DEBUG_BATTERY_READER
    message += "  - " + String(battery_info.raw_millivolts);
#endif // DEBUG_BATTERY

#endif // USE_SENSOR_MAX1704X

    message += " - ";
    message += String(FIRMWARE_VERSION_STRING);

    draw_side_message_with_icon(gravity::TOP_RIGHT, icon_name, message.c_str(), 0, -2);
}

void draw_last_update(const DateTime& lastUpdate, long refresh_seconds) {
#ifdef DEBUG_RENDERER
    Serial.println("[renderer] drawLastUpdate: " + lastUpdate.timestamp() + ", refresh " +
                   String(refresh_seconds) + " seconds");
#endif                             // DEBUG_RENDERER
    char dateTimeBuffer[32] = {0}; // Buffer to hold formatted date and time
    photo_frame::datetime_utils::format_datetime(dateTimeBuffer,
                                                 sizeof(dateTimeBuffer),
                                                 lastUpdate,
                                                 photo_frame::datetime_utils::dateTimeFormatShort);

    // Format the refresh time using datetime_utils
    char refreshBuffer[16] = {0}; // Buffer for refresh time string (e.g., "2h 30m")
    photo_frame::datetime_utils::format_duration(
        refreshBuffer, sizeof(refreshBuffer), refresh_seconds);

    // Build the complete last update string
    char lastUpdateBuffer[64] = {0};
    if (refreshBuffer[0] != '\0') {
        snprintf(
            lastUpdateBuffer, sizeof(lastUpdateBuffer), "%s (%s)", dateTimeBuffer, refreshBuffer);
    } else {
        snprintf(lastUpdateBuffer, sizeof(lastUpdateBuffer), "%s", dateTimeBuffer);
    }

    draw_side_message_with_icon(gravity::TOP_LEFT, icon_name::wi_time_10, lastUpdateBuffer, 0, -2);
} // end drawLastUpdate

void draw_side_message(gravity_t gravity, const char* message, int32_t x_offset, int32_t y_offset) {
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
        Serial.println("[renderer] drawSideMessage: Invalid gravity type");
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
                                 int32_t y_offset) {
    const uint16_t icon_size  = 16;
    const unsigned char* icon = getBitmap(icon_name, icon_size); // ensure the icon is loaded
    if (!icon) {
        Serial.println("[renderer] drawSideMessageWithIcon: Icon not found");
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
        Serial.println("[renderer] drawSizeMessageWithIcon: Invalid gravity type");
        return;
    }
    }

    // Draw the icon
    display.drawInvertedBitmap(
        x + x_offset, y - y + y_offset, icon, icon_size, icon_size, GxEPD_BLACK);
    // Draw the message next to the icon
    draw_string(x + icon_size + 2, y, message, GxEPD_BLACK);
}

uint16_t
draw_bitmap_from_file(File& file, const char* filename, int16_t x, int16_t y, bool with_color) {
    if (!file) {
        Serial.println("[renderer] draw_bitmap_from_file: File not open or invalid");
        return photo_frame::error_type::FileOpenFailed.code;
    }

    Serial.println("[renderer] draw_bitmap_from_file: " + String(filename));

    bool valid         = false; // valid format to be handled
    bool flip          = true;  // bitmap is stored bottom-to-top
    uint32_t startTime = millis();
    if ((x >= display.epd2.WIDTH) || (y >= display.epd2.HEIGHT))
        return photo_frame::error_type::ImageDimensionsInvalid.code;

    // Parse BMP header
    file.seek(0);
    auto signature = read16(file);

#ifdef DEBUG_RENDERER
    Serial.print("[renderer] BMP signature: ");
    Serial.println(signature, HEX);
#endif // DEBUG_RENDERER

    if (signature == 0x4D42) // BMP signature
    {
        uint32_t fileSize = read32(file);
#ifndef DEBUG_RENDERER
        (void)fileSize; // unused
#endif                  // !DEBUG_RENDERER
        uint32_t creatorBytes = read32(file);
        (void)creatorBytes;                  // unused
        uint32_t imageOffset = read32(file); // Start of image data
        uint32_t headerSize  = read32(file);
#ifndef DEBUG_RENDERER
        (void)headerSize; // unused
#endif                    // !DEBUG_RENDERER
        uint32_t width  = read32(file);
        int32_t height  = (int32_t)read32(file);
        uint16_t planes = read16(file);
        uint16_t depth  = read16(file); // bits per pixel
        uint32_t format = read32(file);
        Serial.println("[renderer] planes: " + String(planes));
        Serial.println("[renderer] format: " + String(format));
        if ((planes == 1) && ((format == 0) || (format == 3))) // uncompressed is handled, 565 also
        {
#ifdef DEBUG_RENDERER
            Serial.print("[renderer] File size: ");
            Serial.println(fileSize);
            Serial.print("[renderer] Image Offset: ");
            Serial.println(imageOffset);
            Serial.print("[renderer] Header size: ");
            Serial.println(headerSize);
            Serial.print("[renderer] Bit Depth: ");
            Serial.println(depth);
            Serial.print("[renderer] Image size: ");
            Serial.print(width);
            Serial.print('x');
            Serial.println(height);
#endif // DEBUG_RENDERER
       // BMP rows are padded (if needed) to 4-byte boundary
            uint32_t rowSize = (width * depth / 8 + 3) & ~3;
            if (depth < 8)
                rowSize = ((width * depth + 8 - depth) / 8 + 3) & ~3;
            if (height < 0) {
                height = -height;
                flip   = false;
            }
            uint16_t w = width;
            uint16_t h = height;
            if ((x + w - 1) >= display.epd2.WIDTH)
                w = display.epd2.WIDTH - x;
            if ((y + h - 1) >= display.epd2.HEIGHT)
                h = display.epd2.HEIGHT - y;
            if (w <= max_row_width) // handle with direct drawing
            {
                valid            = true;
                uint8_t bitmask  = 0xFF;
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
                    file.seek(imageOffset -
                              (4 << depth)); // 54 for regular, diff for colorsimportant
                    for (uint16_t pn = 0; pn < (1 << depth); pn++) {
                        blue  = file.read();
                        green = file.read();
                        red   = file.read();
                        file.read();
                        // Serial.print(red); Serial.print(" "); Serial.print(green); Serial.print("
                        // "); Serial.println(blue);
                        whitish = with_color ? ((red > 0x80) && (green > 0x80) && (blue > 0x80))
                                             : ((red + green + blue) > 3 * 0x80); // whitish
                        colored = (red > 0xF0) ||
                                  ((green > 0xF0) && (blue > 0xF0)); // reddish or yellowish?
                        if (0 == pn % 8)
                            mono_palette_buffer[pn / 8] = 0;
                        mono_palette_buffer[pn / 8] |= whitish << pn % 8;
                        if (0 == pn % 8)
                            color_palette_buffer[pn / 8] = 0;
                        color_palette_buffer[pn / 8] |= colored << pn % 8;

                        // Yield every 16 palette entries to prevent watchdog timeout
                        // if (pn % 16 == 0) {
                        //     yield();
                        // }
                    }
                }
                // display.clearScreen();
                Serial.println("[renderer] Starting non-paged BMP rendering");
                Serial.printf("[renderer] Processing %d rows in single pass (no paging)\n", h);
                Serial.printf("[renderer] Image dimensions: %dx%d\n", w, h);
                Serial.printf("[renderer] This will read entire file and process all rows at once\n");

                uint32_t rowPosition = flip ? imageOffset + (height - h) * rowSize : imageOffset;
                unsigned long renderStart = millis();

                for (uint16_t row = 0; row < h; row++, rowPosition += rowSize) // for each line
                {
                    // Yield every 10 rows to prevent watchdog timeout
                    if (row % 10 == 0) {
                        yield();
                    }

                    // Log progress every 50 rows
                    if (row % 50 == 0) {
                        unsigned long elapsed = millis() - renderStart;
                        Serial.printf("[renderer] Processing row %d/%d (%.1f%%) - Elapsed: %lu ms\n",
                                      row, h, (float)row * 100.0 / h, elapsed);
                    }

                    uint32_t in_remain     = rowSize;
                    uint32_t in_idx        = 0;
                    uint32_t in_bytes      = 0;
                    uint8_t in_byte        = 0;    // for depth <= 8
                    uint8_t in_bits        = 0;    // for depth <= 8
                    uint8_t out_byte       = 0xFF; // white (for w%8!=0 border)
                    uint8_t out_color_byte = 0xFF; // white (for w%8!=0 border)
                    uint32_t out_idx       = 0;
                    file.seek(rowPosition);
                    for (uint16_t col = 0; col < w; col++) // for each pixel
                    {
                        // Time to read more pixel data?
                        if (in_idx >=
                            in_bytes) // ok, exact match for 24bit also (size IS multiple of 3)
                        {
                            in_bytes =
                                file.read(input_buffer,
                                          in_remain > sizeof(input_buffer) ? sizeof(input_buffer)
                                                                           : in_remain);
                            in_remain -= in_bytes;
                            in_idx = 0;
                        }
                        switch (depth) {
                        case 32:
                            blue  = input_buffer[in_idx++];
                            green = input_buffer[in_idx++];
                            red   = input_buffer[in_idx++];
                            in_idx++; // skip alpha
                            whitish = with_color ? ((red > 0x80) && (green > 0x80) && (blue > 0x80))
                                                 : ((red + green + blue) > 3 * 0x80); // whitish
                            colored = (red > 0xF0) ||
                                      ((green > 0xF0) && (blue > 0xF0)); // reddish or yellowish?
                            break;
                        case 24:
                            blue    = input_buffer[in_idx++];
                            green   = input_buffer[in_idx++];
                            red     = input_buffer[in_idx++];
                            whitish = with_color ? ((red > 0x80) && (green > 0x80) && (blue > 0x80))
                                                 : ((red + green + blue) > 3 * 0x80); // whitish
                            colored = (red > 0xF0) ||
                                      ((green > 0xF0) && (blue > 0xF0)); // reddish or yellowish?
                            break;
                        case 16: {
                            uint8_t lsb = input_buffer[in_idx++];
                            uint8_t msb = input_buffer[in_idx++];
                            if (format == 0) // 555
                            {
                                blue  = (lsb & 0x1F) << 3;
                                green = ((msb & 0x03) << 6) | ((lsb & 0xE0) >> 2);
                                red   = (msb & 0x7C) << 1;
                            } else // 565
                            {
                                blue  = (lsb & 0x1F) << 3;
                                green = ((msb & 0x07) << 5) | ((lsb & 0xE0) >> 3);
                                red   = (msb & 0xF8);
                            }
                            whitish = with_color ? ((red > 0x80) && (green > 0x80) && (blue > 0x80))
                                                 : ((red + green + blue) > 3 * 0x80); // whitish
                            colored = (red > 0xF0) ||
                                      ((green > 0xF0) && (blue > 0xF0)); // reddish or yellowish?
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
                            whitish     = mono_palette_buffer[pn / 8] & (0x1 << pn % 8);
                            colored     = color_palette_buffer[pn / 8] & (0x1 << pn % 8);
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
                        if ((7 == col % 8) ||
                            (col == w - 1)) // write that last byte! (for w%8!=0 border)
                        {
                            output_row_color_buffer[out_idx]  = out_color_byte;
                            output_row_mono_buffer[out_idx++] = out_byte;
                            out_byte                          = 0xFF; // white (for w%8!=0 border)
                            out_color_byte                    = 0xFF; // white (for w%8!=0 border)
                        }

                        // Yield every 100 pixels to prevent watchdog timeout
                        // if (col % 100 == 0) {
                        //     yield();
                        // }
                    } // end pixel
                    uint16_t yrow = y + (flip ? h - row - 1 : row);
                    display.writeImage(
                        output_row_mono_buffer, output_row_color_buffer, x, yrow, w, 1);
                } // end line

                unsigned long totalTime = millis() - startTime;
                Serial.println("[renderer] ========================================");
                Serial.printf("[renderer] Non-paged BMP rendering completed:\n");
                Serial.printf("  - Total time: %lu ms\n", totalTime);
                Serial.printf("  - Rows processed: %d\n", h);
                Serial.printf("  - Average time per row: %.2f ms\n", (float)totalTime / h);
                Serial.printf("  - Image size: %dx%d pixels\n", w, h);
                Serial.println("[renderer] Note: This method does NOT use paging!");
                Serial.println("[renderer] All rows were sent to display buffer at once");
                Serial.println("[renderer] display.refresh() must be called separately");
                Serial.println("[renderer] ========================================");
                // display.refresh();
                // display.refresh(true); // use partial update
            }
        }
    }
    if (!valid) {
        if (signature != 0x4D42) {
            Serial.printf("[renderer] Invalid BMP signature: 0x%04X\n", signature);
            auto error = photo_frame::error_type::BitmapHeaderCorrupted;
            error.set_timestamp();
            error.log_detailed();
        } else {
            Serial.println("[renderer] bitmap format not handled.");
            auto error = photo_frame::error_type::BitmapRenderingFailed;
            error.set_timestamp();
            error.log_detailed();
        }
        return photo_frame::error_type::BitmapRenderingFailed.code;
    } else {
        Serial.println("[renderer] bitmap loaded successfully.");
        return 0; // Success
    }
} // end drawBitmapFromFile

uint16_t
draw_binary_from_file_paged(File& file, const char* filename, int width, int height, int page_index) {
    Serial.print("[renderer] draw_binary_from_file: ");
    Serial.print(filename);
    Serial.print(", width: ");
    Serial.print(width);
    Serial.print(", height: ");
    Serial.println(height);

    // Validate image dimensions using enhanced error system
    auto dimensionError = photo_frame::error_utils::validate_image_dimensions(
        width, height, display.width(), display.height(), filename);
    if (dimensionError != photo_frame::error_type::None) {
        dimensionError.log_detailed();
        return photo_frame::error_type::BinaryRenderingFailed.code;
    }

    if (!file) {
        Serial.println("[renderer] File not open or invalid");
        return photo_frame::error_type::FileOpenFailed.code;
    }

    auto startTime           = millis();
    uint32_t pixelsProcessed = 0;

    // Calculate page boundaries for efficient processing
    int startY = 0;
    int endY   = height;

    if (page_index >= 0 && display.pages() > 1) {
        int pageHeight = display.pageHeight();
        startY         = page_index * pageHeight;
        endY           = min(startY + pageHeight, height);

        Serial.print("[renderer] Processing page ");
        Serial.print(page_index);
        Serial.print(" of ");
        Serial.print(display.pages());
        Serial.print(", rows ");
        Serial.print(startY);
        Serial.print(" to ");
        Serial.print(endY - 1);
        Serial.println(" (optimized)");
    } else {
        Serial.print("[renderer] Processing entire image (");
        Serial.print(display.pages());
        Serial.print(" pages, page height: ");
        Serial.print(display.pageHeight());
        Serial.println(")");
    }

    // Only process the rows that belong to the current page (or entire image if page_index = -1)
    for (int y = startY; y < endY; y++) {
        // Yield every 10 rows to prevent watchdog timeout
        if (y % 10 == 0) {
            yield();
        }

        for (int x = 0; x < width; x++) {
            uint8_t color;
            uint16_t pixelColor;
            uint32_t idx = (y * width + x);

            int byteRead = 0;

            // Move to the pixel position in the file
            if (file.seek(idx)) {
                byteRead = file.read();
            }

            if (byteRead >= 0) {
                color = static_cast<uint8_t>(byteRead); // Read the byte as color value
                photo_frame::renderer::color2Epd(color, pixelColor, x, y);

                // Always use absolute coordinates - GxEPD2 handles page clipping automatically
                display.writePixel(x, y, pixelColor);
                pixelsProcessed++;
            } else {
                // Create specific image processing error with granular error code
                auto error = photo_frame::error_type::PixelReadError;
                error.set_timestamp();
                error.log_detailed();
                Serial.printf(
                    "[renderer] draw_binary_from_file failed: pixel read error at position %d\n",
                    idx);
                return error.code; // Exit early if read error occurs
            }

            // Yield every 100 pixels to prevent watchdog timeout
            if (pixelsProcessed % 100 == 0) {
                yield();
            }
        }
    }

    auto elapsedTime = millis() - startTime;
    Serial.print("[renderer] Page rendered in: ");
    Serial.print(elapsedTime);
    Serial.print(" ms (");
    Serial.print(pixelsProcessed);
    Serial.println(" pixels)");
    Serial.println("[renderer] Binary file page processed successfully");
    return 0; // Success
}

uint16_t draw_binary_from_file_buffered(File& file, const char* filename, int width, int height) {
    Serial.print("[renderer] draw_binary_from_file_buffered: ");
    Serial.print(filename);
    Serial.print(", width: ");
    Serial.print(width);
    Serial.print(", height: ");
    Serial.println(height);

    // Validate image dimensions using enhanced error system
    auto dimensionError = photo_frame::error_utils::validate_image_dimensions(
        width, height, display.width(), display.height(), filename);
    if (dimensionError != photo_frame::error_type::None) {
        dimensionError.log_detailed();
        return photo_frame::error_type::BinaryRenderingFailed.code;
    }

    if (!file) {
        Serial.println("[renderer] File not open or invalid");
        return photo_frame::error_type::FileOpenFailed.code;
    }

    // Validate file size using enhanced error system
    size_t expectedSize = width * height;
    auto fileSizeError  = photo_frame::error_utils::validate_image_file_size(
        file.size(), expectedSize, expectedSize, filename);
    if (fileSizeError != photo_frame::error_type::None) {
        fileSizeError.log_detailed();
        return photo_frame::error_type::BinaryRenderingFailed.code;
    }

    auto startTime = millis();

    Serial.print("[renderer] Multi-page buffered display: ");
    Serial.print(display.pages());
    Serial.print(" pages, page height: ");
    Serial.println(display.pageHeight());

    // Allocate buffer for reading a row of pixels
    uint8_t* rowBuffer = new uint8_t[width];
    if (!rowBuffer) {
        Serial.println("[renderer] Failed to allocate row buffer");
        return photo_frame::error_type::BinaryRenderingFailed.code;
    }

    // Set up display paging (like draw_bitmap_from_file_buffered does)
    display.setFullWindow();
    display.firstPage();

    int pageIndex = 0;
    uint32_t totalPixelsProcessed = 0;

    // Process each display page
    do {
        // Clear screen for this page (inside the loop like bitmap renderer)
        display.fillScreen(GxEPD_WHITE);
        Serial.printf("[renderer] Processing page %d of %d\n", pageIndex + 1, display.pages());
        uint32_t pageStartTime = millis();
        uint32_t pixelsProcessed = 0;
        uint32_t blackPixels = 0;
        uint32_t whitePixels = 0;
        uint32_t coloredPixels = 0;

        // Calculate page boundaries
        int pageHeight = display.pageHeight();
        int startY = pageIndex * pageHeight;
        int endY = min(startY + pageHeight, height);


        // Process only the rows for this page
        for (int y = startY; y < endY; y++) {
            // Seek to the start of the current row
            size_t rowPosition = y * width;
            if (!file.seek(rowPosition)) {
                delete[] rowBuffer;
                Serial.printf("[renderer] Failed to seek to row %d\n", y);
                return photo_frame::error_type::SeekOperationFailed.code;
            }

            // Read the entire row
            size_t bytesRead = file.read(rowBuffer, width);
            if (bytesRead != width) {
                delete[] rowBuffer;
                Serial.printf("[renderer] Failed to read row %d (got %d bytes, expected %d)\n",
                             y, bytesRead, width);
                return photo_frame::error_type::PixelReadError.code;
            }

            // Process each pixel in the row and draw directly (like bitmap renderer does)
            for (int x = 0; x < width; x++) {
                uint8_t color = rowBuffer[x];
                uint16_t pixelColor = GxEPD_WHITE;

                // Convert color value to EPD color
                photo_frame::renderer::color2Epd(color, pixelColor, x, y);

                // Draw the pixel directly (same as draw_bitmap_from_file_buffered does)
                display.drawPixel(x, y, pixelColor);

                // Track pixel statistics
                if (pixelColor == GxEPD_WHITE) {
                    whitePixels++;
                } else if (pixelColor == GxEPD_BLACK) {
                    blackPixels++;
                } else {
                    coloredPixels++;
                }

                pixelsProcessed++;

                // Yield every 100 pixels to prevent watchdog timeout
                if (pixelsProcessed % 100 == 0) {
                    yield();
                }
            }

            // Yield every row to prevent watchdog timeout
            yield();
        }

        totalPixelsProcessed += pixelsProcessed;
        Serial.printf("[renderer] Page %d rendered in %lu ms (%u pixels)\n",
                     pageIndex + 1, millis() - pageStartTime, pixelsProcessed);
        Serial.printf("[renderer] Pixel distribution - Black: %u, White: %u, Colored: %u\n",
                     blackPixels, whitePixels, coloredPixels);

        // Debug: Sample first few pixels of first row to see raw values
        if (pageIndex == 0) {
            file.seek(0);
            uint8_t samplePixels[10];
            file.read(samplePixels, 10);
            Serial.print("[renderer] First 10 pixel values: ");
            for (int i = 0; i < 10; i++) {
                Serial.printf("%02X ", samplePixels[i]);
            }
            Serial.println();
        }

        pageIndex++;

    } while (display.nextPage());

    // Clean up allocated buffer
    delete[] rowBuffer;

    auto elapsedTime = millis() - startTime;
    Serial.printf("[renderer] Binary file buffered rendering complete in %lu ms\n", elapsedTime);
    Serial.printf("[renderer] Total pixels processed: %u\n", totalPixelsProcessed);
    Serial.println("[renderer] Binary file processed successfully (buffered with internal paging)");
    return 0; // Success
}

uint16_t draw_bitmap_from_file_buffered(File& file,
                                        const char* filename,
                                        int16_t x,
                                        int16_t y,
                                        bool with_color,
                                        bool partial_update) {

    if (!file) {
        Serial.println("[renderer] File not open or invalid");
        return photo_frame::error_type::FileOpenFailed.code;
    }
    Serial.println("[renderer] draw_bitmap_from_file_buffered: " + String(filename) + ", x: " + String(x) + ", y: " + String(y) + ", with_color: " + String(with_color) + ", partial_update: " + String(partial_update));
    Serial.printf("[renderer] File size available: %d bytes\n", file.size());
    Serial.printf("[renderer] Current file position: %d\n", file.position());

    bool valid = false; // valid format to be handled
    bool flip  = true;  // bitmap is stored bottom-to-top
    bool has_multicolors =
        (display.epd2.panel == GxEPD2::ACeP565) || (display.epd2.panel == GxEPD2::GDEY073D46);
    uint32_t startTime = millis();
    if ((x >= display.width()) || (y >= display.height())) {
        Serial.println("[renderer] draw_bitmap_from_file_buffered: x or y out of bounds");
        return photo_frame::error_type::BinaryRenderingFailed.code;
    }
    // Parse BMP header

    uint16_t signature = read16(file);
    if (signature == 0x4D42) // BMP signature
    {
        Serial.println("[renderer] BMP signature: " + String(signature));

        uint32_t fileSize = read32(file);
#ifndef DEBUG_RENDERER
        (void)fileSize; // unused
#endif                  // !DEBUG_RENDERER
        uint32_t creatorBytes = read32(file);
        (void)creatorBytes;                  // unused
        uint32_t imageOffset = read32(file); // Start of image data
        uint32_t headerSize  = read32(file);
#ifndef DEBUG_RENDERER
        (void)headerSize; // unused
#endif                    // !DEBUG_RENDERER
        uint32_t width  = read32(file);
        int32_t height  = (int32_t)read32(file);
        uint16_t planes = read16(file);
        uint16_t depth  = read16(file); // bits per pixel
        uint32_t format = read32(file);

#ifdef DEBUG_RENDERER
        Serial.println("[renderer] planes: " + String(planes));
        Serial.println("[renderer] format: " + String(format));
#endif // DEBUG_RENDERER

        if ((planes == 1) && ((format == 0) || (format == 3))) // uncompressed is handled, 565 also
        {
#ifdef DEBUG_RENDERER
            Serial.print("[renderer] File size: ");
            Serial.println(fileSize);
            Serial.print("[renderer] Image Offset: ");
            Serial.println(imageOffset);
            Serial.print("[renderer] Header size: ");
            Serial.println(headerSize);
            Serial.print("[renderer] Bit Depth: ");
            Serial.println(depth);
            Serial.print("[renderer] Image size: ");
            Serial.print(width);
            Serial.print('x');
            Serial.println(height);
#endif // DEBUG_RENDERER
       // BMP rows are padded (if needed) to 4-byte boundary
            uint32_t rowSize = (width * depth / 8 + 3) & ~3;
            if (depth < 8)
                rowSize = ((width * depth + 8 - depth) / 8 + 3) & ~3;
            if (height < 0) {
                height = -height;
                flip   = false;
            }
            uint16_t w = width;
            uint16_t h = height;
            if ((x + w - 1) >= display.width())
                w = display.width() - x;
            if ((y + h - 1) >= display.height())
                h = display.height() - y;
            // if (w <= max_row_width) // handle with direct drawing
            {
                valid            = true;
                uint8_t bitmask  = 0xFF;
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
                    file.seek(imageOffset -
                              (4 << depth)); // 54 for regular, diff for colorsimportant
                    for (uint16_t pn = 0; pn < (1 << depth); pn++) {
                        blue  = file.read();
                        green = file.read();
                        red   = file.read();
                        file.read();
                        whitish = with_color ? ((red > 0x80) && (green > 0x80) && (blue > 0x80))
                                             : ((red + green + blue) > 3 * 0x80); // whitish
                        colored = (red > 0xF0) ||
                                  ((green > 0xF0) && (blue > 0xF0)); // reddish or yellowish?
                        if (0 == pn % 8)
                            mono_palette_buffer[pn / 8] = 0;
                        mono_palette_buffer[pn / 8] |= whitish << pn % 8;
                        if (0 == pn % 8)
                            color_palette_buffer[pn / 8] = 0;
                        color_palette_buffer[pn / 8] |= colored << pn % 8;
                        rgb_palette_buffer[pn] =
                            ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | ((blue & 0xF8) >> 3);

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
                    uint32_t rowPosition =
                        flip ? imageOffset + (height - h) * rowSize : imageOffset;
                    for (uint16_t row = 0; row < h; row++, rowPosition += rowSize) // for each line
                    {
                        // Yield every 10 rows to prevent watchdog timeout
                        if (row % 10 == 0) {
                            yield();
                        }

                        uint32_t in_remain = rowSize;
                        uint32_t in_idx    = 0;
                        uint32_t in_bytes  = 0;
                        uint8_t in_byte    = 0; // for depth <= 8
                        uint8_t in_bits    = 0; // for depth <= 8
                        uint16_t color     = GxEPD_WHITE;
                        file.seek(rowPosition);
                        for (uint16_t col = 0; col < w; col++) // for each pixel
                        {
                            // Time to read more pixel data?
                            if (in_idx >=
                                in_bytes) // ok, exact match for 24bit also (size IS multiple of 3)
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
                                blue  = input_buffer[in_idx++];
                                green = input_buffer[in_idx++];
                                red   = input_buffer[in_idx++];
                                in_idx++; // skip alpha
                                whitish = with_color
                                              ? ((red > 0x80) && (green > 0x80) && (blue > 0x80))
                                              : ((red + green + blue) > 3 * 0x80); // whitish
                                colored = (red > 0xF0) || ((green > 0xF0) &&
                                                           (blue > 0xF0)); // reddish or yellowish?
                                color   = ((red & 0xF8) << 8) | ((green & 0xFC) << 3) |
                                        ((blue & 0xF8) >> 3);
                                break;
                            case 24:
                                blue    = input_buffer[in_idx++];
                                green   = input_buffer[in_idx++];
                                red     = input_buffer[in_idx++];
                                whitish = with_color
                                              ? ((red > 0x80) && (green > 0x80) && (blue > 0x80))
                                              : ((red + green + blue) > 3 * 0x80); // whitish
                                colored = (red > 0xF0) || ((green > 0xF0) &&
                                                           (blue > 0xF0)); // reddish or yellowish?
                                color   = ((red & 0xF8) << 8) | ((green & 0xFC) << 3) |
                                        ((blue & 0xF8) >> 3);
                                break;
                            case 16: {
                                uint8_t lsb = input_buffer[in_idx++];
                                uint8_t msb = input_buffer[in_idx++];
                                if (format == 0) // 555
                                {
                                    blue  = (lsb & 0x1F) << 3;
                                    green = ((msb & 0x03) << 6) | ((lsb & 0xE0) >> 2);
                                    red   = (msb & 0x7C) << 1;
                                    color = ((red & 0xF8) << 8) | ((green & 0xFC) << 3) |
                                            ((blue & 0xF8) >> 3);
                                } else // 565
                                {
                                    blue  = (lsb & 0x1F) << 3;
                                    green = ((msb & 0x07) << 5) | ((lsb & 0xE0) >> 3);
                                    red   = (msb & 0xF8);
                                    color = (msb << 8) | lsb;
                                }
                                whitish = with_color
                                              ? ((red > 0x80) && (green > 0x80) && (blue > 0x80))
                                              : ((red + green + blue) > 3 * 0x80); // whitish
                                colored = (red > 0xF0) || ((green > 0xF0) &&
                                                           (blue > 0xF0)); // reddish or yellowish?
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
                                whitish     = mono_palette_buffer[pn / 8] & (0x1 << pn % 8);
                                colored     = color_palette_buffer[pn / 8] & (0x1 << pn % 8);
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
                    Serial.print("[renderer] page loaded in ");
                    Serial.print(millis() - startTime);
                    Serial.println(" ms");
                } while (display.nextPage());
                Serial.print("[renderer] loaded in ");
                Serial.print(millis() - startTime);
                Serial.println(" ms");
            }
        }
    }
    if (!valid) {
        if (signature != 0x4D42) {
            Serial.printf("[renderer] Invalid BMP signature: 0x%04X\n", signature);
            auto error = photo_frame::error_type::BitmapHeaderCorrupted;
            error.set_timestamp();
            error.log_detailed();
        } else {
            Serial.println("[renderer] bitmap format not handled (signature: " + String(signature) +
                           ").");
            auto error = photo_frame::error_type::BufferedBitmapRenderingFailed;
            error.set_timestamp();
            error.log_detailed();
        }
        return photo_frame::error_type::BinaryRenderingFailed.code;
    } else {
        Serial.println("[renderer] bitmap loaded successfully.");
        return 0; // Success
    }
} // end drawBitmapFromFile_Buffered

#if defined(DISP_6C) || defined(DISP_7C)

bool is_dominant(uint8_t r, uint8_t g, uint8_t b) {
    return (r >= g && r >= b) || (g >= r && g >= b) || (b >= r && b >= g);
};

bool is_light_grey(uint8_t r, uint8_t g, uint8_t b) { return (r >= 5 && g >= 5 && b >= 2); };

bool is_dark_grey(uint8_t r, uint8_t g, uint8_t b) { return (r < 3 && g < 3 && b < 2); };

bool is_yellow(uint8_t r, uint8_t g, uint8_t b) { return (r > 3 && g > 3 && b < 2); };

bool is_orange(uint8_t r, uint8_t g, uint8_t b) { return (r > 4 && g > 2 && g < r && b < 2); };

#endif // DISP_6C || DISP_7C


void color2Epd(uint8_t color, uint16_t& pixelColor, int x, int y) {
#ifdef DISP_6C
    // Binary files use RGB compression: ((r / 32) << 5) + ((g / 32) << 2) + (b / 64)
    // Pre-computed values for 6-color display palette:
    switch (color) {
    case 0xFF: // White (255,255,255) → 255
        pixelColor = GxEPD_WHITE;
        break;
    case 0x00: // Black (0,0,0) → 0
        pixelColor = GxEPD_BLACK;
        break;
    case 0xE0: // Red (255,0,0) → 224
        pixelColor = GxEPD_RED;
        break;
    case 0x1C: // Green (0,255,0) → 28
        pixelColor = GxEPD_GREEN;
        break;
    case 0x03: // Blue (0,0,255) → 3
        pixelColor = GxEPD_BLUE;
        break;
    case 0xFC: // Yellow (255,255,0) → 252
        pixelColor = GxEPD_YELLOW;
        break;
    default:
        // Handle intermediate values by extracting RGB components
        uint8_t r = (color >> 5) & 0x07; // from 0 to 7
        uint8_t g = (color >> 2) & 0x07; // from 0 to 7
        uint8_t b = color & 0x03;        // from 0 to 3

        // Map to closest 6-color display color
        if (is_dominant(r, g, b)) {
            if (r > g && r > b) {
                pixelColor = GxEPD_RED;
            } else if (g > r && g > b) {
                pixelColor = GxEPD_GREEN;
            } else if (b > r && b > g) {
                pixelColor = GxEPD_BLUE;
            } else {
                pixelColor = GxEPD_WHITE;
            }
        } else {
            if (is_yellow(r, g, b)) {
                pixelColor = GxEPD_YELLOW;
            } else if (is_light_grey(r, g, b)) {
                pixelColor = GxEPD_WHITE;
            } else if (is_dark_grey(r, g, b)) {
                pixelColor = GxEPD_BLACK;
            } else {
                pixelColor = GxEPD_WHITE; // Default fallback
            }
        }
        break;
    }

#elif defined(DISP_7C)
    // Binary files use RGB compression: ((r / 32) << 5) + ((g / 32) << 2) + (b / 64)
    // Pre-computed values for 7-color display palette:
    switch (color) {
    case 0xFF: // White (255,255,255) → 255
        pixelColor = GxEPD_WHITE;
        break;
    case 0x00: // Black (0,0,0) → 0
        pixelColor = GxEPD_BLACK;
        break;
    case 0xE0: // Red (255,0,0) → 224
        pixelColor = GxEPD_RED;
        break;
    case 0x1C: // Green (0,255,0) → 28
        pixelColor = GxEPD_GREEN;
        break;
    case 0x03: // Blue (0,0,255) → 3
        pixelColor = GxEPD_BLUE;
        break;
    case 0xFC: // Yellow (255,255,0) → 252
        pixelColor = GxEPD_YELLOW;
        break;
    case 0xF4: // Orange (255,165,0) → 244
        pixelColor = GxEPD_ORANGE;
        break;
    default:
        // Handle intermediate values by extracting RGB components
        uint8_t r = (color >> 5) & 0x07; // from 0 to 7
        uint8_t g = (color >> 2) & 0x07; // from 0 to 7
        uint8_t b = color & 0x03;        // from 0 to 3

        // Map to closest 7-color display color
        if (is_dominant(r, g, b)) {
            if (r > g && r > b) {
                pixelColor = GxEPD_RED;
            } else if (g > r && g > b) {
                pixelColor = GxEPD_GREEN;
            } else if (b > r && b > g) {
                pixelColor = GxEPD_BLUE;
            } else {
                pixelColor = GxEPD_WHITE;
            }
        } else {
            if (is_orange(r, g, b)) {
                pixelColor = GxEPD_ORANGE;
            } else if (is_yellow(r, g, b)) {
                pixelColor = GxEPD_YELLOW;
            } else if (is_light_grey(r, g, b)) {
                pixelColor = GxEPD_WHITE;
            } else if (is_dark_grey(r, g, b)) {
                pixelColor = GxEPD_BLACK;
            } else {
                pixelColor = GxEPD_WHITE; // Default fallback
            }
        }
        break;
    }

#elif defined(DISP_BW_V2)
    if (color > 127) {
        pixelColor = GxEPD_WHITE; // 255
    } else {
        pixelColor = GxEPD_BLACK; // 0
    }
#else
    // Default handling for any other BW display
    // Binary files for BW displays: 0-127 = black, 128-255 = white
    if (color > 127) {
        pixelColor = GxEPD_WHITE;
    } else {
        pixelColor = GxEPD_BLACK;
    }
#endif
}


} // namespace renderer

} // namespace photo_frame