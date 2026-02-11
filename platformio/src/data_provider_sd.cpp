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

#ifndef ENABLE_WEBSERVER_DATAPROVIDER

#include "data_provider_sd.h"
#include "binary_utils.h"
#include "board_util.h"
#include "config.h"
#include "image_buffer.h"
#include "preferences_helper.h"

namespace photo_frame {

SdCardDataProvider::SdCardDataProvider() {}

ImageLoadResult SdCardDataProvider::load_next_image(bool is_reset, SdCard &sd_card, const unified_config &config) {
  log_i("--------------------------------------");
  log_i(" - SD Card Mode - Multi Directory");
  log_i("--------------------------------------");

  // Ensure display is OFF for SD card operations
  photo_frame::board_utils::displayPowerOff();

  // Initialize SD card
  auto error = sd_card.begin();
  if (error != photo_frame::error_type::None) {
    log_e("Failed to initialize SD card for image source");
    return ImageLoadResult(error);
  }

#if DEBUG_MODE
  sd_card.printStats(); // Print SD card statistics
#endif

  if (config.sd_card.directories.empty()) {
    log_e("SD Card enabled but no directories configured");
    sd_card.end();
    return ImageLoadResult(photo_frame::error_type::InvalidConfigNoImageSource);
  }

  // Validate configured directories exist
  for (const auto &dir : config.sd_card.directories) {
    if (!sd_card.isDirectory(dir.c_str())) {
      log_e("Images directory does not exist: %s", dir.c_str());
      sd_card.end();
      return ImageLoadResult(photo_frame::error_type::NoImagesFound);
    }
  }

  bool rebuild_toc = is_reset || !sd_card.isMultiDirectoryTocValid(config.sd_card.directories, BINARY_FILE_EXTENSION);

  if (rebuild_toc) {
    if (is_reset) {
      log_i("Reset detected - rebuilding all SD card TOC files");
    } else {
      log_i("TOC mismatch - rebuilding all SD card TOC files");
    }

    // Clear existing TOC cache
    sd_card.cleanupDir(SD_TOC_BASE_PATH);
    sd_card.createDirectories(SD_TOC_BASE_PATH);

    photo_frame::photo_frame_error_t tocError;
    if (!sd_card.buildMultiDirectoryToc(config.sd_card.directories, BINARY_FILE_EXTENSION, &tocError)) {
      log_e("Failed to build SD card TOC cache: %s (code: %u)", tocError.message, tocError.code);
      sd_card.end();
      return ImageLoadResult(tocError);
    }
  } else {
    log_i("Using existing SD card TOC cache");
  }

  String selected_directory;
  String file_path;
  uint32_t total_files = 0;
  uint32_t image_index = 0;

  if (!sd_card.selectRandomImageFromDirectories(config.sd_card.directories, selected_directory, file_path, total_files, image_index,
                                                BINARY_FILE_EXTENSION)) {
    log_e("No %s files found in configured directories", BINARY_FILE_EXTENSION);
    sd_card.end();
    return ImageLoadResult(photo_frame::error_type::NoImagesFound);
  }

  // Store the original filename for display
  String original_filename;
  int lastSlash = file_path.lastIndexOf('/');
  if (lastSlash >= 0) {
    original_filename = file_path.substring(lastSlash + 1);
  } else {
    original_filename = file_path;
  }

  log_i("Selected image: %s", original_filename.c_str());

  // Open the file
  auto file = sd_card.open(file_path.c_str());
  if (!file) {
    log_e("Failed to open image file: %s", file_path.c_str());
    sd_card.end();
    return ImageLoadResult(photo_frame::error_type::SdCardFileOpenFailed);
  }

  // Validate the binary file
  log_i("Validating image file dimensions and size...");
  auto wrapper = std::make_unique<photo_frame::PFR1BinaryFile>(DISP_WIDTH, DISP_HEIGHT);
  auto validationError = photo_frame::binary_utils::validatePFR1File(file, *wrapper);
  file.close();

  if (validationError != photo_frame::error_type::None) {
    log_e("Image validation failed for: %s - %s", original_filename.c_str(), validationError.message);
    sd_card.end();
    return ImageLoadResult(validationError);
  }

  log_i("✓ Image validation PASSED");

  // Store last displayed index in preferences
  auto &prefs = photo_frame::PreferencesHelper::getInstance();
  prefs.setImageIndex(image_index);

  sd_card.end();

  return ImageLoadResult(std::move(wrapper), original_filename, image_index, total_files);
}

} // namespace photo_frame

#endif // ENABLE_WEBSERVER_DATAPROVIDER