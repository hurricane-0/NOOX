#include "web_manager.h"
// #include "sd_manager.h" // Removed SDManager include
#include "task_manager.h"
#include "wifi_manager.h" // 引入 WiFiManager 头文件
#include <ArduinoJson.h>
#include <LittleFS.h> // 使用 LittleFS

// Removed AUTHORIZED_TOOLS_DOC_SIZE as authorization is removed

WebManager::WebManager(LLMManager& llm, TaskManager& task, AppWiFiManager& wifi) 
    : llmManager(llm), taskManager(task), wifiManager(wifi), server(80), ws("/ws"),
      currentLLMMode(CHAT_MODE) { // Removed authorizedToolsDoc and authorizedToolsArray initialization
}

void WebManager::begin() {
    if(!LittleFS.begin()){
        Serial.println("An Error has occurred while mounting LittleFS");
        return;
    }
    setupRoutes();
    ws.onEvent(std::bind(&WebManager::onWebSocketEvent, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5, std::placeholders::_6));
    server.addHandler(&ws);
    server.begin();
}

void WebManager::loop() {
    ws.cleanupClients();
}

void WebManager::broadcast(const String& message) {
    ws.textAll(message);
}

void WebManager::setLLMMode(LLMMode mode) {
    currentLLMMode = mode;
    // Update TaskManager's internal mode for UI display
    if (mode == CHAT_MODE) {
        taskManager.setLLMMode("Chat");
    } else { // ADVANCED_MODE
        taskManager.setLLMMode("Advanced");
    }
    Serial.printf("LLM Mode set to %s\n", (mode == CHAT_MODE ? "CHAT_MODE" : "ADVANCED_MODE"));
}

void WebManager::onWebSocketEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len) {
    switch (type) {
        case WS_EVT_CONNECT:
            Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
            break;
        case WS_EVT_DISCONNECT:
            Serial.printf("WebSocket client #%u disconnected\n", client->id());
            break;
        case WS_EVT_DATA:
            // 将 client 对象传递给 handleWebSocketData
            handleWebSocketData(client, arg, data, len);
            break;
        case WS_EVT_PONG:
        case WS_EVT_ERROR:
            break;
    }
}

// 修改后的 handleWebSocketData，接受 AsyncWebSocketClient*
void WebManager::handleWebSocketData(AsyncWebSocketClient * client, void *arg, uint8_t *data, size_t len) {
    AwsFrameInfo *info = (AwsFrameInfo*)arg;
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, (char*)data, len);

        if (error) {
            Serial.print(F("deserializeJson() failed: "));
            Serial.println(error.c_str());
            return;
        }

        String type = doc["type"].as<String>();

        if (type == "set_llm_mode") {
            String modeStr = doc["mode"].as<String>();
            if (modeStr == "chat") {
                setLLMMode(CHAT_MODE);
            } else if (modeStr == "advanced") {
                setLLMMode(ADVANCED_MODE);
            }
            // 可选：向客户端发送确认信息
            client->text("{\"type\":\"llm_mode_set\", \"status\":\"success\", \"mode\":\"" + modeStr + "\"}");
        } else if (type == "chat_message") { // Removed set_authorized_tools handling
            String payload = doc["payload"].as<String>();
            Serial.printf("Received chat message: %s\n", payload.c_str());

            String llmResponse;
            if (currentLLMMode == CHAT_MODE) {
                llmResponse = llmManager.generateResponse(payload, CHAT_MODE, JsonArray()); // Pass empty JsonArray
            } else { // ADVANCED_MODE
                llmResponse = llmManager.generateResponse(payload, ADVANCED_MODE, JsonArray()); // Pass empty JsonArray
                
                // 在高级模式下尝试解析 LLM 响应中的工具调用
                JsonDocument llmResponseDoc;
                DeserializationError llmError = deserializeJson(llmResponseDoc, llmResponse);

    if (!llmError && llmResponseDoc["mode"].is<String>() && llmResponseDoc["mode"].as<String>() == "advanced" &&
        llmResponseDoc["action"].is<JsonObject>() && llmResponseDoc["action"]["type"].is<String>() &&
                    llmResponseDoc["action"]["type"].as<String>() == "tool_call") {
                    
                    String toolName = llmResponseDoc["action"]["tool_name"].as<String>();
                    JsonObject toolParams = llmResponseDoc["action"]["parameters"].as<JsonObject>();

                    // Removed isToolAuthorized check - all tools are authorized in advanced mode
                    Serial.printf("LLM requested tool: %s\n", toolName.c_str());
                    // 通过 TaskManager 执行工具
                    String taskResult = taskManager.executeTool(toolName, toolParams); // 该方法需在 TaskManager 中实现
                    
                    JsonDocument responseDoc;
                    responseDoc["type"] = "tool_execution_result";
                    responseDoc["tool_name"] = toolName;
                    responseDoc["result"] = taskResult;
                    
                    String responseStr;
                    serializeJson(responseDoc, responseStr);
                    broadcast(responseStr);

                } else {
                    // 如果高级模式下 LLM 响应不是有效的工具调用 JSON，则作为聊天消息处理
                    JsonDocument responseDoc;
                    responseDoc["type"] = "chat_message";
                    responseDoc["sender"] = "bot";
                    responseDoc["text"] = llmResponse; // 发送原始 LLM 响应作为文本
                    String responseStr;
                    serializeJson(responseDoc, responseStr);
                    broadcast(responseStr);
                }
            }
        }
        // Removed save_settings handling as SD card is removed and credentials are hardcoded
    }
}

void WebManager::setupRoutes() {
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/index.html", "text/html");
    });

    server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/style.css", "text/css");
    });

    server.on("/script.js", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/script.js", "application/javascript");
    });

    // 提供静态资源（如有）
    server.on("/assets/.*", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, request->url(), "image/svg+xml"); // 如有需要可调整内容类型
    });

    server.onNotFound([](AsyncWebServerRequest *request){
        request->send(404, "text/plain", "Not found");
    });
}