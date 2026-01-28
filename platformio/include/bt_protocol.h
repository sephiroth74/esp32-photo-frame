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

#pragma once

#include <Arduino.h>
#include "pfr1_config.h"

// BT Protocol constants (must be outside namespace for use in macros)
// Protocol version
#define BT_PROTOCOL_VERSION 0x01

// Magic number for validation
#define BT_CONFIG_MAGIC 0xBEEF

// Display rotation constants (match DisplayManager rotation values)
#define BT_ROTATION_0 0 // 0° - Landscape normal
#define BT_ROTATION_90 1 // 90° CW - Portrait
#define BT_ROTATION_180 2 // 180° - Landscape inverted
#define BT_ROTATION_270 3 // 270° CW - Portrait inverted

// Maximum image size (use PFR1 config for consistency with binary_utils)
// Allows flexibility if display dimensions change in the future
#define BT_MAX_IMAGE_SIZE PFR1_MAX_IMAGE_SIZE_ABSOLUTE

// Transfer chunk size - increased to 2048 for faster transfers
// Max payload in BLE is 247 bytes per notification, but buffering allows larger chunks
#define BT_CHUNK_SIZE 247

// Timeouts
#define BT_CHUNK_TIMEOUT_MS 5000 // 5 seconds per chunk
#define BT_CONNECTION_TIMEOUT_MS 300000 // 5 minutes waiting for connection

namespace photo_frame {
namespace bt_protocol {

    // String constants for BLE UUIDs and device name
    constexpr const char* BT_SERVICE_UUID = "0000180a-0000-1000-8000-00805f9b34fb";
    constexpr const char* BT_CHAR_CONFIG_UUID = "00002a29-0000-1000-8000-00805f9b34fb";
    constexpr const char* BT_CHAR_IMAGE_DATA_UUID = "00002a2a-0000-1000-8000-00805f9b34fb";
    constexpr const char* BT_CHAR_STATUS_UUID = "00002a2b-0000-1000-8000-00805f9b34fb";
    constexpr const char* BT_CHAR_DEVICE_INFO_UUID = "00002a2c-0000-1000-8000-00805f9b34fb";
    constexpr const char* BT_DEVICE_NAME = "ESP32-PhotoFrame";
    constexpr uint16_t BT_MANUFACTURER_ID = 0x1337; // Custom manufacturer identifier
    constexpr uint8_t BT_MANUFACTURER_MAGIC[4] = { 'P', 'F', 'R', '1' }; // PhotoFrame Rev1 tag

    /**
     * @brief Configuration data sent at the start of transfer
     *
     * This structure is sent by the client to configure the display
     * before sending the image data.
     */
    struct BTImageConfig {
        uint16_t magic; // Must be BT_CONFIG_MAGIC (0xBEEF)
        uint8_t version; // Protocol version (BT_PROTOCOL_VERSION)
        uint8_t rotation; // Display rotation: 0=0°, 1=90°, 2=180°, 3=270°
        uint16_t width; // Display width (for validation)
        uint16_t height; // Display height (for validation)
        uint32_t timestamp; // Unix timestamp (seconds since epoch)
        uint32_t image_size; // Image size in bytes
        uint16_t crc16; // CRC16 of the config structure (excluding this field)
    } __attribute__((packed));

    /**
     * @brief Device configuration sent to client
     *
     * This structure is sent by the device to the client when connecting,
     * allowing the client to know the display capabilities and current settings.
     * The mtu_size field tells the client the recommended chunk size for image transfer.
     */
    struct BTDeviceConfig {
        uint8_t version; // Protocol version (BT_PROTOCOL_VERSION)
        uint8_t display_type; // Display type: 0=B/W, 1=6-color
        uint16_t width; // Display width in pixels
        uint16_t height; // Display height in pixels
        uint8_t current_rotation; // Current display rotation: 0=0°, 1=90°, 2=180°, 3=270°
        uint16_t mtu_size; // Recommended chunk/MTU size for transfer (e.g., 2048 bytes)
    } __attribute__((packed));

    /**
     * @brief Image data chunk header
     *
     * Each chunk of image data is prefixed with this header.
     */
    struct BTImageChunk {
        uint16_t sequence; // Chunk sequence number (0-based)
        uint16_t data_size; // Size of data in this chunk (≤ BT_CHUNK_SIZE - header)
        uint16_t crc16; // CRC16 of the data in this chunk
    } __attribute__((packed));

    /**
     * @brief Transfer status codes
     */
    enum BTTransferStatus : uint8_t {
        BT_STATUS_IDLE = 0, // No transfer in progress
        BT_STATUS_WAITING_CONFIG = 1, // Waiting for configuration
        BT_STATUS_CONFIG_RECEIVED = 2, // Configuration received and validated
        BT_STATUS_RECEIVING_IMAGE = 3, // Receiving image data
        BT_STATUS_COMPLETE = 4, // Transfer complete and validated
        BT_STATUS_ERROR = 5 // Error occurred
    };

    /**
     * @brief Error codes for BLE communication
     */
    enum BTErrorCode : uint8_t {
        BT_ERROR_NONE = 0,
        BT_ERROR_INVALID_CONFIG = 1,
        BT_ERROR_INVALID_ROTATION = 2,
        BT_ERROR_IMAGE_TOO_LARGE = 3,
        BT_ERROR_CHUNK_TIMEOUT = 4,
        BT_ERROR_CHUNK_CRC_FAILED = 5,
        BT_ERROR_SD_WRITE_FAILED = 6,
        BT_ERROR_INVALID_CHUNK_SEQUENCE = 7,
        BT_ERROR_TRANSFER_TIMEOUT = 8
    };

    /**
     * @brief Status response structure
     *
     * Sent to the client to indicate current status.
     */
    struct BTStatusResponse {
        BTTransferStatus status;
        BTErrorCode error_code;
        uint16_t chunks_received; // Number of chunks successfully received
        uint32_t bytes_received; // Total bytes received
    } __attribute__((packed));

    /**
     * @brief Calculate CRC16 for data validation
     *
     * @param data Pointer to data
     * @param length Length of data in bytes
     * @return CRC16 value
     */
    uint16_t calculateCRC16(const uint8_t* data, size_t length);

    /**
     * @brief Validate BTImageConfig structure
     *
     * @param config Configuration to validate
     * @return true if valid, false otherwise
     */
    bool validateConfig(const BTImageConfig& config);

    /**
     * @brief Get rotation name as string
     *
     * @param rotation Rotation value (0-3)
     * @return String description of rotation
     */
    const char* getRotationName(uint8_t rotation);

    // PFR1 header and validation moved to binary_utils.h (global)

} // namespace bt_protocol
} // namespace photo_frame

#endif // ENABLE_BT_IMAGE