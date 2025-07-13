#ifndef WEB_MANAGER_H
#define WEB_MANAGER_H

#include <ESPAsyncWebServer.h>
#include "llm_manager.h"
#include "task_manager.h"
#include "wifi_manager.h" // Include WiFiManager for WebManager constructor
#include <ArduinoJson.h> // Include ArduinoJson for JsonArray

class WebManager {
public:
    WebManager(LLMManager& llm, TaskManager& task, AppWiFiManager& wifi); // Add AppWiFiManager reference
    void begin();
    void loop();
    void broadcast(const String& message);
    void handleUsbSerialWebRequests(); // Now public

private:
    LLMManager& llmManager;
    TaskManager& taskManager;
    AppWiFiManager& wifiManager; // Store reference to AppWiFiManager
    AsyncWebServer server;
    AsyncWebSocket ws;
    LLMMode currentLLMMode; // To store the current LLM mode (CHAT_MODE or ADVANCED_MODE)
    JsonDocument authorizedToolsDoc; // To store authorized tools as a JsonDocument
    JsonArray authorizedToolsArray; // To store authorized tools as a JsonArray

    void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
    void handleWebSocketData(AsyncWebSocketClient *client, void *arg, uint8_t *data, size_t len); // Modified signature
    void setupRoutes();
    bool isToolAuthorized(const String& toolName); // Helper to check if a tool is authorized
};

#endif // WEB_MANAGER_H
