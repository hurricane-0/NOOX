#include "web_manager.h"
#include "task_manager.h"
#include "wifi_manager.h"
#include <ArduinoJson.h>
#include <AsyncJson.h>
#include <LittleFS.h>

// Constructor
WebManager::WebManager(LLMManager& llm, TaskManager& task, AppWiFiManager& wifi, ConfigManager& config) 
    : llmManager(llm), taskManager(task), wifiManager(wifi), configManager(config), server(80), ws("/ws"),
      currentLLMMode(CHAT_MODE) {
}

// Start web services
void WebManager::begin() {
    if(!LittleFS.begin(true)){
        Serial.println("An Error has occurred while mounting LittleFS");
        return;
    }
    Serial.println("LittleFS mounted successfully.");
    setupRoutes();
    ws.onEvent(std::bind(&WebManager::onWebSocketEvent, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5, std::placeholders::_6));
    server.addHandler(&ws);
    server.begin();
    Serial.println("Web server started on port 80.");
}

// WebSocket cleanup and LLM response handling
void WebManager::loop() {
    ws.cleanupClients();

    LLMResponse response;
    if (xQueueReceive(llmManager.llmResponseQueue, &response, 0) == pdPASS) {
        JsonDocument llmResponseDoc;
        DeserializationError llmError = deserializeJson(llmResponseDoc, response.response);

        if (!llmError && llmResponseDoc["mode"].as<String>() == "advanced" &&
            llmResponseDoc["action"]["type"].as<String>() == "tool_call") {
            
            String toolName = llmResponseDoc["action"]["tool_name"].as<String>();
            JsonObject toolParams = llmResponseDoc["action"]["parameters"].as<JsonObject>();
            String taskResult = taskManager.executeTool(toolName, toolParams);
            
            JsonDocument responseDoc;
            responseDoc["type"] = "tool_execution_result";
            responseDoc["tool_name"] = toolName;
            responseDoc["result"] = taskResult;
            
            String responseStr;
            serializeJson(responseDoc, responseStr);
            broadcast(responseStr);

        } else {
            JsonDocument responseDoc;
            responseDoc["type"] = "chat_message";
            responseDoc["sender"] = "bot";
            responseDoc["text"] = response.response;
            String responseStr;
            serializeJson(responseDoc, responseStr);
            broadcast(responseStr);
        }
    }
}

void WebManager::broadcast(const String& message) {
    ws.textAll(message);
}

void WebManager::setLLMMode(LLMMode mode) {
    currentLLMMode = mode;
    taskManager.setLLMMode((mode == CHAT_MODE) ? "Chat" : "Advanced");
    Serial.printf("LLM Mode set to %s\n", (mode == CHAT_MODE ? "CHAT_MODE" : "ADVANCED_MODE"));
}

void WebManager::onWebSocketEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len) {
    if (type == WS_EVT_CONNECT) {
        Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
    } else if (type == WS_EVT_DISCONNECT) {
        Serial.printf("WebSocket client #%u disconnected\n", client->id());
    } else if (type == WS_EVT_DATA) {
        handleWebSocketData(client, arg, data, len);
    }
}

void WebManager::handleWebSocketData(AsyncWebSocketClient * client, void *arg, uint8_t *data, size_t len) {
    AwsFrameInfo *info = (AwsFrameInfo*)arg;
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
        JsonDocument doc;
        if (deserializeJson(doc, (char*)data, len)) {
            return; // Error
        }

        String type = doc["type"].as<String>();

        if (type == "set_llm_mode") {
            String modeStr = doc["mode"].as<String>();
            setLLMMode((modeStr == "chat") ? CHAT_MODE : ADVANCED_MODE);
            client->text("{\"type\":\"llm_mode_set\", \"status\":\"success\", \"mode\":\"" + modeStr + "\"}");
        } else if (type == "chat_message") {
            String payload = doc["payload"].as<String>();
            LLMRequest request = { payload, currentLLMMode };
            if (xQueueSend(llmManager.llmRequestQueue, &request, 0) != pdPASS) {
                client->text("{\"type\":\"chat_message\", \"sender\":\"bot\", \"text\":\"Error: LLM queue full.\"}");
            } else {
                // Acknowledgment can be sent, but the actual response will come via broadcast
            }
        }
    }
}

void WebManager::setupRoutes() {
    // Serve web interface
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/index.html", "text/html");
    });
    server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/style.css", "text/css");
    });
    server.on("/script.js", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/script.js", "application/javascript");
    });

    // API to get current config
    server.on("/api/config", HTTP_GET, [this](AsyncWebServerRequest *request){
        String jsonString;
        serializeJson(configManager.getConfig(), jsonString);
        request->send(200, "application/json", jsonString);
    });

    // API to update config
    AsyncCallbackJsonWebHandler* handler = new AsyncCallbackJsonWebHandler("/api/config", [this](AsyncWebServerRequest *request, JsonVariant &json) {
        JsonDocument& doc = configManager.getConfig();
        doc.clear();
        doc.set(json);
        
        if (configManager.saveConfig()) {
            request->send(200, "application/json", "{\"status\":\"success\", \"message\":\"Configuration saved.\"}");
            // Re-initialize managers with new config
            llmManager.begin();
            // For WiFi, we might want to connect to the new "last_used" one
            wifiManager.begin();
        } else {
            request->send(500, "application/json", "{\"status\":\"error\", \"message\":\"Failed to save configuration.\"}");
        }
    });
    server.addHandler(handler);

    // API for WiFi actions
    server.on("/api/wifi/connect", HTTP_POST, [this](AsyncWebServerRequest *request){
        if (request->hasParam("ssid", true)) {
            String ssid = request->getParam("ssid", true)->value();
            if (wifiManager.connectToWiFi(ssid)) {
                request->send(200, "application/json", "{\"status\":\"success\", \"message\":\"Connecting to " + ssid + ".\"}");
            } else {
                request->send(400, "application/json", "{\"status\":\"error\", \"message\":\"Failed to connect to " + ssid + ".\"}");
            }
        } else {
            request->send(400, "application/json", "{\"status\":\"error\", \"message\":\"Missing ssid parameter.\"}");
        }
    });

    server.on("/api/wifi/disconnect", HTTP_POST, [this](AsyncWebServerRequest *request){
        wifiManager.disconnect();
        request->send(200, "application/json", "{\"status\":\"success\", \"message\":\"WiFi disconnected.\"}");
    });

    server.on("/api/wifi/delete", HTTP_POST, [this](AsyncWebServerRequest *request){
        if (request->hasParam("ssid", true)) {
            String ssid = request->getParam("ssid", true)->value();
            if (wifiManager.deleteWiFi(ssid)) {
                request->send(200, "application/json", "{\"status\":\"success\", \"message\":\"WiFi " + ssid + " deleted.\"}");
            } else {
                request->send(400, "application/json", "{\"status\":\"error\", \"message\":\"Failed to delete " + ssid + ".\"}");
            }
        } else {
            request->send(400, "application/json", "{\"status\":\"error\", \"message\":\"Missing ssid parameter.\"}");
        }
    });

    server.onNotFound([](AsyncWebServerRequest *request){
        request->send(404, "text/plain", "Not found");
    });
}
