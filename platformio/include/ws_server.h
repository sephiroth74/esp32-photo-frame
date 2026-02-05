#pragma once

#include <Arduino.h>
#include <FS.h>
#include <functional>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <WebSocketsServer.h>

namespace photo_frame {
namespace ws {

    /**
     * @brief WebSocket event types
     */
    enum class WSEventType {
        ERROR, // Error occurred
        IMAGE_RECEIVED, // Complete image received
        CLIENT_CONNECTED, // Client connected
        CLIENT_DISCONNECTED // Client disconnected
    };

    /**
     * @brief WebSocket event data structure
     */
    struct WSEvent {
        WSEventType type;
        String message; // Error message or status info
        String filepath; // Path to saved image file in LittleFS (for IMAGE_RECEIVED)
        String filename; // Filename of uploaded image
        uint32_t timestamp; // Client timestamp
        uint8_t orientation; // Display orientation (0-3)

        WSEvent()
            : type(WSEventType::ERROR)
            , filepath("")
            , filename("")
            , timestamp(0)
            , orientation(0)
        {
        }
    };

    /**
     * @brief Callback function type for WebSocket events
     */
    using WSEventCallback = std::function<void(const WSEvent&)>;

    /**
     * @brief WebSocket Server running on separate FreeRTOS task
     *
     * Manages WebSocket connections for receiving images from clients.
     * Runs on a dedicated FreeRTOS task to avoid blocking main thread.
     */
    class WSServer {
    public:
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
         * @brief Get server port
         * @return Port number
         */
        uint16_t getPort() const { return m_port; }

    private:
        /**
         * @brief FreeRTOS task function (static wrapper)
         */
        static void taskFunction(void* parameter);

        /**
         * @brief Main server loop (runs in task)
         */
        void serverLoop();

        /**
         * @brief Handle WebSocket message
         * @param data Message data
         * @param len Message length
         */
        void handleMessage(const uint8_t* data, size_t len);

        /**
         * @brief Handle WebSocket events from library
         * @param num Client number
         * @param type Event type
         * @param payload Event payload
         * @param length Payload length
         */
        void handleWebSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length);

        /**
         * @brief Handle JSON control messages for upload
         */
        void handleControlMessage(uint8_t num, const String& message);

        /**
         * @brief Reset upload session state
         */
        void resetUploadSession(bool deleteFile, const char* reason);

        /**
         * @brief Check upload timeout and reset if needed
         */
        void checkUploadTimeout();

        /**
         * @brief Send event to callback
         * @param event Event to send
         */
        void sendEvent(const WSEvent& event);

        uint16_t m_port;
        WSEventCallback m_callback;
        TaskHandle_t m_taskHandle;
        bool m_running;

        // WebSocket server instance
        WebSocketsServer* m_webSocket;

        // Upload session state
        bool m_uploadActive;
        uint8_t m_uploadClientId;
        String m_uploadFilename;
        String m_uploadToken;
        uint32_t m_uploadTimestamp;
        uint8_t m_uploadOrientation;
        size_t m_uploadBytesReceived;
        unsigned long m_lastUploadActivityMs;
        File m_uploadFile;

        // Image reception state (legacy buffer-based)
        uint8_t* m_imageBuffer;
        size_t m_imageBufferSize;
        size_t m_imageReceivedSize;
        bool m_receivingImage;
    };

} // namespace ws
} // namespace photo_frame
