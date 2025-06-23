#include "task_scheduler.h"
#include "hardware_config.h" // For Keyboard and tft objects
#include "ui_manager.h"      // For OperatingMode and setOperatingMode

// Task Management Variables
std::vector<TaskAction> taskQueue;
bool taskInProgress = false;
unsigned long lastActionExecutionTime = 0;
const unsigned long ACTION_DELAY_MS = 100; // Delay between actions to ensure stability

// Authorization Variables
bool authorizationPending = false;
String pendingAuthDescription = "";
String pendingAuthCommand = ""; // Store the command that needs authorization

// Extern declaration for webSocket from main.cpp
extern WebSocketsServer webSocket;

void executeTaskAction(const TaskAction& action) {
  Serial.print("Executing action: Type="); Serial.print(action.type);
  Serial.print(", Command="); Serial.println(action.command);

  // Check for authorization if risk level is medium or high
  if ((action.risk_level.equals("medium") || action.risk_level.equals("high")) && !authorizationPending) {
    authorizationPending = true;
    pendingAuthDescription = action.description;
    pendingAuthCommand = action.command; // Store command for re-execution if authorized
    Serial.print("Authorization required for: ");
    Serial.println(action.description);
    tft.fillScreen(ST77XX_BLACK);
    tft.setCursor(0, 0);
    tft.setTextColor(ST77XX_RED);
    tft.println("AUTH REQUIRED!");
    tft.println(action.description);
    webSocket.broadcastTXT("AuthRequest:" + action.description);
    return; // Stop execution until authorized
  }

  if (action.type.equals("keyboard")) {
    if (action.command.equals("type")) {
      Keyboard.print(action.value);
      Serial.print("Typed: "); Serial.println(action.value);
      webSocket.broadcastTXT("System:Typed '" + action.value + "'");
    } else if (action.command.equals("press_release")) {
      for (const String& key : action.keys) {
        if (key.equals("KEY_LEFT_GUI")) Keyboard.press(KEY_LEFT_GUI);
        else if (key.equals("KEY_LEFT_ALT")) Keyboard.press(KEY_LEFT_ALT);
        else if (key.equals("KEY_LEFT_CTRL")) Keyboard.press(KEY_LEFT_CTRL);
        else if (key.equals("KEY_LEFT_SHIFT")) Keyboard.press(KEY_LEFT_SHIFT);
        else if (key.equals("KEY_RETURN")) Keyboard.press(KEY_RETURN);
        else if (key.length() == 1) Keyboard.press(key.charAt(0)); // Single character keys
        // For other special keys (e.g., F1-F12, PrintScreen, Esc, Tab, Delete, Backspace, CapsLock),
        // their definitions might vary across USB HID libraries.
        // If needed, these would require looking up the specific key codes for the USBHIDKeyboard library
        // or using a different HID library that provides them.
        // For now, we only support common modifiers, Enter, and single characters.
      }
      Keyboard.releaseAll();
      String keysStr = "";
      for (const String& key : action.keys) {
        keysStr += key + " ";
      }
      Serial.print("Pressed/Released: "); Serial.println(keysStr);
      webSocket.broadcastTXT("System:Pressed/Released keys: " + keysStr);
    }
  } else if (action.type.equals("system")) {
    if (action.command.equals("wait")) {
      Serial.print("Waiting for "); Serial.print(action.duration_ms); Serial.println(" ms.");
      webSocket.broadcastTXT("System:Waiting for " + String(action.duration_ms) + "ms");
      delay(action.duration_ms); // Blocking delay for now, consider non-blocking for future
    }
  }
  // Add more action types (e.g., mouse, wifi, bluetooth) here
}

void processTaskQueue() {
  if (taskInProgress && !taskQueue.empty() && !authorizationPending) {
    if (millis() - lastActionExecutionTime > ACTION_DELAY_MS) {
      TaskAction currentAction = taskQueue.front();
      taskQueue.erase(taskQueue.begin()); // Remove the action from the queue

      executeTaskAction(currentAction);
      lastActionExecutionTime = millis(); // Update last execution time
      
      if (taskQueue.empty()) {
        taskInProgress = false;
        Serial.println("Task completed.");
        tft.fillScreen(ST77XX_BLACK);
        tft.setCursor(0, 0);
        tft.setTextColor(ST77XX_GREEN);
        tft.println("Task Complete!");
        webSocket.broadcastTXT("System:Task completed.");
      } else {
        webSocket.broadcastTXT("TaskStatus:Executing action " + String(taskQueue.size()) + " of " + String(taskQueue.size() + 1) + ": " + currentAction.description);
      }
    }
  } else if (taskInProgress && taskQueue.empty() && !authorizationPending) {
    // This case handles if the last action was an authorization request and it was confirmed.
    // The task queue would be empty, but taskInProgress would still be true.
    // We need to ensure the taskInProgress flag is reset.
    taskInProgress = false;
    Serial.println("Task completed (after authorization).");
    tft.fillScreen(ST77XX_BLACK);
    tft.setCursor(0, 0);
    tft.setTextColor(ST77XX_GREEN);
    tft.println("Task Complete!");
    webSocket.broadcastTXT("System:Task completed.");
  }
}
