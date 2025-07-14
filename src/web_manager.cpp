#include "web_manager.h"
#include "sd_manager.h"
#include "task_manager.h"
#include "wifi_manager.h" // 引入 WiFiManager 头文件
#include <ArduinoJson.h>
#include <LittleFS.h> // 使用 LittleFS

// 定义 authorizedToolsDoc 的 JsonDocument 大小
// 需要足够大以容纳工具名称数组
// 根据预期工具数量和长度进行调整
const size_t AUTHORIZED_TOOLS_DOC_SIZE = 1024; 

WebManager::WebManager(LLMManager& llm, TaskManager& task, AppWiFiManager& wifi) 
    : llmManager(llm), taskManager(task), wifiManager(wifi), server(80), ws("/ws"),
      currentLLMMode(CHAT_MODE), authorizedToolsDoc() { // 构造函数中不指定大小初始化
    authorizedToolsArray = authorizedToolsDoc.to<JsonArray>(); // 初始化 JsonArray
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
                currentLLMMode = CHAT_MODE;
                Serial.println("LLM Mode set to CHAT_MODE");
            } else if (modeStr == "advanced") {
                currentLLMMode = ADVANCED_MODE;
                Serial.println("LLM Mode set to ADVANCED_MODE");
            }
            // 可选：向客户端发送确认信息
            client->text("{\"type\":\"llm_mode_set\", \"status\":\"success\", \"mode\":\"" + modeStr + "\"}");
        } else if (type == "set_authorized_tools") {
            JsonArray tools = doc["tools"].as<JsonArray>();
            authorizedToolsArray.clear(); // 清除之前的授权工具
            for (JsonVariant v : tools) {
                authorizedToolsArray.add(v.as<String>());
            }
            Serial.print("Authorized tools updated: ");
            serializeJson(authorizedToolsArray, Serial);
            Serial.println();
            client->text("{\"type\":\"authorized_tools_set\", \"status\":\"success\"}");
        } else if (type == "chat_message") {
            String payload = doc["payload"].as<String>();
            Serial.printf("Received chat message: %s\n", payload.c_str());

            String llmResponse;
            if (currentLLMMode == CHAT_MODE) {
                llmResponse = llmManager.generateResponse(payload, CHAT_MODE, authorizedToolsArray);
            } else { // ADVANCED_MODE
                llmResponse = llmManager.generateResponse(payload, ADVANCED_MODE, authorizedToolsArray);
                
                // 在高级模式下尝试解析 LLM 响应中的工具调用
                JsonDocument llmResponseDoc;
                DeserializationError llmError = deserializeJson(llmResponseDoc, llmResponse);

    if (!llmError && llmResponseDoc["mode"].is<String>() && llmResponseDoc["mode"].as<String>() == "advanced" &&
        llmResponseDoc["action"].is<JsonObject>() && llmResponseDoc["action"]["type"].is<String>() &&
                    llmResponseDoc["action"]["type"].as<String>() == "tool_call") {
                    
                    String toolName = llmResponseDoc["action"]["tool_name"].as<String>();
                    JsonObject toolParams = llmResponseDoc["action"]["parameters"].as<JsonObject>();

                    if (isToolAuthorized(toolName)) {
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
                        Serial.printf("LLM requested unauthorized tool: %s\n", toolName.c_str());
                        JsonDocument responseDoc;
                        responseDoc["type"] = "chat_message";
                        responseDoc["sender"] = "bot";
                        responseDoc["text"] = "Error: The requested tool '" + toolName + "' is not authorized.";
                        String responseStr;
                        serializeJson(responseDoc, responseStr);
                        broadcast(responseStr);
                    }
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
        } else if (type == "save_settings") {
            JsonObject payload = doc["payload"];
            String ssid = payload["ssid"].as<String>();
            String pass = payload["pass"].as<String>();
            String apiKey = payload["apiKey"].as<String>();

            Serial.printf("Saving settings: SSID=%s, Pass=****, API Key=%s\n", ssid, apiKey);
            
            // 使用 AppWiFiManager 保存 WiFi 凭据
            bool wifiSaved = wifiManager.addWiFiCredential(ssid, pass);
            if (wifiSaved) {
                Serial.println("WiFi credentials saved successfully.");
            } else {
                Serial.println("Failed to save WiFi credentials.");
            }

            // 使用 LLMManager 保存 LLM API Key
            llmManager.setApiKey(apiKey); // setApiKey 现在负责保存到 SD

            // 确认保存结果
            if (wifiSaved) { // 假设 setApiKey 总是成功
                client->text("{\"type\":\"settings_saved\", \"status\":\"success\"}");
            } else {
                client->text("{\"type\":\"settings_saved\", \"status\":\"failed\", \"message\":\"Failed to save WiFi credentials\"}");
            }
        }
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

void WebManager::handleUsbSerialWebRequests() {
    // 此函数用于处理通过 USB 串口传来的 Web 请求
    // 这是一个占位实现
    // 实际实现需要从 Serial 读取、解析类似 HTTP 的请求，
    // 并通过 Serial 返回响应，可能还要从 LittleFS 提供文件
    // 这是一个复杂话题，需要专门的串口到 HTTP 桥接逻辑
    // 目前仅打印调试信息
    // Serial.println("Handling USB Serial Web Requests...");
}

bool WebManager::isToolAuthorized(const String& toolName) {
    if (authorizedToolsArray.isNull() || authorizedToolsArray.size() == 0) {
        return false; // 如果数组为空或为 null，则没有工具被授权
    }
    for (JsonVariant v : authorizedToolsArray) {
        String authorizedTool = v.as<String>();
        if (authorizedTool == toolName) {
            return true;
        }
        // 处理以 "run_automation_script:" 为前缀的自动化脚本
        if (toolName.startsWith("run_automation_script:") && authorizedTool.startsWith("run_automation_script:")) {
            if (toolName == authorizedTool) { // 脚本名完全匹配
                return true;
            }
        }
    }
    return false;
}
