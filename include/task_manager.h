#ifndef TASK_MANAGER_H
#define TASK_MANAGER_H

#include <Arduino.h>
#include "hid_manager.h" // 引入 HIDManager 头文件
#include "hardware_manager.h" // 引入 HardwareManager 头文件用于 GPIO 任务
#include "sd_manager.h" // 引入 SDManager 头文件用于脚本加载
#include <ArduinoJson.h> // 引入 ArduinoJson 用于 JsonObject

// 前向声明，避免循环依赖
class AppWiFiManager;
class HardwareManager; // HardwareManager 的前向声明
class TimerManager; // TimerManager 的前向声明
class BLEManager; // BLEManager 的前向声明
class SDManager; // SDManager 的前向声明

class TaskManager {
public:
    TaskManager(HIDManager& hid, AppWiFiManager& wifi, HardwareManager& hw, TimerManager& timer, BLEManager& ble, SDManager& sd); // 修改后的构造函数，包含 SDManager
    String executeTool(const String& toolName, const JsonObject& params); // 用于 LLM 工具调用的新方法
    void executeTask(const String& taskName, const String& params); // 保留的旧方法，兼容性需要
    bool isTimerRunning(); // 检查定时器状态的方法
    // 可在此添加更多任务管理功能

private:
    HIDManager& hidManager;     // HIDManager 的引用
    AppWiFiManager& wifiManager; // AppWiFiManager 的引用
    HardwareManager& hardwareManager; // HardwareManager 的引用
    TimerManager& timerManager; // TimerManager 的引用
    BLEManager& bleManager; // BLEManager 的引用
    SDManager& sdManager; // SDManager 的引用，用于脚本加载

    void handleHIDTask(const String& params);
    void handleWiFiTask(const String& params); // 通用 WiFi 任务的占位方法
    void handleBluetoothTask(const String& params);
    void handleTimerTask(const String& params);
    void handleGpioTask(const String& params);
    void handleWifiKillerTask(const String& params); // 专用于 Wi-Fi Killer 模式

    // 用于 TimerManager 集成的静态实例和回调
    static TaskManager* _instance;
    static void IRAM_ATTR onTimerTaskCallback();
    void _handleTimerCallback(); // 非静态辅助回调方法
};

#endif // TASK_MANAGER_H
