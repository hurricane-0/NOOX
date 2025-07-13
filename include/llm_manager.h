#ifndef LLM_MANAGER_H
#define LLM_MANAGER_H

#include <Arduino.h>
#include "sd_manager.h"
#include <ArduinoJson.h> // Include ArduinoJson for structured responses

enum LLMProvider {
    GEMINI,
    DEEPSEEK,
    CHATGPT
    // Add other providers here
};

enum LLMMode {
    CHAT_MODE,
    ADVANCED_MODE
};

class LLMManager {
public:
    LLMManager(SDManager& sd);
    void begin();
    void setProvider(LLMProvider provider);
    void setApiKey(const String& key);
    // Removed saveApiKeyToSD() as setApiKey now handles saving
    String generateResponse(const String& prompt, LLMMode mode, const JsonArray& authorizedTools);

private:
    SDManager& sdManager;
    LLMProvider currentProvider;
    String apiKey;
    // API_KEY_FILE is now handled by SDManager's config

    String getGeminiResponse(const String& prompt, LLMMode mode, const JsonArray& authorizedTools);
    String getDeepSeekResponse(const String& prompt, LLMMode mode, const JsonArray& authorizedTools);
    String getChatGPTResponse(const String& prompt, LLMMode mode, const JsonArray& authorizedTools);

    String generateSystemPrompt(LLMMode mode, const JsonArray& authorizedTools);
    String getToolDescriptions(const JsonArray& authorizedTools);
};

#endif // LLM_MANAGER_H
