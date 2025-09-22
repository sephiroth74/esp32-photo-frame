# ESP32 Photo Frame - Wiring Diagram

This document provides comprehensive wiring instructions for the ESP32 Photo Frame project. **Version 0.5.0** defaults to the **Unexpected Maker FeatherS3** with full RGB status system support. Alternative wiring for **DFRobot FireBeetle 2 ESP32-C6** is also provided.

## 🥇 FeatherS3 Configuration (Default - v0.5.0)

### FeatherS3 Hardware Components
1. **Unexpected Maker FeatherS3** - ESP32-S3 with built-in NeoPixel and 8MB PSRAM
2. **7.5" E-Paper Display** - Waveshare or Good Display DESPI-C02 (800x480)
3. **MicroSD Card Module** - For image storage and configuration
4. **10kΩ Potentiometer** - Manual refresh rate control
5. **Push Button** - External wakeup trigger
6. **5000mAh LiPo Battery** - Power source (JST connector)

### FeatherS3 Pin Configuration

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

### FeatherS3 Wiring Notes
- **RGB Status System**: Built-in NeoPixel provides visual feedback (no external wiring needed)
- **Battery Management**: Built-in JST connector and voltage monitoring
- **Deep Sleep Wakeup**: Button between GPIO1 and GND (internal pull-up enabled)
- **SD Card**: Uses high-speed SD_MMC (SDIO) interface for better performance
- **Separate SPI Buses**: SD card uses SDIO, e-paper uses dedicated SPI pins

---

## Alternative: ESP32-C6 Configuration

## Hardware Components

### Required Components
1. **DFRobot FireBeetle 2 ESP32-C6** - Main microcontroller board
2. **7.5" E-Paper Display** - Waveshare or Good Display DESPI-C02 (800x480)
3. **MicroSD Card Module** - For image storage and configuration
4. **~~DS3231 RTC Module~~** - ~~Real-time clock~~ **REMOVED** (NTP-only for ESP32-C6 compatibility)
5. **10kΩ Potentiometer** - Manual refresh rate control
6. **Push Button** - External wakeup trigger
7. **5000mAh LiPo Battery** - Power source
8. **Solar Panel** (optional) - For battery charging

### Optional Components
- **Jumper Wires** - Male-to-female and male-to-male
- **Breadboard or PCB** - For prototyping
- **Enclosure** - Weather protection for outdoor deployment

## Pin Configuration

Based on the configuration in `platformio/include/config/dfrobot_firebeetle2_esp32c6.h`:

### Complete Pin Mapping Table

| Component | ESP32-C6 Pin | Function | Direction | Notes |
|-----------|--------------|----------|-----------|-------|
| **E-Paper Display (SPI)** |
| BUSY | GPIO 5 | Display busy signal | Input | Pull-up recommended |
| RST | GPIO 4 | Display reset | Output | Active low |
| DC | GPIO 14 | Data/Command select | Output | High=Data, Low=Command |
| CS | GPIO 1 | SPI Chip Select | Output | Active low |
| SCK | GPIO 23 | SPI Clock | Output | Shared with SD card |
| MOSI | GPIO 22 | SPI Data Out | Output | Shared with SD card |
| MISO | GPIO 21 | SPI Data In | Input | Shared with SD card |
| VCC | 3.3V | Power | - | **Not 5V!** |
| GND | GND | Ground | - | |
| **SD Card Module (SPI)** |
| CS | GPIO 9 | SPI Chip Select | Output | Active low |
| SCK | GPIO 23 | SPI Clock | Output | Shared with display |
| MOSI | GPIO 22 | SPI Data Out | Output | Shared with display |
| MISO | GPIO 21 | SPI Data In | Input | Shared with display |
| VCC | 3.3V | Power | - | **Not 5V!** |
| GND | GND | Ground | - | |
| **~~RTC Module DS3231 (I2C)~~** | **REMOVED - Using NTP-only** |
| ~~SDA~~ | ~~GPIO 6~~ | ~~I2C Data~~ | ~~Bidirectional~~ | **Not used - GPIO 6 available** |
| ~~SCL~~ | ~~GPIO 7~~ | ~~I2C Clock~~ | ~~Output~~ | **Not used - GPIO 7 available** |
| **Potentiometer (Analog)** |
| PWR | GPIO 18 | Power Control | Output | Enables pot when high |
| WIPER | GPIO 2 | Analog Input | Input | 12-bit ADC (0-3300mV) |
| VCC | 3.3V | Power | - | Connect to PWR pin |
| GND | GND | Ground | - | |
| **Wakeup Button** |
| BUTTON | GPIO 3 | External Wakeup | Input | INPUT_PULLDOWN mode (GPIO 3 is RTC IO pin) |
| GND | GND | Ground | - | Button pulls to ground |
| **Power Management** |
| BATTERY | GPIO 0 | Battery Voltage | Input | Voltage divider (0.494 ratio) |
| BAT+ | JST-PH 2.0 | Battery Positive | - | Built-in charging circuit |
| BAT- | JST-PH 2.0 | Battery Negative | - | Built-in protection |
| SOLAR+ | Screw Terminal | Solar Panel Positive | - | Built-in MPPT charger |
| SOLAR- | Screw Terminal | Solar Panel Negative | - | |
| **Status LED** |
| LED | GPIO 15 | Built-in LED | Output | Active high |

