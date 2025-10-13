#include "web_manager.h"
#include "wifi_manager.h"
#include "hardware_manager.h"
#include <ArduinoJson.h>
#include <AsyncJson.h>
#include <LittleFS.h>

// Constructor
WebManager::WebManager(LLMManager& llm, AppWiFiManager& wifi, ConfigManager& config, HardwareManager& hardware) 
    : llmManager(llm), wifiManager(wifi), configManager(config), hardwareManager(hardware), server(80), ws("/ws"),
      currentLLMMode(CHAT_MODE) {
}

// Start web services
void WebManager::begin() {
    // Note: LittleFS is already mounted in main.cpp before WebManager initialization
    Serial.println("[WEB] Initializing web server...");
    
    // Check if web files exist in LittleFS
    const char* requiredFiles[] = {"/index.html.gz", "/style.css.gz", "/script.js.gz"};
    bool allFilesExist = true;
    
    for (const char* file : requiredFiles) {
        if (!LittleFS.exists(file)) {
            allFilesExist = false;
            Serial.printf("[WEB] WARNING: %s not found in LittleFS\n", file);
        }
    }
    
    if (!allFilesExist) {
        Serial.println("[WEB] ========================================");
        Serial.println("[WEB] ERROR: Web files missing!");
        Serial.println("[WEB] Please run deployment script:");
        Serial.println("[WEB]   python deploy_all.py");
        Serial.println("[WEB] ========================================");
    } else {
        Serial.println("[WEB] All web files present in LittleFS");
    }
    
    setupRoutes();
    ws.onEvent(std::bind(&WebManager::onWebSocketEvent, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5, std::placeholders::_6));
    server.addHandler(&ws);
    server.begin();
    Serial.println("[WEB] Web server started on port 80");
}

// WebSocket cleanup and LLM response handling
void WebManager::loop() {
    ws.cleanupClients();

    // Handle pending configuration updates
    if (configUpdatePending) {
        Serial.println("Processing pending configuration update...");
        JsonDocument& doc = configManager.getConfig();
        doc.clear();
        doc.set(pendingConfigDoc); // Apply the pending config

        if (configManager.saveConfig()) {
            Serial.println("Configuration saved successfully.");
            broadcast("{\"type\":\"config_update_status\", \"status\":\"success\", \"message\":\"Configuration saved and applied.\"}");
            llmManager.begin(); // Re-initialize managers with new config
            wifiManager.begin(); // For WiFi, we might want to connect to the new "last_used" one
        } else {
            Serial.println("Failed to save configuration.");
            broadcast("{\"type\":\"config_update_status\", \"status\":\"error\", \"message\":\"Failed to save configuration.\"}");
        }
        configUpdatePending = false; // Reset flag
    }

    LLMResponse response;
    if (xQueueReceive(llmManager.llmResponseQueue, &response, 0) == pdPASS) {
        JsonDocument responseDoc;
        String responseStr;

        if (response.isToolCall) {
            responseDoc["type"] = "tool_call";
            responseDoc["tool_name"] = response.toolName;
            // toolArgs is already a JSON string, so parse it back to JsonObject
            if (response.toolArgs) {
                JsonDocument argsDoc;
                DeserializationError argsError = deserializeJson(argsDoc, response.toolArgs);
                if (!argsError) {
                    responseDoc["tool_args"] = argsDoc.as<JsonObject>();
                } else {
                    Serial.printf("WebManager: Failed to parse toolArgs JSON: %s\n", argsError.c_str());
                    responseDoc["tool_args"] = response.toolArgs; // Send as raw string if parsing fails
                }
            }
            serializeJson(responseDoc, responseStr);
            broadcast(responseStr);
        } else {
            responseDoc["type"] = "chat_message";
            responseDoc["sender"] = "bot";
            if (response.naturalLanguageResponse) {
                responseDoc["text"] = response.naturalLanguageResponse;
            } else {
                responseDoc["text"] = "";
            }
            serializeJson(responseDoc, responseStr);
            broadcast(responseStr);
        }
        
        // 释放响应中分配的内存（接收方负责释放）
        if (response.toolArgs) {
            free(response.toolArgs);
            response.toolArgs = nullptr;
        }
        if (response.naturalLanguageResponse) {
            free(response.naturalLanguageResponse);
            response.naturalLanguageResponse = nullptr;
        }
    }
}

void WebManager::broadcast(const String& message) {
    ws.textAll(message);
}

