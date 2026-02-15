# WiFi/WebSocket Image Transfer Guide

## Overview

The ESP32 Photo Frame uses a WiFi Access Point and WebSocket-based architecture for transferring images from mobile and desktop applications to the device. This system replaced the previous Bluetooth-based transfer mechanism to provide faster, more reliable image uploads with better battery management.

This configuration can be enabled by setting the `ENABLE_WEBSERVER_DATAPROVIDER` constant in the **platformio.ini** file.

### Architecture Components

**WiFi Access Point Mode:**
The ESP32 creates its own WiFi Access Point (AP) when ready to receive images. This eliminates the need for the device to connect to an existing WiFi network, simplifying the connection process and ensuring consistent behavior across different environments.

**QR Code Connection:**
The device displays a QR code on its e-paper display containing all necessary connection parameters (WiFi SSID, IP address, WebSocket port, and device capabilities). Users simply scan this QR code with the mobile app to automatically configure the connection.

**WebSocket Protocol:**
Once connected to the WiFi AP, the mobile or desktop application establishes a WebSocket connection for bidirectional communication. The WebSocket protocol handles:
- Device capability negotiation (display type, resolution, supported file format)
- Chunked binary image uploads with acknowledgment
- Real-time progress feedback
- Error handling and recovery
- Battery status monitoring

**Chunked Upload Mechanism:**
Images are transferred in 4096-byte chunks over the WebSocket connection. Each chunk is acknowledged by the device before the next chunk is sent, ensuring reliable delivery and allowing for progress tracking. This chunked approach prevents memory overflow on the ESP32 and provides resilience against connection interruptions.

## Prerequisites

### Firmware Requirements

**Build Flag:**
The WiFi/WebSocket transfer functionality must be enabled at compile time using the build flag:
```
ENABLE_WEBSERVER_DATAPROVIDER
```

This flag should be defined in your `platformio.ini` configuration file. Without this flag, the firmware will not include the WebSocket server or WiFi AP functionality.

**Hardware Requirements:**
- ESP32 module with WiFi capability (all standard ESP32 variants include WiFi)
- E-paper display for showing QR code and status information
- Sufficient flash storage for image files

### Client Application Requirements

**Mobile App (Flutter):**
- Android 5.0 (API level 21) or higher, or iOS 11.0 or higher
- Camera permission for QR code scanning
- WiFi connectivity
- Network binding capability (Android 12+ handles this automatically)

**Desktop App (Rust):**
- WiFi connectivity
- Manual entry of connection parameters (IP address, port, SSID)

### Network Requirements

**WiFi Compatibility:**
- The ESP32 Access Point operates on 2.4 GHz WiFi (802.11 b/g/n)
- Client devices must support 2.4 GHz WiFi networks
- No internet connection is required (the ESP32 AP is a local network)

## Connection Workflow

This section describes the complete process of connecting a client application to the ESP32 Photo Frame and establishing a WebSocket session for image transfer.

### Overview of Connection Process

The connection workflow consists of three main phases:
1. **QR Code Scanning** - Capture device connection parameters
2. **WiFi Connection** - Join the ESP32 Access Point network
3. **WebSocket Handshake** - Establish bidirectional communication channel

### Phase 1: QR Code Scanning

**Step 1: Device Displays QR Code**

When the ESP32 Photo Frame is ready to receive images, it creates a WiFi Access Point and displays a QR code on its e-paper display. This QR code contains all the information needed to connect to the device.

**QR Code Format:**
```
photoframe://connect?ip=<IP>&ssid=<SSID>&port=<PORT>&v=<VERSION>&d=<DISPLAY_TYPE>&w=<WIDTH>&h=<HEIGHT>
```

**QR Code Parameters:**
- `ip` - ESP32 Access Point IP address (typically "192.168.4.1")
- `ssid` - WiFi Access Point SSID (e.g., "PhotoFrame-ABC123")
- `port` - WebSocket server port (default: 81)
- `v` - PFR1 file format version supported by the device
- `d` - Display type: `0` = Black/White, `1` = 6-Color
- `w` - Display width in pixels (e.g., 800)
- `h` - Display height in pixels (e.g., 480)

**Example QR Code:**
```
photoframe://connect?ip=192.168.4.1&ssid=PhotoFrame-ABC123&port=81&v=1&d=1&w=800&h=480
```

**Step 2: User Scans QR Code**

The mobile application uses the device camera to scan the QR code. The app parses the deep link URL and extracts all connection parameters automatically. This eliminates manual configuration and reduces connection errors.

**Step 3: Connection Parameters Validated**

The mobile app validates the extracted parameters:
- Verifies the URL scheme is `photoframe://connect`
- Checks that all required parameters are present
- Validates parameter formats (IP address, port number, dimensions)
- Confirms PFR1 version compatibility

### Phase 2: WiFi Connection

**Step 4: Network Binding (Android 12+)**

On Android 12 and higher, the app must bind to the WiFi network to ensure all traffic routes through the ESP32 Access Point. This is handled automatically by the connection service.

**Step 5: Join WiFi Access Point**

The mobile app connects to the WiFi network using the SSID from the QR code:
- SSID: Value from `ssid` parameter
- Security: Open network (no password required)
- IP Assignment: DHCP (automatic)

**Step 6: WiFi Connection Established**

Once connected, the mobile device receives an IP address from the ESP32's DHCP server. The device is now on the same local network as the ESP32 and can communicate via the IP address from the QR code.

**Connection Indicators:**
- Mobile device shows WiFi connected to the PhotoFrame SSID
- Mobile device may display "No Internet" warning (this is normal and expected)
- App confirms network connectivity to the ESP32 IP address

### Phase 3: WebSocket Handshake

**Step 7: Establish TCP Connection**

The mobile app initiates a WebSocket connection to the ESP32:
```
ws://<ip>:<port>
```

For example: `ws://192.168.4.1:81`

**Step 8: TCP Connection Ready**

The app waits for the TCP connection to be established. This typically takes 1-2 seconds. A timeout is enforced to detect connection failures.

**Step 9: Register Message Handler**

The app registers a stream listener to handle incoming WebSocket messages from the server. This listener will process all subsequent messages during the session.

**Step 10: Send Handshake Message**

The client sends the initial handshake message to identify itself:

```json
{
  "type": "handshake",
  "clientVersion": "1.0.0",
  "platform": "flutter"
}
```

**Handshake Message Fields:**
- `type` - Message type identifier ("handshake")
- `clientVersion` - Client application version
- `platform` - Client platform identifier ("flutter", "rust", etc.)

**Step 11: Receive Board Info Response**

The ESP32 responds with a `board_info` message containing device configuration and capabilities:

```json
{
  "type": "board_info",
  "board": "ESP32-WROVER",
  "flashSize": 4194304,
  "displayType": "6-Color",
  "displayWidth": 800,
  "displayHeight": 480,
  "displayRotation": 0,
  "serverVersion": "2.1.0",
  "fileVersion": 1,
  "binaryFileSize": 1048576,
  "ipAddress": "192.168.4.1",
  "ipPort": 81,
  "ssid": "PhotoFrame-ABC123",
  "batteryLevel": 85,
  "batteryVoltageMv": 3850
}
```

**Board Info Fields:**
- `board` - ESP32 board model name
- `flashSize` - Total flash storage in bytes
- `displayType` - Display type ("BW" or "6-Color")
- `displayWidth` - Display width in pixels
- `displayHeight` - Display height in pixels
- `displayRotation` - Current rotation (0-3)
- `serverVersion` - Firmware version string
- `fileVersion` - PFR1 file format version
- `binaryFileSize` - Compiled firmware size
- `ipAddress` - Device IP address
- `ipPort` - WebSocket server port
- `ssid` - WiFi Access Point SSID
- `batteryLevel` - Battery percentage (0-100, optional)
- `batteryVoltageMv` - Battery voltage in millivolts (optional)

**Step 12: Connection Complete**

The WebSocket connection is now fully established and ready for image transfer operations. The client application can now:
- Send image upload requests
- Receive real-time status updates
- Monitor battery levels
- Request device shutdown

### Complete Workflow Diagram

```
┌─────────────────┐
│   ESP32 Device  │
│  Ready to       │
│  Receive Images │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│ Create WiFi AP  │
│ Display QR Code │
└────────┬────────┘
         │
         │ QR Code Contains:
         │ • WiFi SSID
         │ • IP Address
         │ • WebSocket Port
         │ • Device Capabilities
         │
         ▼
┌─────────────────┐
│  User Scans QR  │
│  with Mobile    │
│  App Camera     │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│ App Parses QR   │
│ Extracts Params │
│ Validates Data  │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│ App Connects to │
│ WiFi AP (SSID)  │
│ No Password     │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│ Device Receives │
│ IP via DHCP     │
│ (e.g. 192.168.4.2)│
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│ App Initiates   │
│ WebSocket to    │
│ ws://IP:PORT    │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│ TCP Connection  │
│ Established     │
│ (1-2 seconds)   │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│ App Sends       │
│ Handshake       │
│ Message         │
└────────┬────────┘
         │
         │ {"type": "handshake",
         │  "clientVersion": "1.0.0",
         │  "platform": "flutter"}
         │
         ▼
┌─────────────────┐
│ ESP32 Sends     │
│ Board Info      │
│ Response        │
└────────┬────────┘
         │
         │ {"type": "board_info",
         │  "displayType": "6-Color",
         │  "displayWidth": 800,
         │  "displayHeight": 480,
         │  "batteryLevel": 85,
         │  ...}
         │
         ▼
┌─────────────────┐
│ Connection      │
│ Complete!       │
│ Ready for       │
│ Image Transfer  │
└─────────────────┘
```

