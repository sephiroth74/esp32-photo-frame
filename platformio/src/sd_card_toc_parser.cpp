#include "sd_card_toc_parser.h"
#include "sd_card.h"
#include <Arduino.h>

namespace photo_frame {

sd_card_toc_parser::sd_card_toc_parser(sd_card& sdCard, const char* tocDataPath, const char* tocMetaPath)
    : sdCard_(sdCard)
    , tocDataPath_(tocDataPath)
    , tocMetaPath_(tocMetaPath)
    , headerParsed_(false)
    , cachedTimestamp_(0)
    , cachedFileCount_(0)
    , cachedDirModTime_(0)
    , metaParsed_(false) {
}

bool sd_card_toc_parser::toc_exists() const {
    return sdCard_.file_exists(tocDataPath_.c_str()) && sdCard_.file_exists(tocMetaPath_.c_str());
}

time_t sd_card_toc_parser::get_timestamp(photo_frame_error_t* error) {
    if (!headerParsed_) {
        File file;
        if (!open_and_validate_toc(file, error)) {
            return 0;
        }
        file.close();
    }
    return cachedTimestamp_;
}

size_t sd_card_toc_parser::get_file_count(photo_frame_error_t* error) {
    if (!headerParsed_) {
        File file;
        if (!open_and_validate_toc(file, error)) {
            return 0;
        }
        file.close();
    }
    return cachedFileCount_;
}

String sd_card_toc_parser::get_directory_path(photo_frame_error_t* error) {
    if (!parse_and_cache_metadata()) {
        if (error) {
            *error = error_type::TocMetadataMissing;
        }
        return String();
    }
    return cachedDirectoryPath_;
}

String sd_card_toc_parser::get_extension(photo_frame_error_t* error) {
    if (!parse_and_cache_metadata()) {
        if (error) {
            *error = error_type::TocMetadataMissing;
        }
        return String();
    }
    return cachedExtension_;
}

String sd_card_toc_parser::get_file_by_index(size_t index, photo_frame_error_t* error) {
    File file;
    if (!open_and_validate_toc(file, error)) {
        return String();
    }

    if (index >= cachedFileCount_) {
        if (error) {
            *error = error_type::TocIndexOutOfRange;
        }
        file.close();
        return String();
    }

    // Skip to the entries section (past header)
    if (!skip_to_entries(file, error)) {
        file.close();
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
            // New format: just path (no size for speed)
            String line(lineBuffer);
            line.trim();
            return line;
        }
    }

    file.close();
    return String();
}

bool sd_card_toc_parser::validate_toc(time_t dirModTime, const char* expectedPath, const char* expectedExt) {
    if (!toc_exists()) {
        return false;
    }

    // Check directory modification time from metadata
    time_t storedModTime = get_directory_mod_time();
    if (storedModTime == 0 || storedModTime != dirModTime) {
        log_v("TOC invalid: directory mod time changed (stored: %ld, current: %ld)",
              storedModTime, dirModTime);
        return false;
    }

    // Check directory path matches
    String storedPath = get_directory_path();
    if (expectedPath && storedPath != String(expectedPath)) {
        log_v("TOC invalid: directory path mismatch (stored: %s, expected: %s)",
              storedPath.c_str(), expectedPath);
        return false;
    }

    // Check extension matches
    String storedExt = get_extension();
    if (expectedExt && storedExt != String(expectedExt)) {
        log_v("TOC invalid: extension mismatch (stored: %s, expected: %s)",
              storedExt.c_str(), expectedExt);
        return false;
    }

    // Parse header to validate format
    File file;
    if (!open_and_validate_toc(file, nullptr)) {
        return false;
    }
    file.close();

    return true;
}

time_t sd_card_toc_parser::get_directory_mod_time(photo_frame_error_t* error) {
    if (!parse_and_cache_metadata()) {
        if (error) {
            *error = error_type::TocMetadataMissing;
        }
        return 0;
    }
    return cachedDirModTime_;
}

