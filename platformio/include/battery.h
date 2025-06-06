#ifndef __BATTERY_READER_H__
#define __BATTERY_READER_H__

#include <Arduino.h>
#include <stdint.h>

namespace battery {

typedef struct battery_step {
    uint8_t percent;
    uint16_t voltage;

    constexpr battery_step(uint16_t percent, uint16_t voltage) :
        percent(percent),
        voltage(voltage) {}
} battery_step_t;

typedef struct battery_info {
  public:
    uint32_t raw_value;
    uint32_t millivolts;
    uint8_t percent;

    constexpr battery_info(uint32_t raw_value, uint32_t millivolts, uint8_t percent) :
        raw_value(raw_value),
        millivolts(millivolts),
        percent(percent) {}

    constexpr battery_info() : raw_value(0), millivolts(0), percent(0) {}

    constexpr battery_info(const battery_step_t& step) :
        raw_value(step.voltage), millivolts(step.voltage), percent(step.percent) {}

    constexpr bool operator==(const battery_info& other) const {
        return (raw_value == other.raw_value && millivolts == other.millivolts &&
                percent == other.percent);
    }

    static inline constexpr battery_info empty() {
        return battery_info{raw_value : 0, millivolts : 0, percent : 0};
    }

    bool is_low() const;
    bool is_critical() const;
    bool is_empty() const;

    static battery_info fromMv(uint32_t mv);

} battery_info_t;

constexpr battery_step_t steps[21] = {
    battery_step(0, 3270),   battery_step(5, 3610),  battery_step(10, 3690), battery_step(15, 3710),
    battery_step(20, 3730),  battery_step(25, 3750), battery_step(30, 3770), battery_step(35, 3790),
    battery_step(40, 3800),  battery_step(45, 3820), battery_step(50, 3840), battery_step(55, 3850),
    battery_step(60, 3870),  battery_step(65, 3910), battery_step(70, 3950), battery_step(75, 3980),
    battery_step(80, 4020),  battery_step(85, 4080), battery_step(90, 4110), battery_step(95, 4150),
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
                            uint32_t delay) :
        pin(pin),
        resistor_ratio(resistor_ratio),
        num_readings(num_readings),
        delay_between_readings(delay) {}

    void init() const {
        Serial.print(F("Initializing BatteryReader on pin "));
        Serial.println(pin);
        pinMode(pin, INPUT);
        // Set the pin to use the ADC with no attenuation
        analogSetPinAttenuation(pin, ADC_11db);
    }

    battery_info_t read() const {
        uint32_t millivolts = 0;
        uint32_t raw        = 0;
        for (int i = 0; i < num_readings; i++) {
            raw += analogRead(pin);
            millivolts += analogReadMilliVolts(pin);
            delay(delay_between_readings);
        }

        raw /= num_readings;
        millivolts /= num_readings;
        uint32_t voltage = millivolts / resistor_ratio;
        uint8_t percent  = BatteryReader::calc_battery_percentage(voltage);

        Serial.print("raw: ");
        Serial.print(raw);
        Serial.print(", millivolts: ");
        Serial.print(millivolts);
        Serial.print(", voltage: ");
        Serial.print(voltage);
        Serial.print(", percent: ");
        Serial.println(percent);

        return battery_info{raw_value : millivolts, millivolts : voltage, percent : percent};
    }

    static uint8_t calc_battery_percentage(uint32_t v) {

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
    }

}; // BatteryReader

} // namespace battery

#endif // __BATTERY_READER_H__