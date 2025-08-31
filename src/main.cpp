#include <Arduino.h>
#include "hardware_manager.h"
#include "wifi_manager.h"
#include "ui_manager.h"
#include "llm_manager.h"
#include "task_manager.h"
#include "hid_manager.h"
#include "web_manager.h"
#include "config_manager.h"
#include <HttpClient.h> // 显式引入 HttpClient 以满足 LLMManager 依赖

HardwareManager hardwareManager;
ConfigManager configManager;
LLMManager llmManager(configManager);
AppWiFiManager wifiManager(llmManager, configManager);
HIDManager hidManager;
TaskManager taskManager(hidManager, wifiManager, hardwareManager);
UIManager uiManager(hardwareManager, wifiManager, taskManager);
WebManager webManager(llmManager, taskManager, wifiManager, configManager);

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("Setup starting...");

    hardwareManager.begin();
    
    configManager.begin();
    configManager.loadConfig();

    llmManager.begin();
    llmManager.startLLMTask();
    
    wifiManager.begin();
    uiManager.begin();
    hidManager.begin();
    webManager.begin();
    
    Serial.println("Setup complete. Starting main loop...");
}
void loop() {
    hardwareManager.update();
    wifiManager.loop();
    uiManager.update();
    webManager.loop();
}
