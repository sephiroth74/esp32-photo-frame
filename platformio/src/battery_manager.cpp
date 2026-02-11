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

#include "battery_manager.h"
#include <algorithm>
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

constexpr BatteryMappingStep steps[21] = {
    BatteryMappingStep(0, 3270),   BatteryMappingStep(5, 3610),  BatteryMappingStep(10, 3690), BatteryMappingStep(15, 3710),
    BatteryMappingStep(20, 3730),  BatteryMappingStep(25, 3750), BatteryMappingStep(30, 3770), BatteryMappingStep(35, 3790),
    BatteryMappingStep(40, 3800),  BatteryMappingStep(45, 3820), BatteryMappingStep(50, 3840), BatteryMappingStep(55, 3850),
    BatteryMappingStep(60, 3870),  BatteryMappingStep(65, 3910), BatteryMappingStep(70, 3950), BatteryMappingStep(75, 3980),
    BatteryMappingStep(80, 4020),  BatteryMappingStep(85, 4080), BatteryMappingStep(90, 4110), BatteryMappingStep(95, 4150),
    BatteryMappingStep(100, 4200),
};

const uint8_t total_steps = 21;

const BatteryMappingStep *findBatteryMappingStep(uint8_t percent) {
  if (percent >= 100) {
    return &steps[total_steps - 1];
  }
  if (percent <= 0) {
    return &steps[0];
  }

  // use binary tree search for efficiency
  const auto it =
      std::find_if(steps, steps + total_steps, [percent](const BatteryMappingStep &step) { return step.percent >= percent; });

  if (it != steps + total_steps) {
    return &(*it);
  }

  return nullptr;
}

bool BatteryInfo::isLow() const {
  auto it = findBatteryMappingStep(BATTERY_PERCENT_LOW);
  if (it)
    return millivolts <= it->voltage;
  else
    return percent <= BATTERY_PERCENT_LOW;
}

bool BatteryInfo::isCritical() const {
  auto it = findBatteryMappingStep(BATTERY_PERCENT_CRITICAL);
  if (it)
    return millivolts <= it->voltage;
  else
    return percent <= BATTERY_PERCENT_CRITICAL;
}

bool BatteryInfo::isCharging() const {
#ifdef USE_SENSOR_MAX1704X
  return chargeRate != 0.0f;
#else
  auto it = findBatteryMappingStep(100);
  if (it) {
    // If we have a mapping step for 100%, we can use it to determine charging
    // state
    return millivolts > it->voltage;
  }
  return millivolts > BATTERY_CHARGING_MILLIVOLTS;
#endif
}

bool BatteryInfo::isEmpty() const {
  auto it = findBatteryMappingStep(BATTERY_PERCENT_EMPTY);
  if (it)
    return millivolts <= it->voltage;
  else
    return percent <= BATTERY_PERCENT_EMPTY;
}

BatteryManager &BatteryManager::getInstance() {
  static BatteryManager instance;
  return instance;
}

photo_frame_error_t BatteryManager::init() {
  if (initialized_) {
    log_w("[BatteryManager] Already initialized, skipping re-initialization");
    return error_type::None;
  }

#ifdef USE_SENSOR_MAX1704X
  log_i("[BatteryManager] Initializing MAX1704X Battery Reader on Wire");
  TheWire.setPins(MAX1704X_SDA_PIN, MAX1704X_SCL_PIN);
  TheWire.setTimeOut((uint16_t)SENSOR_MAX1704X_TIMEOUT);

  unsigned long ms = millis();
  log_d("[BatteryManager] Reading battery level from MAX1704X sensor");
  do {
    if ((millis() - ms) > SENSOR_MAX1704X_TIMEOUT) {
      log_e("[BatteryManager] MAX1704X sensor initialization timed out!");
      return error_type::BatteryNotDetected;
    }
    delay(20); // Wait a bit before trying again
    Serial.print(".");
  } while (!max1704x.begin(&TheWire));

#else
  log_i("[BatteryManager] Initializing BatteryReader on pin %d", BATTERY_PIN);
  pinMode(BATTERY_PIN, INPUT);
  analogSetPinAttenuation(BATTERY_PIN, ADC_11db);
#endif

  delay(1000); // Allow some time for the ADC to stabilize
  initialized_ = true;
  log_i("[BatteryManager] BatteryReader initialized successfully");
  return error_type::None;
}

photo_frame_error_t BatteryManager::read(BatteryInfo &info) const {
#ifdef USE_SENSOR_MAX1704X
  return readFromSensor(info);
#else
  return readFromAnalog(info);
#endif
}

#ifdef USE_SENSOR_MAX1704X

photo_frame_error_t BatteryManager::readFromSensor(BatteryInfo &info) const {
  if (!max1704x.isDeviceReady()) {
    log_e("[BatteryManager] MAX1704X device is not ready!");
    info.setFull(); // Set to full to allow error handling instead of treating
                    // as empty
    return error_type::BatteryNotDetected;
  }

  float voltage = max1704x.cellVoltage();
  float percent = max1704x.cellPercent();
  float charge_rate = max1704x.chargeRate();

  log_d("[BatteryManager] Battery reading: voltage: %.2fV, percent: %.1f%%, "
        "charge rate: %.2f mA",
        voltage, percent, charge_rate);

  info.updateFromSensor(voltage, charge_rate, percent);
  return error_type::None;
}

#else

BatteryInfo BatteryManager::readFromAnalog() const {
  uint32_t millivolts = 0;
  uint32_t raw = 0;
  for (int i = 0; i < BATTERY_NUM_READINGS; i++) {
    millivolts += analogReadMilliVolts(BATTERY_PIN);
    raw += analogRead(BATTERY_PIN);
    delay(BATTERY_DELAY_BETWEEN_READINGS);
  }

  millivolts /= BATTERY_NUM_READINGS;
  raw /= BATTERY_NUM_READINGS;
  uint32_t voltage = millivolts / BATTERY_RESISTORS_RATIO;
  uint8_t percent = calcBatteryPercentage(voltage);

  log_d("[BatteryManager] Battery reading: raw: %lu, millivolts: %lu, voltage: "
        "%lu, percent: %u",
        raw, millivolts, voltage, percent);

  info.updateFromAnalog(raw, millivolts, voltage, percent);
  return error_type::None;
}

uint8_t BatteryManager::calculatePercentage(uint32_t v) {
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
}

#endif // USE_SENSOR_MAX1704X

} // namespace photo_frame