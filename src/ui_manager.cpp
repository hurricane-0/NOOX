#include "ui_manager.h"
#include "hardware_config.h"
#include <U8g2lib.h>
// #include <vector> // Removed as script list is no longer needed

// 按键消抖延迟 (Still needed for basic UI interaction, even if simplified)
static unsigned long lastButtonPressTime = 0;
const unsigned long DEBOUNCE_DELAY = 200;

UIManager::UIManager(HardwareManager& hw, AppWiFiManager& wifi, TaskManager& task, LLMManager& llm)
    : hardware(hw), wifi(wifi), taskManager(task), llmManager(llm), currentState(UI_STATE_STATUS) {}

void UIManager::begin() {
    hardware.getDisplay().clearBuffer();
    hardware.getDisplay().setFont(u8g2_font_ncenB10_tr);
    hardware.getDisplay().drawStr(0, 20, "AIHi Platform");
    hardware.getDisplay().drawStr(0, 40, "Initializing...");
    hardware.getDisplay().sendBuffer();
    delay(2000);
}

void UIManager::update() {
    // Reset button events at the beginning of each update cycle
    buttonA_event = false;
    buttonB_event = false;
    buttonC_event = false;

    handleButtonInput(); // Process raw button inputs and set debounced event flags

    switch (currentState) {
        case UI_STATE_STATUS:
            handleStateStatus();
            break;
        case UI_STATE_MAIN_MENU:
            handleStateMainMenu();
            break;
        case UI_STATE_WIFI_MENU:
            handleStateWifiMenu();
            break;
        case UI_STATE_SAVED_WIFI_LIST:
            handleStateSavedWifiList();
            break;
        default:
            // Should not happen, revert to status
            currentState = UI_STATE_STATUS;
            handleStateStatus();
            break;
    }
}

void UIManager::handleButtonInput() {
    unsigned long currentTime = millis();
    bool rawButtonA_pressed = hardware.getButtonA();
    bool rawButtonB_pressed = hardware.getButtonB();
    bool rawButtonC_pressed = hardware.getButtonC();

    if (currentTime - lastButtonPressTime > DEBOUNCE_DELAY) {
        if (rawButtonA_pressed) {
            buttonA_event = true;
            lastButtonPressTime = currentTime;
        } else if (rawButtonB_pressed) {
            buttonB_event = true;
            lastButtonPressTime = currentTime;
        } else if (rawButtonC_pressed) {
            buttonC_event = true;
            lastButtonPressTime = currentTime;
        }
    }
}

// --- State Handlers ---

void UIManager::handleStateStatus() {
    drawStatusScreen();
    // Button A: Go to Main Menu
    if (buttonA_event) {
        currentState = UI_STATE_MAIN_MENU;
        selectedMenuItem = 0; // Reset selection
        scrollOffset = 0;     // Reset scroll
    }
}

void UIManager::handleStateMainMenu() {
    drawMainMenu();
    // Button B: Up, Button C: Down, Button A: Select
    if (buttonB_event) {
        selectedMenuItem = (selectedMenuItem - 1 + 2) % 2; // 2 menu items: WiFi, System
    } else if (buttonC_event) {
        selectedMenuItem = (selectedMenuItem + 1) % 2;
    } else if (buttonA_event) {
        if (selectedMenuItem == 0) { // WiFi Settings
            currentState = UI_STATE_WIFI_MENU;
            selectedMenuItem = 0;
            scrollOffset = 0;
        } else if (selectedMenuItem == 1) { // System Info (for now, just go back to status)
            currentState = UI_STATE_STATUS;
            selectedMenuItem = 0;
            scrollOffset = 0;
        }
    }
}

void UIManager::handleStateWifiMenu() {
    drawWifiMenu();
    // Button B: Up, Button C: Down, Button A: Select
    if (buttonB_event) {
        selectedMenuItem = (selectedMenuItem - 1 + 3) % 3; // 3 menu items: Disconnect, Saved, Scan
    } else if (buttonC_event) {
        selectedMenuItem = (selectedMenuItem + 1) % 3;
    } else if (buttonA_event) {
        if (selectedMenuItem == 0) { // Disconnect WiFi
            wifi.disconnect();
            currentState = UI_STATE_STATUS; // Go back to status after action
        } else if (selectedMenuItem == 1) { // Connect to Saved WiFi
            currentState = UI_STATE_SAVED_WIFI_LIST;
            selectedMenuItem = 0;
            scrollOffset = 0;
        } else if (selectedMenuItem == 2) { // Scan for Networks (not implemented yet, go back to status)
            // Implement WiFi scan logic here
            currentState = UI_STATE_STATUS;
        }
    }
    // For simplicity, let's use Button B (long press) as back
    // Note: Long press detection needs to be handled in handleButtonInput or a separate mechanism
    // For now, a short press on B will navigate up, so we need a dedicated back button or menu item.
    // Let's add a "Back" option to the WiFi Menu for now.
    // This will be handled in drawWifiMenu and handleStateWifiMenu.
    // For now, no explicit back button logic here, rely on menu navigation.
}

