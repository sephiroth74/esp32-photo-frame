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

#ifndef _DISPLAY_DRIVER_H_
#define _DISPLAY_DRIVER_H_

#include <Adafruit_GFX.h>
#include <Arduino.h>

// Display color definitions for AdafruitGFX (from display_debug.cpp)
#define DISPLAY_COLOR_WHITE 0xFF
#define DISPLAY_COLOR_BLACK 0x00
#define DISPLAY_COLOR_RED 0xE0
#define DISPLAY_COLOR_GREEN 0x1C
#define DISPLAY_COLOR_BLUE 0x03
#define DISPLAY_COLOR_YELLOW 0xFC

// Note: EPD_WIDTH and EPD_HEIGHT are defined in the display libraries
// GDEP073E01/Display_EPD_W21.h and GDEY075T7/Display_EPD_W21.h
// Both define: EPD_WIDTH = 800, EPD_HEIGHT = 480

/**
 * Abstract base class for display drivers
 * Provides a common interface for both 6-color (GDEP073E01) and B&W (GDEY075T7)
 * displays
 *
 * Important: Physical display is never rotated. Only the canvas is rotated for
 * portrait mode. Images are pre-rotated by the Rust photo-processor when
 * needed.
 */
class DisplayDriver {
public:
  DisplayDriver() : initialized(false) {}
  virtual ~DisplayDriver() {}

  /**
   * Initialize the display hardware
   * Sets up SPI communication and configures the display
   * @return true if initialization successful, false otherwise
   */
  virtual bool init() = 0;

  /**
   * Display an image buffer on the screen
   * @param imageBuffer Pointer to image data (800x480 bytes, 1 byte per pixel)
   * @return true if display successful, false otherwise
   */
  virtual bool picDisplay(uint8_t *imageBuffer) = 0;

  /**
   * Put the display into deep sleep mode to save power
   * Must be called after display operations to preserve screen lifespan
   */
  virtual void sleep() = 0;

  /**
   * Refresh the display (if supported by hardware)
   * @param partial_update Use partial update mode if available
   */
  virtual void refresh(bool partial_update = false) = 0;

  /**
   * Clear the display to white
   */
  virtual void clear() = 0;

  /**
   * Power off the display completely
   */
  virtual void power_off() = 0;

  /**
   * Put display into hibernate mode (deeper than sleep)
   */
  virtual void hibernate() = 0;

  /**
   * Get initialization status
   * @return true if display is initialized, false otherwise
   */
  bool isInitialized() const { return initialized; }

  /**
   * Get display type as string for debugging
   * @return Display type string (e.g., "6-Color", "B&W")
   */
  virtual const char *getDisplayType() const = 0;

  /**
   * Check if display supports partial update
   * @return true if partial update is supported
   */
  virtual bool has_partial_update() const { return false; }

  /**
   * Check if display supports fast partial update
   * @return true if fast partial update is supported
   */
  virtual bool has_fast_partial_update() const { return false; }

  /**
   * Check if display supports color
   * @return true if color is supported
   */
  virtual bool has_color() const = 0;

protected:
  bool initialized;

  /**
   * Configure SPI pins for display communication
   * Called internally by init()
   */
  virtual void configureSPI() = 0;
};

#endif // _DISPLAY_DRIVER_H_