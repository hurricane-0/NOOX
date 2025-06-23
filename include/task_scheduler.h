#ifndef TASK_SCHEDULER_H
#define TASK_SCHEDULER_H

#include <Arduino.h>
#include <vector>
#include <ArduinoJson.h> // For parsing task JSON
#include "hardware_config.h" // For Keyboard and tft objects
#include "ui_manager.h"      // For OperatingMode and setOperatingMode
#include <WebSocketsServer.h> // For webSocket.broadcastTXT

// Structure to hold a single task action
struct TaskAction {
  String type;
  String command;
  String value; // For type command
  std::vector<String> keys; // For press_release command
  long duration_ms; // For system wait command
  String risk_level; // low, medium, high
  String description; // For authorization prompt
};

// Task Management Variables
extern std::vector<TaskAction> taskQueue;
extern bool taskInProgress;
extern unsigned long lastActionExecutionTime;
extern const unsigned long ACTION_DELAY_MS;

// Authorization Variables
extern bool authorizationPending;
extern String pendingAuthDescription;
extern String pendingAuthCommand;

// Extern declaration for webSocket from main.cpp
extern WebSocketsServer webSocket;

void executeTaskAction(const TaskAction& action);
void processTaskQueue();

#endif // TASK_SCHEDULER_H
