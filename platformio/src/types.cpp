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

#include "types.h"
#include "pfr1_config.h"
#include <Arduino.h>

namespace photo_frame {

const String getImageSourceString(ImageSource source) {
  switch (source) {
  case IMAGE_SOURCE_CLOUD:
    return "Cloud";
  case IMAGE_SOURCE_LOCAL_CACHE:
    return "Local";
  case IMAGE_SOURCE_BLUETOOTH:
    return "Bluetooth";
  case IMAGE_SOURCE_WEBSOCKET:
    return "Upload";
  case IMAGE_SOURCE_NONE:
  default:
    return "";
  }
}

// PFR1Header Implementation
PFR1Header::PFR1Header()
    : magic(0), version(0), header_len(0), width(0), height(0), rotation(0), color_mode(0), payload_len(0), header_crc32(0) {}

void PFR1Header::reset() {
  magic = 0;
  version = 0;
  header_len = 0;
  width = 0;
  height = 0;
  rotation = 0;
  color_mode = 0;
  payload_len = 0;
  header_crc32 = 0;
}

bool PFR1Header::isValid() const { return (magic == PFR1_MAGIC) && (version == 1) && (header_len == PFR1_HEADER_SIZE); }

// PFR1BinaryFile Implementation
// ================================================================

PFR1BinaryFile::PFR1BinaryFile(uint16_t width, uint16_t height)
    : buffer_size_(PFR1_HEADER_SIZE + (width * height) + 4), is_validated_(false), width_(width), height_(height) {
  // Allocate buffer on PSRAM for complete PFR1 file:
  // Total size = Header (21) + Payload (width*height) + CRC32 (4)
  // via the PFR1_MAX_IMAGE_SIZE_FOR() macro in pfr1_config.h
  buffer_ = make_psram_unique(buffer_size_);

  if (!buffer_) {
    log_e("[PFR1] PSRAM allocation failed for %u x %u (total %u bytes)", width, height, buffer_size_);
    buffer_size_ = 0;
  }

  header.reset();
  log_d("[PFR1BinaryFile] Allocated %u bytes on PSRAM (%ux%u)", buffer_size_, width, height);
}

uint8_t *PFR1BinaryFile::getPayload() const { return buffer_.get() + PFR1_HEADER_SIZE; }

uint8_t PFR1BinaryFile::getRotation() { return header.getRotation(); }

void PFR1BinaryFile::resetValidation() { is_validated_ = false; }

void PFR1BinaryFile::markValidated() { is_validated_ = true; }

bool PFR1BinaryFile::isValidated() const { return is_validated_; }

uint16_t PFR1BinaryFile::getHeight() const { return height_; }

uint16_t PFR1BinaryFile::getWidth() const { return width_; }

size_t PFR1BinaryFile::getPayloadSize() const { return header.getPayloadLen(); }

size_t PFR1BinaryFile::getBufferSize() const { return buffer_size_; }

uint8_t *PFR1BinaryFile::getBuffer() const { return buffer_.get(); }

void PFR1BinaryFile::reset() {
  if (buffer_) {
    memset(buffer_.get(), 0, buffer_size_);
  }
  header.reset();
  is_validated_ = false;
}

} // namespace photo_frame