## Detailed Wiring Instructions

### 1. E-Paper Display Connection

**Display Type**: 7.5" Black & White or 6-Color E-Paper (800x480)

```
ESP32-C6          E-Paper Display
-------           ---------------
GPIO 5    ────────── BUSY
GPIO 4    ────────── RST
GPIO 14   ────────── DC
GPIO 1    ────────── CS
GPIO 23   ────────── CLK/SCK
GPIO 22   ────────── DIN/MOSI
GPIO 21   ────────── DOUT/MISO (if available)
3.3V      ────────── VCC
GND       ────────── GND
```

**Important Notes:**
- **Voltage**: Use 3.3V only, not 5V
- **MISO**: Some displays don't have MISO pin - that's okay
- **Shared SPI**: SCK, MOSI, MISO are shared with SD card

### 2. SD Card Module Connection

**Module Type**: Standard SPI MicroSD adapter

```
ESP32-C6          SD Card Module
-------           --------------
GPIO 9    ────────── CS
GPIO 23   ────────── SCK/CLK
GPIO 22   ────────── MOSI/DI
GPIO 21   ────────── MISO/DO
3.3V      ────────── VCC
GND       ────────── GND
```

**Important Notes:**
- **Voltage**: Ensure module supports 3.3V logic
- **Card Format**: Use FAT32 formatted MicroSD card
- **Shared SPI**: Same SPI bus as display, different CS pin

### 3. RTC Module Connection

**Module Type**: DS3231 with integrated pull-up resistors

```
ESP32-C6          DS3231 RTC
-------           ----------
GPIO 6    ────────── SDA
GPIO 7    ────────── SCL
3.3V      ────────── VCC
GND       ────────── GND
```

**Pull-up Resistors** (if not built into module):
```
3.3V ────┬── 4.7kΩ ──── GPIO 6 (SDA)
         └── 4.7kΩ ──── GPIO 7 (SCL)
```

**Important Notes:**
- **I2C Address**: 0x68 (standard DS3231)
- **Backup Battery**: Install CR2032 in RTC module for time keeping
- **ESP32-C6 Issue**: I2C/WiFi interference handled by firmware

### 4. Potentiometer Connection

**Type**: 10kΩ linear potentiometer for refresh rate control

```
ESP32-C6          Potentiometer
-------           -------------
GPIO 18   ────────── VCC (Power control)
GPIO 2    ────────── WIPER (middle pin)
GND       ────────── GND (one outer pin)
```

**Circuit Details:**
```
GPIO 18 ──── VCC pin of potentiometer
GPIO 2  ──── Wiper (center) pin
GND     ──── One outer pin
            (Other outer pin not connected)
```

**Important Notes:**
- **Power Control**: GPIO 18 powers the pot to save battery
- **Range**: 0-3300mV input range (12-bit ADC)
- **Function**: Controls refresh interval (10 min to 4 hours)

### 5. Wakeup Button Connection

**Type**: Momentary push button for manual wakeup

```
ESP32-C6          Push Button
-------           -----------
GPIO 3    ────────── One terminal
GND       ────────── Other terminal
```

**Improved Circuit with External Pull-down** (prevents random wakeups):
```
                    ┌─── 3.3V
                    │
              Push Button
                    │
GPIO 3 ──────┬──────┘
             │
           10kΩ (Pull-down)
             │
           GND
```

**Configuration:**
- **Pull Mode**: INPUT_PULLDOWN (internal + external pull-down)
- **External Resistor**: 10kΩ to GND (stronger than internal 45kΩ)
- **Trigger**: ESP_EXT1_WAKEUP_ANY_HIGH
- **Function**: Wakes device from deep sleep
- **Anti-noise**: External pull-down prevents floating pin issues

### 6. Power System Connection

**Battery**: 5000mAh LiPo with JST-PH 2.0 connector

```
DFRobot Board     Battery/Solar
-------------     -------------
BAT+ (JST)   ────── Battery Red (+)
BAT- (JST)   ────── Battery Black (-)
SOLAR+       ────── Solar Panel Red (+)
SOLAR-       ────── Solar Panel Black (-)
GPIO 0       ────── Battery voltage monitor
```

**Battery Monitoring Circuit** (built into DFRobot board):
```
Battery+ ──── 680kΩ ──┬── GPIO 0 (ADC)
                      │
                    470kΩ
                      │
                    GND
```

