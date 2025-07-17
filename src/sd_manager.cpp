#include "sd_manager.h"
#include <SPI.h>

SDManager::SDManager() {}

bool SDManager::begin() {
    // 初始化 SD 卡
    return SD.begin(SPI_CS_PIN);
}

String SDManager::readFile(const String& path) {
    // 读取指定路径的文件内容
    File file = SD.open(path);
    if (!file) {
        return "";
    }
    String content = file.readString();
    file.close();
    return content;
}

bool SDManager::writeFile(const String& path, const String& content) {
    // 写入内容到指定路径的文件
    File file = SD.open(path, FILE_WRITE);
    if (!file) {
        return false;
    }
    file.print(content);
    file.close();
    return true;
}

bool SDManager::deleteFile(const String& path) {
    // 删除指定路径的文件
    return SD.remove(path);
}

// 辅助函数：加载 JSON 文件
static JsonDocument loadJsonFile(const String& path) {
    File file = SD.open(path);
    JsonDocument doc;
    if (!file) {
        Serial.printf("File not found: %s\n", path.c_str());
        return doc; // 返回空文档
    }

    DeserializationError error = deserializeJson(doc, file);
    if (error) {
        Serial.printf("deserializeJson() failed for %s: %s\n", path.c_str(), error.c_str());
    }
    file.close();
    return doc;
}

// 辅助函数：保存 JSON 文件
static bool saveJsonFile(const String& path, const JsonDocument& doc) {
    File file = SD.open(path, FILE_WRITE);
    if (!file) {
        Serial.printf("Failed to create file: %s\n", path.c_str());
        return false;
    }
    if (serializeJson(doc, file) == 0) {
        Serial.printf("Failed to write to file: %s\n", path.c_str());
        file.close();
        return false;
    }
    file.close();
    return true;
}

// 新增具体配置方法
JsonDocument SDManager::loadWiFiConfig() {
    return loadJsonFile("/wifi_config.json");
}

bool SDManager::saveWiFiConfig(const JsonDocument& doc) {
    return saveJsonFile("/wifi_config.json", doc);
}

JsonDocument SDManager::loadAPIKeys() {
    return loadJsonFile("/api_keys.json");
}

bool SDManager::saveAPIKeys(const JsonDocument& doc) {
    return saveJsonFile("/api_keys.json", doc);
}

// --- Automation Scripts Management ---

// Helper to get the path for an individual automation script
static String getAutomationScriptPath(const String& scriptName) {
    return "/automation_scripts/" + scriptName + ".json";
}

// Load the list of automation script names
JsonDocument SDManager::loadAutomationScripts() {
    return loadJsonFile("/automation_scripts.json");
}

// Save the list of automation script names
bool SDManager::saveAutomationScripts(const JsonDocument& doc) {
    return saveJsonFile("/automation_scripts.json", doc);
}

// Load a specific automation script by name
JsonDocument SDManager::loadAutomationScript(const String& scriptName) {
    return loadJsonFile(getAutomationScriptPath(scriptName));
}

// Save a specific automation script by name
bool SDManager::saveAutomationScript(const String& scriptName, const JsonDocument& doc) {
    bool success = saveJsonFile(getAutomationScriptPath(scriptName), doc);
    if (success) {
        // Add script name to the list if not already present
        JsonDocument scriptNamesDoc = loadAutomationScripts();
        if (scriptNamesDoc.isNull()) {
            scriptNamesDoc.to<JsonArray>(); // Create an empty array if file was empty or invalid
        }
        JsonArray scriptsArray = scriptNamesDoc.as<JsonArray>();
        bool found = false;
        for (JsonVariant v : scriptsArray) {
            if (v.is<String>() && v.as<String>() == scriptName) {
                found = true;
                break;
            }
        }
        if (!found) {
            scriptsArray.add(scriptName);
            return saveAutomationScripts(scriptNamesDoc);
        }
    }
    return success;
}

// Delete a specific automation script by name
bool SDManager::deleteAutomationScript(const String& scriptName) {
    String scriptPath = getAutomationScriptPath(scriptName);
    bool success = SD.remove(scriptPath);
    if (success) {
        // Remove script name from the list
        JsonDocument scriptNamesDoc = loadAutomationScripts();
        if (!scriptNamesDoc.isNull() && scriptNamesDoc["scripts"].is<JsonArray>()) {
            JsonArray scriptsArray = scriptNamesDoc["scripts"].as<JsonArray>();
            for (int i = 0; i < scriptsArray.size(); ++i) {
                if (scriptsArray[i].is<String>() && scriptsArray[i].as<String>() == scriptName) {
                    scriptsArray.remove(i);
                    break;
                }
            }
            return saveAutomationScripts(scriptNamesDoc);
        }
    }
    return success;
}

// List automation script names
std::vector<String> SDManager::listAutomationScriptNames() {
    std::vector<String> scriptNames;
    JsonDocument doc = loadAutomationScripts();
    // The automation_scripts.json now directly contains an array of strings
    if (!doc.isNull() && doc.is<JsonArray>()) { // Check if the root is an array
        for (JsonVariant v : doc.as<JsonArray>()) {
            if (v.is<String>()) {
                scriptNames.push_back(v.as<String>());
            }
        }
    }
    return scriptNames;
}
