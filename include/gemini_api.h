#ifndef GEMINI_API_H
#define GEMINI_API_H

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "hardware_config.h" // For tft object

// Gemini API Configuration
extern const char* GEMINI_API_KEY;
extern const char* GEMINI_API_URL;

String sendToGemini(String prompt);

#endif // GEMINI_API_H
