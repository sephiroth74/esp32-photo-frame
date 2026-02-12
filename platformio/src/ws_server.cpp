#include "ws_server.h"
#include "binary_utils.h"
#include "config.h"
#include "littlefs_manager.h"
#include "pfr1_config.h"
#include "psram_allocator.h"
#include "renderer.h"
#include "ws_types.h"
#include <ArduinoJson.h>
#include <esp_log.h>

// External flag from main_ws.cpp to track if image is being loaded
extern volatile bool g_isLoadingImage;

namespace photo_frame {
namespace ws {

// FreeRTOS task configuration
#define WS_TASK_STACK_SIZE 8192
#define WS_TASK_PRIORITY 5
#define WS_TASK_CORE 1 // Run on core 1

// Heartbeat configuration
#define WS_HEARTBEAT_INTERVAL_MS 60000
#define WS_HEARTBEAT_TIMEOUT_MS 15000

    WSServer::WSServer(uint16_t port, WSEventCallback callback)
        : m_port(port)
        , m_callback(callback)
        , m_taskHandle(nullptr)
        , m_running(false)
        , m_webSocket(nullptr)
        , m_connectedClientsCount(0)
        , m_activeClientId(255)
        , m_lastPingMs(0)
        , m_lastPongMs(0)
        , m_waitingPong(false)
        , m_uploadActive(false)
        , m_uploadClientId(255)
        , m_uploadFilename("")
        , m_uploadToken("")
        , m_uploadTimestamp(0)
        , m_uploadOrientation(0)
        , m_uploadBytesReceived(0)
        , m_lastUploadActivityMs(0)
        , m_lastActivityMs(0)
        , m_uploadFile()
        , m_imageBuffer(nullptr)
        , m_imageBufferSize(0)
        , m_imageReceivedSize(0)
        , m_receivingImage(false)
    {
        log_i("[WSServer] Created on port %d", m_port);
    }

    WSServer::~WSServer()
    {
        stop();

        // Cleanup WebSocket server
        if (m_webSocket) {
            delete m_webSocket;
            m_webSocket = nullptr;
        }

        // Smart pointer automatically frees m_imageBuffer
    }

    bool WSServer::begin()
    {
        if (m_running) {
            log_w("[WSServer] Already running");
            return true;
        }

        if (!m_callback) {
            log_e("[WSServer] No callback provided");
            return false;
        }

        // Create FreeRTOS task
        BaseType_t result = xTaskCreatePinnedToCore(taskFunction, // Task function
            "ws_server_task", // Task name
            WS_TASK_STACK_SIZE, // Stack size
            this, // Parameter (this pointer)
            WS_TASK_PRIORITY, // Priority
            &m_taskHandle, // Task handle
            WS_TASK_CORE // Core
        );

        if (result != pdPASS) {
            log_e("[WSServer] Failed to create task");
            WSEvent event;
            event.type = WSEventType::ERROR;
            event.message = "Failed to create WebSocket task";
            sendEvent(event);
            return false;
        }

        m_running = true;
        log_i("[WSServer] Started on port %d", m_port);

        return true;
    }

    void WSServer::stop()
    {
        if (!m_running) {
            return;
        }

        m_running = false;

        // Wait for task to finish
        if (m_taskHandle) {
            vTaskDelete(m_taskHandle);
            m_taskHandle = nullptr;
        }

        log_i("[WSServer] Stopped");
    }

    void WSServer::taskFunction(void* parameter)
    {
        WSServer* server = static_cast<WSServer*>(parameter);
        server->serverLoop();
    }

