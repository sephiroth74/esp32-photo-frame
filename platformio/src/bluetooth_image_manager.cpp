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

#include "bluetooth_image_manager.h"
#include "battery.h"
#include "bt_protocol.h"
#include "bt_utils.h"
#include "config.h"
#include "display_manager.h"
#include "esp_task_wdt.h"
#include "io_utils.h"
#include "preferences_helper.h"
#include "rgb_status.h"
#include "sd_card.h"
#include <BLE2902.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>

namespace photo_frame {

// Global pointer to manager for BLE callbacks
static BluetoothImageManager* g_bt_manager = nullptr;

/**
 * @brief BLE Server Callbacks
 */
class BTServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer, esp_ble_gatts_cb_param_t* param) override
    {
        if (g_bt_manager) {
            g_bt_manager->connected_ = true;
            g_bt_manager->connection_start_ms_ = millis();
            log_i("[BLE] Client connected, address: %s",
                BLEAddress(param->connect.remote_bda).toString().c_str());
            RGB_SET_STATE(BT_CONNECTED);

            // Send device configuration to client
            bt_protocol::BTDeviceConfig device_config = g_bt_manager->buildDeviceConfig();

            log_i("[BLE] Sending device config: display_type=%u, %ux%u, rotation=%u, version=%u, "
                  "mtu_size=%u",
                device_config.display_type,
                device_config.width,
                device_config.height,
                device_config.current_rotation,
                device_config.version,
                device_config.mtu_size);

            // Send device config via the device info characteristic
            if (g_bt_manager->char_device_info_) {
                g_bt_manager->char_device_info_->setValue((uint8_t*)&device_config,
                    sizeof(bt_protocol::BTDeviceConfig));
                g_bt_manager->char_device_info_->notify();
            }
        }
    }

    void onDisconnect(BLEServer* pServer, esp_ble_gatts_cb_param_t* param) override
    {
        if (g_bt_manager) {
            g_bt_manager->connected_ = false;
            log_w("[BLE] Client disconnected");
            if (!g_bt_manager->transfer_complete_) {
                g_bt_manager->last_error_ = error_type::BtClientDisconnected;
            }

            // Restart advertising so the device can be discovered again
            log_i("[BLE] Restarting advertising after disconnect");
            BLEDevice::startAdvertising();
        }
    }
};

/**
 * @brief Configuration Characteristic Callbacks
 */
class BTConfigCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* pCharacteristic) override
    {
        if (!g_bt_manager)
            return;

        std::string value = pCharacteristic->getValue();
        log_i("[BLE] Config data received: %u bytes", value.length());

        if (value.length() != sizeof(bt_protocol::BTImageConfig)) {
            log_e("[BLE] Invalid config size: %u (expected %u)",
                value.length(),
                sizeof(bt_protocol::BTImageConfig));
            g_bt_manager->last_error_ = error_type::BtInvalidConfig;
            return;
        }

        // Parse config
        memcpy(&g_bt_manager->config_, value.c_str(), sizeof(bt_protocol::BTImageConfig));

        // Validate config
        if (!bt_protocol::validateConfig(g_bt_manager->config_)) {
            log_e("[BLE] Config validation failed");
            g_bt_manager->last_error_ = error_type::BtInvalidConfig;
            return;
        }

        log_i("[BLE] Config validated: %ux%u, rotation=%u, size=%u bytes",
            g_bt_manager->config_.width,
            g_bt_manager->config_.height,
            g_bt_manager->config_.rotation,
            g_bt_manager->config_.image_size);

        // Validate rotation
        if (!BluetoothImageManager::validateOrientation(g_bt_manager->config_.rotation)) {
            log_e("[BLE] Invalid rotation: %u", g_bt_manager->config_.rotation);
            g_bt_manager->last_error_ = error_type::BtInvalidRotation;
            return;
        }

        // Check image size
        if (g_bt_manager->config_.image_size == 0 || g_bt_manager->config_.image_size > BT_MAX_IMAGE_SIZE) {
            log_e("[BLE] Invalid image size: %u", g_bt_manager->config_.image_size);
            g_bt_manager->last_error_ = error_type::BtImageTooLarge;
            return;
        }

        // Allocate buffer for image data
        log_i("[BLE] Allocating %u bytes for image buffer", g_bt_manager->config_.image_size);
        g_bt_manager->image_buffer_ = (uint8_t*)malloc(g_bt_manager->config_.image_size);
        if (!g_bt_manager->image_buffer_) {
            log_e("[BLE] Failed to allocate image buffer (%u bytes)",
                g_bt_manager->config_.image_size);
            g_bt_manager->last_error_ = error_type::ImageMemoryAllocationFailed;
            return;
        }
        g_bt_manager->image_buffer_size_ = g_bt_manager->config_.image_size;
        g_bt_manager->image_buffer_allocated_ = true;

        g_bt_manager->config_received_ = true;
        g_bt_manager->bytes_received_ = 0;
        g_bt_manager->chunks_received_ = 0;
        g_bt_manager->last_chunk_ms_ = millis();

        log_i("[BLE] Ready to receive image (%u bytes)", g_bt_manager->config_.image_size);
        RGB_SET_STATE(BT_RECEIVING);
    }
};

