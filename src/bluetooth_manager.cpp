#include "bluetooth_manager.h"
#include "ui_manager.h" // For displaying status on LCD

// BLE Server and Client objects
BLEServer* pServer = nullptr;
BLEAdvertising* pAdvertising = nullptr;
BLECharacteristic* pCharacteristic = nullptr;
bool deviceConnected = false;

// BLE Client specific
static BLEAdvertisedDevice* myAdvertisedDevice;
static bool doConnect = false;
static BLEUUID serviceUUID("4fafc201-1fb5-459e-8fcc-c2c6c4791911"); // Example Service UUID
static BLEUUID charUUID("beb5483e-36e1-4688-b7f5-ea07361b26a8");   // Example Characteristic UUID

// Callbacks for BLE Server
class MyServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      Serial.println("BLE Client Connected!");
      ui_manager_set_status("BLE Client Connected!");
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      Serial.println("BLE Client Disconnected!");
      ui_manager_set_status("BLE Client Disconnected!");
      // Restart advertising to allow new connections
      pAdvertising->start();
    }
};

// Callbacks for BLE Client
class MyClientCallbacks : public BLEClientCallbacks {
  void onConnect(BLEClient* pClient) {
    Serial.println("BLE Server Connected!");
    ui_manager_set_status("BLE Server Connected!");
  }

  void onDisconnect(BLEClient* pClient) {
    Serial.println("BLE Server Disconnected!");
    ui_manager_set_status("BLE Server Disconnected!");
    doConnect = false; // Allow reconnection attempts
  }
};

// Callback for BLE Scan
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    Serial.printf("Advertised Device found: %s \n", advertisedDevice.toString().c_str());
    // We have found a device, now see if it contains the service we are looking for.
    if (advertisedDevice.haveServiceUUID() && advertisedDevice.getServiceUUID().equals(serviceUUID)) {
      // Found our device, now stop the scan and save the device for connection
      BLEDevice::getScan()->stop();
      myAdvertisedDevice = new BLEAdvertisedDevice(advertisedDevice);
      doConnect = true; // Set flag to connect in main loop
    }
  }
};

void bluetooth_init() {
  Serial.println("Initializing BLE...");
  ui_manager_set_status("Initializing BLE...");
  BLEDevice::init("ESP32-S3-Gemini"); // Give your device a name
  Serial.println("BLE Initialized.");
  ui_manager_set_status("BLE Initialized.");
}

void bluetooth_scan_devices() {
  Serial.println("Starting BLE scan...");
  ui_manager_set_status("Scanning BLE...");
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true); // Active scan uses more power, but gets more results
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);  // Less or equal than scan interval
  pBLEScan->start(5, false); // Scan for 5 seconds, don't stop after first device
  Serial.println("BLE scan finished.");
  ui_manager_set_status("BLE Scan Finished.");
}

void bluetooth_start_gatt_server() {
  Serial.println("Starting BLE GATT Server...");
  ui_manager_set_status("Starting BLE Server...");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(serviceUUID);
  pCharacteristic = pService->createCharacteristic(
                                         charUUID,
                                         BLECharacteristic::PROPERTY_READ |
                                         BLECharacteristic::PROPERTY_WRITE |
                                         BLECharacteristic::PROPERTY_NOTIFY
                                       );
  pCharacteristic->addDescriptor(new BLE2902());
  pService->start();

  pAdvertising = pServer->getAdvertising();
  pAdvertising->addServiceUUID(serviceUUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);  // functions that help with iPhone connections issue
  pAdvertising->setMinPreferred(0x12);
  pAdvertising->start();
  Serial.println("BLE GATT Server Started. Advertising...");
  ui_manager_set_status("BLE Server Advertising.");
}

