#include "web_manager.h"
// #include "sd_manager.h" // Removed SDManager include
#include "task_manager.h"
#include "wifi_manager.h" // 引入 WiFiManager 头文件
#include <ArduinoJson.h>
#include <LittleFS.h> // 使用 LittleFS

// 构造函数：初始化成员对象和Web服务
WebManager::WebManager(LLMManager& llm, TaskManager& task, AppWiFiManager& wifi) 
    : llmManager(llm), taskManager(task), wifiManager(wifi), server(80), ws("/ws"),
      currentLLMMode(CHAT_MODE) {
}

// 启动Web服务和WebSocket
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

// WebSocket客户端清理和 LLM 响应处理
void WebManager::loop() {
    ws.cleanupClients();

    // 检查 LLM 响应队列
    LLMResponse response;
    if (xQueueReceive(llmManager.llmResponseQueue, &response, 0) == pdPASS) {
        Serial.printf("WebManager: Received LLM response from queue: %s\n", response.response.c_str());
        // 广播 LLM 响应给所有连接的 WebSocket 客户端
        // 这里需要判断是工具调用结果还是普通聊天消息
        JsonDocument llmResponseDoc;
        DeserializationError llmError = deserializeJson(llmResponseDoc, response.response);

        if (!llmError && llmResponseDoc["mode"].is<String>() && llmResponseDoc["mode"].as<String>() == "advanced" &&
            llmResponseDoc["action"].is<JsonObject>() && llmResponseDoc["action"]["type"].is<String>() &&
            llmResponseDoc["action"]["type"].as<String>() == "tool_call") {
            
            String toolName = llmResponseDoc["action"]["tool_name"].as<String>();
            JsonObject toolParams = llmResponseDoc["action"]["parameters"].as<JsonObject>();

            Serial.printf("LLM requested tool: %s\n", toolName.c_str());
            // 通过TaskManager执行工具
            String taskResult = taskManager.executeTool(toolName, toolParams);
            
            JsonDocument responseDoc;
            responseDoc["type"] = "tool_execution_result";
            responseDoc["tool_name"] = toolName;
            responseDoc["result"] = taskResult;
            
            String responseStr;
            serializeJson(responseDoc, responseStr);
            broadcast(responseStr);

        } else {
            // 非工具调用，作为普通聊天消息处理
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

// 向所有WebSocket客户端广播消息
void WebManager::broadcast(const String& message) {
    ws.textAll(message);
}

// 设置LLM模式，并同步到TaskManager
void WebManager::setLLMMode(LLMMode mode) {
    currentLLMMode = mode;
    if (mode == CHAT_MODE) {
        taskManager.setLLMMode("Chat");
    } else {
        taskManager.setLLMMode("Advanced");
    }
    Serial.printf("LLM Mode set to %s\n", (mode == CHAT_MODE ? "CHAT_MODE" : "ADVANCED_MODE"));
}

// WebSocket事件处理
void WebManager::onWebSocketEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len) {
    switch (type) {
        case WS_EVT_CONNECT:
            Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
            break;
        case WS_EVT_DISCONNECT:
            Serial.printf("WebSocket client #%u disconnected\n", client->id());
            break;
        case WS_EVT_DATA:
            // 数据事件：交由handleWebSocketData处理
            handleWebSocketData(client, arg, data, len);
            break;
        case WS_EVT_PONG:
        case WS_EVT_ERROR:
            break;
    }
}

// WebSocket数据处理：解析消息类型并分发
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
            // 设置LLM模式
            String modeStr = doc["mode"].as<String>();
            if (modeStr == "chat") {
                setLLMMode(CHAT_MODE);
            } else if (modeStr == "advanced") {
                setLLMMode(ADVANCED_MODE);
            }
            client->text("{\"type\":\"llm_mode_set\", \"status\":\"success\", \"mode\":\"" + modeStr + "\"}");
        } else if (type == "chat_message") {
            // 聊天消息处理
            String payload = doc["payload"].as<String>();
            Serial.printf("Received chat message: %s\n", payload.c_str());

            LLMRequest request;
            request.prompt = payload;
            request.mode = currentLLMMode;

            // 将请求发送到 LLM 任务队列
            if (xQueueSend(llmManager.llmRequestQueue, &request, 0) != pdPASS) {
                Serial.println("Failed to send LLM request to queue.");
                client->text("{\"type\":\"chat_message\", \"sender\":\"bot\", \"text\":\"Error: Failed to process your request. LLM queue full.\"}");
            } else {
                client->text("{\"type\":\"chat_message\", \"sender\":\"bot\", \"text\":\"Processing your request...\"}");
            }
        }
        // Removed save_settings handling as SD card is removed and credentials are hardcoded
    }
}

