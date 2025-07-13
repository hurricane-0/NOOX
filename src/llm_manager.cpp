#include "llm_manager.h"
#include <HttpClient.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>

// Define API endpoints
const char* GEMINI_API_HOST = "generativelanguage.googleapis.com";
const int GEMINI_API_PORT = 443;
const char* GEMINI_API_PATH = "/v1beta/models/gemini-pro:generateContent?key=";

const char* DEEPSEEK_API_HOST = "api.deepseek.com";
const int DEEPSEEK_API_PORT = 443;
const char* DEEPSEEK_API_PATH = "/chat/completions";

const char* CHATGPT_API_HOST = "api.openai.com";
const int CHATGPT_API_PORT = 443;
const char* CHATGPT_API_PATH = "/v1/chat/completions";

// Constructor
LLMManager::LLMManager(SDManager& sd) : sdManager(sd), currentProvider(GEMINI) {}

// Begin method to load config
void LLMManager::begin() {
    JsonDocument doc = sdManager.loadConfig();
    if (!doc.isNull() && doc["llm_api_key"].is<String>()) {
        apiKey = doc["llm_api_key"].as<String>();
        Serial.println("LLM API Key loaded from SD card.");
    } else {
        Serial.println("LLM API Key not found in config.");
    }
}

void LLMManager::setProvider(LLMProvider provider) {
    currentProvider = provider;
}

void LLMManager::setApiKey(const String& key) {
    apiKey = key;
    
    // Save the new key to the config file
    JsonDocument doc = sdManager.loadConfig();
    JsonObject config = doc.as<JsonObject>();
    config["llm_api_key"] = apiKey;
    
    if (sdManager.saveConfig(doc)) {
        Serial.println("LLM API Key saved to SD card.");
    } else {
        Serial.println("Failed to save LLM API Key.");
    }
}

String LLMManager::generateResponse(const String& prompt, LLMMode mode, const JsonArray& authorizedTools) {
    if (apiKey.length() == 0) {
        return "Error: API Key is not set.";
    }
    switch (currentProvider) {
        case GEMINI:
            return getGeminiResponse(prompt, mode, authorizedTools);
        case DEEPSEEK:
            return getDeepSeekResponse(prompt, mode, authorizedTools);
        case CHATGPT:
            return getChatGPTResponse(prompt, mode, authorizedTools);
        default:
            return "Error: Invalid LLM provider.";
    }
}

