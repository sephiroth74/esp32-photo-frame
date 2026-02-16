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

#include "datetime_utils.h"
#include "config.h"

namespace photo_frame {

namespace datetime_utils {

// This will format the date and time as "YYYY/MM/DD HH:MM"
const char dateTimeFormatLong[] = "%04d/%02d/%02d %02d:%02d";

// This will format the date and time as "Mon, 1 Jan 2023 12:00"
const char dateTimeFormatFull[] = "%a, %e %b %Y %H:%M";

// This will format the date and time as ""
const char dateTimeFormatShort[] = "%d/%m/%y %H:%M";

int format_datetime(char *buffer, size_t buffer_size, const DateTime &now, const char *format) {
  if (format == nullptr) {
    // Default to short format if not specified
    // Callers should pass appropriate format based on runtime orientation
    format = dateTimeFormatShort;
  }

  // Convert DateTime to tm structure for strftime
  struct tm timeinfo;
  timeinfo.tm_year = now.year() - 1900; // tm_year is years since 1900
  timeinfo.tm_mon = now.month() - 1;    // tm_mon is 0-11
  timeinfo.tm_mday = now.day();
  timeinfo.tm_hour = now.hour();
  timeinfo.tm_min = now.minute();
  timeinfo.tm_sec = now.second();
  timeinfo.tm_wday = now.dayOfTheWeek(); // 0 = Sunday
  timeinfo.tm_yday = 0;                  // Day of year (not used)
  timeinfo.tm_isdst = -1;                // Daylight saving time info not available

  return strftime(buffer, buffer_size, format, &timeinfo);
}

int format_datetime(char *buffer, size_t buffer_size, const tm &timeinfo, const char *format) {
  if (format == nullptr) {
    format = dateTimeFormatLong;
  }

  DateTime dt(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  return format_datetime(buffer, buffer_size, dt, format);
}

int format_datetime(char *buffer, size_t buffer_size, const time_t &timeinfo, const char *format) {
#ifdef DEBUG_DATETIME_UTILS
  log_d("format_datetime: %ld", timeinfo);
#endif // DEBUG_DATETIME_UTILS

  if (format == nullptr) {
    format = dateTimeFormatLong;
  }

  struct tm *tm_info = localtime(&timeinfo);

#ifdef DEBUG_DATETIME_UTILS
  log_d("format_datetime: %d:%d", tm_info->tm_hour, tm_info->tm_min);
#endif // DEBUG_DATETIME_UTILS

  // return format_datetime(buffer, buffer_size, *tm_info, format);
  return strftime(buffer, buffer_size, format, tm_info);
}

int format_duration(char *buffer, size_t buffer_size, long seconds) {
  if (!buffer || buffer_size == 0) {
    return -1;
  }

  if (seconds <= 0) {
    buffer[0] = '\0';
    return 0;
  }

  // Calculate time components
  long hours = seconds / 3600;
  long minutes = (seconds % 3600) / 60;
  long remaining_seconds = seconds % 60;

  // Build format string and collect non-zero components
  char temp_buffer[32]; // Temporary buffer for building the string
  int pos = 0;
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
  if (remaining_seconds > 0 && !has_previous) {
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