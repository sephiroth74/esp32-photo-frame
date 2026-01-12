# ESP32 Photo Frame - Wiring Diagram

This document provides comprehensive wiring instructions for the ESP32 Photo Frame project. **Version 0.7.1** uses the **Unexpected Maker FeatherS3** as the recommended and optimized hardware platform with full RGB status system support and unified configuration system.

## 🥇 FeatherS3 Configuration (Default - v0.7.1)

### Hardware Components
1. **Unexpected Maker FeatherS3** - ESP32-S3 with built-in NeoPixel and 8MB PSRAM
2. **7.5" E-Paper Display** - Waveshare or Good Display DESPI-C02 (800x480)
3. **MicroSD Card Module** - For unified configuration and image caching
4. **50kΩ Potentiometer** - Manual refresh rate control
5. **Push Button** - External wakeup trigger
6. **5000mAh LiPo Battery** - Power source (JST connector)

### Pin Configuration

Based on the configuration in `platformio/include/config/feathers3_unexpectedmaker.h`:

| Component | FeatherS3 Pin | Function | Direction | Notes |
|-----------|---------------|----------|-----------|-------|
| **SD Card (SD_MMC/SDIO)** |
| CLK | GPIO14 | SDIO Clock | Output | High-speed SDIO interface |
| CMD | GPIO17 | SDIO Command | Bidirectional | Command/response line |
| D0 | GPIO7 | SDIO Data 0 | Bidirectional | Data line 0 |
| D1 | GPIO3 | SDIO Data 1 | Bidirectional | Data line 1 |
| D2 | GPIO12 | SDIO Data 2 | Bidirectional | Data line 2 |
| D3 | GPIO11 | SDIO Data 3 | Bidirectional | Data line 3 |
| **E-Paper Display (SPI)** |
| SCK | GPIO36 | SPI Clock | Output | Dedicated SPI bus |
| MOSI | GPIO35 | SPI Data Out | Output | Master out, slave in |
| MISO | GPIO37 | SPI Data In | Input | Master in, slave out |
| CS | GPIO38 | Chip Select | Output | Device selection |
| DC | GPIO10 | Data/Command | Output | Data/command selection |
| RST | GPIO5 | Reset | Output | Display reset |
| BUSY | GPIO6 | Busy Signal | Input | Display busy status |
| **System Controls** |
| WAKEUP | GPIO1 | Wakeup Button | Input | Deep sleep wakeup (RTC GPIO) |
| BATTERY | GPIO2 | Battery Monitor | Analog | Built-in voltage divider |
| RGB LED | GPIO40 | NeoPixel Data | Output | Built-in RGB status LED |
| RGB PWR | GPIO39 | NeoPixel Power | Output | LED power control (LDO2) |
| LED | GPIO13 | Status LED | Output | Built-in LED |
| **Potentiometer** |
| PWR | GPIO33 | Pot Power | Output | Power control |
| INPUT | GPIO18 | Pot Reading | Analog | Refresh rate control |

### Wiring Notes
- **RGB Status System**: Built-in NeoPixel provides visual feedback (no external wiring needed)
- **Battery Management**: Built-in JST connector and voltage monitoring
- **Deep Sleep Wakeup**: Button between GPIO1 and GND (internal pull-up enabled)
- **SD Card**: Uses high-speed SD_MMC (SDIO) interface for better performance
- **Separate SPI Buses**: SD card uses SDIO, e-paper uses dedicated SPI pins
- **Unified Configuration**: All settings (WiFi, Google Drive, Weather, Board) in single `/config.json` file

## Detailed Wiring Instructions

### 1. E-Paper Display Connection

**Display Type**: 7.5" Black & White or 6-Color E-Paper (800x480)

```
FeatherS3         E-Paper Display
---------         ---------------
GPIO6     ────────── BUSY
GPIO5     ────────── RST
GPIO10    ────────── DC
GPIO38    ────────── CS
GPIO36    ────────── CLK/SCK
GPIO35    ────────── DIN/MOSI
GPIO37    ────────── DOUT/MISO (if available)
3.3V      ────────── VCC
GND       ────────── GND
```

