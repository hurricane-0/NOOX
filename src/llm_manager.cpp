#include "llm_manager.h"
#include <HttpClient.h>
#include <WiFiClientSecure.h>

// Define API endpoints - some might be consolidated if providers share hosts
const char* GEMINI_API_HOST = "generativelanguage.googleapis.com";
const char* DEEPSEEK_API_HOST = "api.deepseek.com";
const char* OPENROUTER_API_HOST = "openrouter.ai";
// Note: ChatGPT uses api.openai.com, which can be a separate constant if needed

// Constructor
LLMManager::LLMManager(ConfigManager& config) : configManager(config) {
    llmRequestQueue = xQueueCreate(5, sizeof(LLMRequest));
    llmResponseQueue = xQueueCreate(5, sizeof(LLMResponse));

    if (llmRequestQueue == NULL || llmResponseQueue == NULL) {
        Serial.println("Error creating LLM queues!");
    }
}

// begin method to load configuration
void LLMManager::begin() {
    JsonDocument& config = configManager.getConfig();
    currentProvider = config["last_used"]["llm_provider"].as<String>();
    currentModel = config["last_used"]["model"].as<String>();
    currentApiKey = config["llm_providers"][currentProvider]["api_key"].as<String>();

    Serial.printf("LLMManager initialized. Provider: %s, Model: %s\n", currentProvider.c_str(), currentModel.c_str());
}

// Start the LLM processing task
void LLMManager::startLLMTask() {
    xTaskCreatePinnedToCore(
        llmTask, "LLMTask", 8192 * 2, this, 5, NULL, 0);
    Serial.println("LLM processing task started.");
}

