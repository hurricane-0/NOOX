#ifndef APP_WIFI_MANAGER_H
#define APP_WIFI_MANAGER_H

#include <WiFi.h>
#include "llm_manager.h"
// #include "sd_manager.h" // Removed SDManager include
// #include <vector> // Removed as WiFiCredential management is removed

// Hardcoded WiFi credentials
extern const char* WIFI_SSID;     // !!! 请替换为您的 WiFi SSID !!!
extern const char* WIFI_PASSWORD; // !!! 请替换为您的 WiFi 密码 !!!

class AppWiFiManager {
public:
    AppWiFiManager(LLMManager& llm); // Removed SDManager reference
    void begin();
    void loop();
    String getIPAddress();
    String getWiFiStatus();
    
private:
    LLMManager& llmManager;
    // SDManager& sdManager; // Removed SDManager reference
    void connectToWiFi();
    // Removed loadAndConnect, WiFi Credential Management, and Wi-Fi Killer Mode functions
};

#endif // APP_WIFI_MANAGER_H
