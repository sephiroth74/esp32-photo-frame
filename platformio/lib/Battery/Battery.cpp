#include "Battery.h"

#include <Arduino.h>
#include <cmath>
#include <driver/adc.h>
#include <esp_adc_cal.h>
#include <vector>

void Battery::print() {
    Serial.print("Raw ADC: ");
    Serial.print(raw);
    Serial.print(" | Input Voltage: ");
    Serial.print(inputVoltage);
    Serial.print(" | Battery Voltage: ");
    Serial.print(voltage);
    Serial.print(" | Battery Percent: ");
    Serial.println(percent);
}

float calc_battery_percent(uint32_t v, uint32_t minv, uint32_t maxv) {
    Serial.print("calc_battery_percent: ");
    Serial.print(v);
    Serial.print(" | minv: ");
    Serial.print(minv);
    Serial.print(" | maxv: ");
    Serial.println(maxv);

    float perc = (((float)v - minv) / (maxv - minv)) * 100.0;
    Serial.print("calc_battery_percent: perc: ");
    Serial.println(perc);

    return (perc < 0) ? 0 : (perc > 100) ? 100 : perc;
}

void read_battery_with_resistor(Battery* bat,
                                uint8_t pin,
                                uint32_t r1,
                                uint32_t r2,
                                uint32_t minV,
                                uint32_t maxV) {
    Serial.println("read_battery_with_resistor on pin: " + String(pin));
    uint32_t rawADC = 0;
    uint32_t voltage;
    uint32_t batVoltage;
    float batPercent;

    pinMode(pin, INPUT);

    for (int i = 0; i < BATTERY_READINGS; i++) {
        rawADC += analogRead(pin);
        delay(10);
    }
    rawADC /= 10; // Average the ADC value

    if (rawADC < 2023) {
        Serial.println("ADC value too low, using 0");
        rawADC = 0;
    }

    // Adjust for voltage divider and ADC
    voltage = (rawADC / 4.095) * 3.3;
    // voltage = (rawADC * 3.3) / 4.095; // Convert to mV

    // Adjust for voltage divider
    batVoltage = voltage * ((r1 + r2) / r2) * BATTERY_ADJUSTMENT;

    batPercent = calc_battery_percent(batVoltage, minV, maxV);

    Serial.print("Raw ADC: ");
    Serial.print(rawADC);
    Serial.print(" | Voltage: ");
    Serial.print(voltage);
    Serial.print(" | Battery Voltage: ");
    Serial.print(batVoltage);
    Serial.print(" | Battery Percent: ");
    Serial.println(batPercent);

    // Fill the battery struct
    bat->raw          = rawADC;
    bat->inputVoltage = voltage;
    bat->voltage      = batVoltage;
    bat->percent      = batPercent;
}

void read_battery(Battery* bat, uint8_t pin, uint32_t minV, uint32_t maxV) {
    esp_adc_cal_characteristics_t adc_chars;
    // __attribute__((unused)) disables compiler warnings about this variable
    // being unused (Clang, GCC) which is the case when DEBUG_LEVEL == 0.
    pinMode(pin, INPUT);
    esp_adc_cal_value_t val_type __attribute__((unused));
    adc_power_acquire();
    uint16_t adc_val = 0;
    for (int i = 0; i < BATTERY_READINGS; i++) {
        adc_val += analogRead(pin);
        delay(10);
    }
    adc_val /= 10; // Average the ADC value
    adc_power_release();

    // We will use the eFuse ADC calibration bits, to get accurate voltage
    // readings. The DFRobot FireBeetle Esp32-E V1.0's ADC is 12 bit, and uses
    // 11db attenuation, which gives it a measurable input voltage range of 150mV
    // to 2450mV.
    val_type =
        esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_11db, ADC_WIDTH_BIT_12, 1100, &adc_chars);

    uint32_t batteryVoltage = esp_adc_cal_raw_to_voltage(adc_val, &adc_chars);

    Serial.print("ADC Value: ");
    Serial.print(adc_val);
    Serial.print(" | Battery Voltage (mV): ");
    Serial.println(batteryVoltage);

    // DFRobot FireBeetle Esp32-E V1.0 voltage divider (1M+1M), so readings are
    // multiplied by 2.
    batteryVoltage /= 100;

    bat->voltage = batteryVoltage;
    bat->percent = calc_battery_percent(batteryVoltage, minV, maxV);
}
