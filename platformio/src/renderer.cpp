#include "renderer.h"
#include "GUI_Paint.h"
#include "io_util.h"
#include <EPD.h>
#include <SDCard.h>

#include <icons/icons.h>
#include FONT_HEADER

namespace photo_frame {
static const uint16_t max_row_width       = DISPLAY_WIDTH;
static const uint16_t max_palette_pixels  = 256; // for depth <= 8

static const uint16_t input_buffer_pixels = 800;     // may affect performance
uint8_t input_buffer[3 * input_buffer_pixels];       // up to depth 24
uint8_t output_row_mono_buffer[max_row_width / 8];   // buffer for at least one row of b/w bits
uint8_t mono_palette_buffer[max_palette_pixels / 8]; // palette buffer for depth <= 8 b/w
uint16_t rgb_palette_buffer[max_palette_pixels];     // palette buffer for depth <= 8 for buffered
                                                     // graphics, needed for 7-color display

Renderer::Renderer() { BlackImage = nullptr; }

Renderer::~Renderer() {
    if (BlackImage) {
        free(BlackImage);
        BlackImage = nullptr;
    }
}

bool Renderer::init() {
    // Create a new image cache
    UWORD Imagesize =
        ((DISPLAY_WIDTH % 8 == 0) ? (DISPLAY_WIDTH / 8) : (DISPLAY_WIDTH / 8 + 1)) * DISPLAY_HEIGHT;
    if ((BlackImage = (UBYTE*)malloc(Imagesize)) == NULL) {
        Serial.println(F("Failed to apply for black memory..."));
        return false;
    }

    DEV_Module_Init();
    EPD_7IN5_V2_Init();
    EPD_7IN5_V2_Clear();
    DEV_Delay_ms(500);
    Paint_NewImage(BlackImage, DISPLAY_WIDTH, DISPLAY_HEIGHT, 0, WHITE);
    return true;
}

void Renderer::clear() {
    Paint_Clear(WHITE);
    DEV_Delay_ms(500);
}

renderer_error_code_t
Renderer::drawBitmapFromSD(SDCard& sdCard, const char* filename, UWORD x, UWORD y) {
    bool valid         = false; // valid format to be handled
    bool flip          = true;  // bitmap is stored bottom-to-top
    uint32_t startTime = millis();
    if ((x >= DISPLAY_WIDTH) || (y >= DISPLAY_HEIGHT)) {
        Serial.println("Invalid coordinates");
        return renderer_error_code::CoordinatesOutOfBounds;
    }

    Serial.println();
    Serial.print("Loading image '");
    Serial.print(filename);
    Serial.println('\'');
    fs::File file = sdCard.open(filename, "r");
    if (!file) {
        Serial.print("File not found");
        return renderer_error_code::FileReadError;
    }

    renderer_error_code result = drawBitmapFromFile(file, x, y);
    file.close();
    return result;
}

renderer_error_code_t Renderer::drawBitmapFromFile(fs::File file, UWORD x, UWORD y) {
    bool valid         = false; // valid format to be handled
    bool flip          = true;  // bitmap is stored bottom-to-top
    uint32_t startTime = millis();
    if ((x >= DISPLAY_WIDTH) || (y >= DISPLAY_HEIGHT)) {
        Serial.println("Invalid coordinates");
        return renderer_error_code::CoordinatesOutOfBounds;
    }

    // Parse BMP header
    if (bmpRead16(file) == 0x4D42) // BMP signature
    {
        uint32_t fileSize     = bmpRead32(file);
        uint32_t creatorBytes = bmpRead32(file);
        (void)creatorBytes;                     // unused
        uint32_t imageOffset = bmpRead32(file); // Start of image data
        uint32_t headerSize  = bmpRead32(file);
        uint32_t width       = bmpRead32(file);
        int32_t height       = (int32_t)bmpRead32(file);
        uint16_t planes      = bmpRead16(file);
        uint16_t depth       = bmpRead16(file); // bits per pixel
        uint32_t format      = bmpRead32(file);
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
        return renderer_error_code::InvalidFileFormat;
    }
    return renderer_error_code::OperatoionComplete;
}

void Renderer::drawCenteredImage(const unsigned char* image_buffer, UWORD W_Image, UWORD H_Image) {
    UWORD x = (DISPLAY_WIDTH - W_Image) / 2;
    UWORD y = (DISPLAY_HEIGHT - H_Image) / 2;

    // Draw the image centered
    Paint_DrawImage(image_buffer, x, y, W_Image, H_Image);
}

void Renderer::drawCenteredWarningMessage(const char* message) {
    const unsigned char* image_buffer = warning_icon_196x196;
    UWORD W_Image                     = 196;
    UWORD H_Image                     = 196;
    UWORD x                           = (DISPLAY_WIDTH - W_Image) / 2;
    UWORD y                           = (DISPLAY_HEIGHT - H_Image) / 2;

    // Draw the image centered
    drawCenteredImage(image_buffer, W_Image, H_Image);

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

void Renderer::drawCentererMessage(const char* message,
                                   font_size_t fontSize,
                                   uint16_t fgColor,
                                   uint16_t bgColor) {

    sFONT font = FONT_24pt;
    switch (fontSize) {
    case FontSize10pt: font = FONT_10pt; break;
    case FontSize14pt: font = FONT_14pt; break;
    }

    uint16_t message_width = font.Width * strlen(message);

    // Draw the message centered
    Paint_DrawString_EN((DISPLAY_WIDTH - message_width) / 2,
                        (DISPLAY_HEIGHT - font.Height) / 2,
                        message,
                        &font,
                        fgColor,
                        bgColor);
}

void Renderer::drawBatteryStatus(const Battery& battery) {
    // Draw battery icon
    const unsigned char* battery_icon = nullptr;
    uint32_t percent                  = battery.percent;
    uint32_t voltage                  = battery.voltage;

    if (!battery) {
        battery_icon = battery_charging_full_90deg_24x24;
        percent      = 100;
        voltage      = MAX_BATTERY_VOLTAGE;
    } else {
        if (battery.percent >= 100) {
            battery_icon = battery_full_90deg_24x24;
        } else if (battery.percent >= 90) {
            battery_icon = battery_6_bar_90deg_24x24;
        } else if (battery.percent >= 80) {
            battery_icon = battery_5_bar_90deg_24x24;
        } else if (battery.percent >= 70) {
            battery_icon = battery_4_bar_90deg_24x24;
        } else if (battery.percent >= 60) {
            battery_icon = battery_3_bar_90deg_24x24;
        } else if (battery.percent >= 50) {
            battery_icon = battery_2_bar_90deg_24x24;
        } else if (battery.percent >= 40) {
            battery_icon = battery_1_bar_90deg_24x24;
        } else {
            battery_icon = battery_alert_90deg_24x24;
        }
    }

    char buffer[21] = {0};
    snprintf(buffer, sizeof(buffer), "%d%% (%.2fmV)", percent, (double)voltage / 1000.0);
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

void Renderer::display() {
    EPD_7IN5_V2_Display(BlackImage);
    DEV_Delay_ms(500);
}

void Renderer::sleep() { EPD_7IN5_V2_Sleep(); }

}; // namespace photo_frame