/**
 * @brief Device Info Characteristic Callbacks
 */
class BTDeviceInfoCallbacks : public BLECharacteristicCallbacks {
    void onRead(BLECharacteristic* pCharacteristic) override
    {
        if (!g_bt_manager)
            return;

        // Build device config dynamically
        bt_protocol::BTDeviceConfig device_config = g_bt_manager->buildDeviceConfig();

        log_i("[BLE] Device config requested: display_type=%u, %ux%u, rotation=%u, version=%u",
            device_config.display_type,
            device_config.width,
            device_config.height,
            device_config.current_rotation,
            device_config.version);

        // Update characteristic value
        pCharacteristic->setValue((uint8_t*)&device_config, sizeof(bt_protocol::BTDeviceConfig));
    }
};

/**
 * @brief Image Data Characteristic Callbacks
 */
class BTImageDataCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* pCharacteristic) override
    {
        // Reset watchdog to prevent timeout during BLE chunk processing
        esp_task_wdt_reset();

        if (!g_bt_manager || !g_bt_manager->config_received_) {
            log_w("[BLE] Image data received but config not ready");
            return;
        }

        if (!g_bt_manager->image_buffer_allocated_) {
            log_w("[BLE] Image buffer not allocated");
            return;
        }

        std::string value = pCharacteristic->getValue();
        g_bt_manager->last_chunk_ms_ = millis();

        // Copy received data to buffer
        size_t chunk_size = value.length();
        if (g_bt_manager->bytes_received_ + chunk_size > g_bt_manager->image_buffer_size_) {
            log_e("[BLE] Buffer overflow: trying to write %u bytes at offset %u (buffer size: %u)",
                chunk_size,
                g_bt_manager->bytes_received_,
                g_bt_manager->image_buffer_size_);
            g_bt_manager->last_error_ = error_type::ImageBufferOverflow;
            return;
        }

        // Copy chunk data to buffer
        memcpy(
            &g_bt_manager->image_buffer_[g_bt_manager->bytes_received_], value.c_str(), chunk_size);

        g_bt_manager->bytes_received_ += chunk_size;
        g_bt_manager->chunks_received_++;

        log_d("[BLE] Chunk %u: %u bytes (total: %u/%u)",
            g_bt_manager->chunks_received_,
            chunk_size,
            g_bt_manager->bytes_received_,
            g_bt_manager->config_.image_size);

        // Check if transfer is complete
        if (g_bt_manager->bytes_received_ >= g_bt_manager->config_.image_size) {
            log_i("[BLE] Image transfer complete - parsing PFR1 header");
            log_d("[BLE] Total bytes received: %u", g_bt_manager->bytes_received_);

            // Parse and validate PFR1 header
            bt_protocol::PFR1Header pfr1_header;
            if (!bt_protocol::parsePFR1Header(g_bt_manager->image_buffer_,
                    g_bt_manager->bytes_received_,
                    pfr1_header)) {
                log_e("[BLE] PFR1 header validation failed");
                g_bt_manager->last_error_ = error_type::BtInvalidConfig;
                g_bt_manager->transfer_complete_ = false;
                return;
            }

            // Extract payload CRC32 (last 4 bytes after payload)
            uint32_t payload_crc32 = g_bt_manager->image_buffer_[PFR1_HEADER_SIZE + pfr1_header.payload_len] | (g_bt_manager->image_buffer_[PFR1_HEADER_SIZE + pfr1_header.payload_len + 1] << 8) | (g_bt_manager->image_buffer_[PFR1_HEADER_SIZE + pfr1_header.payload_len + 2] << 16) | (g_bt_manager->image_buffer_[PFR1_HEADER_SIZE + pfr1_header.payload_len + 3] << 24);

            // Validate payload CRC
            if (!bt_protocol::validatePFR1PayloadCRC(
                    &g_bt_manager->image_buffer_[PFR1_HEADER_SIZE],
                    pfr1_header.payload_len,
                    payload_crc32)) {
                log_e("[BLE] PFR1 payload CRC validation failed");
                g_bt_manager->last_error_ = error_type::ImageSizeInvalid;
                g_bt_manager->transfer_complete_ = false;
                return;
            }

            // Store parsed header info for later use
            g_bt_manager->pfr1_width_ = pfr1_header.width;
            g_bt_manager->pfr1_height_ = pfr1_header.height;
            g_bt_manager->pfr1_rotation_ = pfr1_header.rotation;
            g_bt_manager->pfr1_color_mode_ = pfr1_header.color_mode;
            g_bt_manager->pfr1_payload_offset_ = PFR1_HEADER_SIZE;
            g_bt_manager->pfr1_payload_len_ = pfr1_header.payload_len;

            log_i("[BLE] ✓ PFR1 validation passed: %ux%u, rotation=%u, color_mode=%u, payload=%u bytes",
                pfr1_header.width, pfr1_header.height, pfr1_header.rotation,
                pfr1_header.color_mode, pfr1_header.payload_len);
            // Log rotation info - config vs header
            if (g_bt_manager->config_.rotation != pfr1_header.rotation) {
                log_w("[BLE] Rotation mismatch: config=%u, header=%u (config takes priority for rendering)",
                    g_bt_manager->config_.rotation, pfr1_header.rotation);
            } else {
                log_d("[BLE] Rotation matches: config=%u, header=%u",
                    g_bt_manager->config_.rotation, pfr1_header.rotation);
            }
            // Validate dimensions match expected (from header, not config)
            auto sizeError = photo_frame::io_utils::validate_image_size_exact(
                pfr1_header.payload_len,
                pfr1_header.width,
                pfr1_header.height);

            if (sizeError != photo_frame::error_type::None) {
                log_e("[BLE] Payload size validation failed: %s", sizeError.message);
                g_bt_manager->last_error_ = sizeError;
                g_bt_manager->transfer_complete_ = false;
                return;
            }

            log_i("[BLE] ✓ Transfer complete and validated!");
            g_bt_manager->transfer_complete_ = true;
        }
    }
};

