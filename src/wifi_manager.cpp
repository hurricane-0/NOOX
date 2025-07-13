#include "wifi_manager.h"
#include <WiFi.h>
#include <ArduinoJson.h> // Include ArduinoJson for config handling
#include <vector> // For std::vector

// For Wi-Fi Killer Mode
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "esp_system.h"
#include "esp_event.h" // For event loop

// Global pointer for static promiscuous mode callback
static AppWiFiManager* s_instance = nullptr;

// Packet capture callback function
void IRAM_ATTR wifi_packet_sniffer_cb(void* buf, wifi_promiscuous_pkt_type_t type) {
    if (s_instance != nullptr) {
        // Process the packet. For now, just print a message.
        // In a full implementation, you would parse the packet to identify clients, APs, etc.
        wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t*)buf;
        Serial.printf("Packet sniffed! Type: %d, RSSI: %d, Len: %d\n", type, pkt->rx_ctrl.rssi, pkt->rx_ctrl.sig_len);
        // You can access pkt->payload for raw packet data
    }
}

// Update the constructor to accept and store the SDManager reference
AppWiFiManager::AppWiFiManager(LLMManager& llm, SDManager& sd) 
    : llmManager(llm), sdManager(sd) {
    s_instance = this; // Set the static instance
}

void AppWiFiManager::begin() {
    loadAndConnect(); // Call the new helper function
}

void AppWiFiManager::loop() {
    // Keep alive or check status if needed
    // For now, just ensure connection is maintained
    if (WiFi.status() != WL_CONNECTED) {
        // Optional: Implement a more robust reconnection strategy here
        // For simplicity, we'll rely on loadAndConnect for initial connection
        // and assume it's called when credentials are saved.
    }
}

String AppWiFiManager::getIPAddress() {
    if (WiFi.status() == WL_CONNECTED) {
        return WiFi.localIP().toString();
    }
    return "N/A";
}

String AppWiFiManager::getWiFiStatus() {
    switch (WiFi.status()) {
        case WL_IDLE_STATUS: return "Idle";
        case WL_NO_SSID_AVAIL: return "No SSID";
        case WL_SCAN_COMPLETED: return "Scan Done";
        case WL_CONNECTED: return "Connected";
        case WL_CONNECT_FAILED: return "Failed";
        case WL_CONNECTION_LOST: return "Lost";
        case WL_DISCONNECTED: return "Disconnected";
        default: return "Unknown";
    }
}

bool AppWiFiManager::addWiFiCredential(const String& ssid, const String& password) {
    JsonDocument doc = sdManager.loadConfig();
    JsonObject config = doc.as<JsonObject>();

    JsonArray wifiNetworks = config["wifi_networks"].to<JsonArray>(); // This creates the array if it doesn't exist

    // Check if SSID already exists, if so, update it
    bool updated = false;
    for (JsonObject network : wifiNetworks) {
        if (network["ssid"] == ssid) {
            network["password"] = password;
            updated = true;
            break;
        }
    }

    if (!updated) {
        // Add new credential
        JsonObject newNetwork = wifiNetworks.add<JsonObject>(); // Use add<JsonObject>()
        newNetwork["ssid"] = ssid;
        newNetwork["password"] = password;
    }

    bool success = sdManager.saveConfig(doc); // Save the entire document
    if (success) {
        Serial.println("WiFi credentials added/updated. Attempting to connect...");
        loadAndConnect(); // Attempt to connect with updated credentials
    }
    return success;
}

bool AppWiFiManager::deleteWiFiCredential(const String& ssid) {
    JsonDocument doc = sdManager.loadConfig();
    JsonObject config = doc.as<JsonObject>();

    JsonArray wifiNetworks = config["wifi_networks"].as<JsonArray>();
    if (wifiNetworks.isNull()) {
        Serial.println("No WiFi networks to delete.");
        return false;
    }

    int indexToRemove = -1;
    for (size_t i = 0; i < wifiNetworks.size(); ++i) {
        if (wifiNetworks[i]["ssid"] == ssid) {
            indexToRemove = i;
            break;
        }
    }

    if (indexToRemove != -1) {
        wifiNetworks.remove(indexToRemove);
    bool success = sdManager.saveConfig(doc);
        if (success) {
            Serial.println("WiFi credential deleted. Reconnecting if current network was deleted.");
            // If the currently connected network was deleted, disconnect and try to connect to another
            if (WiFi.SSID() == ssid) {
                WiFi.disconnect();
                loadAndConnect();
            }
        }
        return success;
    }
    Serial.println("SSID not found for deletion.");
    return false;
}

