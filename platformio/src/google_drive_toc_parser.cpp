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

#include "google_drive_toc_parser.h"

namespace photo_frame {

google_drive_toc_parser::google_drive_toc_parser(sd_card& sdCard, const char* tocFilePath)
    : sdCard_(sdCard), tocFilePath_(tocFilePath) {
}

time_t google_drive_toc_parser::get_timestamp(photo_frame_error_t* error) {
    if (error) {
        *error = error_type::None;
    }

    fs::File file = sdCard_.open(tocFilePath_, FILE_READ);
    if (!file) {
        Serial.print(F("Failed to open TOC file for timestamp: "));
        Serial.println(tocFilePath_);
        if (error) {
            *error = error_type::SdCardFileOpenFailed;
        }
        return 0;
    }

    // Read line 1 (timestamp = <value>)
    String line = file.readStringUntil('\n');
    file.close();

    if (line.length() == 0) {
        Serial.println(F("TOC file is empty or missing timestamp line"));
        if (error) {
            *error = error_type::JsonParseFailed;
        }
        return 0;
    }

    // Parse "timestamp = <number>"
    int equalPos = line.indexOf('=');
    if (equalPos == -1) {
        Serial.println(F("Invalid TOC format: missing '=' in timestamp line"));
        if (error) {
            *error = error_type::JsonParseFailed;
        }
        return 0;
    }

    String timestampStr = line.substring(equalPos + 1);
    timestampStr.trim();

    time_t timestamp = timestampStr.toInt();
    if (timestamp == 0 && timestampStr != "0") {
        Serial.println(F("Invalid timestamp value in TOC"));
        if (error) {
            *error = error_type::JsonParseFailed;
        }
        return 0;
    }

    return timestamp;
}

size_t google_drive_toc_parser::get_file_count(photo_frame_error_t* error) {
    if (error) {
        *error = error_type::None;
    }

    fs::File file = sdCard_.open(tocFilePath_, FILE_READ);
    if (!file) {
        Serial.print(F("Failed to open TOC file for file count: "));
        Serial.println(tocFilePath_);
        if (error) {
            *error = error_type::SdCardFileOpenFailed;
        }
        return 0;
    }

    // Skip line 1 (timestamp)
    String line = file.readStringUntil('\n');
    if (line.length() == 0) {
        Serial.println(F("TOC file is empty or invalid"));
        file.close();
        if (error) {
            *error = error_type::JsonParseFailed;
        }
        return 0;
    }

    // Read line 2 (fileCount)
    line = file.readStringUntil('\n');
    file.close();

    if (line.length() == 0) {
        Serial.println(F("TOC file missing fileCount line"));
        if (error) {
            *error = error_type::JsonParseFailed;
        }
        return 0;
    }

    // Parse "fileCount = <number>"
    int equalPos = line.indexOf('=');
    if (equalPos == -1) {
        Serial.println(F("Invalid TOC format: missing '=' in fileCount line"));
        if (error) {
            *error = error_type::JsonParseFailed;
        }
        return 0;
    }

    String countStr = line.substring(equalPos + 1);
    countStr.trim();

    size_t fileCount = countStr.toInt();
    if (fileCount == 0 && countStr != "0") {
        Serial.println(F("Invalid fileCount value in TOC"));
        if (error) {
            *error = error_type::JsonParseFailed;
        }
        return 0;
    }

    return fileCount;
}

google_drive_file google_drive_toc_parser::get_file_by_index(size_t index, photo_frame_error_t* error) {
    Serial.print(F("Getting TOC file at index: "));
    Serial.println(index);

    if (error) {
        *error = error_type::None;
    }

    fs::File file;
    if (!open_and_validate_toc(file, error)) {
        return google_drive_file();
    }

    if (!skip_header(file, error)) {
        file.close();
        return google_drive_file();
    }

    // Skip to the desired index
    for (size_t i = 0; i < index; i++) {
        String skipLine = file.readStringUntil('\n');
        if (skipLine.length() == 0) {
            Serial.print(F("TOC file ended before reaching index "));
            Serial.println(index);
            file.close();
            if (error) {
                *error = error_type::JsonParseFailed;
            }
            return google_drive_file();
        }
    }

    // Read the target line
    String targetLine = file.readStringUntil('\n');
    file.close();

    if (targetLine.length() == 0) {
        Serial.print(F("No file entry found at index "));
        Serial.println(index);
        if (error) {
            *error = error_type::JsonParseFailed;
        }
        return google_drive_file();
    }

    return parse_file_line(targetLine.c_str(), error);
}

google_drive_file google_drive_toc_parser::get_file_by_name(const char* filename, photo_frame_error_t* error) {
    Serial.print(F("Getting TOC file by name: "));
    Serial.println(filename);
    
    if (error) {
        *error = error_type::None;
    }

    fs::File file;
    if (!open_and_validate_toc(file, error)) {
        return google_drive_file();
    }

    if (!skip_header(file, error)) {
        file.close();
        return google_drive_file();
    }

    // Search through all file entries
    while (file.available()) {
        String line = file.readStringUntil('\n');
        line.trim();
        
        if (line.length() == 0) {
            continue;
        }

        // Parse line to get the name
        int pos1 = line.indexOf('|');
        if (pos1 == -1) continue;

        int pos2 = line.indexOf('|', pos1 + 1);
        if (pos2 == -1) continue;

        String name = line.substring(pos1 + 1, pos2);
        
        // Check if this is the file we're looking for
        if (name.equals(filename)) {
            file.close();
            
            Serial.print(F("Found file by name: "));
            Serial.println(filename);
            
            return parse_file_line(line.c_str(), error);
        }
    }

    file.close();

    Serial.print(F("File not found by name: "));
    Serial.println(filename);
    
    if (error) {
        *error = error_type::SdCardFileNotFound;
    }
    
    return google_drive_file();
}


google_drive_file google_drive_toc_parser::parse_file_line(const char* line, photo_frame_error_t* error) {
    if (error) {
        *error = error_type::None;
    }

    String lineStr(line);
    
    // Parse line: id|name
    int pos1 = lineStr.indexOf('|');
    if (pos1 == -1) {
        Serial.println(F("Invalid file entry format: missing separator"));
        if (error) {
            *error = error_type::JsonParseFailed;
        }
        return google_drive_file("", "");
    }

    String id = lineStr.substring(0, pos1);
    String name = lineStr.substring(pos1 + 1);

    return google_drive_file(id, name);
}

bool google_drive_toc_parser::open_and_validate_toc(fs::File& file, photo_frame_error_t* error) {
    file = sdCard_.open(tocFilePath_, FILE_READ);
    if (!file) {
        Serial.print(F("Failed to open TOC file: "));
        Serial.println(tocFilePath_);
        if (error) {
            *error = error_type::SdCardFileOpenFailed;
        }
        return false;
    }

    return true;
}

bool google_drive_toc_parser::skip_header(fs::File& file, photo_frame_error_t* error) {
    Serial.println(F("Skipping TOC header lines..."));
    // Skip line 1 (timestamp) and line 2 (fileCount)
    String line1 = file.readStringUntil('\n');
    String line2 = file.readStringUntil('\n');

    if (line1.length() == 0 || line2.length() == 0) {
        Serial.println(F("TOC file missing header lines"));
        if (error) {
            *error = error_type::JsonParseFailed;
        }
        return false;
    }

    return true;
}

} // namespace photo_frame