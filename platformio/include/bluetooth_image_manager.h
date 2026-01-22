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
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <FS.h>

#include "bt_protocol.h"
#include "errors.h"

namespace photo_frame {

/**
 * @brief Bluetooth Image Manager
 *
 * Manages BLE communication for receiving images and configuration
 * in Bluetooth mode (ENABLE_BT_IMAGE).
 *
 * Features:
 * - BLE advertising and connection management
 * - Image and config reception with validation
 * - Timeout handling
 * - Error classification and recovery
 */
class BluetoothImageManager {
  public:
    /**
     * @brief Error severity levels for classification
     */
    enum ErrorSeverity {
        SEVERITY_IGNORE,  // Only log, continue operation
        SEVERITY_WARNING, // Log + show icon, use fallback
        SEVERITY_CRITICAL // Log + display full error
    };

    BluetoothImageManager();
    ~BluetoothImageManager();

    /**
     * @brief Initialize Bluetooth manager
     *
     * @param timeout_ms Timeout for waiting connection (0 = indefinite)
     * @return Error code
     */
    photo_frame_error_t init(uint32_t timeout_ms = BT_CONNECTION_TIMEOUT_MS);

    /**
     * @brief Start BLE advertising and wait for connection/data
     *
     * This is a blocking call that waits for a client to connect,
     * send configuration and image data.
     *
     * @param timeout_ms Maximum time to wait (0 = indefinite)
     * @return Error code
     */
    photo_frame_error_t waitForImage(uint32_t timeout_ms);

    /**
     * @brief Wait for image with periodic battery checks
     *
     * Blocking call that waits for image reception with periodic
     * battery level monitoring. Will abort transfer if battery becomes
     * critical and show appropriate error.
     *
     * @param battery_reader Battery reader instance for checks
     * @param timeout_ms Maximum time to wait
     * @param timeout_callback Optional callback on timeout (shows message on display)
     * @return Error code (BtLowBatterySkip if battery too low)
     */
    photo_frame_error_t waitForImageWithBatteryCheck(uint32_t timeout_ms,
                                                     bool (*timeout_callback)() = nullptr);

    /**
     * @brief Get received configuration
     *
     * @return Configuration structure (valid only after successful reception)
     */
    const bt_protocol::BTImageConfig& getConfig() const { return config_; }

    /**
     * @brief Build device configuration
     *
     * Builds the device configuration structure with current display settings,
     * display type, resolution, and protocol version.
     *
     * @return Device configuration structure
     */
    bt_protocol::BTDeviceConfig buildDeviceConfig() const;

    /**
     * @brief Get pointer to received image buffer
     *
     * @return Pointer to image buffer (nullptr if not available)
     */
    const uint8_t* getImageBuffer() const { return image_buffer_; }

    /**
     * @brief Get size of received image
     *
     * @return Size in bytes
     */
    uint32_t getImageSize() const { return bytes_received_; }

    /**
     * @brief Get last error
     *
     * @return Last error code
     */
    photo_frame_error_t getLastError() const { return last_error_; }

    /**
     * @brief Shutdown Bluetooth and cleanup
     */
    void shutdown();

    /**
     * @brief Get error severity classification
     *
     * @param error Error to classify
     * @return Severity level
     */
    static ErrorSeverity getErrorSeverity(photo_frame_error_t error);

    /**
     * @brief Check if error should be displayed on screen
     *
     * @param error Error to check
     * @return true if should display error
     */
    static bool shouldDisplayError(photo_frame_error_t error);

    /**
     * @brief Check if should use fallback image on error
     *
     * @param error Error to check
     * @return true if should use fallback
     */
    static bool shouldUseFallback(photo_frame_error_t error);

    /**
     * @brief Validate rotation value
     *
     * @param rotation Rotation value to validate (0-3)
     * @return true if valid
     */
    static bool validateRotation(uint8_t rotation);

    /**
     * @brief Get human-readable error suggestion
     *
     * @param error Error to get suggestion for
     * @return Suggestion string or nullptr
     */
    static const char* getErrorSuggestion(photo_frame_error_t error);

  private:
    BLEServer* server_;
    BLEService* service_;
    BLECharacteristic* char_config_;
    BLECharacteristic* char_image_data_;
    BLECharacteristic* char_status_;
    BLECharacteristic* char_device_info_;

    bt_protocol::BTImageConfig config_;
    photo_frame_error_t last_error_;

    bool connected_;
    bool config_received_;
    bool transfer_complete_;
    uint32_t bytes_received_;
    uint16_t chunks_received_;

    unsigned long connection_start_ms_;
    unsigned long last_chunk_ms_;

    // Image data buffer for accumulating received chunks
    uint8_t* image_buffer_;
    size_t image_buffer_size_;
    bool image_buffer_allocated_;

    /**
     * @brief Process received configuration
     *
     * @param data Configuration data
     * @param length Data length
     * @return Error code
     */
    photo_frame_error_t processConfig(const uint8_t* data, size_t length);

    /**
     * @brief Process received image data chunk
     *
     * @param data Chunk data
     * @param length Data length
     * @return Error code
     */
    photo_frame_error_t processImageChunk(const uint8_t* data, size_t length);

    /**
     * @brief Finalize transfer and validate complete image
     *
     * @return Error code
     */
    photo_frame_error_t finalizeTransfer();

    /**
     * @brief Send status response to client
     *
     * @param status Status code
     * @param error Error code
     */
    void sendStatus(bt_protocol::BTTransferStatus status, bt_protocol::BTErrorCode error);

    /**
     * @brief Check timeouts
     *
     * @param timeout_ms Maximum allowed time
     * @return true if timeout occurred
     */
    bool checkTimeout(uint32_t timeout_ms);

    // BLE callbacks (defined as friend classes)
    friend class BTServerCallbacks;
    friend class BTConfigCallbacks;
    friend class BTImageDataCallbacks;
};

} // namespace photo_frame

#endif // ENABLE_BT_IMAGE