bool sd_card_toc_parser::open_and_validate_toc(File& file, photo_frame_error_t* error) {
    file = sdCard_.open(tocDataPath_.c_str(), "r");
    if (!file) {
        if (error) {
            *error = error_type::TocMetadataMissing;
        }
        return false;
    }

    if (!parse_header(file, error)) {
        file.close();
        return false;
    }

    return true;
}

bool sd_card_toc_parser::parse_header(File& file, photo_frame_error_t* error) {
    char lineBuffer[256];

    // Line 1: timestamp = <value>
    int len = read_line(file, lineBuffer, sizeof(lineBuffer));
    if (len <= 0) {
        if (error) {
            *error = error_type::TocHeaderInvalid;
        }
        return false;
    }
    String line(lineBuffer);
    String value;
    if (!parse_key_value(line, "timestamp", value)) {
        if (error) {
            *error = error_type::TocHeaderInvalid;
        }
        return false;
    }
    cachedTimestamp_ = value.toInt();

    // Line 2: fileCount = <value>
    len = read_line(file, lineBuffer, sizeof(lineBuffer));
    if (len <= 0) {
        if (error) {
            *error = error_type::TocHeaderInvalid;
        }
        return false;
    }
    line = String(lineBuffer);
    if (!parse_key_value(line, "fileCount", value)) {
        if (error) {
            *error = error_type::TocHeaderInvalid;
        }
        return false;
    }
    cachedFileCount_ = value.toInt();

    // Directory path and extension are now in the meta file, not in data file
    // We'll load them from meta file when needed

    headerParsed_ = true;
    return true;
}

bool sd_card_toc_parser::skip_to_entries(File& file, photo_frame_error_t* error) {
    // If we haven't parsed the header yet, do it now
    if (!headerParsed_) {
        if (!parse_header(file, error)) {
            return false;
        }
    } else {
        // Header already parsed, just skip the 2 header lines (timestamp and fileCount)
        file.seek(0);
        char lineBuffer[256];
        for (int i = 0; i < 2; i++) {
            if (read_line(file, lineBuffer, sizeof(lineBuffer)) <= 0) {
                if (error) {
                    *error = error_type::TocHeaderInvalid;
                }
                return false;
            }
        }
    }
    return true;
}

int sd_card_toc_parser::read_line(File& file, char* buffer, size_t maxLength) const {
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

        if (c != '\r') {  // Skip carriage returns
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

bool sd_card_toc_parser::parse_key_value(const String& line, const char* key, String& value) const {
    String keyStr = String(key) + " = ";
    if (line.startsWith(keyStr)) {
        value = line.substring(keyStr.length());
        value.trim();
        return true;
    }
    return false;
}

bool sd_card_toc_parser::parse_and_cache_metadata() const {
    if (metaParsed_) {
        return true; // Already cached
    }

    File metaFile = sdCard_.open(tocMetaPath_.c_str(), "r");
    if (!metaFile) {
        return false;
    }

    char lineBuffer[256];

    // Line 1: directoryModTime
    int len = read_line(metaFile, lineBuffer, sizeof(lineBuffer));
    if (len > 0) {
        String line(lineBuffer);
        String value;
        if (parse_key_value(line, "directoryModTime", value)) {
            cachedDirModTime_ = value.toInt();
        }
    }

    // Line 2: directoryPath
    len = read_line(metaFile, lineBuffer, sizeof(lineBuffer));
    if (len > 0) {
        String line(lineBuffer);
        parse_key_value(line, "directoryPath", cachedDirectoryPath_);
    }

    // Line 3: extension
    len = read_line(metaFile, lineBuffer, sizeof(lineBuffer));
    if (len > 0) {
        String line(lineBuffer);
        parse_key_value(line, "extension", cachedExtension_);
    }

    metaFile.close();
    metaParsed_ = true;
    return true;
}

} // namespace photo_frame