**Important Notes:**
- **Voltage**: Use 3.3V only, not 5V
- **MISO**: Some displays don't have MISO pin - that's okay
- **Dedicated SPI**: Uses separate SPI bus from SD card (no conflicts)

### 2. SD Card Module Connection (SDIO Interface)

**Module Type**: SDIO-compatible MicroSD adapter

```
FeatherS3         SD Card Module (SDIO)
---------         ---------------------
GPIO14    ────────── CLK
GPIO17    ────────── CMD
GPIO7     ────────── D0
GPIO3     ────────── D1
GPIO12    ────────── D2
GPIO11    ────────── D3
3.3V      ────────── VCC
GND       ────────── GND
```

**Important Notes:**
- **Interface**: Uses high-speed SDIO instead of SPI for better performance
- **Card Format**: Use FAT32 formatted MicroSD card
- **Configuration File**: Unified `/config.json` replaces separate config files
- **No Conflicts**: SDIO interface is separate from e-paper SPI

### 3. Potentiometer Connection

**Type**: 50kΩ linear potentiometer for refresh rate control

```
FeatherS3         Potentiometer
---------         -------------
GPIO33    ────────── VCC (Power control)
GPIO18    ────────── WIPER (middle pin)
GND       ────────── GND (one outer pin)
```

**Circuit Details:**
```
GPIO33 ──── VCC pin of potentiometer
GPIO18 ──── Wiper (center) pin
GND    ──── One outer pin
           (Other outer pin not connected)
```

**Important Notes:**
- **Power Control**: GPIO33 powers the pot to save battery
- **Range**: 0-3300mV input range (12-bit ADC)
- **Function**: Controls refresh interval (5 min to 4 hours)

### 4. Wakeup Button Connection

**Type**: Momentary push button for manual wakeup

```
FeatherS3         Push Button
---------         -----------
GPIO1     ────────── One terminal
GND       ────────── Other terminal
```

**Configuration:**
- **Pull Mode**: INPUT_PULLUP (internal pull-up enabled)
- **Trigger**: Button press pulls pin LOW
- **Function**: Wakes device from deep sleep
- **RTC GPIO**: GPIO1 is RTC-capable for deep sleep wakeup

### 5. Power System Connection

**Battery**: 5000mAh LiPo with JST connector

```
FeatherS3         Battery
---------         -------
JST Connector ──── Battery (Red=+, Black=-)
GPIO2         ──── Battery voltage monitor
```

**Battery Monitoring Circuit** (built into FeatherS3):
```
Battery+ ──── Voltage Divider ──── GPIO2 (ADC)
                    │
                   GND
```

**Voltage Divider Ratio**: 0.2574679943 (configured in firmware)

## Assembly Diagram

### FeatherS3 Layout (Top View)
```
                    Unexpected Maker FeatherS3
                         ┌─────────────────┐
  E-Paper BUSY    ──────┤ 6               │
  E-Paper RST     ──────┤ 5               │
  E-Paper DC      ──────┤ 10              │
  E-Paper CS      ──────┤ 38              │
  E-Paper SCK     ──────┤ 36              │
  E-Paper MOSI    ──────┤ 35              │
  E-Paper MISO    ──────┤ 37              │
  SD D0           ──────┤ 7               │
  SD D1           ──────┤ 3               │
  SD D2           ──────┤ 12              │
  SD D3           ──────┤ 11              │
  SD CLK          ──────┤ 14              │
  SD CMD          ──────┤ 17              │
  Potentiometer   ──────┤ 18        33 ├───── Pot Power
  Battery Monitor ──────┤ 2               │
  Wakeup Button   ──────┤ 1               │
  Built-in LED    ──────┤ 13              │
  RGB NeoPixel    ──────┤ 40              │
  RGB Power       ──────┤ 39              │
                         └─────────────────┘
```

