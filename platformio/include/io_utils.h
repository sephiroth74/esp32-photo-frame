#pragma once

#include "errors.h"
#include "sd_card.h"
#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>

namespace photo_frame {
namespace io_utils {

/**
 * @brief Copy a file from SD card to LittleFS
 *
 * This utility copies image files from SD card to LittleFS for shared SPI scenarios.
 * In shared SPI configurations, the SD card and e-paper display cannot operate
 * simultaneously, so files must be copied to LittleFS before the SD card is shut down
 * and e-paper display operations begin.
 *
 * @param sourceFile Open SD card file to copy from (caller responsible for closing)
 * @param littlefsPath Target path in LittleFS (e.g., "/temp_image.tmp")
 * @return photo_frame_error_t Error code (None on success)
 */
photo_frame_error_t copy_sd_to_littlefs(fs::File& sourceFile, const String& littlefsPath);
} // namespace io_utils
} // namespace photo_frame