### Connection Timing

**Typical Connection Times:**
- QR Code Scan: 1-2 seconds
- WiFi Connection: 3-5 seconds
- WebSocket Handshake: 1-2 seconds
- **Total Connection Time: 5-9 seconds**

**Timeout Values:**
- TCP Connection: 10 seconds
- Handshake Response: 10 seconds
- WiFi Connection: 30 seconds (platform dependent)

### Connection Error Handling

**QR Code Scan Failures:**
- Invalid QR code format → Display error message, prompt to rescan
- Missing parameters → Display error message, prompt to rescan
- Incompatible version → Display compatibility warning

**WiFi Connection Failures:**
- Cannot find SSID → Verify ESP32 is powered on and displaying QR code
- Connection timeout → Check device proximity, retry connection
- DHCP failure → Restart ESP32, retry connection

**WebSocket Handshake Failures:**
- TCP connection timeout → Verify IP address and port, check WiFi connection
- No board_info response → Restart ESP32, retry connection
- Connection refused → Verify WebSocket server is running on ESP32

### Next Steps After Connection

Once the connection is established, the client application can proceed with image transfer operations. See the **Technical Details** section for information about the image upload protocol, including:
- Initializing an upload session
- Sending image chunks
- Receiving acknowledgments
- Handling upload completion

## Technical Details

This section provides detailed technical specifications for developers implementing or debugging the WiFi/WebSocket transfer system.

### QR Code Format

The ESP32 generates a deep link URL encoded in a QR code that contains all necessary connection parameters. This URL follows a custom URI scheme designed specifically for the PhotoFrame application.

**URL Format:**
```
photoframe://connect?ip=<IP>&ssid=<SSID>&port=<PORT>&v=<VERSION>&d=<DISPLAY_TYPE>&w=<WIDTH>&h=<HEIGHT>
```

**Parameter Specifications:**

| Parameter | Type | Description | Example |
|-----------|------|-------------|---------|
| `ip` | IPv4 Address | ESP32 Access Point IP address | `192.168.4.1` |
| `ssid` | String | WiFi Access Point SSID | `PhotoFrame-ABC123` |
| `port` | Integer | WebSocket server port number | `81` |
| `v` | Integer | PFR1 file format version supported | `1` |
| `d` | Integer | Display type: `0` = Black/White, `1` = 6-Color | `1` |
| `w` | Integer | Display width in pixels | `800` |
| `h` | Integer | Display height in pixels | `480` |

**Complete Example:**
```
photoframe://connect?ip=192.168.4.1&ssid=PhotoFrame-ABC123&port=81&v=1&d=1&w=800&h=480
```

**Display Type Determination:**

The display type parameter (`d`) is determined at compile time based on the firmware configuration:
- `d=0` - Black and White (2-color) e-paper display
- `d=1` - 6-Color (ACeP) e-paper display

This parameter allows the client application to generate the correct PFR1 binary format for the specific display hardware.

**URL Scheme Registration:**

Client applications must register the `photoframe://` URL scheme to handle deep links. When a QR code is scanned, the operating system will automatically launch the PhotoFrame app and pass the connection parameters.

### WebSocket Protocol

The WebSocket protocol uses JSON messages for control and coordination, with binary frames for image data transfer. All JSON messages follow a consistent structure with a `type` field identifying the message type.

#### Message Types Overview

**Client to Server Messages:**
- `handshake` - Initial client connection and identification
- `init` - Initialize image upload session
- `end` - Signal completion of image upload
- `shutdown` - Request device shutdown

**Server to Client Messages:**
- `board_info` - Device configuration and capabilities
- `ready` - Server ready to receive image chunks
- `chunk_ack` - Acknowledgment of received chunk
- `ack` - General acknowledgment
- `final_response` - Final upload status
- `display_ready` - Display update complete notification
- `error` - Error message with details

#### Message Specifications

##### 1. Handshake Message (Client → Server)

Sent immediately after WebSocket connection is established to identify the client.

**Format:**
```json
{
  "type": "handshake",
  "clientVersion": "1.0.0",
  "platform": "flutter"
}
```

**Fields:**
- `type` - Always `"handshake"`
- `clientVersion` - Client application version string
- `platform` - Client platform identifier (`"flutter"`, `"rust"`, etc.)

**Purpose:**
- Identifies client application and version
- Allows server to track client types and versions
- Triggers server to send board_info response

##### 2. Board Info Message (Server → Client)

Sent by server in response to handshake, providing device configuration and capabilities.

**Format:**
```json
{
  "type": "board_info",
  "board": "ESP32-WROVER",
  "flashSize": 4194304,
  "displayType": "6-Color",
  "displayWidth": 800,
  "displayHeight": 480,
  "displayRotation": 0,
  "serverVersion": "2.1.0",
  "fileVersion": 1,
  "binaryFileSize": 1048576,
  "ipAddress": "192.168.4.1",
  "ipPort": 81,
  "ssid": "PhotoFrame-ABC123",
  "batteryLevel": 85,
  "batteryVoltageMv": 3850
}
```

**Fields:**
- `type` - Always `"board_info"`
- `board` - ESP32 board model name
- `flashSize` - Total flash storage in bytes
- `displayType` - Display type string (`"BW"` or `"6-Color"`)
- `displayWidth` - Display width in pixels
- `displayHeight` - Display height in pixels
- `displayRotation` - Current rotation (0-3, where 0=0°, 1=90°, 2=180°, 3=270°)
- `serverVersion` - Firmware version string
- `fileVersion` - PFR1 file format version supported
- `binaryFileSize` - Compiled firmware size in bytes
- `ipAddress` - Device IP address
- `ipPort` - WebSocket server port
- `ssid` - WiFi Access Point SSID
- `batteryLevel` - Battery percentage (0-100, optional)
- `batteryVoltageMv` - Battery voltage in millivolts (optional)

**Purpose:**
- Provides device capabilities to client
- Allows client to validate compatibility
- Enables client to generate correct image format
- Provides battery status for user feedback

##### 3. Init Message (Client → Server)

Sent to initialize an image upload session before sending image data.

**Format:**
```json
{
  "type": "init",
  "filename": "photo_2024_01_15.pfr1",
  "token": "a3f5c8e2",
  "timestamp": 1705334400,
  "orientation": 0
}
```

**Fields:**
- `type` - Always `"init"`
- `filename` - Image filename (typically with `.pfr1` extension)
- `token` - Hexadecimal token for session identification
- `timestamp` - Unix timestamp (seconds since epoch)
- `orientation` - Display orientation (0-3, where 0=0°, 1=90°, 2=180°, 3=270°)

**Purpose:**
- Prepares server to receive image data
- Establishes upload session with unique token
- Specifies image metadata (filename, timestamp, orientation)
- Triggers server to send ready response

##### 4. Ready Message (Server → Client)

Sent by server to indicate readiness to receive image chunks.

**Format:**
```json
{
  "type": "ready",
  "sessionId": "a3f5c8e2"
}
```

**Fields:**
- `type` - Always `"ready"`
- `sessionId` - Session identifier (matches token from init message)

**Purpose:**
- Confirms server is ready to receive chunks
- Provides session ID for tracking
- Signals client to begin sending binary chunks

##### 5. Chunk (Client → Server)

Binary WebSocket frame containing image data. Not a JSON message.

**Format:**
- Binary WebSocket frame
- Payload: Raw bytes from PFR1 file
- Size: 4096 bytes (except final chunk which may be smaller)

**Purpose:**
- Transfers image data in manageable chunks
- Allows progress tracking
- Prevents memory overflow on ESP32

##### 6. Chunk Acknowledgment Message (Server → Client)

Sent after each chunk is received to confirm receipt and report progress.

**Format:**
```json
{
  "type": "chunk_ack",
  "received": 8192
}
```

**Fields:**
- `type` - Always `"chunk_ack"`
- `received` - Total bytes received so far (cumulative)

**Purpose:**
- Confirms chunk was received successfully
- Provides cumulative byte count for progress calculation
- Signals client to send next chunk

##### 7. End Message (Client → Server)

Sent after all chunks have been transmitted to signal upload completion.

**Format:**
```json
{
  "type": "end"
}
```

**Fields:**
- `type` - Always `"end"`

**Purpose:**
- Signals that all chunks have been sent
- Triggers server to finalize upload
- Initiates server-side validation and processing

