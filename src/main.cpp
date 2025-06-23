#include <Arduino.h>
#include <WiFi.h> // For WiFi connectivity
#include <HTTPClient.h> // For HTTP/HTTPS requests
#include <ArduinoJson.h> // For JSON parsing (will be used for Gemini API)
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WebSocketsServer.h>
#include <SPIFFS.h> // For serving web files

#include "hardware_config.h" // Contains hardware definitions and externs
#include "hardware_init.h"   // Contains initHardware()
#include "ui_manager.h"      // Contains setOperatingMode()
#include "gemini_api.h"      // Contains sendToGemini()
#include "task_scheduler.h"  // Contains TaskAction, executeTaskAction, processTaskQueue
#include "wifi_manager.h"    // For WiFi management functions
#include "bluetooth_manager.h" // For Bluetooth management functions

// Web Server and WebSocket Server instances
AsyncWebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81); // WebSocket port

// WiFi Connection (example)
// Note: In actual deployment, it is recommended to use WiFiManager library to manage WiFi configuration, so that users can easily configure SSID and password.
// For demonstration purposes, it is hardcoded here for now.
const char* ssid = "Your_SSID";     // Replace with your WiFi SSID
const char* password = "Your_PASSWORD"; // Replace with your WiFi password

void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.printf("[%u] Disconnected!\n", num);
      break;
    case WStype_CONNECTED: {
      IPAddress ip = webSocket.remoteIP(num);
      Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
      // Send welcome message
      webSocket.sendTXT(num, "Hello from ESP32-S3!");
    }
      break;
    case WStype_TEXT:
      Serial.printf("[%u] get Text: %s\n", num, payload);
      if (String((char*)payload).startsWith("chat:")) {
        String prompt = String((char*)payload).substring(5);
        Serial.print("Chat command received: ");
        Serial.println(prompt);
        ui_manager_clear_screen();
        ui_manager_print_message("User:\n" + prompt + "\nThinking...");

        String geminiResponse = sendToGemini(prompt);
        Serial.print("Gemini Response: ");
        Serial.println(geminiResponse);

        ui_manager_clear_screen();
        ui_manager_print_message("Gemini Says:\n" + geminiResponse);

        webSocket.sendTXT(num, "Gemini:" + geminiResponse); // Send Gemini response back to mobile web page
      } else if (String((char*)payload).startsWith("advanced:")) {
        if (currentMode != ADVANCED_MODE) {
          webSocket.sendTXT(num, "System:Please switch to Advanced Mode first.");
          Serial.println("Attempted advanced command in chat mode.");
          return;
        }
        String jsonPayload = String((char*)payload).substring(9); // Remove "advanced:" prefix
        Serial.print("Advanced command received: ");
        Serial.println(jsonPayload);
        
        // Parse the JSON task from Gemini
        StaticJsonDocument<2048> doc; // Increased size for potentially larger tasks
        DeserializationError error = deserializeJson(doc, jsonPayload);

        if (error) {
          Serial.print(F("deserializeJson() failed for advanced command: "));
          Serial.println(error.f_str());
          webSocket.sendTXT(num, "System:Error parsing advanced command JSON.");
          return;
        }

        if (doc.containsKey("mode") && String(doc["mode"].as<const char*>()).equals("advanced") && doc.containsKey("actions")) {
          taskQueue.clear(); // Clear any previous tasks
          JsonArray actions = doc["actions"].as<JsonArray>();
          for (JsonObject action : actions) {
            TaskAction ta;
            ta.type = action["type"].as<String>();
            ta.command = action["command"].as<String>();
            if (action.containsKey("value")) ta.value = action["value"].as<String>();
            if (action.containsKey("keys")) {
              JsonArray keysArray = action["keys"].as<JsonArray>();
              for (JsonVariant key : keysArray) {
                ta.keys.push_back(key.as<String>());
              }
            }
            if (action.containsKey("duration_ms")) ta.duration_ms = action["duration_ms"].as<long>();
            if (action.containsKey("risk_level")) ta.risk_level = action["risk_level"].as<String>();
            if (action.containsKey("description")) ta.description = action["description"].as<String>();
            taskQueue.push_back(ta);
          }
          Serial.printf("Parsed %d actions for advanced mode.\n", taskQueue.size());
          webSocket.sendTXT(num, "System:Task received and queued. Starting execution.");
          taskInProgress = true; // Start task execution
          lastActionExecutionTime = millis(); // Reset timer for immediate execution
        } else {
          webSocket.sendTXT(num, "System:Invalid advanced command format.");
          Serial.println("Invalid advanced command format.");
        }
      } else if (String((char*)payload).startsWith("AuthConfirm:")) {
        // Handle authorization confirmation from web page
        bool confirmed = String((char*)payload).substring(12).equals("true");
        if (authorizationPending) {
          authorizationPending = false; // Clear pending authorization
          if (confirmed) {
            Serial.println("Authorization confirmed from web page. Resuming task.");
            webSocket.sendTXT(num, "System:Action authorized. Resuming task.");
            // The task processing loop will automatically resume
          } else {
          Serial.println("Authorization denied from web page. Halting task.");
          webSocket.sendTXT(num, "System:Action denied. Task halted.");
          ui_manager_print_message("Action Denied!");
          taskQueue.clear(); // Clear remaining tasks
          taskInProgress = false;
        }
        } else {
          Serial.println("Received AuthConfirm but no authorization was pending.");
          webSocket.sendTXT(num, "System:No authorization pending.");
        }
      } else if (String((char*)payload).startsWith("SetMode:")) {
        String modeStr = String((char*)payload).substring(8);
        if (modeStr.equals("chat")) {
          setOperatingMode(CHAT_MODE);
          webSocket.sendTXT(num, "System:Switched to Chat Mode.");
        } else if (modeStr.equals("advanced")) {
          setOperatingMode(ADVANCED_MODE);
          webSocket.sendTXT(num, "System:Switched to Advanced Mode.");
        } else {
          webSocket.sendTXT(num, "System:Invalid mode specified.");
        }
      } else if (String((char*)payload).equals("ScanWiFi")) {
        Serial.println("Received ScanWiFi command.");
        wifi_manager_scan_networks();
        // Send results back to web page
        String jsonResponse = "{\"networks\":[";
        for (size_t i = 0; i < availableNetworks.size(); ++i) {
          jsonResponse += "{\"ssid\":\"" + availableNetworks[i] + "\",\"rssi\":" + String(WiFi.RSSI(i)) + "}";
          if (i < availableNetworks.size() - 1) {
            jsonResponse += ",";
          }
        }
        jsonResponse += "]}";
        webSocket.sendTXT(num, "WiFiScanResults:" + jsonResponse);
      } else if (String((char*)payload).startsWith("WiFiConnect:")) {
        String connectData = String((char*)payload).substring(12);
        int commaIndex = connectData.indexOf(',');
        if (commaIndex != -1) {
          String ssidToConnect = connectData.substring(0, commaIndex);
          String passwordToConnect = connectData.substring(commaIndex + 1);
          Serial.printf("Received WiFiConnect command for SSID: %s\n", ssidToConnect.c_str());
          if (wifi_manager_connect_to_network(ssidToConnect, passwordToConnect)) {
            webSocket.sendTXT(num, "System:WiFi connected to " + ssidToConnect);
          } else {
            webSocket.sendTXT(num, "System:WiFi connection failed for " + ssidToConnect);
          }
        } else {
          webSocket.sendTXT(num, "System:Invalid WiFiConnect command format.");
        }
      } else if (String((char*)payload).equals("ScanBluetooth")) {
        Serial.println("Received ScanBluetooth command.");
        bluetooth_scan_devices();
        // Note: Bluetooth scan results are handled asynchronously by MyAdvertisedDeviceCallbacks
        // and stored in myAdvertisedDevice. You'd need a mechanism to send these back
        // For now, this is a placeholder.
        webSocket.sendTXT(num, "System:Bluetooth scan initiated. Results will appear in serial monitor.");
        // A more robust solution would involve collecting results and sending them as JSON
        // Similar to WiFiScanResults.
      } else if (String((char*)payload).startsWith("BluetoothConnect:")) {
        String deviceAddress = String((char*)payload).substring(17);
        Serial.printf("Received BluetoothConnect command for address: %s\n", deviceAddress.c_str());
        bluetooth_start_gatt_client(deviceAddress.c_str());
        // Connection status will be reported via BLEClientCallbacks
      }
      break;
    case WStype_BIN:
      Serial.printf("[%u] get Binary length: %u\n", num, length);
      break;
    case WStype_ERROR:
    case WStype_FRAGMENT_TEXT_START:
    case WStype_FRAGMENT_BIN_START:
    case WStype_FRAGMENT:
    case WStype_FRAGMENT_FIN:
      break;
  }
}


