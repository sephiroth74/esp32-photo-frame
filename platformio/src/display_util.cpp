#include "display_util.h"
#include "config.h"
#include <Arduino.h>
#include <EPD.h>
#include <FS.h>
#include <GUI_Paint.h>
#include <SD.h>
#include <SPI.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <fonts.h>
#include <icons/icons.h>

#include FONT_HEADER

#if DISPLAY_GREY_SCALE
const uint8_t row_divisor = 4; // 4 gray level
#else
const uint8_t row_divisor = 8; // 8 gray level
#endif

namespace photo_frame {
static const uint16_t max_row_width       = DISPLAY_WIDTH;
static const uint16_t max_palette_pixels  = 256; // for depth <= 8

static const uint16_t input_buffer_pixels = 800;     // may affect performance
uint8_t input_buffer[3 * input_buffer_pixels];       // up to depth 24
uint8_t output_row_mono_buffer[max_row_width / 8];   // buffer for at least one row of b/w bits
uint8_t mono_palette_buffer[max_palette_pixels / 8]; // palette buffer for depth <= 8 b/w
uint16_t rgb_palette_buffer[max_palette_pixels];     // palette buffer for depth <= 8 for buffered
                                                     // graphics, needed for 7-color display

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

bitmap_error_code_t draw_bitmap_from_sd(const char* filename, UWORD x, UWORD y) {
    Serial.println("drawBitmapFromSD()");

    bool valid         = false; // valid format to be handled
    bool flip          = true;  // bitmap is stored bottom-to-top
    uint32_t startTime = millis();
    if ((x >= DISPLAY_WIDTH) || (y >= DISPLAY_HEIGHT)) {
        Serial.println("Invalid coordinates");
        return bitmap_error_code::CoordinatesOutOfBounds;
    }

    Serial.println();
    Serial.print("Loading image '");
    Serial.print(filename);
    Serial.println('\'');
    fs::File file = SD.open(filename, "r");
    if (!file) {
        Serial.print("File not found");
        return bitmap_error_code::FileReadError;
    }

    bitmap_error_code_t result = draw_bitmap_from_file(file, x, y);
    file.close();
    return result;
}

bitmap_error_code_t draw_bitmap_from_file(fs::File file, UWORD x, UWORD y) {
    Serial.println("drawBitmapFromFile()");

    bool valid         = false; // valid format to be handled
    bool flip          = true;  // bitmap is stored bottom-to-top
    uint32_t startTime = millis();
    if ((x >= DISPLAY_WIDTH) || (y >= DISPLAY_HEIGHT)) {
        Serial.println("Invalid coordinates");
        return bitmap_error_code::CoordinatesOutOfBounds;
    }

    // Parse BMP header
    if (read16(file) == 0x4D42) // BMP signature
    {
        uint32_t fileSize     = read32(file);
        uint32_t creatorBytes = read32(file);
        (void)creatorBytes;                  // unused
        uint32_t imageOffset = read32(file); // Start of image data
        uint32_t headerSize  = read32(file);
        uint32_t width       = read32(file);
        int32_t height       = (int32_t)read32(file);
        uint16_t planes      = read16(file);
        uint16_t depth       = read16(file); // bits per pixel
        uint32_t format      = read32(file);
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
                flip   = false;
            }
            uint16_t w = width;
            uint16_t h = height;
            if ((x + w - 1) >= DISPLAY_WIDTH)
                w = DISPLAY_WIDTH - x;
            if ((y + h - 1) >= DISPLAY_HEIGHT)
                h = DISPLAY_HEIGHT - y;
            if (w <= max_row_width) // handle with direct drawing
            {
                valid            = true;
                uint8_t bitmask  = 0xFF;
                uint8_t bitshift = 8 - depth;
                uint16_t red, green, blue;
                bool whitish = false;
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
                        whitish = ((red + green + blue) > 3 * 0x80); // whitish
                        if (0 == pn % 8)
                            mono_palette_buffer[pn / 8] = 0;
                        mono_palette_buffer[pn / 8] |= whitish << pn % 8;
                    }
                }

                // clear screen
                // Paint_Clear(WHITE);

