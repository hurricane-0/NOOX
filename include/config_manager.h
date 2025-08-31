#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <ArduinoJson.h>
#include <LittleFS.h>

class ConfigManager {
public:
    ConfigManager();
    bool begin();
    bool loadConfig();
    bool saveConfig();

    JsonDocument& getConfig();

private:
    JsonDocument configDoc;
    const char* configFilePath = "/config.json";
};

#endif // CONFIG_MANAGER_H
