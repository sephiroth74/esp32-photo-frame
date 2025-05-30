#include "Battery.h"

#include <Arduino.h>
#include <cmath>
#include <driver/adc.h>
#include <esp_adc_cal.h>
#include <vector>

void Battery::print(Print& p) {
    p.print("Raw ADC: ");
    p.print(raw);
    p.print(" | Input Voltage: ");
    p.print(input);
    p.print(" | Battery Voltage: ");
    p.print(voltage);
    p.print(" | Battery Percent: ");
    p.println(percent);
}

float calc_battery_percent(float v, float minv, float maxv) {
    float perc = (((float)v - minv) / (maxv - minv)) * 100.0;
    return (perc < 0) ? 0 : (perc > 100) ? 100 : perc;
}

BatteryLib::BatteryLib(uint8_t pin, float r1, float r2, float minV, float maxV) :
    pin(pin),
    r1(r1),
    r2(r2),
    minV(minV),
    maxV(maxV),
    adjustment(DEFAULT_BATTERY_ADJUSTMENT),
    battery_ratio(maxV / VOLT_3V3),
    resistor_divider(((r1 + r2) / r2)) {}

BatteryLib::BatteryLib(uint8_t pin, float r1, float r2, float minV, float maxV, double adjustment) :
    pin(pin),
    r1(r1),
    r2(r2),
    minV(minV),
    maxV(maxV),
    adjustment(adjustment),
    battery_ratio(maxV / VOLT_3V3),
    resistor_divider(((r1 + r2) / r2)) {}

Battery BatteryLib::read(uint8_t num_readings) {
    float raw           = 0;
    float rawMillivolts = 0;
    float inputVoltage;
    float finalVoltage;
    float batPercent;

    pinMode(pin, INPUT);
    analogSetPinAttenuation(pin, ADC_11db); // Set ADC attenuation to 11dB

    for (int i = 0; i < num_readings; i++) {
        rawMillivolts += analogReadMilliVolts(pin);
        // raw += analogRead(pin);
        // raw += analogRead(pin);
        delayMicroseconds(500); // 0.1 milliseconds
    }
    raw /= num_readings;           // Average the ADC value
    rawMillivolts /= num_readings; // Average the ADC value

    // if (raw > ADC_MAX_VALUE) {
    //     raw = ADC_MAX_VALUE; // Clamp to max ADC value
    // } else if (raw < 1000) {
    //     raw = ADC_MAX_VALUE / resistor_divider; // Clamp to min ADC value
    // }

    // inputVoltage = (raw * (((float)VOLT_3V3 / 1000.0f) / ADC_MAX_VALUE)) * 1000.0f; // Convert to
    // mV
    inputVoltage = rawMillivolts;

    // now we need to calculate the final voltage
    // using the voltage divider formula
    // Vout = Vin * (R2 / (R1 + R2))
    // finalVoltage = ((inputVoltage * resistor_divider) * adjustment) * battery_ratio;
    finalVoltage = ((inputVoltage * resistor_divider) * adjustment);

#ifdef BATTERY_DEBUG
    Serial.print("Raw ADC: ");
    Serial.print(raw);
    Serial.print(" | Raw Millivolts: ");
    Serial.print(rawMillivolts);
    Serial.print(" | Input Voltage: ");
    Serial.print(inputVoltage);
    Serial.print(" | Final Voltage: ");
    Serial.print(finalVoltage);
    Serial.print(" | Battery Ratio: ");
    Serial.println(battery_ratio);
#endif

    batPercent = calc_battery_percent(finalVoltage, minV, maxV);

    // Fill the battery struct
    raw = rawMillivolts;
    Battery bat(raw, inputVoltage, finalVoltage, batPercent);
    return bat;
}

void read_battery(Battery* bat, uint8_t pin, uint32_t minV, uint32_t maxV, uint8_t num_readings) {
    esp_adc_cal_characteristics_t adc_chars;
    // __attribute__((unused)) disables compiler warnings about this variable
    // being unused (Clang, GCC) which is the case when DEBUG_LEVEL == 0.
    pinMode(pin, INPUT);
    esp_adc_cal_value_t val_type __attribute__((unused));
    adc_power_acquire();
    uint16_t adc_val = 0;
    for (int i = 0; i < num_readings; i++) {
        adc_val += analogRead(pin);
        delay(6);
    }
    adc_val /= num_readings; // Average the ADC value
    adc_power_release();

    // We will use the eFuse ADC calibration bits, to get accurate voltage
    // readings. The DFRobot FireBeetle Esp32-E V1.0's ADC is 12 bit, and uses
    // 11db attenuation, which gives it a measurable input voltage range of 150mV
    // to 2450mV.
    val_type =
        esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_11db, ADC_WIDTH_BIT_12, 1100, &adc_chars);

    uint32_t batteryVoltage = esp_adc_cal_raw_to_voltage(adc_val, &adc_chars);

    // Serial.print("ADC Value: ");
    // Serial.print(adc_val);
    // Serial.print(" | Battery Voltage (mV): ");
    // Serial.println(batteryVoltage);

    // DFRobot FireBeetle Esp32-E V1.0 voltage divider (1M+1M), so readings are
    // multiplied by 2.
    batteryVoltage /= 100;

    bat->voltage = batteryVoltage;
    bat->percent = calc_battery_percent(batteryVoltage, minV, maxV);
}
