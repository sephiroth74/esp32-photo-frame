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
    Serial.print(F("[datetime_utils] format_datetime: "));
    Serial.println(timeinfo);

    if (format == nullptr) {
        format = dateTimeFormatLong;
    }

    struct tm* tm_info = localtime(&timeinfo);
    Serial.print(F("[datetime_utils] format_datetime: "));
    Serial.print(tm_info->tm_hour);
    Serial.print(F(":"));
    Serial.print(tm_info->tm_min);
    Serial.println();

    // return format_datetime(buffer, buffer_size, *tm_info, format);
    return strftime(buffer, buffer_size, format, tm_info);
}

} // namespace datetime_utils

} // namespace photo_frame