// Generate response based on the current provider
String LLMManager::generateResponse(const String& prompt, LLMMode mode, const JsonArray& authorizedTools) {
    Serial.printf("Largest Free Heap Block before LLM call: %u bytes\n", heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    
    if (currentApiKey.length() == 0) {
        return "Error: API Key is not set for the current provider.";
    }

    String response;
    if (currentProvider == "google_gemini") {
        response = getGeminiResponse(prompt, mode, authorizedTools);
    } else if (currentProvider == "deepseek") {
        response = getDeepSeekResponse(prompt, mode, authorizedTools);
    } else if (currentProvider == "openrouter") {
        // OpenRouter uses a similar API structure to OpenAI/DeepSeek
        response = getChatGPTResponse(prompt, mode, authorizedTools); 
    }
    // Add other providers like ChatGPT here if needed
    else {
        response = "Error: Invalid LLM provider selected.";
    }

    Serial.printf("Largest Free Heap Block after LLM call: %u bytes\n", heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    return response;
}

// Get Gemini response
String LLMManager::getGeminiResponse(const String& prompt, LLMMode mode, const JsonArray& authorizedTools) {
    WiFiClientSecure client;
    client.setInsecure();
    if (!client.connect(GEMINI_API_HOST, 443)) {
        return "Error: Connection to Gemini API failed.";
    }
    
    String url = String("/v1beta/models/") + currentModel + ":generateContent?key=" + currentApiKey;
    JsonDocument doc;

    String systemPrompt = generateSystemPrompt(mode, authorizedTools);
    if (systemPrompt.length() > 0) {
        doc["contents"][0]["parts"][0]["text"] = systemPrompt;
        doc["contents"][1]["parts"][0]["text"] = prompt;
    } else {
        doc["contents"][0]["parts"][0]["text"] = prompt;
    }
    
    String requestBody;
    serializeJson(doc, requestBody);

    HttpClient http(client, GEMINI_API_HOST, 443);
    http.post(url, "application/json", requestBody);
    
    int statusCode = http.responseStatusCode();
    String response = http.responseBody();

    if (statusCode == 200) {
        JsonDocument responseDoc;
        deserializeJson(responseDoc, response);
        return responseDoc["candidates"][0]["content"]["parts"][0]["text"].as<String>();
    } else {
        return "Error: Gemini API request failed with status code " + String(statusCode) + " " + response;
    }
}

// Get DeepSeek response (can be a template for other OpenAI-like APIs)
String LLMManager::getDeepSeekResponse(const String& prompt, LLMMode mode, const JsonArray& authorizedTools) {
    WiFiClientSecure client;
    client.setInsecure();
    if (!client.connect(DEEPSEEK_API_HOST, 443)) {
        return "Error: Connection to DeepSeek API failed.";
    }

    JsonDocument doc;
    doc["model"] = currentModel;

    String systemPrompt = generateSystemPrompt(mode, authorizedTools);
    if (systemPrompt.length() > 0) {
        doc["messages"][0]["role"] = "system";
        doc["messages"][0]["content"] = systemPrompt;
        doc["messages"][1]["role"] = "user";
        doc["messages"][1]["content"] = prompt;
    } else {
        doc["messages"][0]["role"] = "user";
        doc["messages"][0]["content"] = prompt;
    }
    
    String requestBody;
    serializeJson(doc, requestBody);

    HttpClient http(client, DEEPSEEK_API_HOST, 443);
    http.beginRequest();
    http.post("/chat/completions");
    http.sendHeader("Authorization", "Bearer " + currentApiKey);
    http.sendHeader("Content-Type", "application/json");
    http.sendHeader("Content-Length", String(requestBody.length()));
    http.write((const uint8_t*)requestBody.c_str(), requestBody.length());
    http.endRequest();

    int statusCode = http.responseStatusCode();
    String response = http.responseBody();

    if (statusCode == 200) {
        JsonDocument responseDoc;
        deserializeJson(responseDoc, response);
        return responseDoc["choices"][0]["message"]["content"].as<String>();
    } else {
        return "Error: DeepSeek API request failed with status code " + String(statusCode) + " " + response;
    }
}

// Get ChatGPT/OpenRouter response
String LLMManager::getChatGPTResponse(const String& prompt, LLMMode mode, const JsonArray& authorizedTools) {
    WiFiClientSecure client;
    client.setInsecure();
    
    const char* apiHost = (currentProvider == "openrouter") ? OPENROUTER_API_HOST : "api.openai.com";

    if (!client.connect(apiHost, 443)) {
        return "Error: Connection to API failed.";
    }

    JsonDocument doc;
    doc["model"] = currentModel;

    String systemPrompt = generateSystemPrompt(mode, authorizedTools);
    if (systemPrompt.length() > 0) {
        doc["messages"][0]["role"] = "system";
        doc["messages"][0]["content"] = systemPrompt;
        doc["messages"][1]["role"] = "user";
        doc["messages"][1]["content"] = prompt;
    } else {
        doc["messages"][0]["role"] = "user";
        doc["messages"][0]["content"] = prompt;
    }
    
    String requestBody;
    serializeJson(doc, requestBody);

    HttpClient http(client, apiHost, 443);
    http.beginRequest();
    http.post("/api/v1/chat/completions");
    http.sendHeader("Authorization", "Bearer " + currentApiKey);
    http.sendHeader("Content-Type", "application/json");
    http.sendHeader("Content-Length", String(requestBody.length()));
    if (currentProvider == "openrouter") {
        // OpenRouter requires extra headers, e.g., HTTP-Referer
        http.sendHeader("HTTP-Referer", "http://localhost"); // Replace with your actual site URL
    }
    http.write((const uint8_t*)requestBody.c_str(), requestBody.length());
    http.endRequest();

    int statusCode = http.responseStatusCode();
    String response = http.responseBody();

    if (statusCode == 200) {
        JsonDocument responseDoc;
        deserializeJson(responseDoc, response);
        return responseDoc["choices"][0]["message"]["content"].as<String>();
    } else {
        return "Error: API request failed with status code " + String(statusCode) + " " + response;
    }
}


// Generate system prompt based on mode and tools
String LLMManager::generateSystemPrompt(LLMMode mode, const JsonArray& authorizedTools) {
    String prompt = "";
    if (mode == CHAT_MODE) {
        prompt += "You are a helpful and friendly AI assistant. Respond concisely and accurately to user queries.";
    } else if (mode == ADVANCED_MODE) {
        prompt += "You are an intelligent assistant capable of controlling a hardware device. ";
        prompt += "Your response MUST be a JSON object with a 'mode' field set to 'advanced' and an 'action' field. ";
        prompt += "The 'action' field should contain 'type' (always 'tool_call'), 'tool_name', and 'parameters'. ";
        prompt += "If you cannot fulfill the request with available tools, respond with a text message. ";
        prompt += "Here are the tools you are authorized to use:\n";
        prompt += getToolDescriptions(authorizedTools);
        prompt += "\nExample of a tool call:\n";
        prompt += "```json\n";
        prompt += "{\n";
        prompt += "  \"mode\": \"advanced\",\n";
        prompt += "  \"action\": {\n";
        prompt += "    \"type\": \"tool_call\",\n";
        prompt += "    \"tool_name\": \"usb_hid_keyboard_type\",\n";
        prompt += "    \"parameters\": {\n";
        prompt += "      \"text\": \"Hello World\"\n";
        prompt += "    }\n";
        prompt += "  }\n";
        prompt += "}\n";
        prompt += "```\n";
    }
    return prompt;
}

// Get tool descriptions
String LLMManager::getToolDescriptions(const JsonArray& authorizedTools) {
    String descriptions = "";
    descriptions += "- **usb_hid_keyboard_type**: Types a given string on the connected computer via USB HID. Parameters: `{\"text\": \"string_to_type\"}`\n";
    descriptions += "- **usb_hid_mouse_click**: Clicks the mouse at the current cursor position. Parameters: `{\"button\": \"left\"}` (or \"right\", \"middle\")\n";
    descriptions += "- **usb_hid_mouse_move**: Moves the mouse cursor by a specified offset. Parameters: `{\"x\": 10, \"y\": 20}`\n";
    descriptions += "- **gpio_set_level**: Sets the digital level of a specified GPIO pin. Parameters: `{\"pin\": 1, \"level\": 1}` (0 for LOW, 1 for HIGH)\n";

    if (descriptions.length() == 0) {
        descriptions = "No specific tools are available for use in this session.";
    }
    return descriptions;
}

// LLM task function
void LLMManager::llmTask(void* pvParameters) {
    LLMManager* llmManager = static_cast<LLMManager*>(pvParameters);
    LLMRequest request;
    LLMResponse response;

    for (;;) {
        if (xQueueReceive(llmManager->llmRequestQueue, &request, portMAX_DELAY) == pdPASS) {
            Serial.printf("LLMTask: Received request for prompt: %s\n", request.prompt.c_str());
            response.response = llmManager->generateResponse(request.prompt, request.mode, JsonArray());
            Serial.printf("LLMTask: Generated response: %s\n", response.response.c_str());

            if (xQueueSend(llmManager->llmResponseQueue, &response, portMAX_DELAY) != pdPASS) {
                Serial.println("LLMTask: Failed to send response to queue.");
            }
        }
    }
}
