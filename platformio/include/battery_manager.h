// ESP32 Photo Frame
// Copyright (C) 2025 Alessandro Crugnola
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#ifndef __PHOTO_FRAME_BATTERY_MANAGER_H__
#define __PHOTO_FRAME_BATTERY_MANAGER_H__

#include "config.h"
#include "errors.h"
#include <Arduino.h>
#include <stdint.h>

namespace photo_frame {

/**
 * @brief Structure representing a battery voltage-to-percentage mapping step.
 *
 * This structure is used to define the relationship between battery voltage
 * and percentage levels for accurate battery level calculation.
 */
struct BatteryMappingStep {
    uint8_t percent;  ///< Battery percentage (0-100)
    uint16_t voltage; ///< Corresponding voltage in millivolts

    /**
     * @brief Constructor for BatteryMappingStep.
     * @param percent Battery percentage (0-100)
     * @param voltage Corresponding voltage in millivolts
     */
    constexpr BatteryMappingStep(uint16_t percent, uint16_t voltage) :
        percent(percent),
        voltage(voltage) {}
};

/**
 * @brief Structure containing comprehensive battery information.
 *
 * This structure holds all relevant battery data including voltage, percentage,
 * and charging status. The structure adapts based on whether the MAX1704X sensor
 * is being used or analog voltage reading is employed.
 */
struct BatteryInfo {
  public:
#ifdef USE_SENSOR_MAX1704X
    float cellVoltage;   ///< Battery cell voltage in volts (MAX1704X sensor)
    float chargeRate;    ///< Battery charge rate in mA (MAX1704X sensor)
    float percent;       ///< Battery percentage (0-100)
    uint32_t millivolts; ///< Battery voltage in millivolts
#else
    uint32_t rawValue;      ///< Raw ADC reading value
    uint32_t rawMillivolts; ///< Raw voltage reading in millivolts
    uint32_t millivolts;    ///< Corrected battery voltage in millivolts
    float percent;          ///< Battery percentage (0-100)
#endif

#ifdef USE_SENSOR_MAX1704X
    /**
     * @brief Constructor for MAX1704X sensor battery info.
     * @param cell_voltage Battery cell voltage in volts
     * @param charge_rate Battery charge rate in mA
     * @param percent Battery percentage (0-100)
     */
    constexpr BatteryInfo(float cellVoltage, float chargeRate, float percent) :
        cellVoltage(cellVoltage),
        chargeRate(chargeRate),
        percent(percent),
        millivolts(static_cast<uint32_t>(cellVoltage * 1000)) {}

    /**
     * @brief Update the BatteryInfo with new sensor reading values.
     * @param cellVoltage Battery cell voltage in volts
     * @param chargeRate Battery charge rate in mA
     * @param percent Battery percentage (0-100)
     */
    void updateFromSensor(float cellVoltage, float chargeRate, float percent) {
        this->cellVoltage = cellVoltage;
        this->chargeRate  = chargeRate;
        this->percent     = percent;
        this->millivolts  = static_cast<uint32_t>(cellVoltage * 1000);
    }

    /**
     * @brief Set the BatteryInfo to represent an empty battery state.
     */
    void setEmpty() {
        this->cellVoltage = 0.0f;
        this->chargeRate  = 0.0f;
        this->percent     = 0.0f;
        this->millivolts  = 0;
    }

    /**
     * @brief Set the BatteryInfo to represent a full battery state.
     */
    void setFull() {
        this->cellVoltage = 4.2f;
        this->chargeRate  = 0.0f;
        this->percent     = 100.0f;
        this->millivolts  = 4200;
    }

    /**
     * @brief Default constructor initializing all values to zero.
     */
    constexpr BatteryInfo() : BatteryInfo(0.0f, 0.0f, 0.0f) {}

    /**
     * @brief Constructor from BatteryMappingStep for MAX1704X sensor.
     * @param step Battery step containing voltage and percentage data
     */
    constexpr BatteryInfo(const BatteryMappingStep& step) :
        BatteryInfo(step.voltage / 1000.0f, 0.0f, step.percent) {}

    /**
     * @brief Equality operator for BatteryInfo comparison.
     * @param other Another BatteryInfo instance to compare with
     * @return True if both instances are equal, false otherwise
     */
    constexpr bool operator==(const BatteryInfo& other) const {
        return (cellVoltage == other.cellVoltage && chargeRate == other.chargeRate &&
                percent == other.percent);
    }

    /**
     * @brief Creates a BatteryInfo representing an empty battery.
     * @return BatteryInfo with zero values
     */
    static inline constexpr BatteryInfo empty() { return BatteryInfo(0.0f, 0.0f, 0); }