### Power Distribution Diagram
```
    ┌─── 3.3V Rail ───┬─── E-Paper VCC
    │                 ├─── SD Card VCC
    │                 └─── (Potentiometer via GPIO33)
    │
FeatherS3
    │
    └─── GND Rail ────┬─── E-Paper GND
                      ├─── SD Card GND
                      ├─── Potentiometer GND
                      └─── Button GND

Battery System (Built-in):
JST Connector ──── 5000mAh LiPo Battery
USB-C        ──── Charging and Programming
```

## Unified Configuration System (v0.7.1)

### SD Card Structure
```
SD Card Root:
├── config.json              # Unified configuration (NEW)
├── weather_cache.json       # Weather data cache (auto-created)
├── certs/
│   └── google_root_ca.pem   # SSL certificate
└── gdrive/
    ├── cache/               # Google Drive cached images
    ├── temp/                # Temporary download files
    ├── toc_data.txt        # Table of contents data
    └── toc_meta.txt        # Table of contents metadata
```

### Configuration Benefits
- **Single File**: All settings (WiFi, Google Drive, Weather, Board) in one place
- **Runtime Control**: Weather functionality controlled without recompilation
- **Enhanced Validation**: Automatic value capping and fallback handling
- **Better Error Handling**: Extended sleep on SD card failure with graceful recovery
- **Faster Startup**: Single SD card read instead of 3 separate reads

## Testing and Verification

### Pre-Power Checks
1. **Voltage Levels**: Verify all connections use 3.3V, not 5V
2. **SDIO Connections**: Check 6-wire SDIO interface to SD card
3. **SPI Connections**: Verify e-paper display SPI connections
4. **Power Polarity**: Verify battery polarity on JST connector
5. **Button Wiring**: Confirm button pulls GPIO1 to GND

### Power-On Tests
1. **RGB LED Test**: Built-in NeoPixel should show status colors during startup
2. **Status LED**: Built-in LED should blink during startup
3. **SD Card**: Check if unified config.json is loaded
4. **Display Test**: Look for e-paper initialization and test pattern
5. **Battery Reading**: Check if voltage monitoring works
6. **Configuration**: Verify unified config loads all sections

### Common Issues and Solutions

| Issue | Symptom | Solution |
|-------|---------|----------|
| **Display not updating** | Blank or corrupted display | Check SPI connections, verify 3.3V power |
| **SD card not detected** | Config load errors | Verify SDIO connections, check FAT32 format |
| **Config not loading** | Fallback to defaults | Check config.json syntax, verify SD card read |
| **Battery reading incorrect** | Wrong voltage values | Check voltage divider ratio in config |
| **Button not waking device** | No response to button | Verify GND connection, check RTC GPIO1 |
| **RGB LED not working** | No status colors | Check GPIO40 connection, verify power control |

## RGB Status System

### Built-in Status Indicators
The FeatherS3's built-in NeoPixel provides comprehensive system status:

- **🔷 Starting**: White pulse during system startup
- **🔵 WiFi Connecting**: Blue pulse while connecting to WiFi
- **🔴 WiFi Failed**: Red slow blink when connection fails
- **🟢 Weather Fetching**: Green pulse while fetching weather data
- **🟠 SD Operations**: Orange pulse during SD card operations
- **🟡 SD Writing**: Yellow pulse during SD card writes
- **🔵 Google Drive**: Cyan pulse during Google Drive operations
- **🟣 Downloading**: Purple pulse during file downloads
- **🟡 Rendering**: Pink solid during display rendering
- **🔴 Battery Low**: Red slow blink for critical battery warning
- **🔴 Error**: Red fast blink for system errors
- **⚪ Sleep Prep**: Dim white fade-out before deep sleep

### Power Management
- **Battery-Aware**: Automatic brightness reduction when battery is low
- **Ultra Power Efficient**: 2-5mA additional consumption
- **Sleep Optimization**: Complete shutdown during deep sleep (0mA impact)
- **Critical Battery Protection**: RGB system auto-disables to preserve power

This wiring diagram provides the complete electrical interface for the ESP32 Photo Frame project using the optimized FeatherS3 platform. The unified configuration system simplifies setup while the dedicated RGB status system provides comprehensive visual feedback for all system operations.