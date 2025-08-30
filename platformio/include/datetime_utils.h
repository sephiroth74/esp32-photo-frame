#pragma once

#include <time.h>
#include <Arduino.h>
#include <RTClib.h>

namespace photo_frame {

namespace datetime_utils {

    // This will format the date and time as "YYYY/MM/DD HH:MM"
    const char dateTimeFormatLong[] = "%04d/%02d/%02d %02d:%02d";

    // This will format the date and time as "Monday, January 01 2023 12:00:00"
    const char dateTimeFormatFull[] = "%A, %B %d %Y %H:%M:%S";

    int format_datetime(char* buffer, size_t buffer_size, const DateTime& now, const char* format = dateTimeFormatLong)
    {
        return snprintf(buffer, buffer_size,
            format,
            now.year(),
            now.month(),
            now.day(),
            now.hour(),
            now.minute());
    } // format_datetime

    int format_datetime(char* buffer, size_t buffer_size, const tm& timeinfo, const char* format = dateTimeFormatLong)
    {
        DateTime dt(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
        return format_datetime(buffer, buffer_size, dt, format);
    }

    int format_datetime(char* buffer, size_t buffer_size, const time_t& timeinfo, const char* format = dateTimeFormatLong)
    {
        struct tm* tm_info = localtime(&timeinfo);
        return format_datetime(buffer, buffer_size, *tm_info, format);
    }

} // namespace datetime_utils

} // namespace photo_frame
