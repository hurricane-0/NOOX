#include <ArduinoJson.h> // Explicitly include ArduinoJson
#include "usb_shell_manager.h"
#include "llm_manager.h" // Include the actual LLMManager header
#include "wifi_manager.h" // Include the WiFiManager header
#include "USBHIDKeyboard.h" // For HID keyboard simulation

// Define the size of the JSON document buffer
const size_t JSON_DOC_SIZE = 1024; 

// Create a keyboard instance
USBHIDKeyboard Keyboard;

UsbShellManager::UsbShellManager(LLMManager* llmManager, AppWiFiManager* wifiManager)
    : _llmManager(llmManager), _wifiManager(wifiManager) {
    // Constructor
}

void UsbShellManager::begin() {
    // Initialize USB Composite Device
    USB.begin();
    _cdc.begin(); // Initialize CDC
    Serial.begin(115200); // Standard Serial for debug output
    Serial.println("UsbShellManager initialized. Waiting for USB connection...");
}

void UsbShellManager::setLLMManager(LLMManager* llmManager) {
    _llmManager = llmManager;
}

void UsbShellManager::loop() {
    handleUsbSerialData();
    // Other tasks can be added here
}

void UsbShellManager::handleUsbSerialData() {
    while (_cdc.available()) {
        char c = _cdc.read();
        _inputBuffer += c;

        // Assuming messages are newline-terminated JSON strings
        if (c == '\n') {
            Serial.print("Received from host: ");
            Serial.println(_inputBuffer);
            processHostMessage(_inputBuffer);
            _inputBuffer = ""; // Clear buffer after processing
        }
    }
}

void UsbShellManager::processHostMessage(const String& message) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, message);

    if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        sendToHost("{\"type\":\"error\",\"content\":\"Invalid JSON\"}");
        return;
    }

    String type = doc["type"].as<String>();
    String requestId = doc["requestId"] | ""; // Extract requestId

    if (type == "userInput") {
        String payload = doc["payload"] | "";
        Serial.print("User input: ");
        Serial.println(payload);
        // Forward to LLMManager with requestId
        _llmManager->processUserInput(requestId, payload);
    } else if (type == "linkTest") {
        String payload = doc["payload"] | "";
        Serial.print("Received linkTest: ");
        Serial.println(payload);
        // Respond with linkTestResult
        sendLinkTestResultToHost(requestId, true, "pong");
    } else if (type == "connectToWifi") {
        String ssid = doc["payload"]["ssid"] | "";
        String password = doc["payload"]["password"] | "";
        Serial.print("Received connectToWifi for SSID: ");
        Serial.println(ssid);
        // Forward to WiFiManager
        bool success = _wifiManager->connectToWiFi(ssid, password); // Corrected case to connectToWiFi
        sendWifiConnectStatusToHost(requestId, success, success ? "Connected" : "Failed to connect");
    } else if (type == "shellCommandResult") { // Changed from "shellResult"
        String command = doc["payload"]["command"] | ""; // Assuming command is part of payload for context
        String shellStdout = doc["payload"]["stdout"] | "";
        String shellStderr = doc["payload"]["stderr"] | "";
        String status = doc["status"] | "error"; // New status field
        int exitCode = doc["exitCode"] | -1; // New exitCode field
        
        Serial.print("Shell output for '");
        Serial.print(command);
        Serial.print("':\nSTDOUT: ");
        Serial.println(shellStdout);
        Serial.print("STDERR: ");
        Serial.println(shellStderr);
        Serial.print("Status: ");
        Serial.println(status);
        Serial.print("Exit Code: ");
        Serial.println(exitCode);

        // Forward to LLMManager with context and requestId
        _llmManager->processShellOutput(requestId, command, shellStdout, shellStderr, status, exitCode);
    } else {
        Serial.print("Unknown message type: ");
        Serial.println(type);
        sendToHost(String("{\"type\":\"error\",\"payload\":\"Unknown message type\",\"requestId\":\"") + requestId + String("\"}"));
    }
}

void UsbShellManager::sendToHost(const String& message) {
    _cdc.println(message);
    Serial.print("Sent to host: ");
    Serial.println(message);
}

void UsbShellManager::sendShellCommandToHost(const String& requestId, const String& command) {
    JsonDocument doc;
    doc["requestId"] = requestId;
    doc["type"] = "shellCommand";
    doc["payload"] = command;
    String output;
    serializeJson(doc, output);
    sendToHost(output);
}

void UsbShellManager::sendAiResponseToHost(const String& requestId, const String& response) {
    JsonDocument doc;
    doc["requestId"] = requestId;
    doc["type"] = "aiResponse";
    doc["payload"] = response;
    String output;
    serializeJson(doc, output);
    sendToHost(output);
}

void UsbShellManager::sendLinkTestResultToHost(const String& requestId, bool success, const String& payload) {
    JsonDocument doc;
    doc["requestId"] = requestId;
    doc["type"] = "linkTestResult";
    doc["status"] = success ? "success" : "error";
    doc["payload"] = payload;
    String output;
    serializeJson(doc, output);
    sendToHost(output);
}

void UsbShellManager::sendWifiConnectStatusToHost(const String& requestId, bool success, const String& message) {
    JsonDocument doc;
    doc["requestId"] = requestId;
    doc["type"] = "wifiConnectStatus";
    doc["status"] = success ? "success" : "error";
    doc["payload"] = message;
    String output;
    serializeJson(doc, output);
    sendToHost(output);
}

void UsbShellManager::simulateKeyboardLaunchAgent(const String& wifiStatus) {
    Serial.println("Simulating keyboard to launch agent...");
    Keyboard.begin();
    delay(1000); // Give host time to recognize HID device

    // Example: Open Run dialog (Win+R), type agent.exe --wifi-status=connected, press Enter
    // This is a simplified example and might need adjustment for different OS/user setups.
    // For Windows: Win+R, type, Enter
    Keyboard.press(KEY_LEFT_GUI); // Windows key
    Keyboard.press('r');
    Keyboard.releaseAll();
    delay(500);

    String command = "agent.exe --wifi-status=" + wifiStatus;
    Keyboard.print(command);
    delay(100);
    Keyboard.press(KEY_RETURN);
    Keyboard.releaseAll();
    delay(500);
    Keyboard.end();
    Serial.println("Keyboard simulation complete.");
}
