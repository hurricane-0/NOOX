#include "gemini_api.h"
#include "hardware_config.h" // For tft object

// Gemini API Configuration
const char* GEMINI_API_KEY = "YOUR_GEMINI_API_KEY"; // Replace with your Gemini API Key
const char* GEMINI_API_URL = "https://generativelanguage.googleapis.com/v1beta/models/gemini-pro:generateContent?key=";

String sendToGemini(String prompt) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected for Gemini API.");
    return "";
  }

  HTTPClient http;
  String url = String(GEMINI_API_URL) + GEMINI_API_KEY;
  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  String requestBody = "{\"contents\": [{\"parts\": [{\"text\": \"" + prompt + "\"}]}]}";
  Serial.print("Sending to Gemini: ");
  Serial.println(requestBody);

  int httpResponseCode = http.POST(requestBody);

  String response = "";
  if (httpResponseCode > 0) {
    Serial.printf("[HTTP] POST... code: %d\n", httpResponseCode);
    response = http.getString();
    Serial.println(response);

    // Simple JSON parsing example (actual needs more robust parsing)
    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, response);

    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
      return "Error parsing Gemini response.";
    }

    // Assuming response structure is { "candidates": [ { "content": { "parts": [ { "text": "..." } ] } } ] }
    if (doc.containsKey("candidates") && doc["candidates"][0].containsKey("content") && doc["candidates"][0]["content"].containsKey("parts")) {
      return doc["candidates"][0]["content"]["parts"][0]["text"].as<String>();
    } else {
      return "No text content found in Gemini response.";
    }

  } else {
    Serial.printf("[HTTP] POST... failed, error: %s\n", http.errorToString(httpResponseCode).c_str());
    response = "HTTP Error: " + String(httpResponseCode);
  }

  http.end();
  return response;
}
