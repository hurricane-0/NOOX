#include "hid_manager.h"

HIDManager::HIDManager() : ready(false) {}

void HIDManager::begin() {
    keyboard.begin();
    mouse.begin();
    USB.begin();
    ready = true;
    lastError = "";
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

// ==================== Advanced HID Operations ====================

bool HIDManager::isReady() {
    return ready;
}

String HIDManager::getLastError() {
    return lastError;
}

// Parse modifier keys from string
uint8_t HIDManager::parseModifier(const String& modifier) {
    String mod = modifier;
    mod.toLowerCase();
    
    if (mod == "ctrl" || mod == "control") return KEY_LEFT_CTRL;
    if (mod == "shift") return KEY_LEFT_SHIFT;
    if (mod == "alt") return KEY_LEFT_ALT;
    if (mod == "win" || mod == "meta" || mod == "gui" || mod == "cmd") return KEY_LEFT_GUI;
    
    return 0;
}

// Parse special key codes
uint8_t HIDManager::parseSpecialKeyCode(const String& keyName) {
    String key = keyName;
    key.toLowerCase();
    
    // Function keys
    if (key == "f1") return KEY_F1;
    if (key == "f2") return KEY_F2;
    if (key == "f3") return KEY_F3;
    if (key == "f4") return KEY_F4;
    if (key == "f5") return KEY_F5;
    if (key == "f6") return KEY_F6;
    if (key == "f7") return KEY_F7;
    if (key == "f8") return KEY_F8;
    if (key == "f9") return KEY_F9;
    if (key == "f10") return KEY_F10;
    if (key == "f11") return KEY_F11;
    if (key == "f12") return KEY_F12;
    
    // Navigation keys
    if (key == "home") return KEY_HOME;
    if (key == "end") return KEY_END;
    if (key == "pageup" || key == "pgup") return KEY_PAGE_UP;
    if (key == "pagedown" || key == "pgdn") return KEY_PAGE_DOWN;
    if (key == "insert" || key == "ins") return KEY_INSERT;
    if (key == "delete" || key == "del") return KEY_DELETE;
    
    // Arrow keys
    if (key == "up" || key == "arrowup") return KEY_UP_ARROW;
    if (key == "down" || key == "arrowdown") return KEY_DOWN_ARROW;
    if (key == "left" || key == "arrowleft") return KEY_LEFT_ARROW;
    if (key == "right" || key == "arrowright") return KEY_RIGHT_ARROW;
    
    // Other special keys
    if (key == "enter" || key == "return") return KEY_RETURN;
    if (key == "tab") return KEY_TAB;
    if (key == "backspace") return KEY_BACKSPACE;
    if (key == "escape" || key == "esc") return KEY_ESC;
    if (key == "space") return ' ';
    
    return 0;
}

// Parse media key codes
uint8_t HIDManager::parseMediaKeyCode(const String& mediaKey) {
    String key = mediaKey;
    key.toLowerCase();
    
    // Note: Media keys might use different codes depending on the HID library implementation
    // These are common HID usage IDs for consumer controls
    if (key == "play" || key == "playpause") return 0xCD;
    if (key == "pause") return 0xB1;
    if (key == "next" || key == "nextrack") return 0xB5;
    if (key == "previous" || key == "prevtrack") return 0xB6;
    if (key == "stop") return 0xB7;
    if (key == "volumeup" || key == "volup") return 0xE9;
    if (key == "volumedown" || key == "voldown") return 0xEA;
    if (key == "mute") return 0xE2;
    
    return 0;
}

// Press key combination like "Ctrl+C", "Alt+Tab", "Ctrl+Shift+Esc"
bool HIDManager::pressKeyCombination(const String& keys) {
    if (!ready) {
        lastError = "HID not ready";
        return false;
    }
    
    // Split the key combination by "+"
    int partCount = 0;
    String parts[5]; // Max 4 modifiers + 1 key
    String remaining = keys;
    
    while (remaining.length() > 0 && partCount < 5) {
        int plusIndex = remaining.indexOf('+');
        if (plusIndex == -1) {
            parts[partCount++] = remaining;
            remaining = "";
        } else {
            parts[partCount++] = remaining.substring(0, plusIndex);
            remaining = remaining.substring(plusIndex + 1);
        }
    }
    
    if (partCount == 0) {
        lastError = "Empty key combination";
        return false;
    }
    
    // Press modifiers (all parts except the last one)
    uint8_t modifiers = 0;
    for (int i = 0; i < partCount - 1; i++) {
        parts[i].trim();
        uint8_t mod = parseModifier(parts[i]);
        if (mod == 0) {
            lastError = "Unknown modifier: " + parts[i];
            return false;
        }
        modifiers |= mod;
        keyboard.press(mod);
    }
    
    // Press the main key (last part)
    String mainKey = parts[partCount - 1];
    mainKey.trim();
    
    uint8_t keyCode = parseSpecialKeyCode(mainKey);
    if (keyCode != 0) {
        keyboard.press(keyCode);
    } else if (mainKey.length() == 1) {
        keyboard.press(mainKey.charAt(0));
    } else {
        keyboard.releaseAll();
        lastError = "Unknown key: " + mainKey;
        return false;
    }
    
    delay(50);
    keyboard.releaseAll();
    
    lastError = "";
    return true;
}

// Execute a macro (series of actions)
bool HIDManager::executeMacro(const JsonArray& actions) {
    if (!ready) {
        lastError = "HID not ready";
        return false;
    }
    
    for (JsonVariant action : actions) {
        if (!action.is<JsonObject>()) {
            lastError = "Invalid action format";
            return false;
        }
        
        JsonObject actionObj = action.as<JsonObject>();
        String actionType = actionObj["action"] | "";
        
        if (actionType == "type") {
            String text = actionObj["value"] | "";
            keyboard.print(text);
            delay(50);
        } else if (actionType == "press") {
            String key = actionObj["key"] | "";
            if (!pressKeyCombination(key)) {
                return false;
            }
        } else if (actionType == "delay") {
            int delayMs = actionObj["ms"] | 100;
            delay(delayMs);
        } else if (actionType == "click") {
            int button = actionObj["button"] | MOUSE_BUTTON_LEFT;
            clickMouse(button);
        } else if (actionType == "move") {
            int x = actionObj["x"] | 0;
            int y = actionObj["y"] | 0;
            moveMouse(x, y);
        } else {
            lastError = "Unknown action type: " + actionType;
            return false;
        }
    }
    
    lastError = "";
    return true;
}

// Press special key
bool HIDManager::pressSpecialKey(const String& keyName) {
    if (!ready) {
        lastError = "HID not ready";
        return false;
    }
    
    uint8_t keyCode = parseSpecialKeyCode(keyName);
    if (keyCode == 0) {
        lastError = "Unknown special key: " + keyName;
        return false;
    }
    
    keyboard.press(keyCode);
    delay(50);
    keyboard.releaseAll();
    
    lastError = "";
    return true;
}

// Press media key
bool HIDManager::pressMediaKey(const String& mediaKey) {
    if (!ready) {
        lastError = "HID not ready";
        return false;
    }
    
    uint8_t keyCode = parseMediaKeyCode(mediaKey);
    if (keyCode == 0) {
        lastError = "Unknown media key: " + mediaKey;
        return false;
    }
    
    // Note: Media keys may require special handling depending on the HID library
    // This is a basic implementation
    keyboard.press(keyCode);
    delay(50);
    keyboard.releaseAll();
    
    lastError = "";
    return true;
}