void setup() {
  initHardware(); // Initialize all hardware components
  ui_manager_init(); // Initialize UI manager

  // Initialize SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    ui_manager_set_status("SPIFFS Failed!");
    return;
  } else {
    Serial.println("SPIFFS Mounted successfully");
    ui_manager_set_status("SPIFFS OK!");
  }

  // Configure Web Server
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/index.html", "text/html");
  });

  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/style.css", "text/css");
  });

  server.on("/script.js", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/script.js", "application/javascript");
  });

  // Start Web Server
  server.begin();
  Serial.println("Web Server Started.");

  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  ui_manager_set_status("Connecting WiFi...");

  WiFi.begin(ssid, password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) { // Try to connect 20 times
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    ui_manager_set_status("WiFi Connected!");
    Serial.print("Access web interface at http://");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nWiFi Connection Failed!");
    ui_manager_set_status("WiFi Failed!");
  }

  // Start WebSocket Server
  webSocket.begin();
  webSocket.onEvent(onWebSocketEvent);
  Serial.println("WebSocket Server Started.");
}


// Function to execute a single task action (Moved to task_scheduler.cpp)
/*
void executeTaskAction(const TaskAction& action) {
  Serial.print("Executing action: Type="); Serial.print(action.type);
  Serial.print(", Command="); Serial.println(action.command);

  // Check for authorization if risk level is medium or high
  if ((action.risk_level.equals("medium") || action.risk_level.equals("high")) && !authorizationPending) {
    authorizationPending = true;
    pendingAuthDescription = action.description;
    pendingAuthCommand = action.command; // Store command for re-execution if authorized
    Serial.print("Authorization required for: ");
    Serial.println(action.description);
    tft.fillScreen(ST77XX_BLACK);
    tft.setCursor(0, 0);
    tft.setTextColor(ST77XX_RED);
    tft.println("AUTH REQUIRED!");
    tft.println(action.description);
    webSocket.broadcastTXT("AuthRequest:" + action.description);
    return; // Stop execution until authorized
  }

  if (action.type.equals("keyboard")) {
    if (action.command.equals("type")) {
      Keyboard.print(action.value);
      Serial.print("Typed: "); Serial.println(action.value);
      webSocket.broadcastTXT("System:Typed '" + action.value + "'");
    } else if (action.command.equals("press_release")) {
      for (const String& key : action.keys) {
        if (key.equals("KEY_LEFT_GUI")) Keyboard.press(KEY_LEFT_GUI);
        else if (key.equals("KEY_LEFT_ALT")) Keyboard.press(KEY_LEFT_ALT);
        else if (key.equals("KEY_LEFT_CTRL")) Keyboard.press(KEY_LEFT_CTRL);
        else if (key.equals("KEY_LEFT_SHIFT")) Keyboard.press(KEY_LEFT_SHIFT);
        else if (key.equals("KEY_RETURN")) Keyboard.press(KEY_RETURN);
        else if (key.length() == 1) Keyboard.press(key.charAt(0)); // Single character keys
      }
      Keyboard.releaseAll();
      String keysStr = "";
      for (const String& key : action.keys) {
        keysStr += key + " ";
      }
      Serial.print("Pressed/Released: "); Serial.println(keysStr);
      webSocket.broadcastTXT("System:Pressed/Released keys: " + keysStr);
    }
  } else if (action.type.equals("system")) {
    if (action.command.equals("wait")) {
      Serial.print("Waiting for "); Serial.print(action.duration_ms); Serial.println(" ms.");
      webSocket.broadcastTXT("System:Waiting for " + String(action.duration_ms) + "ms");
      delay(action.duration_ms); // Blocking delay for now, consider non-blocking for future
    }
  }
  // Add more action types (e.g., mouse, wifi, bluetooth) here
}
*/

