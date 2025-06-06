#ifndef __PHOTO_FRAME_ERRORS_H__
#define __PHOTO_FRAME_ERRORS_H__

namespace photo_frame {

typedef class photo_frame_error {
  public:
    const char* message;
    uint8_t code;
    uint8_t blink_count; // Number of times to blink the built-in LED for this error

    constexpr photo_frame_error(const char* msg, uint8_t err_code) :
        message(msg),
        code(err_code),
        blink_count(1) {}
    constexpr photo_frame_error(const char* msg, uint8_t err_code, uint8_t blink_count) :
        message(msg),
        code(err_code),
        blink_count(blink_count) {}

    constexpr bool operator==(const photo_frame_error& other) const {
        return (this->code == other.code) && (this->blink_count == other.blink_count);
    }
    constexpr bool operator!=(const photo_frame_error& other) const { return !(*this == other); }
} photo_frame_error_t;

namespace error_type {
const photo_frame_error None{TXT_NO_ERROR, 0, 0};
const photo_frame_error CardMountFailed{TXT_CARD_MOUNT_FAILED, 3, 3};
const photo_frame_error NoSdCardAttached{TXT_NO_SD_CARD_ATTACHED, 4, 4};
const photo_frame_error UnknownSdCardType{TXT_UNKNOWN_SD_CARD_TYPE, 5, 5};
const photo_frame_error CardOpenFileFailed{TXT_CARD_OPEN_FILE_FAILED, 6, 6};
const photo_frame_error SdCardFileNotFound{TXT_SD_CARD_FILE_NOT_FOUND, 7, 7};
const photo_frame_error SdCardFileOpenFailed{TXT_SD_CARD_FILE_OPEN_FAILED, 8, 8};
const photo_frame_error ImageFormatNotSupported{TXT_IMAGE_FORMAT_NOT_SUPPORTED, 9, 9};
const photo_frame_error BatteryLevelCritical{TXT_BATTERY_LEVEL_CRITICAL, 10, 10};
// Add more errors here
} // namespace error_type

} // namespace photo_frame

#endif // __PHOTO_FRAME_ERRORS_H__