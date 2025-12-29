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

#ifndef __PHOTO_FRAME_STRING_UTILS_H__
#define __PHOTO_FRAME_STRING_UTILS_H__

#include <Arduino.h>

namespace photo_frame {
namespace string_utils {

/**
 * @brief Build a String with optimized memory allocation for 2 components
 * @param s1 First string component
 * @param s2 Second string component
 * @return Optimized concatenated String
 */
inline String build_string(const String& s1, const String& s2) {
    String result;
    result.reserve(s1.length() + s2.length() + 1);
    result = s1;
    result += s2;
    return result;
}

/**
 * @brief Build a String with optimized memory allocation for 3 components
 * @param s1 First string component
 * @param s2 Second string component
 * @param s3 Third string component
 * @return Optimized concatenated String
 */
inline String build_string(const String& s1, const String& s2, const String& s3) {
    String result;
    result.reserve(s1.length() + s2.length() + s3.length() + 1);
    result = s1;
    result += s2;
    result += s3;
    return result;
}

/**
 * @brief Build a String with optimized memory allocation for 4 components
 * @param s1 First string component
 * @param s2 Second string component
 * @param s3 Third string component
 * @param s4 Fourth string component
 * @return Optimized concatenated String
 */
inline String build_string(const String& s1, const String& s2, const String& s3, const String& s4) {
    String result;
    result.reserve(s1.length() + s2.length() + s3.length() + s4.length() + 1);
    result = s1;
    result += s2;
    result += s3;
    result += s4;
    return result;
}

/**
 * @brief Build a file path with optimized memory allocation
 * @param dir Directory path
 * @param filename Filename
 * @param extension File extension (optional)
 * @return Optimized file path String
 */
inline String build_path(const String& dir, const String& filename, const String& extension = "") {
    String result;
    size_t totalLen =
        dir.length() + filename.length() + extension.length() + 2; // "/" and null terminator
    result.reserve(totalLen);
    result = dir;
    if (!dir.endsWith("/")) {
        result += "/";
    }
    result += filename;
    if (extension.length() > 0 && !extension.startsWith(".")) {
        result += ".";
    }
    result += extension;
    return result;
}

/**
 * @brief Build an HTTP request line with optimized memory allocation
 * @param method HTTP method (GET, POST, etc.)
 * @param path Request path
 * @param version HTTP version (default: "HTTP/1.1")
 * @return Optimized HTTP request line
 */
inline String build_http_request_line(const String& method,
                                      const String& path,
                                      const String& version = "HTTP/1.1") {
    String result;
    result.reserve(method.length() + path.length() + version.length() + 4); // spaces and \r\n
    result = method;
    result += " ";
    result += path;
    result += " ";
    result += version;
    result += "\r\n";
    return result;
}

/**
 * @brief Build an HTTP header with optimized memory allocation
 * @param name Header name
 * @param value Header value
 * @return Optimized HTTP header line
 */
inline String build_http_header(const String& name, const String& value) {
    String result;
    result.reserve(name.length() + value.length() + 4); // ": " and "\r\n"
    result = name;
    result += ": ";
    result += value;
    result += "\r\n";
    return result;
}

/**
 * @brief Check available heap and log warning if low
 * @param context Context description for logging
 * @param threshold Warning threshold in bytes (default: 4096)
 * @return true if heap is sufficient, false if low
 */
inline bool check_heap_health(const char* context, size_t threshold = 4096) {
    size_t freeHeap = ESP.getFreeHeap();
    if (freeHeap < threshold) {
        log_w("LOW HEAP WARNING in %s: %u bytes free", context, freeHeap);
        return false;
    }
    return true;
}

/**
 * @brief Convert seconds to human-readable format
 * @param buffer Output buffer
 * @param buffer_len Length of the output buffer
 * @param seconds Time in seconds
 */
inline void seconds_to_human(char* buffer, size_t buffer_len, unsigned long seconds) {
    unsigned long days = seconds / 86400;
    seconds %= 86400;
    unsigned long hours = seconds / 3600;
    seconds %= 3600;
    unsigned long minutes = seconds / 60;
    seconds %= 60;

    buffer[0] = '\0';
    char temp[32];

    if (days > 0) {
        snprintf(temp, sizeof(temp), "%lud ", days);
        strncat(buffer, temp, buffer_len - strlen(buffer) - 1);
    }
    if (hours > 0) {
        snprintf(temp, sizeof(temp), "%luh ", hours);
        strncat(buffer, temp, buffer_len - strlen(buffer) - 1);
    }
    if (minutes > 0) {
        snprintf(temp, sizeof(temp), "%lum ", minutes);
        strncat(buffer, temp, buffer_len - strlen(buffer) - 1);
    }
    snprintf(temp, sizeof(temp), "%lus", seconds);
    strncat(buffer, temp, buffer_len - strlen(buffer) - 1);
}

/**
 * @brief Convert file size to human-readable format (B, KB, MB, GB, TB)
 * @param buffer Output buffer to write the formatted size string
 * @param buffer_len Length of the output buffer
 * @param size File size in bytes
 */
inline void format_size_to_human_readable(char* buffer, size_t buffer_len, uint64_t size) {
    const char* units[]        = {"B", "KB", "MB", "GB", "TB"};
    size_t unit_index          = 0;
    double human_readable_size = (double)size;

    while (human_readable_size >= 1024 && unit_index < sizeof(units) / sizeof(units[0]) - 1) {
        human_readable_size /= 1024;
        unit_index++;
    }

    snprintf(buffer, buffer_len, "%.2f %s", human_readable_size, units[unit_index]);
}

} // namespace string_utils
} // namespace photo_frame

#endif // __PHOTO_FRAME_STRING_UTILS_H__