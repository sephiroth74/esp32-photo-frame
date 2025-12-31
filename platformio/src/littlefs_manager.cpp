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

#include "littlefs_manager.h"
#include "config.h"
#include "renderer.h"

namespace photo_frame {
namespace littlefs_manager {

    bool LittleFsManager::initialized = false;

    bool LittleFsManager::init()
    {
        if (initialized) {
            return true;
        }

        // First try to mount without formatting
        if (!LittleFS.begin(false)) {
            log_w("LittleFS mount failed, attempting format...");

            // Force format and try again
            if (!LittleFS.begin(true)) {
                log_e("LittleFS format and initialization failed");
                return false;
            }
            log_i("LittleFS formatted and mounted successfully");
        } else {
            log_i("LittleFS mounted successfully");

            // Check for filesystem corruption by testing basic operations
            size_t totalBytes = LittleFS.totalBytes();
            if (totalBytes == 0) {
                log_w("LittleFS appears corrupted (totalBytes = 0), reformatting...");
                LittleFS.end();
                if (!LittleFS.begin(true)) {
                    log_e("LittleFS reformat failed");
                    return false;
                }
                log_i("LittleFS successfully reformatted");
            }
        }

        initialized = true;
        log_i("LittleFS initialized successfully");

        // Show detailed filesystem info for debugging LittleFS issues
        size_t totalBytes = LittleFS.totalBytes();
        size_t usedBytes = LittleFS.usedBytes();
        size_t freeBytes = totalBytes - usedBytes;

        log_i("LittleFS - Total: %zu bytes (%.2f MB)",
            totalBytes, totalBytes / 1024.0 / 1024.0);
        log_i("LittleFS - Used: %zu bytes (%.2f MB)",
            usedBytes, usedBytes / 1024.0 / 1024.0);
        log_i("LittleFS - Free: %zu bytes (%.2f MB)",
            freeBytes, freeBytes / 1024.0 / 1024.0);

        // Check if there's enough space for a 384KB image file
        const size_t IMAGE_SIZE = DISP_WIDTH * DISP_HEIGHT * 3; // 384KB for 800x480 RGB
        if (freeBytes < IMAGE_SIZE) {
            log_w("WARNING: Only %zu bytes free, need %zu for image file",
                freeBytes, IMAGE_SIZE);
        } else {
            log_i("Sufficient space: %zu bytes available for %zu byte image",
                freeBytes, IMAGE_SIZE);
        }

        return true;
    }

    void LittleFsManager::cleanup_temp_files()
    {
        if (!initialized) {
            return;
        }

        File root = LittleFS.open("/");
        if (!root || !root.isDirectory()) {
            log_e("Failed to open LittleFS root directory");
            return;
        }

        int files_deleted = 0;
        File file = root.openNextFile();
        while (file) {
            String filename = file.name();

            // Simple pattern matching for .tmp files
            if (filename.endsWith(".tmp")) {
                String filepath = "/" + filename;
                file.close();

                if (LittleFS.remove(filepath.c_str())) {
                    log_i("Deleted temp file: %s", filepath.c_str());
                    files_deleted++;
                } else {
                    log_w("Failed to delete: %s", filepath.c_str());
                }

                // Reopen directory after deletion
                root.close();
                root = LittleFS.open("/");
                file = root.openNextFile();
            } else {
                file.close();
                file = root.openNextFile();
            }
        }

        root.close();

        if (files_deleted > 0) {
            log_i("Cleaned up %d temporary files from LittleFS", files_deleted);
        }
    }

    bool LittleFsManager::copy_to_littlefs(fs::File& source_file, const char* dest_filename)
    {
        if (!initialized) {
            log_e("LittleFS not initialized, cannot copy file");
            return false;
        }

        if (!source_file || source_file.size() == 0) {
            log_e("Invalid source file");
            return false;
        }

        // Ensure we're at the start of the file
        source_file.seek(0);

        // Open destination file for writing
        File dest_file = LittleFS.open(dest_filename, FILE_WRITE, true);
        if (!dest_file) {
            log_e("Failed to create destination file: %s", dest_filename);
            return false;
        }

        size_t source_size = source_file.size();
        log_i("Copying %zu bytes from SD card to LittleFS: %s", source_size, dest_filename);

        // Copy file in chunks to manage memory efficiently
        const size_t CHUNK_SIZE = 4096; // 4KB chunks
        uint8_t* buffer = (uint8_t*)malloc(CHUNK_SIZE);
        if (!buffer) {
            log_e("Failed to allocate copy buffer");
            dest_file.close();
            LittleFS.remove(dest_filename);
            return false;
        }

        size_t total_copied = 0;
        bool success = true;

        while (source_file.available()) {
            size_t bytes_to_read = min(CHUNK_SIZE, (size_t)source_file.available());
            size_t bytes_read = source_file.read(buffer, bytes_to_read);

            if (bytes_read == 0) {
                log_e("Failed to read from source file");
                success = false;
                break;
            }

            size_t bytes_written = dest_file.write(buffer, bytes_read);
            if (bytes_written != bytes_read) {
                log_e("Write error: expected %zu bytes, wrote %zu bytes", bytes_read, bytes_written);
                success = false;
                break;
            }

            total_copied += bytes_written;

            // Yield every chunk to prevent watchdog timeout
            yield();
        }

        free(buffer);

        // Flush and close destination file
        dest_file.flush();
        delay(50); // Give filesystem time to complete write
        dest_file.close();

        if (!success) {
            log_e("Copy failed, cleaning up partial file");
            LittleFS.remove(dest_filename);
            return false;
        }

        // Verify file size matches
        File verify_file = LittleFS.open(dest_filename, FILE_READ);
        if (!verify_file) {
            log_e("Failed to open file for verification: %s", dest_filename);
            return false;
        }

        size_t dest_size = verify_file.size();
        verify_file.close();

        if (dest_size != source_size) {
            log_e("Size mismatch: source=%zu, dest=%zu", source_size, dest_size);
            LittleFS.remove(dest_filename);
            return false;
        }

        log_i("Successfully copied %zu bytes to LittleFS", total_copied);
        return true;
    }

} // namespace littlefs_manager
} // namespace photo_frame