    void WSServer::serverLoop()
    {
        log_i("[WSServer] Task started");

        // Create WebSocket server instance
        m_webSocket = new WebSocketsServer(m_port);

        if (!m_webSocket) {
            log_e("[WSServer] Failed to create WebSocket instance");
            WSEvent event;
            event.type = WSEventType::ERROR;
            event.message = "Failed to allocate WebSocket server";
            sendEvent(event);
            return;
        }

        log_i("[WSServer] WebSocket instance created, setting up event handlers...");

        // Setup WebSocket event handlers
        // Note: We'll use a static wrapper to handle events from the library
        m_webSocket->onEvent([this](uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
            this->handleWebSocketEvent(num, type, payload, length);
        });

        log_i("[WSServer] Event handlers configured, starting server on port %d...", m_port);

        // Start the WebSocket server
        m_webSocket->begin();
        log_i("[WSServer] ✓ WebSocket server SUCCESSFULLY started on port %d", m_port);
        log_i("[WSServer] ✓ Server is now listening for incoming connections");
        log_i("[WSServer] ✓ Ready to accept WebSocket handshakes");

        // Main loop - process WebSocket events
        while (m_running) {
            m_webSocket->loop();
            checkUploadTimeout();
            checkHeartbeat();
            vTaskDelay(pdMS_TO_TICKS(10)); // Yield CPU every 10ms
        }

        // Cleanup
        m_webSocket->close();
        delete m_webSocket;
        m_webSocket = nullptr;

        log_i("[WSServer] Task ended");
    }

    void WSServer::handleMessage(const uint8_t* data, size_t len)
    {
        // TODO: Implement WebSocket protocol
        // Expected protocol:
        // 1. Client sends image size
        // 2. Client sends image data in chunks
        // 3. Server validates and calls callback with complete image

        log_d("[WSServer] Received message: %d bytes", len);

        if (!m_receivingImage) {
            // Start receiving new image
            // First message should contain image size
            if (len == sizeof(size_t)) {
                m_imageBufferSize = *reinterpret_cast<const size_t*>(data);
                log_i("[WSServer] Starting image reception: %d bytes", m_imageBufferSize);

                // Allocate buffer using smart pointer
                m_imageBuffer = photo_frame::make_psram_unique(m_imageBufferSize);

                if (!m_imageBuffer) {
                    log_e("[WSServer] Failed to allocate image buffer");
                    WSEvent event;
                    event.type = WSEventType::ERROR;
                    event.message = "Out of memory";
                    sendEvent(event);
                    return;
                }

                m_imageReceivedSize = 0;
                m_receivingImage = true;
            }
        } else {
            // Continue receiving image data
            if (m_imageReceivedSize + len <= m_imageBufferSize) {
                memcpy(m_imageBuffer.get() + m_imageReceivedSize, data, len);
                m_imageReceivedSize += len;

                log_d("[WSServer] Received chunk: %d/%d bytes", m_imageReceivedSize, m_imageBufferSize);

                // Check if complete
                if (m_imageReceivedSize == m_imageBufferSize) {
                    log_i("[WSServer] Image reception complete");

                    // NOTE: This is legacy buffer-based code, not used with file-based
                    // upload Send event with image data (legacy - should save to file
                    // instead)
                    WSEvent event;
                    event.type = WSEventType::IMAGE_RECEIVED;
                    event.filepath = ""; // Legacy: buffer not saved to file
                    event.message = "Image received successfully (legacy buffer mode)";
                    sendEvent(event);

                    // Reset state - smart pointer automatically frees memory
                    m_imageBuffer.reset();
                    m_imageBufferSize = 0;
                    m_imageReceivedSize = 0;
                    m_receivingImage = false;
                }
            } else {
                log_e("[WSServer] Received too much data");
                WSEvent event;
                event.type = WSEventType::ERROR;
                event.message = "Protocol error: data overflow";
                sendEvent(event);

                // Reset state - smart pointer automatically frees memory
                m_imageBuffer.reset();
                m_imageBufferSize = 0;
                m_imageReceivedSize = 0;
                m_receivingImage = false;
            }
        }
    }

