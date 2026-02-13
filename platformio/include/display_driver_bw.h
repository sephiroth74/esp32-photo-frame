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

#ifndef _DISPLAY_DRIVER_BW_H_
#define _DISPLAY_DRIVER_BW_H_

#include "display_driver.h"
#include <SPI.h>

/**
 * Display driver for GDEY075T7 Black & White e-paper display
 * Implements the DisplayDriver interface for B&W display operations
 */
class DisplayDriverBW : public DisplayDriver {
public:
  DisplayDriverBW(int8_t cs_pin, int8_t dc_pin, int8_t rst_pin, int8_t busy_pin, int8_t sck_pin, int8_t mosi_pin);
  virtual ~DisplayDriverBW();

  // DisplayDriver interface implementation
  bool init() override;
  bool picDisplay(uint8_t *imageBuffer) override;
  void sleep() override;
  void refresh(bool partial_update = false) override;
  void clear() override;
  void power_off() override;
  void hibernate() override;
  const char *getDisplayType() const override { return "Black & White"; }
  bool has_color() const override { return false; } // B&W display

protected:
  void configureSPI() override;

private:
  // Pin definitions
  int8_t _cs_pin;
  int8_t _dc_pin;
  int8_t _rst_pin;
  int8_t _busy_pin;
  int8_t _sck_pin;
  int8_t _mosi_pin;

  // SPI transaction settings
  static constexpr uint32_t SPI_FREQUENCY = 10000000; // 10MHz
};

#endif // _DISPLAY_DRIVER_BW_H_