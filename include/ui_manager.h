#ifndef UI_MANAGER_H
#define UI_MANAGER_H

#include "hardware_manager.h"
#include "wifi_manager.h"   // Include WiFiManager
#include "task_manager.h"   // Include TaskManager
#include "llm_manager.h"    // Include LLMManager

// Define UI states for different screens
enum UIState {
    UI_STATE_STATUS,            // Displays current status (Mode, Task, WiFi SSID, IP, Model, Memory)
    UI_STATE_MAIN_MENU,         // Main navigation menu
    UI_STATE_WIFI_MENU,         // WiFi management screen (Disconnect, Connect to Saved, Scan for Networks)
    UI_STATE_SAVED_WIFI_LIST    // List of saved WiFi networks for connection
};

class UIManager {
public:
    UIManager(HardwareManager& hw, AppWiFiManager& wifi, TaskManager& task, LLMManager& llm);
    void begin();
    void update();

private:
    HardwareManager& hardware;
    AppWiFiManager& wifi;
    TaskManager& taskManager;
    LLMManager& llmManager; // Reference to LLMManager

    UIState currentState = UI_STATE_STATUS; // Initial state

    // Variables for menu navigation
    int selectedMenuItem = 0;
    int scrollOffset = 0;
    const int MAX_DISPLAY_ITEMS = 4; // Max items to display on screen at once

    // Debounced button event flags
    bool buttonA_event = false;
    bool buttonB_event = false;
    bool buttonC_event = false;

    // State Handlers
    void handleStateStatus();
    void handleStateMainMenu();
    void handleStateWifiMenu();
    void handleStateSavedWifiList();

    // Drawing Functions
    void drawStatusScreen();
    void drawMainMenu();
    void drawWifiMenu();
    void drawSavedWifiList();

    // Helper for button handling
    void handleButtonInput();
};

#endif // UI_MANAGER_H
