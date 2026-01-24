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
#include <FS.h>
#include <LittleFS.h>

namespace photo_frame {
namespace littlefs_manager {

/**
 * @brief Manages SPI configuration switching between SD card and e-paper display
 *
 * This utility class handles the complexity of using different SPI pins
 * for SD card and e-paper display when using shared SPI mode on ESP32-C6.
 */
class LittleFsManager {
  public:
    static LittleFsManager& getInstance();

    ~LittleFsManager();

    /**
     * @brief Initialize LittleFS if not already mounted
     * @return true if LittleFS is ready, false on error
     */
    bool init();

    /**
     * @brief Release LittleFS resources
     */
    void release();

    /**
     * @brief Clean up temporary files from LittleFS
     * @param pattern File pattern to match (e.g., "*.tmp")
     */
    void cleanup_temp_files();

    /**
     * @brief List all files in LittleFS (for debugging)
     * @param path Directory path to list
     */
    void list_files(const char* path);

    /**
     * @brief Copy a file from SD card to LittleFS
     * @param source_file Open file handle from SD card
     * @param dest_filename Destination filename in LittleFS (including path)
     * @return true if copy succeeded, false on error
     */
    bool copy_to_littlefs(fs::File& source_file, const char* dest_filename);

    /**
     * @brief Open a file in LittleFS
     * @param filename Filename to open (including path)
     * @param mode File open mode (e.g., "r", "w")
     * @return Opened File object, or invalid File on error
     */
    File open_file(const char* filename, const char* mode);

    /**
     * @brief Read a file from LittleFS into a buffer
     * @param filename Filename to read (including path)
     * @param buffer Destination buffer
     * @param buffer_size Maximum bytes to read
     * @return Number of bytes read, or 0 on error
     */
    size_t read_file(const char* filename, uint8_t* buffer, size_t buffer_size);

    /**
     * @brief Delete a file from LittleFS
     * @param filename Filename to delete (including path)
     * @return true if deletion succeeded, false on error
     */
    bool delete_file(const char* filename);

    /**
     * @brief Write a buffer to a file in LittleFS
     * @param filename Filename to write (including path)
     * @param buffer Data to write
     * @param buffer_size Number of bytes to write
     * @return true if write succeeded, false on error
     */
    bool write_file(const char* filename, const uint8_t* buffer, size_t buffer_size);

    /**
     * @brief Get the size of a file in LittleFS
     * @param filename Filename to check (including path)
     * @return File size in bytes, or 0 if file doesn't exist
     */
    size_t get_file_size(const char* filename);

    /**
     * @brief Check if a file exists in LittleFS
     * @param filename Filename to check (including path)
     * @return true if file exists, false otherwise
     */
    bool file_exists(const char* filename);

  private:
    LittleFsManager() = default;
};

} // namespace littlefs_manager
} // namespace photo_frame

#endif // __SPI_MANAGER_H__