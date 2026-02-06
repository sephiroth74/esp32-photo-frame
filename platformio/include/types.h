#ifndef PHOTO_FRAME_TYPES_H
#define PHOTO_FRAME_TYPES_H

#include <Arduino.h>
#include <memory>

namespace photo_frame {
/**
 * @brief Image source enumeration for tracking where an image was loaded from.
 */
enum ImageSource {
    IMAGE_SOURCE_CLOUD,       ///< Image was downloaded from Google Drive
    IMAGE_SOURCE_LOCAL_CACHE, ///< Image was loaded from local SD card cache
    IMAGE_SOURCE_BLUETOOTH,   ///< Image was received via Bluetooth
    IMAGE_SOURCE_WEBSOCKET,   ///< Image was received via WebSocket
    IMAGE_SOURCE_NONE,
};

const String getImageSourceString(ImageSource source);

/**
 * PFR1 Binary Format Header
 * Magic: 'PFR1' (0x50465231 little-endian)
 * Total header size: 21 bytes
 */
class PFR1Header {
  private:
    uint32_t magic;        // 'PFR1' (0x50465231 LE)
    uint8_t version;       // Format version (currently 1)
    uint16_t header_len;   // Header length in bytes (21)
    uint16_t width;        // Image width
    uint16_t height;       // Image height
    uint8_t rotation;      // Display rotation (0-3)
    uint8_t color_mode;    // 0=BW, 1=6C
    uint32_t payload_len;  // Payload size (excludes header and CRC)
    uint32_t header_crc32; // CRC32 of header (bytes 0-16)

  public:
    PFR1Header();

    // Deleted copy, allowed move
    PFR1Header(const PFR1Header&)            = delete;
    PFR1Header& operator=(const PFR1Header&) = delete;
    PFR1Header(PFR1Header&&)                 = default;
    PFR1Header& operator=(PFR1Header&&)      = default;

    void reset();

    void setMagic(uint32_t val) { magic = val; };
    void setVersion(uint8_t val) { version = val; };
    void setWidth(uint16_t val) { width = val; };
    void setHeight(uint16_t val) { height = val; };
    void setPayloadLen(uint32_t val) { payload_len = val; };
    void setRotation(uint8_t val) { rotation = val; };
    void setHeaderCRC32(uint32_t val) { header_crc32 = val; };
    void setColorMode(uint8_t val) { color_mode = val; };
    void setHeaderLen(uint16_t val) { header_len = val; };

    uint8_t getVersion() const { return version; };

    uint8_t getMagic() const { return magic; };

    uint16_t getWidth() const {
        if (rotation == 1 || rotation == 3) {
            return height;
        }
        return width;
    };

    uint16_t getHeight() const {
        if (rotation == 1 || rotation == 3) {
            return width;
        }
        return height;
    };

    uint32_t getPayloadLen() const { return payload_len; };

    uint8_t getRotation() const { return rotation; };

    uint32_t getHeaderCRC32() const { return header_crc32; };

    uint8_t getColorMode() const { return color_mode; };

    uint16_t getHeaderLen() const { return header_len; };

    bool isValid() const;

} __attribute__((packed));

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
    PFR1BinaryFile(const PFR1BinaryFile&)            = delete;

    PFR1BinaryFile& operator=(const PFR1BinaryFile&) = delete;

    PFR1BinaryFile(PFR1BinaryFile&&)                 = default;

    PFR1BinaryFile& operator=(PFR1BinaryFile&&)      = default;

    /**
     * Get pointer to buffer (header + payload)
     */
    uint8_t* getBuffer() const;

    /**
     * Get buffer size (header + payload + crc)
     */
    size_t getBufferSize() const;

    /**
     * Get pointer to payload (after header)
     */
    uint8_t* getPayload() const;

    /**
     * Get payload size (excluding header)
     */
    size_t getPayloadSize() const;

    /**
     * Get display width
     */
    uint16_t getWidth() const;

    /**
     * Get display height
     */
    uint16_t getHeight() const;

    /**
     * Check if file has been validated
     */
    bool isValidated() const;

    /**
     * Mark as validated (internal use only)
     */
    void markValidated();

    /**
     * Reset validation state
     */
    void resetValidation();

    /**
     * Get rotation from header
     */
    uint8_t getRotation();

    /**
     * Reset entire wrapper (clear buffer and header)
     */
    void reset();
};

} // namespace photo_frame

#endif // PHOTO_FRAME_TYPES_H