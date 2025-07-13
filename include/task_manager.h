#ifndef TASK_MANAGER_H
#define TASK_MANAGER_H

#include <Arduino.h>
#include "hid_manager.h" // Include HIDManager header
#include "hardware_manager.h" // Include HardwareManager header for GPIO tasks
#include "sd_manager.h" // Include SDManager header for script loading
#include <ArduinoJson.h> // Include ArduinoJson for JsonObject

// Forward declarations to break circular dependency
class AppWiFiManager;
class HardwareManager; // Forward declaration for HardwareManager
class TimerManager; // Forward declaration for TimerManager
class BLEManager; // Forward declaration for BLEManager
class SDManager; // Forward declaration for SDManager

class TaskManager {
public:
    TaskManager(HIDManager& hid, AppWiFiManager& wifi, HardwareManager& hw, TimerManager& timer, BLEManager& ble, SDManager& sd); // Modified constructor to include SDManager
    String executeTool(const String& toolName, const JsonObject& params); // New method for LLM tool calls
    void executeTask(const String& taskName, const String& params); // Existing method, keep for compatibility if needed
    bool isTimerRunning(); // Add method to check timer status
    // Add more task management functionalities here

private:
    HIDManager& hidManager;     // Reference to HIDManager
    AppWiFiManager& wifiManager; // Reference to AppWiFiManager
    HardwareManager& hardwareManager; // Reference to HardwareManager
    TimerManager& timerManager; // Reference to TimerManager
    BLEManager& bleManager; // Reference to BLEManager
    SDManager& sdManager; // Reference to SDManager for script loading

    void handleHIDTask(const String& params);
    void handleWiFiTask(const String& params); // Placeholder for general WiFi tasks
    void handleBluetoothTask(const String& params);
    void handleTimerTask(const String& params);
    void handleGpioTask(const String& params);
    void handleWifiKillerTask(const String& params); // Specific for Wi-Fi Killer mode

    // Static instance and callback for TimerManager integration
    static TaskManager* _instance;
    static void IRAM_ATTR onTimerTaskCallback();
    void _handleTimerCallback(); // Non-static helper for the callback
};

#endif // TASK_MANAGER_H
