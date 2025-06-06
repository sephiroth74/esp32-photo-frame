#include "battery.h"
#include "config.h"

#include <Arduino.h>
#include <cmath>
#include <driver/adc.h>
#include <esp_adc_cal.h>

namespace battery {

bool battery_info_t::is_low() const { return percent <= BATTERY_PERCENT_LOW; }

bool battery_info_t::is_critical() const { return percent <= BATTERY_PERCENT_CRITICAL; }

bool battery_info_t::is_empty() const { return percent <= BATTERY_PERCENT_EMPTY; }

// inline constexpr bool batter_info::isLow() const { return percent <= BATTERY_PERCENT_LOW; }
// inline constexpr bool battery_info::isCritical() const {return percent <=
// BATTERY_PERCENT_CRITICAL;}

battery_info_t battery_info::fromMv(uint32_t mv) {
    return battery_info_t{
        raw_value : mv,
        millivolts : mv,
        percent : BatteryReader::calc_battery_percentage(mv)
    };
} // fromMv

} // namespace battery