#include "wifi_manager.h"
#include "ui_manager.h" // For displaying status on LCD
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h> // For parsing JSON responses if needed

// WiFi network list
std::vector<String> availableNetworks;

void wifi_manager_init() {
  Serial.println("Initializing WiFi Manager...");
  ui_manager_set_status("Initializing WiFi...");
  WiFi.mode(WIFI_STA); // Set WiFi to station mode
  Serial.println("WiFi Manager Initialized.");
  ui_manager_set_status("WiFi Manager OK.");
}

void wifi_manager_scan_networks() {
  Serial.println("Starting WiFi scan...");
  ui_manager_set_status("Scanning WiFi...");
  availableNetworks.clear();
  int n = WiFi.scanNetworks();
  Serial.println("WiFi scan finished.");
  if (n == 0) {
    Serial.println("No networks found.");
    ui_manager_set_status("No WiFi Networks.");
  } else {
    Serial.printf("%d networks found:\n", n);
    ui_manager_set_status(String(n) + " WiFi Networks.");
    for (int i = 0; i < n; ++i) {
      String ssid = WiFi.SSID(i);
      availableNetworks.push_back(ssid);
      Serial.printf("  %d: %s (%d)\n", i + 1, ssid.c_str(), WiFi.RSSI(i));
    }
  }
}

bool wifi_manager_connect_to_network(const String& ssid, const String& password) {
  Serial.printf("Connecting to WiFi: %s\n", ssid.c_str());
  ui_manager_set_status("Connecting to " + ssid + "...");
  WiFi.begin(ssid.c_str(), password.c_str());

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) { // Try to connect 20 seconds
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    ui_manager_set_status("WiFi Connected: " + WiFi.localIP().toString());
    return true;
  } else {
    Serial.println("\nWiFi Connection Failed!");
    ui_manager_set_status("WiFi Connect Failed!");
    return false;
  }
}

void wifi_manager_disconnect() {
  WiFi.disconnect();
  Serial.println("WiFi Disconnected.");
  ui_manager_set_status("WiFi Disconnected.");
}

// Example HTTP GET request
String wifi_manager_http_get(const String& url) {
  HTTPClient http;
  String payload = "";

  Serial.printf("HTTP GET: %s\n", url.c_str());
  ui_manager_set_status("HTTP GET...");

  http.begin(url);
  int httpCode = http.GET();

  if (httpCode > 0) {
    Serial.printf("[HTTP] GET... code: %d\n", httpCode);
    if (httpCode == HTTP_CODE_OK) {
      payload = http.getString();
      Serial.println(payload);
      ui_manager_set_status("HTTP GET OK.");
    }
  } else {
    Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    ui_manager_set_status("HTTP GET Failed.");
  }
  http.end();
  return payload;
}

// Example HTTP POST request
String wifi_manager_http_post(const String& url, const String& contentType, const String& postData) {
  HTTPClient http;
  String payload = "";

  Serial.printf("HTTP POST: %s\n", url.c_str());
  ui_manager_set_status("HTTP POST...");

  http.begin(url);
  http.addHeader("Content-Type", contentType);
  int httpCode = http.POST(postData);

  if (httpCode > 0) {
    Serial.printf("[HTTP] POST... code: %d\n", httpCode);
    if (httpCode == HTTP_CODE_OK) {
      payload = http.getString();
      Serial.println(payload);
      ui_manager_set_status("HTTP POST OK.");
    }
  } else {
    Serial.printf("[HTTP] POST... failed, error: %s\n", http.errorToString(httpCode).c_str());
    ui_manager_set_status("HTTP POST Failed.");
  }
  http.end();
  return payload;
}

// Placeholder for MQTT client (requires a separate MQTT library)
void wifi_manager_mqtt_publish(const String& topic, const String& message) {
  Serial.printf("MQTT Publish (TODO): Topic='%s', Message='%s'\n", topic.c_str(), message.c_str());
  ui_manager_set_status("MQTT Publish (TODO).");
}

void wifi_manager_mqtt_subscribe(const String& topic) {
  Serial.printf("MQTT Subscribe (TODO): Topic='%s'\n", topic.c_str());
  ui_manager_set_status("MQTT Subscribe (TODO).");
}
