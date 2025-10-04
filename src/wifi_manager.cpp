#include "wifi_manager.h"
#include <WiFi.h>

AppWiFiManager::AppWiFiManager(ConfigManager& config) 
    : configManager(config) {
}

void AppWiFiManager::begin() {
    WiFi.mode(WIFI_STA);
    connectToLastSSID();
}

void AppWiFiManager::loop() {
    handleWiFiConnection();
}

String AppWiFiManager::getIPAddress() {
    if (_connectionState == CONNECTED) {
        return WiFi.localIP().toString();
    }
    return "0.0.0.0";
}

String AppWiFiManager::getWiFiStatus() {
    switch (_connectionState) {
        case IDLE: return "Idle";
        case CONNECTING: return "Connecting";
        case CONNECTED: return "Connected";
        case FAILED: return "Failed";
        default: return "Unknown";
    }
}

String AppWiFiManager::getSSID() {
    if (_connectionState == CONNECTED) {
        return WiFi.SSID();
    }
    return "N/A";
}

void AppWiFiManager::connectToLastSSID() {
    JsonDocument& config = configManager.getConfig();
    String lastSSID = config["last_used"]["wifi_ssid"].as<String>();
    String lastPassword = ""; // Need to retrieve password as well

    if (lastSSID.length() > 0) {
        JsonArray wifiNetworks = config["wifi_networks"].as<JsonArray>();
        for (JsonObject network : wifiNetworks) {
            if (network["ssid"].as<String>() == lastSSID) {
                lastPassword = network["password"].as<String>();
                break;
            }
        }
        Serial1.printf("Attempting to connect to last known WiFi: %s\n", lastSSID.c_str());
        connectToWiFi(lastSSID, lastPassword); // Pass password
    } else {
        Serial1.println("No last used WiFi SSID found in config.");
        _connectionState = IDLE; // Ensure state is IDLE if no SSID to connect
    }
}

bool AppWiFiManager::connectToWiFi(const String& ssid, const String& password) {
    if (_connectionState == CONNECTED && WiFi.SSID() == ssid) {
        Serial1.println("Already connected to this WiFi.");
        return true;
    }

    Serial1.printf("Initiating connection to WiFi: %s\n", ssid.c_str());
    WiFi.begin(ssid.c_str(), password.c_str());
    _connectionAttemptStartTime = millis();
    _connectionState = CONNECTING;
    return true;
}

void AppWiFiManager::handleWiFiConnection() {
    if (_connectionState == CONNECTING) {
        if (WiFi.status() == WL_CONNECTED) {
            Serial1.println("\nConnected to WiFi!");
            Serial1.print("IP Address: ");
            Serial1.println(WiFi.localIP());
            JsonDocument& config = configManager.getConfig();
            config["last_used"]["wifi_ssid"] = WiFi.SSID();
            configManager.saveConfig();
            _connectionState = CONNECTED;
        } else if (millis() - _connectionAttemptStartTime > WIFI_CONNECTION_TIMEOUT_MS) {
            Serial1.println("\nWiFi connection timed out.");
            WiFi.disconnect();
            _connectionState = FAILED;
        } else {
            // Still connecting, do nothing or print a dot
            // Serial1.print(".");
        }
    } else if (_connectionState == CONNECTED) {
        if (WiFi.status() != WL_CONNECTED) {
            Serial1.println("WiFi connection lost. Attempting to reconnect...");
            _connectionState = IDLE; // Go back to IDLE to trigger reconnection logic
            connectToLastSSID(); // Attempt to reconnect to the last known SSID
        }
    }
}

void AppWiFiManager::disconnect() {
    if (WiFi.status() == WL_CONNECTED) {
        Serial1.printf("Disconnecting from %s\n", WiFi.SSID().c_str());
        WiFi.disconnect(true);
    }
}

bool AppWiFiManager::addWiFi(const String& ssid, const String& password) {
    JsonDocument& config = configManager.getConfig();
    JsonArray wifiNetworks = config["wifi_networks"].as<JsonArray>();

    // Check if SSID already exists
    for (JsonObject network : wifiNetworks) {
        if (network["ssid"].as<String>() == ssid) {
            // Update password if SSID exists
            network["password"] = password;
            Serial1.printf("Updated password for SSID: %s\n", ssid.c_str());
            return configManager.saveConfig();
        }
    }

    // Add new network
    JsonObject newNetwork = wifiNetworks.add<JsonObject>();
    newNetwork["ssid"] = ssid;
    newNetwork["password"] = password;
    Serial1.printf("Added new WiFi network: %s\n", ssid.c_str());
    return configManager.saveConfig();
}

bool AppWiFiManager::deleteWiFi(const String& ssid) {
    JsonDocument& config = configManager.getConfig();
    JsonArray wifiNetworks = config["wifi_networks"].as<JsonArray>();

    for (int i = 0; i < wifiNetworks.size(); i++) {
        if (wifiNetworks[i]["ssid"].as<String>() == ssid) {
            wifiNetworks.remove(i);
            Serial1.printf("Removed WiFi network: %s\n", ssid.c_str());
            return configManager.saveConfig();
        }
    }
    
    Serial1.printf("SSID %s not found for deletion.\n", ssid.c_str());
    return false;
}

JsonArray AppWiFiManager::getSavedSSIDs() {
    JsonDocument& config = configManager.getConfig();
    return config["wifi_networks"].as<JsonArray>();
}
