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
    inline String build_string(const String& s1, const String& s2)
    {
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
    inline String build_string(const String& s1, const String& s2, const String& s3)
    {
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
    inline String build_string(const String& s1, const String& s2, const String& s3, const String& s4)
    {
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
    inline String buildPath(const String& dir, const String& filename, const String& extension = "")
    {
        String result;
        size_t totalLen = dir.length() + filename.length() + extension.length() + 2; // "/" and null terminator
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
    inline String
    buildHttpRequestLine(const String& method, const String& path, const String& version = "HTTP/1.1")
    {
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
    inline String buildHttpHeader(const String& name, const String& value)
    {
        String result;
        result.reserve(name.length() + value.length() + 4); // ": " and "\r\n"
        result = name;
        result += ": ";
        result += value;
        result += "\r\n";
        return result;
    }

    /**
     * @brief Convert seconds to human-readable format
     * @param buffer Output buffer
     * @param buffer_len Length of the output buffer
     * @param seconds Time in seconds
     */
    inline void seconds_to_human(char* buffer, size_t buffer_len, unsigned long seconds)
    {
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
    inline void format_size_to_human_readable(char* buffer, size_t buffer_len, uint64_t size)
    {
        const char* units[] = { "B", "KB", "MB", "GB", "TB" };
        size_t unit_index = 0;
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