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
 * @brief Detect binary format based on filename extension for runtime rendering selection
 *
 * This function checks if a file is in binary format (.bin extension) used by the
 * ESP32 photo frame for optimized e-paper rendering.
 *
 * @param filename The filename to examine (must include extension)
 * @return true if binary format (.bin) - uses optimized binary renderer
 * @return false if filename is null, has no extension, or is not a .bin file
 *
 * @note Only binary format (.bin) is supported - other formats will return false
 *
 * @example
 * ```cpp
 * if (photo_frame::io_utils::is_binary_format("image.bin")) {
 *     // Use binary rendering engine
 *     draw_binary_from_file(...);
 * } else {
 *     // Format not supported
 *     log_e("Unsupported file format");
 * }
 * ```
 */
bool is_binary_format(const char* filename);


} // namespace io_utils
} // namespace photo_frame