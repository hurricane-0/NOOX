#include "config_manager.h"

// 构造函数
ConfigManager::ConfigManager() {
}

// 加载配置文件
bool ConfigManager::loadConfig() {
    File configFile = LittleFS.open(configFilePath, "r"); // 以只读模式打开配置文件
    if (!configFile) { // 如果文件不存在或无法打开
        Serial.println("Failed to open config file for reading, creating default."); // 打印信息，创建默认配置
        // 创建默认配置
        configDoc["last_used"]["llm_provider"] = "deepseek"; // 默认 LLM 提供商
        configDoc["last_used"]["model"] = "deepseek-chat";   // 默认模型
        configDoc["last_used"]["wifi_ssid"] = "CMCC-Tjv9";            // WiFi SSID 占位符

        // DeepSeek LLM 提供商配置
        JsonObject deepseek = configDoc["llm_providers"]["deepseek"].to<JsonObject>();
        deepseek["api_key"] = "sk-3f54327d09514e21868e78aca13730dd"; // DeepSeek API 密钥
        deepseek["models"].add("deepseek-chat");     // DeepSeek 聊天模型
        deepseek["models"].add("deepseek-reasoner"); // DeepSeek 推理模型

        // OpenRouter LLM 提供商配置
        JsonObject openrouter = configDoc["llm_providers"]["openrouter"].to<JsonObject>();
        openrouter["api_key"] = "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"; // OpenRouter API 密钥
        openrouter["models"].add("z-ai/glm-4.5-air:free");
        openrouter["models"].add("qwen/qwen3-235b-a22b:free");
        openrouter["models"].add("qwen/qwen3-coder:free");
        openrouter["models"].add("deepseek/deepseek-chat-v3.1:free");

        // OpenAI LLM 提供商配置
        JsonObject openai = configDoc["llm_providers"]["openai"].to<JsonObject>();
        openai["api_key"] = ""; // OpenAI API 密钥
        openai["models"].add("gpt-4o");       // OpenAI GPT-4o 模型
        openai["models"].add("gpt-3.5-turbo"); // OpenAI GPT-3.5-turbo 模型

        // WiFi 网络配置
        JsonArray wifiNetworks = configDoc["wifi_networks"].to<JsonArray>();
        JsonObject defaultWifi = wifiNetworks.add<JsonObject>();
        defaultWifi["ssid"] = "CMCC-Tjv9";     // WiFi SSID 占位符
        defaultWifi["password"] = "n2w5yk6u"; // WiFi 密码占位符

        return saveConfig(); // 保存默认配置
    }

    DeserializationError error = deserializeJson(configDoc, configFile); // 反序列化 JSON 配置
    configFile.close(); // 关闭文件
    if (error) { // 如果解析失败
        Serial.println("Failed to parse config file"); // 打印错误信息
        return false;
    }
    
    Serial.println("Configuration loaded successfully."); // 打印加载成功信息
    return true; // 加载成功
}

// 保存配置文件
bool ConfigManager::saveConfig() {
    File configFile = LittleFS.open(configFilePath, "w"); // 以写入模式打开配置文件
    if (!configFile) { // 如果文件不存在或无法打开
        Serial.println("Failed to open config file for writing"); // 打印错误信息
        return false;
    }

    if (serializeJson(configDoc, configFile) == 0) { // 如果序列化 JSON 失败
        Serial.println("Failed to write to config file"); // 打印错误信息
        configFile.close(); // 关闭文件
        return false;
    }

    configFile.close(); // 关闭文件
    Serial.println("Configuration saved."); // 打印保存成功信息
    return true; // 保存成功
}

// 获取配置文档的引用
JsonDocument& ConfigManager::getConfig() {
    return configDoc; // 返回配置文档
}