// Function to process the task queue (Moved to task_scheduler.cpp)
/*
void processTaskQueue() {
  if (taskInProgress && !taskQueue.empty() && !authorizationPending) {
    if (millis() - lastActionExecutionTime > ACTION_DELAY_MS) {
      TaskAction currentAction = taskQueue.front();
      taskQueue.erase(taskQueue.begin()); // Remove the action from the queue

      executeTaskAction(currentAction);
      lastActionExecutionTime = millis(); // Update last execution time
      
      if (taskQueue.empty()) {
        taskInProgress = false;
    Serial.println("Task completed.");
    ui_manager_print_message("Task Complete!");
    webSocket.broadcastTXT("System:Task completed.");
      } else {
        webSocket.broadcastTXT("TaskStatus:Executing action " + String(taskQueue.size()) + " of " + String(taskQueue.size() + 1) + ": " + currentAction.description);
      }
    }
  } else if (taskInProgress && taskQueue.empty() && !authorizationPending) {
    // This case handles if the last action was an authorization request and it was confirmed.
    // The task queue would be empty, but taskInProgress would still be true.
    // We need to ensure the taskInProgress flag is reset.
    taskInProgress = false;
    Serial.println("Task completed (after authorization).");
    tft.fillScreen(ST77XX_BLACK);
    tft.setCursor(0, 0);
    tft.setTextColor(ST77XX_GREEN);
    tft.println("Task Complete!");
    webSocket.broadcastTXT("System:Task completed.");
  }
}
*/

