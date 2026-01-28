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

uint16_t calculateCRC16(const uint8_t* data, size_t length) {
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

bool validateConfig(const BTImageConfig& config) {
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
    if ((config.width != 0 || config.height != 0) &&
        (config.width == 0 || config.height == 0 || config.width > 2000 || config.height > 2000)) {
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
    uint16_t calculated_crc =
        calculateCRC16((const uint8_t*)&config,
                       sizeof(BTImageConfig) - sizeof(uint16_t)); // Exclude CRC field itself

    if (calculated_crc != config.crc16) {
        log_e(
            "[BT] CRC mismatch: calculated=0x%04X, received=0x%04X", calculated_crc, config.crc16);
        return false;
    }

    return true;
}

// PFR1 header/CRC32 moved to binary_utils

const char* getRotationName(uint8_t rotation) {
    switch (rotation) {
    case BT_ROTATION_0:   return "0° (Landscape)";
    case BT_ROTATION_90:  return "90° (Portrait)";
    case BT_ROTATION_180: return "180° (Landscape Inverted)";
    case BT_ROTATION_270: return "270° (Portrait Inverted)";
    default:              return "Invalid";
    }
}

} // namespace bt_protocol
} // namespace photo_frame

#endif // ENABLE_BT_IMAGE