String LLMManager::getGeminiResponse(const String& prompt, LLMMode mode, const JsonArray& authorizedTools) {
    WiFiClientSecure client;
    client.setInsecure();
    if (!client.connect(GEMINI_API_HOST, GEMINI_API_PORT)) {
        return "Error: Connection to Gemini API failed.";
    }
    
    String url = String(GEMINI_API_PATH) + apiKey;
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

    HttpClient http(client, GEMINI_API_HOST, GEMINI_API_PORT);
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

String LLMManager::getDeepSeekResponse(const String& prompt, LLMMode mode, const JsonArray& authorizedTools) {
    WiFiClientSecure client;
    client.setInsecure();
    if (!client.connect(DEEPSEEK_API_HOST, DEEPSEEK_API_PORT)) {
        return "Error: Connection to DeepSeek API failed.";
    }

    JsonDocument doc;
    doc["model"] = "deepseek-chat";

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

    HttpClient http(client, DEEPSEEK_API_HOST, DEEPSEEK_API_PORT);
    http.beginRequest();
    http.post(DEEPSEEK_API_PATH);
    http.sendHeader("Authorization", "Bearer " + apiKey);
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

String LLMManager::getChatGPTResponse(const String& prompt, LLMMode mode, const JsonArray& authorizedTools) {
    WiFiClientSecure client;
    client.setInsecure();
    if (!client.connect(CHATGPT_API_HOST, CHATGPT_API_PORT)) {
        return "Error: Connection to ChatGPT API failed.";
    }

    JsonDocument doc;
    doc["model"] = "gpt-3.5-turbo";

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

    HttpClient http(client, CHATGPT_API_HOST, CHATGPT_API_PORT);
    http.beginRequest();
    http.post(CHATGPT_API_PATH);
    http.sendHeader("Authorization", "Bearer " + apiKey);
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
        return "Error: ChatGPT API request failed with status code " + String(statusCode) + " " + response;
    }
}

String LLMManager::generateSystemPrompt(LLMMode mode, const JsonArray& authorizedTools) {
    String prompt = "";
    if (mode == ADVANCED_MODE) {
        prompt += "You are an intelligent assistant capable of controlling a hardware device. ";
        prompt += "In advanced mode, you can call specific tools to interact with the device's peripherals or execute automation scripts. ";
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
        prompt += "Example of running an automation script:\n";
        prompt += "```json\n";
        prompt += "{\n";
        prompt += "  \"mode\": \"advanced\",\n";
        prompt += "  \"action\": {\n";
        prompt += "    \"type\": \"tool_call\",\n";
        prompt += "    \"tool_name\": \"run_automation_script\",\n";
        prompt += "    \"parameters\": {\n";
        prompt += "      \"script_name\": \"configure_cpp_env\"\n";
        prompt += "    }\n";
        prompt += "  }\n";
        prompt += "}\n";
        prompt += "```\n";
    }
    return prompt;
}

String LLMManager::getToolDescriptions(const JsonArray& authorizedTools) {
    String descriptions = "";
    // Define descriptions for known tools. This should be kept in sync with TaskManager's capabilities.
    // For automation scripts, the 'tool_name' will be 'run_automation_script' and 'script_name' will be the actual script name.
    // The authorizedTools array will contain strings like "usb_hid_keyboard_type", "run_automation_script:configure_cpp_env"

    // Helper lambda to check if a tool is authorized
    auto isToolAuthorized = [&](const String& toolName) {
        if (authorizedTools.isNull() || authorizedTools.size() == 0) {
            return true; // If no specific tools are authorized, assume all are available for description
        }
        for (JsonVariant v : authorizedTools) {
            if (v.as<String>() == toolName) {
                return true;
            }
        }
        return false;
    };

    // Example tool descriptions (these should be comprehensive and accurate for the LLM)
    // HID Tools
    if (isToolAuthorized("usb_hid_keyboard_type")) {
        descriptions += "- **usb_hid_keyboard_type**: Types a given string on the connected computer via USB HID. Parameters: `{\"text\": \"string_to_type\"}`\n";
    }
    if (isToolAuthorized("usb_hid_mouse_click")) {
        descriptions += "- **usb_hid_mouse_click**: Clicks the mouse at the current cursor position. Parameters: `{\"button\": \"left\"}` (or \"right\", \"middle\")\n";
    }
    if (isToolAuthorized("usb_hid_mouse_move")) {
        descriptions += "- **usb_hid_mouse_move**: Moves the mouse cursor by a specified offset. Parameters: `{\"x\": 10, \"y\": 20}`\n";
    }
    // WiFi Killer (example, actual implementation might vary)
    if (isToolAuthorized("wifi_killer_scan")) {
        descriptions += "- **wifi_killer_scan**: Scans for nearby Wi-Fi networks. Parameters: `{}`\n";
    }
    // Timer (example)
    if (isToolAuthorized("timer_set_countdown")) {
        descriptions += "- **timer_set_countdown**: Sets a countdown timer. Parameters: `{\"duration_ms\": 5000}`\n";
    }
    // GPIO (example)
    if (isToolAuthorized("gpio_set_level")) {
        descriptions += "- **gpio_set_level**: Sets the digital level of a specified GPIO pin. Parameters: `{\"pin\": 1, \"level\": 1}` (0 for LOW, 1 for HIGH)\n";
    }
    // BLE (example)
    if (isToolAuthorized("ble_scan_devices")) {
        descriptions += "- **ble_scan_devices**: Scans for nearby Bluetooth Low Energy devices. Parameters: `{}`\n";
    }

    // Automation Scripts
    // The authorizedTools array will contain entries like "run_automation_script:script_name"
    // We need to iterate through authorizedTools to find automation scripts
    for (JsonVariant v : authorizedTools) {
        String tool = v.as<String>();
        if (tool.startsWith("run_automation_script:")) {
            String scriptName = tool.substring(String("run_automation_script:").length());
            descriptions += "- **run_automation_script** (Script: " + scriptName + "): Executes the custom automation script '" + scriptName + "'. Parameters: `{\"script_name\": \"" + scriptName + "\", \"args\": {}}` (args are optional, depending on script)\n";
        }
    }

    if (descriptions.length() == 0) {
        descriptions = "No specific tools are authorized for use in this session, or no descriptions are available for the authorized tools.";
    }

    return descriptions;
}
