#ifndef BATTERY_H__
#define BATTERY_H__

#include <Arduino.h>
#include <stdlib.h>

#define BATTERY_READINGS   20      // Number of readings to average
#define BATTERY_ADJUSTMENT 1.06548 // 1.1165 // 1.06548 // Adjust for voltage divider

class Battery {
  public:
    Battery() : raw(0), input(0), voltage(0), percent(0.0) {}
    uint32_t raw     = 0;   // raw ADC value
    uint32_t input   = 0;   // in mV
    uint32_t voltage = 0;   // in mV
    float percent    = 0.0; // percentage of battery

    const bool operator!() const { return (raw == 0); }
    operator bool() const { return (raw > 0); }
    void print();

    String toString() const {
        // return 'raw: 1023 | input: 2024 | voltage: 4024 | percent: 100.0'
        return String("raw: ") + String(raw) + " | input: " + String(input) +
               " | voltage: " + String(voltage) + " | percent: " + String(percent);
    }
};

void read_battery_with_resistor(Battery* bat,
                                uint8_t pin,
                                uint32_t r1,
                                uint32_t r2,
                                uint32_t minV,
                                uint32_t maxV,
                                double adjustment = BATTERY_ADJUSTMENT);

void read_battery(Battery* bat, uint8_t pin, uint32_t minV, uint32_t maxV);

#endif // BATTERY_H__