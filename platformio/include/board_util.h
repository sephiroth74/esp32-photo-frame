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

#ifdef WAKEUP_EXT0
#include "driver/rtc_io.h" // For RTC GPIO functions needed for EXT0 wakeup
#endif

namespace photo_frame {

namespace board_utils {

/**
 * @brief Gets and prints the reason for the last wakeup from deep sleep.
 *
 * This function retrieves the cause of the ESP32 wakeup from deep sleep mode
 * and prints it to the Serial console for debugging purposes.
 *
 * @return The wakeup cause as an esp_sleep_wakeup_cause_t enum value
 * @note This function also prints the wakeup reason to Serial output
 */
esp_sleep_wakeup_cause_t get_wakeup_reason();

/**
 * @brief Converts the wakeup reason to a human-readable string.
 *
 * Takes an ESP32 wakeup cause enum and converts it to a descriptive string
 * that can be used for logging or display purposes.
 *
 * @param wakeup_reason The reason for waking up from deep sleep
 * @param buffer The buffer to store the string representation
 * @param buffer_size The size of the buffer (must be large enough for the string)
 * @note The buffer should be at least 32 characters to accommodate all possible strings
 */
void get_wakeup_reason_string(esp_sleep_wakeup_cause_t wakeup_reason,
                              char* buffer,
                              size_t buffer_size);

/**
 * @brief Enters deep sleep mode, disabling peripherals and LEDs to save power.
 *
 * This function prepares the ESP32 for deep sleep by disabling various peripherals,
 * turning off LEDs, and configuring wakeup sources based on the board configuration.
 * The function configures EXT0 or EXT1 wakeup sources and timer wakeup as appropriate.
 *
 * @param wakeup_reason The reason for the previous wakeup, used for sleep duration calculation
 * @note This function does not return as the ESP32 enters deep sleep
 * @note Wakeup sources are configured based on compile-time flags (WAKEUP_EXT0/WAKEUP_EXT1)
 */
void enter_deep_sleep(esp_sleep_wakeup_cause_t wakeup_reason);

/**
 * @brief Prints comprehensive board statistics to Serial console.
 *
 * Outputs detailed information about the ESP32 board including:
 * - Heap size and free heap memory
 * - Flash size and available flash memory
 * - PSRAM availability and size
 * - Chip model, revision, and number of cores
 * - Current CPU frequency
 *
 * @note Information is printed to Serial console for debugging purposes
 */
void print_board_stats();

/**
 * @brief Prints the pin assignments for the current board configuration.
 *
 * Displays the GPIO pin assignments used by various peripherals on the board,
 * including display pins, sensor pins, LED pins, and wakeup pins. Pin assignments
 * vary based on the board configuration defined in the config files.
 *
 * @note Pin information is printed to Serial console
 */
void print_board_pins();

/**
 * @brief Disables the RGB LED by setting all color pins to LOW.
 *
 * Turns off all RGB LED components (red, green, blue) by setting their
 * corresponding GPIO pins to LOW state. Only available on boards with
 * RGB LED support.
 *
 * @note This function only operates if RGB LED pins are defined in the board config
 */
void disable_rgb_led();

/**
 * @brief Toggles the RGB LED to display the specified color combination.
 *
 * Controls the RGB LED by setting the individual color components on or off.
 * Multiple colors can be combined to create different colors (e.g., red+green=yellow).
 *
 * @param red If true, turns on the red LED component
 * @param green If true, turns on the green LED component
 * @param blue If true, turns on the blue LED component
 * @note This function only operates if RGB LED pins are defined in the board config
 * @note All parameters default to false (LED off)
 */
void toggle_rgb_led(bool red = false, bool green = false, bool blue = false);

/**
 * @brief Disables the built-in LED by setting its pin to LOW.
 *
 * Turns off the ESP32's built-in LED (usually connected to GPIO 2) by
 * setting the pin to LOW state.
 *
 * @note This function only operates if BUILTIN_LED_PIN is defined
 */
void disable_built_in_led();

/**
 * @brief Blinks the built-in LED a specified number of times.
 *
 * Creates a visual indicator by blinking the built-in LED with configurable
 * timing. Useful for debugging and status indication.
 *
 * @param count The number of times to blink the LED
 * @param on_ms The duration in milliseconds to keep the LED on (default: 100ms)
 * @param off_ms The duration in milliseconds to keep the LED off (default: 300ms)
 * @note This function blocks execution during the blinking sequence
 */
void blink_builtin_led(int count, unsigned long on_ms = 100, unsigned long off_ms = 300);

/**
 * @brief Blinks the built-in LED according to error code for debugging.
 *
 * Uses the error code to determine the number of blinks, providing a visual
 * indication of different error types. The LED blinks a number of times
 * corresponding to the error code.
 *
 * @param error The error containing the code used for blink count
 * @note Blink count is derived from the error code modulo 10
 * @note This function blocks execution during the blinking sequence
 */
void blink_builtin_led(photo_frame_error_t error);

/**
 * @brief Blinks the RGB LED with specified color and timing pattern.
 *
 * Creates a visual indicator using the RGB LED with configurable color
 * and timing. Useful for status indication and debugging with color coding.
 *
 * @param count The number of times to blink the LED
 * @param red If true, includes red component in the blink color
 * @param green If true, includes green component in the blink color
 * @param blue If true, includes blue component in the blink color
 * @param on_ms The duration in milliseconds to keep the LED on (default: 100ms)
 * @param off_ms The duration in milliseconds to keep the LED off (default: 300ms)
 * @note This function blocks execution during the blinking sequence
 * @note Only available on boards with RGB LED support
 */
void blink_rgb_led(uint32_t count,
                   bool red,
                   bool green,
                   bool blue,
                   unsigned long on_ms  = 100,
                   unsigned long off_ms = 300);

/**
 * @brief Blinks the RGB LED with color coding based on error category.
 *
 * Uses the error category to determine the LED color, providing visual
 * feedback about different types of errors. Different error categories
 * map to different colors for easy identification.
 *
 * @param error The error containing category and code information
 * @note Color mapping: Network=Red, Storage=Blue, Battery=Yellow, etc.
 * @note Blink count is derived from the error code
 * @note This function blocks execution during the blinking sequence
 */
void blink_rgb_led(photo_frame_error_t error);

/**
 * @brief Reads the refresh interval from potentiometer, adjusting for battery level.
 *
 * Determines the display refresh interval by reading an analog potentiometer value
 * and mapping it to a time range. The interval can be adjusted based on battery
 * level to conserve power when the battery is low.
 *
 * @param is_battery_low If true, may increase the refresh interval to save power
 * @return The refresh interval in seconds
 * @note Actual implementation depends on board configuration and potentiometer presence
 * @note Returns a default value if no potentiometer is configured
 */
long read_refresh_seconds(bool is_battery_low = false);

} // namespace board_utils

} // namespace photo_frame

#endif // __FOTO_FRAME_BOARD_UTIL_H__