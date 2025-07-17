#include <Arduino.h>
#include "hardware_manager.h"
#include "wifi_manager.h"
#include "ui_manager.h"
#include "llm_manager.h"
#include "task_manager.h"
#include "hid_manager.h"
#include "web_manager.h"
#include <HttpClient.h> // 显式引入 HttpClient 以满足 LLMManager 依赖

HardwareManager hardwareManager;
LLMManager llmManager; // 调整构造函数
AppWiFiManager wifiManager(llmManager); // 调整构造函数
HIDManager hidManager;
TaskManager taskManager(hidManager, wifiManager, hardwareManager); // 调整构造函数，移除 TimerManager
UIManager uiManager(hardwareManager, wifiManager, taskManager); // 调整构造函数
WebManager webManager(llmManager, taskManager, wifiManager); // 调整构造函数

void setup() {
    Serial.begin(115200);
    hardwareManager.begin();
    llmManager.begin();
    wifiManager.begin();
    uiManager.begin();
    hidManager.begin();
    webManager.begin();
}
void loop() {
    hardwareManager.update();
    wifiManager.loop();
    uiManager.update();
    webManager.loop();
}
