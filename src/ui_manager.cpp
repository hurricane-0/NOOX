#include "ui_manager.h"
#include "hardware_config.h" // For tft object

OperatingMode currentMode = CHAT_MODE; // Default mode

void setOperatingMode(OperatingMode mode) {
  currentMode = mode;
  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(0, 0);
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE);
  if (currentMode == CHAT_MODE) {
    tft.println("Mode: Chat");
    Serial.println("Switched to Chat Mode.");
  } else {
    tft.println("Mode: Advanced");
    Serial.println("Switched to Advanced Mode.");
  }
  webSocket.broadcastTXT("System:Switched to " + String(currentMode == CHAT_MODE ? "Chat" : "Advanced") + " Mode.");
}
