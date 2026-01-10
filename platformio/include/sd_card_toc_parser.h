#ifndef SD_CARD_TOC_PARSER_H
#define SD_CARD_TOC_PARSER_H

#include <Arduino.h>
#include <FS.h>
#include "errors.h"

namespace photo_frame {

// Forward declaration
class sd_card;

/**
 * Parser for SD card Table of Contents (TOC) files.
 *
 * The TOC system caches directory listings to avoid expensive
 * directory iterations on every image selection.
 *
 * TOC data file format (optimized for speed - no size):
 * - Line 1: timestamp = <unix_timestamp>
 * - Line 2: fileCount = <number>
 * - Lines 3+: path (e.g., "/6c/portrait/bin/image.bin")
 *
 * TOC meta file format:
 * - directoryModTime = <unix_timestamp>
 * - directoryPath = <path>
 * - extension = <ext>
 */
class sd_card_toc_parser {
public:
    /**
     * Constructor
     * @param sdCard Reference to SD card instance
     * @param tocDataPath Path to TOC data file
     * @param tocMetaPath Path to TOC metadata file
     */
    sd_card_toc_parser(sd_card& sdCard, const char* tocDataPath, const char* tocMetaPath);

    /**
     * Get the timestamp when the TOC was created
     * @param error Optional error output
     * @return Unix timestamp, or 0 on error
     */
    time_t get_timestamp(photo_frame_error_t* error = nullptr);

    /**
     * Get the number of files in the TOC
     * @param error Optional error output
     * @return File count, or 0 on error
     */
    size_t get_file_count(photo_frame_error_t* error = nullptr);

    /**
     * Get the directory path this TOC represents
     * @param error Optional error output
     * @return Directory path, or empty string on error
     */
    String get_directory_path(photo_frame_error_t* error = nullptr);

    /**
     * Get the file extension filter used for this TOC
     * @param error Optional error output
     * @return Extension (e.g., ".bin"), or empty string on error
     */
    String get_extension(photo_frame_error_t* error = nullptr);

    /**
     * Get a file path by index from the TOC
     * @param index Zero-based index
     * @param error Optional error output
     * @return File path, or empty string on error
     */
    String get_file_by_index(size_t index, photo_frame_error_t* error = nullptr);

    /**
     * Validate TOC integrity
     * @param dirModTime Expected directory modification time
     * @param expectedPath Expected directory path (optional)
     * @param expectedExt Expected file extension (optional)
     * @return true if TOC is valid and matches expected state
     */
    bool validate_toc(time_t dirModTime, const char* expectedPath = nullptr, const char* expectedExt = nullptr);

    /**
     * Check if TOC files exist
     * @return true if both TOC data and metadata files exist
     */
    bool toc_exists() const;

    /**
     * Get directory modification time from metadata
     * @param error Optional error output
     * @return Modification time, or 0 on error
     */
    time_t get_directory_mod_time(photo_frame_error_t* error = nullptr);

private:
    sd_card& sdCard_;
    String tocDataPath_;  // Changed from const char* to String to avoid dangling pointers
    String tocMetaPath_;  // Changed from const char* to String to avoid dangling pointers

    // Cached header values
    mutable bool headerParsed_;
    mutable time_t cachedTimestamp_;
    mutable size_t cachedFileCount_;
    mutable String cachedDirectoryPath_;
    mutable String cachedExtension_;
    mutable time_t cachedDirModTime_;
    mutable bool metaParsed_;

    /**
     * Open and validate TOC file
     * @param file Output file handle
     * @param error Optional error output
     * @return true on success
     */
    bool open_and_validate_toc(File& file, photo_frame_error_t* error = nullptr);

    /**
     * Parse header from TOC file
     * @param file Open file handle
     * @param error Optional error output
     * @return true on success
     */
    bool parse_header(File& file, photo_frame_error_t* error = nullptr);

    /**
     * Skip to the start of file entries (past header)
     * @param file Open file handle
     * @param error Optional error output
     * @return true on success
     */
    bool skip_to_entries(File& file, photo_frame_error_t* error = nullptr);

    /**
     * Read a line from file
     * @param file Open file handle
     * @param buffer Output buffer
     * @param maxLength Maximum line length
     * @return Number of characters read, or -1 on error
     */
    int read_line(File& file, char* buffer, size_t maxLength) const;

    /**
     * Parse a key=value line
     * @param line Input line
     * @param key Key to match
     * @param value Output value
     * @return true if key matches and value extracted
     */
    bool parse_key_value(const String& line, const char* key, String& value) const;

    /**
     * Parse and cache all metadata from meta file
     * @return true on success
     */
    bool parse_and_cache_metadata() const;
};

} // namespace photo_frame

#endif // SD_CARD_TOC_PARSER_H