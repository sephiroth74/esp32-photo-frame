#ifndef BATTERY_H__
#define BATTERY_H__

#include <Arduino.h>
#include <stdlib.h>

#define DEFAULT_BATTERY_ADJUSTMENT 1.0f // Default adjustment factor
#define DEFAULT_BATTERY_READINGS   20   // Number of readings to average
#define VOLT_3V3                   3300
#define ADC_MAX_VALUE              4095.0f // 12-bit ADC max value

class Battery {
  public:
    Battery() : raw(0), input(0), voltage(0), percent(0.0) {}
    Battery(uint32_t raw, uint32_t input, uint32_t voltage, float percent) :
        raw(raw),
        input(input),
        voltage(voltage),
        percent(percent) {}
    uint32_t raw     = 0;   // raw ADC value
    uint32_t input   = 0;   // in mV
    uint32_t voltage = 0;   // in mV
    float percent    = 0.0; // percentage of battery

    const bool operator!() const { return (raw == 0); }
    operator bool() const { return (raw > 0); }
    void print(Print& p);

    String toString() const {
        // return 'raw: 1023 | input: 2024 | voltage: 4024 | percent: 100.0'
        return String("raw: ") + String(raw) + " | input: " + String(input) +
               " | voltage: " + String(voltage) + " | percent: " + String(percent);
    }
};

class BatteryLib {
  public:
    uint8_t pin;      // ADC pin
    float r1;         // Resistor 1 value
    float r2;         // Resistor 2 value
    float minV;       // Minimum voltage
    float maxV;       // Maximum voltage
    float adjustment; // Adjustment factor

    BatteryLib(uint8_t pin, float r1, float r2, float minV, float maxV);

    BatteryLib(uint8_t pin, float r1, float r2, float minV, float maxV, double adjustment);

    Battery read(uint8_t num_readings = DEFAULT_BATTERY_READINGS);

  private:
    float battery_ratio;    // Ratio of battery voltage to ADC reference voltage
    float resistor_divider; // Resistor divider ratio
};

#endif // BATTERY_H__