##### 8. Final Response Message (Server → Client)

Sent by server after processing the uploaded image to report final status.

**Format:**
```json
{
  "type": "final_response",
  "success": true,
  "message": "Image uploaded successfully",
  "filepath": "/spiffs/current.pfr1"
}
```

**Fields:**
- `type` - Always `"final_response"`
- `success` - Boolean indicating upload success
- `message` - Human-readable status message
- `filepath` - Path where image was saved on device

**Purpose:**
- Reports upload success or failure
- Provides status message for user feedback
- Indicates where image was stored

##### 9. Display Ready Message (Server → Client)

Sent after the e-paper display has been updated with the new image.

**Format:**
```json
{
  "type": "display_ready",
  "sessionId": "a3f5c8e2"
}
```

**Fields:**
- `type` - Always `"display_ready"`
- `sessionId` - Session identifier

**Purpose:**
- Confirms display update is complete
- Signals that device is ready for next operation
- Allows client to show completion status to user

**Note:** Display updates can take 20-40 seconds depending on display size and type. The client should wait for this message before considering the operation fully complete.

##### 10. Error Message (Server → Client)

Sent when an error occurs during any operation.

**Format:**
```json
{
  "type": "error",
  "message": "Invalid file format",
  "code": 400
}
```

**Fields:**
- `type` - Always `"error"`
- `message` - Human-readable error description
- `code` - Numeric error code (HTTP-style codes)

**Common Error Codes:**
- `400` - Bad request (invalid message format or parameters)
- `413` - Payload too large (file exceeds available storage)
- `500` - Internal server error (firmware error)
- `503` - Service unavailable (device busy or low battery)

**Purpose:**
- Reports errors to client
- Provides diagnostic information
- Allows client to display appropriate error messages

##### 11. Shutdown Message (Client → Server)

Sent to request device shutdown after operations are complete.

**Format:**
```json
{
  "type": "shutdown"
}
```

**Fields:**
- `type` - Always `"shutdown"`

**Purpose:**
- Requests graceful device shutdown
- Triggers server to close connections and enter deep sleep
- Conserves battery after operations complete

### Message Flow Sequence

This section describes the complete message exchange sequence for a typical image upload operation.

#### Complete Upload Sequence

```
Client                                    Server
  |                                         |
  |--- WebSocket Connection --------------->|
  |                                         |
  |--- handshake -------------------------->|
  |    {                                    |
  |      "type": "handshake",               |
  |      "clientVersion": "1.0.0",          |
  |      "platform": "flutter"              |
  |    }                                    |
  |                                         |
  |<-- board_info --------------------------|
  |    {                                    |
  |      "type": "board_info",              |
  |      "displayType": "6-Color",          |
  |      "displayWidth": 800,               |
  |      "displayHeight": 480,              |
  |      "batteryLevel": 85,                |
  |      ...                                |
  |    }                                    |
  |                                         |
  |--- init -------------------------------->|
  |    {                                    |
  |      "type": "init",                    |
  |      "filename": "photo.pfr1",          |
  |      "token": "a3f5c8e2",               |
  |      "timestamp": 1705334400,           |
  |      "orientation": 0                   |
  |    }                                    |
  |                                         |
  |<-- ready -------------------------------|
  |    {                                    |
  |      "type": "ready",                   |
  |      "sessionId": "a3f5c8e2"            |
  |    }                                    |
  |                                         |
  |--- chunk (binary, 4096 bytes) --------->|
  |                                         |
  |<-- chunk_ack ---------------------------|
  |    {                                    |
  |      "type": "chunk_ack",               |
  |      "received": 4096                   |
  |    }                                    |
  |                                         |
  |--- chunk (binary, 4096 bytes) --------->|
  |                                         |
  |<-- chunk_ack ---------------------------|
  |    {                                    |
  |      "type": "chunk_ack",               |
  |      "received": 8192                   |
  |    }                                    |
  |                                         |
  |    ... (repeat for all chunks) ...     |
  |                                         |
  |--- chunk (binary, 2048 bytes, final) -->|
  |                                         |
  |<-- chunk_ack ---------------------------|
  |    {                                    |
  |      "type": "chunk_ack",               |
  |      "received": 102400                 |
  |    }                                    |
  |                                         |
  |--- end --------------------------------->|
  |    {                                    |
  |      "type": "end"                      |
  |    }                                    |
  |                                         |
  |<-- final_response ----------------------|
  |    {                                    |
  |      "type": "final_response",          |
  |      "success": true,                   |
  |      "message": "Upload successful",    |
  |      "filepath": "/spiffs/current.pfr1" |
  |    }                                    |
  |                                         |
  |    ... (wait for display update) ...   |
  |                                         |
  |<-- display_ready -----------------------|
  |    {                                    |
  |      "type": "display_ready",           |
  |      "sessionId": "a3f5c8e2"            |
  |    }                                    |
  |                                         |
  |--- shutdown (optional) ---------------->|
  |    {                                    |
  |      "type": "shutdown"                 |
  |    }                                    |
  |                                         |
  |<-- WebSocket Close ---------------------|
  |                                         |
```

#### Sequence Phases

**Phase 1: Connection Establishment (Steps 1-2)**
- Client establishes WebSocket connection
- Client sends handshake with version and platform
- Server responds with board_info containing device capabilities

**Phase 2: Upload Initialization (Steps 3-4)**
- Client sends init with filename, token, timestamp, and orientation
- Server prepares to receive data and responds with ready message

**Phase 3: Chunked Data Transfer (Steps 5-N)**
- Client sends binary chunks of 4096 bytes each
- Server acknowledges each chunk with cumulative byte count
- Process repeats until all data is transferred
- Final chunk may be smaller than 4096 bytes

**Phase 4: Upload Finalization (Steps N+1 to N+2)**
- Client sends end message to signal completion
- Server validates data and responds with final_response

**Phase 5: Display Update (Step N+3)**
- Server updates e-paper display (20-40 seconds)
- Server sends display_ready when update completes

**Phase 6: Shutdown (Optional, Step N+4)**
- Client may send shutdown to conserve battery
- Server closes connection and enters deep sleep

#### Error Handling in Message Flow

**Error During Upload:**
```
Client                                    Server
  |                                         |
  |--- chunk (binary) ---------------------->|
  |                                         |
  |<-- error -------------------------------|
  |    {                                    |
  |      "type": "error",                   |
  |      "message": "Storage full",         |
  |      "code": 413                        |
  |    }                                    |
  |                                         |
  |--- (abort upload) ----------------------|
  |                                         |
```

When an error occurs, the server sends an error message and the client should abort the upload operation. The client may retry after addressing the error condition (e.g., freeing storage space).

#### Timeout Behavior

**Client Timeouts:**
- Chunk acknowledgment: 10 seconds per chunk
- Ready response: 10 seconds after init
- Final response: 10 seconds after end
- Display ready: 40 seconds after final_response

**Server Timeouts:**
- Idle connection: Configurable (WS_LISTEN_TIMEOUT_MS)
- Battery conservation: Automatic shutdown when no activity

If a timeout occurs, the client should close the connection and inform the user. The server will automatically clean up and return to idle state.

### Chunked Upload Mechanism

The image transfer system uses a chunked upload mechanism to reliably transfer large binary files over WebSocket while managing memory constraints on the ESP32.

#### Chunk Size Specification

**Chunk Size:** `4096 bytes` (4 KB)

This chunk size was chosen to balance several factors:
- **Memory Efficiency:** Fits comfortably in ESP32 RAM without causing memory pressure
- **Network Efficiency:** Large enough to minimize protocol overhead
- **Progress Granularity:** Provides smooth progress updates for user feedback
- **Error Recovery:** Limits data loss if a chunk fails to transfer

#### Chunking Process

**Step 1: File Preparation**

The client application reads the complete PFR1 binary file into memory or prepares it for streaming.

**Step 2: Chunk Division**

The file is divided into chunks of 4096 bytes:
```
File Size: 102,400 bytes
Chunk 1:   bytes 0-4095     (4096 bytes)
Chunk 2:   bytes 4096-8191  (4096 bytes)
Chunk 3:   bytes 8192-12287 (4096 bytes)
...
Chunk 24:  bytes 98304-102399 (4096 bytes)
Chunk 25:  bytes 102400-102399 (0 bytes, not sent)
```

**Step 3: Sequential Transmission**

Chunks are sent sequentially, one at a time:
1. Client sends chunk N as binary WebSocket frame
2. Client waits for chunk_ack from server
3. Client verifies received byte count
4. Client calculates and reports progress
5. Client sends chunk N+1
6. Process repeats until all chunks are sent

**Step 4: Final Chunk Handling**

The final chunk may be smaller than 4096 bytes:
```
File Size: 102,400 bytes
Final Chunk: bytes 98304-102399 (4096 bytes, full chunk)

File Size: 100,000 bytes
Final Chunk: bytes 98304-99999 (1696 bytes, partial chunk)
```

