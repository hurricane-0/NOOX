#include <Arduino.h>
#include "hardware_manager.h"
#include "sd_manager.h"
#include "wifi_manager.h"
#include "ui_manager.h"
#include "llm_manager.h"
#include "task_manager.h"
#include "hid_manager.h"
#include "ble_manager.h"
#include "web_manager.h" // Include WebManager header
#include "timer_manager.h" // Include TimerManager header
#include <HttpClient.h> // Explicitly include HttpClient for LLMManager dependency

HardwareManager hardwareManager;
SDManager sdManager;
LLMManager llmManager(sdManager); // Pass sdManager to LLMManager
AppWiFiManager wifiManager(llmManager, sdManager); // Pass llmManager and sdManager to WiFiManager
TimerManager timerManager; // Declare TimerManager instance first to resolve dependency
HIDManager hidManager; // Declare hidManager
BLEManager bleManager; // Declare BLEManager instance
TaskManager taskManager(hidManager, wifiManager, hardwareManager, timerManager, bleManager, sdManager); // Pass hidManager, wifiManager, hardwareManager, timerManager, bleManager, and sdManager to TaskManager
UIManager uiManager(hardwareManager, wifiManager, sdManager, taskManager); // taskManager is now declared
WebManager webManager(llmManager, taskManager, wifiManager); // Create an instance of WebManager, passing llmManager, taskManager, and wifiManager

void setup() {
    Serial.begin(115200);
    hardwareManager.begin();
    sdManager.begin();
    llmManager.begin(); // Load API key from SD
    wifiManager.begin();
    uiManager.begin();
    hidManager.begin();
    bleManager.begin();
    webManager.begin(); // Initialize WebManager
    timerManager.begin(); // Initialize TimerManager
}

void loop() {
    hardwareManager.update();
    wifiManager.loop();
    uiManager.update();
    webManager.loop(); // Call WebManager loop
    webManager.handleUsbSerialWebRequests(); // Handle USB serial web requests
}