// 创建并发送LLM请求的辅助方法
bool WebManager::createAndSendLLMRequest(const String& requestId, const String& payload, LLMMode mode) {
    LLMRequest request;
    memset(&request, 0, sizeof(LLMRequest));
    
    // 安全拷贝requestId（Web请求通常为空）
    strncpy(request.requestId, requestId.c_str(), sizeof(request.requestId) - 1);
    request.requestId[sizeof(request.requestId) - 1] = '\0';
    
    // 使用PSRAM分配prompt内存
    size_t promptLen = payload.length() + 1;
    request.prompt = (char*)ps_malloc(promptLen);
    if (!request.prompt) {
        return false; // 内存分配失败
    }
    strncpy(request.prompt, payload.c_str(), promptLen);
    request.prompt[promptLen - 1] = '\0';
    
    request.mode = mode;
    
    // 发送到队列
    if (xQueueSend(llmManager.llmRequestQueue, &request, 0) != pdPASS) {
        free(request.prompt); // 发送失败，释放内存
        return false;
    }
    
    return true;
}

void WebManager::setLLMMode(LLMMode mode) {
    currentLLMMode = mode;
    llmManager.setCurrentMode(mode);
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
            
            // 使用辅助函数创建并发送LLM请求
            if (!createAndSendLLMRequest("", payload, currentLLMMode)) {
                client->text("{\"type\":\"chat_message\", \"sender\":\"bot\", \"text\":\"Error: Failed to process request.\"}");
            }
            // 实际响应将通过broadcast发送
        } else if (type == "clear_history") {
            // 清除对话历史
            llmManager.clearConversationHistory();
            client->text("{\"type\":\"history_cleared\", \"status\":\"success\", \"message\":\"对话历史已清除\"}");
        } else if (type == "gpio_control") {
            // GPIO控制
            String gpioNum = doc["gpio"].as<String>();
            bool state = doc["state"].as<bool>();
            
            // 根据GPIO编号调用相应的硬件管理器方法
            bool success = false;
            if (gpioNum == "1") {
                hardwareManager.setGpio1State(state);
                success = true;
            } else if (gpioNum == "2") {
                hardwareManager.setGpio2State(state);
                success = true;
            }
            
            if (success) {
                String response = "{\"type\":\"gpio_status\", \"status\":\"success\", \"gpio\":\"" + gpioNum + "\", \"state\":" + (state ? "true" : "false") + "}";
                client->text(response);
                Serial.printf("GPIO %s set to %s\n", gpioNum.c_str(), state ? "HIGH" : "LOW");
            } else {
                String response = "{\"type\":\"gpio_status\", \"status\":\"error\", \"message\":\"Invalid GPIO number\"}";
                client->text(response);
            }
        }
    }
}

void WebManager::setupRoutes() {
    // Serve web interface with Gzip and Caching
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/index.html.gz", "text/html");
        response->addHeader("Content-Encoding", "gzip");
        response->addHeader("Cache-Control", "max-age=86400"); // Cache for 1 day
        request->send(response);
    });
    server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){
        AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/style.css.gz", "text/css");
        response->addHeader("Content-Encoding", "gzip");
        response->addHeader("Cache-Control", "max-age=86400"); // Cache for 1 day
        request->send(response);
    });
    server.on("/script.js", HTTP_GET, [](AsyncWebServerRequest *request){
        AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/script.js.gz", "application/javascript");
        response->addHeader("Content-Encoding", "gzip");
        response->addHeader("Cache-Control", "max-age=86400"); // Cache for 1 day
        request->send(response);
    });

    // API to get current config
    server.on("/api/config", HTTP_GET, [this](AsyncWebServerRequest *request){
        String jsonString;
        serializeJson(configManager.getConfig(), jsonString);
        request->send(200, "application/json", jsonString);
    });

    // API to update config
    AsyncCallbackJsonWebHandler* handler = new AsyncCallbackJsonWebHandler("/api/config", [this](AsyncWebServerRequest *request, JsonVariant &json) {
        // Copy the received JSON to pendingConfigDoc
        pendingConfigDoc.clear();
        pendingConfigDoc.set(json);
        configUpdatePending = true;
        request->send(200, "application/json", "{\"status\":\"success\", \"message\":\"Configuration update initiated.\"}");
    });
    server.addHandler(handler);

    // API for WiFi actions
    server.on("/api/wifi/connect", HTTP_POST, [this](AsyncWebServerRequest *request){
        if (request->hasParam("ssid", true) && request->hasParam("password", true)) {
            String ssid = request->getParam("ssid", true)->value();
            String password = request->getParam("password", true)->value();
            if (wifiManager.connectToWiFi(ssid, password)) { // Pass both ssid and password
                request->send(200, "application/json", "{\"status\":\"success\", \"message\":\"Connecting to " + ssid + ".\"}");
            } else {
                request->send(400, "application/json", "{\"status\":\"error\", \"message\":\"Failed to connect to " + ssid + ".\"}");
            }
        } else {
            request->send(400, "application/json", "{\"status\":\"error\", \"message\":\"Missing ssid or password parameter.\"}");
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
