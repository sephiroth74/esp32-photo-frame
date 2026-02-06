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

#include "errors.h"
#include "pfr1_config.h"
#include "types.h"
#include <Arduino.h>
#include <FS.h>
#include <memory>

namespace photo_frame {
namespace binary_utils {

// Calculate CRC32 for PFR1 validation
uint32_t calculateCRC32(const uint8_t* data, size_t length);

// Calculate CRC32 from a file stream without loading the whole buffer
uint32_t calculateCRC32Stream(fs::File& file, size_t start, size_t length);

// Parse and validate PFR1 header from buffer
bool parsePFR1Header(const uint8_t* buffer, size_t buffer_size, PFR1Header& header);

// Validate PFR1 payload CRC
bool validatePFR1PayloadCRC(const uint8_t* payload, size_t payload_len, uint32_t expected_crc32);

// Validate PFR1 file structure without loading entire payload in memory
photo_frame_error validatePFR1FileStructure(fs::File& file, bool validate_payload_crc = true);

/**
 * @brief Validate PFR1BinaryFile wrapper (header + payload CRC)
 *
 * Parses header and validates both header CRC and payload CRC.
 * Verifies dimensions match those used during wrapper construction.
 * Sets is_validated flag on success.
 *
 * @param wrapper Wrapper to validate (buffer must be pre-populated)
 * @return photo_frame_error - None if valid, appropriate error otherwise
 */
photo_frame_error validatePFR1Wrapper(PFR1BinaryFile& wrapper);

/**
 * @brief Load and validate PFR1 file from filesystem into wrapper
 *
 * Reads entire file into wrapper buffer and validates.
 * Verifies dimensions match those used during wrapper construction.
 * Sets is_validated flag on success.
 *
 * @param file Open file to read from
 * @param wrapper Wrapper to populate (must be pre-allocated with correct dimensions)
 * @return photo_frame_error - None if valid, appropriate error otherwise
 */
photo_frame_error validatePFR1File(fs::File& file, PFR1BinaryFile& wrapper);

} // namespace binary_utils
} // namespace photo_frame
