# esp32-photo-frame

**esp32-photo-frame** is a firmware project for an ESP32-based digital photo frame. The project is designed to display images from an SD card on an e-paper (EPD) display, with features such as battery monitoring, real-time clock (RTC) support, and multi-language localization.

<img src="assets/image001.jpg" alt="ESP32 Photo Frame Example" width="800" />

## Features

- Displays images stored on an SD card using an e-paper display
- Battery voltage monitoring and reporting
- Real-time clock integration for time-based features
- Potentiometer to regulate the refresh delay
- Multi-language support (e.g., English, Italian)
- Modular code structure for easy customization and extension

## Project Structure

- `src/` - Main source code for device logic, battery, SD card, rendering, RTC, and localization
- `include/` - Header files and configuration
- `icons/` - Scripts and assets for generating icon headers
- `lib/` - External libraries and assets
- `platformio.ini` - PlatformIO project configuration

## Getting Started

1. **Hardware Requirements**
   - [Arduino Nano ESP32](https://docs.arduino.cc/hardware/nano-esp32/)
   - [Waveshare 7.5 e-paper display](https://www.waveshare.com/7.5inch-e-paper-hat.htm)
   - [Good Display DESPI-C02](https://www.good-display.com/companyfile/DESPI-C02-Specification-29.html) Connector board
   - [Adafruit microSD](https://www.adafruit.com/product/4682?srsltid=AfmBOopfN-tYU3fKgpQquFOTLEY50Pl4PY8iTpBpGoXCpbJ8EQGVqBHn)
   - [AZDelivery Real Time Clock](https://www.amazon.it/dp/B077XN4LL4?ref=ppx_yo2ov_dt_b_fed_asin_title&th=1)
   - 3.7 5000mAh LiPo Battery

2. **Setup**
   - Clone this repository
   - Open the project in [Visual Studio Code](https://code.visualstudio.com/) with the [PlatformIO extension](https://platformio.org/)
   - Configure your hardware settings in `include/config.h` and `src/config.cpp` if needed

3. **Bitmaps preparation**
   - Use the `assets/convert_all.sh` script to convert your favourites pictures into .bmp files and store them in the SD card.

4. **Build and Upload**
   - Connect your ESP32 board
   - Use PlatformIO to build and upload the firmware

## Customization

- **Localization:** Add or modify language files in `include/locales/`
- **Icons:** Use scripts in `icons/` to generate new icon headers from PNG or SVG files
- **Configuration:** Adjust settings in `include/config.h` for your hardware and preferences

## License

This project is licensed under the MIT License.