Both full and partial final chunks are handled identically by the protocol.

#### Implementation Example

**Client-Side Chunking (Pseudocode):**
```dart
const chunkSize = 4096;
final fileBytes = await readFile('image.pfr1');

for (int i = 0; i < fileBytes.length; i += chunkSize) {
  // Calculate chunk boundaries
  final endIndex = (i + chunkSize < fileBytes.length) 
      ? i + chunkSize 
      : fileBytes.length;
  
  // Extract chunk
  final chunk = fileBytes.sublist(i, endIndex);
  
  // Send chunk as binary WebSocket frame
  webSocket.sink.add(chunk);
  
  // Wait for acknowledgment with timeout
  final ack = await waitForChunkAck(timeout: Duration(seconds: 10));
  
  // Verify received byte count
  if (ack.received != endIndex) {
    throw Exception('Byte count mismatch');
  }
  
  // Calculate and report progress
  final progress = (ack.received / fileBytes.length).clamp(0.0, 1.0);
  progressCallback(progress);
}

// All chunks sent, send end message
await sendEndMessage();
```

**Server-Side Reception (Pseudocode):**
```cpp
int totalReceived = 0;

void onBinaryFrame(uint8_t* data, size_t length) {
  // Write chunk to file
  file.write(data, length);
  
  // Update total received
  totalReceived += length;
  
  // Send acknowledgment
  sendChunkAck(totalReceived);
}
```

#### Progress Calculation

Progress is calculated based on cumulative bytes received:

```
Progress = (Total Bytes Received / Total File Size) × 100%

Example:
File Size: 102,400 bytes
After Chunk 1: 4,096 / 102,400 = 4.0%
After Chunk 2: 8,192 / 102,400 = 8.0%
After Chunk 10: 40,960 / 102,400 = 40.0%
After Chunk 25: 102,400 / 102,400 = 100.0%
```

The client application can display this progress to the user in real-time as each chunk is acknowledged.

#### Error Handling and Recovery

**Chunk Timeout:**
If a chunk acknowledgment is not received within 10 seconds:
- Client aborts the upload
- Client displays timeout error to user
- Client may offer retry option

**Byte Count Mismatch:**
If the acknowledged byte count doesn't match expected:
- Client aborts the upload
- Client displays data corruption error
- Upload must be restarted from beginning

**Connection Loss:**
If WebSocket connection is lost during upload:
- Client detects connection closure
- Client aborts the upload
- Upload must be restarted from beginning

**Note:** The current implementation does not support resume functionality. If an upload fails, it must be restarted from the beginning.

#### Performance Characteristics

**Typical Upload Speeds:**
- WiFi connection: 100-500 KB/s
- Chunk overhead: ~2-5% (acknowledgment messages)
- 100 KB file: 2-5 seconds
- 500 KB file: 5-15 seconds
- 1 MB file: 10-30 seconds

**Memory Usage:**
- Client: One chunk (4096 bytes) in memory at a time
- Server: One chunk (4096 bytes) + file buffer
- Total ESP32 RAM impact: ~8-12 KB during upload

**Network Efficiency:**
- Chunk size: 4096 bytes
- Acknowledgment size: ~50 bytes JSON
- Overhead per chunk: ~1.2%
- Overall protocol efficiency: ~98%

#### Advantages of Chunked Upload

**Memory Efficiency:**
- ESP32 only needs to buffer one chunk at a time
- Prevents out-of-memory errors on large files
- Allows streaming directly to flash storage

**Reliability:**
- Each chunk is acknowledged before proceeding
- Detects transmission errors immediately
- Prevents silent data corruption

**User Experience:**
- Real-time progress feedback
- Smooth progress bar updates
- Clear indication of upload status

**Error Recovery:**
- Failures detected quickly (within one chunk)
- Limited data loss on connection issues
- Clear error reporting to user


## Usage Instructions

This section provides step-by-step instructions for using the WiFi/WebSocket image transfer system with both mobile and desktop applications.

### Mobile App Usage

The mobile app provides the most streamlined experience with QR code scanning for automatic connection setup.

#### Prerequisites

Before starting, ensure:
- Your mobile device has WiFi enabled
- The PhotoFrame mobile app is installed
- Camera permissions are granted for QR code scanning
- The ESP32 Photo Frame is powered on and displaying a QR code

#### Step-by-Step Instructions

**Step 1: Power On the ESP32 Photo Frame**

Press the power button or wake the device from sleep. The device will:
- Initialize the WiFi Access Point
- Generate a unique QR code with connection parameters
- Display the QR code on the e-paper screen

The QR code display includes:
- Large QR code in the center
- WiFi SSID below the QR code
- Battery level indicator (if available)
- Status text indicating "Ready to receive images"

**Step 2: Open the PhotoFrame Mobile App**

Launch the PhotoFrame app on your mobile device. The app will display the main screen with options to:
- Scan QR code to connect
- View previously connected devices
- Access app settings

**Step 3: Scan the QR Code**

Tap the "Scan QR Code" button in the app. This will:
- Open the device camera
- Display a viewfinder with scanning guides
- Automatically detect and scan the QR code

**Scanning Tips:**
- Hold your device 6-12 inches from the e-paper display
- Ensure good lighting conditions
- Keep the camera steady until the QR code is recognized
- The app will vibrate or beep when the scan is successful

**Step 4: Automatic Connection**

Once the QR code is scanned, the app will automatically:
1. Parse the connection parameters from the QR code
2. Display a connection progress indicator
3. Connect to the ESP32 WiFi Access Point
4. Establish the WebSocket connection
5. Exchange handshake messages
6. Display the device information screen

**Connection Progress Indicators:**
- "Connecting to WiFi..." (3-5 seconds)
- "Establishing secure connection..." (1-2 seconds)
- "Connected!" (success confirmation)

**Note:** On Android 12 and higher, the system may display a notification that the WiFi network has no internet access. This is expected and normal - the ESP32 Access Point is a local network and does not provide internet connectivity.

**Step 5: Select an Image**

Once connected, the app displays the device information screen showing:
- Device model and firmware version
- Display type (Black/White or 6-Color)
- Display resolution (width × height)
- Current battery level
- "Send Image" button

Tap the "Send Image" button to open the image picker. You can:
- Select a photo from your device gallery
- Take a new photo with the camera
- Browse recent photos

**Image Selection Tips:**
- Choose images with good contrast for best display results
- Landscape orientation images work best for most photo frames
- The app will automatically convert and optimize the image

**Step 6: Image Processing**

After selecting an image, the app will:
1. Display a preview of the selected image
2. Show orientation options (0°, 90°, 180°, 270°)
3. Allow you to adjust the orientation if needed
4. Display a "Send to Frame" button

**Orientation Selection:**
- **0°** - No rotation (default)
- **90°** - Rotate clockwise 90 degrees
- **180°** - Rotate 180 degrees (upside down)
- **270°** - Rotate counter-clockwise 90 degrees

Select the appropriate orientation and tap "Send to Frame".

**Step 7: Image Conversion**

The app will convert your image to the PFR1 binary format:
- Resize to match display resolution
- Apply dithering for the display type (BW or 6-Color)
- Compress to PFR1 format
- Display conversion progress

**Conversion Time:**
- Small images (< 1 MB): 1-3 seconds
- Medium images (1-5 MB): 3-8 seconds
- Large images (> 5 MB): 8-15 seconds

**Step 8: Image Upload**

The app uploads the converted image to the ESP32:
1. Sends initialization message with image metadata
2. Waits for ready signal from device
3. Uploads image in 4096-byte chunks
4. Displays real-time progress bar
5. Shows upload speed and estimated time remaining

**Upload Progress Display:**
```
Uploading image...
[████████████░░░░░░░░] 65%
32 KB / 48 KB
Speed: 12 KB/s
Time remaining: 2 seconds
```

**Upload Time Estimates:**
- 50 KB image: 2-5 seconds
- 100 KB image: 5-10 seconds
- 200 KB image: 10-20 seconds
- 500 KB image: 20-40 seconds

**Step 9: Display Update**

After the upload completes, the ESP32 updates the e-paper display:
1. App shows "Upload complete, updating display..."
2. ESP32 processes the image data
3. E-paper display refreshes with the new image
4. App receives display ready notification

**Display Update Time:**
- Black/White displays: 15-25 seconds
- 6-Color displays: 25-40 seconds

**Important:** Do not power off the device during the display update. The app will show a warning if you try to disconnect during this phase.

**Step 10: Completion**

Once the display update is complete:
- App shows "Image displayed successfully!" message
- App displays the updated battery level
- App offers options to:
  - Send another image
  - Disconnect and shutdown device
  - Return to main screen

**Battery Conservation:**
If you're finished sending images, tap "Shutdown Device" to:
- Close the WebSocket connection
- Disable the WiFi Access Point
- Put the ESP32 into deep sleep mode
- Maximize battery life

#### Troubleshooting Mobile App Issues

