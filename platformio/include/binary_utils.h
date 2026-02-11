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
uint32_t calculateCRC32(const uint8_t *data, size_t length);

// Calculate CRC32 from a file stream without loading the whole buffer
uint32_t calculateCRC32Stream(fs::File &file, size_t start, size_t length);

// Parse and validate PFR1 header from buffer
bool parsePFR1Header(const uint8_t *buffer, size_t buffer_size, PFR1Header &header);

// Validate PFR1 payload CRC
bool validatePFR1PayloadCRC(const uint8_t *payload, size_t payload_len, uint32_t expected_crc32);

// Validate PFR1 file structure without loading entire payload in memory
photo_frame_error validatePFR1FileStructure(fs::File &file, bool validate_payload_crc = true);

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
photo_frame_error validatePFR1Wrapper(PFR1BinaryFile &wrapper);

/**
 * @brief Load and validate PFR1 file from filesystem into wrapper
 *
 * Reads entire file into wrapper buffer and validates.
 * Verifies dimensions match those used during wrapper construction.
 * Sets is_validated flag on success.
 *
 * @param file Open file to read from
 * @param wrapper Wrapper to populate (must be pre-allocated with correct
 * dimensions)
 * @return photo_frame_error - None if valid, appropriate error otherwise
 */
photo_frame_error validatePFR1File(fs::File &file, PFR1BinaryFile &wrapper);

} // namespace binary_utils
} // namespace photo_frame
