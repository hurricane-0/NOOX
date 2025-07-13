#ifndef SD_MANAGER_H
#define SD_MANAGER_H

#include <SD.h>
#include "hardware_config.h"
#include <vector>
#include <Arduino.h>
#include <ArduinoJson.h>

#define CONFIG_FILE "/config.json"

class SDManager {
public:
    SDManager();
    bool begin();
    String readFile(const String& path);
    bool writeFile(const String& path, const String& content);
    bool deleteFile(const String& path);
    std::vector<String> listScripts();

    // Config management
    bool saveConfig(const JsonDocument& doc);
    JsonDocument loadConfig();
};

#endif // SD_MANAGER_H
