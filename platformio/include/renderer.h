#ifndef __PHOTO_FRAME_RENDERER_H__
#define __PHOTO_FRAME_RENDERER_H__

#include "FS.h"
#include "RTClib.h"
#include "config.h"
#include "errors.h"
#include <Arduino.h>
#include <assets/icons/icons.h>

#if defined(DISP_BW_V2)
#define DISP_WIDTH  800
#define DISP_HEIGHT 480
#include <GxEPD2_BW.h>
#define GxEPD2_DISPLAY_CLASS GxEPD2_BW
#define GxEPD2_DRIVER_CLASS  GxEPD2_750_GDEY075T7
#define MAX_HEIGHT           GxEPD2_DRIVER_CLASS::HEIGHT
extern GxEPD2_DISPLAY_CLASS<GxEPD2_DRIVER_CLASS, MAX_HEIGHT> display;

#elif defined(DISP_7C_F)
#define DISP_WIDTH  800
#define DISP_HEIGHT 480
#include <GxEPD2_7C.h>
#define GxEPD2_DISPLAY_CLASS GxEPD2_7C
#define GxEPD2_DRIVER_CLASS  GxEPD2_730c_GDEY073D46
#define MAX_HEIGHT           GxEPD2_DRIVER_CLASS::HEIGHT / 4
extern GxEPD2_DISPLAY_CLASS<GxEPD2_DRIVER_CLASS, MAX_HEIGHT> display;
#endif

typedef struct rect {
    int16_t x;
    int16_t y;
    uint16_t width;
    uint16_t height;

  public:
    operator bool() const { return (width > 0 && height > 0); }

    inline constexpr bool isEmpty() const { return (width == 0 || height == 0); }
    inline constexpr bool isValid() const { return (x >= 0 && y >= 0 && width > 0 && height > 0); }
    inline constexpr bool contains(int16_t px, int16_t py) const {
        return (px >= x && px < x + width && py >= y && py < y + height);
    }
    inline constexpr bool intersects(const rect& other) const {
        return (x < other.x + other.width && x + width > other.x && y < other.y + other.height &&
                y + height > other.y);
    }

    constexpr rect(int16_t x = 0, int16_t y = 0, uint16_t width = 0, uint16_t height = 0) :
        x(x),
        y(y),
        width(width),
        height(height) {}

    static inline constexpr rect fromXYWH(int16_t x, int16_t y, uint16_t width, uint16_t height) {
        return rect(x, y, width, height);
    }

    static inline constexpr rect empty() { return rect(0, 0, 0, 0); }

} rect_t;

typedef enum alignment { LEFT, RIGHT, CENTER } alignment_t;

typedef enum gravity { TOP_LEFT, TOP_RIGHT, BOTTOM_LEFT, BOTTOM_RIGHT } gravity_t;

namespace photo_frame {
namespace renderer {

rect_t getTextBounds(const char* text, int16_t x = 0, int16_t y = 0);

uint16_t getStringWidth(const String& text);

uint16_t getStringHeight(const String& text);

void initDisplay();

void powerOffDisplay();

void clearScreen(uint16_t value = GxEPD_WHITE);

void refresh(bool partial_update_mode = false);

void firstPage();

bool nextPage();

void setFullWindow();

void fillScreen(uint16_t color = GxEPD_WHITE);

void drawString(int16_t x,
                int16_t y,
                const char* text,
                alignment_t alignment,
                uint16_t color = GxEPD_BLACK);

void drawString(int16_t x, int16_t y, const char* text, uint16_t color = GxEPD_BLACK);

void drawMultiLnString(int16_t x,
                       int16_t y,
                       const String& text,
                       alignment_t alignment,
                       uint16_t max_width,
                       uint16_t max_lines,
                       int16_t line_spacing,
                       uint16_t color = GxEPD_BLACK);

void drawError(photo_frame_error error_type);

void drawError(const uint8_t* bitmap_196x196,
               const String& errMsgLn1,
               const String& errMsgLn2 = "");

void drawErrorMessage(const gravity_t gravity, const uint8_t code);

void drawSideMessageWithIcon(gravity_t gravity, icon_name_t icon_name, const char* message, int32_t x_offset = 0, int32_t y_offset = 0);

void drawLastUpdate(const DateTime& lastUpdate, const char* refreshStr = nullptr);

void drawBatteryStatus(const uint32_t battery_voltage, const uint8_t battery_percentage);

bool drawBitmapFromFile(File& file, int16_t x = 0, int16_t y = 0, bool with_color = true);

bool drawBitmapFromFile_Buffered(File& file,
                                 int16_t x           = 0,
                                 int16_t y           = 0,
                                 bool with_color     = true,
                                 bool partial_update = false);

} // namespace renderer

} // namespace photo_frame

#endif // __PHOTO_FRAME_RENDERER_H__