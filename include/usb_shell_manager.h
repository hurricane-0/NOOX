#ifndef USB_SHELL_MANAGER_H
#define USB_SHELL_MANAGER_H

#include <Arduino.h>
#include <USB.h>
#include <USBHID.h>
#include <USBCDC.h>
#include <USBMSC.h>
#include <ArduinoJson.h> // For JSON parsing

// Forward declaration for LLMManager
class LLMManager;
// Forward declaration for WiFiManager
class AppWiFiManager;

class UsbShellManager {
public:
    UsbShellManager(LLMManager* llmManager, AppWiFiManager* wifiManager); // Updated constructor
    void begin();
    void loop();

    void setLLMManager(LLMManager* llmManager); // Setter for LLMManager

    // New methods for sending messages to host
    void sendShellCommandToHost(const String& requestId, const String& command);
    void sendAiResponseToHost(const String& requestId, const String& response);
    void sendLinkTestResultToHost(const String& requestId, bool success, const String& payload);
    void sendWifiConnectStatusToHost(const String& requestId, bool success, const String& message);

    // New method for HID keyboard simulation
    void simulateKeyboardLaunchAgent(const String& wifiStatus);

private:
    LLMManager* _llmManager;
    AppWiFiManager* _wifiManager; // New member for WiFiManager
    USBCDC _cdc; // USB CDC instance
    String _inputBuffer; // Buffer for incoming serial data

    void handleUsbSerialData();
    void processHostMessage(const String& message);
    void sendToHost(const String& message); // This will be a generic helper for JSON messages
};

#endif // USB_SHELL_MANAGER_H
