# ESP32 Firmware Overview

The `platformio/` directory contains the C++ source code for the ESP32 microcontroller. It is built using the **PlatformIO** ecosystem, which handles dependency management, build chains, and uploading.

## 🛠️ Development Environment Setup

To compile and modify the firmware, we recommend using **Visual Studio Code** with the **PlatformIO** extension.

### 1. Install Prerequisites

1.  **Visual Studio Code**: Download and install from [code.visualstudio.com](https://code.visualstudio.com/).
2.  **PlatformIO IDE Extension**:
    - Open VS Code.
    - Go to the **Extensions** view (click the square icon on the left sidebar or press `Cmd+Shift+X`).
    - Search for `"PlatformIO IDE"`.
    - Click **Install**.
    - *Note: This installation may take a few minutes as it installs the core PlatformIO CLI and toolchains.*

### 2. Open the Project

1.  Launch VS Code.
2.  Click **File > Open Folder...**.
3.  Navigate to the `platformio/` folder inside this repository and click **Open**.
4.  PlatformIO will detect the `platformio.ini` file and automatically initialize the project. You should see a generic "PlatformIO" icon (alien head) appear in the left sidebar.

## 🏗️ Building and Uploading

### Select Environment

The project supports multiple hardware configurations (defined in `platformio.ini`). The currently supported environments are:

- **`pros3d_unexpectedmaker`** (Recommended): For the Unexpected Maker ProS3 Board.
- **`feathers3_unexpectedmaker`**: For the Unexpected Maker FeatherS3 Board.

To select your environment:
1.  Click the **PlatformIO icon** in the left sidebar.
2.  In the **Project Tasks** view, expand the folder corresponding to your board (e.g., `pros3d_unexpectedmaker`).

### Compile

To verify the code compiles without errors:
- Click **Build** under the selected environment in the Project Tasks.
- *Alternatively*: Click the **✓ (Check)** icon in the detailed blue status bar at the bottom of the window.

### Upload

To flash the firmware to your board:
1.  Connect your ESP32 board to your computer via USB-C.
2.  Click **Upload** under the selected environment.
3.  *Alternatively*: Click the **→ (Right Arrow)** icon in the bottom status bar.

## 📂 Project Structure

| Directory | Description |
|-----------|-------------|
| `src/` | Main source code (`.cpp` files). `main.cpp` is the entry point. |
| `include/` | Header files (`.h`). Includes definitions for PFR1 format and configuration structs. |
| `lib/` | Private libraries and dependencies not managed by the registry. |
| `platformio.ini` | Project configuration file. Defines build flags, libraries, and board settings. |

## 🧩 Key Build Flags

The `platformio.ini` file controls various features via build flags:

- **`ENABLE_BT_IMAGE`**: Enables Bluetooth image transfer mode. When uncommented, the frame will prioritize Bluetooth connections over WiFi/Cloud.
- **`DISABLE_DEEP_SLEEP`**: Prevents the board from sleeping, useful for debugging via Serial monitor.
- **`DEFAULT_ORIENTATION`**: Sets the default screen rotation (0=Landscape, 1=Portrait).

## ❓ Troubleshooting

- **Upload Failed**: Ensure the correct port is selected. You may need to put the board into "Bootloader Mode" manually (Hold BOOT, press RESET, release BOOT).
- **Missing Libraries**: Run the **platformio: Rebuild C/C++ Index** command from the VS Code Command Palette (`Cmd+Shift+P`) or run `pio pkg install` in the terminal.

## 💾 Data & Filesystem

### SD Card Configuration (Standard Mode)
For the frame to operate in standard mode (WiFi/SD), you **must** manually prepare the SD card:
1.  Format your microSD card to **FAT32**.
2.  Copy your customized `config.json` file to the **ROOT** of the SD card.
3.  Insert the SD card into the device before powering it on.

### LittleFS Partition (Bluetooth Mode)
If you are using **Bluetooth Mode** (`ENABLE_BT_IMAGE`), the firmware expects a fallback image to be present in the device's internal memory (LittleFS partition).

To upload this data (which includes `default.pfr1` found in `platformio/data/`):
1.  Connect your board via USB.
2.  In the PlatformIO Project Tasks (left sidebar), expand your environment (e.g., `pros3d_unexpectedmaker`).
3.  Click **Platform > Upload Filesystem Image**.
4.  *Note: This effectively flashes the files inside the `platformio/data` folder to the ESP32's flash memory.*

## ⚙️ Configuration Architecture

The firmware uses a two-layer configuration system to separate hardware definitions from user preferences.

### 1. Hardware Configuration (Compile-Time)
Located in `platformio/include/config/`, these headers define the physical wiring and capabilities of your specific board.

- **Entry Point**: `include/config.h` is the main configuration header. It sets system-wide defaults and includes the specific board file defined by the `LOCAL_CONFIG_FILE` macro in `platformio.ini`.
- **Board Files** (e.g., `pros3d_unexpectedmaker.h`):
    - **Pin Definitions**: Mappings for SPI (Display, SD Card), I2C (Battery), and Buttons.
    - **Hardware Features**: Enables specific drivers like `USE_SENSOR_MAX1704X` for battery monitoring.
    - **Display Type**: Selects the driver (e.g., `DISP_6C` for color, `DISP_BW` for monochrome).
    - **Power Settings**: Voltage dividers and sleep behavior.


### 2. User Configuration (Runtime)
Located on the **SD Card** as `config.json`.
- Controls **WiFi credentials**, **Refresh intervals**, and **Image sources** (Google Drive vs SD).
- Can be changed by the user *without* recompiling the firmware.
- See [Configuration Reference](CONFIG_REFERENCE.md) for details.
