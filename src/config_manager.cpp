#include "config_manager.h"

ConfigManager::ConfigManager() {
}

bool ConfigManager::begin() {
    if (!LittleFS.begin(true)) {
        Serial.println("Failed to mount LittleFS");
        return false;
    }
    return true;
}

bool ConfigManager::loadConfig() {
    File configFile = LittleFS.open(configFilePath, "r");
    if (!configFile) {
        Serial.println("Failed to open config file for reading, creating default.");
        // Create a default configuration
        configDoc["last_used"]["llm_provider"] = "deepseek";
        configDoc["last_used"]["model"] = "deepseek-chat";
        configDoc["last_used"]["wifi_ssid"] = ""; // Placeholder

        JsonObject deepseek = configDoc["llm_providers"]["deepseek"].to<JsonObject>();
        deepseek["api_key"] = "";
        deepseek["models"].add("deepseek-chat");
        deepseek["models"].add("deepseek-reasoner");

        JsonObject openrouter = configDoc["llm_providers"]["openrouter"].to<JsonObject>();
        openrouter["api_key"] = "";
        openrouter["models"].add("google/gemini-pro");
        openrouter["models"].add("openai/gpt-4o");

        JsonObject google_gemini = configDoc["llm_providers"]["google_gemini"].to<JsonObject>();
        google_gemini["api_key"] = "";
        google_gemini["models"].add("gemini-pro");
        google_gemini["models"].add("gemini-1.5-flash");

        JsonArray wifiNetworks = configDoc["wifi_networks"].to<JsonArray>();
        JsonObject defaultWifi = wifiNetworks.add<JsonObject>();
        defaultWifi["ssid"] = ""; // Placeholder
        defaultWifi["password"] = ""; // Placeholder

        return saveConfig();
    }

    DeserializationError error = deserializeJson(configDoc, configFile);
    configFile.close();
    if (error) {
        Serial.println("Failed to parse config file");
        return false;
    }
    
    Serial.println("Configuration loaded successfully.");
    return true;
}

bool ConfigManager::saveConfig() {
    File configFile = LittleFS.open(configFilePath, "w");
    if (!configFile) {
        Serial.println("Failed to open config file for writing");
        return false;
    }

    if (serializeJson(configDoc, configFile) == 0) {
        Serial.println("Failed to write to config file");
        configFile.close();
        return false;
    }

    configFile.close();
    Serial.println("Configuration saved.");
    return true;
}

JsonDocument& ConfigManager::getConfig() {
    return configDoc;
}
