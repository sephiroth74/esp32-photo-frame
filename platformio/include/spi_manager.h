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

#ifndef __SPI_MANAGER_H__
#define __SPI_MANAGER_H__

#include <Arduino.h>

namespace photo_frame {
namespace spi_manager {

/**
 * @brief Manages SPI configuration switching between SD card and e-paper display
 *
 * This utility class handles the complexity of using different SPI pins
 * for SD card and e-paper display when using shared SPI mode on ESP32-C6.
 */
class SPIManager {
  public:
    /**
     * @brief Initialize LittleFS if not already mounted
     * @return true if LittleFS is ready, false on error
     */
    static bool init_littlefs();

    /**
     * @brief Clean up temporary files from LittleFS
     * @param pattern File pattern to match (e.g., "*.tmp")
     */
    static void cleanup_temp_files(const char* pattern = "*.tmp");

  private:
    static bool littlefs_initialized;
};

} // namespace spi_manager
} // namespace photo_frame

#endif // __SPI_MANAGER_H__