// 处理设置 LLM 提供商的 Web 请求
void WebManager::handleSetLLMProvider(AsyncWebServerRequest *request) {
    if (request->hasParam("provider", true)) {
        String providerStr = request->getParam("provider", true)->value();
        LLMProvider provider;
        if (providerStr == "gemini") {
            provider = GEMINI;
        } else if (providerStr == "deepseek") {
            provider = DEEPSEEK;
        } else if (providerStr == "chatgpt") {
            provider = CHATGPT;
        } else {
            request->send(400, "application/json", "{\"status\":\"error\", \"message\":\"Invalid LLM provider specified.\"}");
            return;
        }
        llmManager.setProvider(provider);
        request->send(200, "application/json", "{\"status\":\"success\", \"message\":\"LLM Provider updated to " + providerStr + ".\"}");
    } else {
        request->send(400, "application/json", "{\"status\":\"error\", \"message\":\"Missing provider parameter.\"}");
    }
}

// 处理Gemini API Key设置请求
void WebManager::handleSetGeminiApiKey(AsyncWebServerRequest *request) {
    if (request->hasParam("apiKey", true)) {
        String apiKey = request->getParam("apiKey", true)->value();
        llmManager.setGeminiApiKey(apiKey);
        request->send(200, "application/json", "{\"status\":\"success\", \"message\":\"Gemini API Key updated.\"}");
    } else {
        request->send(400, "application/json", "{\"status\":\"error\", \"message\":\"Missing apiKey parameter.\"}");
    }
}

// 处理DeepSeek API Key设置请求
void WebManager::handleSetDeepSeekApiKey(AsyncWebServerRequest *request) {
    if (request->hasParam("apiKey", true)) {
        String apiKey = request->getParam("apiKey", true)->value();
        llmManager.setDeepSeekApiKey(apiKey);
        request->send(200, "application/json", "{\"status\":\"success\", \"message\":\"DeepSeek API Key updated.\"}");
    } else {
        request->send(400, "application/json", "{\"status\":\"error\", \"message\":\"Missing apiKey parameter.\"}");
    }
}

// 处理ChatGPT API Key设置请求
void WebManager::handleSetChatGPTApiKey(AsyncWebServerRequest *request) {
    if (request->hasParam("apiKey", true)) {
        String apiKey = request->getParam("apiKey", true)->value();
        llmManager.setChatGPTApiKey(apiKey);
        request->send(200, "application/json", "{\"status\":\"success\", \"message\":\"ChatGPT API Key updated.\"}");
    } else {
        request->send(400, "application/json", "{\"status\":\"error\", \"message\":\"Missing apiKey parameter.\"}");
    }
}

// 路由设置：静态文件和API接口
void WebManager::setupRoutes() {
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        if(LittleFS.exists("/index.html")) {
            Serial.println("Serving /index.html");
            request->send(LittleFS, "/index.html", "text/html");
        } else {
            Serial.println("/index.html not found on LittleFS!");
            request->send(404, "text/plain", "File not found");
        }
    });

    server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){
        if(LittleFS.exists("/style.css")) {
            Serial.println("Serving /style.css");
            request->send(LittleFS, "/style.css", "text/css");
        } else {
            Serial.println("/style.css not found on LittleFS!");
            request->send(404, "text/plain", "File not found");
        }
    });

    server.on("/script.js", HTTP_GET, [](AsyncWebServerRequest *request){
        if(LittleFS.exists("/script.js")) {
            Serial.println("Serving /script.js");
            request->send(LittleFS, "/script.js", "application/javascript");
        } else {
            Serial.println("/script.js not found on LittleFS!");
            request->send(404, "text/plain", "File not found");
        }
    });

    // API Key设置接口
    server.on("/setGeminiApiKey", HTTP_POST, std::bind(&WebManager::handleSetGeminiApiKey, this, std::placeholders::_1));
    server.on("/setDeepSeekApiKey", HTTP_POST, std::bind(&WebManager::handleSetDeepSeekApiKey, this, std::placeholders::_1));
    server.on("/setChatGPTApiKey", HTTP_POST, std::bind(&WebManager::handleSetChatGPTApiKey, this, std::placeholders::_1));
    server.on("/setLLMProvider", HTTP_POST, std::bind(&WebManager::handleSetLLMProvider, this, std::placeholders::_1));

    // 静态资源接口
    server.on("/assets/.*", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, request->url(), "image/svg+xml");
    });

    server.onNotFound([](AsyncWebServerRequest *request){
        request->send(404, "text/plain", "Not found");
    });
}
