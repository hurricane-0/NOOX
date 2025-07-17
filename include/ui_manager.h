#ifndef UI_MANAGER_H
#define UI_MANAGER_H

#include "hardware_manager.h"
#include "wifi_manager.h" // Include WiFiManager
// #include "sd_manager.h"   // Removed SDManager include
#include "task_manager.h" // Include TaskManager

// Only one UI state for simplified display
enum UIState {
    UI_STATE_STATUS // Only status display
};

class UIManager {
public:
    UIManager(HardwareManager& hw, AppWiFiManager& wifi, TaskManager& task); // Removed SDManager reference
    void begin();
    void update();

private:
    HardwareManager& hardware;
    AppWiFiManager& wifi;
    // SDManager& sd; // Removed SDManager reference
    TaskManager& taskManager; // Reference to TaskManager

    UIState currentState = UI_STATE_STATUS; // Always status state

    // State Handlers - only status handler remains
    void handleStateStatus();

    // Drawing Functions - only status screen remains
    void drawStatusScreen();
};

#endif // UI_MANAGER_H
