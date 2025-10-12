#include <Arduino.h>
#include "esp32-hal-psram.h" // For PSRAM initialization
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
AppWiFiManager wifiManager(configManager); // Corrected constructor
HIDManager hidManager;

// Declare pointers for LLMManager, UsbShellManager, and WebManager to handle circular dependency and initialization order
LLMManager* llmManagerPtr;
UsbShellManager* usbShellManagerPtr;
WebManager* webManagerPtr; // Declare WebManager pointer

TaskManager taskManager(hidManager, wifiManager, hardwareManager);
// UIManager uiManager(hardwareManager, wifiManager, taskManager); // Old constructor call
// Declare UIManager after llmManagerPtr is instantiated
UIManager* uiManagerPtr;

// Task for WebManager
void webTask(void* pvParameters) {
    for (;;) {
        webManagerPtr->loop();
        vTaskDelay(pdMS_TO_TICKS(10)); // Small delay to yield
    }
}

// Task for UIManager
void uiTask(void* pvParameters) {
    for (;;) {
        uiManagerPtr->update();
        vTaskDelay(pdMS_TO_TICKS(10)); // Small delay to yield
    }
}

// Task for UsbShellManager
void usbTask(void* pvParameters) {
    for (;;) {
        usbShellManagerPtr->loop();
        vTaskDelay(pdMS_TO_TICKS(10)); // Small delay to yield
    }
}

// Task for LLMManager
void llmTask(void* pvParameters) {
    for (;;) {
        llmManagerPtr->loop();
        vTaskDelay(pdMS_TO_TICKS(10)); // Small delay to yield
    }
}

void setup() {
    // Initialize UART1 for Serial output
    Serial.begin(115200);
    delay(500);
    Serial.println("Serial setup");
    Serial.println("Setup starting...");
    
    hardwareManager.begin();
    configManager.begin();
    configManager.loadConfig();

    // Instantiate UsbShellManager first, passing nullptr for LLMManager initially
    usbShellManagerPtr = new UsbShellManager(nullptr, &wifiManager);
    // Instantiate LLMManager, passing the UsbShellManager pointer
    llmManagerPtr = new LLMManager(configManager, wifiManager, usbShellManagerPtr);
    // Resolve circular dependency by setting LLMManager in UsbShellManager
    usbShellManagerPtr->setLLMManager(llmManagerPtr);

    wifiManager.begin();
    
    llmManagerPtr->begin();
    
    // Instantiate UIManager after LLMManager is ready
    uiManagerPtr = new UIManager(hardwareManager, wifiManager, taskManager, *llmManagerPtr);
    uiManagerPtr->begin();

    hidManager.begin();
    // Instantiate WebManager after LLMManager and UsbShellManager are initialized
    webManagerPtr = new WebManager(*llmManagerPtr, taskManager, wifiManager, configManager);
    webManagerPtr->begin();

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
    usb_msc_driver.vendorID("NOOX_Agent");
    usb_msc_driver.productID("NOOXDisk");
    usb_msc_driver.productRevision("1.0");

    // Call USBMSC::begin with LittleFS capacity information
    if (usb_msc_driver.begin(block_count, block_size)) {
        Serial.println("USB MSC driver started successfully");
    } else {
        Serial.println("USB MSC driver failed to start");
    }

    usbShellManagerPtr->begin(); // Initialize UsbShellManager

    // Create FreeRTOS tasks for all managers
    xTaskCreatePinnedToCore(webTask, "WebTask", 4096, NULL, 2, NULL, 0);
    xTaskCreatePinnedToCore(uiTask, "UITask", 4096, NULL, 2, NULL, 1);
    xTaskCreatePinnedToCore(usbTask, "USBTask", 4096, NULL, 2, NULL, 1);
    xTaskCreatePinnedToCore(llmTask, "LLMTask", 8192 * 4, NULL, 2, NULL, 1);

    Serial.println("Setup complete. Starting main loop...");
}
void loop() {
    wifiManager.loop();
    delay(1); // Add a small delay to yield to other tasks
}
