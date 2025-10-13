#include <Arduino.h>
#include "esp32-hal-psram.h" // For PSRAM initialization
#include "esp_log.h" // For ESP-IDF logging configuration
#include "hardware_config.h" // Include hardware pin definitions
#include "hardware_manager.h"
#include "wifi_manager.h"
#include "ui_manager.h"
#include "llm_manager.h"
#include "hid_manager.h"
#include "web_manager.h"
#include "config_manager.h"
#include "usb_shell_manager.h" // Include UsbShellManager
#include <USBMSC.h> // Explicitly include USBMSC for main.cpp
#include <HttpClient.h> // 显式引入 HttpClient 以满足 LLMManager 依赖
#include <LittleFS.h> // Include LittleFS for internal config and web files
#include <FFat.h> // Include FFat for USBMSC (U disk)

USBMSC usb_msc_driver; // Instantiate USBMSC

HardwareManager hardwareManager;
ConfigManager configManager;
AppWiFiManager wifiManager(configManager); // Corrected constructor
HIDManager hidManager;

// Declare pointers for LLMManager, UsbShellManager, and WebManager to handle circular dependency and initialization order
LLMManager* llmManagerPtr;
UsbShellManager* usbShellManagerPtr;
WebManager* webManagerPtr; // Declare WebManager pointer
UIManager* uiManagerPtr; // Declare UIManager pointer

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
    // Initialize UART0 for Serial output
    Serial.begin(115200);
    delay(500);
    
    Serial.println("Serial setup");
    Serial.println("Setup starting...");
    Serial.println("=====================================");
    
    hardwareManager.begin();

    // ========================================================================
    // STEP 1: Initialize LittleFS (for internal config and web files)
    // ========================================================================
    Serial.println("[FS] Initializing LittleFS...");
    if (!LittleFS.begin(true)) { // true = format if mount fails, uses "spiffs" partition by default
        Serial.println("[FS]  LittleFS Mount Failed!");
        return;
    }
    Serial.println("[FS]  LittleFS Mounted successfully");
    Serial.printf("[FS]  Total: %u bytes (%.2f MB)\n", 
                   LittleFS.totalBytes(), LittleFS.totalBytes() / 1024.0 / 1024.0);
    Serial.printf("[FS]  Used:  %u bytes (%.2f MB)\n", 
                   LittleFS.usedBytes(), LittleFS.usedBytes() / 1024.0 / 1024.0);

    // ========================================================================
    // STEP 2: Initialize FFat (dedicated for USBMSC U disk)
    // ========================================================================
    Serial.println("[FS] Initializing FFat for USBMSC...");
    if (!FFat.begin(true, "/ffat")) { // true = format if mount fails
        Serial.println("[FS]  FFat Mount Failed!");
        Serial.println("[FS]  USBMSC will not work without FFat!");
        return;
    }
    Serial.println("[FS]  FFat Mounted successfully");
    Serial.printf("[FS]  Total: %u bytes (%.2f MB)\n", 
                   FFat.totalBytes(), FFat.totalBytes() / 1024.0 / 1024.0);
    Serial.printf("[FS]  Used:  %u bytes (%.2f MB)\n", 
                   FFat.usedBytes(), FFat.usedBytes() / 1024.0 / 1024.0);

    // Check if agent file exists in FFat partition
    if (!FFat.exists("/noox-host-agent.exe")) {
        Serial.println("[FS]  noox-host-agent.exe NOT found in FFat");
        Serial.println("[FS]  Please upload the agent file via:");
        Serial.println("[FS]  1. PlatformIO: pio run --target uploadfs");
        Serial.println("[FS]  2. Web interface: /upload_agent endpoint");
    } else {
        File agentFile = FFat.open("/noox-host-agent.exe", "r");
        if (agentFile) {
            Serial.printf("[FS] Agent file found: %u bytes (%.2f MB)\n", 
                          agentFile.size(), agentFile.size() / 1024.0 / 1024.0);
            agentFile.close();
        }
    }

    // ========================================================================
    // STEP 3: Initialize USBMSC (will automatically use FFat)
    // ========================================================================
    Serial.println("[USB] Configuring USBMSC driver...");
    usb_msc_driver.vendorID("NOOX");      // 8 characters max
    usb_msc_driver.productID("NOOXDisk"); // 16 characters max
    usb_msc_driver.productRevision("1.0");
    usb_msc_driver.mediaPresent(true);

    // Calculate block count for FFat partition
    uint32_t ffat_total = FFat.totalBytes();
    uint16_t block_size = 512; // Standard USB MSC block size
    uint32_t block_count = ffat_total / block_size;

    // ESP32-Arduino will automatically bind USBMSC to the last mounted FAT filesystem (FFat)
    if (usb_msc_driver.begin(block_count, block_size)) {
        Serial.println("[USB]  USB MSC driver started successfully");
        Serial.println("[USB]  PC will see NOOX as a removable disk");
        Serial.println("[USB]  FFat partition is accessible via U disk");
    } else {
        Serial.println("[USB]  USB MSC driver failed to start");
    }
    Serial.println("=====================================");

    configManager.loadConfig();

    usbShellManagerPtr = new UsbShellManager(nullptr, &wifiManager);
    
    llmManagerPtr = new LLMManager(configManager, wifiManager, usbShellManagerPtr, &hidManager, &hardwareManager);

    usbShellManagerPtr->setLLMManager(llmManagerPtr);

    wifiManager.begin();
    
    llmManagerPtr->begin();
    
    // Instantiate UIManager after LLMManager is ready
    uiManagerPtr = new UIManager(hardwareManager, wifiManager, *llmManagerPtr);
    uiManagerPtr->begin();

    hidManager.begin();
    
    // Initialize UsbShellManager (CDC) after USBMSC
    usbShellManagerPtr->begin();

    // Instantiate WebManager AFTER LittleFS is already mounted
    // WebManager will skip LittleFS.begin() since it's already mounted
    webManagerPtr = new WebManager(*llmManagerPtr, wifiManager, configManager, hardwareManager);
    webManagerPtr->begin();

    // Create FreeRTOS tasks for all managers
    xTaskCreatePinnedToCore(webTask, "WebTask", 4096, NULL, 2, NULL, 0);
    xTaskCreatePinnedToCore(uiTask, "UITask", 4096, NULL, 2, NULL, 1);
    xTaskCreatePinnedToCore(usbTask, "USBTask", 4096, NULL, 2, NULL, 1);
    xTaskCreatePinnedToCore(llmTask, "LLMTask", 8192 * 4, NULL, 2, NULL, 0);

    Serial.println("Setup complete. Starting main loop...");
}
void loop() {
    wifiManager.loop();
    delay(1); // Add a small delay to yield to other tasks
}
