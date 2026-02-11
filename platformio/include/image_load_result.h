// ESP32 Photo Frame
// Copyright (C) 2025 Alessandro Crugnola
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

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
  std::unique_ptr<PFR1BinaryFile> image_file;

  // Metadata
  String original_filename;
  uint32_t file_index = 0;
  uint32_t total_files = 0;

  // Status
  photo_frame_error_t error = error_type::None;

  // Constructor for success case
  ImageLoadResult(std::unique_ptr<PFR1BinaryFile> file, const String &filename, uint32_t index, uint32_t total)
      : image_file(std::move(file)), original_filename(filename), file_index(index), total_files(total), error(error_type::None) {}

  // Constructor for error case
  explicit ImageLoadResult(photo_frame_error_t err)
      : image_file(nullptr), original_filename(""), file_index(0), total_files(0), error(err) {}

  // Default constructor
  ImageLoadResult() = default;

  // Move semantics
  ImageLoadResult(ImageLoadResult &&) = default;
  ImageLoadResult &operator=(ImageLoadResult &&) = default;

  // Delete copy semantics (unique_ptr is non-copyable)
  ImageLoadResult(const ImageLoadResult &) = delete;
  ImageLoadResult &operator=(const ImageLoadResult &) = delete;

  // Check if load was successful
  bool is_success() const { return error == error_type::None && image_file != nullptr && image_file->isValidated(); }
};

} // namespace photo_frame

#endif // PHOTO_FRAME_IMAGE_LOAD_RESULT_H
