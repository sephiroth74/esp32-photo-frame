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

#pragma once

#include "config.h"
#include "google_drive_client.h"
#include "sd_card.h"
#include "errors.h"

namespace photo_frame {

/**
 * @brief Utility class for parsing Google Drive TOC (Table of Contents) files
 * 
 * This class provides methods to efficiently read and parse TOC files stored on SD card
 * in plain text format. The TOC format is:
 * Line 1: timestamp = <value>
 * Line 2: fileCount = <value>  
 * Line 3+: id|name|mimeType|modifiedTime
 */
class google_drive_toc_parser {
public:
    /**
     * @brief Constructor
     * @param sdCard Reference to the SD card instance
     * @param tocFilePath Path to the TOC file
     */
    google_drive_toc_parser(sd_card& sdCard, const char* tocFilePath);

    /**
     * @brief Get the timestamp from the TOC file
     * @param error Pointer to error code (optional)
     * @return Timestamp value, or 0 if error
     */
    time_t get_timestamp(photo_frame_error_t* error = nullptr);

    /**
     * @brief Get the file count from the TOC file
     * @param error Pointer to error code (optional)
     * @return Number of files in the TOC, or 0 if error
     */
    size_t get_file_count(photo_frame_error_t* error = nullptr);

    /**
     * @brief Get a file entry by index
     * @param index Zero-based index of the file to retrieve
     * @param error Pointer to error code (optional)
     * @return google_drive_file at the specified index, or empty file if error
     */
    google_drive_file get_file_by_index(size_t index, photo_frame_error_t* error = nullptr);

    /**
     * @brief Find a file by name
     * @param filename Name of the file to search for
     * @param error Pointer to error code (optional)
     * @return google_drive_file with the specified name, or empty file if not found
     */
    google_drive_file get_file_by_name(const char* filename, photo_frame_error_t* error = nullptr);

    /**
     * @brief Load all files from the TOC into a vector
     * @param error Pointer to error code (optional)
     * @return Vector of all google_drive_file entries
     */
    google_drive_files_list load_all_files(photo_frame_error_t* error = nullptr);

    /**
     * @brief Parse a single TOC line into a google_drive_file
     * @param line The line to parse in format: id|name|mimeType|modifiedTime
     * @param error Pointer to error code (optional)
     * @return Parsed google_drive_file, or empty file if parse error
     */
    static google_drive_file parse_file_line(const char* line, photo_frame_error_t* error = nullptr);

private:
    sd_card& sdCard_;
    const char* tocFilePath_;

    /**
     * @brief Open the TOC file and validate header
     * @param file Reference to store the opened file
     * @param error Pointer to error code (optional)
     * @return true if file opened and header is valid
     */
    bool open_and_validate_toc(fs::File& file, photo_frame_error_t* error = nullptr);

    /**
     * @brief Skip the header lines (timestamp and fileCount)
     * @param file Reference to the open file
     * @param error Pointer to error code (optional)
     * @return true if header was successfully skipped
     */
    bool skip_header(fs::File& file, photo_frame_error_t* error = nullptr);
};

} // namespace photo_frame