**QR Code Won't Scan:**
- Ensure camera permissions are granted
- Clean the camera lens
- Improve lighting conditions
- Try moving closer or farther from the display
- Manually enter connection details if QR scan fails repeatedly

**WiFi Connection Fails:**
- Verify the ESP32 is powered on and displaying the QR code
- Check that WiFi is enabled on your mobile device
- Forget other WiFi networks if your device is trying to auto-connect
- Restart the ESP32 and try again
- Move closer to the ESP32 (within 10 feet)

**Upload Fails or Times Out:**
- Check WiFi signal strength (move closer to ESP32)
- Verify the ESP32 has sufficient battery (> 20%)
- Try a smaller image file
- Restart both the app and the ESP32
- Check that the ESP32 has available storage space

**Display Update Doesn't Complete:**
- Wait the full display update time (up to 40 seconds)
- Check ESP32 battery level (low battery can cause failures)
- Verify the image format is correct (PFR1)
- Restart the ESP32 and retry the upload

**App Shows "No Internet" Warning:**
- This is normal and expected behavior
- The ESP32 Access Point is a local network without internet
- Ignore the warning and proceed with the connection
- On some devices, you may need to select "Stay connected" when prompted

### Desktop App Usage

The desktop app (Rust-based) provides a command-line interface for advanced users and automation scenarios.

#### Prerequisites

Before starting, ensure:
- The desktop app is installed and in your system PATH
- The ESP32 Photo Frame is powered on and displaying a QR code
- Your computer has WiFi capability
- You have the connection parameters from the QR code

#### Step-by-Step Instructions

**Step 1: Power On the ESP32 Photo Frame**

Power on the device and wait for it to display the QR code. Note the connection parameters shown on the display:
- WiFi SSID (e.g., "PhotoFrame-ABC123")
- IP Address (typically "192.168.4.1")
- Port (typically "81")

**Step 2: Connect to WiFi Access Point**

Manually connect your computer to the ESP32 WiFi Access Point:

**On macOS:**
```bash
# Click WiFi icon in menu bar
# Select the PhotoFrame SSID
# Or use command line:
networksetup -setairportnetwork en0 PhotoFrame-ABC123
```

**On Linux:**
```bash
# Using nmcli:
nmcli device wifi connect PhotoFrame-ABC123

# Or using wpa_supplicant:
sudo wpa_supplicant -B -i wlan0 -c <(wpa_passphrase PhotoFrame-ABC123)
sudo dhclient wlan0
```

**On Windows:**
```powershell
# Click WiFi icon in system tray
# Select the PhotoFrame SSID
# Or use command line:
netsh wlan connect name="PhotoFrame-ABC123"
```

**Step 3: Verify Connection**

Verify you're connected to the ESP32 Access Point:

```bash
# Ping the ESP32
ping 192.168.4.1

# Expected output:
# 64 bytes from 192.168.4.1: icmp_seq=0 ttl=64 time=5.2 ms
```

**Step 4: Prepare Your Image**

You can convert and upload images using one of the following methods:

**Option A: Flutter Desktop App (`flutter/desktop`)**

The Flutter desktop app provides a GUI for image conversion and upload:
1. Launch the desktop app
2. Drag and drop your image (JPEG/PNG)
3. Configure display settings (width, height, display type, orientation)
4. Preview the dithered result
5. Click "Upload to Device" and enter connection details (IP: 192.168.4.1, Port: 81)

**Option B: Rust CLI Processor + WebSocket Client**

Use the Rust command-line tools for conversion and upload:

```bash
# Step 1: Convert image to PFR1 format using rust/processor
cd rust/processor
cargo run --release -- \
  --input photo.jpg \
  --output photo.pfr1 \
  --t 6c \
  --orientation 0

# Step 2: Upload using rust/ws_client
cd ../ws_client
cargo run --release -- connect 192.168.4.1:81
```
**Step 6: Monitor Upload Progress**

The desktop app displays real-time upload progress:

```
Connecting to ws://192.168.4.1:81...
Connected successfully
Sending handshake...
Received board info:
  Device: ESP32-WROVER
  Display: 800x480 6-Color
  Battery: 85%
  Firmware: v2.1.0

Initializing upload...
Server ready to receive

Uploading photo.pfr1 (156,672 bytes)...
[████████████████░░░░] 80% (125,440 / 156,672 bytes)
Speed: 15.2 KB/s | Elapsed: 8.2s | Remaining: 2.1s
```

**Step 7: Wait for Display Update**

After the upload completes, wait for the display to update:

```
Upload complete!
Waiting for display update...
Display update in progress (this may take 20-40 seconds)...
Display ready!

Summary:
  File: photo.pfr1
  Size: 156,672 bytes
  Upload time: 10.3 seconds
  Display update time: 28.5 seconds
  Total time: 38.8 seconds
  Battery remaining: 83%

Image successfully displayed on device.
```

### General Usage Tips

**Battery Management:**
- Always check battery level before starting uploads
- Avoid uploads when battery is below 20%
- Use shutdown command after uploads to conserve battery
- The device will automatically shutdown after 5 minutes of inactivity

**Image Quality Tips:**
- Use high-contrast images for best results
- Avoid images with fine details or small text
- Landscape orientation works best for most frames
- Test different dithering settings for optimal appearance
- 6-Color displays show more detail than Black/White

**Network Performance:**
- Stay within 10 feet of the ESP32 for best performance
- Avoid areas with WiFi interference (microwaves, other routers)
- Upload speeds are typically 100-500 KB/s
- Larger images take proportionally longer to upload and display

**Storage Management:**
- The ESP32 stores the current image in flash memory
- Each new upload overwrites the previous image
- Check available storage space if uploads fail
- The device typically has 2-4 MB of available storage

**Firmware Updates:**
- Keep the mobile/desktop app updated for best compatibility
- Check firmware version in board info message
- Update ESP32 firmware if experiencing persistent issues
- Backup important images before firmware updates


## Troubleshooting

This section provides solutions to common issues encountered when using the WiFi/WebSocket image transfer system.

### Connection Issues

#### QR Code Scanning Problems

**Problem: QR Code Won't Scan or Takes Multiple Attempts**

**Symptoms:**
- Mobile app camera doesn't recognize the QR code
- Scanning takes more than 5-10 seconds
- App shows "Invalid QR code" error

**Possible Causes:**
- Poor lighting conditions
- Camera lens is dirty or obstructed
- QR code is too small or too large in camera view
- E-paper display contrast is low
- Camera permissions not granted

**Solutions:**
1. **Improve Lighting:** Ensure adequate lighting on the e-paper display. Avoid direct sunlight or glare that can wash out the QR code.
2. **Clean Camera Lens:** Wipe the camera lens with a soft cloth to remove smudges or fingerprints.
3. **Adjust Distance:** Hold your device 6-12 inches from the display. Move closer or farther until the QR code fills about 50-70% of the viewfinder.
4. **Check Permissions:** Verify the app has camera permissions in your device settings.
5. **Restart Display:** Power cycle the ESP32 to regenerate the QR code with better contrast.
6. **Manual Entry:** If scanning repeatedly fails, use the manual connection option in the app to enter the SSID, IP, and port directly.

**Prevention:**
- Keep the e-paper display clean and free of dust
- Ensure the ESP32 has sufficient battery (> 30%) for good display contrast
- Use the app in well-lit environments

**Problem: QR Code Contains Invalid or Incomplete Parameters**

**Symptoms:**
- App shows "Invalid connection parameters" error
- Missing IP address, SSID, or port in parsed data
- App crashes after scanning QR code

**Possible Causes:**
- Firmware bug or corruption
- QR code generation error
- Incomplete firmware initialization

**Solutions:**
1. **Restart ESP32:** Power cycle the device to reinitialize the WiFi Access Point and regenerate the QR code.
2. **Check Firmware Version:** Verify you're running a stable firmware version. Update if necessary.
3. **Factory Reset:** If the problem persists, perform a factory reset of the ESP32.
4. **Manual Connection:** Use manual entry mode to bypass QR code scanning.

**Prevention:**
- Keep firmware updated to the latest stable version
- Avoid interrupting the device during boot/initialization
- Ensure the device has sufficient battery during startup

#### WiFi Connection Failures

**Problem: Cannot Connect to ESP32 WiFi Access Point**

**Symptoms:**
- Mobile device cannot find the PhotoFrame SSID
- Connection attempt times out
- "Unable to join network" error message
- WiFi connection drops immediately after connecting

**Possible Causes:**
- ESP32 is not powered on or has shut down
- ESP32 is out of range
- WiFi Access Point failed to initialize
- Device WiFi is disabled or in airplane mode
- Too many devices connected to the ESP32 AP (maximum 4 clients)
- Interference from other WiFi networks

