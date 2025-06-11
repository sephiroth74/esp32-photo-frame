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
DateTime fetch_datetime(SDCard& sdCard, bool reset = true);

} // namespace photo_frame
#endif // __PHOTO_FRAME_RTC_UTIL_H__