                uint32_t rowPosition = flip ? imageOffset + (height - h) * rowSize : imageOffset;
                for (uint16_t row = 0; row < h; row++, rowPosition += rowSize) // for each line
                {
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
                            in_idx++;                                    // skip alpha
                            whitish = ((red + green + blue) > 3 * 0x80); // whitish
                            break;
                        case 24:
                            blue    = input_buffer[in_idx++];
                            green   = input_buffer[in_idx++];
                            red     = input_buffer[in_idx++];
                            whitish = ((red + green + blue) > 3 * 0x80); // whitish
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
                            whitish = ((red + green + blue) > 3 * 0x80); // whitish
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
                            in_byte <<= depth;
                            in_bits -= depth;
                        } break;
                        }
                        if (whitish) {
                            // keep white
                        } else {
                            out_byte &= ~(0x80 >> col % 8); // black
                        }
                        if ((7 == col % 8) ||
                            (col == w - 1)) // write that last byte! (for w%8!=0 border)
                        {
                            output_row_mono_buffer[out_idx++] = out_byte;
                            out_byte                          = 0xFF; // white (for w%8!=0 border)
                            out_color_byte                    = 0xFF; // white (for w%8!=0 border)
                        }
                    } // end pixel
                    uint16_t yrow = y + (flip ? h - row - 1 : row);
                    Paint_DrawImage(output_row_mono_buffer, x, yrow, w, 1);
                } // end line
                Serial.print("loaded in ");
                Serial.print(millis() - startTime);
                Serial.println(" ms");
            }
        }
    }
    if (!valid) {
        Serial.println("bitmap format not handled.");
        return bitmap_error_code::InvalidFileFormat;
    }
    return bitmap_error_code::OperatoionComplete;
}

bitmap_error_code_t draw_bitmap_from_file_4gray(fs::File file, UWORD x, UWORD y) {
    Serial.println("drawBitmapFromFile()");

    bool valid         = false; // valid format to be handled
    bool flip          = true;  // bitmap is stored bottom-to-top
    uint32_t startTime = millis();
    if ((x >= DISPLAY_WIDTH) || (y >= DISPLAY_HEIGHT)) {
        Serial.println("Invalid coordinates");
        return bitmap_error_code::CoordinatesOutOfBounds;
    }

    // Parse BMP header
    if (read16(file) == 0x4D42) // BMP signature
    {
        uint32_t fileSize     = read32(file);
        uint32_t creatorBytes = read32(file);
        (void)creatorBytes;                  // unused
        uint32_t imageOffset = read32(file); // Start of image data
        uint32_t headerSize  = read32(file);
        uint32_t width       = read32(file);
        int32_t height       = (int32_t)read32(file);
        uint16_t planes      = read16(file);
        uint16_t depth       = read16(file); // bits per pixel
        uint32_t format      = read32(file);
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
                flip   = false;
            }
            uint16_t w = width;
            uint16_t h = height;
            if ((x + w - 1) >= DISPLAY_WIDTH)
                w = DISPLAY_WIDTH - x;
            if ((y + h - 1) >= DISPLAY_HEIGHT)
                h = DISPLAY_HEIGHT - y;
            if (w <= max_row_width) // handle with direct drawing
            {
                valid            = true;
                uint8_t bitmask  = 0xFF;
                uint8_t bitshift = 8 - depth;
                uint16_t red, green, blue;
                bool whitish = false;
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
                        whitish = ((red + green + blue) > 3 * 0x80); // whitish
                        if (0 == pn % 8)
                            mono_palette_buffer[pn / 8] = 0;
                        mono_palette_buffer[pn / 8] |= whitish << pn % 8;
                    }
                }

                // clear screen
                // Paint_Clear(WHITE);

                uint32_t rowPosition = flip ? imageOffset + (height - h) * rowSize : imageOffset;
                for (uint16_t row = 0; row < h; row++, rowPosition += rowSize) // for each line
                {
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
                            in_idx++;                                    // skip alpha
                            whitish = ((red + green + blue) > 3 * 0x80); // whitish
                            break;
                        case 24:
                            blue    = input_buffer[in_idx++];
                            green   = input_buffer[in_idx++];
                            red     = input_buffer[in_idx++];
                            whitish = ((red + green + blue) > 3 * 0x80); // whitish
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
                            whitish = ((red + green + blue) > 3 * 0x80); // whitish
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
                            in_byte <<= depth;
                            in_bits -= depth;
                        } break;
                        }
                        if (whitish) {
                            // keep white
                        } else {
                            out_byte &= ~(0x80 >> col % 8); // black
                        }
                        if ((7 == col % 8) ||
                            (col == w - 1)) // write that last byte! (for w%8!=0 border)
                        {
                            output_row_mono_buffer[out_idx++] = out_byte;
                            out_byte                          = 0xFF; // white (for w%8!=0 border)
                            out_color_byte                    = 0xFF; // white (for w%8!=0 border)
                        }
                    } // end pixel
                    uint16_t yrow = y + (flip ? h - row - 1 : row);
                    Paint_DrawImage(output_row_mono_buffer, x, yrow, w, 1);
                } // end line
                Serial.print("loaded in ");
                Serial.print(millis() - startTime);
                Serial.println(" ms");
            }
        }
    }
    if (!valid) {
        Serial.println("bitmap format not handled.");
        return bitmap_error_code::InvalidFileFormat;
    }
    return bitmap_error_code::OperatoionComplete;
}