**Solutions:**
1. **Verify ESP32 is On:** Check that the ESP32 is powered on and displaying the QR code. If the display is blank, press the power button or charge the battery.
2. **Check WiFi is Enabled:** Ensure WiFi is enabled on your mobile device and airplane mode is off.
3. **Refresh WiFi Networks:** Pull down to refresh the list of available networks in your device's WiFi settings.
4. **Move Closer:** Move within 10 feet (3 meters) of the ESP32 for optimal signal strength.
5. **Forget Other Networks:** Temporarily forget other WiFi networks to prevent your device from auto-connecting away from the ESP32 AP.
6. **Restart ESP32:** Power cycle the ESP32 to reinitialize the WiFi Access Point.
7. **Disconnect Other Clients:** If multiple devices are connected, disconnect unused clients to free up connection slots.
8. **Check for Interference:** Move away from other WiFi routers, microwaves, or Bluetooth devices that may cause interference.

**Prevention:**
- Keep the ESP32 charged (> 20% battery)
- Stay within reasonable range (< 30 feet)
- Limit simultaneous connections to 2-3 devices
- Use the device in areas with minimal WiFi interference

**Problem: Connected to WiFi but "No Internet" Warning Appears**

**Symptoms:**
- Device shows connected to PhotoFrame SSID
- System displays "No Internet" or "Sign in to network" notification
- Some apps may not work while connected

**Possible Causes:**
- This is expected behavior - the ESP32 Access Point is a local network without internet access
- Operating system detects no internet connectivity and displays a warning

**Solutions:**
1. **Ignore the Warning:** This is normal and expected. The PhotoFrame app will work correctly despite the warning.
2. **Dismiss Notification:** Swipe away or dismiss the "No Internet" notification.
3. **Select "Stay Connected":** If prompted, choose to stay connected to the network even without internet.
4. **Disable Auto-Switch:** On some devices, disable the setting that automatically switches to mobile data when WiFi has no internet.

**Note:** This is not an error. The ESP32 Access Point provides local connectivity only and does not route traffic to the internet. The PhotoFrame app communicates directly with the ESP32 over the local network.

**Prevention:**
- Understand this is expected behavior
- Configure your device to not auto-switch networks
- Educate users that the warning is normal

#### WebSocket Connection Failures

**Problem: WebSocket Connection Timeout or Refused**

**Symptoms:**
- App shows "Connection timeout" error
- "Connection refused" error message
- App hangs on "Connecting..." screen
- Connection fails after WiFi is established

**Possible Causes:**
- Incorrect IP address or port number
- WebSocket server not running on ESP32
- Firewall blocking connection (rare on mobile devices)
- ESP32 has shut down or entered deep sleep
- Network binding issue (Android 12+)

**Solutions:**
1. **Verify Connection Parameters:** Double-check the IP address (typically 192.168.4.1) and port (typically 81) match the QR code.
2. **Restart ESP32:** Power cycle the device to restart the WebSocket server.
3. **Check WiFi Connection:** Verify you're still connected to the PhotoFrame WiFi network.
4. **Ping the ESP32:** Use a network utility app to ping the IP address and verify connectivity.
5. **Restart the App:** Force close and reopen the PhotoFrame app.
6. **Check Battery:** Ensure the ESP32 has sufficient battery (> 20%). Low battery can cause the WebSocket server to fail.
7. **Wait and Retry:** If the ESP32 just powered on, wait 10-15 seconds for full initialization before connecting.

**Prevention:**
- Always verify you're connected to the correct WiFi network
- Ensure the ESP32 is fully booted before attempting connection
- Keep the ESP32 charged above 20%
- Use the QR code scan feature to avoid manual entry errors

**Problem: Handshake Fails or No Board Info Response**

**Symptoms:**
- WebSocket connects but no response from server
- Timeout waiting for board info message
- App shows "Handshake failed" error

**Possible Causes:**
- WebSocket server is overloaded or unresponsive
- Firmware bug or crash
- Incompatible protocol version
- Memory exhaustion on ESP32

**Solutions:**
1. **Restart ESP32:** Power cycle the device to reset the WebSocket server.
2. **Update Firmware:** Ensure the ESP32 firmware is up to date and compatible with your app version.
3. **Update App:** Ensure the mobile/desktop app is the latest version.
4. **Check Logs:** If possible, connect to the ESP32 serial console to check for error messages.
5. **Reduce Load:** Disconnect other clients that may be connected to the ESP32.
6. **Factory Reset:** If the problem persists, perform a factory reset of the ESP32.

**Prevention:**
- Keep firmware and app versions synchronized
- Avoid connecting multiple clients simultaneously during uploads
- Monitor ESP32 memory usage and restart periodically

### Transfer Issues

#### Upload Failures

**Problem: Upload Starts but Fails Mid-Transfer**

**Symptoms:**
- Upload progress stops at a specific percentage
- "Chunk acknowledgment timeout" error
- Connection drops during upload
- App shows "Upload failed" error

**Possible Causes:**
- WiFi signal strength degraded
- ESP32 battery critically low
- Flash storage full or corrupted
- Memory exhaustion on ESP32
- WiFi interference
- File corruption

**Solutions:**
1. **Check Signal Strength:** Move closer to the ESP32 (within 10 feet) to improve signal strength.
2. **Check Battery:** Verify ESP32 battery is above 20%. Charge if necessary.
3. **Check Storage:** Ensure the ESP32 has sufficient free flash storage (at least 500 KB free).
4. **Reduce File Size:** Try uploading a smaller image or reduce image resolution.
5. **Restart Both Devices:** Power cycle the ESP32 and restart the mobile app.
6. **Retry Upload:** Attempt the upload again. Temporary interference may have caused the failure.
7. **Check File Integrity:** Verify the PFR1 file is not corrupted by re-converting the image.

**Prevention:**
- Keep the ESP32 charged above 30% during uploads
- Stay within 10 feet of the device during transfers
- Avoid uploading very large files (> 1 MB)
- Minimize WiFi interference from other devices
- Regularly check and free up flash storage space

**Problem: Chunk Acknowledgment Timeout**

**Symptoms:**
- Upload stops with "No ACK for chunk X" error
- Timeout after 10 seconds waiting for acknowledgment
- Upload fails at the same chunk repeatedly

**Possible Causes:**
- ESP32 is busy processing previous chunk
- Flash write operation is slow or failing
- Memory pressure on ESP32
- Network packet loss
- ESP32 has crashed or frozen

**Solutions:**
1. **Wait and Retry:** The ESP32 may be temporarily busy. Wait 30 seconds and retry the upload.
2. **Restart ESP32:** Power cycle the device to clear any stuck state.
3. **Check Flash Health:** Flash storage may be failing. Try uploading to a different filename or perform a factory reset.
4. **Reduce Upload Speed:** If using desktop app, add delays between chunks (not currently supported in standard app).
5. **Check Serial Logs:** Connect to serial console to see if ESP32 is reporting errors.
6. **Update Firmware:** Flash the latest firmware version which may include bug fixes.

**Prevention:**
- Keep firmware updated
- Avoid interrupting uploads in progress
- Regularly restart the ESP32 to clear memory
- Monitor flash storage health

**Problem: Upload Completes but Display Update Fails**

**Symptoms:**
- Upload shows 100% complete
- "Display update timeout" error after 40 seconds
- Display doesn't show the new image
- ESP32 appears frozen

**Possible Causes:**
- Battery too low to drive display update
- Invalid or corrupted image data
- Display driver error
- Insufficient power for e-paper refresh
- Firmware bug in display update code

**Solutions:**
1. **Check Battery:** Ensure battery is above 30%. E-paper displays require significant power for updates. Charge the device and retry.
2. **Verify Image Format:** Ensure the PFR1 file is valid and matches the display specifications (resolution, color depth).
3. **Wait Longer:** Some display updates can take up to 60 seconds. Wait an additional 20-30 seconds.
4. **Restart ESP32:** Power cycle the device. The image may be saved and will display on next boot.
5. **Re-upload Image:** Try uploading the image again.
6. **Try Different Image:** Upload a simpler image to test if the problem is image-specific.
7. **Check Power Supply:** If using USB power, ensure the power supply provides sufficient current (at least 500mA).

**Prevention:**
- Always maintain battery above 30% before uploads
- Test images with simple content first
- Use a high-quality power supply for USB-powered devices
- Keep firmware updated with display driver fixes

#### Image Quality Issues

**Problem: Image Appears Distorted or Incorrect Colors**

**Symptoms:**
- Image displays but colors are wrong
- Image is stretched or squashed
- Image appears rotated incorrectly
- Dithering artifacts are excessive

**Possible Causes:**
- Incorrect display type specified (BW vs 6-Color)
- Wrong resolution in PFR1 conversion
- Incorrect orientation setting
- Image conversion error

**Solutions:**
1. **Verify Display Type:** Ensure you selected the correct display type (BW or 6-Color) when converting the image.
2. **Check Resolution:** Verify the image was converted to match the display resolution (e.g., 800x480).
3. **Adjust Orientation:** Try different orientation settings (0°, 90°, 180°, 270°) to find the correct rotation.
4. **Re-convert Image:** Convert the original image again with correct parameters.
5. **Check Source Image:** Ensure the source image is high quality and not already corrupted.

