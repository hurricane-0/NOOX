#ifndef LLM_MANAGER_H
#define LLM_MANAGER_H

#include <Arduino.h>
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
    LLMManager(); // Modified constructor
    void begin();
    void setProvider(LLMProvider provider);
    void setApiKey(const String& key); // This will now set the hardcoded key
    String generateResponse(const String& prompt, LLMMode mode, const JsonArray& authorizedTools);

private:
    LLMProvider currentProvider;
    String apiKey; // API key will be hardcoded or set directly

    String getGeminiResponse(const String& prompt, LLMMode mode, const JsonArray& authorizedTools);
    String getDeepSeekResponse(const String& prompt, LLMMode mode, const JsonArray& authorizedTools);
    String getChatGPTResponse(const String& prompt, LLMMode mode, const JsonArray& authorizedTools);

    String generateSystemPrompt(LLMMode mode, const JsonArray& authorizedTools);
    String getToolDescriptions(const JsonArray& authorizedTools);
};

#endif // LLM_MANAGER_H
