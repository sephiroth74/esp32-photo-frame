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

#ifndef RGB_STATUS_H
#define RGB_STATUS_H

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "config.h"

/**
 * @brief RGB Status System for FeatherS3 Built-in NeoPixel
 *
 * Provides comprehensive visual feedback for different system operations using colors and effects.
 * Runs in a separate FreeRTOS task for smooth transitions and non-blocking operation.
 *
 * @version v0.5.0
 * @date 2025-01-21
 *
 * Features:
 * - 14 predefined status states with optimized colors and effects
 * - Battery-aware power management with automatic brightness scaling
 * - FreeRTOS task-based operation for smooth animations (50Hz update rate)
 * - Ultra power-efficient design (2-5mA additional current consumption)
 * - Complete shutdown capability for deep sleep power conservation
 * - Hardware power control via GPIO39 (LDO2) for FeatherS3
 *
 * Status States:
 * - STARTING: White pulse during system startup (3s duration)
 * - WIFI_CONNECTING: Blue pulse while connecting to WiFi
 * - WIFI_FAILED: Red slow blink when connection fails
 * - WEATHER_FETCHING: Green pulse while fetching weather data
 * - SD_READING/SD_WRITING: Orange/Yellow pulse during SD operations
 * - GOOGLE_DRIVE: Cyan pulse during Google Drive operations
 * - DOWNLOADING: Purple pulse during file downloads
 * - RENDERING: Pink solid during display rendering
 * - BATTERY_LOW: Red slow blink for critical battery warning
 * - ERROR: Red fast blink for system errors (maximum brightness)
 * - SLEEP_PREP: Dim white fade-out before deep sleep (2s duration)
 * - IDLE: Dark blue low-brightness idle state
 * - CUSTOM: User-defined colors and effects
 *
 * Power Management:
 * - Normal operation: 48-64 brightness levels (ultra power-efficient)
 * - Low battery: 32 brightness (automatic reduction)
 * - Critical battery: Complete RGB disable after warning
 * - Deep sleep: Full shutdown with task termination
 */

// Color definitions (RGB values) - always available
struct RGBColor {
    uint8_t r, g, b;

    RGBColor(uint8_t red = 0, uint8_t green = 0, uint8_t blue = 0)
        : r(red), g(green), b(blue) {}

    uint32_t toUint32() const {
        return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
    }
};

// System operation states - always available for macro compatibility
enum class SystemState {
    IDLE,            // Off/Dark blue - system idle
    STARTING,        // White pulse - system starting up
    OTA_UPDATING,    // Dark Green - performing OTA update
    WIFI_CONNECTING, // Blue pulse - connecting to WiFi
    WIFI_FAILED,     // Red - WiFi connection failed
    WEATHER_FETCHING,// Light blue - fetching weather data
    SD_READING,      // Orange - reading from SD card
    SD_WRITING,      // Yellow - writing to SD card
    GOOGLE_DRIVE,    // Cyan - Google Drive operations
    DOWNLOADING,     // Purple pulse - downloading files
    RENDERING,       // Pink - rendering to display
    BATTERY_LOW,     // Red slow blink - battery low warning
    ERROR,           // Red fast blink - system error
    SLEEP_PREP,      // Dim white fade - preparing for sleep
    CUSTOM           // User-defined color
};

// Effect types for color transitions - always available
enum class RGBEffect {
    SOLID,           // Steady color
    PULSE,           // Breathing effect
    BLINK_SLOW,      // Slow blinking
    BLINK_FAST,      // Fast blinking
    FADE_IN,         // Fade in once
    FADE_OUT,        // Fade out once
    RAINBOW,         // Rainbow cycle
    OFF              // Turn off LED
};

// Predefined colors - always available
namespace RGBColors {
    const RGBColor OFF(0, 0, 0);
    const RGBColor WHITE(255, 255, 255);
    const RGBColor RED(255, 0, 0);
    const RGBColor GREEN(0, 255, 0);
    const RGBColor DARK_GREEN(0, 128, 0);
    const RGBColor BLUE(0, 0, 255);
    const RGBColor LIGHT_BLUE(128, 192, 255);
    const RGBColor YELLOW(255, 255, 0);
    const RGBColor ORANGE(255, 128, 0);
    const RGBColor CYAN(0, 255, 255);
    const RGBColor PURPLE(128, 0, 255);
    const RGBColor PINK(255, 0, 128);
    const RGBColor DARK_BLUE(0, 0, 64);
    const RGBColor DIM_WHITE(128, 128, 128);
}

#ifdef RGB_STATUS_ENABLED
// Forward declarations
class RGBStatus;

// Status configuration structure
struct StatusConfig {
    SystemState state;
    RGBColor color;
    RGBEffect effect;
    uint16_t duration_ms;    // Duration for temporary effects (0 = indefinite)
    uint8_t brightness;      // 0-255

    StatusConfig(SystemState s, RGBColor c, RGBEffect e = RGBEffect::SOLID,
                uint16_t dur = 0, uint8_t bright = 64)
        : state(s), color(c), effect(e), duration_ms(dur), brightness(bright) {}
};

/**
 * @brief RGB Status Controller Class
 *
 * Controls the FeatherS3 built-in NeoPixel LED for system status indication.
 * Supports various colors, effects, and FreeRTOS task-based operation.
 */
