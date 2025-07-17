#ifndef SD_MANAGER_H
#define SD_MANAGER_H

#include <SD.h>
#include "hardware_config.h"
#include <vector>
#include <Arduino.h>
#include <ArduinoJson.h>

class SDManager {
public:
    SDManager();
    bool begin();
    String readFile(const String& path);
    bool writeFile(const String& path, const String& content);
    bool deleteFile(const String& path);
    
    // New methods for specific configurations
    JsonDocument loadWiFiConfig();
    bool saveWiFiConfig(const JsonDocument& doc);

    JsonDocument loadAPIKeys();
    bool saveAPIKeys(const JsonDocument& doc);

    /**
     * @brief Loads the list of automation script names from /automation_scripts.json.
     *        This file now contains a JSON array of strings (script names).
     * @return JsonDocument containing the array of script names.
     */
    JsonDocument loadAutomationScripts(); 
    
    /**
     * @brief Saves the list of automation script names to /automation_scripts.json.
     *        The input JsonDocument should contain a JSON array of strings.
     * @param doc JsonDocument containing the array of script names.
     * @return True if successful, false otherwise.
     */
    bool saveAutomationScripts(const JsonDocument& doc); 
    
    /**
     * @brief Lists automation script names from the main automation_scripts.json file.
     * @return std::vector<String> containing the names of automation scripts.
     */
    std::vector<String> listAutomationScriptNames(); 

    // New methods for individual automation scripts
    /**
     * @brief Loads a specific automation script by its name from /automation_scripts/[scriptName].json.
     * @param scriptName The name of the script to load.
     * @return JsonDocument containing the content of the automation script.
     */
    JsonDocument loadAutomationScript(const String& scriptName);
    
    /**
     * @brief Saves a specific automation script to /automation_scripts/[scriptName].json.
     *        Also ensures the script name is added to the main automation_scripts.json list.
     * @param scriptName The name of the script to save.
     * @param doc JsonDocument containing the content of the automation script.
     * @return True if successful, false otherwise.
     */
    bool saveAutomationScript(const String& scriptName, const JsonDocument& doc);
    
    /**
     * @brief Deletes a specific automation script file from /automation_scripts/[scriptName].json.
     *        Also removes the script name from the main automation_scripts.json list.
     * @param scriptName The name of the script to delete.
     * @return True if successful, false otherwise.
     */
    bool deleteAutomationScript(const String& scriptName);

};

#endif // SD_MANAGER_H
