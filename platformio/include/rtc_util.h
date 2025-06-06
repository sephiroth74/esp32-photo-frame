#ifndef __PHOTO_FRAME_RTC_UTIL_H__
#define __PHOTO_FRAME_RTC_UTIL_H__

#include "sd_card.h"
#include <Arduino.h>
#include <RTClib.h>

namespace photo_frame {

DateTime fetch_datetime(SDCard& sdCard);

} // namespace photo_frame
#endif // __PHOTO_FRAME_RTC_UTIL_H__