**Voltage Divider Ratio**: 0.4939242316 (configured in firmware)

## Assembly Diagrams

### Breadboard Layout (Top View)
```
                    DFRobot FireBeetle 2 ESP32-C6
                           ┌─────────────────┐
    E-Paper BUSY    ──────┤ 5               │
    E-Paper RST     ──────┤ 4               │
    E-Paper DC      ──────┤ 14              │
    E-Paper CS      ──────┤ 1               │
    SD Card CS      ──────┤ 9               │
    RTC SDA         ──────┤ 6               │
    RTC SCL         ──────┤ 7               │
    Potentiometer   ──────┤ 2         18 ├───── Pot Power
    Wakeup Button   ──────┤ 3               │
    Battery Monitor ──────┤ 0               │
    Shared SPI SCK  ──────┤ 23              │
    Shared SPI MOSI ──────┤ 22              │
    Shared SPI MISO ──────┤ 21              │
    Built-in LED    ──────┤ 15              │
                          └─────────────────┘
```

### Power Distribution Diagram
```
    ┌─── 3.3V Rail ───┬─── E-Paper VCC
    │                 ├─── SD Card VCC
    │                 ├─── RTC VCC
    │                 └─── (Potentiometer via GPIO 18)
    │
ESP32-C6
    │
    └─── GND Rail ────┬─── E-Paper GND
                      ├─── SD Card GND
                      ├─── RTC GND
                      ├─── Potentiometer GND
                      └─── Button GND

Battery System (Built-in):
JST Connector ──── 5000mAh LiPo Battery
Solar Input   ──── Solar Panel (6V, 2W recommended)
```

## Testing and Verification

### Pre-Power Checks
1. **Voltage Levels**: Verify all connections use 3.3V, not 5V
2. **SPI Connections**: Check shared SCK/MOSI/MISO between display and SD
3. **I2C Pull-ups**: Ensure SDA/SCL have pull-up resistors
4. **Power Polarity**: Verify battery and solar panel polarity
5. **Button Wiring**: Confirm button pulls GPIO 3 to GND

### Power-On Tests
1. **LED Test**: Built-in LED should blink during startup
2. **SD Card**: Check if card is detected and readable
3. **RTC Test**: Verify I2C communication with DS3231
4. **Display Test**: Look for e-paper initialization
5. **Battery Reading**: Check if voltage monitoring works

### Common Issues and Solutions

| Issue | Symptom | Solution |
|-------|---------|----------|
| **Display not updating** | Blank or corrupted display | Check SPI connections, verify 3.3V power |
| **SD card not detected** | File system errors | Verify CS pin, check FAT32 format |
| **RTC communication failed** | Time sync errors | Check I2C pull-ups, verify address 0x68 |
| **Battery reading incorrect** | Wrong voltage values | Check voltage divider ratio in config |
| **Button not waking device** | No response to button | Verify GND connection, check pull-down mode |
| **JSON parsing errors** | Google Drive failures | ESP32-C6 I2C/WiFi interference (handled by firmware) |

## Fritzing File Creation Guide

To create a Fritzing file from this diagram:

### 1. Component Selection
- **ESP32-C6**: Use generic ESP32 part and modify labels
- **E-Paper Display**: Create custom part or use generic SPI display
- **SD Card Module**: Standard breadboard-friendly module
- **DS3231 RTC**: Available in Fritzing parts library
- **Potentiometer**: 10kΩ rotary potentiometer
- **Push Button**: Standard momentary switch
- **Battery**: 3.7V LiPo battery symbol

### 2. Wiring Colors (Suggested)
- **Power (3.3V)**: Red
- **Ground**: Black
- **SPI Clock**: Yellow
- **SPI Data**: Blue/Green
- **I2C**: Purple/Orange
- **Analog**: Brown
- **Digital Control**: Various colors

### 3. Layout Tips
- Group components by function (SPI, I2C, Power)
- Use bus connections for shared signals (SPI bus)
- Label all connections with pin numbers
- Add component values and specifications
- Include power distribution clearly

## PCB Design Considerations

For permanent installation:

### 1. Component Placement
- Keep switching regulators away from analog circuits
- Place decoupling capacitors close to power pins
- Minimize trace lengths for high-speed SPI signals

### 2. Power Management
- Use thick traces for power distribution (3.3V, GND)
- Add test points for voltage monitoring
- Include LED indicators for power status

### 3. Signal Integrity
- Keep SPI traces short and equal length
- Use ground plane for EMI reduction
- Separate analog and digital sections

### 4. Mechanical Considerations
- Provide mounting holes for enclosure
- Position connectors for easy access
- Plan for display cable routing

This wiring diagram provides the complete electrical interface for the ESP32 Photo Frame project. Follow safety precautions when working with batteries and ensure all connections are secure before powering on the system.