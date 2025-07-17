#ifndef APP_WIFI_MANAGER_H
#define APP_WIFI_MANAGER_H

#include <WiFi.h>
#include "llm_manager.h"
#include "sd_manager.h" // Include SDManager
#include <vector> // For std::vector

// Define a struct to hold WiFi credentials
struct WiFiCredential {
    String ssid;
    String password;
};

class AppWiFiManager {
public:
    AppWiFiManager(LLMManager& llm, SDManager& sd); // Add SDManager reference
    void begin();
    void loop();
    String getIPAddress();
    String getWiFiStatus();
    
    // WiFi Credential Management
    bool addWiFiCredential(const String& ssid, const String& password);
    bool deleteWiFiCredential(const String& ssid);
    std::vector<WiFiCredential> getSavedCredentials(); // Change return type to vector of WiFiCredential

    // Wi-Fi Killer Mode
    void startWifiKillerMode();
    void stopWifiKillerMode();

private:
    LLMManager& llmManager;
    SDManager& sdManager; // Store reference to SDManager
    void connectToWiFi();
    void loadAndConnect(); // New helper to load credentials and connect
};

#endif // APP_WIFI_MANAGER_H
