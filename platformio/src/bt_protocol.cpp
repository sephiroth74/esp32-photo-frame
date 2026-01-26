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

#ifdef ENABLE_BT_IMAGE

#include "bt_protocol.h"

namespace photo_frame {
namespace bt_protocol {

    uint16_t calculateCRC16(const uint8_t* data, size_t length)
    {
        uint16_t crc = 0xFFFF;

        for (size_t i = 0; i < length; i++) {
            crc ^= data[i];
            for (uint8_t j = 0; j < 8; j++) {
                if (crc & 0x0001) {
                    crc = (crc >> 1) ^ 0xA001; // CRC-16/MODBUS polynomial
                } else {
                    crc >>= 1;
                }
            }
        }

        return crc;
    }

    bool validateConfig(const BTImageConfig& config)
    {
        // Check magic number
        if (config.magic != BT_CONFIG_MAGIC) {
            log_e("[BT] Invalid config magic: 0x%04X (expected 0x%04X)", config.magic, BT_CONFIG_MAGIC);
            return false;
        }

        // Check protocol version
        if (config.version != BT_PROTOCOL_VERSION) {
            log_w("[BT] Protocol version mismatch: %d (expected %d)",
                config.version,
                BT_PROTOCOL_VERSION);
            // Allow for backward compatibility - don't fail
        }

        // Validate rotation (0-3)
        if (config.rotation > 3) {
            log_e("[BT] Invalid rotation: %d (must be 0-3)", config.rotation);
            return false;
        }

        // Validate dimensions (0 means "use device defaults", otherwise validate range)
        if ((config.width != 0 || config.height != 0) && (config.width == 0 || config.height == 0 || config.width > 2000 || config.height > 2000)) {
            log_e("[BT] Invalid dimensions: %ux%u", config.width, config.height);
            return false;
        }

        // Validate image size
        if (config.image_size == 0 || config.image_size > BT_MAX_IMAGE_SIZE) {
            log_e("[BT] Invalid image size: %u (max: %u)", config.image_size, BT_MAX_IMAGE_SIZE);
            return false;
        }

        // Validate timestamp (basic sanity check - not in far future or past)
        uint32_t now = time(nullptr);
        if (config.timestamp > 0 && (config.timestamp > now + 86400 || config.timestamp < 946684800)) {
            // Allow up to 1 day in future, must be after year 2000
            log_w("[BT] Suspicious timestamp: %u", config.timestamp);
            // Warning only, don't fail
        }

        // Validate CRC
        uint16_t calculated_crc = calculateCRC16((const uint8_t*)&config,
            sizeof(BTImageConfig) - sizeof(uint16_t)); // Exclude CRC field itself

        if (calculated_crc != config.crc16) {
            log_e(
                "[BT] CRC mismatch: calculated=0x%04X, received=0x%04X", calculated_crc, config.crc16);
            return false;
        }

        return true;
    }

    uint32_t calculateCRC32(const uint8_t* data, size_t length)
    {
        uint32_t crc = 0xFFFFFFFF;

        for (size_t i = 0; i < length; i++) {
            crc ^= data[i];
            for (uint8_t j = 0; j < 8; j++) {
                crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
            }
        }

        return ~crc;
    }

    bool parsePFR1Header(const uint8_t* buffer, size_t buffer_size, PFR1Header& header)
    {
        // Check minimum size
        if (buffer_size < PFR1_HEADER_SIZE) {
            log_e("[PFR1] Buffer too small: %u bytes (need %u)", buffer_size, PFR1_HEADER_SIZE);
            return false;
        }

        // Parse header fields (little-endian)
        header.magic = buffer[0] | (buffer[1] << 8) | (buffer[2] << 16) | (buffer[3] << 24);
        header.version = buffer[4];
        header.header_len = buffer[5] | (buffer[6] << 8);
        header.width = buffer[7] | (buffer[8] << 8);
        header.height = buffer[9] | (buffer[10] << 8);
        header.rotation = buffer[11];
        header.color_mode = buffer[12];
        header.payload_len = buffer[13] | (buffer[14] << 8) | (buffer[15] << 16) | (buffer[16] << 24);
        header.header_crc32 = buffer[17] | (buffer[18] << 8) | (buffer[19] << 16) | (buffer[20] << 24);

        // Validate magic
        if (header.magic != PFR1_MAGIC) {
            log_e("[PFR1] Invalid magic: 0x%08X (expected 0x%08X)", header.magic, PFR1_MAGIC);
            return false;
        }

        // Validate version
        if (header.version != PFR1_VERSION) {
            log_w("[PFR1] Version mismatch: %u (expected %u)", header.version, PFR1_VERSION);
            // Continue anyway - might be backward compatible
        }

        // Validate header length
        if (header.header_len != PFR1_HEADER_SIZE) {
            log_e("[PFR1] Invalid header length: %u (expected %u)", header.header_len, PFR1_HEADER_SIZE);
            return false;
        }

        // Validate rotation (0-3)
        if (header.rotation > 3) {
            log_e("[PFR1] Invalid rotation: %u (must be 0-3)", header.rotation);
            return false;
        }

        // Validate color mode (0=BW, 1=6C)
        if (header.color_mode > 1) {
            log_e("[PFR1] Invalid color mode: %u (must be 0 or 1)", header.color_mode);
            return false;
        }

        // Validate dimensions
        if (header.width == 0 || header.height == 0 || header.width > 2000 || header.height > 2000) {
            log_e("[PFR1] Invalid dimensions: %ux%u", header.width, header.height);
            return false;
        }

        // Validate header CRC32 (first 17 bytes: magic through payload_len)
        uint32_t calculated_crc = calculateCRC32(buffer, 17);
        if (calculated_crc != header.header_crc32) {
            log_e("[PFR1] Header CRC mismatch: calculated=0x%08X, received=0x%08X",
                calculated_crc, header.header_crc32);
            return false;
        }

        // Check if buffer contains full image (header + payload + payload_crc32)
        size_t expected_total = PFR1_HEADER_SIZE + header.payload_len + 4;
        if (buffer_size < expected_total) {
            log_e("[PFR1] Buffer incomplete: %u bytes (need %u)", buffer_size, expected_total);
            return false;
        }

        log_i("[PFR1] Header validated: %ux%u, rotation=%u, color_mode=%u, payload=%u bytes",
            header.width, header.height, header.rotation, header.color_mode, header.payload_len);

        return true;
    }

    bool validatePFR1PayloadCRC(const uint8_t* payload, size_t payload_len, uint32_t expected_crc32)
    {
        uint32_t calculated_crc = calculateCRC32(payload, payload_len);

        if (calculated_crc != expected_crc32) {
            log_e("[PFR1] Payload CRC mismatch: calculated=0x%08X, expected=0x%08X",
                calculated_crc, expected_crc32);
            return false;
        }

        log_i("[PFR1] Payload CRC validated");
        return true;
    }

    const char* getRotationName(uint8_t rotation)
    {
        switch (rotation) {
        case BT_ROTATION_0:
            return "0° (Landscape)";
        case BT_ROTATION_90:
            return "90° (Portrait)";
        case BT_ROTATION_180:
            return "180° (Landscape Inverted)";
        case BT_ROTATION_270:
            return "270° (Portrait Inverted)";
        default:
            return "Invalid";
        }
    }

} // namespace bt_protocol
} // namespace photo_frame

#endif // ENABLE_BT_IMAGE