#ifndef __FOTO_FRAME_BOARD_UTIL_H__
#define __FOTO_FRAME_BOARD_UTIL_H__
#include "config.h"
#include "errors.h"
#include <Arduino.h>

namespace photo_frame {

void print_wakeup_reason();

void enter_deep_sleep();

void print_board_stats();

void disable_rgb_led();

void toggle_rgb_led(uint8_t red = LOW, uint8_t green = LOW, uint8_t blue = LOW);

void disable_built_in_led();

void blink_builtin_led(int count, uint32_t delay_ms = 500);

void blink_builtin_led(photo_frame_error_t error);

} // namespace photo_frame

#endif // __FOTO_FRAME_BOARD_UTIL_H__