#ifndef PHOTO_FRAME_DISPLAY_UTIL_H__
#define PHOTO_FRAME_DISPLAY_UTIL_H__

#include "DEV_Config.h"
#include <Arduino.h>
#include <FS.h>

namespace photo_frame {

typedef enum bitmap_error_code {
    CoordinatesOutOfBounds = 0,
    InvalidFileFormat,
    FileReadError,
    OperatoionComplete,
} bitmap_error_code_t;

bitmap_error_code_t
draw_bitmap_from_sd(const char* filename, UWORD x = 0, UWORD y = 0); // namespace photo_frame

bitmap_error_code_t
draw_bitmap_from_file(fs::File file, UWORD x = 0, UWORD y = 0); // namespace photo_frame

bitmap_error_code_t
draw_bitmap_from_file_4gray(fs::File file, UWORD x = 0, UWORD y = 0); // namespace photo_frame

void draw_centered_image(const unsigned char* image_buffer, UWORD W_Image, UWORD H_Image);

void draw_battery_status(uint32_t voltage, uint32_t percent);

void draw_centered_warning_message(const unsigned char* image_buffer,
                                const char* message,
                                UWORD W_Image,
                                UWORD H_Image);

} // namespace photo_frame

#endif // PHOTO_FRAME_DISPLAY_UTIL_H__