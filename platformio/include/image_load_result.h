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

#ifndef PHOTO_FRAME_IMAGE_LOAD_RESULT_H
#define PHOTO_FRAME_IMAGE_LOAD_RESULT_H

#include "binary_utils.h"
#include "errors.h"
#include <Arduino.h>

namespace photo_frame {

/**
 * @brief Result from loading an image via DataProvider
 *
 * This struct encapsulates all the information returned when loading an image:
 * - The binary file data (PFR1BinaryFile)
 * - Metadata about the file (filename, index, total count)
 * - Any error that occurred during loading
 *
 * The caller (typically main.cpp) is responsible for rendering the image,
 * writing to display, and handling errors.
 */
struct ImageLoadResult {
    // Image data - populated if error is None
    std::unique_ptr<binary_utils::PFR1BinaryFile> image_file;

    // Metadata
    String original_filename;
    uint32_t file_index  = 0;
    uint32_t total_files = 0;

    // Status
    photo_frame_error_t error = error_type::None;

    // Constructor for success case
    ImageLoadResult(std::unique_ptr<binary_utils::PFR1BinaryFile> file,
                    const String& filename,
                    uint32_t index,
                    uint32_t total) :
        image_file(std::move(file)),
        original_filename(filename),
        file_index(index),
        total_files(total),
        error(error_type::None) {}

    // Constructor for error case
    explicit ImageLoadResult(photo_frame_error_t err) :
        image_file(nullptr),
        original_filename(""),
        file_index(0),
        total_files(0),
        error(err) {}

    // Default constructor
    ImageLoadResult() = default;

    // Move semantics
    ImageLoadResult(ImageLoadResult&&)            = default;
    ImageLoadResult& operator=(ImageLoadResult&&) = default;

    // Delete copy semantics (unique_ptr is non-copyable)
    ImageLoadResult(const ImageLoadResult&)            = delete;
    ImageLoadResult& operator=(const ImageLoadResult&) = delete;

    // Check if load was successful
    bool is_success() const {
        return error == error_type::None && image_file != nullptr && image_file->isValidated();
    }
};

} // namespace photo_frame

#endif // PHOTO_FRAME_IMAGE_LOAD_RESULT_H