class RGBStatus {
private:
    Adafruit_NeoPixel* pixels;
    TaskHandle_t rgbTaskHandle;

    // Current status
    SystemState currentState;
    StatusConfig currentConfig;
    bool enabled;
    bool taskRunning;

    // Effect timing
    unsigned long lastUpdate;
    uint16_t effectStep;
    uint8_t currentBrightness;

    // Static task function for FreeRTOS
    static void rgbTask(void* parameter);

    // Internal methods
    void updateEffect();
    void setPixelColor(const RGBColor& color, uint8_t brightness);
    uint8_t calculatePulse(uint16_t step, uint16_t period);
    RGBColor rainbow(uint8_t pos);

public:
    RGBStatus();
    ~RGBStatus();

    // Initialization
    /**
     * @brief Initialize the RGB status system
     * @return true if initialization successful, false otherwise
     *
     * Initializes the NeoPixel LED, powers on the LED via GPIO39 (LDO2),
     * and creates the FreeRTOS task for RGB control. Shows STARTING status
     * for 1 second after successful initialization.
     */
    bool begin();

    /**
     * @brief Shutdown the RGB status system
     *
     * Terminates the FreeRTOS task, turns off the NeoPixel LED,
     * and powers down the LED via GPIO39 to save power during deep sleep.
     */
    void end();

    // Control methods
    /**
     * @brief Set the system status state
     * @param state The system state to display
     * @param duration_ms Duration in milliseconds (0 = indefinite)
     *
     * Sets the RGB LED to display a predefined status state with
     * associated color, effect, and brightness. If duration is specified,
     * the status will automatically return to IDLE after the timeout.
     */
    void setState(SystemState state, uint16_t duration_ms = 0);

    /**
     * @brief Set a custom color and effect
     * @param color RGB color to display
     * @param effect Animation effect to apply
     * @param duration_ms Duration in milliseconds (0 = indefinite)
     * @param brightness Brightness level (0-255, default 96)
     *
     * Sets the RGB LED to display a custom color with the specified
     * effect and brightness. Useful for application-specific status
     * indications not covered by predefined states.
     */
    void setCustomColor(const RGBColor& color, RGBEffect effect = RGBEffect::SOLID,
                       uint16_t duration_ms = 0, uint8_t brightness = 64);

    /**
     * @brief Set the global brightness level
     * @param brightness Brightness level (0-255)
     *
     * Sets the brightness for subsequent status displays. Used for
     * battery-aware power management to reduce current consumption
     * when battery is low.
     */
    void setBrightness(uint8_t brightness);

    /**
     * @brief Enable or disable the RGB status system
     * @param enabled true to enable, false to disable
     *
     * When disabled, the RGB LED is turned off but the FreeRTOS task
     * continues running. Used for power conservation during critical
     * battery conditions.
     */
    void enable(bool enabled = true);

    /**
     * @brief Disable the RGB status system
     *
     * Convenience method equivalent to enable(false).
     */
    void disable() { enable(false); }

    /**
     * @brief Turn off the RGB LED immediately
     *
     * Turns off the LED and sets state to IDLE. The system remains
     * enabled and can be activated again with setState() or setCustomColor().
     */
    void turnOff();

    // Status queries
    SystemState getCurrentState() const { return currentState; }
    bool isEnabled() const { return enabled; }
    bool isTaskRunning() const { return taskRunning; }

    // Predefined status configurations
    static const StatusConfig STATUS_CONFIGS[];
    static const size_t NUM_STATUS_CONFIGS;
};

// Global RGB status instance (defined in rgb_status.cpp)
extern RGBStatus rgbStatus;
#endif // RGB_STATUS_ENABLED

/**
 * @brief Convenience macros for common RGB status operations
 *
 * These macros provide simplified access to the global rgbStatus instance
 * for easy integration throughout the application code. When RGB_STATUS_ENABLED
 * is not defined, all macros become no-ops.
 *
 * Usage Examples:
 * @code
 * RGB_SET_STATE(WIFI_CONNECTING);              // Show WiFi connecting
 * RGB_SET_STATE_TIMED(WIFI_CONNECTED, 2000);  // Show connected for 2s
 * RGB_SET_CUSTOM(RED, BLINK_FAST);             // Custom red fast blink
 * RGB_OFF();                                   // Turn off LED
 * RGB_DISABLE();                               // Disable for power saving
 * @endcode
 */
#ifdef RGB_STATUS_ENABLED
#define RGB_SET_STATE(state) rgbStatus.setState(SystemState::state)
#define RGB_SET_STATE_TIMED(state, ms) rgbStatus.setState(SystemState::state, ms)
#define RGB_SET_CUSTOM(color, effect) rgbStatus.setCustomColor(RGBColors::color, RGBEffect::effect)
#define RGB_OFF() rgbStatus.turnOff()
#define RGB_ENABLE() rgbStatus.enable()
#define RGB_DISABLE() rgbStatus.disable()
#else
// RGB system disabled - all macros become no-ops
#define RGB_SET_STATE(state) do {} while(0)
#define RGB_SET_STATE_TIMED(state, ms) do {} while(0)
#define RGB_SET_CUSTOM(color, effect) do {} while(0)
#define RGB_OFF() do {} while(0)
#define RGB_ENABLE() do {} while(0)
#define RGB_DISABLE() do {} while(0)
#endif // RGB_STATUS_ENABLED

#endif // RGB_STATUS_H