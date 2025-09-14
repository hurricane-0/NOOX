#include "usb_shell_manager.h"
#include "llm_manager.h" // Include the actual LLMManager header

// Define the size of the JSON document buffer
const size_t JSON_DOC_SIZE = 1024; 

UsbShellManager::UsbShellManager(LLMManager* llmManager)
    : _llmManager(llmManager) {
    // Constructor
}

void UsbShellManager::begin() {
    // Initialize USB Composite Device
    USB.begin();
    _cdc.begin(); // Initialize CDC
    Serial.begin(115200); // Standard serial for debug output
    Serial.println("UsbShellManager initialized. Waiting for USB connection...");
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

    if (type == "userInput") {
        String payload = doc["payload"] | "";
        Serial.print("User input: ");
        Serial.println(payload);
        // Forward to LLMManager
        String llmResponse = _llmManager->processUserInput(payload);
        sendToHost(llmResponse); // Send LLM's response back to host
    } else if (type == "shellResult") {
        JsonObject payload = doc["payload"].as<JsonObject>();
        String command = payload["command"] | "";
        String shellStdout = payload["stdout"] | "";
        String shellStderr = payload["stderr"] | "";
        
        Serial.print("Shell output for '");
        Serial.print(command);
        Serial.print("':\nSTDOUT: ");
        Serial.println(shellStdout);
        Serial.print("STDERR: ");
        Serial.println(shellStderr);
        // Forward to LLMManager with context
        String llmResponse = _llmManager->processShellOutput(command, shellStdout, shellStderr);
        sendToHost(llmResponse); // Send LLM's response back to host
    } else {
        Serial.print("Unknown message type: ");
        Serial.println(type);
        sendToHost(String("{\"type\":\"error\",\"payload\":\"Unknown message type\"}"));
    }
}

void UsbShellManager::sendToHost(const String& message) {
    _cdc.println(message);
    Serial.print("Sent to host: ");
    Serial.println(message);
}
