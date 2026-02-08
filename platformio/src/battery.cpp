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

#include "battery.h"
#include "config.h"

#include <Arduino.h>
#include <cmath>
#if defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32H2)
#include <esp_adc/adc_cali.h>
#include <esp_adc/adc_cali_scheme.h>
#include <esp_adc/adc_oneshot.h>
#else
#include <driver/adc.h>
#include <esp_adc_cal.h>
#endif

#ifdef USE_SENSOR_MAX1704X
#include <Adafruit_MAX1704X.h>
#include <Wire.h>

Adafruit_MAX17048 max1704x;

#define TheWire Wire

#endif // USE_SENSOR_MAX1704X

namespace photo_frame {

const BatteryMappingStep steps[21] = {
    BatteryMappingStep(0, 3270),
    BatteryMappingStep(5, 3610),
    BatteryMappingStep(10, 3690),
    BatteryMappingStep(15, 3710),
    BatteryMappingStep(20, 3730),
    BatteryMappingStep(25, 3750),
    BatteryMappingStep(30, 3770),
    BatteryMappingStep(35, 3790),
    BatteryMappingStep(40, 3800),
    BatteryMappingStep(45, 3820),
    BatteryMappingStep(50, 3840),
    BatteryMappingStep(55, 3850),
    BatteryMappingStep(60, 3870),
    BatteryMappingStep(65, 3910),
    BatteryMappingStep(70, 3950),
    BatteryMappingStep(75, 3980),
    BatteryMappingStep(80, 4020),
    BatteryMappingStep(85, 4080),
    BatteryMappingStep(90, 4110),
    BatteryMappingStep(95, 4150),
    BatteryMappingStep(100, 4200),
};

const uint8_t total_steps = 21;

uint8_t calcBatteryPercentage(uint32_t v)
{
    if (v >= steps[total_steps - 1].voltage)
        return steps[total_steps - 1].percent;
    if (v <= steps[0].voltage)
        return steps[0].percent;

    for (int8_t i = total_steps - 1; i > 0; i--) {
        BatteryMappingStep current = steps[i];
        BatteryMappingStep previous = steps[i - 1];
        if (v >= previous.voltage && v <= current.voltage) {
            return map(v, previous.voltage, current.voltage, previous.percent, current.percent);
        }
    }
    return 0;
} // calcBatteryPercentage

bool BatteryInfo::is_low() const { return percent <= BATTERY_PERCENT_LOW; }

bool BatteryInfo::is_critical() const { return percent <= BATTERY_PERCENT_CRITICAL; }

bool BatteryInfo::is_empty() const { return percent <= BATTERY_PERCENT_EMPTY; }

#ifdef USE_SENSOR_MAX1704X
bool BatteryInfo::is_charging() const { return percent > 100; }
#else
bool BatteryInfo::is_charging() const { return millivolts > BATTERY_CHARGING_MILLIVOLTS; }
#endif // USE_SENSOR_MAX1704X

void BatteryReader::init() const
{
#ifdef USE_SENSOR_MAX1704X
    log_i("Initializing MAX1704X Battery Reader on Wire");
#if defined(MAX1704X_SDA_PIN) && defined(MAX1704X_SCL_PIN)
    TheWire.setPins(MAX1704X_SDA_PIN, MAX1704X_SCL_PIN);
#endif
    TheWire.setTimeOut((uint16_t)SENSOR_MAX1704X_TIMEOUT);
#else
    log_i("Initializing BatteryReader on pin %d", pin);
    pinMode(pin, INPUT);
    // Set the pin to use the ADC with no attenuation
    analogSetPinAttenuation(pin, ADC_11db);
#endif
    delay(200); // Allow some time for the ADC to stabilize
} // init

BatteryInfo BatteryReader::read() const
{
#ifdef USE_SENSOR_MAX1704X

    unsigned long ms = millis();
    log_i("Reading battery level from MAX1704X sensor");
    do {
        if ((millis() - ms) > SENSOR_MAX1704X_TIMEOUT) {
            log_e("MAX1704X sensor initialization timed out!");
            return BatteryInfo::full();
        }
        delay(200); // Wait a bit before trying again
        log_i(".");
    } while (!max1704x.begin(&TheWire));

    log_d("MAX1704X initialized..");
    delay(1000); // Allow some time for the sensor to initialize

    if (!max1704x.isDeviceReady()) {
        log_e("MAX1704X device is not ready!");
        return BatteryInfo::full();
    }

    float voltage = max1704x.cellVoltage();
    float percent = max1704x.cellPercent();
    float charge_rate = max1704x.chargeRate();

    log_d("Battery reading: voltage: %.2fV, percent: %.1f%%, charge rate: %.2f mA",
        voltage,
        percent,
        charge_rate);

    return BatteryInfo(
        voltage /* cell_voltage */, charge_rate /* charge_rate */, percent /* percent */);

#else

    uint32_t millivolts = 0;
    uint32_t raw = 0;
    for (int i = 0; i < num_readings; i++) {
        millivolts += analogReadMilliVolts(pin);
        raw += analogRead(pin);
        delay(delay_between_readings);
    }

    millivolts /= num_readings;
    raw /= num_readings;
    uint32_t voltage = millivolts / resistor_ratio;
    uint8_t percent = calcBatteryPercentage(voltage);

#ifdef DEBUG_BATTERY_READER
    log_d("Battery reading: raw: %lu, millivolts: %lu, voltage: %lu, percent: %u",
        raw,
        millivolts,
        voltage,
        percent);
#endif // DEBUG_BATTERY_READER

    return BatteryInfo(raw /* raw_value */,
        millivolts /* raw_millivolts */,
        voltage /* adjusted millivolts */,
        percent /* percent */);

#endif // USE_SENSOR_MAX1704X
} // read

} // namespace photo_frame