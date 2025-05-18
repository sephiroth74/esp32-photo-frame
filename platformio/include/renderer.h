#ifndef PHOTO_FRAME_RENDERER_H__
#define PHOTO_FRAME_RENDERER_H__

#include "DEV_Config.h"
#include "config.h"
#include <Arduino.h>
#include <Battery.h>
#include <FS.h>
#include <SDCard.h>
#include <fonts.h>

namespace photo_frame {

typedef enum font_size {
    FontSize10pt = 10,
    FontSize14pt = 14,
    FontSize24pt = 24,
} font_size_t;

typedef enum renderer_error_code {
    CoordinatesOutOfBounds = 0,
    InvalidFileFormat,
    FileReadError,
    OperatoionComplete,
} renderer_error_code_t;

class Renderer {
  public:
    Renderer();
    ~Renderer();

    bool init();
    renderer_error_code_t drawBitmapFromSD(SDCard& sdCard, const char* filename, UWORD x, UWORD y);
    renderer_error_code_t drawBitmapFromFile(fs::File file, UWORD x = 0, UWORD y = 0);
    void drawCenteredImage(const unsigned char* image_buffer, UWORD W_Image, UWORD H_Image);
    void drawCenteredWarningMessage(const char* message);
    void drawCentererMessage(const char* message,
                             font_size_t fontSize,
                             uint16_t fgColor,
                             uint16_t bgColor);
    void drawBatteryStatus(const Battery& battery);
    void display();
    void sleep();
    void clear();

  private:
    UBYTE* BlackImage;
};
}; // namespace photo_frame

#endif // PHOTO_FRAME_RENDERER_H__