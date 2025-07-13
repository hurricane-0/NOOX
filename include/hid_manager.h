#ifndef HID_MANAGER_H
#define HID_MANAGER_H

#include <Arduino.h>
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

private:
    USBHIDKeyboard keyboard;
    USBHIDMouse mouse;
};

#endif // HID_MANAGER_H
