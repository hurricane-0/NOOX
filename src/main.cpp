#include <Arduino.h>
#include "hardware_manager.h"
#include "sd_manager.h"
#include "wifi_manager.h"
#include "ui_manager.h"
#include "llm_manager.h"
#include "task_manager.h"
#include "hid_manager.h"
#include "ble_manager.h"
#include "web_manager.h" // 引入 WebManager 头文件
#include "timer_manager.h" // 引入 TimerManager 头文件
#include <HttpClient.h> // 显式引入 HttpClient 以满足 LLMManager 依赖

HardwareManager hardwareManager;
SDManager sdManager;
LLMManager llmManager(sdManager); // 将 sdManager 传递给 LLMManager
AppWiFiManager wifiManager(llmManager, sdManager); // 将 llmManager 和 sdManager 传递给 WiFiManager
TimerManager timerManager; // 首先声明 TimerManager 实例以解决依赖关系
HIDManager hidManager; // 声明 hidManager
BLEManager bleManager; // 声明 BLEManager 实例
TaskManager taskManager(hidManager, wifiManager, hardwareManager, timerManager, bleManager, sdManager); // 将 hidManager、wifiManager、hardwareManager、timerManager、bleManager 和 sdManager 传递给 TaskManager
UIManager uiManager(hardwareManager, wifiManager, sdManager, taskManager); // taskManager 现已声明
WebManager webManager(llmManager, taskManager, wifiManager); // 创建 WebManager 实例，传递 llmManager、taskManager 和 wifiManager

void setup() {
    Serial.begin(115200);
    hardwareManager.begin();
    sdManager.begin();
    llmManager.begin(); // 从 SD 卡加载 API key
    wifiManager.begin();
    uiManager.begin();
    hidManager.begin();
    bleManager.begin();
    webManager.begin(); // 初始化 WebManager
    timerManager.begin(); // 初始化 TimerManager
}

void loop() {
    hardwareManager.update();
    wifiManager.loop();
    uiManager.update();
    webManager.loop(); // 调用 WebManager 的 loop
    webManager.handleUsbSerialWebRequests(); // 处理 USB 串口 Web 请求
}
