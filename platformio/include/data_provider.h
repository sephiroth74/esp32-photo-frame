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

#ifndef PHOTO_FRAME_DATA_PROVIDER_H
#define PHOTO_FRAME_DATA_PROVIDER_H

#include "battery.h"
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
    virtual ~DataProvider()          = default;

    virtual const char* name() const = 0;

    /**
     * @brief Load the next image from this data provider
     *
     * @param is_reset Whether this is a reset/first load
     * @param sd_card Reference to SD card instance
     * @param config Reference to unified configuration
     * @return ImageLoadResult containing the image file, metadata, and error status
     *
     * The caller is responsible for:
     * - Rendering the image to display
     * - Handling any errors returned
     * - Managing the returned PFR1BinaryFile lifetime
     */
    virtual ImageLoadResult
    load_next_image(bool is_reset, SdCard& sd_card, const unified_config& config) = 0;
};

} // namespace photo_frame

#endif // PHOTO_FRAME_DATA_PROVIDER_H#endif // PHOTO_FRAME_DATA_PROVIDER_H