void loop() {
  webSocket.loop(); // Process WebSocket client events

  // Process advanced mode tasks
  processTaskQueue();

  // Keep WiFi connected and send status to web UI
  static unsigned long lastStatusUpdateTime = 0;
  const unsigned long STATUS_UPDATE_INTERVAL_MS = 5000; // Update every 5 seconds

  if (millis() - lastStatusUpdateTime > STATUS_UPDATE_INTERVAL_MS) {
    String wifiStatus = (WiFi.status() == WL_CONNECTED) ? "已连接" : "未连接";
    String ipAddress = (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString() : "N/A";
    String bluetoothStatus = BLEDevice::getInitialized() ? (deviceConnected ? "已连接" : "未连接") : "未初始化"; // Assuming deviceConnected is from bluetooth_manager

    String statusJson = "{\"wifiStatus\":\"" + wifiStatus + "\",\"ipAddress\":\"" + ipAddress + "\",\"bluetoothStatus\":\"" + bluetoothStatus + "\"}";
    webSocket.broadcastTXT("DeviceStatus:" + statusJson);
    lastStatusUpdateTime = millis();
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected. Reconnecting...");
    ui_manager_set_status("WiFi Disconnected!");
    WiFi.reconnect();
  }

  // WS2812 color change example
  static unsigned long lastColorChangeMillis = 0;
  if (millis() - lastColorChangeMillis > 500) { // Change color every 500ms
    static byte hue = 0;
    leds[0] = CHSV(hue++, 255, 255);
    FastLED.show();
    lastColorChangeMillis = millis();
  }

  // Heartbeat and LED blink example
  static unsigned long lastMillis = 0;
  static bool ledState = false;
  if (millis() - lastMillis > 1000) {
    Serial.println("Heartbeat...");
    lastMillis = millis();
    ledState = !ledState;
    digitalWrite(LED1_PIN, ledState); // LED1 blinks
    digitalWrite(LED2_PIN, !ledState); // LED2 blinks alternately
  }

  // Read button states (high-level trigger)
  // For now, only print to serial to avoid rapid screen overwrites
  // More complex UI update logic will be needed for actual application
  if (digitalRead(KEY1_PIN) == HIGH) {
    Serial.println("KEY1 Pressed!");
    ui_manager_print_message("KEY1 Pressed!");
    delay(100); // Simple debounce
    // Example: Simulate keyboard press (for testing)
    if (currentMode == ADVANCED_MODE) {
      // In advanced mode, KEY1 could trigger a predefined task or a specific action
      Serial.println("KEY1 in Advanced Mode: Triggering test task.");
      String testTaskJson = "{\"mode\":\"advanced\",\"task_id\":\"test-task-1\",\"actions\":[{\"type\":\"keyboard\",\"command\":\"press_release\",\"keys\":[\"KEY_LEFT_GUI\",\"r\"],\"risk_level\":\"low\",\"description\":\"Open Run dialog\"},{\"type\":\"system\",\"command\":\"wait\",\"duration_ms\":500},{\"type\":\"keyboard\",\"command\":\"type\",\"value\":\"notepad\",\"risk_level\":\"low\",\"description\":\"Type notepad\"},{\"type\":\"keyboard\",\"command\":\"press_release\",\"keys\":[\"KEY_RETURN\"],\"risk_level\":\"low\",\"description\":\"Press Enter\"}]}";
      // Call onWebSocketEvent directly to simulate receiving the task
      // This is for testing purposes only, in real scenario, it comes from WebSocket
      onWebSocketEvent(0, WStype_TEXT, (uint8_t*)("advanced:" + testTaskJson).c_str(), ("advanced:" + testTaskJson).length());
    } else {
      Serial.println("KEY1 in Chat Mode: No action.");
    }
  }
  if (digitalRead(KEY2_PIN) == HIGH) {
    Serial.println("KEY2 Pressed!");
    ui_manager_print_message("KEY2 Pressed!");
    delay(100);
    // Example: Switch mode with KEY2
    if (currentMode == CHAT_MODE) {
      setOperatingMode(ADVANCED_MODE);
    } else {
      setOperatingMode(CHAT_MODE);
    }
  }
  if (digitalRead(KEY3_PIN) == HIGH) {
    Serial.println("KEY3 Pressed!");
    ui_manager_print_message("KEY3 Pressed!");
    delay(100);
    // Example: Trigger authorization denial for testing
    if (authorizationPending) {
      authorizationPending = false;
      Serial.println("Authorization denied by KEY3.");
      webSocket.broadcastTXT("System:Action denied by device button.");
      ui_manager_print_message("Action Denied by Button!");
      taskQueue.clear(); // Halt task
      taskInProgress = false;
    }
  }

  // Check UART1 received data (example)
  if (Serial1.available()) {
    String data = Serial1.readStringUntil('\n');
    Serial.print("UART1 Received: ");
    Serial.println(data);
  }

  // Check UART2 received data (example)
  if (Serial2.available()) {
    String data = Serial2.readStringUntil('\n');
    Serial.print("UART2 Received: ");
    Serial.println(data);
  }
}
