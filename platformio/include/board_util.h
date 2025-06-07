#ifndef __FOTO_FRAME_BOARD_UTIL_H__
#define __FOTO_FRAME_BOARD_UTIL_H__
#include "config.h"
#include "errors.h"
#include <Arduino.h>

namespace photo_frame {

/**
 * Prints the reason for the last wakeup from deep sleep.
 */
void print_wakeup_reason();

/**
 * Enters deep sleep mode, disabling peripherals and LEDs to save power.
 */
void enter_deep_sleep();

/**
 * Prints various statistics about the board, including heap size, flash size, free heap, free
 * flash, PSRAM, chip model, revision, cores, and CPU frequency.
 */
void print_board_stats();

/**
 * Disables the RGB LED by setting its pins to LOW.
 */
void disable_rgb_led();

/**
 * Toggles the RGB LED to the specified color.
 * @param red   The state of the red LED (HIGH or LOW).
 * @param green The state of the green LED (HIGH or LOW).
 * @param blue  The state of the blue LED (HIGH or LOW).
 */
void toggle_rgb_led(uint8_t red = LOW, uint8_t green = LOW, uint8_t blue = LOW);

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
void blink_builtin_led(int count, uint32_t on_ms = 200, uint32_t off_ms = 400);

/**
 * Blinks the built-in LED according to the specified error.
 * @param error The error containing the message, code, and blink count.
 */
void blink_builtin_led(photo_frame_error_t error);

} // namespace photo_frame

#endif // __FOTO_FRAME_BOARD_UTIL_H__