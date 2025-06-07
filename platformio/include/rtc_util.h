#ifndef __PHOTO_FRAME_RTC_UTIL_H__
#define __PHOTO_FRAME_RTC_UTIL_H__

#include "sd_card.h"
#include <Arduino.h>
#include <RTClib.h>

namespace photo_frame {

/**
 * Fetches the current date and time from the RTC or WiFi if the RTC is not available.
 * @param sdCard Reference to the SDCard object used for fetching time from WiFi if RTC is not
 * available.
 * @return DateTime object representing the current date and time.
 * If the RTC is not available or has lost power, it attempts to fetch the time from WiFi.
 * If both RTC and WiFi are unavailable, it returns an invalid DateTime object.
 */
DateTime fetch_datetime(SDCard& sdCard);

} // namespace photo_frame
#endif // __PHOTO_FRAME_RTC_UTIL_H__