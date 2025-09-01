#pragma once

#include <Arduino.h>
#include <RTClib.h>
#include <time.h>

namespace photo_frame {

namespace datetime_utils {

// This will format the date and time as "YYYY/MM/DD HH:MM"
extern const char dateTimeFormatLong[];

// This will format the date and time as "Monday, January 01 2023 12:00:00"
extern const char dateTimeFormatFull[];

int format_datetime(char* buffer,
                    size_t buffer_size,
                    const DateTime& now,
                    const char* format = nullptr);

int format_datetime(char* buffer,
                    size_t buffer_size,
                    const tm& timeinfo,
                    const char* format = nullptr);

int format_datetime(char* buffer,
                    size_t buffer_size,
                    const time_t& timeinfo,
                    const char* format = nullptr);

} // namespace datetime_utils

} // namespace photo_frame
