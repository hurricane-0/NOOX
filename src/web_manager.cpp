#include "web_manager.h"
#include "sd_manager.h"
#include "task_manager.h"
#include "wifi_manager.h" // Include WiFiManager header
#include <ArduinoJson.h>
#include <LittleFS.h> // 使用 LittleFS

// Define the size of the JsonDocument for authorizedToolsDoc
// This needs to be large enough to hold the array of tool names.
// Adjust as needed based on the expected number and length of tool names.
const size_t AUTHORIZED_TOOLS_DOC_SIZE = 1024; 

WebManager::WebManager(LLMManager& llm, TaskManager& task, AppWiFiManager& wifi) 
    : llmManager(llm), taskManager(task), wifiManager(wifi), server(80), ws("/ws"),
      currentLLMMode(CHAT_MODE), authorizedToolsDoc() { // Initialize without size in constructor
    authorizedToolsArray = authorizedToolsDoc.to<JsonArray>(); // Initialize JsonArray
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
            // Pass the client object to handleWebSocketData
            handleWebSocketData(client, arg, data, len);
            break;
        case WS_EVT_PONG:
        case WS_EVT_ERROR:
            break;
    }
}

// Modified handleWebSocketData to accept AsyncWebSocketClient*
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
            // Optionally send confirmation back to client
            client->text("{\"type\":\"llm_mode_set\", \"status\":\"success\", \"mode\":\"" + modeStr + "\"}");
        } else if (type == "set_authorized_tools") {
            JsonArray tools = doc["tools"].as<JsonArray>();
            authorizedToolsArray.clear(); // Clear previous authorizations
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
                
                // Attempt to parse LLM response for tool calls in advanced mode
                JsonDocument llmResponseDoc;
                DeserializationError llmError = deserializeJson(llmResponseDoc, llmResponse);

    if (!llmError && llmResponseDoc["mode"].is<String>() && llmResponseDoc["mode"].as<String>() == "advanced" &&
        llmResponseDoc["action"].is<JsonObject>() && llmResponseDoc["action"]["type"].is<String>() &&
                    llmResponseDoc["action"]["type"].as<String>() == "tool_call") {
                    
                    String toolName = llmResponseDoc["action"]["tool_name"].as<String>();
                    JsonObject toolParams = llmResponseDoc["action"]["parameters"].as<JsonObject>();

                    if (isToolAuthorized(toolName)) {
                        Serial.printf("LLM requested tool: %s\n", toolName.c_str());
                        // Execute tool via TaskManager
                        String taskResult = taskManager.executeTool(toolName, toolParams); // This method needs to be implemented in TaskManager
                        
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
                    // If LLM response is not a valid tool call JSON in advanced mode, treat as chat message
                    JsonDocument responseDoc;
                    responseDoc["type"] = "chat_message";
                    responseDoc["sender"] = "bot";
                    responseDoc["text"] = llmResponse; // Send raw LLM response as text
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
            
            // Save WiFi credentials using AppWiFiManager
            bool wifiSaved = wifiManager.addWiFiCredential(ssid, pass);
            if (wifiSaved) {
                Serial.println("WiFi credentials saved successfully.");
            } else {
                Serial.println("Failed to save WiFi credentials.");
            }

            // Save LLM API Key using LLMManager
            llmManager.setApiKey(apiKey); // setApiKey now handles saving to SD

            // Acknowledge saving
            if (wifiSaved) { // Assuming API key saving is always successful with setApiKey
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

    // Serve assets if any
    server.on("/assets/.*", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, request->url(), "image/svg+xml"); // Adjust content type if needed
    });

    server.onNotFound([](AsyncWebServerRequest *request){
        request->send(404, "text/plain", "Not found");
    });
}

void WebManager::handleUsbSerialWebRequests() {
    // This function will handle web requests coming over USB serial.
    // This is a placeholder implementation.
    // Actual implementation would involve reading from Serial, parsing HTTP-like requests,
    // and sending responses back over Serial, potentially serving files from LittleFS.
    // This is a complex topic and requires a dedicated serial-to-HTTP bridge logic.
    // For now, we'll just print a debug message.
    // Serial.println("Handling USB Serial Web Requests...");
}

bool WebManager::isToolAuthorized(const String& toolName) {
    if (authorizedToolsArray.isNull() || authorizedToolsArray.size() == 0) {
        return false; // No tools are authorized if the array is empty or null
    }
    for (JsonVariant v : authorizedToolsArray) {
        String authorizedTool = v.as<String>();
        if (authorizedTool == toolName) {
            return true;
        }
        // Handle automation scripts which are prefixed with "run_automation_script:"
        if (toolName.startsWith("run_automation_script:") && authorizedTool.startsWith("run_automation_script:")) {
            if (toolName == authorizedTool) { // Exact match for script name
                return true;
            }
        }
    }
    return false;
}
