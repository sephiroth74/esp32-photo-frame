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
#include <SPI.h>
#include <SD.h>
#include <LittleFS.h>

namespace photo_frame {
namespace spi_manager {

    bool SPIManager::littlefs_initialized = false;

    void SPIManager::configure_spi_for_sd() {
#ifdef USE_SHARED_SPI
        SPI.end();
        SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
        Serial.printf("[spi_manager] SPI configured for SD: SCK=%d, MOSI=%d, MISO=%d\n", 
                     SD_SCK_PIN, SD_MOSI_PIN, SD_MISO_PIN);
#endif
    }

    void SPIManager::configure_spi_for_epd() {
#ifdef USE_SHARED_SPI
        SPI.end();
        SPI.begin(EPD_SCK_PIN, EPD_MISO_PIN, EPD_MOSI_PIN, EPD_CS_PIN);
        Serial.printf("[spi_manager] SPI configured for EPD: SCK=%d, MOSI=%d, MISO=%d\n", 
                     EPD_SCK_PIN, EPD_MOSI_PIN, EPD_MISO_PIN);
#endif
    }

    bool SPIManager::init_littlefs() {
        if (littlefs_initialized) {
            return true;
        }

        if (!LittleFS.begin(true)) { // format if mount fails
            Serial.println("[spi_manager] LittleFS initialization failed");
            return false;
        }

        littlefs_initialized = true;
        Serial.println("[spi_manager] LittleFS initialized successfully");
        
        // Show filesystem info
        size_t total_bytes = LittleFS.totalBytes();
        size_t used_bytes = LittleFS.usedBytes();
        Serial.printf("[spi_manager] LittleFS: %d KB total, %d KB used, %d KB free\n",
                     total_bytes / 1024, used_bytes / 1024, (total_bytes - used_bytes) / 1024);
        
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
        File file = root.openNextFile();
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