    void WSServer::handleWebSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length)
    {
        switch (type) {
        case WStype_DISCONNECTED:
            if (num == m_activeClientId) {
                m_activeClientId = 255;
                m_connectedClientsCount = 0;
                resetHeartbeatState();
                log_i("[WSServer] Client %u disconnected (active clients: %u)", num, m_connectedClientsCount);
                WSEvent event;
                event.type = WSEventType::CLIENT_DISCONNECTED;
                event.message = "Client disconnected";
                event.clientId = num;
                event.clientsCount = m_connectedClientsCount;
                sendEvent(event);
            } else {
                log_i("[WSServer] Client %u disconnected (not active)", num);
            }
            break;

        case WStype_CONNECTED:
            if (m_activeClientId != 255 && m_activeClientId != num) {
                log_w("[WSServer] Rejecting client %u: another client is active (%u)", num, m_activeClientId);
                if (m_webSocket) {
                    WSErrorInfo errorInfo("Another client is already connected", 409);
                    String json = errorInfo.toJson();
                    m_webSocket->sendTXT(num, json);
                    m_webSocket->disconnect(num);
                }
                break;
            }

            m_activeClientId = num;
            m_connectedClientsCount = 1;
            m_lastActivityMs = millis();
            m_lastPongMs = m_lastActivityMs;
            m_lastPingMs = m_lastActivityMs;
            m_waitingPong = false;
            log_i("[WSServer] Client %u connected (active clients: %u)", num, m_connectedClientsCount);
            {
                WSEvent event;
                event.type = WSEventType::CLIENT_CONNECTED;
                event.message = "Client connected";
                event.clientId = num;
                event.clientsCount = m_connectedClientsCount;
                sendEvent(event);
            }
            break;

        case WStype_TEXT: {
            if (m_activeClientId != 255 && num != m_activeClientId) {
                log_w("[WSServer] Ignoring TEXT from non-active client %u", num);
                if (m_webSocket) {
                    m_webSocket->disconnect(num);
                }
                break;
            }
            String message(reinterpret_cast<const char*>(payload), length);
            message.trim();
            log_i("[WSServer] Received TEXT from client %u: %s", num, message.c_str());
            m_lastActivityMs = millis();

            if (message == "GET_CONFIG") {
                String json = BoardInfo::toJson();
                log_d("[WSServer] Sending config JSON (%d bytes): %s", json.length(), json.c_str());
                if (m_webSocket) {
                    bool sent = m_webSocket->sendTXT(num, json);
                    log_d("[WSServer] sendTXT result: %s", sent ? "SUCCESS" : "FAILED");
                }
            } else if (message.startsWith("{")) {
                handleControlMessage(num, message);
            }
            break;
        }

        case WStype_BIN:
            if (m_activeClientId != 255 && num != m_activeClientId) {
                log_w("[WSServer] Ignoring BIN from non-active client %u", num);
                if (m_webSocket) {
                    m_webSocket->disconnect(num);
                }
                break;
            }
            log_d("[WSServer] Received BIN from client %u: %u bytes", num, length);
            m_lastActivityMs = millis();
            if (m_uploadActive && num == m_uploadClientId && m_uploadFile) {
                // Check file size limit
                uint32_t maxSize = PFR1_MAX_IMAGE_SIZE_FOR(DISP_WIDTH, DISP_HEIGHT);
                if (m_uploadBytesReceived + length > maxSize) {
                    log_e("[WSServer] File size limit exceeded: %u + %u > %u", m_uploadBytesReceived, length, maxSize);
                    if (m_webSocket) {
                        WSErrorInfo errorInfo("File size limit exceeded", 413);
                        String json = errorInfo.toJson();
                        m_webSocket->sendTXT(num, json);
                    }
                    resetUploadSession(true, "File size limit exceeded");
                    break;
                }

                size_t written = m_uploadFile.write(payload, length);
                if (written != length) {
                    log_e("[WSServer] Failed to write chunk to file (%u/%u)", written, length);
                    if (m_webSocket) {
                        WSErrorInfo errorInfo("Failed to write chunk to file", 500);
                        String json = errorInfo.toJson();
                        m_webSocket->sendTXT(num, json);
                    }
                    resetUploadSession(true, "Write failed");
                } else {
                    m_uploadBytesReceived += written;
                    m_lastUploadActivityMs = millis();
                    if (m_webSocket) {
                        WSChunkAck ackInfo(m_uploadBytesReceived);
                        String json = ackInfo.toJson();
                        m_webSocket->sendTXT(num, json);
                    }
                }
            } else {
                log_w("[WSServer] BIN received without active upload session");
            }
            break;

        case WStype_PING:
            if (m_activeClientId != 255 && num != m_activeClientId) {
                log_w("[WSServer] Ignoring PING from non-active client %u", num);
                if (m_webSocket) {
                    m_webSocket->disconnect(num);
                }
                break;
            }
            log_d("[WSServer] Received PING from client %u", num);
            m_lastActivityMs = millis();
            break;

        case WStype_PONG:
            if (m_activeClientId != 255 && num != m_activeClientId) {
                log_w("[WSServer] Ignoring PONG from non-active client %u", num);
                if (m_webSocket) {
                    m_webSocket->disconnect(num);
                }
                break;
            }
            log_d("[WSServer] Received PONG from client %u", num);
            m_lastActivityMs = millis();
            m_lastPongMs = m_lastActivityMs;
            m_waitingPong = false;
            break;

        case WStype_ERROR:
            log_e("[WSServer] Error from client %u", num);
            {
                WSEvent event;
                event.type = WSEventType::ERROR;
                event.message = "WebSocket error";
                sendEvent(event);
            }
            break;

        default:
            log_w("[WSServer] Unknown event type: %d", type);
            break;
        }
    }

