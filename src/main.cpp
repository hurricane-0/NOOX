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
#include <HttpClient.h> // 显式引入 HttpClient 以满足 LLMManager 依赖
#include <LittleFS.h> // Include LittleFS for internal config, web files and agent

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
    delay(200);
    
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

    Serial.println("=====================================");

    configManager.loadConfig();

    usbShellManagerPtr = new UsbShellManager(nullptr);
    
    llmManagerPtr = new LLMManager(configManager, wifiManager, usbShellManagerPtr, &hidManager, &hardwareManager);

    usbShellManagerPtr->setLLMManager(llmManagerPtr);
    usbShellManagerPtr->setWiFiManager(&wifiManager);

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

    delay(300);
    
    // ========================================================================
    // Auto WiFi Configuration and Agent Launch
    // ========================================================================
    Serial.println("=====================================");
    Serial.println("[BOOT] Checking WiFi status...");
    
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[BOOT] No saved WiFi configuration found");
        Serial.println("[BOOT] Starting automatic WiFi configuration...");
        Serial.println("[BOOT] HID will execute PowerShell script to get WiFi credentials");
        delay(2000); // Wait for system to stabilize
        
        hidManager.autoGetWindowsWiFi();
        
        Serial.println("[BOOT] Waiting for WiFi connection...");
        // Wait up to 15 seconds for WiFi to connect
        int timeout = 15;
        while (WiFi.status() != WL_CONNECTED && timeout-- > 0) {
            delay(1000);
            Serial.print(".");
        }
        Serial.println();
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("[BOOT] WiFi connected successfully!");
        String deviceIP = WiFi.localIP().toString();
        Serial.printf("[BOOT] Device IP: %s\n", deviceIP.c_str());
        
        Serial.println("[BOOT] Starting agent download and execution...");
        delay(2000); // Wait for web server to be ready
        
        hidManager.downloadAndRunAgent(deviceIP);
        
        Serial.println("[BOOT] Agent launch sequence initiated");
        Serial.println("[BOOT] Please check PowerShell window on host");
    } else {
        Serial.println("[BOOT] WiFi auto-config failed or timed out");
        Serial.println("[BOOT] Please configure WiFi manually:");
    }
    
    Serial.println("=====================================");
}
void loop() {
    wifiManager.loop();
    delay(1); // Add a small delay to yield to other tasks
}
