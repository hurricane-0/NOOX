#include "ui_manager.h"
#include "hardware_config.h" // For tft object

OperatingMode currentMode = CHAT_MODE; // Default mode

void ui_manager_init() {
  // Initialize TFT display settings (e.g., text size, color)
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(0, 0);
}

void ui_manager_clear_screen() {
  tft.fillScreen(ST77XX_BLACK);
}

void ui_manager_print_message(const String& message) {
  ui_manager_clear_screen();
  tft.setCursor(0, 0);
  tft.println(message);
}

void ui_manager_set_status(const String& status) {
  // This function can be used to display a small status message at a fixed location
  // For now, let's just print to serial and clear a specific area on screen
  Serial.print("UI Status: ");
  Serial.println(status);
  // Example: Clear a status line and print new status
  tft.fillRect(0, tft.height() - 20, tft.width(), 20, ST77XX_BLACK); // Clear bottom line
  tft.setCursor(0, tft.height() - 20);
  tft.setTextColor(ST77XX_YELLOW);
  tft.println(status);
  tft.setTextColor(ST77XX_WHITE); // Reset to default
}

void ui_manager_loop() {
  // Placeholder for any periodic UI updates if needed
}

void setOperatingMode(OperatingMode mode) {
  currentMode = mode;
  ui_manager_clear_screen();
  tft.setCursor(0, 0);
  tft.setTextSize(2); // Ensure text size is consistent for mode display
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