void UIManager::handleStateSavedWifiList() {
    drawSavedWifiList();
    // Button B: Up, Button C: Down, Button A: Select
    JsonArray savedNetworks = wifi.getSavedSSIDs();
    int numNetworks = savedNetworks.size();

    if (buttonB_event) {
        selectedMenuItem = (selectedMenuItem - 1 + numNetworks) % numNetworks;
        if (selectedMenuItem < scrollOffset) {
            scrollOffset = selectedMenuItem;
        }
    } else if (buttonC_event) {
        selectedMenuItem = (selectedMenuItem + 1) % numNetworks;
        if (selectedMenuItem >= scrollOffset + MAX_DISPLAY_ITEMS) {
            scrollOffset = selectedMenuItem - MAX_DISPLAY_ITEMS + 1;
        }
    } else if (buttonA_event) {
        if (numNetworks > 0) {
            JsonObject network = savedNetworks[selectedMenuItem];
            String ssid = network["ssid"].as<String>();
            String password = network["password"].as<String>();
            wifi.connectToWiFi(ssid, password);
            currentState = UI_STATE_STATUS; // Go back to status after attempting connection
        }
    }
    // For now, no explicit back button logic here, rely on menu navigation.
}

// --- Drawing Functions ---

void UIManager::drawStatusScreen() {
    hardware.getDisplay().firstPage();
    do {
        hardware.getDisplay().setFont(u8g2_font_ncenB08_tr);
        
        // Get information
        String currentMode = taskManager.getCurrentLLMMode();
        String currentModel = llmManager.getCurrentModelName();
        String wifiSSID = wifi.getSSID();
        String ipAddress = wifi.getIPAddress();
        String wifiStatus = wifi.getWiFiStatus();
        
        // Memory Usage
        uint32_t freeHeap = ESP.getFreeHeap();
        uint32_t totalHeap = ESP.getHeapSize(); // ESP.getHeapSize() might not be available on all ESP32 versions, use total_heap_size if needed
        uint32_t usedHeap = totalHeap - freeHeap;
        int memoryPercentage = (totalHeap > 0) ? (usedHeap * 100 / totalHeap) : 0;

        // Display information
        hardware.getDisplay().drawStr(0, 10, ("Mode: " + currentMode).c_str());
        hardware.getDisplay().drawStr(0, 22, ("Model: " + currentModel).c_str());
        hardware.getDisplay().drawStr(0, 34, ("SSID: " + wifiSSID).c_str());
        
        // Conditional display for IP or WiFi status
        if (wifiStatus == "Connected") {
            hardware.getDisplay().drawStr(0, 46, ("IP: " + ipAddress).c_str());
        } else {
            hardware.getDisplay().drawStr(0, 46, ("WiFi: " + wifiStatus).c_str());
        }

        // Memory Bar (simple text for now, can be graphical later)
        String memStr = "Mem: " + String(memoryPercentage) + "% (" + String(freeHeap / 1024) + "KB)";
        hardware.getDisplay().drawStr(0, 58, memStr.c_str());

    } while (hardware.getDisplay().nextPage());
}

void UIManager::drawMainMenu() {
    hardware.getDisplay().firstPage();
    do {
        hardware.getDisplay().setFont(u8g2_font_ncenB08_tr);
        hardware.getDisplay().drawStr(0, 10, "Main Menu");

        String menuItems[] = {"WiFi Settings", "System Info"};
        int numMenuItems = sizeof(menuItems) / sizeof(menuItems[0]);

        for (int i = 0; i < numMenuItems; ++i) {
            String item = menuItems[i];
            if (i == selectedMenuItem) {
                item = "> " + item; // Indicate selected item
            }
            hardware.getDisplay().drawStr(0, 25 + (i * 10), item.c_str());
        }
    } while (hardware.getDisplay().nextPage());
}

void UIManager::drawWifiMenu() {
    hardware.getDisplay().firstPage();
    do {
        hardware.getDisplay().setFont(u8g2_font_ncenB08_tr);
        hardware.getDisplay().drawStr(0, 10, "WiFi Menu");

        String menuItems[] = {"Disconnect", "Connect Saved", "Scan Networks"};
        int numMenuItems = sizeof(menuItems) / sizeof(menuItems[0]);

        for (int i = 0; i < numMenuItems; ++i) {
            String item = menuItems[i];
            if (i == selectedMenuItem) {
                item = "> " + item;
            }
            hardware.getDisplay().drawStr(0, 25 + (i * 10), item.c_str());
        }
    } while (hardware.getDisplay().nextPage());
}

void UIManager::drawSavedWifiList() {
    hardware.getDisplay().firstPage();
    do {
        hardware.getDisplay().setFont(u8g2_font_ncenB08_tr);
        hardware.getDisplay().drawStr(0, 10, "Saved WiFi Networks");

        JsonArray savedNetworks = wifi.getSavedSSIDs();
        int numNetworks = savedNetworks.size();

        for (int i = 0; i < MAX_DISPLAY_ITEMS; ++i) {
            int displayIndex = scrollOffset + i;
            if (displayIndex < numNetworks) {
                String ssid = savedNetworks[displayIndex]["ssid"].as<String>();
                if (displayIndex == selectedMenuItem) {
                    ssid = "> " + ssid;
                }
                hardware.getDisplay().drawStr(0, 25 + (i * 10), ssid.c_str());
            }
        }
    } while (hardware.getDisplay().nextPage());
}
