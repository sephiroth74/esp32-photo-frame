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

#ifndef __BATTERY_READER_H__
#define __BATTERY_READER_H__

#include "config.h"
#include <Arduino.h>
#include <stdint.h>

namespace photo_frame {

/**
 * Calculates the battery percentage based on the voltage.
 * @param v The voltage in millivolts.
 * @return The battery percentage (0-100).
 */
uint8_t calc_battery_percentage(uint32_t v);

/**
 * @brief Structure representing a battery voltage-to-percentage mapping step.
 *
 * This structure is used to define the relationship between battery voltage
 * and percentage levels for accurate battery level calculation.
 */
typedef struct battery_step {
    uint8_t percent;  ///< Battery percentage (0-100)
    uint16_t voltage; ///< Corresponding voltage in millivolts

    /**
     * @brief Constructor for battery_step.
     * @param percent Battery percentage (0-100)
     * @param voltage Corresponding voltage in millivolts
     */
    constexpr battery_step(uint16_t percent, uint16_t voltage) :
        percent(percent),
        voltage(voltage) {}
} battery_step_t;

/**
 * @brief Structure containing comprehensive battery information.
 *
 * This structure holds all relevant battery data including voltage, percentage,
 * and charging status. The structure adapts based on whether the MAX1704X sensor
 * is being used or analog voltage reading is employed.
 */
typedef struct battery_info {
  public:
#ifdef USE_SENSOR_MAX1704X
    float cell_voltage;  ///< Battery cell voltage in volts (MAX1704X sensor)
    float charge_rate;   ///< Battery charge rate in mA (MAX1704X sensor)
    float percent;       ///< Battery percentage (0-100)
    uint32_t millivolts; ///< Battery voltage in millivolts
#else
    uint32_t raw_value;      ///< Raw ADC reading value
    uint32_t raw_millivolts; ///< Raw voltage reading in millivolts
    uint32_t millivolts;     ///< Corrected battery voltage in millivolts
    float percent;           ///< Battery percentage (0-100)
#endif

#ifdef USE_SENSOR_MAX1704X
    /**
     * @brief Constructor for MAX1704X sensor battery info.
     * @param cell_voltage Battery cell voltage in volts
     * @param charge_rate Battery charge rate in mA
     * @param percent Battery percentage (0-100)
     */
    constexpr battery_info(float cell_voltage, float charge_rate, float percent) :
        cell_voltage(cell_voltage),
        charge_rate(charge_rate),
        percent(percent),
        millivolts(static_cast<uint32_t>(cell_voltage * 1000)) {}

    /**
     * @brief Default constructor initializing all values to zero.
     */
    constexpr battery_info() : battery_info(0.0f, 0.0f, 0.0f) {}

    /**
     * @brief Constructor from battery_step_t for MAX1704X sensor.
     * @param step Battery step containing voltage and percentage data
     */
    constexpr battery_info(const battery_step_t& step) :
        battery_info(step.voltage / 1000.0f, 0.0f, step.percent) {}

    /**
     * @brief Equality operator for battery_info comparison.
     * @param other Another battery_info instance to compare with
     * @return True if both instances are equal, false otherwise
     */
    constexpr bool operator==(const battery_info& other) const {
        return (cell_voltage == other.cell_voltage && charge_rate == other.charge_rate &&
                percent == other.percent);
    }

    /**
     * @brief Creates a battery_info representing an empty battery.
     * @return battery_info with zero values
     */
    static inline constexpr battery_info empty() { return battery_info(0.0f, 0.0f, 0); }

    /**
     * @brief Creates a battery_info representing a full battery.
     * @return battery_info with full charge values (4.2V, 100%)
     */
    static inline constexpr battery_info full() { return battery_info(4.2f, 0.0f, 100); }

#else
    /**
     * @brief Constructor for analog battery reading.
     * @param raw_value Raw ADC reading value
     * @param raw_millivolts Raw voltage reading in millivolts
     * @param millivolts Corrected battery voltage in millivolts
     * @param percent Battery percentage (0-100)
     */
    constexpr battery_info(uint32_t raw_value,
                           uint32_t raw_millivolts,
                           uint32_t millivolts,
                           float percent) :
        raw_value(raw_value),
        raw_millivolts(raw_millivolts),
        millivolts(millivolts),
        percent(percent) {}

    /**
     * @brief Default constructor initializing all values to zero.
     */
    constexpr battery_info() : raw_value(0), raw_millivolts(0), millivolts(0), percent(0) {}

    /**
     * @brief Constructor from battery_step_t for analog reading.
     * @param step Battery step containing voltage and percentage data
     */
    constexpr battery_info(const battery_step_t& step) :
        raw_value(step.voltage),
        raw_millivolts(step.voltage),
        millivolts(step.voltage),
        percent(step.percent) {}

    /**
     * @brief Equality operator for battery_info comparison.
     * @param other Another battery_info instance to compare with
     * @return True if both instances are equal, false otherwise
     */
    constexpr bool operator==(const battery_info& other) const {
        return (raw_millivolts == other.raw_millivolts && millivolts == other.millivolts &&
                percent == other.percent);
    }

    /**
     * @brief Creates a battery_info representing an empty battery.
     * @return battery_info with zero values
     */
    static inline constexpr battery_info empty() { return battery_info(0, 0, 0, 0); }

    /**
     * @brief Creates a battery_info representing a full battery.
     * @return battery_info with full charge values
     */
    static inline constexpr battery_info full() { return battery_info(3999, 3999, 3999, 100); }

#endif // USE_SENSOR_MAX1704X

    /**
     * @brief Checks if the battery level is low.
     * @return True if battery percentage is at or below the low threshold
     */
    bool is_low() const;

    /**
     * @brief Checks if the battery level is critical.
     * @return True if battery percentage is at or below the critical threshold
     */
    bool is_critical() const;

    /**
     * @brief Checks if the battery is considered empty.
     * @return True if battery percentage is at or below the empty threshold
     */
    bool is_empty() const;

    /**
     * Checks if the battery is currently charging.
     * @return True if the battery is charging, false otherwise.
     */
    bool is_charging() const;

} battery_info_t;

/**
 * @brief Battery voltage to percentage mapping steps.
 *
 * This array defines the relationship between battery voltage (in millivolts)
 * and corresponding percentage levels. Used for accurate battery level calculation
 * based on voltage readings.
 */
extern const battery_step_t steps[21];

/// Total number of battery mapping steps
extern const uint8_t total_steps;

/**
 * @brief Battery reader class for monitoring battery voltage and percentage.
 *
 * This class provides functionality to read battery information using either
 * the MAX1704X sensor or analog voltage divider circuit. It handles initialization,
 * voltage reading, and percentage calculation.
 */
class battery_reader {
  public:
#ifndef USE_SENSOR_MAX1704X
    uint8_t pin;                     ///< Analog pin for battery voltage reading
    double resistor_ratio;           ///< Voltage divider ratio (R1/(R1+R2))
    uint8_t num_readings;            ///< Number of readings to average
    uint32_t delay_between_readings; ///< Delay between readings in milliseconds

