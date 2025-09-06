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

#ifndef __FOTO_FRAME_BOARD_UTIL_H__
#define __FOTO_FRAME_BOARD_UTIL_H__
#include "config.h"
#include "errors.h"
#include <Arduino.h>

namespace photo_frame {

namespace board_utils {

/**
 * Gets and prints the reason for the last wakeup from deep sleep.
 */
esp_sleep_wakeup_cause_t get_wakeup_reason();

/**
 * Converts the wakeup reason to a human-readable string.
 * @param wakeup_reason The reason for waking up from deep sleep.
 * @param buffer The buffer to store the string representation.
 * @param buffer_size The size of the buffer.
 */
void get_wakeup_reason_string(esp_sleep_wakeup_cause_t wakeup_reason,
                              char* buffer,
                              size_t buffer_size);

/**
 * Enters deep sleep mode, disabling peripherals and LEDs to save power.
 * @param wakeup_reason The reason for waking up, which can be used to determine the next action.
 */
void enter_deep_sleep(esp_sleep_wakeup_cause_t wakeup_reason);

/**
 * Prints various statistics about the board, including heap size, flash size, free heap, free
 * flash, PSRAM, chip model, revision, cores, and CPU frequency.
 */
void print_board_stats();

/**
 * Prints the pin assignments for the board.
 */
void print_board_pins();

/**
 * Disables the RGB LED by setting its pins to LOW.
 */
void disable_rgb_led();

/**
 * Toggles the RGB LED to the specified color.
 * @param red    If true, turns on the red LED.
 * @param green  If true, turns on the green LED.
 * @param blue   If true, turns on the blue LED.
 */
void toggle_rgb_led(bool red = false, bool green = false, bool blue = false);

/**
 * Disables the built-in LED by setting its pin to LOW.
 */
void disable_built_in_led();

/**
 * Blinks the built-in LED a specified number of times with a delay between blinks.
 * @param count     The number of times to blink the LED.
 * @param on_ms     The duration in milliseconds to keep the LED on.
 * @param off_ms    The duration in milliseconds to keep the LED off.
 */
void blink_builtin_led(int count, unsigned long on_ms = 100, unsigned long off_ms = 300);

/**
 * Blinks the built-in LED according to the specified error.
 * @param error The error containing the message, code, and blink count.
 */
void blink_builtin_led(photo_frame_error_t error);

/**
 * Blinks the built-in RGB led a specified number of times with a delay between blinks and a
 * specified color.
 * @param count     The number of times to blink the LED.
 * @param red       If true, turns on the red LED.
 * @param green     If true, turns on the green LED.
 * @param blue      If true, turns on the blue LED.
 * @param on_ms     The duration in milliseconds to keep the LED on.
 * @param off_ms    The duration in milliseconds to keep the LED off.
 */
void blink_rgb_led(uint32_t count,
                   bool red,
                   bool green,
                   bool blue,
                   unsigned long on_ms  = 100,
                   unsigned long off_ms = 300);

/**
 * Blinks the built-in RGB LED according to the specified error.
 * @param error The error containing the message, code, and blink count.
 */
void blink_rgb_led(photo_frame_error_t error);

/**
 * Reads the refresh interval in seconds, adjusting for battery level if specified.
 * @param is_battery_low Indicates if the battery is low, which may affect the refresh rate.
 * @return The refresh interval in seconds.
 */
long read_refresh_seconds(bool is_battery_low = false);

} // namespace board_utils

} // namespace photo_frame

#endif // __FOTO_FRAME_BOARD_UTIL_H__