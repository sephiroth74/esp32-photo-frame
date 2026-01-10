#include "canvas_renderer.h"
#include "config.h"
#include "datetime_utils.h"
#include <Fonts/FreeMonoBold9pt7b.h>
#include <assets/fonts/Ubuntu_R.h>
#include <assets/icons/icons.h>

namespace photo_frame {
namespace canvas_renderer {

// Helper function to draw inverted bitmap (for icons)
static void draw_inverted_bitmap(GFXcanvas8& canvas,
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
            if (i & 7)
                byte <<= 1;
            else {
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

void draw_overlay(GFXcanvas8& canvas) {
    // Determine portrait mode from canvas rotation
    // Rotation 1 or 3 = portrait, 0 or 2 = landscape
    bool portrait_mode = (canvas.getRotation() == 1 || canvas.getRotation() == 3);

    log_i("draw_overlay(rotation=%d)", canvas.getRotation());
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

void draw_last_update(GFXcanvas8& canvas, const DateTime& lastUpdate, long refresh_seconds) {
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
        draw_side_message_with_icon(
            canvas, gravity::TOP_LEFT, icon_name::wi_time_10, lastUpdateBuffer, 0, -2);
    } else {
        draw_side_message(canvas, gravity::TOP_LEFT, lastUpdateBuffer, 0, 0);
    }
}

void draw_battery_status(GFXcanvas8& canvas, battery_info_t battery_info) {
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

    draw_side_message_with_icon(canvas, TOP_RIGHT, icon_name, message.c_str(), 0, 0);
}

void draw_image_info(GFXcanvas8& canvas,
                     uint32_t index,
                     uint32_t total_images,
                     image_source_t image_source) {
    String message = String(index + 1) + " / " + String(total_images);
    draw_side_message_with_icon(canvas,
                                gravity::TOP_CENTER,
                                image_source == photo_frame::image_source_t::IMAGE_SOURCE_CLOUD
                                    ? icon_name::cloud_0deg
                                    : icon_name::micro_sd_card_0deg,
                                message.c_str(),
                                -4,
                                0);
}

void draw_error(GFXcanvas8& canvas, photo_frame_error_t error) {
    // Clear canvas
    canvas.fillScreen(DISPLAY_COLOR_WHITE);

    // Draw error message centered
    const char* errorMsg = "Error";
    if (error.code != 0) {
        errorMsg = error.message ? error.message : "Unknown Error";
    }

    draw_string(
        canvas, canvas.width() / 2, canvas.height() / 2, errorMsg, CENTER, DISPLAY_COLOR_BLACK);
}

void draw_error_with_details(GFXcanvas8& canvas,
                             const String& errMsgLn1,
                             const String& errMsgLn2,
                             const char* filename,
                             uint16_t errorCode) {
    // Clear canvas
    canvas.fillScreen(DISPLAY_COLOR_WHITE);

    // Draw main error message
    draw_string(canvas,
                canvas.width() / 2,
                canvas.height() / 2 - 20,
                errMsgLn1.c_str(),
                CENTER,
                DISPLAY_COLOR_BLACK);

    // Draw secondary message if provided
    if (errMsgLn2.length() > 0) {
        draw_string(canvas,
                    canvas.width() / 2,
                    canvas.height() / 2,
                    errMsgLn2.c_str(),
                    CENTER,
                    DISPLAY_COLOR_BLACK);
    }

    // Draw filename and error code if provided
    if (filename && errorCode > 0) {
        String details = String(filename) + " (Error: " + String(errorCode) + ")";
        draw_string(canvas,
                    canvas.width() / 2,
                    canvas.height() / 2 + 20,
                    details.c_str(),
                    CENTER,
                    DISPLAY_COLOR_BLACK);
    }
}

void draw_side_message_with_icon(GFXcanvas8& canvas,
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
        int16_t text_width = get_string_width(canvas, String(message));
        icon_x             = canvas.width() - text_width - 20 + x_offset;
        icon_y             = 0 + y_offset;
        text_x             = icon_x + 18;
        text_y             = 10 + y_offset;
    } break;
    case TOP_CENTER: {
        int16_t text_width = get_string_width(canvas, String(message));
        icon_x             = (canvas.width() - text_width - 18) / 2 + x_offset;
        icon_y             = 0 + y_offset;
        text_x             = icon_x + 18;
        text_y             = 10 + y_offset;
    } break;
    default: break;
    }

    // Draw icon
    draw_inverted_bitmap(canvas, icon_x, icon_y, icon, 16, 16, DISPLAY_COLOR_BLACK);

    // Draw text
    canvas.setCursor(text_x, text_y);
    canvas.print(message);
}

void draw_side_message(GFXcanvas8& canvas,
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
        int16_t text_width = get_string_width(canvas, String(message));
        x                  = canvas.width() - text_width - 2 + x_offset;
        y                  = 10 + y_offset;
        canvas.setCursor(x, y);
        canvas.print(message);
    } break;
    case TOP_CENTER: {
        int16_t text_width = get_string_width(canvas, String(message));
        x                  = (canvas.width() - text_width) / 2 + x_offset;
        y                  = 10 + y_offset;
        canvas.setCursor(x, y);
        canvas.print(message);
    } break;
    default: break;
    }
}

rect_t get_text_bounds(GFXcanvas8& canvas, const char* text, int16_t x, int16_t y) {
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

uint16_t get_string_width(GFXcanvas8& canvas, const String& text) {
    rect_t bounds = get_text_bounds(canvas, text.c_str(), 0, 0);
    return bounds.width;
}

uint16_t get_string_height(GFXcanvas8& canvas, const String& text) {
    rect_t bounds = get_text_bounds(canvas, text.c_str(), 0, 0);
    return bounds.height;
}

void draw_string(GFXcanvas8& canvas,
                 int16_t x,
                 int16_t y,
                 const char* text,
                 alignment_t alignment,
                 uint16_t color) {
    canvas.setTextColor(color);

    // Calculate aligned position
    int16_t draw_x = x;
    if (alignment != LEFT) {
        int16_t text_width = get_string_width(canvas, String(text));
        if (alignment == CENTER) {
            draw_x = x - text_width / 2;
        } else if (alignment == RIGHT) {
            draw_x = x - text_width;
        }
    }

    canvas.setCursor(draw_x, y);
    canvas.print(text);
}

void draw_multiline_string(GFXcanvas8& canvas,
                           int16_t x,
                           int16_t y,
                           const String& text,
                           alignment_t alignment,
                           uint16_t max_width,
                           uint16_t max_lines,
                           int16_t line_spacing,
                           uint16_t color) {
    canvas.setTextColor(color);

    // Simple implementation - split by newlines
    String remaining     = text;
    int16_t current_y    = y;
    uint16_t lines_drawn = 0;

    while (remaining.length() > 0 && lines_drawn < max_lines) {
        int newline_pos = remaining.indexOf('\n');
        String line;

        if (newline_pos >= 0) {
            line      = remaining.substring(0, newline_pos);
            remaining = remaining.substring(newline_pos + 1);
        } else {
            line      = remaining;
            remaining = "";
        }

        // Draw the line
        draw_string(canvas, x, current_y, line.c_str(), alignment, color);

        current_y += line_spacing;
        lines_drawn++;
    }
}

void draw_rounded_rect(GFXcanvas8& canvas,
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

} // namespace canvas_renderer
} // namespace photo_frame