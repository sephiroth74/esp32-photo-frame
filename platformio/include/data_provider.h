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

#ifndef PHOTO_FRAME_DATA_PROVIDER_H
#define PHOTO_FRAME_DATA_PROVIDER_H

#include "battery_manager.h"
#include "errors.h"
#include "image_load_result.h"
#include "sd_card.h"
#include "unified_config.h"
#include <Arduino.h>
#include <FS.h>

namespace photo_frame {

/**
 * @brief Abstract base class for data providers
 * Data providers supply images to the photo frame from various sources
 * (e.g., SD card, Google Drive,...).
 */
class DataProvider {
public:
  virtual ~DataProvider() = default;

  virtual const char *name() const = 0;

  /**
   * @brief Load the next image from this data provider
   *
   * @param is_reset Whether this is a reset/first load
   * @param sd_card Reference to SD card instance
   * @param config Reference to unified configuration
   * @return ImageLoadResult containing the image file, metadata, and error
   * status
   *
   * The caller is responsible for:
   * - Rendering the image to display
   * - Handling any errors returned
   * - Managing the returned PFR1BinaryFile lifetime
   */
  virtual ImageLoadResult load_next_image(bool is_reset, SdCard &sd_card, const unified_config &config) = 0;
};

} // namespace photo_frame

#endif // PHOTO_FRAME_DATA_PROVIDER_H#endif // PHOTO_FRAME_DATA_PROVIDER_H