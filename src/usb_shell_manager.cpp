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
    Serial1.begin(115200); // Standard serial for debug output
    Serial1.println("UsbShellManager initialized. Waiting for USB connection...");
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
            Serial1.print("Received from host: ");
            Serial1.println(_inputBuffer);
            processHostMessage(_inputBuffer);
            _inputBuffer = ""; // Clear buffer after processing
        }
    }
}

void UsbShellManager::processHostMessage(const String& message) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, message);

    if (error) {
        Serial1.print(F("deserializeJson() failed: "));
        Serial1.println(error.f_str());
        sendToHost("{\"type\":\"error\",\"content\":\"Invalid JSON\"}");
        return;
    }

    String type = doc["type"].as<String>();
    String requestId = doc["requestId"] | ""; // Extract requestId

    if (type == "userInput") {
        String payload = doc["payload"] | "";
        Serial1.print("User input: ");
        Serial1.println(payload);
        // Forward to LLMManager with requestId
        _llmManager->processUserInput(requestId, payload);
    } else if (type == "linkTest") {
        String payload = doc["payload"] | "";
        Serial1.print("Received linkTest: ");
        Serial1.println(payload);
        // Respond with linkTestResult
        sendLinkTestResultToHost(requestId, true, "pong");
    } else if (type == "connectToWifi") {
        String ssid = doc["payload"]["ssid"] | "";
        String password = doc["payload"]["password"] | "";
        Serial1.print("Received connectToWifi for SSID: ");
        Serial1.println(ssid);
        // Forward to WiFiManager
        bool success = _wifiManager->connectToWiFi(ssid, password); // Corrected case to connectToWiFi
        sendWifiConnectStatusToHost(requestId, success, success ? "Connected" : "Failed to connect");
    } else if (type == "shellCommandResult") { // Changed from "shellResult"
        String command = doc["payload"]["command"] | ""; // Assuming command is part of payload for context
        String shellStdout = doc["payload"]["stdout"] | "";
        String shellStderr = doc["payload"]["stderr"] | "";
        String status = doc["status"] | "error"; // New status field
        int exitCode = doc["exitCode"] | -1; // New exitCode field
        
        Serial1.print("Shell output for '");
        Serial1.print(command);
        Serial1.print("':\nSTDOUT: ");
        Serial1.println(shellStdout);
        Serial1.print("STDERR: ");
        Serial1.println(shellStderr);
        Serial1.print("Status: ");
        Serial1.println(status);
        Serial1.print("Exit Code: ");
        Serial1.println(exitCode);

        // Forward to LLMManager with context and requestId
        _llmManager->processShellOutput(requestId, command, shellStdout, shellStderr, status, exitCode);
    } else {
        Serial1.print("Unknown message type: ");
        Serial1.println(type);
        sendToHost(String("{\"type\":\"error\",\"payload\":\"Unknown message type\",\"requestId\":\"") + requestId + String("\"}"));
    }
}

void UsbShellManager::sendToHost(const String& message) {
    _cdc.println(message);
    Serial1.print("Sent to host: ");
    Serial1.println(message);
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
    Serial1.println("Simulating keyboard to launch agent...");
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
    Serial1.println("Keyboard simulation complete.");
}
