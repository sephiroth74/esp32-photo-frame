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

/**
 * @brief Format a duration in seconds as a human-readable string
 *
 * Formats seconds into a compact time representation like "2h 30m", "5m 15s", or "45s".
 * Only includes non-zero components and adds appropriate spacing.
 *
 * @param buffer Output buffer to write the formatted string
 * @param buffer_size Size of the output buffer
 * @param seconds Duration in seconds to format
 * @return int Number of characters written to buffer, or -1 on error
 */
int format_duration(char* buffer, size_t buffer_size, long seconds);

} // namespace datetime_utils

} // namespace photo_frame
