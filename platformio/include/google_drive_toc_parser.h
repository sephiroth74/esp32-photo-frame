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

#pragma once

#include "config.h"
#include "errors.h"
#include "google_drive_client.h"
#include "sd_card.h"

namespace photo_frame {

/**
 * @brief Utility class for parsing Google Drive TOC (Table of Contents) files
 *
 * This class provides methods to efficiently read and parse TOC files stored on
 * SD card in plain text format. The TOC format is: Line 1: timestamp = <value>
 * Line 2: fileCount = <value>
 * Line 3+: id|name
 */
class GoogleDriveTocParser {
public:
  /**
   * @brief Constructor
   * @param sdCard Reference to the SD card instance
   * @param tocFilePath Path to the TOC file
   */
  GoogleDriveTocParser(SdCard &sdCard, const char *tocFilePath);

  /**
   * @brief Get the timestamp from the TOC file
   * @param error Pointer to error code (optional)
   * @return Timestamp value, or 0 if error
   */
  time_t get_timestamp(photo_frame_error_t *error = nullptr);

  /**
   * @brief Get the file count from the TOC file
   * @param error Pointer to error code (optional)
   * @return Number of files in the TOC, or 0 if error
   */
  size_t get_file_count(photo_frame_error_t *error = nullptr);

  /**
   * @brief Get a file entry by index
   * @param index Zero-based index of the file to retrieve
   * @param error Pointer to error code (optional)
   * @return GoogleDriveFile at the specified index, or empty file if error
   */
  GoogleDriveFile get_file_by_index(size_t index, photo_frame_error_t *error = nullptr);

  /**
   * @brief Find a file by name
   * @param filename Name of the file to search for
   * @param error Pointer to error code (optional)
   * @return GoogleDriveFile with the specified name, or empty file if not found
   */
  GoogleDriveFile get_file_by_name(const char *filename, photo_frame_error_t *error = nullptr);

  /**
   * @brief Parse a single TOC line into a GoogleDriveFile
   * @param line The line to parse in format: id|name
   * @param error Pointer to error code (optional)
   * @return Parsed GoogleDriveFile, or empty file if parse error
   */
  static GoogleDriveFile parse_file_line(const char *line, photo_frame_error_t *error = nullptr);

private:
  SdCard &sdCard_;
  const char *tocFilePath_;

  /**
   * @brief Open the TOC file and validate header
   * @param file Reference to store the opened file
   * @param error Pointer to error code (optional)
   * @return true if file opened and header is valid
   */
  bool open_and_validate_toc(fs::File &file, photo_frame_error_t *error = nullptr);

  /**
   * @brief Skip the header lines (timestamp and fileCount)
   * @param file Reference to the open file
   * @param error Pointer to error code (optional)
   * @return true if header was successfully skipped
   */
  bool skip_header(fs::File &file, photo_frame_error_t *error = nullptr);
};

} // namespace photo_frame

#endif // ENABLE_WEBSERVER_DATAPROVIDER