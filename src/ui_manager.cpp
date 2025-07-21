#include "ui_manager.h"
#include "hardware_config.h"
#include <U8g2lib.h>
// #include <vector> // Removed as script list is no longer needed

// 按键消抖延迟 (Still needed for basic UI interaction, even if simplified)
static unsigned long lastButtonPressTime = 0;
const unsigned long DEBOUNCE_DELAY = 200;

// Updated constructor to remove SDManager reference
UIManager::UIManager(HardwareManager& hw, AppWiFiManager& wifi, TaskManager& task)
    : hardware(hw), wifi(wifi), taskManager(task), currentState(UI_STATE_STATUS) {}

void UIManager::begin() {
    hardware.getDisplay().clearBuffer();
    hardware.getDisplay().setFont(u8g2_font_ncenB10_tr);
    hardware.getDisplay().drawStr(0, 20, "AIHi Platform");
    hardware.getDisplay().drawStr(0, 40, "Initializing...");
    hardware.getDisplay().sendBuffer();
    delay(2000);
}

void UIManager::update() {
    // Only one state: UI_STATE_STATUS
    handleStateStatus();
}

// --- 状态处理函数 ---

void UIManager::handleStateStatus() {
    drawStatusScreen();

    // No button interaction for menu navigation in simplified UI
    // The UI will just display information
}

// Removed handleStateMainMenu, handleStateAutoScriptList, handleStateWifiKiller, handleStateTimer, handleStateSettingsMenu

// --- 绘制函数 ---

void UIManager::drawStatusScreen() {
    // 绘制状态界面（显示WiFi状态、IP、时间、模式信息、任务信息）
    hardware.getDisplay().firstPage();
    do {
        hardware.getDisplay().setFont(u8g2_font_ncenB08_tr);
        
        // 获取WiFi信息
        String ipAddress = wifi.getIPAddress();
        String wifiStatus = wifi.getWiFiStatus();

        // 获取当前时间（假设已NTP同步）
        time_t now;
        struct tm timeinfo;
        time(&now);
        localtime_r(&now, &timeinfo);
        char timeStr[9];
        strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);
        String currentTime = String(timeStr);

        // 获取模式信息 (假设 TaskManager 或 LLMManager 可以提供)
        // 需要 TaskManager 提供当前模式 (对话/高级)
        String currentMode = taskManager.getCurrentLLMMode(); // Assuming this method exists
        String taskStatus = taskManager.getCurrentTaskStatus(); // Assuming this method exists

        // OLED显示各项信息
        hardware.getDisplay().drawStr(0, 10, ("Mode: " + currentMode).c_str());
        hardware.getDisplay().drawStr(0, 24, ("Task: " + taskStatus).c_str());
        hardware.getDisplay().drawStr(0, 38, ("WiFi: " + wifiStatus).c_str());
        hardware.getDisplay().drawStr(0, 52, ("IP: " + ipAddress).c_str());
        // hardware.getDisplay().drawStr(0, 60, ("Time: " + currentTime).c_str()); // Space is limited, prioritize other info
    } while (hardware.getDisplay().nextPage());
}

// Removed drawMainMenu, drawAutoScriptList, drawWifiKillerScreen, drawTimerScreen, drawSettingsMenu
