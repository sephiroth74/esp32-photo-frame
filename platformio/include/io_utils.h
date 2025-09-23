// MIT License
//
// Copyright (c) 2025 Alessandro Crugnola
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

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

/**
 * @brief Detect file format based on filename extension for runtime rendering selection
 *
 * This function enables runtime file format detection by examining the file extension.
 * It's used throughout the ESP32 photo frame system to automatically select the
 * appropriate rendering engine without compile-time configuration.
 *
 * @param filename The filename to examine (must include extension)
 * @return true if binary format (.bin) - uses optimized binary renderer
 * @return false if bitmap format (.bmp) - uses standard bitmap renderer
 * @return false if filename is null or has no extension
 *
 * @note This replaces the old compile-time EPD_USE_BINARY_FILE flag system
 * @note Both formats can coexist in the same directory/Google Drive folder
 * @see ALLOWED_FILE_EXTENSIONS for complete list of supported extensions
 *
 * @example
 * ```cpp
 * if (photo_frame::io_utils::is_binary_format("image.bin")) {
 *     // Use binary rendering engine
 *     draw_binary_from_file(...);
 * } else {
 *     // Use bitmap rendering engine
 *     draw_bitmap_from_file(...);
 * }
 * ```
 */
bool is_binary_format(const char* filename);

/**
 * @brief Detect if the file is in BMP format based on filename extension
 * 
 * @param filename The filename to examine (must include extension)
 * @return true if BMP format (.bmp) - uses standard bitmap renderer
 * @return false if filename is null or has no extension
 */
bool is_bmp_format(const char* filename);

} // namespace io_utils
} // namespace photo_frame