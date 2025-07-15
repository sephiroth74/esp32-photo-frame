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

#include <Arduino.h>
#include <stdint.h>
#include "config.h"

namespace battery {

/**
 * Calculates the battery percentage based on the voltage.
 * @param v The voltage in millivolts.
 * @return The battery percentage (0-100).
 */
uint8_t calc_battery_percentage(uint32_t v);

typedef struct battery_step {
    uint8_t percent;
    uint16_t voltage;

    constexpr battery_step(uint16_t percent, uint16_t voltage)
        : percent(percent)
        , voltage(voltage)
    {
    }
} battery_step_t;

typedef struct battery_info {
public:
#ifdef SENSOR_MAX1704X
    float cell_voltage; // Voltage in volts
    float charge_rate; // Charge rate in mA
    float percent;
    uint32_t millivolts;
#else
    uint32_t raw_value;
    uint32_t raw_millivolts;
    uint32_t millivolts;
    float percent;
#endif


#ifdef SENSOR_MAX1704X
    constexpr battery_info(float cell_voltage, float charge_rate, float percent)
        : cell_voltage(cell_voltage)
        , charge_rate(charge_rate)
        , percent(percent)
        , millivolts(static_cast<uint32_t>(cell_voltage * 1000)) // Convert volts to millivolts
    {
    }

    constexpr battery_info()
        : battery_info(0.0f, 0.0f, 0.0f) // Default constructor
    {
    }

    constexpr battery_info(const battery_step_t& step)
        : battery_info(step.voltage / 1000.0f, 0.0f, step.percent) // Convert to volts
    {
    }

    constexpr bool operator==(const battery_info& other) const
    {
        return (cell_voltage == other.cell_voltage && charge_rate == other.charge_rate && percent == other.percent);
    }

    static inline constexpr battery_info empty() { return battery_info(0.0f, 0.0f, 0); }

    static inline constexpr battery_info full() { return battery_info(4.2f, 0.0f, 100); }

#else
    constexpr battery_info(uint32_t raw_value, uint32_t raw_millivolts, uint32_t millivolts, float percent)
        : raw_value(raw_value)
        , raw_millivolts(raw_millivolts)
        , millivolts(millivolts)
        , percent(percent)
    {
    }

    constexpr battery_info()
        : raw_value(0)
        , raw_millivolts(0)
        , millivolts(0)
        , percent(0)
    {
    }

    constexpr battery_info(const battery_step_t& step)
        : raw_value(step.voltage)
        , raw_millivolts(step.voltage)
        , millivolts(step.voltage)
        , percent(step.percent)
    {
    }

    constexpr bool operator==(const battery_info& other) const
    {
        return (raw_millivolts == other.raw_millivolts && millivolts == other.millivolts && percent == other.percent);
    }

    static inline constexpr battery_info empty() { return battery_info(0, 0, 0, 0); }

    static inline constexpr battery_info full() { return battery_info(3999, 3999, 3999, 100); }

#endif // SENSOR_MAX1704X

    bool is_low() const;

    bool is_critical() const;

    bool is_empty() const;

    /**
     * Checks if the battery is currently charging.
     * @return True if the battery is charging, false otherwise.
     */
    bool is_charging() const;

} battery_info_t;

// Battery steps for voltage to percentage mapping
constexpr battery_step_t steps[21] = {
    battery_step(0, 3270),
    battery_step(5, 3610),
    battery_step(10, 3690),
    battery_step(15, 3710),
    battery_step(20, 3730),
    battery_step(25, 3750),
    battery_step(30, 3770),
    battery_step(35, 3790),
    battery_step(40, 3800),
    battery_step(45, 3820),
    battery_step(50, 3840),
    battery_step(55, 3850),
    battery_step(60, 3870),
    battery_step(65, 3910),
    battery_step(70, 3950),
    battery_step(75, 3980),
    battery_step(80, 4020),
    battery_step(85, 4080),
    battery_step(90, 4110),
    battery_step(95, 4150),
    battery_step(100, 4200),
};

constexpr uint8_t total_steps = 21;

class BatteryReader {
public:
    uint8_t pin;
    double resistor_ratio;
    uint8_t num_readings;
    uint32_t delay_between_readings;

    constexpr BatteryReader(uint8_t pin,
        double resistor_ratio,
        uint8_t num_readings,
        uint32_t delay)
        : pin(pin)
        , resistor_ratio(resistor_ratio)
        , num_readings(num_readings)
        , delay_between_readings(delay)
    {
    }

    /**
     * Initializes the battery reader by setting the pin mode and ADC attenuation.
     */
    void init() const;

    /**
     * Reads the battery voltage and returns a battery_info_t structure containing
     * the raw value, millivolts, and percentage.
     * @return A battery_info_t structure with the battery information.
     */
    battery_info_t read() const;

}; // BatteryReader

} // namespace battery

#endif // __BATTERY_READER_H__