**Prevention:**
- Always use the QR code scan to get correct display parameters
- Double-check conversion settings before uploading
- Test with known-good images first
- Keep a library of successfully displayed images for reference

### Performance Issues

**Problem: Upload Speed is Very Slow**

**Symptoms:**
- Upload takes much longer than expected
- Transfer speed below 50 KB/s
- Progress bar moves very slowly

**Possible Causes:**
- Weak WiFi signal
- WiFi interference from other devices
- ESP32 is far from client device
- Multiple clients connected simultaneously
- ESP32 CPU is overloaded

**Solutions:**
1. **Move Closer:** Position your device within 5-10 feet of the ESP32.
2. **Reduce Interference:** Move away from other WiFi routers, microwaves, or Bluetooth devices.
3. **Disconnect Other Clients:** Ensure only one device is connected during upload.
4. **Restart ESP32:** Power cycle to clear any performance issues.
5. **Check Battery:** Low battery can reduce WiFi transmission power. Charge above 50%.
6. **Use Different WiFi Channel:** If possible, configure the ESP32 to use a less congested WiFi channel (requires firmware modification).

**Expected Performance:**
- Good conditions: 200-500 KB/s
- Average conditions: 100-200 KB/s
- Poor conditions: 50-100 KB/s
- Below 50 KB/s indicates a problem

**Prevention:**
- Maintain good signal strength (stay close to device)
- Minimize WiFi interference
- Upload during off-peak times if in a crowded WiFi environment
- Keep battery charged above 50%

### Error Messages

#### Common Error Codes

**Error 400: Bad Request**

**Message:** "Invalid file format" or "Invalid message format"

**Cause:** The server received malformed data or an invalid message.

**Solutions:**
- Verify the PFR1 file is valid and not corrupted
- Ensure the app is sending properly formatted JSON messages
- Re-convert the image to PFR1 format
- Update the app to the latest version

**Error 413: Payload Too Large**

**Message:** "File too large" or "Storage full"

**Cause:** The image file exceeds available flash storage space.

**Solutions:**
- Reduce image resolution or file size
- Free up storage space on the ESP32 (may require firmware access)
- Check available storage using device info command
- Delete old images if possible

**Error 500: Internal Server Error**

**Message:** "Server error" or "Processing failed"

**Cause:** The ESP32 firmware encountered an internal error.

**Solutions:**
- Restart the ESP32
- Check serial logs for detailed error information
- Update firmware to the latest version
- Report the issue if it persists

**Error 503: Service Unavailable**

**Message:** "Device busy" or "Low battery"

**Cause:** The device cannot process the request due to low battery or being busy with another operation.

**Solutions:**
- Charge the battery above 30%
- Wait for current operation to complete
- Restart the ESP32 if it appears stuck
- Disconnect other clients

### Diagnostic Tools

#### Network Connectivity Testing

**Ping Test:**
```bash
# Test basic connectivity to ESP32
ping 192.168.4.1

# Expected output:
# 64 bytes from 192.168.4.1: icmp_seq=0 ttl=64 time=5.2 ms
```

**WebSocket Connection Test:**
```bash
# Test WebSocket connection using wscat (install: npm install -g wscat)
wscat -c ws://192.168.4.1:81

# Expected output:
# Connected (press CTRL+C to quit)

# Send handshake:
# {"type":"handshake","clientVersion":"1.0.0","platform":"test"}

# Expected response:
# {"type":"board_info","board":"ESP32-WROVER",...}
```

## Battery Management

The ESP32 Photo Frame includes intelligent battery management features to maximize battery life while maintaining a responsive user experience. This section describes how battery conservation works during WiFi/WebSocket operations and how to optimize battery usage.

### Battery Consumption Overview

WiFi and WebSocket operations are the most power-intensive activities on the ESP32 Photo Frame. Understanding power consumption helps you optimize battery life.

**Power Consumption by Activity:**

| Activity | Current Draw | Battery Impact |
|----------|--------------|----------------|
| Deep Sleep | 10-50 µA | Minimal (months of standby) |
| Display Only | 1-5 mA | Low (weeks of display time) |
| WiFi AP Idle | 80-120 mA | Moderate (8-12 hours) |
| WiFi AP + WebSocket Active | 120-180 mA | High (5-8 hours) |
| Image Upload (WiFi + Flash Write) | 150-250 mA | Very High (3-5 hours) |
| Display Update (E-paper Refresh) | 200-400 mA | Peak (2-3 hours if continuous) |

**Battery Life Estimates:**

For a typical 2000 mAh battery:
- **Deep Sleep Mode:** 4-6 months (display updates once per day)
- **WiFi AP Idle:** 10-16 hours continuous
- **Active Uploads:** 8-13 hours of continuous uploading
- **Mixed Usage:** 2-4 weeks (1-2 uploads per day, rest in deep sleep)

**Note:** Actual battery life depends on battery capacity, age, temperature, and usage patterns.

### Automatic Timeout Mechanism

The ESP32 implements an intelligent timeout system that automatically shuts down the WiFi Access Point and WebSocket server when not in use, conserving battery power.

#### How the Timeout Works

**Timeout Configuration:**
- **Timeout Duration:** Configurable via `WS_LISTEN_TIMEOUT_MS` (typically 5 minutes / 300,000 ms)
- **Battery Check Interval:** `WS_BATTERY_CHECK_INTERVAL_MS` (typically 60 seconds / 60,000 ms)

**Timeout Behavior:**

The timeout timer operates in a "pausable" mode:
- **Timer Active:** Counts down when the system is idle (no client connected, no upload in progress)
- **Timer Paused:** Stops counting during active operations (client connected, upload active, display updating)
- **Timer Resumes:** Continues counting when operations complete

**Timeout States:**

```
┌─────────────────────────────────────────────────────────┐
│ ESP32 Powers On                                         │
│ WiFi AP Started                                         │
│ WebSocket Server Started                                │
│ QR Code Displayed                                       │
└────────────────┬────────────────────────────────────────┘
                 │
                 ▼
┌─────────────────────────────────────────────────────────┐
│ IDLE STATE                                              │
│ - Timeout timer ACTIVE (counting down)                  │
│ - Waiting for client connection                         │
│ - Battery check every 60 seconds                        │
└────────────────┬────────────────────────────────────────┘
                 │
                 │ Client Connects
                 ▼
┌─────────────────────────────────────────────────────────┐
│ CLIENT CONNECTED STATE                                  │
│ - Timeout timer PAUSED                                  │
│ - WebSocket handshake in progress                       │
│ - Battery monitoring continues                          │
└────────────────┬────────────────────────────────────────┘
                 │
                 │ Upload Starts
                 ▼
┌─────────────────────────────────────────────────────────┐
│ UPLOAD ACTIVE STATE                                     │
│ - Timeout timer PAUSED                                  │
│ - Receiving image chunks                                │
│ - Writing to flash storage                              │
│ - Battery monitoring continues                          │
└────────────────┬────────────────────────────────────────┘
                 │
                 │ Upload Complete
                 ▼
┌─────────────────────────────────────────────────────────┐
│ DISPLAY UPDATE STATE                                    │
│ - Timeout timer PAUSED                                  │
│ - E-paper display refreshing (20-40 seconds)            │
│ - High power consumption                                │
└────────────────┬────────────────────────────────────────┘
                 │
                 │ Display Complete
                 ▼
┌─────────────────────────────────────────────────────────┐
│ CLIENT CONNECTED (IDLE)                                 │
│ - Timeout timer PAUSED (client still connected)         │
│ - Waiting for next operation or disconnect              │
└────────────────┬────────────────────────────────────────┘
                 │
                 │ Client Disconnects
                 ▼
┌─────────────────────────────────────────────────────────┐
│ IDLE STATE                                              │
│ - Timeout timer RESUMES (counting down from remaining)  │
│ - Waiting for next client or timeout                    │
└────────────────┬────────────────────────────────────────┘
                 │
                 │ Timeout Expires (5 minutes of idle)
                 ▼
┌─────────────────────────────────────────────────────────┐
│ SHUTDOWN SEQUENCE                                       │
│ - Close all WebSocket connections                       │
│ - Stop WebSocket server                                 │
│ - Disable WiFi Access Point                             │
│ - Enter deep sleep mode                                 │
└─────────────────────────────────────────────────────────┘
```

#### Timeout Examples

**Example 1: Quick Upload and Disconnect**
```
00:00 - ESP32 powers on, WiFi AP starts, timeout timer starts (5:00 remaining)
00:15 - User scans QR code, client connects → timeout timer PAUSES
00:20 - Upload starts → timeout timer remains PAUSED
00:35 - Upload completes, display update starts → timeout timer remains PAUSED
01:05 - Display update completes → timeout timer remains PAUSED (client connected)
01:10 - User disconnects → timeout timer RESUMES (5:00 remaining)
06:10 - Timeout expires → ESP32 enters deep sleep
```

