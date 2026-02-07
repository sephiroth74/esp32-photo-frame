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

#pragma once

#include "binary_utils.h"
#include "errors.h"
#include "sd_card.h"
#include <Arduino.h>
#include <FS.h>

namespace photo_frame {
namespace io_utils {

    /**
     * @brief Detect binary format based on filename extension for runtime rendering selection
     *
     * This function checks if a file is in binary format (.pfr1 extension) used by the
     * ESP32 photo frame for optimized e-paper rendering.
     *
     * @param filename The filename to examine (must include extension)
     * @return true if binary format (.pfr1) - uses optimized binary renderer
     * @return false if filename is null, has no extension, or is not a .pfr1 file
     *
     * @note Only binary format (.pfr1) is supported - other formats will return false
     *
     * @example
     * ```cpp
     * if (photo_frame::io_utils::is_binary_format("image.pfr1")) {
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