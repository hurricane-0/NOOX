#ifndef TASK_MANAGER_H
#define TASK_MANAGER_H

#include <Arduino.h>
#include "hid_manager.h" // 引入 HIDManager 头文件
#include "hardware_manager.h" // 引入 HardwareManager 头文件用于 GPIO 任务
#include <ArduinoJson.h> // 引入 ArduinoJson 用于 JsonObject

// 前向声明，避免循环依赖
class AppWiFiManager;
class HardwareManager; // HardwareManager 的前向声明


class TaskManager {
public:
    TaskManager(HIDManager& hid, AppWiFiManager& wifi, HardwareManager& hw); // 调整构造函数，移除 TimerManager, BLEManager 和 SDManager
    String executeTool(const String& toolName, const JsonObject& params); // 用于 LLM 工具调用的新方法

    // 新增用于 UI 显示的方法
    String getCurrentLLMMode(); // 获取当前 LLM 模式 (对话/高级)
    String getCurrentTaskStatus(); // 获取当前任务状态

    // Methods to update internal state (can be called by WebManager/LLMManager)
    void setLLMMode(const String& mode);
    void setTaskStatus(const String& status);

private:
    HIDManager& hidManager;     // HIDManager 的引用
    AppWiFiManager& wifiManager; // AppWiFiManager 的引用
    HardwareManager& hardwareManager; // HardwareManager 的引用

    // 新增的私有辅助方法
    String _handleHidTool(const String& toolName, const JsonObject& params);
    String _handleWiFiTool(const String& toolName, const JsonObject& params);
    String _handleGpioTool(const String& toolName, const JsonObject& params);
  
    // 内部状态变量，用于 UI 显示
    String currentLLMMode = "Chat"; // 默认对话模式
    String currentTaskStatus = "Idle"; // 默认空闲
};

#endif // TASK_MANAGER_H
