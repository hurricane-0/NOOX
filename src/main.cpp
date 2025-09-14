#include <Arduino.h>
#include "hardware_manager.h"
#include "wifi_manager.h"
#include "ui_manager.h"
#include "llm_manager.h"
#include "task_manager.h"
#include "hid_manager.h"
#include "web_manager.h"
#include "config_manager.h"
#include "usb_shell_manager.h" // Include UsbShellManager
#include <USBMSC.h> // Explicitly include USBMSC for main.cpp
#include <HttpClient.h> // 显式引入 HttpClient 以满足 LLMManager 依赖
#include <LittleFS.h> // Include LittleFS for MSD

USBMSC usb_msc_driver; // Instantiate USBMSC

HardwareManager hardwareManager;
ConfigManager configManager;
LLMManager llmManager(configManager);
AppWiFiManager wifiManager(llmManager, configManager);
HIDManager hidManager;
TaskManager taskManager(hidManager, wifiManager, hardwareManager);
UIManager uiManager(hardwareManager, wifiManager, taskManager);
WebManager webManager(llmManager, taskManager, wifiManager, configManager);
UsbShellManager usbShellManager(&llmManager); // Instantiate UsbShellManager

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

    // Initialize LittleFS for MSD
    if (!LittleFS.begin()) {
        Serial.println("LittleFS Mount Failed!");
        return;
    }
    Serial.println("LittleFS Mounted.");

    // Get LittleFS capacity information
    uint32_t total_bytes = LittleFS.totalBytes();
    uint16_t block_size = 512; // Assume USB MSC block size is 512 bytes
    uint32_t block_count = total_bytes / block_size;

    // Ensure total bytes are divisible by block size, or round up
    if (total_bytes % block_size != 0) {
         block_count++;
    }

    // Set vendorID, productID, etc. for USBMSC
    usb_msc_driver.vendorID("AIHi_Agent");
    usb_msc_driver.productID("MyAIHiDisk");
    usb_msc_driver.productRevision("1.0");

    // Call USBMSC::begin with LittleFS capacity information
    if (usb_msc_driver.begin(block_count, block_size)) {
        Serial.println("USB MSC driver started successfully");
    } else {
        Serial.println("USB MSC driver failed to start");
    }

    usbShellManager.begin(); // Initialize UsbShellManager
    
    Serial.println("Setup complete. Starting main loop...");
}
void loop() {
    hardwareManager.update();
    wifiManager.loop();
    uiManager.update();
    webManager.loop();
    usbShellManager.loop(); // Call UsbShellManager loop
}