// ============================================================================
// BluetoothImageManager Implementation
// ============================================================================

BluetoothImageManager::BluetoothImageManager()
    : server_(nullptr)
    , service_(nullptr)
    , char_config_(nullptr)
    , char_image_data_(nullptr)
    , char_status_(nullptr)
    , connected_(false)
    , config_received_(false)
    , transfer_complete_(false)
    , bytes_received_(0)
    , chunks_received_(0)
    , connection_start_ms_(0)
    , last_chunk_ms_(0)
    , image_buffer_(nullptr)
    , image_buffer_size_(0)
    , image_buffer_allocated_(false)
    , pfr1_width_(0)
    , pfr1_height_(0)
    , pfr1_rotation_(0)
    , pfr1_color_mode_(0)
    , pfr1_payload_offset_(0)
    , pfr1_payload_len_(0)
{
    log_d("[BT Manager] Constructor");
    last_error_ = error_type::None;
    memset(&config_, 0, sizeof(bt_protocol::BTImageConfig));

    // Set global pointer for BLE callbacks
    g_bt_manager = this;
}

BluetoothImageManager::~BluetoothImageManager()
{
    log_d("[BT Manager] Destructor");

    // Free image buffer if allocated
    if (image_buffer_allocated_ && image_buffer_) {
        log_d("[BT Manager] Freeing image buffer");
        free(image_buffer_);
        image_buffer_ = nullptr;
        image_buffer_allocated_ = false;
    }

    shutdown();
    g_bt_manager = nullptr;
}