void bluetooth_start_gatt_client(const std::string& deviceAddress) {
  Serial.printf("Attempting to connect to BLE device: %s\n", deviceAddress.c_str());
  ui_manager_set_status("Connecting BLE Client...");

  BLEClient* pClient = BLEDevice::createClient();
  pClient->setClientCallbacks(new MyClientCallbacks());

  // Convert string address to BLEAddress
  BLEAddress bleAddress(deviceAddress);

  if (pClient->connect(bleAddress)) {
    Serial.println("Successfully connected to BLE server.");
    ui_manager_set_status("BLE Client Connected!");

    BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
    if (pRemoteService == nullptr) {
      Serial.print("Failed to find service UUID: ");
      Serial.println(serviceUUID.toString().c_str());
      pClient->disconnect();
      return;
    }
    Serial.println("Found service.");

    BLERemoteCharacteristic* pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);
    if (pRemoteCharacteristic == nullptr) {
      Serial.print("Failed to find characteristic UUID: ");
      Serial.println(charUUID.toString().c_str());
      pClient->disconnect();
      return;
    }
    Serial.println("Found characteristic.");

    // Read the characteristic value
    if (pRemoteCharacteristic->canRead()) {
      std::string value = pRemoteCharacteristic->readValue();
      Serial.printf("Characteristic value: %s\n", value.c_str());
      ui_manager_print_message("BLE Data: " + String(value.c_str()));
    }

    // Register for notifications
    if (pRemoteCharacteristic->canNotify()) {
      pRemoteCharacteristic->registerForNotify([](BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
        Serial.printf("Notify callback for characteristic %s of service %s, data: %.*s\n",
                      pBLERemoteCharacteristic->getUUID().toString().c_str(),
                      pBLERemoteCharacteristic->getRemoteService()->getUUID().toString().c_str(), // Corrected
                      length, pData);
        ui_manager_print_message("BLE Notify: " + String((char*)pData, length));
      });
    }

  } else {
    Serial.println("Failed to connect to BLE server.");
    ui_manager_set_status("BLE Client Connect Failed.");
  }
}

void bluetooth_send_data(const std::string& serviceUUID_str, const std::string& charUUID_str, const std::string& data) {
  Serial.printf("Attempting to send data via BLE. Service: %s, Char: %s, Data: %s\n",
                serviceUUID_str.c_str(), charUUID_str.c_str(), data.c_str());
  ui_manager_set_status("Sending BLE Data...");

  // This function assumes a client connection is already established or will be established.
  // For simplicity, we'll try to find the client and characteristic.
  // In a real application, you'd manage active connections.
  BLEClient* pClient = BLEDevice::createClient(); // This creates a new client, not ideal for existing connection
  BLEAddress bleAddress("XX:XX:XX:XX:XX:XX"); // Placeholder, needs actual connected device address

  // This part needs to be refined to use an *existing* connected client
  // For now, it's a simplified example.
  if (pClient->connect(bleAddress)) { // This will try to connect again
    BLERemoteService* pRemoteService = pClient->getService(BLEUUID(serviceUUID_str));
    if (pRemoteService) {
      BLERemoteCharacteristic* pRemoteCharacteristic = pRemoteService->getCharacteristic(BLEUUID(charUUID_str));
      if (pRemoteCharacteristic && pRemoteCharacteristic->canWrite()) {
        pRemoteCharacteristic->writeValue(data);
        Serial.println("BLE data sent successfully.");
        ui_manager_set_status("BLE Data Sent.");
      } else {
        Serial.println("Failed to find characteristic or cannot write.");
        ui_manager_set_status("BLE Send Failed: Char.");
      }
    } else {
      Serial.println("Failed to find service.");
      ui_manager_set_status("BLE Send Failed: Service.");
    }
    pClient->disconnect();
  } else {
    Serial.println("Failed to connect for sending data.");
    ui_manager_set_status("BLE Send Failed: Connect.");
  }
}

void bluetooth_set_hid_keyboard_mode() {
  Serial.println("Setting BLE HID Keyboard Mode...");
  ui_manager_set_status("BLE HID Mode...");
  // This would typically involve setting up a BLE HID service.
  // This is a more complex topic and requires a dedicated BLE HID library/implementation.
  // For now, this is a placeholder.
  Serial.println("BLE HID Keyboard Mode placeholder.");
  ui_manager_set_status("BLE HID Mode (TODO).");
}
