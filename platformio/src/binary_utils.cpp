// MIT License
// (see header for details)

#include "binary_utils.h"
#include "config.h"
#include "errors.h"

namespace photo_frame {
namespace binary_utils {

    uint32_t calculateCRC32(const uint8_t* data, size_t length)
    {
        // CRC32 (polynomial 0xEDB88320), standard implementation
        uint32_t crc = 0xFFFFFFFF;

        for (size_t i = 0; i < length; i++) {
            crc ^= data[i];
            for (uint8_t j = 0; j < 8; j++) {
                crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
            }
        }

        return ~crc;
    }

    uint32_t calculateCRC32Stream(fs::File& file, size_t start, size_t length)
    {
        if (!file) {
            return 0;
        }

        size_t original_pos = file.position();
        if (!file.seek(start)) {
            return 0;
        }

        uint32_t crc = 0xFFFFFFFF;
        const size_t chunk_size = 1024;
        uint8_t buffer[chunk_size];

        size_t remaining = length;
        while (remaining > 0) {
            size_t to_read = remaining > chunk_size ? chunk_size : remaining;
            size_t read_bytes = file.read(buffer, to_read);
            if (read_bytes == 0) {
                break;
            }

            for (size_t i = 0; i < read_bytes; i++) {
                crc ^= buffer[i];
                for (uint8_t j = 0; j < 8; j++) {
                    crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
                }
            }

            remaining -= read_bytes;
        }

        file.seek(original_pos);
        return ~crc;
    }

    bool parsePFR1Header(const uint8_t* buffer, size_t buffer_size, PFR1Header& header)
    {
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

        // Version (warn only)
        if (header.version != PFR1_VERSION) {
            log_w("[PFR1] Version mismatch: %u (expected %u)", header.version, PFR1_VERSION);
        }

        // Validate header length
        if (header.header_len != PFR1_HEADER_SIZE) {
            log_e(
                "[PFR1] Invalid header length: %u (expected %u)", header.header_len, PFR1_HEADER_SIZE);
            return false;
        }

        // Validate rotation and color_mode bounds
        if (header.rotation > 3) {
            log_e("[PFR1] Invalid rotation: %u (must be 0-3)", header.rotation);
            return false;
        }
        if (header.color_mode > 1) {
            log_e("[PFR1] Invalid color mode: %u (must be 0 or 1)", header.color_mode);
            return false;
        }

        // Validate dimensions sanity
        if (header.width == 0 || header.height == 0 || header.width > 2000 || header.height > 2000) {
            log_e("[PFR1] Invalid dimensions: %ux%u", header.width, header.height);
            return false;
        }

        // Validate header CRC32 (first 17 bytes: magic..payload_len)
        uint32_t calculated_crc = calculateCRC32(buffer, 17);
        if (calculated_crc != header.header_crc32) {
            log_e("[PFR1] Header CRC mismatch: calculated=0x%08X, received=0x%08X",
                calculated_crc,
                header.header_crc32);
            return false;
        }

        // Ensure buffer has header+payload+payload_crc32
        size_t expected_total = PFR1_HEADER_SIZE + header.payload_len + 4;
        if (buffer_size < expected_total) {
            log_e("[PFR1] Buffer incomplete: %u bytes (need %u)", buffer_size, expected_total);
            return false;
        }

        log_i("[PFR1] Header validated: %ux%u, rotation=%u, color_mode=%u, payload=%u bytes",
            header.width,
            header.height,
            header.rotation,
            header.color_mode,
            header.payload_len);
        return true;
    }

    bool validatePFR1PayloadCRC(const uint8_t* payload, size_t payload_len, uint32_t expected_crc32)
    {
        uint32_t calculated_crc = calculateCRC32(payload, payload_len);
        if (calculated_crc != expected_crc32) {
            log_e("[PFR1] Payload CRC mismatch: calculated=0x%08X, expected=0x%08X",
                calculated_crc,
                expected_crc32);
            return false;
        }
        log_i("[PFR1] Payload CRC validated");
        return true;
    }