void draw_centered_image(const unsigned char* image_buffer, UWORD W_Image, UWORD H_Image) {
    UWORD x = (DISPLAY_WIDTH - W_Image) / 2;
    UWORD y = (DISPLAY_HEIGHT - H_Image) / 2;
    Paint_DrawImage(image_buffer, x, y, W_Image, H_Image);
}

void draw_battery_status(uint32_t voltage, uint32_t percent) {
    Serial.print("Battery voltage: ");
    Serial.print(voltage);
    Serial.print(" mV, percent: ");
    Serial.print(percent);
    Serial.println(" %");

    // Draw battery icon
    const unsigned char* battery_icon = nullptr;

    if (voltage < NO_BATTERY) {
        battery_icon = battery_charging_full_90deg_24x24;
        percent      = 100;
        voltage      = MAX_BATTERY_VOLTAGE;
    } else {

        if (percent >= 100) {
            battery_icon = battery_full_90deg_24x24;
        } else if (percent >= 90) {
            battery_icon = battery_6_bar_90deg_24x24;
        } else if (percent >= 80) {
            battery_icon = battery_5_bar_90deg_24x24;
        } else if (percent >= 70) {
            battery_icon = battery_4_bar_90deg_24x24;
        } else if (percent >= 60) {
            battery_icon = battery_3_bar_90deg_24x24;
        } else if (percent >= 50) {
            battery_icon = battery_2_bar_90deg_24x24;
        } else if (percent >= 40) {
            battery_icon = battery_1_bar_90deg_24x24;
        } else {
            battery_icon = battery_alert_90deg_24x24;
        }
    }

    char buffer[20] = {0};
    snprintf(buffer, sizeof(buffer), "%d%% (%.1fmV)", percent, (double)voltage / 1000.0);
    uint16_t string_width = FONT_14pt.Width * strlen(buffer);

    // draw battery icon followed by the text at the bottom right corner of the display
    // first draw a black rectangle to cover the image and text
    Paint_DrawRectangle(DISPLAY_WIDTH - 26 - string_width,
                        DISPLAY_HEIGHT - 24,
                        DISPLAY_WIDTH,
                        DISPLAY_HEIGHT,
                        WHITE,
                        DOT_PIXEL_1X1,
                        DRAW_FILL_FULL);

    Paint_DrawImage(battery_icon, DISPLAY_WIDTH - 26 - string_width, DISPLAY_HEIGHT - 25, 24, 24);
    Paint_DrawString_EN(DISPLAY_WIDTH - 26 - string_width + 24,
                        DISPLAY_HEIGHT - 20,
                        buffer,
                        &FONT_14pt,
                        WHITE,
                        BLACK);
}

void draw_centered_warning_message(const unsigned char* image_buffer,
                                   const char* message,
                                   UWORD W_Image,
                                   UWORD H_Image) {
    UWORD x = (DISPLAY_WIDTH - W_Image) / 2;
    UWORD y = (DISPLAY_HEIGHT - H_Image) / 2;

    // Draw the image centered
    Paint_DrawImage(image_buffer, x, y, W_Image, H_Image);

    sFONT font             = FONT_24pt;
    uint16_t message_width = font.Width * strlen(message);

    if (message_width > DISPLAY_WIDTH) {
        font          = FONT_14pt;
        message_width = font.Width * strlen(message);
        if (message_width > DISPLAY_WIDTH) {
            font          = FONT_10pt;
            message_width = font.Width * strlen(message);
        }
    }

    // Draw the message centered below the image
    Paint_DrawString_EN(
        x + (W_Image - message_width) / 2, y + H_Image + 10, message, &font, WHITE, BLACK);
}

}; // namespace photo_frame