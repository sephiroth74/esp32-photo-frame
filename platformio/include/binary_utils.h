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

#pragma once

#include <Arduino.h>
#include <memory>
#include <FS.h>
#include "pfr1_config.h"

namespace photo_frame {
namespace binary_utils {

    /**
     * PFR1 Binary Format Header
     * Magic: 'PFR1' (0x50465231 little-endian)
     * Total header size: 21 bytes
     */
    struct PFR1Header {
        uint32_t magic; // 'PFR1' (0x50465231 LE)
        uint8_t version; // Format version (currently 1)
        uint16_t header_len; // Header length in bytes (21)
        uint16_t width; // Image width
        uint16_t height; // Image height
        uint8_t rotation; // Display rotation (0-3)
        uint8_t color_mode; // 0=BW, 1=6C
        uint32_t payload_len; // Payload size (excludes header and CRC)
        uint32_t header_crc32; // CRC32 of header (bytes 0-16)
    } __attribute__((packed));

// PFR1 format constants (now imported from pfr1_config.h)
#define PFR1_MAGIC 0x50465231 // 'PFR1' in little-endian
#define PFR1_VERSION 1
    // PFR1_HEADER_SIZE and PFR1_CRC32_SIZE are defined in pfr1_config.h

    // Calculate CRC32 for PFR1 validation
    uint32_t calculateCRC32(const uint8_t* data, size_t length);

    // Parse and validate PFR1 header from buffer
    bool parsePFR1Header(const uint8_t* buffer, size_t buffer_size, PFR1Header& header);

    // Validate PFR1 payload CRC
    bool validatePFR1PayloadCRC(const uint8_t* payload, size_t payload_len, uint32_t expected_crc32);

    /**
     * @brief Wrapper class for PFR1 binary files with automatic PSRAM allocation
     *
     * Manages a complete PFR1 file: header + payload buffer.
     * Buffer is allocated on PSRAM with size = width * height.
     * Includes validation state tracking.
     */
    class PFR1BinaryFile {
    private:
        std::unique_ptr<uint8_t[]> buffer_;
        size_t buffer_size_;
        bool is_validated_;
        uint16_t width_;
        uint16_t height_;

    public:
        PFR1Header header;

        /**
         * Constructor: allocate PSRAM buffer for a PFR1 file
         * @param width Display width
         * @param height Display height
         */
        PFR1BinaryFile(uint16_t width, uint16_t height);

        // Deleted copy, allowed move
        PFR1BinaryFile(const PFR1BinaryFile&) = delete;
        PFR1BinaryFile& operator=(const PFR1BinaryFile&) = delete;
        PFR1BinaryFile(PFR1BinaryFile&&) = default;
        PFR1BinaryFile& operator=(PFR1BinaryFile&&) = default;

        /**
         * Get pointer to buffer (header + payload)
         */
        uint8_t* getBuffer() const { return buffer_.get(); }

        /**
         * Get buffer size (header + payload + crc)
         */
        size_t getBufferSize() const { return buffer_size_; }

        /**
         * Get pointer to payload (after header)
         */
        uint8_t* getPayload() const { return buffer_.get() + PFR1_HEADER_SIZE; }

        /**
         * Get display width
         */
        uint16_t getWidth() const { return width_; }

        /**
         * Get display height
         */
        uint16_t getHeight() const { return height_; }

        /**
         * Check if file has been validated
         */
        bool isValidated() const { return is_validated_; }

        /**
         * Mark as validated (internal use only)
         */
        void markValidated() { is_validated_ = true; }

        /**
         * Reset validation state
         */
        void resetValidation() { is_validated_ = false; }
    };

    /**
     * @brief Validate PFR1BinaryFile wrapper (header + payload CRC)
     *
     * Parses header and validates both header CRC and payload CRC.
     * Verifies dimensions match those used during wrapper construction.
     * Sets is_validated flag on success.
     *
     * @param wrapper Wrapper to validate (buffer must be pre-populated)
     * @return true if valid, false otherwise
     */
    bool validatePFR1Wrapper(PFR1BinaryFile& wrapper);

    /**
     * @brief Load and validate PFR1 file from filesystem into wrapper
     *
     * Reads entire file into wrapper buffer and validates.
     * Verifies dimensions match those used during wrapper construction.
     * Sets is_validated flag on success.
     *
     * @param file Open file to read from
     * @param wrapper Wrapper to populate (must be pre-allocated with correct dimensions)
     * @return true if valid, false otherwise
     */
    bool validatePFR1File(fs::File& file,
        PFR1BinaryFile& wrapper);

} // namespace binary_utils
} // namespace photo_frame
