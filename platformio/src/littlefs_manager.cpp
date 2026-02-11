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

#include "littlefs_manager.h"
#include "config.h"
#include "renderer.h"

namespace photo_frame {
namespace littlefs_manager {

// Static singleton instance
static LittleFsManager *_instance = nullptr;
static bool initialized = false;

LittleFsManager &LittleFsManager::getInstance() {
  if (_instance == nullptr) {
    _instance = new LittleFsManager();
    log_i("LittleFsManager singleton created");
  }
  return *_instance;
}

LittleFsManager::~LittleFsManager() {
  log_d("LittleFsManager destructor");
  release();
}

bool LittleFsManager::init() {
  if (initialized) {
    return true;
  }

  // First try to mount without formatting
  if (!LittleFS.begin(false)) {
    log_w("LittleFS mount failed, attempting format...");

    // Force format and try again
    if (!LittleFS.begin(true)) {
      log_e("LittleFS format and initialization failed");
      return false;
    }
    log_i("LittleFS formatted and mounted successfully");
  } else {
    log_i("LittleFS mounted successfully");

    // Check for filesystem corruption by testing basic operations
    size_t totalBytes = LittleFS.totalBytes();
    if (totalBytes == 0) {
      log_w("LittleFS appears corrupted (totalBytes = 0), reformatting...");
      LittleFS.end();
      if (!LittleFS.begin(true)) {
        log_e("LittleFS reformat failed");
        return false;
      }
      log_i("LittleFS successfully reformatted");
    }
  }

  initialized = true;
  log_i("LittleFS initialized successfully");

  // Show detailed filesystem info for debugging LittleFS issues
  size_t totalBytes = LittleFS.totalBytes();
  size_t usedBytes = LittleFS.usedBytes();
  size_t freeBytes = totalBytes - usedBytes;

  log_i("LittleFS - Total: %zu bytes (%.2f MB)", totalBytes, totalBytes / 1024.0 / 1024.0);
  log_i("LittleFS - Used: %zu bytes (%.2f MB)", usedBytes, usedBytes / 1024.0 / 1024.0);
  log_i("LittleFS - Free: %zu bytes (%.2f MB)", freeBytes, freeBytes / 1024.0 / 1024.0);

  // Check if there's enough space for a 384KB image file
  const size_t IMAGE_SIZE = DISP_WIDTH * DISP_HEIGHT * 3; // 384KB for 800x480 RGB
  if (freeBytes < IMAGE_SIZE) {
    log_w("WARNING: Only %zu bytes free, need %zu for image file", freeBytes, IMAGE_SIZE);
  } else {
    log_i("Sufficient space: %zu bytes available for %zu byte image", freeBytes, IMAGE_SIZE);
  }

  return true;
}

void LittleFsManager::list_files(const char *path) {
  if (!initialized) {
    log_e("LittleFS not initialized, cannot list files");
    return;
  }

  File dir = LittleFS.open(path);
  if (!dir || !dir.isDirectory()) {
    log_e("Failed to open directory: %s", path);
    return;
  }

  log_i("Listing files in LittleFS directory: %s", path);
  File file = dir.openNextFile();
  while (file) {
    String filename = file.name();
    size_t filesize = file.size();
    log_i(" - %s (%zu bytes)", filename.c_str(), filesize);
    file = dir.openNextFile();
  }
  dir.close();
}

File LittleFsManager::open_file(const char *filename, const char *mode) {
  if (!initialized) {
    log_e("LittleFS not initialized, cannot open file");
    return File();
  }

  File file = LittleFS.open(filename, mode);
  if (!file) {
    log_e("Failed to open file: %s", filename);
  }
  return file;
}

bool LittleFsManager::delete_file(const char *filename) {
  if (!initialized) {
    log_e("LittleFS not initialized, cannot delete file");
    return false;
  }

  if (!LittleFS.exists(filename)) {
    log_w("File does not exist, cannot delete: %s", filename);
    return false;
  }

  if (LittleFS.remove(filename)) {
    log_i("Successfully deleted file: %s", filename);
    return true;
  } else {
    log_e("Failed to delete file: %s", filename);
    return false;
  }
}

void LittleFsManager::release() {
  if (initialized) {
    LittleFS.end();
    initialized = false;
    log_i("LittleFS resources released");
  }
}

void LittleFsManager::cleanup_temp_files() {
  if (!initialized) {
    return;
  }

  File root = LittleFS.open("/");
  if (!root || !root.isDirectory()) {
    log_e("Failed to open LittleFS root directory");
    return;
  }

  int files_deleted = 0;
  File file = root.openNextFile();
  while (file) {
    String filename = file.name();

    // Simple pattern matching for .tmp files
    if (filename.endsWith(".tmp")) {
      String filepath = "/" + filename;
      file.close();

      if (LittleFS.remove(filepath.c_str())) {
        log_i("Deleted temp file: %s", filepath.c_str());
        files_deleted++;
      } else {
        log_w("Failed to delete: %s", filepath.c_str());
      }

      // Reopen directory after deletion
      root.close();
      root = LittleFS.open("/");
      file = root.openNextFile();
    } else {
      file.close();
      file = root.openNextFile();
    }
  }

  root.close();

  if (files_deleted > 0) {
    log_i("Cleaned up %d temporary files from LittleFS", files_deleted);
  }
}

size_t LittleFsManager::read_file(const char *filename, uint8_t *buffer, size_t buffer_size) {
  if (!initialized) {
    log_e("LittleFS not initialized, cannot read file");
    return 0;
  }

  if (!buffer || buffer_size == 0) {
    log_e("Invalid buffer or buffer size");
    return 0;
  }

  File file = LittleFS.open(filename, FILE_READ);
  if (!file) {
    log_e("Failed to open file for reading: %s", filename);
    return 0;
  }

  size_t bytes_read = file.readBytes((char *)buffer, buffer_size);
  file.close();

  if (bytes_read > 0) {
    log_i("Read %zu bytes from %s", bytes_read, filename);
  } else {
    log_w("Failed to read from file: %s", filename);
  }

  return bytes_read;
}

bool LittleFsManager::write_file(const char *filename, const uint8_t *buffer, size_t buffer_size) {
  if (!initialized) {
    log_e("LittleFS not initialized, cannot write file");
    return false;
  }

  if (!buffer || buffer_size == 0) {
    log_e("Invalid buffer or buffer size");
    return false;
  }

  File file = LittleFS.open(filename, FILE_WRITE, true);
  if (!file) {
    log_e("Failed to open file for writing: %s", filename);
    return false;
  }

  size_t bytes_written = file.write(buffer, buffer_size);
  file.flush();
  file.close();

  if (bytes_written != buffer_size) {
    log_e("Write failed: expected %zu bytes, wrote %zu bytes", buffer_size, bytes_written);
    LittleFS.remove(filename);
    return false;
  }

  log_i("Successfully wrote %zu bytes to %s", bytes_written, filename);
  return true;
}

size_t LittleFsManager::get_file_size(const char *filename) {
  if (!initialized) {
    log_e("LittleFS not initialized");
    return 0;
  }

  File file = LittleFS.open(filename, FILE_READ);
  if (!file) {
    log_w("File not found: %s", filename);
    return 0;
  }

  size_t size = file.size();
  file.close();

  return size;
}

bool LittleFsManager::file_exists(const char *filename) {
  if (!initialized) {
    log_e("LittleFS not initialized");
    return false;
  }

  return LittleFS.exists(filename);
}

} // namespace littlefs_manager
} // namespace photo_frame