    /**
     * @brief Creates a BatteryInfo representing a full battery.
     * @return BatteryInfo with full charge values (4.2V, 100%)
     */
    static inline constexpr BatteryInfo full() { return BatteryInfo(4.2f, 0.0f, 100); }

#else
    /**
     * @brief Constructor for analog battery reading.
     * @param raw_value Raw ADC reading value
     * @param raw_millivolts Raw voltage reading in millivolts
     * @param millivolts Corrected battery voltage in millivolts
     * @param percent Battery percentage (0-100)
     */
    constexpr BatteryInfo(uint32_t rawValue,
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
    constexpr BatteryInfo() : raw_value(0), raw_millivolts(0), millivolts(0), percent(0) {}

    /**
     * @brief Constructor from BatteryMappingStep for analog reading.
     * @param step Battery step containing voltage and percentage data
     */
    constexpr BatteryInfo(const BatteryMappingStep& step) :
        raw_value(step.voltage),
        raw_millivolts(step.voltage),
        millivolts(step.voltage),
        percent(step.percent) {}

    /**
     * @brief Update the BatteryInfo with new analog reading values.
     * @param rawValue New raw ADC reading value
     * @param rawMillivolts New raw voltage reading in millivolts
     * @param millivolts New corrected battery voltage in millivolts
     * @param percent New battery percentage (0-100)
     */
    void updateFromAnalog(uint32_t rawValue,
                          uint32_t rawMillivolts,
                          uint32_t millivolts,
                          float percent) {
        this->rawValue      = rawValue;
        this->rawMillivolts = rawMillivolts;
        this->millivolts    = millivolts;
        this->percent       = percent;
    }

    /**
     * @brief Set the BatteryInfo to represent a full battery state.
     */
    void setFull() {
        this->rawValue      = 4095; // Assuming 12-bit ADC
        this->rawMillivolts = 4200; // Max voltage in millivolts
        this->millivolts    = 4200;
        this->percent       = 100;
    }

    /**
     * @brief Set the BatteryInfo to represent an empty battery state.
     */
    void setEmpty() {
        this->rawValue      = 0;
        this->rawMillivolts = 0;
        this->millivolts    = 0;
        this->percent       = 0;
    }

    /**
     * @brief Equality operator for BatteryInfo comparison.
     * @param other Another BatteryInfo instance to compare with
     * @return True if both instances are equal, false otherwise
     */
    constexpr bool operator==(const BatteryInfo& other) const {
        return (raw_millivolts == other.raw_millivolts && millivolts == other.millivolts &&
                percent == other.percent);
    }

    /**
     * @brief Creates a BatteryInfo representing an empty battery.
     * @return BatteryInfo with zero values
     */
    static inline constexpr BatteryInfo empty() { return BatteryInfo(0, 0, 0, 0); }

    /**
     * @brief Creates a BatteryInfo representing a full battery.
     * @return BatteryInfo with full charge values
     */
    static inline constexpr BatteryInfo full() { return BatteryInfo(3999, 3999, 3999, 100); }

#endif // USE_SENSOR_MAX1704X

    /**
     * @brief Checks if the battery level is low.
     * @return True if battery percentage is at or below the low threshold
     */
    bool isLow() const;

    /**
     * @brief Checks if the battery level is critical.
     * @return True if battery percentage is at or below the critical threshold
     */
    bool isCritical() const;

    /**
     * @brief Checks if the battery is considered empty.
     * @return True if battery percentage is at or below the empty threshold
     */
    bool isEmpty() const;

    /**
     * Checks if the battery is currently charging.
     * @return True if the battery is charging, false otherwise.
     */
    bool isCharging() const;
};

class BatteryManager {
  public:
    static BatteryManager& getInstance();

    BatteryManager() = default;

    photo_frame::photo_frame_error_t init();

    photo_frame::photo_frame_error_t read(BatteryInfo& info) const;

  private:
    bool initialized_ = false; ///< Flag to track if the battery manager has been initialized

#ifdef USE_SENSOR_MAX1704X
    photo_frame::photo_frame_error_t readFromSensor(BatteryInfo& info) const;
#else
    photo_frame::photo_frame_error_t readFromAnalog(BatteryInfo& info) const;
    /**
     * Calculates the battery percentage based on the voltage.
     * @param v The voltage in millivolts.
     * @return The battery percentage (0-100).
     */
    static uint8_t calculatePercentage(uint32_t v);
#endif // USE_SENSOR_MAX1704X
};

} // namespace photo_frame

#endif // __PHOTO_FRAME_BATTERY_MANAGER_H__