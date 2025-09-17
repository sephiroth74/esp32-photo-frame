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

#include "datetime_utils.h"

namespace photo_frame {

namespace datetime_utils {

// This will format the date and time as "YYYY/MM/DD HH:MM"
const char dateTimeFormatLong[] = "%04d/%02d/%02d %02d:%02d";

// This will format the date and time as "Monday, January 01 2023 12:00:00"
const char dateTimeFormatFull[] = "%A, %B %d %Y %H:%M:%S";

int format_datetime(char* buffer, size_t buffer_size, const DateTime& now, const char* format) {
    if (format == nullptr) {
        format = dateTimeFormatLong;
    }

    return snprintf(
        buffer, buffer_size, format, now.year(), now.month(), now.day(), now.hour(), now.minute());
}

int format_datetime(char* buffer, size_t buffer_size, const tm& timeinfo, const char* format) {
    if (format == nullptr) {
        format = dateTimeFormatLong;
    }

    DateTime dt(timeinfo.tm_year + 1900,
                timeinfo.tm_mon + 1,
                timeinfo.tm_mday,
                timeinfo.tm_hour,
                timeinfo.tm_min,
                timeinfo.tm_sec);
    return format_datetime(buffer, buffer_size, dt, format);
}

int format_datetime(char* buffer, size_t buffer_size, const time_t& timeinfo, const char* format) {
#ifdef DEBUG_DATETIME_UTILS
    Serial.print(F("[datetime_utils] format_datetime: "));
    Serial.println(timeinfo);
#endif // DEBUG_DATETIME_UTILS

    if (format == nullptr) {
        format = dateTimeFormatLong;
    }

    struct tm* tm_info = localtime(&timeinfo);

#ifdef DEBUG_DATETIME_UTILS
    Serial.print(F("[datetime_utils] format_datetime: "));
    Serial.print(tm_info->tm_hour);
    Serial.print(F(":"));
    Serial.print(tm_info->tm_min);
    Serial.println();
#endif // DEBUG_DATETIME_UTILS

    // return format_datetime(buffer, buffer_size, *tm_info, format);
    return strftime(buffer, buffer_size, format, tm_info);
}

int format_duration(char* buffer, size_t buffer_size, long seconds) {
    if (!buffer || buffer_size == 0) {
        return -1;
    }

    if (seconds <= 0) {
        buffer[0] = '\0';
        return 0;
    }

    // Calculate time components
    long hours             = seconds / 3600;
    long minutes           = (seconds % 3600) / 60;
    long remaining_seconds = seconds % 60;

    // Build format string and collect non-zero components
    char temp_buffer[32]; // Temporary buffer for building the string
    int pos           = 0;
    bool has_previous = false;

    // Add hours if present
    if (hours > 0) {
        pos += snprintf(temp_buffer + pos, sizeof(temp_buffer) - pos, "%ldh", hours);
        has_previous = true;
    }

    // Add minutes if present
    if (minutes > 0) {
        if (has_previous && pos < sizeof(temp_buffer) - 1) {
            temp_buffer[pos++] = ' ';
        }
        pos += snprintf(temp_buffer + pos, sizeof(temp_buffer) - pos, "%ldm", minutes);
        has_previous = true;
    }

    // Add seconds if present
    if (remaining_seconds > 0) {
        if (has_previous && pos < sizeof(temp_buffer) - 1) {
            temp_buffer[pos++] = ' ';
        }
        pos += snprintf(temp_buffer + pos, sizeof(temp_buffer) - pos, "%lds", remaining_seconds);
    }

    // Ensure null termination
    if (pos >= sizeof(temp_buffer)) {
        pos = sizeof(temp_buffer) - 1;
    }
    temp_buffer[pos] = '\0';

    // Copy to output buffer
    int result = snprintf(buffer, buffer_size, "%s", temp_buffer);

    // Return number of characters that would be written (or -1 if truncated)
    return (result >= 0 && result < (int)buffer_size) ? result : -1;
}

} // namespace datetime_utils

} // namespace photo_frame