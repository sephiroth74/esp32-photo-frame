#include "canvas_renderer.h"
#include "config.h"
#include "datetime_utils.h"
#include <Fonts/FreeMonoBold9pt7b.h>
#include <assets/fonts/Ubuntu_R.h>
#include <assets/icons/icons.h>

namespace photo_frame {
// Canvas rendering functions

// Helper function to draw inverted bitmap (for icons)
static void drawInvertedBitmap(GFXcanvas8& canvas,
                               int16_t x,
                               int16_t y,
                               const uint8_t bitmap[],
                               int16_t w,
                               int16_t h,
                               uint16_t color) {
    int16_t byteWidth = (w + 7) / 8;
    uint8_t byte      = 0;

    for (int16_t j = 0; j < h; j++) {
        for (int16_t i = 0; i < w; i++) {
            if (i & 7) {
                byte <<= 1;
            } else {
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

void drawOverlay(GFXcanvas8& canvas) {
    // Determine portrait mode from canvas rotation
    // Rotation 1 or 3 = portrait, 0 or 2 = landscape
    bool portrait_mode = (canvas.getRotation() == 1 || canvas.getRotation() == 3);

    log_i("drawOverlay(rotation=%d)", canvas.getRotation());
    log_v("canvas size: %dx%d", canvas.width(), canvas.height());

    // Draw a simple status bar
    if (portrait_mode) {
        // Portrait: status bar on the right side
        canvas.fillRect(0, 0, canvas.width(), 16, DISPLAY_COLOR_WHITE);
        // canvas.fillRect(canvas.width() - 16, 0, 16, canvas.height(), DISPLAY_COLOR_WHITE);
    } else {
        // Landscape: status bar on top
        canvas.fillRect(0, 0, canvas.width(), 16, DISPLAY_COLOR_WHITE);
    }
}

void drawLastUpdate(GFXcanvas8& canvas, const DateTime& lastUpdate, long refresh_seconds) {
    // Determine portrait mode from canvas rotation
    bool portrait_mode      = (canvas.getRotation() == 1 || canvas.getRotation() == 3);

    char dateTimeBuffer[32] = {0};

    // Format the date and time using datetime_utils
    photo_frame::datetime_utils::format_datetime(
        dateTimeBuffer,
        sizeof(dateTimeBuffer),
        lastUpdate,
        portrait_mode ? photo_frame::datetime_utils::dateTimeFormatShort
                      : photo_frame::datetime_utils::dateTimeFormatFull);

    // Format the refresh time using datetime_utils
    char refreshBuffer[16] = {0};
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

    // In landscape mode, show icon; in portrait mode, just text
    if (!portrait_mode) {
        drawSideMessageWithIcon(
            canvas, gravity::TOP_LEFT, icon_name::wi_time_10, lastUpdateBuffer, 0, -2);
    } else {
        drawSideMessage(canvas, gravity::TOP_LEFT, lastUpdateBuffer, 0, 0);
    }
}

void drawBatteryStatus(GFXcanvas8& canvas, battery_info_t battery_info) {
    auto battery_voltage    = battery_info.millivolts;
    auto battery_percentage = battery_info.percent;

    // Determine battery icon based on charge level
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
            icon_name = icon_name::battery_alert_90deg;
        }
    }

    // Build message string
    String message = String((uint16_t)battery_percentage) + "% (" +
                     String((float)battery_voltage / 1000, 2) + "V)";

#if defined(USE_SENSOR_MAX1704X) && defined(DEBUG_BATTERY_READER)
    if (battery_info.charge_rate != 0.0) {
        message += " - " + String(battery_info.charge_rate, 2) + "mA";
    }
#else
#ifdef DEBUG_BATTERY_READER
    message += "  - " + String(battery_info.raw_millivolts);
#endif
#endif

    drawSideMessageWithIcon(canvas, TOP_RIGHT, icon_name, message.c_str(), 0, 0);
}

void drawImageInfo(GFXcanvas8& canvas,
                   uint32_t index,
                   uint32_t total_images,
                   image_source_t image_source) {
    String message = String(index + 1) + " / " + String(total_images);
    drawSideMessageWithIcon(canvas,
                            gravity::TOP_CENTER,
                            image_source == photo_frame::image_source_t::IMAGE_SOURCE_CLOUD
                                ? icon_name::cloud_0deg
                                : icon_name::micro_sd_card_0deg,
                            message.c_str(),
                            -4,
                            0);
}

void drawError(GFXcanvas8& canvas, photo_frame_error_t error, const char* filename) {
    log_d("drawError: Starting with error code %d, filename: %s",
          error.code,
          filename ? filename : "N/A");

    // Clear canvas
    canvas.fillScreen(DISPLAY_COLOR_WHITE);

    if (error.code == 0) {
        log_i("drawError: No error to display");
        return;
    }

    log_d("drawError: Canvas cleared, proceeding with drawing");

    // Select appropriate 196x196 icon based on error type
    const unsigned char* bitmap_196x196 = nullptr;

    // Compare error codes directly
    if (error.code == 4) { // NoSdCardAttached error code
        bitmap_196x196 = getBitmap(icon_name::micro_sd_card_0deg, 196);
    } else if (error.code == 10) { // BatteryLevelCritical error code
        bitmap_196x196 = getBitmap(icon_name::battery_alert_0deg, 196);
    } else {
        bitmap_196x196 = getBitmap(icon_name::error_icon, 196);
    }

    // Get display dimensions
    int16_t disp_w = canvas.width();
    int16_t disp_h = canvas.height();

    // Check if we're in portrait mode
    bool is_portrait = (canvas.getRotation() == 1 || canvas.getRotation() == 3);

    // Calculate vertical offset for portrait mode
    int16_t vertical_offset = is_portrait ? -60 : 0;

    // Draw large central icon if we have one
    if (bitmap_196x196) {
        // Determine accent color based on display type
        uint8_t accent_color = DISPLAY_COLOR_BLACK; // Default for B&W displays
#ifdef DISP_6C
        accent_color = DISPLAY_COLOR_RED; // Red for 6-color displays
#endif

        // Icon position with portrait mode adjustment
        int16_t icon_x = disp_w / 2 - 196 / 2;
        int16_t icon_y = disp_h / 2 - 196 / 2 - 42 + vertical_offset;

        drawInvertedBitmap(canvas, icon_x, icon_y, bitmap_196x196, 196, 196, accent_color);
    }

    // Draw error message text below the icon
    // Use larger font for error message
    canvas.setFont(&FONT_26pt8b);
    canvas.setTextColor(DISPLAY_COLOR_BLACK);

    // Position text below icon with portrait mode adjustment
    int16_t baseY = disp_h / 2 + 196 / 2 - 12 + vertical_offset;

    // Get error message
    String errorMsg = error.format_for_display();
    log_d("drawError: Error message: %s", errorMsg.c_str());

    // Adjust padding and max lines based on orientation
    int multiline_error_h_padding;
    int multiline_error_maxlines;

    if (is_portrait) {
        // Portrait mode: more vertical space, less horizontal
        multiline_error_h_padding = 80; // Total horizontal padding (40 per side)
        multiline_error_maxlines  = 5;  // More lines available vertically in portrait
    } else {
        // Landscape mode: more horizontal space, less vertical
        multiline_error_h_padding = 120; // Total horizontal padding (60 per side)
        multiline_error_maxlines  = 3;   // Fewer lines in landscape
    }

    // Calculate actual text width available
    int16_t text_max_width = disp_w - multiline_error_h_padding;

    log_d("drawError: About to draw multiline string - max_width: %d, total_padding: %d, "
          "max_lines: %d",
          text_max_width,
          multiline_error_h_padding,
          multiline_error_maxlines);

    // Draw multi-line error message
    drawMultilineString(canvas,
                        disp_w / 2,
                        baseY,
                        errorMsg,
                        CENTER,
                        text_max_width, // Use calculated max width
                        multiline_error_maxlines,
                        55,
                        DISPLAY_COLOR_BLACK);

    log_d("drawError: Multiline string drawn successfully");

    // Draw error code in top-right corner
    String errorCode = "Error " + String(error.code) + " - " + String(FIRMWARE_VERSION_STRING);
    canvas.setFont(&FONT_6pt8b);
    canvas.setTextColor(DISPLAY_COLOR_BLACK);

    int16_t text_width = getStringWidth(canvas, errorCode);
    drawString(canvas, disp_w - text_width - 10, 10, errorCode.c_str(), LEFT, DISPLAY_COLOR_BLACK);

    // Draw filename at bottom center if provided
    if (filename && strlen(filename) > 0) {
        // Extract just the filename from the path (if it's a full path)
        const char* lastSlash   = strrchr(filename, '/');
        const char* displayName = lastSlash ? (lastSlash + 1) : filename;

        // Use small font for filename
        canvas.setFont(&FONT_6pt8b);
        canvas.setTextColor(DISPLAY_COLOR_BLACK);

        // Position at bottom center
        int16_t filename_y = disp_h - 20; // 20 pixels from bottom
        drawString(canvas, disp_w / 2, filename_y, displayName, CENTER, DISPLAY_COLOR_BLACK);

        log_d("drawError: Filename '%s' drawn at bottom", displayName);
    }
}

void drawErrorWithDetails(GFXcanvas8& canvas,
                          const String& errMsgLn1,
                          const String& errMsgLn2,
                          const char* filename,
                          uint16_t errorCode) {
    // Clear canvas
    canvas.fillScreen(DISPLAY_COLOR_WHITE);

    // Draw main error message
    drawString(canvas,
               canvas.width() / 2,
               canvas.height() / 2 - 20,
               errMsgLn1.c_str(),
               CENTER,
               DISPLAY_COLOR_BLACK);

    // Draw secondary message if provided
    if (errMsgLn2.length() > 0) {
        drawString(canvas,
                   canvas.width() / 2,
                   canvas.height() / 2,
                   errMsgLn2.c_str(),
                   CENTER,
                   DISPLAY_COLOR_BLACK);
    }

    // Draw filename and error code if provided
    if (filename && errorCode > 0) {
        String details = String(filename) + " (Error: " + String(errorCode) + ")";
        drawString(canvas,
                   canvas.width() / 2,
                   canvas.height() / 2 + 20,
                   details.c_str(),
                   CENTER,
                   DISPLAY_COLOR_BLACK);
    }
}

void drawSideMessageWithIcon(GFXcanvas8& canvas,
                             gravity_t gravity,
                             icon_name_t icon_name,
                             const char* message,
                             int32_t x_offset,
                             int32_t y_offset) {
    canvas.setFont(&FONT_7pt8b);
    canvas.setTextColor(DISPLAY_COLOR_BLACK);

    // Get icon bitmap
    const unsigned char* icon = getBitmap(icon_name, 16);
    if (!icon)
        return;

    // Calculate positions based on gravity
    int16_t icon_x = 0, icon_y = 0;
    int16_t text_x = 0, text_y = 0;

    switch (gravity) {
    case TOP_LEFT:
        icon_x = 2 + x_offset;
        icon_y = 0 + y_offset;
        text_x = 20 + x_offset;
        text_y = 10 + y_offset;
        break;
    case TOP_RIGHT: {
        int16_t text_width = getStringWidth(canvas, String(message));
        icon_x             = canvas.width() - text_width - 20 + x_offset;
        icon_y             = 0 + y_offset;
        text_x             = icon_x + 18;
        text_y             = 10 + y_offset;
    } break;
    case TOP_CENTER: {
        int16_t text_width = getStringWidth(canvas, String(message));
        icon_x             = (canvas.width() - text_width - 18) / 2 + x_offset;
        icon_y             = 0 + y_offset;
        text_x             = icon_x + 18;
        text_y             = 10 + y_offset;
    } break;
    default: break;
    }

    // Draw icon
    drawInvertedBitmap(canvas, icon_x, icon_y, icon, 16, 16, DISPLAY_COLOR_BLACK);

    // Draw text
    canvas.setCursor(text_x, text_y);
    canvas.print(message);
}

void drawSideMessage(GFXcanvas8& canvas,
                     gravity_t gravity,
                     const char* message,
                     int32_t x_offset,
                     int32_t y_offset) {
    canvas.setFont(&FONT_7pt8b);
    canvas.setTextColor(DISPLAY_COLOR_BLACK);

    // Calculate position based on gravity
    int16_t x = 0, y = 0;

    switch (gravity) {
    case TOP_LEFT:
        x = 2 + x_offset;
        y = 10 + y_offset;
        canvas.setCursor(x, y);
        canvas.print(message);
        break;
    case TOP_RIGHT: {
        int16_t text_width = getStringWidth(canvas, String(message));
        x                  = canvas.width() - text_width - 2 + x_offset;
        y                  = 10 + y_offset;
        canvas.setCursor(x, y);
        canvas.print(message);
    } break;
    case TOP_CENTER: {
        int16_t text_width = getStringWidth(canvas, String(message));
        x                  = (canvas.width() - text_width) / 2 + x_offset;
        y                  = 10 + y_offset;
        canvas.setCursor(x, y);
        canvas.print(message);
    } break;
    default: break;
    }
}

rect_t getTextBounds(GFXcanvas8& canvas, const char* text, int16_t x, int16_t y) {
    rect_t bounds;
    int16_t x1, y1;
    uint16_t w, h;

    canvas.getTextBounds(text, x, y, &x1, &y1, &w, &h);

    bounds.x      = x1;
    bounds.y      = y1;
    bounds.width  = w;
    bounds.height = h;

    return bounds;
}

uint16_t getStringWidth(GFXcanvas8& canvas, const String& text) {
    rect_t bounds = getTextBounds(canvas, text.c_str(), 0, 0);
    return bounds.width;
}

uint16_t getStringHeight(GFXcanvas8& canvas, const String& text) {
    rect_t bounds = getTextBounds(canvas, text.c_str(), 0, 0);
    return bounds.height;
}

void drawString(GFXcanvas8& canvas,
                int16_t x,
                int16_t y,
                const char* text,
                alignment_t alignment,
                uint16_t color) {
    canvas.setTextColor(color);

    // Calculate aligned position
    int16_t draw_x = x;
    if (alignment != LEFT) {
        int16_t text_width = getStringWidth(canvas, String(text));
        if (alignment == CENTER) {
            draw_x = x - text_width / 2;
        } else if (alignment == RIGHT) {
            draw_x = x - text_width;
        }
    }

    canvas.setCursor(draw_x, y);
    canvas.print(text);
}

void drawMultilineString(GFXcanvas8& canvas,
                         int16_t x,
                         int16_t y,
                         const String& text,
                         alignment_t alignment,
                         uint16_t max_width,
                         uint16_t max_lines,
                         int16_t line_spacing,
                         uint16_t color) {
    canvas.setTextColor(color);

    uint16_t current_line = 0;
    String textRemaining  = text;
    int16_t current_y     = y;

    // Safety check for invalid parameters
    if (max_width == 0 || max_lines == 0 || text.isEmpty()) {
        return;
    }

    // Print until we reach max_lines or no more text remains
    while (current_line < max_lines && !textRemaining.isEmpty()) {
        // Get text bounds to check width
        int16_t x1, y1;
        uint16_t w, h;
        canvas.getTextBounds(textRemaining.c_str(), 0, 0, &x1, &y1, &w, &h);

        int endIndex             = textRemaining.length();
        String subStr            = textRemaining;
        int splitAt              = 0;
        int keepLastChar         = 0;
        int iterations           = 0; // Safety counter to prevent infinite loops
        const int MAX_ITERATIONS = 100;

        // Check if remaining text is too wide, if it is then print what we can
        while (w > max_width && splitAt != -1 && iterations < MAX_ITERATIONS) {
            iterations++;

            if (keepLastChar) {
                // Remove last character to avoid infinite loop
                if (subStr.length() > 0) {
                    subStr.remove(subStr.length() - 1);
                } else {
                    break; // Safety: avoid removing from empty string
                }
            }

            // Find the last place in the string that we can break it
            if (current_line < max_lines - 1) {
                // Not the last line - break at space or hyphen
                int spacePos  = subStr.lastIndexOf(" ");
                int hyphenPos = subStr.lastIndexOf("-");
                splitAt       = (spacePos > hyphenPos) ? spacePos : hyphenPos;
            } else {
                // This is the last line, only break at spaces so we can add ellipsis
                splitAt = subStr.lastIndexOf(" ");
            }

            // If splitAt == -1 then there is an unbroken set of characters longer than max_width
            if (splitAt != -1) {
                endIndex = splitAt;

                // Check if we're splitting at a space or hyphen
                char splitChar = subStr.charAt(splitAt);
                if (splitChar == ' ') {
                    // Don't include the space in the output
                    subStr       = subStr.substring(0, endIndex);
                    keepLastChar = 0;
                } else if (splitChar == '-') {
                    // Include the hyphen in the output
                    subStr       = subStr.substring(0, endIndex + 1);
                    keepLastChar = 1;
                } else {
                    // Shouldn't happen, but handle it
                    subStr       = subStr.substring(0, endIndex);
                    keepLastChar = 0;
                }

                // Recalculate bounds for the substring
                canvas.getTextBounds(subStr.c_str(), 0, 0, &x1, &y1, &w, &h);
            } else {
                // No valid split point found
                if (subStr.length() == 0 || iterations >= MAX_ITERATIONS - 1) {
                    // Force truncate to prevent infinite loop
                    // Estimate how many characters might fit
                    int avgCharWidth = 12; // Approximate for large font
                    int maxChars     = max_width / avgCharWidth;
                    if (maxChars > 0 && textRemaining.length() > maxChars) {
                        subStr   = textRemaining.substring(0, maxChars);
                        endIndex = maxChars;
                    } else {
                        // Just take what we can
                        subStr   = textRemaining;
                        endIndex = textRemaining.length();
                    }
                    break;
                }
            }
        }

        // Check if this is the last line and we have more text
        if (current_line == max_lines - 1 && endIndex < textRemaining.length()) {
            // Add ellipsis if we're truncating
            if (endIndex > 3) {
                subStr = textRemaining.substring(0, endIndex - 3) + "...";
            } else {
                subStr = "...";
            }
        } else if (!keepLastChar && endIndex > 0 && endIndex < textRemaining.length()) {
            // Remove trailing space if not keeping last character
            char lastChar = subStr.charAt(endIndex - 1);
            if (lastChar == ' ') {
                subStr = textRemaining.substring(0, endIndex - 1);
                endIndex--;
            } else {
                subStr = textRemaining.substring(0, endIndex);
            }
        } else {
            subStr = textRemaining.substring(0, endIndex);
        }

        // Draw the line with proper alignment
        drawString(canvas, x, current_y, subStr.c_str(), alignment, color);

        // Update remaining text
        if (endIndex < textRemaining.length()) {
            textRemaining = textRemaining.substring(endIndex);
            // Skip leading spaces on next line
            while (textRemaining.length() > 0 && textRemaining.charAt(0) == ' ') {
                textRemaining.remove(0, 1);
            }
        } else {
            textRemaining = "";
        }

        current_y += line_spacing;
        current_line++;
    }
}

void drawRoundedRect(GFXcanvas8& canvas,
                     int16_t x,
                     int16_t y,
                     int16_t width,
                     int16_t height,
                     uint16_t radius,
                     uint16_t color,
                     uint8_t transparency) {
    // Draw filled rounded rectangle
    canvas.fillRoundRect(x, y, width, height, radius, color);

    // If transparency requested, create a dithering pattern
    if (transparency > 0) {
        // Simple checkeboard pattern for transparency effect
        for (int16_t dy = 0; dy < height; dy++) {
            for (int16_t dx = 0; dx < width; dx++) {
                // Create dithering based on transparency level
                bool draw_pixel = false;
                if (transparency < 64) {
                    // 75% opacity - skip every 4th pixel
                    draw_pixel = ((dx + dy) % 4) == 0;
                } else if (transparency < 128) {
                    // 50% opacity - checkerboard
                    draw_pixel = ((dx + dy) % 2) == 0;
                } else if (transparency < 192) {
                    // 25% opacity - draw every 4th pixel
                    draw_pixel = ((dx + dy) % 4) != 0;
                } else {
                    // Almost transparent - draw very few pixels
                    draw_pixel = ((dx + dy) % 8) != 0;
                }

                if (draw_pixel) {
                    // Check if pixel is within rounded rect bounds
                    if (dx < radius || dx >= width - radius || dy < radius ||
                        dy >= height - radius) {
                        // Corner regions - check distance from corner center
                        int16_t cx, cy;
                        if (dx < radius && dy < radius) {
                            cx = radius;
                            cy = radius;
                        } else if (dx >= width - radius && dy < radius) {
                            cx = width - radius - 1;
                            cy = radius;
                        } else if (dx < radius && dy >= height - radius) {
                            cx = radius;
                            cy = height - radius - 1;
                        } else if (dx >= width - radius && dy >= height - radius) {
                            cx = width - radius - 1;
                            cy = height - radius - 1;
                        } else {
                            // Not in corner, draw pixel
                            canvas.drawPixel(x + dx, y + dy, DISPLAY_COLOR_WHITE);
                            continue;
                        }

                        // Check if within circle for corner
                        int16_t dist_sq = (dx - cx) * (dx - cx) + (dy - cy) * (dy - cy);
                        if (dist_sq <= radius * radius) {
                            canvas.drawPixel(x + dx, y + dy, DISPLAY_COLOR_WHITE);
                        }
                    } else {
                        // Inside rectangle, draw pixel
                        canvas.drawPixel(x + dx, y + dy, DISPLAY_COLOR_WHITE);
                    }
                }
            }
        }
    }
}

} // namespace photo_frame