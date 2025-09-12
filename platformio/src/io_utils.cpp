#include "io_utils.h"
#include "spi_manager.h"

namespace photo_frame {
    namespace io_utils {
        
        photo_frame_error_t copy_sd_to_littlefs(fs::File& sourceFile, 
                                                const String& littlefsPath) {
            
            Serial.println(F("[io_utils] Copying file from SD card to LittleFS for shared SPI compatibility"));
            
            // Initialize LittleFS
            if (!photo_frame::spi_manager::SPIManager::init_littlefs()) {
                Serial.println(F("[io_utils] Failed to initialize LittleFS"));
                return error_type::LittleFSInitFailed;
            }
            
            // Open LittleFS file for writing
            fs::File littlefsFile = LittleFS.open(littlefsPath.c_str(), FILE_WRITE);
            if (!littlefsFile) {
                Serial.print(F("[io_utils] Failed to create LittleFS temp file: "));
                Serial.println(littlefsPath);
                return error_type::LittleFSFileCreateFailed;
            }
            
            // Copy data from SD card file to LittleFS
            uint8_t buffer[1024];
            size_t totalCopied = 0;
            
            while (sourceFile.available()) {
                size_t bytesRead = sourceFile.read(buffer, sizeof(buffer));
                if (bytesRead > 0) {
                    littlefsFile.write(buffer, bytesRead);
                    totalCopied += bytesRead;
                }
                
                // Yield every 4KB to prevent watchdog timeout
                if (totalCopied % 4096 == 0) {
                    yield();
                }
            }
            
            // Flush and close the LittleFS file
            littlefsFile.flush();
            littlefsFile.close();
            
            Serial.print(F("[io_utils] Copied "));
            Serial.print(totalCopied);
            Serial.println(F(" bytes to LittleFS"));
            
            Serial.println(F("[io_utils] File successfully copied to LittleFS"));
            return error_type::None;
        }
    }
}