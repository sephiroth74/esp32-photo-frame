#include "battery.h"
#include "config.h"

#include <Arduino.h>
#include <cmath>
#include <driver/adc.h>
#include <esp_adc_cal.h>

namespace battery {

uint8_t calc_battery_percentage(uint32_t v)
{
    if (v >= steps[total_steps - 1].voltage)
        return steps[total_steps - 1].percent;
    if (v <= steps[0].voltage)
        return steps[0].percent;

    for (int8_t i = total_steps - 1; i > 0; i--) {
        battery_step_t current = steps[i];
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

bool battery_info_t::is_charging() const { return millivolts > BATTERY_CHARGING_MILLIVOLTS; }

battery_info_t battery_info::fromMv(uint32_t mv)
{
    return battery_info(mv, mv, calc_battery_percentage(mv));
} // fromMv

void BatteryReader::init() const
{
    Serial.print(F("Initializing BatteryReader on pin "));
    Serial.println(pin);
    pinMode(pin, INPUT);
    // Set the pin to use the ADC with no attenuation
    analogSetPinAttenuation(pin, ADC_11db);
} // init

battery_info_t BatteryReader::read() const
{
    uint32_t millivolts = 0;
    uint32_t raw = 0;
    for (int i = 0; i < num_readings; i++) {
        raw += analogRead(pin);
        millivolts += analogReadMilliVolts(pin);
        delay(delay_between_readings);
    }

    raw /= num_readings;
    millivolts /= num_readings;
    uint32_t voltage = millivolts / resistor_ratio;
    uint8_t percent = calc_battery_percentage(voltage);

#if DEBUG_MODE
    Serial.print("raw: ");
    Serial.print(raw);
    Serial.print(", millivolts: ");
    Serial.print(millivolts);
    Serial.print(", voltage: ");
    Serial.print(voltage);
    Serial.print(", percent: ");
    Serial.println(percent);
#endif // DEBUG_MODE

    return battery_info(
        millivolts /* raw_value */, voltage /* millivolts */, percent /* percent */);
} // read

} // namespace battery