    /**
     * @brief Constructor for analog battery reader.
     * @param pin Analog pin for battery voltage reading
     * @param resistor_ratio Voltage divider ratio (R1/(R1+R2))
     * @param num_readings Number of readings to average for stability
     * @param delay Delay between readings in milliseconds
     */
    constexpr battery_reader(uint8_t pin,
                             double resistor_ratio,
                             uint8_t num_readings,
                             uint32_t delay) :
        pin(pin),
        resistor_ratio(resistor_ratio),
        num_readings(num_readings),
        delay_between_readings(delay) {}

#else
    /**
     * @brief Constructor for MAX1704X sensor battery reader.
     * No parameters needed as the sensor handles everything internally.
     */
    constexpr battery_reader() {}

#endif // USE_SENSOR_MAX1704X

    /**
     * @brief Initializes the battery reader.
     *
     * For analog reading: Sets the pin mode and ADC attenuation.
     * For MAX1704X sensor: Initializes the I2C communication and sensor.
     */
    void init() const;

    /**
     * @brief Reads the current battery information.
     *
     * Performs battery voltage reading and calculates percentage based on
     * the configured method (analog or MAX1704X sensor).
     *
     * @return battery_info_t structure containing voltage, percentage, and status
     */
    battery_info_t read() const;

}; // battery_reader

} // namespace photo_frame

#endif // __BATTERY_READER_H__