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

#include "spi_manager.h"
#include "config.h"
#include "renderer.h"
#include <LittleFS.h>
#include <SD_MMC.h>

namespace photo_frame {
namespace spi_manager {

bool SPIManager::littlefs_initialized = false;


bool SPIManager::init_littlefs() {
    if (littlefs_initialized) {
        return true;
    }

    // First try to mount without formatting
    if (!LittleFS.begin(false)) {
        Serial.println("[spi_manager] LittleFS mount failed, attempting format...");

        // Force format and try again
        if (!LittleFS.begin(true)) {
            Serial.println("[spi_manager] LittleFS format and initialization failed");
            return false;
        }
        Serial.println("[spi_manager] LittleFS formatted and mounted successfully");
    } else {
        Serial.println("[spi_manager] LittleFS mounted successfully");

        // Check for filesystem corruption by testing basic operations
        size_t totalBytes = LittleFS.totalBytes();
        if (totalBytes == 0) {
            Serial.println(
                "[spi_manager] LittleFS appears corrupted (totalBytes = 0), reformatting...");
            LittleFS.end();
            if (!LittleFS.begin(true)) {
                Serial.println("[spi_manager] LittleFS reformat failed");
                return false;
            }
            Serial.println("[spi_manager] LittleFS successfully reformatted");
        }
    }

    littlefs_initialized = true;
    Serial.println("[spi_manager] LittleFS initialized successfully");

    // Show detailed filesystem info for debugging LittleFS issues
    size_t totalBytes = LittleFS.totalBytes();
    size_t usedBytes  = LittleFS.usedBytes();
    size_t freeBytes  = totalBytes - usedBytes;

    Serial.printf("[spi_manager] LittleFS - Total: %zu bytes (%.2f MB)\n",
                  totalBytes,
                  totalBytes / 1024.0 / 1024.0);
    Serial.printf("[spi_manager] LittleFS - Used: %zu bytes (%.2f MB)\n",
                  usedBytes,
                  usedBytes / 1024.0 / 1024.0);
    Serial.printf("[spi_manager] LittleFS - Free: %zu bytes (%.2f MB)\n",
                  freeBytes,
                  freeBytes / 1024.0 / 1024.0);

    // Check if there's enough space for a 384KB image file
    const size_t IMAGE_SIZE = DISP_WIDTH * DISP_HEIGHT * 3; // 384KB for 800x480 RGB
    if (freeBytes < IMAGE_SIZE) {
        Serial.printf("[spi_manager] ⚠️ WARNING: Only %zu bytes free, need %zu for image file\n",
                      freeBytes,
                      IMAGE_SIZE);
    } else {
        Serial.printf("[spi_manager] ✅ Sufficient space: %zu bytes available for %zu byte image\n",
                      freeBytes,
                      IMAGE_SIZE);
    }

    return true;
}

void SPIManager::cleanup_temp_files(const char* pattern) {
    if (!littlefs_initialized) {
        return;
    }

    File root = LittleFS.open("/");
    if (!root || !root.isDirectory()) {
        Serial.println("[spi_manager] Failed to open LittleFS root directory");
        return;
    }

    int files_deleted = 0;
    File file         = root.openNextFile();
    while (file) {
        String filename = file.name();

        // Simple pattern matching for .tmp files
        if (filename.endsWith(".tmp")) {
            String filepath = "/" + filename;
            file.close();

            if (LittleFS.remove(filepath.c_str())) {
                Serial.printf("[spi_manager] Deleted temp file: %s\n", filepath.c_str());
                files_deleted++;
            } else {
                Serial.printf("[spi_manager] Failed to delete: %s\n", filepath.c_str());
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
        Serial.printf("[spi_manager] Cleaned up %d temporary files from LittleFS\n", files_deleted);
    }
}

} // namespace spi_manager
} // namespace photo_frame