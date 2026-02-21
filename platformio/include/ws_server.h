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

#pragma once

#ifdef ENABLE_WEBSERVER_DATAPROVIDER

#include "psram_allocator.h"
#include <Arduino.h>
#include <FS.h>
#include <WebSocketsServer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <functional>

namespace photo_frame {
namespace ws {

/**
 * @brief WebSocket event types
 */
enum class WSEventType {
  ERROR,               // Error occurred
  IMAGE_RECEIVED,      // Complete image received
  CLIENT_CONNECTED,    // Client connected
  CLIENT_DISCONNECTED, // Client disconnected
  SHUTDOWN_REQUEST     // Shutdown command received
};

/**
 * @brief WebSocket event data structure
 */
struct WSEvent {
  WSEventType type;
  String message;        // Error message or status info
  String filepath;       // Path to saved image file in LittleFS (for IMAGE_RECEIVED)
  String filename;       // Filename of uploaded image
  uint32_t timestamp;    // Client timestamp
  uint8_t orientation;   // Display orientation (0-3)
  uint32_t clientsCount; // Number of currently connected clients
  uint8_t clientId;      // Client ID for connection/disconnection events

  WSEvent() : type(WSEventType::ERROR), filepath(""), filename(""), timestamp(0), orientation(0), clientsCount(0), clientId(255) {}
};

/**
 * @brief Callback function type for WebSocket events
 */
using WSEventCallback = std::function<void(const WSEvent &)>;

/**
 * @brief WebSocket Server running on separate FreeRTOS task
 *
 * Manages WebSocket connections for receiving images from clients.
 * Runs on a dedicated FreeRTOS task to avoid blocking main thread.
 */
class WSServer {
public:
  /**
   * @brief Get active client ID (255 if no active client)
   * @return Active client ID or 255 if no client connected
   */
  uint8_t getActiveClientId() const;

  /**
   * @brief Get IP address of a connected client by ID
   * @param clientId Client ID to query
   * @return IPAddress of the client, or
   * IPAddress(0,0,0,0) if client not found or no WebSocket instance
   */
  IPAddress getClientIp(uint8_t clientId) const;
  /**
   * @brief Send display_ready message to all clients
   */
  void sendDisplayReadyMessage();

  /**
   * @brief Get upload client id
   */
  uint8_t getUploadClientId() const { return m_uploadClientId; }

  /**
   * @brief Construct WebSocket server
   * @param port WebSocket server port
   * @param callback Event callback function
   */
  WSServer(uint16_t port, WSEventCallback callback);

  /**
   * @brief Destructor
   */
  ~WSServer();

  /**
   * @brief Start WebSocket server on separate task
   * @return true if server started successfully
   */
  bool begin();

  /**
   * @brief Stop WebSocket server and task
   */
  void stop();

  /**
   * @brief Check if server is running
   * @return true if server is running
   */
  bool isRunning() const { return m_running; }

  /**
   * @brief Check if an upload session is active
   * @return true if upload in progress
   */
  bool isUploadActive() const { return m_uploadActive; }

  /**
   * @brief Get server port
   * @return Port number
   */
  uint16_t getPort() const { return m_port; }

  /**
   * @brief Get time of last activity (connection or upload)
   * @return Milliseconds since last activity (millis())
   */
  unsigned long getLastActivityMs() const { return m_lastActivityMs; }

  bool isClientConnected() const { return m_connectedClientsCount > 0; }

private:
  /**
   * @brief FreeRTOS task function (static wrapper)
   */
  static void taskFunction(void *parameter);

  /**
   * @brief Main server loop (runs in task)
   */
  void serverLoop();

  /**
   * @brief Handle WebSocket message
   * @param data Message data
   * @param len Message length
   */
  void handleMessage(const uint8_t *data, size_t len);

  /**
   * @brief Handle WebSocket events from library
   * @param num Client number
   * @param type Event type
   * @param payload Event payload
   * @param length Payload length
   */
  void handleWebSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length);

  /**
   * @brief Handle JSON control messages for upload
   */
  void handleControlMessage(uint8_t num, const String &message);

  /**
   * @brief Reset upload session state
   */
  void resetUploadSession(bool deleteFile, const char *reason);

  /**
   * @brief Check upload timeout and reset if needed
   */
  void checkUploadTimeout();

  /**
   * @brief Check heartbeat ping/pong and disconnect if needed
   */
  void checkHeartbeat();

  /**
   * @brief Reset heartbeat state for active client
   */
  void resetHeartbeatState();

  /**
   * @brief Send event to callback
   * @param event Event to send
   */
  void sendEvent(const WSEvent &event);

  uint16_t m_port;
  WSEventCallback m_callback;
  TaskHandle_t m_taskHandle;
  bool m_running;

  // WebSocket server instance
  WebSocketsServer *m_webSocket;

  // Connected clients tracking (multiple clients supported)
  uint32_t m_connectedClientsCount;
  uint8_t m_activeClientId;
  unsigned long m_lastPingMs;
  unsigned long m_lastPongMs;
  bool m_waitingPong;

  // Upload session state
  bool m_uploadActive;
  uint8_t m_uploadClientId;
  String m_uploadFilename;
  String m_uploadToken;
  uint32_t m_uploadTimestamp;
  uint8_t m_uploadOrientation;
  size_t m_uploadBytesReceived;
  unsigned long m_lastUploadActivityMs;
  unsigned long m_lastActivityMs; // General activity tracking (connections,
                                  // uploads, etc.)
  File m_uploadFile;

  // Image reception state (legacy buffer-based)
  photo_frame::PSRAMUniquePtr m_imageBuffer;
  size_t m_imageBufferSize;
  size_t m_imageReceivedSize;
  bool m_receivingImage;
};

} // namespace ws
} // namespace photo_frame

#endif // ENABLE_WEBSERVER_DATAPROVIDER