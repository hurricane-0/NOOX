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

class UsbShellManager {
public:
    UsbShellManager(LLMManager* llmManager);
    void begin();
    void loop();

private:
    LLMManager* _llmManager;
    USBCDC _cdc; // USB CDC instance
    String _inputBuffer; // Buffer for incoming serial data

    void handleUsbSerialData();
    void processHostMessage(const String& message);
    void sendToHost(const String& message);
};

#endif // USB_SHELL_MANAGER_H
