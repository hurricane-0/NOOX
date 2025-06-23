#ifndef UI_MANAGER_H
#define UI_MANAGER_H

#include <Arduino.h>
#include <WebSocketsServer.h> // For webSocket.broadcastTXT
#include "hardware_config.h"  // For tft object

// Operating Modes
enum OperatingMode {
  CHAT_MODE,
  ADVANCED_MODE
};

extern OperatingMode currentMode; // Default mode

// Extern declaration for webSocket from main.cpp
extern WebSocketsServer webSocket;

void ui_manager_init();
void ui_manager_clear_screen();
void ui_manager_print_message(const String& message);
void ui_manager_set_status(const String& status);
void ui_manager_loop(); // For any UI-related periodic updates

void setOperatingMode(OperatingMode mode);

#endif // UI_MANAGER_H
