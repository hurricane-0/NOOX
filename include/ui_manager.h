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

void setOperatingMode(OperatingMode mode);

#endif // UI_MANAGER_H
