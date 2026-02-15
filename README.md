# ESP32 E-Paper Photo Frame

<p>
  <center><img src="assets/render-004.png" height="400" /></center>
</p>

<p>
  <center>
    <a href='assets/render-003.png' target='_blank'><img src="assets/render-003.png" height="200" /></a>
    <a href='assets/screenshot-004.jpg' target='_blank'><img src="assets/screenshot-004.jpg" height="200" /></a>
    <a href='assets/screenshot-006.jpg' target='_blank'><img src="assets/screenshot-006.jpg" height="200" /></a>
    <a href='assets/screenshot-003.jpg' target='_blank'><img src="assets/screenshot-003.jpg" height="200" /></a>
    <a href='assets/screenshot-002.jpg' target='_blank'><img src="assets/screenshot-002.jpg" height="200" /></a>
  </center>
</p>



**ESP32 E-Paper Photo Frame** is an open-source project designed to turn a low-power e-paper display into a smart, battery-efficient digital photo frame. Built around the ESP32 ecosystem, it offers a seamless way to display your memories using modern connectivity or local storage.

It allows to display images from different types of data providers like:
  * Shared Google Drive folders
  * Internal SD Card
  * Upload directly into the board via Wifi and your phone


The project also provides a **complete hardware solution**:
- **3D Printable Enclosure**: A custom-designed case available in [esp32-photo-frame.3mf](assets/printables/esp32-photo-frame.3mf).
- **Assembly Guide**: Comprehensive instructions for assemblying the device. See the [Assembly Guide](docs/assembly_guide.pdf)
- **Hardware Schematics**: See the [Schematics](docs/pros3d_schematics.pdf) on how to connect the hardware components.




## Hardware Requirements

To build this project, you will need the following core components. A detailed bill of materials is available in the [Assembly Guide](docs/assembly_guide.pdf).