    photo_frame_error validatePFR1FileStructure(fs::File& file, bool validate_payload_crc)
    {
        PFR1Header header;

        if (!file) {
            log_e("[PFR1] Invalid file object");
            return photo_frame::error_type::ImageFileReadFailed;
        }

        size_t file_size = file.size();
        if (file_size < PFR1_HEADER_SIZE + PFR1_CRC32_SIZE) {
            log_e("[PFR1] File too small: %u bytes", file_size);
            return photo_frame::error_type::ImageFileTruncated;
        }

        uint8_t header_buf[PFR1_HEADER_SIZE];
        size_t original_pos = file.position();
        file.seek(0);
        size_t read_bytes = file.read(header_buf, PFR1_HEADER_SIZE);
        if (read_bytes != PFR1_HEADER_SIZE) {
            log_e("[PFR1] Failed to read header");
            file.seek(original_pos);
            return photo_frame::error_type::ImageFileReadFailed;
        }

        if (!parsePFR1Header(header_buf, file_size, header)) {
            file.seek(original_pos);
            return photo_frame::error_type::ImageFileHeaderInvalid;
        }

        // Validate payload size vs width/height
        size_t expected_payload_size = static_cast<size_t>(header.width) * static_cast<size_t>(header.height);
        if (header.payload_len != expected_payload_size) {
            log_e("[PFR1] Payload size mismatch: header=%u bytes, expected=%u bytes",
                header.payload_len,
                expected_payload_size);
            file.seek(original_pos);
            return photo_frame::error_type::ImageDimensionsInvalid;
        }

        if (header.payload_len != EXPECTED_IMAGE_SIZE_BYTES) {
            log_e("[PFR1] Payload size does not match expected display size: header=%u bytes, expected=%u bytes",
                header.payload_len,
                EXPECTED_IMAGE_SIZE_BYTES);
            file.seek(original_pos);
            return photo_frame::error_type::ImageDimensionsInvalid;
        }

        size_t expected_total = PFR1_HEADER_SIZE + header.payload_len + PFR1_CRC32_SIZE;
        if (file_size != expected_total) {
            log_e("[PFR1] File size mismatch: actual=%u bytes, expected=%u bytes",
                file_size,
                expected_total);
            file.seek(original_pos);
            return photo_frame::error_type::ImageSizeInvalid;
        }

        if (validate_payload_crc) {
            // Read expected payload CRC at end of file
            uint8_t crc_buf[4];
            if (!file.seek(PFR1_HEADER_SIZE + header.payload_len)) {
                file.seek(original_pos);
                return photo_frame::error_type::ImageFileSeekFailed;
            }
            if (file.read(crc_buf, 4) != 4) {
                file.seek(original_pos);
                return photo_frame::error_type::ImageFileReadFailed;
            }
            uint32_t expected_crc = crc_buf[0] | (crc_buf[1] << 8) | (crc_buf[2] << 16) | (crc_buf[3] << 24);

            uint32_t calculated_crc = calculateCRC32Stream(file, PFR1_HEADER_SIZE, header.payload_len);
            if (calculated_crc != expected_crc) {
                log_e("[PFR1] Payload CRC mismatch: calculated=0x%08X, expected=0x%08X",
                    calculated_crc,
                    expected_crc);
                file.seek(original_pos);
#ifdef ENABLE_BT_IMAGE
                return photo_frame::error_type::BtImageValidationFailed;
#else
                return photo_frame::error_type::ImageFileCorrupted;
#endif
            }
        }

        file.seek(original_pos);
        log_i("[PFR1] File structure validated successfully");
        return photo_frame::error_type::None;
    }

    // ============================================================
    // PFR1BinaryFile Implementation
    // ============================================================

    PFR1BinaryFile::PFR1BinaryFile(uint16_t width, uint16_t height)
        : buffer_size_(PFR1_HEADER_SIZE + (width * height) + 4)
        , is_validated_(false)
        , width_(width)
        , height_(height)
    {
        // Allocate buffer on PSRAM for complete PFR1 file:
        // Total size = Header (21) + Payload (width*height) + CRC32 (4)
        // This size calculation is synchronized with BT_MAX_IMAGE_SIZE
        // via the PFR1_MAX_IMAGE_SIZE_FOR() macro in pfr1_config.h
        buffer_ = std::unique_ptr<uint8_t[]>(static_cast<uint8_t*>(ps_malloc(buffer_size_)));

        if (!buffer_) {
            log_e("[PFR1] PSRAM allocation failed for %u x %u (total %u bytes)",
                width,
                height,
                buffer_size_);
            buffer_size_ = 0;
        }

        memset(&header, 0, sizeof(header));
        log_d("[PFR1BinaryFile] Allocated %u bytes on PSRAM (%ux%u)", buffer_size_, width, height);
    }

