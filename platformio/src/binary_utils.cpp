// MIT License
// (see header for details)

#include "binary_utils.h"
#include "config.h"
#include "errors.h"

namespace photo_frame {
namespace binary_utils {

uint32_t calculateCRC32(const uint8_t *data, size_t length) {
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

uint32_t calculateCRC32Stream(fs::File &file, size_t start, size_t length) {
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

bool parsePFR1Header(const uint8_t *buffer, size_t buffer_size, PFR1Header &header) {
  if (buffer_size < PFR1_HEADER_SIZE) {
    log_e("[PFR1] Buffer too small: %u bytes (need %u)", buffer_size, PFR1_HEADER_SIZE);
    return false;
  }

  uint32_t magic = buffer[0] | (buffer[1] << 8) | (buffer[2] << 16) | (buffer[3] << 24);

  // Parse header fields (little-endian)
  header.setMagic(magic);
  header.setVersion(buffer[4]);
  header.setHeaderLen(buffer[5] | (buffer[6] << 8));
  header.setWidth(buffer[7] | (buffer[8] << 8));
  header.setHeight(buffer[9] | (buffer[10] << 8));
  header.setRotation(buffer[11]);
  header.setColorMode(buffer[12]);
  header.setPayloadLen(buffer[13] | (buffer[14] << 8) | (buffer[15] << 16) | (buffer[16] << 24));
  header.setHeaderCRC32(buffer[17] | (buffer[18] << 8) | (buffer[19] << 16) | (buffer[20] << 24));

  // Validate magic
  if (header.getMagic() != PFR1_MAGIC) {
    log_e("[PFR1] Invalid magic: 0x%08X (expected 0x%08X)", header.getMagic(), PFR1_MAGIC);
    return false;
  }

  // Version (warn only)
  if (header.getVersion() != PFR1_VERSION) {
    log_w("[PFR1] Version mismatch: %u (expected %u)", header.getVersion(), PFR1_VERSION);
  }

  // Validate header length
  if (header.getHeaderLen() != PFR1_HEADER_SIZE) {
    log_e("[PFR1] Invalid header length: %u (expected %u)", header.getHeaderLen(), PFR1_HEADER_SIZE);
    return false;
  }

  // Validate rotation and color_mode bounds
  if (header.getRotation() > 3) {
    log_e("[PFR1] Invalid rotation: %u (must be 0-3)", header.getRotation());
    return false;
  }
  if (header.getColorMode() > 1) {
    log_e("[PFR1] Invalid color mode: %u (must be 0 or 1)", header.getColorMode());
    return false;
  }

  // Validate dimensions sanity
  if (header.getWidth() == 0 || header.getHeight() == 0 || header.getWidth() > 2000 || header.getHeight() > 2000) {
    log_e("[PFR1] Invalid dimensions: %ux%u", header.getWidth(), header.getHeight());
    return false;
  }

  // Validate header CRC32 (first 17 bytes: magic..payload_len)
  uint32_t calculated_crc = calculateCRC32(buffer, 17);
  if (calculated_crc != header.getHeaderCRC32()) {
    log_e("[PFR1] Header CRC mismatch: calculated=0x%08X, received=0x%08X", calculated_crc, header.getHeaderCRC32());
    return false;
  }

  // Ensure buffer has header+payload+payload_crc32
  size_t expected_total = PFR1_HEADER_SIZE + header.getPayloadLen() + PFR1_CRC32_SIZE;
  if (buffer_size < expected_total) {
    log_e("[PFR1] Buffer incomplete: %u bytes (need %u)", buffer_size, expected_total);
    return false;
  }

  log_v("[PFR1] Header validated: %ux%u, rotation=%u, color_mode=%u, "
        "payload=%u bytes",
        header.getWidth(), header.getHeight(), header.getRotation(), header.getColorMode(), header.getPayloadLen());
  return true;
}

bool validatePFR1PayloadCRC(const uint8_t *payload, size_t payload_len, uint32_t expected_crc32) {
  uint32_t calculated_crc = calculateCRC32(payload, payload_len);
  if (calculated_crc != expected_crc32) {
    log_e("[PFR1] Payload CRC mismatch: calculated=0x%08X, expected=0x%08X", calculated_crc, expected_crc32);
    return false;
  }
  log_v("[PFR1] Payload CRC validated");
  return true;
}

photo_frame_error validatePFR1FileStructure(fs::File &file, bool validate_payload_crc) {
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
  size_t expected_payload_size = static_cast<size_t>(header.getWidth()) * static_cast<size_t>(header.getHeight());
  if (header.getPayloadLen() != expected_payload_size) {
    log_e("[PFR1] Payload size mismatch: header=%u bytes, expected=%u bytes", header.getPayloadLen(), expected_payload_size);
    file.seek(original_pos);
    return photo_frame::error_type::ImageDimensionsInvalid;
  }

  if (header.getPayloadLen() != EXPECTED_IMAGE_SIZE_BYTES) {
    log_e("[PFR1] Payload size does not match expected display size: header=%u "
          "bytes, "
          "expected=%u bytes",
          header.getPayloadLen(), EXPECTED_IMAGE_SIZE_BYTES);
    file.seek(original_pos);
    return photo_frame::error_type::ImageDimensionsInvalid;
  }

  size_t expected_total = PFR1_HEADER_SIZE + header.getPayloadLen() + PFR1_CRC32_SIZE;
  if (file_size != expected_total) {
    log_e("[PFR1] File size mismatch: actual=%u bytes, expected=%u bytes", file_size, expected_total);
    file.seek(original_pos);
    return photo_frame::error_type::ImageSizeInvalid;
  }

  if (validate_payload_crc) {
    // Read expected payload CRC at end of file
    uint8_t crc_buf[4];
    if (!file.seek(PFR1_HEADER_SIZE + header.getPayloadLen())) {
      file.seek(original_pos);
      return photo_frame::error_type::ImageFileSeekFailed;
    }
    if (file.read(crc_buf, 4) != 4) {
      file.seek(original_pos);
      return photo_frame::error_type::ImageFileReadFailed;
    }
    uint32_t expected_crc = crc_buf[0] | (crc_buf[1] << 8) | (crc_buf[2] << 16) | (crc_buf[3] << 24);

    uint32_t calculated_crc = calculateCRC32Stream(file, PFR1_HEADER_SIZE, header.getPayloadLen());
    if (calculated_crc != expected_crc) {
      log_e("[PFR1] Payload CRC mismatch: calculated=0x%08X, expected=0x%08X", calculated_crc, expected_crc);
      file.seek(original_pos);
      return photo_frame::error_type::ImageFileCorrupted;
    }
  }

  file.seek(original_pos);
  log_v("[PFR1] File structure validated successfully");
  return photo_frame::error_type::None;
}

// ============================================================
// PFR1BinaryFile Implementation
// ============================================================

photo_frame_error validatePFR1Wrapper(PFR1BinaryFile &wrapper) {
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
  size_t expected_payload_size = static_cast<size_t>(wrapper.header.getWidth()) * static_cast<size_t>(wrapper.header.getHeight());
  if (wrapper.header.getPayloadLen() != expected_payload_size) {
    log_e("[PFR1] Payload size mismatch: header=%u bytes, expected=%u bytes", wrapper.header.getPayloadLen(),
          expected_payload_size);
    return photo_frame::error_type::ImageDimensionsInvalid;
  }

  if (wrapper.header.getPayloadLen() != EXPECTED_IMAGE_SIZE_BYTES) {
    log_e("[PFR1] Payload size does not match expected display size: header=%u "
          "bytes, "
          "expected=%u bytes",
          wrapper.header.getPayloadLen(), EXPECTED_IMAGE_SIZE_BYTES);
    return photo_frame::error_type::ImageDimensionsInvalid;
  }

  // Validate payload CRC
  const uint8_t *payload = wrapper.getPayload();
  size_t payload_len = wrapper.header.getPayloadLen();
  uint32_t payload_crc = *reinterpret_cast<const uint32_t *>(payload + payload_len);

  if (!validatePFR1PayloadCRC(payload, payload_len, payload_crc)) {
    log_e("[PFR1] Payload validation failed");
    return photo_frame::error_type::ImageFileCorrupted;
  }

  wrapper.markValidated();
  log_v("[PFR1] Wrapper validated successfully");
  return photo_frame::error_type::None;
}

photo_frame_error validatePFR1File(fs::File &file, PFR1BinaryFile &wrapper) {
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
    log_e("[PFR1] File too large: %u bytes (buffer: %u bytes)", to_read, wrapper.getBufferSize());
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

} // namespace binary_utils
} // namespace photo_frame
