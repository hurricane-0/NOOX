#ifndef HID_MANAGER_H
#define HID_MANAGER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include "USB.h"
#include "USBHIDKeyboard.h"
#include "USBHIDMouse.h"

class HIDManager {
public:
    HIDManager();
    void begin();
    void sendKey(char key);
    void sendString(const String& str);
    void moveMouse(int x, int y);
    void clickMouse(int button);

    // New functions for system control
    void openApplication(const String& appName);
    void runCommand(const String& command);
    void takeScreenshot();
    void simulateKeyPress(uint8_t key, uint8_t modifiers = 0); // For more complex key presses

    // Advanced HID operations for LLM integration
    bool pressKeyCombination(const String& keys); // Parse and execute key combinations like "Ctrl+C"
    bool executeMacro(const JsonArray& actions); // Execute a series of actions
    bool pressSpecialKey(const String& keyName); // Press special keys like F1-F12, Home, End, etc.
    bool pressMediaKey(const String& mediaKey); // Press media control keys

    // State management
    bool isReady(); // Check if HID is ready
    String getLastError(); // Get last error message

private:
    USBHIDKeyboard keyboard;
    USBHIDMouse mouse;
    String lastError;
    bool ready;

    // Helper methods
    uint8_t parseModifier(const String& modifier);
    uint8_t parseSpecialKeyCode(const String& keyName);
    uint8_t parseMediaKeyCode(const String& mediaKey);
};

#endif // HID_MANAGER_H
