#include "hid_manager.h"

HIDManager::HIDManager() {}

void HIDManager::begin() {
    keyboard.begin();
    mouse.begin();
    USB.begin();
}

void HIDManager::sendKey(char key) {
    keyboard.write(key);
}

void HIDManager::sendString(const String& str) {
    keyboard.print(str);
}

void HIDManager::moveMouse(int x, int y) {
    mouse.move(x, y);
}

void HIDManager::clickMouse(int button) {
    mouse.click(button);
}

void HIDManager::openApplication(const String& appName) {
    // Simulate Windows Key + R to open Run dialog
    keyboard.press(KEY_LEFT_GUI); // Windows Key
    keyboard.press('r');
    delay(100);
    keyboard.releaseAll();
    delay(500); // Give time for the Run dialog to appear

    // Type the application name and press Enter
    keyboard.print(appName);
    keyboard.press(KEY_RETURN);
    delay(100);
    keyboard.releaseAll();
}

void HIDManager::runCommand(const String& command) {
    // Simulate Windows Key + R to open Run dialog
    keyboard.press(KEY_LEFT_GUI); // Windows Key
    keyboard.press('r');
    delay(100);
    keyboard.releaseAll();
    delay(500); // Give time for the Run dialog to appear

    // Type the command and press Enter
    keyboard.print(command);
    keyboard.press(KEY_RETURN);
    delay(100);
    keyboard.releaseAll();
}

void HIDManager::takeScreenshot() {
    // Simulate Print Screen key press.
    // KEY_PRTSC is often defined as 0x46 in USBHIDKeyboard.h for ESP32.
    keyboard.press(0x46); // HID Usage ID for Print Screen
    delay(100);
    keyboard.releaseAll();
}

void HIDManager::simulateKeyPress(uint8_t key, uint8_t modifiers) {
    if (modifiers & KEY_LEFT_CTRL) keyboard.press(KEY_LEFT_CTRL);
    if (modifiers & KEY_LEFT_SHIFT) keyboard.press(KEY_LEFT_SHIFT);
    if (modifiers & KEY_LEFT_ALT) keyboard.press(KEY_LEFT_ALT);
    if (modifiers & KEY_LEFT_GUI) keyboard.press(KEY_LEFT_GUI);

    keyboard.press(key);
    delay(50);
    keyboard.releaseAll();
}
