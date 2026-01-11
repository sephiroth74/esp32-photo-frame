#include "sd_card_toc_parser.h"
#include "sd_card.h"
#include <Arduino.h>

namespace photo_frame {

SdCardTocParser::SdCardTocParser(SdCard& sdCard, const char* tocDataPath, const char* tocMetaPath) :
    sdCard_(sdCard),
    tocDataPath_(tocDataPath),
    tocMetaPath_(tocMetaPath),
    metaParsed_(false),
    cachedDirModTime_(0),
    cachedFileCount_(0),
    cachedFileSize_(0) {}

bool SdCardTocParser::toc_exists() const {
    return sdCard_.fileExists(tocDataPath_.c_str()) && sdCard_.fileExists(tocMetaPath_.c_str());
}

time_t SdCardTocParser::get_timestamp(photo_frame_error_t* error) {
    // Timestamp is the modification time of the directory
    if (!parse_and_cache_metadata()) {
        if (error) {
            *error = error_type::TocMetadataMissing;
        }
        return 0;
    }
    return cachedDirModTime_;
}

size_t SdCardTocParser::get_file_count(photo_frame_error_t* error) {
    if (!parse_and_cache_metadata()) {
        if (error) {
            *error = error_type::TocMetadataMissing;
        }
        return 0;
    }
    return cachedFileCount_;
}

String SdCardTocParser::get_directory_path(photo_frame_error_t* error) {
    if (!parse_and_cache_metadata()) {
        if (error) {
            *error = error_type::TocMetadataMissing;
        }
        return String();
    }
    return cachedDirectoryPath_;
}

String SdCardTocParser::get_extension(photo_frame_error_t* error) {
    if (!parse_and_cache_metadata()) {
        if (error) {
            *error = error_type::TocMetadataMissing;
        }
        return String();
    }
    return cachedExtension_;
}

String SdCardTocParser::get_file_by_index(size_t index, photo_frame_error_t* error) {
    // First ensure metadata is cached (to get file count)
    if (!parse_and_cache_metadata()) {
        if (error) {
            *error = error_type::TocMetadataMissing;
        }
        return String();
    }

    if (index >= cachedFileCount_) {
        if (error) {
            *error = error_type::TocIndexOutOfRange;
        }
        return String();
    }

    // Open data file directly (no header now, just file list)
    File file = sdCard_.open(tocDataPath_.c_str(), "r");
    if (!file) {
        if (error) {
            *error = error_type::TocMetadataMissing; // Use existing error type
        }
        return String();
    }

    // Read lines until we reach the target index
    char lineBuffer[512];
    for (size_t i = 0; i <= index; i++) {
        int len = read_line(file, lineBuffer, sizeof(lineBuffer));
        if (len <= 0) {
            if (error) {
                *error = error_type::TocReadFailed;
            }
            file.close();
            return String();
        }

        if (i == index) {
            file.close();
            String line(lineBuffer);
            line.trim();
            return line;
        }
    }

    file.close();
    return String();
}

bool SdCardTocParser::validate_toc(const char* expectedPath, const char* expectedExt) {
    if (!toc_exists()) {
        return false;
    }

    // Parse metadata if not already cached
    if (!parse_and_cache_metadata()) {
        return false;
    }

    // Check directory path matches
    if (expectedPath && cachedDirectoryPath_ != String(expectedPath)) {
        log_v("TOC invalid: directory path mismatch (stored: %s, expected: %s)",
              cachedDirectoryPath_.c_str(),
              expectedPath);
        return false;
    }

    // Check extension matches
    if (expectedExt && cachedExtension_ != String(expectedExt)) {
        log_v("TOC invalid: extension mismatch (stored: %s, expected: %s)",
              cachedExtension_.c_str(),
              expectedExt);
        return false;
    }

    // Validate TOC data file size matches what was stored in metadata
    if (cachedFileSize_ > 0) {
        File tocDataFile = sdCard_.open(tocDataPath_.c_str(), "r");
        if (!tocDataFile) {
            log_v("TOC invalid: cannot open TOC data file for size validation");
            return false;
        }

        size_t actualSize = tocDataFile.size();
        tocDataFile.close();

        if (actualSize != cachedFileSize_) {
            log_v("TOC invalid: file size mismatch (stored: %u, actual: %u)",
                  cachedFileSize_,
                  actualSize);
            return false;
        }
    }

    return true;
}

// Old functions removed - TOC data file now contains only file list

int SdCardTocParser::read_line(File& file, char* buffer, size_t maxLength) const {
    if (!file || !buffer || maxLength == 0) {
        return -1;
    }

    size_t pos = 0;
    while (pos < maxLength - 1) {
        int c = file.read();
        if (c < 0) {
            // End of file
            if (pos > 0) {
                buffer[pos] = '\0';
                return pos;
            }
            return -1;
        }

        if (c == '\n') {
            buffer[pos] = '\0';
            return pos;
        }

        if (c != '\r') { // Skip carriage returns
            buffer[pos++] = (char)c;
        }
    }

    // Line too long
    buffer[pos] = '\0';
    // Skip rest of line
    int c;
    do {
        c = file.read();
    } while (c >= 0 && c != '\n');

    return pos;
}

bool SdCardTocParser::parse_key_value(const String& line, const char* key, String& value) const {
    String keyStr = String(key) + " = ";
    if (line.startsWith(keyStr)) {
        value = line.substring(keyStr.length());
        value.trim();
        return true;
    }
    return false;
}

bool SdCardTocParser::parse_and_cache_metadata() const {
    if (metaParsed_) {
        return true; // Already cached
    }

    File metaFile = sdCard_.open(tocMetaPath_.c_str(), "r");
    if (!metaFile) {
        return false;
    }

    char lineBuffer[256];

    // Line 1: directoryPath
    int len = read_line(metaFile, lineBuffer, sizeof(lineBuffer));
    if (len > 0) {
        String line(lineBuffer);
        parse_key_value(line, "directoryPath", cachedDirectoryPath_);
    }

    // Line 2: directoryModTime
    len = read_line(metaFile, lineBuffer, sizeof(lineBuffer));
    if (len > 0) {
        String line(lineBuffer);
        String value;
        if (parse_key_value(line, "directoryModTime", value)) {
            cachedDirModTime_ = value.toInt();
        }
    }

    // Line 3: fileCount
    len = read_line(metaFile, lineBuffer, sizeof(lineBuffer));
    if (len > 0) {
        String line(lineBuffer);
        String value;
        if (parse_key_value(line, "fileCount", value)) {
            cachedFileCount_ = value.toInt();
        }
    }

    // Line 4: extension
    len = read_line(metaFile, lineBuffer, sizeof(lineBuffer));
    if (len > 0) {
        String line(lineBuffer);
        parse_key_value(line, "extension", cachedExtension_);
    }

    // Line 5: fileSize
    len = read_line(metaFile, lineBuffer, sizeof(lineBuffer));
    if (len > 0) {
        String line(lineBuffer);
        String value;
        if (parse_key_value(line, "fileSize", value)) {
            cachedFileSize_ = value.toInt();
        }
    }

    metaFile.close();
    metaParsed_ = true;
    return true;
}

} // namespace photo_frame