std::vector<std::pair<String, String>> AppWiFiManager::getSavedCredentials() {
    std::vector<std::pair<String, String>> credentials;
    JsonDocument doc = sdManager.loadConfig();
    JsonObject config = doc.as<JsonObject>();

    JsonArray wifiNetworks = config["wifi_networks"].as<JsonArray>();
    if (!wifiNetworks.isNull()) {
        for (JsonObject network : wifiNetworks) {
            credentials.push_back({network["ssid"].as<String>(), network["password"].as<String>()});
        }
    }
    return credentials;
}

void AppWiFiManager::connectToWiFi() {
    // This function is now a helper for loadAndConnect, connecting to a specific SSID/password
    // It's kept private and called by loadAndConnect
}

void AppWiFiManager::loadAndConnect() {
    JsonDocument doc = sdManager.loadConfig();
    
    if (doc.isNull() || !doc["wifi_networks"].is<JsonArray>()) {
        Serial.println("WiFi config not found on SD card or no networks saved.");
        return;
    }

    JsonArray wifiNetworks = doc["wifi_networks"].as<JsonArray>();
    if (wifiNetworks.isNull() || wifiNetworks.size() == 0) {
        Serial.println("No WiFi networks saved.");
        return;
    }

    // Try to connect to each saved network
    for (JsonObject network : wifiNetworks) {
        String ssid = network["ssid"].as<String>();
        String password = network["password"].as<String>();

        if (ssid.length() == 0) {
            Serial.println("Skipping network with empty SSID.");
            continue;
        }

        Serial.print("Attempting to connect to WiFi: ");
        Serial.println(ssid);

        WiFi.begin(ssid.c_str(), password.c_str());
        int retries = 0;
        while (WiFi.status() != WL_CONNECTED && retries < 20) {
            delay(500);
            Serial.print(".");
            retries++;
        }

        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("\nConnected to WiFi!");
            Serial.print("IP Address: ");
            Serial.println(WiFi.localIP());
            return; // Connected successfully, exit
        } else {
            Serial.println("\nWiFi connection failed for this network.");
            WiFi.disconnect(); // Disconnect before trying next network
        }
    }
    Serial.println("Failed to connect to any saved WiFi network.");
}

void AppWiFiManager::startWifiKillerMode() {
    Serial.println("Wi-Fi Killer Mode: Starting...");
    
    // Save current WiFi mode to restore later
    // Note: WiFi.getMode() returns a uint8_t, cast to wifi_mode_t
    // This might not be perfectly reliable if WiFi is not connected or in a specific state.
    // A more robust solution might involve storing the last *intended* mode.
    // For simplicity, we'll assume STA mode is the default to return to.
    // currentWifiMode = WiFi.getMode(); 
    
    WiFi.disconnect(true); // Disconnect from any connected AP
    delay(100); // Give it a moment to disconnect

    // Set WiFi to station mode (or APSTA if needed for other functions)
    // For promiscuous mode, WIFI_MODE_STA is usually sufficient.
    esp_wifi_set_mode(WIFI_MODE_STA); 
    
    // Enable promiscuous mode
    esp_wifi_set_promiscuous(true);
    
    // Register the packet sniffer callback
    esp_wifi_set_promiscuous_rx_cb(&wifi_packet_sniffer_cb);

    // Set a default channel to monitor (e.g., channel 1)
    // In a full implementation, you'd scan channels or allow user selection.
    esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);

    Serial.println("Wi-Fi Killer Mode: Promiscuous mode enabled on channel 1.");
    Serial.println("WARNING: Wi-Fi Killer functionality (deauthentication) has ethical and legal implications.");
    Serial.println("         Use only on networks you own or have explicit permission to test.");
}

void AppWiFiManager::stopWifiKillerMode() {
    Serial.println("Wi-Fi Killer Mode: Stopping...");
    
    // Disable promiscuous mode
    esp_wifi_set_promiscuous(false);
    
    // Unregister the packet sniffer callback
    esp_wifi_set_promiscuous_rx_cb(NULL);

    // Return WiFi to normal operation (e.g., reconnect to saved network)
    Serial.println("Wi-Fi Killer Mode: Promiscuous mode disabled. Attempting to reconnect to WiFi.");
    loadAndConnect(); // Attempt to reconnect to a saved network
}
