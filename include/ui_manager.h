#ifndef UI_MANAGER_H
#define UI_MANAGER_H

#include "hardware_manager.h"
#include "wifi_manager.h" // Include WiFiManager
#include "sd_manager.h"   // Include SDManager
#include "task_manager.h" // Include TaskManager

enum UIState {
    UI_STATE_STATUS,
    UI_STATE_MAIN_MENU,
    UI_STATE_WIFI_KILLER,
    UI_STATE_TIMER,
    UI_STATE_AUTO_SCRIPT_LIST,
    UI_STATE_SETTINGS_MENU
};

class UIManager {
public:
    UIManager(HardwareManager& hw, AppWiFiManager& wifi, SDManager& sd, TaskManager& task); // Add TaskManager reference
    void begin();
    void update();

private:
    HardwareManager& hardware;
    AppWiFiManager& wifi;
    SDManager& sd;
    TaskManager& taskManager; // Reference to TaskManager

    UIState currentState = UI_STATE_STATUS;
    int selectedMenuItem = 0;
    int scriptListScrollOffset = 0;

    // State Handlers
    void handleStateStatus();
    void handleStateMainMenu();
    void handleStateAutoScriptList();
    void handleStateWifiKiller();
    void handleStateTimer();
    void handleStateSettingsMenu();

    // Drawing Functions
    void drawStatusScreen();
    void drawMainMenu();
    void drawAutoScriptList();
    void drawWifiKillerScreen();
    void drawTimerScreen();
    void drawSettingsMenu();
};

#endif // UI_MANAGER_H
