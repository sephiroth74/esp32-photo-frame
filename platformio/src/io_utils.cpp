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

#include "io_utils.h"
#include "renderer.h"
#include "spi_manager.h"

namespace photo_frame {
namespace io_utils {

photo_frame_error_t validate_image_file(fs::File& sourceFile,
                                        const char* filename,
                                        int expectedWidth,
                                        int expectedHeight) {
    Serial.print(F("[io_utils] Comprehensive image validation: "));
    Serial.println(filename);

    auto startTime = millis();

    // Basic file validation
    if (!sourceFile) {
        Serial.println(F("[io_utils] File is not open or invalid"));
        return error_type::FileOpenFailed;
    }

    // Check file size - ensure it's not zero
    size_t fileSize = sourceFile.size();
    if (fileSize == 0) {
        Serial.println(F("[io_utils] File is empty"));
        return error_type::ImageFileEmpty;
    }

    // Validate file size against reasonable bounds
    const size_t MIN_IMAGE_SIZE = 100;     // Minimum 100 bytes
    const size_t MAX_IMAGE_SIZE = 2097152; // Maximum 2MB per image file

    auto fileSizeError =
        error_utils::validate_image_file_size(fileSize, MIN_IMAGE_SIZE, MAX_IMAGE_SIZE, filename);
    if (fileSizeError != error_type::None) {
        Serial.print(F("[io_utils] File size validation failed: "));
        Serial.println(fileSizeError.message);
        return fileSizeError;
    }

    // Determine file type based on extension or header
    String fileName   = String(filename);
    bool isBinaryFile = is_binary_format(filename);
    bool isBmpFile    = is_bmp_format(filename);

    if (isBinaryFile) {
        Serial.println(F("[io_utils] Performing comprehensive BINARY file validation"));

        if(expectedWidth <= 0 || expectedHeight <= 0) {
            Serial.println(F("[io_utils] Expected dimensions not provided for binary validation"));
            return error_type::ImageDimensionsNotProvided;
        }

        // Check if file size matches expected binary size exactly
        size_t expectedBinarySize = (size_t)expectedWidth * expectedHeight;
        Serial.printf("[io_utils] Expected binary size: %dx%d = %zu bytes\n",
                      expectedWidth,
                      expectedHeight,
                      expectedBinarySize);
        Serial.printf("[io_utils] Actual file size: %zu bytes\n", fileSize);

        if (fileSize != expectedBinarySize) {
            Serial.printf(
                "[io_utils] ❌ EXACT size mismatch! Expected: %zu bytes, Got: %zu bytes\n",
                expectedBinarySize,
                fileSize);

            return error_type::ImageFileTruncated;
        }

        Serial.println("[io_utils] ✅ File size matches expected binary format exactly");

        // Full validation: read entire file like renderer::validate_binary_image_file
        sourceFile.seek(0);
        // uint32_t pixelsValidated = 0;
        // pixel by pixel validation
        // auto now = millis();
        // for (size_t i = 0; i < expectedBinarySize; i++) {
        //     int byteRead = sourceFile.read();
        //     if (byteRead < 0) {
        //         Serial.printf("[io_utils] Read error at position %zu\n", i);
        //         return error_type::ImageFileReadFailed;
        //     }
        //     // All 8-bit values are valid for our binary format
        //     pixelsValidated++;
        //     // Yield every 1000 pixels to prevent watchdog timeout
        //     if (pixelsValidated % 1000 == 0) {
        //         yield();
        //     }
        // }
        // auto validationTime = millis() - now;

        // Serial.print(F("[io_utils] ✅ Binary pixel data validated in "));
        // Serial.print(validationTime);
        // Serial.println(F(" ms"));
        // Serial.print(F("[io_utils] Binary validation: "));
        // Serial.print(pixelsValidated);
        // Serial.println(F(" pixels validated"));

    } else if (isBmpFile) {
        Serial.println(F("[io_utils] Performing BMP file validation"));

        // Read BMP header
        sourceFile.seek(0);
        uint8_t bmpHeader[54];
        if (sourceFile.read(bmpHeader, sizeof(bmpHeader)) < 54) {
            Serial.println(F("[io_utils] Cannot read BMP header"));
            return error_type::ImageFileReadFailed;
        }

        // Check BMP signature
        if (bmpHeader[0] != 'B' || bmpHeader[1] != 'M') {
            Serial.println(F("[io_utils] Invalid BMP signature"));
            return error_type::ImageFileHeaderInvalid;
        }

        // Extract BMP dimensions
        uint32_t bmpWidth  = *((uint32_t*)&bmpHeader[18]);
        uint32_t bmpHeight = *((uint32_t*)&bmpHeader[22]);

        Serial.printf("[io_utils] BMP dimensions: %ux%u\n", bmpWidth, bmpHeight);

        // Validate against expected dimensions if provided
        if (expectedWidth > 0 && expectedHeight > 0) {
            if (bmpWidth != expectedWidth || bmpHeight != expectedHeight) {
                Serial.printf("[io_utils] BMP dimension mismatch: expected %dx%d, got %ux%u\n",
                              expectedWidth,
                              expectedHeight,
                              bmpWidth,
                              bmpHeight);
                return error_type::ImageDimensionsMismatch;
            }
        }

        // Validate BMP file integrity by checking data offset
        uint32_t dataOffset = *((uint32_t*)&bmpHeader[10]);
        if (dataOffset >= fileSize) {
            Serial.println(F("[io_utils] BMP data offset beyond file size"));
            return error_type::ImageFileCorrupted;
        }

    } else {
        Serial.println(F("[io_utils] Unknown or unsupported file format"));
        return error_type::ImageFileHeaderInvalid;
    }

    // Reset file position for subsequent operations
    sourceFile.seek(0);

    auto elapsedTime = millis() - startTime;
    Serial.print(F("[io_utils] ✅ Validation completed in "));
    Serial.print(elapsedTime);
    Serial.println(F(" ms"));

    return error_type::None;
}

photo_frame_error_t copy_sd_to_littlefs(fs::File& sourceFile,
                                        const String& littlefsPath,
                                        int expectedWidth,
                                        int expectedHeight) {
    Serial.println(F("[io_utils] Copying validated file from SD card to LittleFS"));

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

// Backward-compatible version (assumes validation already done externally)
photo_frame_error_t copy_sd_to_littlefs(fs::File& sourceFile, const String& littlefsPath) {
    return copy_sd_to_littlefs(sourceFile, littlefsPath, 0, 0);
}

bool is_binary_format(const char* filename) {
    if (!filename) return false;

    const char* extension = strrchr(filename, '.');
    if (!extension) return false;

    return strcmp(extension, ".bin") == 0;
}

bool is_bmp_format(const char* filename) {
    if (!filename) return false;

    const char* extension = strrchr(filename, '.');
    if (!extension) return false;

    return strcmp(extension, ".bmp") == 0;
}
} // namespace io_utils
} // namespace photo_frame