photo_frame_error_t BluetoothImageManager::init(uint32_t timeout_ms)
{
    log_i("[BT Manager] Initializing Bluetooth manager");

    // Initialize BLE device
    const String deviceName = photo_frame::bt_utils::getBluetoothDeviceName();
    log_i("[BT Manager] Initializing BLE device with name: %s", deviceName.c_str());
    BLEDevice::init(deviceName.c_str());

    // Create BLE Server
    server_ = BLEDevice::createServer();
    if (!server_) {
        log_e("[BT Manager] Failed to create BLE server");
        return error_type::BtInitFailed;
    }

    server_->setCallbacks(new BTServerCallbacks());

    // Create BLE Service
    service_ = server_->createService(bt_protocol::BT_SERVICE_UUID);
    if (!service_) {
        log_e("[BT Manager] Failed to create BLE service");
        return error_type::BtInitFailed;
    }

    // Create Config Characteristic
    char_config_ = service_->createCharacteristic(bt_protocol::BT_CHAR_CONFIG_UUID,
        BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);

    if (!char_config_) {
        log_e("[BT Manager] Failed to create config characteristic");
        return error_type::BtInitFailed;
    }

    char_config_->setCallbacks(new BTConfigCallbacks());
    char_config_->addDescriptor(new BLE2902());

    // Create Image Data Characteristic
    char_image_data_ = service_->createCharacteristic(bt_protocol::BT_CHAR_IMAGE_DATA_UUID,
        BLECharacteristic::PROPERTY_WRITE);

    if (!char_image_data_) {
        log_e("[BT Manager] Failed to create image data characteristic");
        return error_type::BtInitFailed;
    }

    char_image_data_->setCallbacks(new BTImageDataCallbacks());

    // Create Status Characteristic
    char_status_ = service_->createCharacteristic(bt_protocol::BT_CHAR_STATUS_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);

    if (!char_status_) {
        log_e("[BT Manager] Failed to create status characteristic");
        return error_type::BtInitFailed;
    }

    char_status_->addDescriptor(new BLE2902());

    // Create Device Info Characteristic
    char_device_info_ = service_->createCharacteristic(bt_protocol::BT_CHAR_DEVICE_INFO_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);

    if (!char_device_info_) {
        log_e("[BT Manager] Failed to create device info characteristic");
        return error_type::BtInitFailed;
    }

    char_device_info_->addDescriptor(new BLE2902());
    char_device_info_->setCallbacks(new BTDeviceInfoCallbacks());

    // Start service
    service_->start();

    // Start advertising with manufacturer data tag to avoid false positives during scan
    uint8_t manufacturer_payload[2 + sizeof(bt_protocol::BT_MANUFACTURER_MAGIC)] = { 0 };
    manufacturer_payload[0] = bt_protocol::BT_MANUFACTURER_ID & 0xFF; // LSB first
    manufacturer_payload[1] = (bt_protocol::BT_MANUFACTURER_ID >> 8) & 0xFF; // MSB
    memcpy(&manufacturer_payload[2],
        bt_protocol::BT_MANUFACTURER_MAGIC,
        sizeof(bt_protocol::BT_MANUFACTURER_MAGIC));

    BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(bt_protocol::BT_SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMaxPreferred(0x12);

    // Set manufacturer data in the advertising packet
    std::string mfg_data((char*)manufacturer_payload, sizeof(manufacturer_payload));
    BLEAdvertisementData adv_data;
    adv_data.setManufacturerData(mfg_data);
    pAdvertising->setAdvertisementData(adv_data);

    BLEDevice::startAdvertising();
    log_i("[BT Manager] BLE advertising started");

    // we should print the mac address of the device
    log_i("[BT Manager] BLE device initialized with address: %s", BLEDevice::getAddress().toString().c_str());
    return error_type::None;
}

photo_frame_error_t BluetoothImageManager::waitForImage(uint32_t timeout_ms)
{
    log_i("[BT Manager] Waiting for image (timeout: %u ms)", timeout_ms);

    unsigned long start_time = millis();

    while (!transfer_complete_) {
        // // print a message every 10 seconds
        // if (timeout_ms > 0 && (millis() - start_time) % 10000 == 0) {
        //     log_i("[BT Manager] Still waiting for image...");
        // }

        // Check timeout
        if (timeout_ms > 0 && millis() - start_time >= timeout_ms) {
            log_w("[BT Manager] Timeout waiting for image");
            return error_type::BtConnectionTimeout;
        }

        // Check chunk timeout (30 seconds between chunks)
        if (config_received_ && millis() - last_chunk_ms_ > BT_CHUNK_TIMEOUT_MS) {
            log_w("[BT Manager] Chunk timeout - resetting current transfer");
            resetCurrentTransfer();
            // Continue waiting instead of returning error
            // This keeps the BLE window active for retry
            continue;
        }

        delay(100);
        yield();
    }

    // Transfer complete - image is ready in memory buffer
    log_i("[BT Manager] ✓ Transfer complete, image ready in memory");
    return error_type::None;
}

BluetoothImageManager::ErrorSeverity
BluetoothImageManager::getErrorSeverity(photo_frame_error_t error)
{
    // CRITICAL - codes 301-310, 354
    if ((error.code >= 301 && error.code <= 310) || error.code == 354) {
        return SEVERITY_CRITICAL;
    }

    // WARNING - codes 360-369
    if (error.code >= 360 && error.code <= 369) {
        return SEVERITY_WARNING;
    }

    // IGNORE - codes 350-359 (except 354)
    if (error.code >= 350 && error.code <= 359) {
        return SEVERITY_IGNORE;
    }

    // Default to CRITICAL for safety
    return SEVERITY_CRITICAL;
}

bool BluetoothImageManager::shouldDisplayError(photo_frame_error_t error)
{
    ErrorSeverity severity = getErrorSeverity(error);
    return severity == SEVERITY_CRITICAL || severity == SEVERITY_WARNING;
}

bool BluetoothImageManager::shouldUseFallback(photo_frame_error_t error)
{
    // Use fallback for timeouts, disconnections, warnings
    return error.code == 350 || // Timeout
        error.code == 351 || // Disconnected
        error.code == 360 || // Partial transfer
        error.code == 361; // Low battery
}

bool BluetoothImageManager::validateOrientation(uint8_t rotation) { return rotation <= 3; }

const char* BluetoothImageManager::getErrorSuggestion(photo_frame_error_t error)
{
    switch (error.code) {
    case 301:
        return "Check BT module hardware";
    case 302:
        return "Verify config data format";
    case 303:
        return "Reduce image file size";
    case 304:
        return "Use rotation 0-3 only";
    case 305:
        return "Retry transfer";
    case 306:
        return "Check SD card";
    case 307:
        return "Press RESET to initialize";
    case 308:
        return "Use correct .bin format";
    case 354:
        return "Restart device";
    default:
        return nullptr;
    }
}

void BluetoothImageManager::shutdown()
{
    log_i("[BT Manager] Shutting down Bluetooth");

    // Stop advertising
    BLEDevice::stopAdvertising();

    // Delete characteristics and service
    if (service_) {
        service_->stop();
    }

    // Deinitialize BLE
    BLEDevice::deinit();

    connected_ = false;
    config_received_ = false;
    transfer_complete_ = false;
}

void BluetoothImageManager::resetCurrentTransfer()
{
    log_i("[BT Manager] Resetting current transfer (keeping BLE connection active)");

    // Free image buffer if allocated
    if (image_buffer_) {
        free(image_buffer_);
        image_buffer_ = nullptr;
    }

    // Reset transfer state
    image_buffer_allocated_ = false;
    image_buffer_size_ = 0;
    bytes_received_ = 0;
    chunks_received_ = 0;
    config_received_ = false;
    transfer_complete_ = false;
    last_error_ = error_type::None;
    last_chunk_ms_ = 0;

    // Keep connected flag and connection_start_ms_ unchanged
    // This allows the BLE connection window to remain open
    log_i("[BT Manager] Transfer reset complete, ready for new transfer");
    RGB_SET_STATE(BT_CONNECTED); // Back to connected state
}

bt_protocol::BTDeviceConfig BluetoothImageManager::buildDeviceConfig() const
{
    bt_protocol::BTDeviceConfig device_config;
    device_config.version = BT_PROTOCOL_VERSION;

#ifdef DISP_6C
    device_config.display_type = 1; // 6-color display
#else
    device_config.display_type = 0; // B/W display
#endif

    device_config.width = DisplayManager::getNativeWidth();
    device_config.height = DisplayManager::getNativeHeight();
    device_config.current_rotation = device_orientation_; // Could be enhanced to read from preferences
    device_config.mtu_size = BT_CHUNK_SIZE; // Communicate optimal chunk size to client

    return device_config;
}

} // namespace photo_frame

#endif // ENABLE_BT_IMAGE