### Essential Components
| Component | Recommendation | Notes |
|-----------|----------------|-------|
| **Microcontroller** | [Unexpected Maker ProS3-D](https://unexpectedmaker.com/shop.html#!/ProS3-D/p/759221737) | ESP32-S3 with PSRAM (Required for image buffer) |
| **Display** | [Good Display 7.3" ACeP](https://www.good-display.com/blank7.html?productId=533) | 800×480, 6-Color (GDEP073E01) |
| **Adapter** | [DESPI-C73](https://www.good-display.com/product/522.html) | Connection interface for the display |
| **Storage** | [Adafruit MicroSD SPI/SDIO](https://learn.adafruit.com/adafruit-microsd-spi-sdio) | High-speed SDIO support for faster image loading |
| **Battery** | 3.7V LiPo (5000mAh+) | Required for portable operation |


### Key Features

- **Ultra-Low Power**: Designed for longevity, the frame enters deep sleep between updates, lasting months on a standard LiPo battery.
- **E-Paper Optimized**: Leverages multiple dithering algorithms (Floyd-Steinberg, Atkinson, etc.) and a custom 6-color palette (via `.pfr1` format) to transform any image into a stunning e-ink display.
- **Smart & Autonomous**:
    - **Cloud Mode**: Connects to Google Drive to fetch and display random images at set intervals.
    - **Offline Mode**: Cycles through images stored locally on the SD Card.
    - **Local WiFi Mode**: Functions as a static display, updated instantly via the mobile or desktop app.
    - **Night Mode**: Automatically pauses updates during sleeping hours to save energy.
    - **Smart Cropping**: Integrated AI (InsightFace) detects faces to automatically center and crop images for the best composition.
- **Zero-Recompile Config**: All settings (WiFi, schedules, refresh intervals) are managed via a simple `config.json` file on the SD card—no programming knowledge required to tweak settings.

## Architecture Overview

The system operates on a clear pipeline:
1.  **Input**: Images are taken from your phone, computer, sdcard or cloud storage.
2.  **Processing**: Images are resized, dithered, and converted into the efficient [**`.pfr1`**](docs/BINARY_FILE_FORMAT.md) binary format by the provided tools (Rust CLI / Desktop App / Mobile App).
3.  **Transfer**: Processed files are moved to the frame via SD Card, Google Drive, or WiFi.
4.  **Display**: The ESP32 wakes up, loads the image, renders it to the e-paper screen, and returns to deep sleep.



## Project Structure

 - `platformio`: The main ESP32 firmware source code (C++)
 - `rust`
   - `processor`: This is the main desktop command line utility used to optimize and convert your images into the dedicated `.pfr1` file format used by the ESP32 board. It's a rust cli, which can be compiled for almost all operating systems. [Read more here](rust/processor/README.md)
   - `ws_client`: Small rust command line utility used to upload the `.pfr1` images into the ESP32 board, when it is compiled to accept images from Wifi
 - `flutter`:
   - `desktop`: Cross platform desktop GUI which can be used to interface with the rust processor cli. It can also be used to upload images into the board when the Wifi option is enabled on the ESP32.
   - `modile`: iOS and Android app used to convert and upload your images into the ESP32 board when the Wifi option is enabled. 
 - `docs`: Contains most of the the documentation




## Quick Start

Follow these steps to get your frame running:

There are basically 2 configurations for the ESP32:

 - **DEFAULT**: It will display images from Google Drive or the internal SD Card based on the configuration stored into the `config.json` in the root of the SD Card.
 - **WIFI**: It will display the images using an internal WebServer to which devices can connect and upload images into. This configuration can be enabled by setting the `ENABLE_WEBSERVER_DATAPROVIDER` constant to the platformio.ini environment


### Firmware & Hardware
1.  **Assemble**: Follow the [Assembly Guide](docs/assembly_guide.pdf) to build the hardware.
2.  **Flash Firmware**:
    - Install **Visual Studio Code** and the **PlatformIO** extension.
    - Open the `platformio/` folder.
    - Select your board (e.g., `pros3d_unexpectedmaker`) and click **Build** and **Upload**.
    - *See [Firmware Overview](docs/FIRMWARE_OVERVIEW.md) for detailed instructions.*

### Configuration (SD Card)
1.  Format a microSD card (FAT32).
2.  Create a `config.json` file in the root (see the [example_config.json](platformio/example_config.json))
3.  Add your WiFi and Component settings.
    ```json
    {
      "wifi": [{"ssid": "MyWifi", "password": "pass"}],
      "sd_card_config": {"enabled": true, "directories": ["/images"]},
      "google_drive_config": {"enabled": false}
    }
    ```
> See [Configuration Reference](docs/CONFIG_REFERENCE.md) for the complete `config.json` reference.


## Configuration (Google Drive)

In order to enable the Google Drive dataprovider, your config.json should look something like this:

```json
{
  ...
  "google_drive_config": {
    "enabled": true,
    "authentication": {
      "service_account_email": "your-service-account@your-project.iam.gserviceaccount.com",
      "private_key_pem": "-----BEGIN PRIVATE KEY-----\nYOUR_PRIVATE_KEY_CONTENT_HERE\n-----END PRIVATE KEY-----\n",
      "client_id": "your-client-id"
    },
    "drive": {
      "folder_ids": [
        "your-google-drive-folder-id-1",
        "your-google-drive-folder-id-2",
      ],
      "root_ca_path": "/certs/google_root_ca.pem",
      "list_page_size": 100,
      "use_insecure_tls": false
    },
    ...
  }
  ...
}
```
> Read more how to setup your Google Drive here: [GOOLGE DRIVE.md](docs/GOOGLE_DRIVE.md)


### Image Processing

The ESP32 board requires images in the custom `.pfr1` format (read the file format specifications here: [BINARY FILE FORMAT.md](docs/BINARY_FILE_FORMAT.md)). Use one of the included tools in order to create the file:

- **Desktop App (GUI)**: Drag-and-drop tool for macOS/Windows/Linux.  
  [Desktop App Guide](flutter/desktop/README.md)
- **Mobile App**: Process and upload directly from your phone.  
  [Mobile App Guide](flutter/mobile/README.md)
- **CLI Tool**: Power-user tool for batch processing.  
  [Rust Tools Overview](docs/RUST_OVERVIEW.md)

For instance, with the included rust cli, it can be done like this:

```bash
$ cargo run -- -i "~/photos/google_photos/" -o ~/Desktop/outputs/ -t six-colors --orientation 1 --output-format pfr1,jpg --dithering atkinson --dither-strength 100 --contrast 10 --brightness 40 --saturation 100 --auto-color --detect-people --confidence=0.5 --annotate --font Geneva --font-size 20 --annotation_background '#40000000' --no-pairing --extensions jpg,jpeg,png,heic,webp,tiff --verbose
```

When `detect-people` is set, the tool will automatically resize and crop the image based on the person found in the picture.

> Note that [imagemagick](https://imagemagick.org) is required for some operations.


### Upload Images
Choose your preferred method:
- **SD Card**: Copy `.pfr1` files directly to the SD card.
  [Enable the SD Card dataprovider](docs/CONFIG_REFERENCE.md)
- **Google Drive**: Upload files to your Drive folder.  
  [Google Drive Setup](docs/GOOGLE_DRIVE.md)
- **WiFi**: Use the Mobile or Desktop App to upload images wirelessly (requires `ENABLE_WEBSERVER_DATAPROVIDER` firmware build).  
  [WiFi Guide](docs/WIFI.md)

For detailed technical documentation, please refer to the `docs/` folder.