    void WSServer::sendEvent(const WSEvent& event)
    {
        if (m_callback) {
            m_callback(event);
        }
    }

    void WSServer::handleControlMessage(uint8_t num, const String& message)
    {
        StaticJsonDocument<256> doc;
        DeserializationError error = deserializeJson(doc, message);

        if (error) {
            log_e("[WSServer] JSON parse error: %s", error.c_str());
            if (m_webSocket) {
                WSErrorInfo errorInfo("Invalid JSON", 400);
                String json = errorInfo.toJson();
                m_webSocket->sendTXT(num, json);
            }
            return;
        }

        const char* type = doc["type"];
        if (!type) {
            log_w("[WSServer] Control message missing 'type' field");
            if (m_webSocket) {
                WSErrorInfo errorInfo("Missing 'type' field", 400);
                String json = errorInfo.toJson();
                m_webSocket->sendTXT(num, json);
            }
            return;
        }

        if (String(type) == "handshake") {
            // Client sends handshake, respond with device_info
            log_i("[WSServer] Received handshake from client %u", num);

            if (m_webSocket) {
                String json = BoardInfo::toJson();
                log_i("[WSServer] Sending device_info to client %u: %s", num, json.c_str());
                m_webSocket->sendTXT(num, json);
            }
            return;
        }

        if (String(type) == "init") {
            // Parse upload initiation message
            const char* filename = doc["filename"];
            const char* token = doc["token"];
            uint32_t timestamp = doc["timestamp"] | 0;
            uint8_t orientation = doc["orientation"] | 0;

            if (!filename || !token) {
                log_w("[WSServer] Upload init missing required fields");
                if (m_webSocket) {
                    WSErrorInfo errorInfo("Missing filename or token", 400);
                    String json = errorInfo.toJson();
                    m_webSocket->sendTXT(num, json);
                }
                return;
            }

            // Validate filename extension
            String filenameStr(filename);
            if (!filenameStr.endsWith(".pfr1")) {
                log_w("[WSServer] Invalid filename extension: %s", filename);
                if (m_webSocket) {
                    WSErrorInfo errorInfo("Filename must end with .pfr1", 400);
                    String json = errorInfo.toJson();
                    m_webSocket->sendTXT(num, json);
                }
                return;
            }

            // Validate orientation (0-3)
            if (orientation > 3) {
                log_w("[WSServer] Invalid orientation: %u", orientation);
                if (m_webSocket) {
                    WSErrorInfo errorInfo("Orientation must be 0-3", 400);
                    String json = errorInfo.toJson();
                    m_webSocket->sendTXT(num, json);
                }
                return;
            }

            // Check if image is currently being loaded and displayed
            if (g_isLoadingImage) {
                log_w("[WSServer] Cannot start upload: image is being loaded and "
                      "displayed on screen");
                if (m_webSocket) {
                    WSErrorInfo errorInfo("Display busy", 503);
                    String json = errorInfo.toJson();
                    m_webSocket->sendTXT(num, json);
                }
                return;
            }

            // Check if another upload session is already active
            if (m_uploadActive && m_uploadClientId != num) {
                log_w("[WSServer] Upload already in progress from client %u, rejecting "
                      "request from "
                      "client %u",
                    m_uploadClientId, num);
                if (m_webSocket) {
                    WSErrorInfo errorInfo("Another upload is already in progress", 409);
                    String json = errorInfo.toJson();
                    m_webSocket->sendTXT(num, json);
                }
                return;
            }

            // Reset any previous session for this client
            if (m_uploadActive && m_uploadClientId == num) {
                resetUploadSession(true, "New upload init");
            }

            // Setup new upload session
            m_uploadActive = true;
            m_uploadClientId = num;
            m_uploadFilename = filenameStr;
            m_uploadToken = String(token);
            m_uploadTimestamp = timestamp;
            m_uploadOrientation = orientation;
            m_uploadBytesReceived = 0;
            m_lastUploadActivityMs = millis();

            // Open temp file for writing
            m_uploadFile = LittleFS.open(WS_UPLOAD_TEMP_FILENAME, "w");
            if (!m_uploadFile) {
                log_e("[WSServer] Failed to open temp file: %s", WS_UPLOAD_TEMP_FILENAME);
                resetUploadSession(false, "Failed to open temp file");
                if (m_webSocket) {
                    WSErrorInfo errorInfo("Failed to create upload file", 500);
                    String json = errorInfo.toJson();
                    m_webSocket->sendTXT(num, json);
                }
                return;
            }

            log_i("[WSServer] Upload session started: file=%s, token=%s, client=%u", filename, token, num);

            // Send ready response
            if (m_webSocket) {
                WSReadyInfo readyInfo(num);
                String json = readyInfo.toJson();
                m_webSocket->sendTXT(num, json);
            }
        } else if (String(type) == "end") {
            // Upload completion request
            if (!m_uploadActive || m_uploadClientId != num) {
                log_w("[WSServer] End upload without active session from client %u", num);
                if (m_webSocket) {
                    WSErrorInfo errorInfo("No active upload session", 400);
                    String json = errorInfo.toJson();
                    m_webSocket->sendTXT(num, json);
                }
                return;
            }

            // Close the file
            if (m_uploadFile) {
                m_uploadFile.close();
            }

            log_i("[WSServer] Upload ended, validating file: %u bytes received", m_uploadBytesReceived);

            // Validate PFR1 file structure using streaming validation

            File tmp = LittleFS.open(WS_UPLOAD_TEMP_FILENAME, "r");
            photo_frame_error validationResult = photo_frame::binary_utils::validatePFR1FileStructure(tmp, true);
            tmp.close();

            if (validationResult == photo_frame::error_type::None) {
                log_i("[WSServer] Upload validation successful");

                // Move temp file to current image
                LittleFS.remove(WS_CURRENT_IMAGE_FILENAME);
                if (LittleFS.rename(WS_UPLOAD_TEMP_FILENAME, WS_CURRENT_IMAGE_FILENAME)) {
                    log_i("[WSServer] Image file moved to: %s", WS_CURRENT_IMAGE_FILENAME);

                    // Send final_response with success
                    if (m_webSocket) {
                        WSFinalResponse finalResponse(true, "Upload successful", WS_CURRENT_IMAGE_FILENAME);
                        String json = finalResponse.toJson();
                        m_webSocket->sendTXT(num, json);
                    }

                    // Trigger callback to main with upload metadata
                    WSEvent event;
                    event.type = WSEventType::IMAGE_RECEIVED;
                    event.message = "Image received successfully";
                    event.filepath = WS_CURRENT_IMAGE_FILENAME; // Path to saved file in LittleFS
                    event.filename = m_uploadFilename;
                    event.timestamp = m_uploadTimestamp;
                    event.orientation = m_uploadOrientation;
                    sendEvent(event);

                    resetUploadSession(false, "Upload successful");
                } else {
                    log_e("[WSServer] Failed to move upload file");
                    if (m_webSocket) {
                        WSFinalResponse finalResponse(false, "Failed to save image", "");
                        String json = finalResponse.toJson();
                        m_webSocket->sendTXT(num, json);
                    }
                    resetUploadSession(true, "Move failed");
                }
            } else {
                log_e("[WSServer] Upload validation failed: error=%d", validationResult.code);
                if (m_webSocket) {
                    WSFinalResponse finalResponse(false, "Invalid image file", "");
                    String json = finalResponse.toJson();
                    m_webSocket->sendTXT(num, json);
                }
                resetUploadSession(true, "Validation failed");
            }
        } else if (String(type) == "shutdown") {
            // Shutdown command - put device in deep sleep
            log_i("[WSServer] Received shutdown command from client %u", num);

            // Check if upload is in progress
            if (m_uploadActive) {
                log_w("[WSServer] Cannot shutdown: upload session is active from client %u", m_uploadClientId);
                if (m_webSocket) {
                    WSErrorInfo errorInfo("Cannot shutdown: upload in progress", 409);
                    String json = errorInfo.toJson();
                    m_webSocket->sendTXT(num, json);
                }
                return;
            }

            // Send acknowledgement before shutting down
            if (m_webSocket) {
                WSMessageInfo ackInfo("Device entering deep sleep", "ack");
                String json = ackInfo.toJson();
                m_webSocket->sendTXT(num, json);
            }

            // Trigger shutdown event
            WSEvent event;
            event.type = WSEventType::SHUTDOWN_REQUEST;
            event.message = "Shutdown requested via WebSocket";
            sendEvent(event);

            log_i("[WSServer] Shutdown initiated");
        } else {
            log_w("[WSServer] Unknown control message type: %s", type);
            if (m_webSocket) {
                WSErrorInfo errorInfo("Unknown command", 400);
                String json = errorInfo.toJson();
                m_webSocket->sendTXT(num, json);
            }
        }
    }

