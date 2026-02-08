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

#include "io_utils.h"
#include "config.h"
#include "renderer.h"

namespace photo_frame {
namespace io_utils {

bool is_binary_format(const char* filename) {
    if (!filename)
        return false;

    const char* extension = strrchr(filename, '.');
    if (!extension)
        return false;

    return strcmp(extension, BINARY_FILE_EXTENSION) == 0;
}

} // namespace io_utils
} // namespace photo_frame