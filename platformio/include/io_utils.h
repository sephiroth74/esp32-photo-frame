#pragma once

#include "errors.h"
#include "sd_card.h"
#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>

namespace photo_frame {
namespace io_utils {

/**
 * @brief Validate an image file before processing
 *
 * Performs comprehensive validation of an image file including:
 * - File accessibility and basic properties
 * - File size validation (not zero, within reasonable bounds)
 * - Basic file format validation (header checks)
 * - Image dimension validation for binary files
 * - File integrity checks
 *
 * @param sourceFile Open file to validate (file position may be modified)
 * @param filename Filename for error reporting
 * @param expectedWidth Expected image width (for binary files, 0 to skip)
 * @param expectedHeight Expected image height (for binary files, 0 to skip)
 * @return photo_frame_error_t Error code (None if valid)
 */
photo_frame_error_t validate_image_file(fs::File& sourceFile,
                                        const char* filename,
                                        int expectedWidth  = 0,
                                        int expectedHeight = 0);

/**
 * @brief Copy a file from SD card to LittleFS with validation
 *
 * This utility validates and copies image files from SD card to LittleFS for shared SPI scenarios.
 * In shared SPI configurations, the SD card and e-paper display cannot operate
 * simultaneously, so files must be copied to LittleFS before the SD card is shut down
 * and e-paper display operations begin.
 *
 * The file is validated before copying to prevent corrupted files from being stored in LittleFS.
 *
 * @param sourceFile Open SD card file to copy from (caller responsible for closing)
 * @param littlefsPath Target path in LittleFS (e.g., LITTLEFS_TEMP_IMAGE_FILE)
 * @param expectedWidth Expected image width (for binary files, 0 to skip validation)
 * @param expectedHeight Expected image height (for binary files, 0 to skip validation)
 * @return photo_frame_error_t Error code (None on success)
 */
photo_frame_error_t copy_sd_to_littlefs(fs::File& sourceFile,
                                        const String& littlefsPath,
                                        int expectedWidth  = 0,
                                        int expectedHeight = 0);
} // namespace io_utils
} // namespace photo_frame