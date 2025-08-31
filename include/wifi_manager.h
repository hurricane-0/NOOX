#ifndef APP_WIFI_MANAGER_H
#define APP_WIFI_MANAGER_H

#include <WiFi.h>
#include "llm_manager.h"
#include "config_manager.h"
#include <ArduinoJson.h>

class AppWiFiManager {
public:
    AppWiFiManager(LLMManager& llm, ConfigManager& config);
    void begin();
    void loop();
    String getIPAddress();
    String getWiFiStatus();

    // New public methods for WiFi management
    bool connectToWiFi(const String& ssid);
    void disconnect();
    bool addWiFi(const String& ssid, const String& password);
    bool deleteWiFi(const String& ssid);
    JsonArray getSavedSSIDs();

private:
    LLMManager& llmManager;
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
