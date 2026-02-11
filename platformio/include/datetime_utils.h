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

#pragma once

#include <Arduino.h>
#include <RTClib.h>
#include <time.h>

namespace photo_frame {

namespace datetime_utils {

/**
 * @brief Long date-time format string constant.
 *
 * This format string produces date and time in the format "YYYY/MM/DD HH:MM".
 * Commonly used for compact display of date and time information on the e-paper
 * display.
 *
 * @note This is a null-terminated string constant defined in datetime_utils.cpp
 */
extern const char dateTimeFormatLong[];

/**
 * @brief Full date-time format string constant.
 *
 * This format string produces date and time in the format "Monday, January 01
 * 2023 12:00:00". Used for detailed display of date and time information when
 * space permits.
 *
 * @note This is a null-terminated string constant defined in datetime_utils.cpp
 */
extern const char dateTimeFormatFull[];

/**
 * @brief Short date-time format string constant.
 *
 * This format string produces date and time in the format "Lun, 1 Gen 2023
 * 12:00". Used for concise display of date and time information when space is
 * limited. Uses strftime format specifiers: %a (weekday), %e (day), %b (month),
 * %Y (year), %H:%M (time).
 *
 * @note This is a null-terminated string constant defined in datetime_utils.cpp
 */
extern const char dateTimeFormatShort[];

/**
 * @brief Format DateTime object as string using specified format.
 *
 * Converts an RTClib DateTime object to a formatted string representation.
 * If no format is specified, uses a default format suitable for display.
 *
 * @param buffer Output buffer to write the formatted string
 * @param buffer_size Size of the output buffer in bytes
 * @param now DateTime object to format
 * @param format Optional format string (uses default if nullptr)
 * @return Number of characters written to buffer, or -1 on error
 * @note Buffer should be large enough to accommodate the formatted string
 */
int format_datetime(char *buffer, size_t buffer_size, const DateTime &now, const char *format = nullptr);

/**
 * @brief Format tm struct as string using specified format.
 *
 * Converts a standard C tm time structure to a formatted string representation.
 * If no format is specified, uses a default format suitable for display.
 *
 * @param buffer Output buffer to write the formatted string
 * @param buffer_size Size of the output buffer in bytes
 * @param timeinfo tm structure containing time information
 * @param format Optional format string (uses default if nullptr)
 * @return Number of characters written to buffer, or -1 on error
 * @note Buffer should be large enough to accommodate the formatted string
 */
int format_datetime(char *buffer, size_t buffer_size, const tm &timeinfo, const char *format = nullptr);

/**
 * @brief Format time_t as string using specified format.
 *
 * Converts a time_t timestamp to a formatted string representation.
 * If no format is specified, uses a default format suitable for display.
 *
 * @param buffer Output buffer to write the formatted string
 * @param buffer_size Size of the output buffer in bytes
 * @param timeinfo time_t timestamp to format
 * @param format Optional format string (uses default if nullptr)
 * @return Number of characters written to buffer, or -1 on error
 * @note Buffer should be large enough to accommodate the formatted string
 */
int format_datetime(char *buffer, size_t buffer_size, const time_t &timeinfo, const char *format = nullptr);

/**
 * @brief Format a duration in seconds as a human-readable string
 *
 * Formats seconds into a compact time representation like "2h 30m", "5m 15s",
 * or "45s". Only includes non-zero components and adds appropriate spacing.
 *
 * @param buffer Output buffer to write the formatted string
 * @param buffer_size Size of the output buffer
 * @param seconds Duration in seconds to format
 * @return int Number of characters written to buffer, or -1 on error
 */
int format_duration(char *buffer, size_t buffer_size, long seconds);

} // namespace datetime_utils

} // namespace photo_frame
