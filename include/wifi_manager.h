#ifndef APP_WIFI_MANAGER_H
#define APP_WIFI_MANAGER_H

#include <WiFi.h>
// #include "llm_manager.h" // Removed direct dependency
#include "config_manager.h"
#include <ArduinoJson.h>

class AppWiFiManager {
public:
    AppWiFiManager(ConfigManager& config); // Updated constructor
    void begin();
    void loop();
    String getIPAddress();
    String getWiFiStatus();
    String getSSID(); // Added to get current connected SSID

    // New public methods for WiFi management
    bool connectToWiFi(const String& ssid, const String& password); // Updated to accept password
    void disconnect();
    bool addWiFi(const String& ssid, const String& password); // Updated to accept password
    bool deleteWiFi(const String& ssid);
    JsonArray getSavedSSIDs();

private:
    // LLMManager& llmManager; // Removed
    ConfigManager& configManager;
    
    enum WiFiConnectionState {
        IDLE,
        CONNECTING,
        CONNECTED,
        FAILED
    };

    WiFiConnectionState _connectionState = IDLE;
    unsigned long _connectionAttemptStartTime = 0;
    const long WIFI_CONNECTION_TIMEOUT_MS = 30000; // 30 seconds timeout

    void connectToLastSSID();
    void handleWiFiConnection();
};

#endif // APP_WIFI_MANAGER_H