    void WSServer::resetUploadSession(bool deleteFile, const char* reason)
    {
        if (m_uploadFile) {
            m_uploadFile.close();
        }

        if (deleteFile) {
            LittleFS.remove(WS_UPLOAD_TEMP_FILENAME);
        }

        log_i("[WSServer] Upload session reset: %s", reason);

        m_uploadActive = false;
        m_uploadClientId = 255;
        m_uploadFilename = "";
        m_uploadToken = "";
        m_uploadTimestamp = 0;
        m_uploadOrientation = 0;
        m_uploadBytesReceived = 0;
        m_lastUploadActivityMs = 0;
    }

    void WSServer::checkUploadTimeout()
    {
        if (!m_uploadActive) {
            return;
        }

        unsigned long now = millis();
        unsigned long elapsed = now - m_lastUploadActivityMs;

        if (elapsed > WS_UPLOAD_TIMEOUT_MS) {
            log_w("[WSServer] Upload timeout after %lu ms (limit: %lu ms)", elapsed, (unsigned long)WS_UPLOAD_TIMEOUT_MS);

            // Send timeout error to client
            if (m_webSocket && m_uploadClientId != 255) {
                WSErrorInfo errorInfo("Upload timeout", 408);
                String json = errorInfo.toJson();
                m_webSocket->sendTXT(m_uploadClientId, json);
            }

            resetUploadSession(true, "Timeout");
        }
    }

    void WSServer::checkHeartbeat()
    {
        if (!m_webSocket || m_activeClientId == 255) {
            return;
        }

        unsigned long now = millis();

        if (m_waitingPong) {
            unsigned long elapsed = now - m_lastPingMs;
            if (elapsed > WS_HEARTBEAT_TIMEOUT_MS) {
                if (m_uploadActive && m_uploadClientId == m_activeClientId) {
                    log_w("[WSServer] PONG timeout from client %u during upload; keeping connection", m_activeClientId);
                    m_waitingPong = false;
                    return;
                }

                log_w("[WSServer] PONG timeout from client %u, disconnecting", m_activeClientId);
                m_webSocket->disconnect(m_activeClientId);
            }
            return;
        }

        if (now - m_lastPingMs >= WS_HEARTBEAT_INTERVAL_MS) {
            m_lastPingMs = now;
            m_waitingPong = true;
            log_d("[WSServer] Sending PING to client %u", m_activeClientId);
            m_webSocket->sendPing(m_activeClientId);
        }
    }

    void WSServer::resetHeartbeatState()
    {
        m_lastPingMs = 0;
        m_lastPongMs = 0;
        m_waitingPong = false;
    }

} // namespace ws
} // namespace photo_frame