**Example 2: Multiple Uploads**
```
00:00 - ESP32 powers on, timeout timer starts (5:00 remaining)
00:30 - Client connects → timeout timer PAUSES
00:35 - First upload (30 seconds) → timeout timer remains PAUSED
01:35 - Display update (30 seconds) → timeout timer remains PAUSED
02:05 - Second upload (25 seconds) → timeout timer remains PAUSED
03:00 - Display update (28 seconds) → timeout timer remains PAUSED
03:28 - Client disconnects → timeout timer RESUMES (5:00 remaining)
08:28 - Timeout expires → ESP32 enters deep sleep
```

**Example 3: Idle Timeout**
```
00:00 - ESP32 powers on, timeout timer starts (5:00 remaining)
05:00 - No client connected, timeout expires → ESP32 enters deep sleep
```

**Example 4: Manual Shutdown**
```
00:00 - ESP32 powers on, timeout timer starts (5:00 remaining)
00:20 - Client connects → timeout timer PAUSES
00:25 - Upload completes, display updates → timeout timer remains PAUSED
01:00 - User sends shutdown command → ESP32 immediately enters deep sleep
```

### Battery Monitoring During Transfer

The ESP32 continuously monitors battery status during WiFi/WebSocket operations and provides real-time battery information to connected clients.

#### Battery Status Reporting

**Board Info Message:**
When a client connects, the ESP32 sends battery information in the `board_info` message:

```json
{
  "type": "board_info",
  "batteryLevel": 85,
  "batteryVoltageMv": 3850,
  ...
}
```

**Battery Fields:**
- `batteryLevel` - Battery percentage (0-100), optional field
- `batteryVoltageMv` - Battery voltage in millivolts, optional field

**Battery Update Frequency:**
- Initial report: Sent immediately in board_info response
- Periodic updates: Every 60 seconds during active connection (via `WS_BATTERY_CHECK_INTERVAL_MS`)
- On-demand: Can be requested by client (if supported by firmware)

**Battery Level Interpretation:**

| Battery Level | Voltage (typical) | Status | Recommended Action |
|---------------|-------------------|--------|-------------------|
| 100% | 4.2V (4200 mV) | Full | Normal operation |
| 80-99% | 3.9-4.1V | Good | Normal operation |
| 50-79% | 3.7-3.9V | Fair | Normal operation |
| 30-49% | 3.6-3.7V | Low | Complete uploads soon |
| 20-29% | 3.5-3.6V | Very Low | Charge recommended |
| 10-19% | 3.4-3.5V | Critical | Charge immediately |
| < 10% | < 3.4V | Empty | Device may shutdown |

**Note:** Voltage ranges are approximate and depend on battery chemistry (LiPo, Li-ion) and load conditions.

#### Low Battery Protection

The ESP32 firmware includes protection mechanisms to prevent battery damage and data loss due to low battery conditions.

**Low Battery Thresholds:**

**Warning Level (< 30%):**
- System continues normal operation
- Client apps should display battery warning to user
- Recommend completing current operation and charging

**Critical Level (< 20%):**
- System may reject new upload requests (returns error 503)
- Existing uploads are allowed to complete
- Display updates may be skipped to conserve power
- Automatic shutdown timer may be shortened

**Emergency Level (< 10%):**
- System rejects all new operations
- WebSocket server may shutdown immediately
- Device enters deep sleep to prevent battery damage
- Display shows low battery warning before shutdown

**Battery Protection Features:**
- **Under-voltage Protection:** Hardware prevents battery discharge below safe voltage (typically 3.0V)
- **Graceful Shutdown:** Firmware closes connections and saves state before shutdown
- **Data Integrity:** Flash writes are completed before power-off to prevent corruption
- **Wake-up Prevention:** Device will not wake from deep sleep if battery is too low

#### Battery Conservation Best Practices

**For Users:**

1. **Charge Before Use:** Ensure battery is above 30% before starting uploads
2. **Monitor Battery Level:** Check battery status in the app before and during uploads
3. **Use Shutdown Command:** Always use the shutdown command after uploads to conserve battery
4. **Avoid Continuous Operation:** Don't leave WiFi AP running continuously for hours
5. **Plan Uploads:** Upload multiple images in one session rather than multiple short sessions
6. **Charge Regularly:** Charge the device after every 5-10 uploads or weekly

**For Developers:**

1. **Display Battery Status:** Show battery level prominently in the app UI
2. **Warn Users:** Display warnings when battery is below 30%
3. **Prevent Low Battery Uploads:** Optionally prevent uploads when battery is critically low
4. **Auto-Shutdown:** Implement auto-shutdown after successful uploads
5. **Estimate Battery Usage:** Calculate and display estimated battery consumption for uploads
6. **Battery Alerts:** Notify users if battery drops significantly during upload

### Power Optimization Strategies

#### Minimizing Power Consumption

**Optimize Upload Sessions:**
- **Batch Uploads:** Upload multiple images in one session rather than separate sessions
- **Pre-convert Images:** Convert images to PFR1 format before connecting to reduce processing time
- **Optimize Image Size:** Use appropriate resolution and compression to minimize upload time
- **Quick Disconnect:** Disconnect immediately after display update completes

**Optimize WiFi Usage:**
- **Stay Close:** Maintain good signal strength to reduce transmission power and retries
- **Minimize Interference:** Reduce WiFi interference to improve transfer efficiency
- **Limit Connections:** Connect only one client at a time during uploads
- **Use Shutdown Command:** Always shutdown the device when finished

**Optimize Display Updates:**
- **Partial Updates:** If supported, use partial display updates instead of full refreshes
- **Reduce Update Frequency:** Don't update the display unnecessarily
- **Skip Redundant Updates:** Don't upload the same image multiple times

#### Power Consumption During Operations

**Typical Power Profile for One Upload:**

```
Power (mA)
400 |                    ╭─╮
    |                    │ │ Display Update
300 |                    │ │
    |                    │ │
200 |        ╭───────────╯ ╰─╮
    |        │ Upload          │
100 |    ╭───╯                 ╰───╮
    |╭───╯ Connect                 ╰───╮
 50 |│ Idle                             │ Idle
    |│                                  │
  0 └┴──────────────────────────────────┴─────
    0   1   2   3   4   5   6   7   8   9  10
                    Time (minutes)

Total Energy: ~15-25 mAh per upload cycle
Battery Impact: 0.75-1.25% per upload (2000 mAh battery)
```

**Energy Breakdown:**
- WiFi AP Idle (5 min): ~10 mAh
- Connection & Handshake (30 sec): ~1 mAh
- Image Upload (1 min): ~3 mAh
- Display Update (30 sec): ~3-5 mAh
- Post-upload Idle (2 min): ~4 mAh
- **Total per Upload: ~21-23 mAh**

**Battery Life Calculation:**

For a 2000 mAh battery:
- **Uploads per Charge:** ~85-95 uploads (assuming 23 mAh per upload)
- **Daily Usage (2 uploads/day):** ~40-45 days per charge
- **Daily Usage (5 uploads/day):** ~17-19 days per charge
- **Heavy Usage (10 uploads/day):** ~8-9 days per charge

**Note:** These are estimates. Actual battery life depends on battery health, temperature, image size, and display type.

### Deep Sleep Mode

Deep sleep is the primary battery conservation mechanism, reducing power consumption to minimal levels when the device is not actively displaying or transferring images.

#### Deep Sleep Characteristics

**Power Consumption:**
- **Deep Sleep Current:** 10-50 µA (microamps)
- **Standby Time:** 4-6 months on a 2000 mAh battery
- **Wake-up Time:** 1-2 seconds from deep sleep to WiFi AP ready

**What's Preserved in Deep Sleep:**
- Current displayed image (e-paper displays retain image without power)
- Configuration settings in flash memory
- RTC (Real-Time Clock) state for wake-up timers

**What's Disabled in Deep Sleep:**
- WiFi radio (completely off)
- WebSocket server (not running)
- CPU (halted, only RTC active)
- Most peripherals (disabled)

#### Wake-up Methods

**Manual Wake-up:**
- Press the power button or wake button
- Device boots, initializes WiFi AP, displays QR code
- Ready for connections in 5-10 seconds

**Scheduled Wake-up (if configured):**
- RTC timer wakes device at scheduled time
- Useful for automatic daily image updates
- Requires firmware configuration

**External Wake-up (if configured):**
- GPIO pin trigger (e.g., motion sensor, button)
- Requires hardware and firmware support

#### Entering Deep Sleep

**Automatic Entry:**
- Timeout expires after 5 minutes of inactivity
- Low battery condition (< 10%)
- Firmware error or crash (safety shutdown)

**Manual Entry:**
- Client sends shutdown command
- User presses power button (if configured)
- Serial command (for debugging)

**Shutdown Sequence:**
1. Close all active WebSocket connections
2. Send disconnect messages to clients
3. Stop WebSocket server
4. Disable WiFi Access Point
5. Save state to flash memory
6. Configure wake-up sources
7. Enter deep sleep mode

**Shutdown Time:** Typically 1-2 seconds for graceful shutdown

