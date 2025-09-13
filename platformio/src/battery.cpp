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

#ifdef USE_RTC
#define TheWire Wire1
#else
#define TheWire Wire
#endif

#endif // USE_SENSOR_MAX1704X

namespace photo_frame {

const battery_step_t steps[21] = {
    battery_step_t(0, 3270),  battery_step_t(5, 3610),  battery_step_t(10, 3690),
    battery_step_t(15, 3710), battery_step_t(20, 3730), battery_step_t(25, 3750),
    battery_step_t(30, 3770), battery_step_t(35, 3790), battery_step_t(40, 3800),
    battery_step_t(45, 3820), battery_step_t(50, 3840), battery_step_t(55, 3850),
    battery_step_t(60, 3870), battery_step_t(65, 3910), battery_step_t(70, 3950),
    battery_step_t(75, 3980), battery_step_t(80, 4020), battery_step_t(85, 4080),
    battery_step_t(90, 4110), battery_step_t(95, 4150), battery_step_t(100, 4200),
};

const uint8_t total_steps = 21;

uint8_t calc_battery_percentage(uint32_t v) {
    if (v >= steps[total_steps - 1].voltage)
        return steps[total_steps - 1].percent;
    if (v <= steps[0].voltage)
        return steps[0].percent;

    for (int8_t i = total_steps - 1; i > 0; i--) {
        battery_step_t current  = steps[i];
        battery_step_t previous = steps[i - 1];
        if (v >= previous.voltage && v <= current.voltage) {
            return map(v, previous.voltage, current.voltage, previous.percent, current.percent);
        }
    }
    return 0;
} // calc_battery_percentage

bool battery_info_t::is_low() const { return percent <= BATTERY_PERCENT_LOW; }

bool battery_info_t::is_critical() const { return percent <= BATTERY_PERCENT_CRITICAL; }

bool battery_info_t::is_empty() const { return percent <= BATTERY_PERCENT_EMPTY; }

#ifdef USE_SENSOR_MAX1704X
bool battery_info_t::is_charging() const { return percent > 100; }
#else
bool battery_info_t::is_charging() const { return millivolts > BATTERY_CHARGING_MILLIVOLTS; }
#endif // USE_SENSOR_MAX1704X

void battery_reader::init() const {
#ifdef USE_SENSOR_MAX1704X
    Serial.println(F("[battery] Initializing MAX1704X Battery Reader on Wire"));
#if defined(MAX1704X_SDA_PIN) && defined(MAX1704X_SCL_PIN)
    TheWire.setPins(MAX1704X_SDA_PIN, MAX1704X_SCL_PIN);
#endif
    TheWire.setTimeOut((uint16_t)SENSOR_MAX1704X_TIMEOUT);
#else
    Serial.print(F("[battery] Initializing battery_reader on pin "));
    Serial.println(pin);
    pinMode(pin, INPUT);
    // Set the pin to use the ADC with no attenuation
    analogSetPinAttenuation(pin, ADC_11db);
#endif
    delay(200); // Allow some time for the ADC to stabilize
} // init

battery_info_t battery_reader::read() const {
#ifdef USE_SENSOR_MAX1704X

    unsigned long ms = millis();
    Serial.println(F("[battery] Reading battery level from MAX1704X sensor"));
    do {
        if ((millis() - ms) > SENSOR_MAX1704X_TIMEOUT) {
            Serial.println();
            Serial.println(F("[battery] MAX1704X sensor initialization timed out!"));
            return battery_info_t::full();
        }
        delay(200); // Wait a bit before trying again
        Serial.print(F("."));
    } while (!max1704x.begin(&TheWire));

    Serial.println();
    Serial.println(F("[battery] MAX1704X initialized.."));
    delay(1000); // Allow some time for the sensor to initialize

    if (!max1704x.isDeviceReady()) {
        Serial.println(F("[battery] MAX1704X device is not ready!"));
        return battery_info_t::full();
    }

    float voltage     = max1704x.cellVoltage();
    float percent     = max1704x.cellPercent();
    float charge_rate = max1704x.chargeRate();

    Serial.print(F("[battery] Battery reading: "));
    Serial.print("voltage: ");
    Serial.print(voltage);
    Serial.print("V, percent: ");
    Serial.print(percent);
    Serial.print("%, charge rate: ");
    Serial.print(charge_rate);
    Serial.println(" mA");

    return battery_info(
        voltage /* cell_voltage */, charge_rate /* charge_rate */, percent /* percent */);

#else

    uint32_t millivolts = 0;
    uint32_t raw        = 0;
    for (int i = 0; i < num_readings; i++) {
        millivolts += analogReadMilliVolts(pin);
        raw += analogRead(pin);
        delay(delay_between_readings);
    }

    millivolts /= num_readings;
    raw /= num_readings;
    uint32_t voltage = millivolts / resistor_ratio;
    uint8_t percent  = calc_battery_percentage(voltage);

#ifdef DEBUG_BATTERY_READER
    Serial.print(F("[battery] Battery reading: "));
    Serial.print("raw: ");
    Serial.print(raw);
    Serial.print(", millivolts: ");
    Serial.print(millivolts);
    Serial.print(", voltage: ");
    Serial.print(voltage);
    Serial.print(", percent: ");
    Serial.println(percent);
#endif // DEBUG_BATTERY_READER

    return battery_info(raw /* raw_value */,
                        millivolts /* raw_millivolts */,
                        voltage /* adjusted millivolts */,
                        percent /* percent */);

#endif // USE_SENSOR_MAX1704X
} // read

} // namespace photo_frame