    photo_frame_error validatePFR1Wrapper(PFR1BinaryFile& wrapper)
    {
        if (!wrapper.getBuffer() || wrapper.getBufferSize() == 0) {
            log_e("[PFR1] Wrapper has no buffer allocated");
            return photo_frame::error_type::ImageMemoryAllocationFailed;
        }

        // Parse and validate header
        if (!parsePFR1Header(wrapper.getBuffer(), wrapper.getBufferSize(), wrapper.header)) {
            log_e("[PFR1] Header parsing/validation failed");
            return photo_frame::error_type::ImageFileHeaderInvalid;
        }

        // Validate payload size against width * height
        size_t expected_payload_size = static_cast<size_t>(wrapper.header.width) * static_cast<size_t>(wrapper.header.height);
        if (wrapper.header.payload_len != expected_payload_size) {
            log_e("[PFR1] Payload size mismatch: header=%u bytes, expected=%u bytes",
                wrapper.header.payload_len,
                expected_payload_size);
            return photo_frame::error_type::ImageDimensionsInvalid;
        }

        if (wrapper.header.payload_len != EXPECTED_IMAGE_SIZE_BYTES) {
            log_e("[PFR1] Payload size does not match expected display size: header=%u bytes, "
                  "expected=%u bytes",
                wrapper.header.payload_len,
                EXPECTED_IMAGE_SIZE_BYTES);
            return photo_frame::error_type::ImageDimensionsInvalid;
        }

        // Validate payload CRC
        const uint8_t* payload = wrapper.getPayload();
        size_t payload_len = wrapper.header.payload_len;
        uint32_t payload_crc = *reinterpret_cast<const uint32_t*>(payload + payload_len);

        if (!validatePFR1PayloadCRC(payload, payload_len, payload_crc)) {
            log_e("[PFR1] Payload validation failed");
#ifdef ENABLE_BT_IMAGE
            return photo_frame::error_type::BtImageValidationFailed;
#else
            return photo_frame::error_type::ImageFileCorrupted;
#endif
        }

        wrapper.markValidated();
        log_i("[PFR1] Wrapper validated successfully");
        return photo_frame::error_type::None;
    }

    photo_frame_error validatePFR1File(fs::File& file, PFR1BinaryFile& wrapper)
    {
        if (!file) {
            log_e("[PFR1] Invalid file object");
            return photo_frame::error_type::SdCardFileOpenFailed;
        }

        // Read entire file into wrapper buffer
        size_t to_read = file.size();
        if (to_read == 0) {
            log_e("[PFR1] File is empty");
            return photo_frame::error_type::ImageFileEmpty;
        }

        if (to_read > wrapper.getBufferSize()) {
            log_e(
                "[PFR1] File too large: %u bytes (buffer: %u bytes)", to_read, wrapper.getBufferSize());
            return photo_frame::error_type::ImageFileTooLarge;
        }

        size_t bytes_read = file.read(wrapper.getBuffer(), to_read);
        if (bytes_read != to_read) {
            log_e("[PFR1] Read error: got %u bytes, expected %u", bytes_read, to_read);
            return photo_frame::error_type::ImageFileReadFailed;
        }

        // Validate wrapper (which includes payload CRC check)
        return validatePFR1Wrapper(wrapper);
    }

    void PFR1Header_reset(PFR1Header& header)
    {
        header.magic = 0;
        header.version = 0;
        header.header_len = 0;
        header.width = 0;
        header.height = 0;
        header.rotation = 0;
        header.color_mode = 0;
        header.payload_len = 0;
        header.header_crc32 = 0;
    }

    uint16_t PFR1Header_getWidth(const PFR1Header& header)
    {
        // width must be swapped with height if rotation is 1 or 3
        if (header.rotation == 1 || header.rotation == 3) {
            return header.height;
        }
        return header.width;
    }

    uint16_t PFR1Header_getHeight(const PFR1Header& header)
    {
        if (header.rotation == 1 || header.rotation == 3) {
            return header.width;
        }
        return header.height;
    }

} // namespace binary_utils
} // namespace photo_frame
