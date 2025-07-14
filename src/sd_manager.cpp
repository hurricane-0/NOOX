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

std::vector<String> SDManager::listScripts() {
    // 列出 /scripts 目录下的所有脚本文件
    std::vector<String> scripts;
    File root = SD.open("/scripts");
    if (!root) {
        Serial.println("Scripts directory not found, creating it.");
        SD.mkdir("/scripts");
        root = SD.open("/scripts");
    }
    if (!root.isDirectory()) {
        Serial.println("Error: /scripts is not a directory.");
        return scripts;
    }

    File file = root.openNextFile();
    while (file) {
        if (!file.isDirectory()) {
            scripts.push_back(String(file.name()));
        }
        file.close(); // 关闭当前文件
        file = root.openNextFile(); // 打开下一个文件
    }
    root.close();
    return scripts;
}

bool SDManager::saveConfig(const JsonDocument& doc) {
    // 保存配置到 SD 卡
    File file = SD.open(CONFIG_FILE, FILE_WRITE);
    if (!file) {
        Serial.println(F("Failed to create config file"));
        return false;
    }
    if (serializeJson(doc, file) == 0) {
        Serial.println(F("Failed to write to config file"));
        file.close();
        return false;
    }
    file.close();
    return true;
}

JsonDocument SDManager::loadConfig() {
    // 从 SD 卡加载配置
    File file = SD.open(CONFIG_FILE);
    JsonDocument doc;
    if (!file) {
        Serial.println(F("Config file not found"));
        return doc; // 返回空文档
    }

    DeserializationError error = deserializeJson(doc, file);
    if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.c_str());
    }
    file.close();
    return doc;
}
