#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <vector> // For std::vector<String>

extern std::vector<String> availableNetworks; // Declare availableNetworks as extern

void wifi_manager_init();
void wifi_manager_scan_networks();
bool wifi_manager_connect_to_network(const String& ssid, const String& password);
void wifi_manager_disconnect();
String wifi_manager_http_get(const String& url);
String wifi_manager_http_post(const String& url, const String& contentType, const String& postData);
void wifi_manager_mqtt_publish(const String& topic, const String& message);
void wifi_manager_mqtt_subscribe(const String& topic);

#endif // WIFI_MANAGER_H
