#include "ws_server.h"
#include <esp_log.h>

static const char* TAG = "WSServer";

// FreeRTOS task configuration
#define WS_TASK_STACK_SIZE 8192
#define WS_TASK_PRIORITY 5
#define WS_TASK_CORE 1  // Run on core 1

WSServer::WSServer(uint16_t port, WSEventCallback callback)
    : m_port(port)
    , m_callback(callback)
    , m_taskHandle(nullptr)
    , m_running(false)
    , m_webSocket(nullptr)
    , m_imageBuffer(nullptr)
    , m_imageBufferSize(0)
    , m_imageReceivedSize(0)
    , m_receivingImage(false)
{
    log_i("[WSServer] Created on port %d", m_port);
}

WSServer::~WSServer() {
    stop();
    
    // Cleanup WebSocket server
    if (m_webSocket) {
        delete m_webSocket;
        m_webSocket = nullptr;
    }
    
    // Free image buffer if allocated
    if (m_imageBuffer) {
        free(m_imageBuffer);
        m_imageBuffer = nullptr;
    }
}

bool WSServer::begin() {
    if (m_running) {
        log_w("[WSServer] Already running");
        return true;
    }
    
    if (!m_callback) {
        log_e("[WSServer] No callback provided");
        WSEvent event;
        event.type = WSEventType::ERROR;
        event.message = "No callback provided";
        return false;
    }
    
    // Create FreeRTOS task
    BaseType_t result = xTaskCreatePinnedToCore(
        taskFunction,           // Task function
        "ws_server_task",       // Task name
        WS_TASK_STACK_SIZE,     // Stack size
        this,                   // Parameter (this pointer)
        WS_TASK_PRIORITY,       // Priority
        &m_taskHandle,          // Task handle
        WS_TASK_CORE            // Core
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

void WSServer::stop() {
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

void WSServer::taskFunction(void* parameter) {
    WSServer* server = static_cast<WSServer*>(parameter);
    server->serverLoop();
}

void WSServer::serverLoop() {
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
    
    // Setup WebSocket event handlers
    // Note: We'll use a static wrapper to handle events from the library
    m_webSocket->onEvent([this](uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
        this->handleWebSocketEvent(num, type, payload, length);
    });
    
    // Start the WebSocket server
    m_webSocket->begin();
    log_i("[WSServer] WebSocket server started on port %d", m_port);
    
    WSEvent event;
    event.type = WSEventType::ERROR;
    event.message = "WebSocket server initialized";
    sendEvent(event);
    
    // Main loop - process WebSocket events
    while (m_running) {
        m_webSocket->loop();
        vTaskDelay(pdMS_TO_TICKS(10));  // Yield CPU every 10ms
    }
    
    // Cleanup
    m_webSocket->close();
    delete m_webSocket;
    m_webSocket = nullptr;
    
    log_i("[WSServer] Task ended");
}

void WSServer::handleMessage(const uint8_t* data, size_t len) {
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
            
            // Allocate buffer
            m_imageBuffer = static_cast<uint8_t*>(malloc(m_imageBufferSize));
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
            memcpy(m_imageBuffer + m_imageReceivedSize, data, len);
            m_imageReceivedSize += len;
            
            log_d("[WSServer] Received chunk: %d/%d bytes", m_imageReceivedSize, m_imageBufferSize);
            
            // Check if complete
            if (m_imageReceivedSize == m_imageBufferSize) {
                log_i("[WSServer] Image reception complete");
                
                // Send event with image data
                WSEvent event;
                event.type = WSEventType::IMAGE_RECEIVED;
                event.imageData = m_imageBuffer;
                event.imageSize = m_imageBufferSize;
                event.message = "Image received successfully";
                sendEvent(event);
                
                // Reset state (but don't free buffer - callback owns it now)
                m_imageBuffer = nullptr;
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
            
            // Reset state
            free(m_imageBuffer);
            m_imageBuffer = nullptr;
            m_imageBufferSize = 0;
            m_imageReceivedSize = 0;
            m_receivingImage = false;
        }
    }
}

void WSServer::handleWebSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
    switch (type) {
        case WStype_DISCONNECTED:
            log_i("[WSServer] Client %u disconnected", num);
            {
                WSEvent event;
                event.type = WSEventType::CLIENT_DISCONNECTED;
                event.message = "Client disconnected";
                sendEvent(event);
            }
            break;
            
        case WStype_CONNECTED:
            log_i("[WSServer] Client %u connected", num);
            {
                WSEvent event;
                event.type = WSEventType::CLIENT_CONNECTED;
                event.message = "Client connected";
                sendEvent(event);
            }
            break;
            
        case WStype_TEXT:
            log_d("[WSServer] Received TEXT from client %u: %s", num, payload);
            // Text messages not used in current protocol
            break;
            
        case WStype_BIN:
            log_d("[WSServer] Received BIN from client %u: %u bytes", num, length);
            handleMessage(payload, length);
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

void WSServer::sendEvent(const WSEvent& event) {
    if (m_callback) {
        m_callback(event);
    }
}
