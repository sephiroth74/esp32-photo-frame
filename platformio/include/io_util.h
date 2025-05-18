#ifndef PHOTO_FRAME_IO_UTIL_H__
#define PHOTO_FRAME_IO_UTIL_H__

#include <Arduino.h>
#include <FS.h>

namespace photo_frame {

uint16_t bmpRead16(File& f);

uint32_t bmpRead32(File& f);

}; // namespace photo_frame

#endif