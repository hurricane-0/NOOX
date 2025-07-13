#include "ble_manager.h"
#include <Arduino.h>

// Global pointer to BLEManager instance for static callback
static BLEManager* pThisBLEManager = nullptr;

// Optional: Client callbacks for connection events
class MyClientCallbacks : public BLEClientCallbacks {
private:
    BLEManager* _bleManagerInstance; // Pointer to the BLEManager instance

public:
    MyClientCallbacks(BLEManager* instance) : _bleManagerInstance(instance) {}

    void onConnect(BLEClient* pclient) {
        Serial.println("BLE Client connected.");
        if (_bleManagerInstance != nullptr) {
            _bleManagerInstance->connected = true; // Update connection status
        }
    }

    void onDisconnect(BLEClient* pclient) {
        Serial.println("BLE Client disconnected.");
        if (_bleManagerInstance != nullptr) {
            _bleManagerInstance->connected = false; // Update connection status
            // Clean up resources if needed
            if (_bleManagerInstance->pClient) { // Check if pClient is not null before deleting
                delete _bleManagerInstance->pClient;
                _bleManagerInstance->pClient = nullptr;
                _bleManagerInstance->pRemoteService = nullptr;
                _bleManagerInstance->pRxCharacteristic = nullptr;
                _bleManagerInstance->pTxCharacteristic = nullptr;
            }
        }
    }
};

BLEManager::BLEManager() : pBLEScan(nullptr), pClient(nullptr), pRemoteService(nullptr), pRxCharacteristic(nullptr), pTxCharacteristic(nullptr) {
    pThisBLEManager = this; // Set global pointer
}

void BLEManager::begin() {
    Serial.println("Initializing BLE...");
    BLEDevice::init(""); // Initialize BLE
    pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(this);
    pBLEScan->setActiveScan(true); // Active scan will yield more info
    pBLEScan->setInterval(100);
    pBLEScan->setWindow(99);  // Less or equal than scanInterval
}

void BLEManager::startScan(int scanTimeSeconds) {
    Serial.println("Starting BLE scan...");
    discoveredDevices.clear(); // Clear previous scan results
    pBLEScan->start(scanTimeSeconds, false); // Non-blocking scan
    // The scan results will be collected in onResult callback
}

void BLEManager::onResult(BLEAdvertisedDevice advertisedDevice) {
    // Store all discovered devices
    discoveredDevices.push_back(advertisedDevice);
    Serial.print("Advertised Device found: ");
    Serial.println(advertisedDevice.toString().c_str());
}

void BLEManager::connectToServer(BLEAdvertisedDevice* pDevice) {
    if (connected) {
        Serial.println("Already connected to a BLE server.");
        return;
    }
    Serial.print("Forming a connection to ");
    Serial.println(pDevice->getAddress().toString().c_str());

    pClient = BLEDevice::createClient();
    Serial.println(" - Created client");

    pClient->setClientCallbacks(new MyClientCallbacks(this)); // Pass 'this' to the callback

    // Connect to the remote BLE Server.
    if (!pClient->connect(pDevice)) {
        Serial.println(" - Failed to connect to server.");
        delete pClient;
        pClient = nullptr;
        return;
    }
    Serial.println(" - Connected to server");
    pClient->setMTU(500); //set client to request maximum MTU from server (default is 23)

    // Obtain a reference to the service we are after in the remote BLE server.
    pRemoteService = pClient->getService(SERVICE_UUID);
    if (pRemoteService == nullptr) {
        Serial.print("Failed to find our service UUID: ");
        Serial.println(SERVICE_UUID);
        pClient->disconnect();
        delete pClient;
        pClient = nullptr;
        return;
    }
    Serial.println(" - Found our service");

    // Obtain a reference to the characteristics in the service a pointer to the characteristic.
    pRxCharacteristic = pRemoteService->getCharacteristic(CHARACTERISTIC_UUID_RX);
    pTxCharacteristic = pRemoteService->getCharacteristic(CHARACTERISTIC_UUID_TX);

    if (pRxCharacteristic == nullptr || pTxCharacteristic == nullptr) {
        Serial.print("Failed to find our characteristics UUIDs");
        pClient->disconnect();
        delete pClient;
        pClient = nullptr;
        return;
    }
    Serial.println(" - Found our characteristics");

    // Register for notifications on the TX characteristic
    if (pTxCharacteristic->canNotify()) {
        pTxCharacteristic->registerForNotify(notifyCallback);
        Serial.println(" - Registered for notifications on TX characteristic");
    } else {
        Serial.println(" - TX characteristic does not support notifications.");
    }

    connected = true;
    Serial.println("BLE connection established.");
}

void BLEManager::connectToAddress(const String& macAddress) {
    if (connected) {
        Serial.println("Already connected. Disconnect first.");
        return;
    }

    BLEAddress targetAddress(macAddress.c_str());
    BLEAdvertisedDevice* deviceToConnect = nullptr;

    // Try to find in already discovered devices
    for (auto& device : discoveredDevices) {
        if (device.getAddress().equals(targetAddress)) {
            deviceToConnect = new BLEAdvertisedDevice(device); // Create a copy
            break;
        }
    }

    if (deviceToConnect) {
        connectToServer(deviceToConnect);
        delete deviceToConnect; // Clean up the copied device
    } else {
        Serial.println("Device not found in discovered list. Initiating a short scan...");
        // Start a short, blocking scan to find the device if not already discovered
        BLEScanResults foundDevices = pBLEScan->start(5, false); // 5 seconds blocking scan
        for (int i = 0; i < foundDevices.getCount(); i++) {
            BLEAdvertisedDevice device = foundDevices.getDevice(i);
            if (device.getAddress().equals(targetAddress)) {
                deviceToConnect = new BLEAdvertisedDevice(device);
                break;
            }
        }
        pBLEScan->stop(); // Stop the blocking scan

        if (deviceToConnect) {
            connectToServer(deviceToConnect);
            delete deviceToConnect;
        } else {
            Serial.println("Failed to find device with MAC: " + macAddress);
        }
    }
}

void BLEManager::disconnect() {
    if (pClient && pClient->isConnected()) {
        pClient->disconnect();
        Serial.println("Disconnected from BLE server.");
    } else {
        Serial.println("Not connected to any BLE server.");
    }
    connected = false;
    // Clean up client resources
    if (pClient) {
        delete pClient;
        pClient = nullptr;
        pRemoteService = nullptr;
        pRxCharacteristic = nullptr;
        pTxCharacteristic = nullptr;
    }
}

bool BLEManager::sendData(const String& data) {
    if (connected && pRxCharacteristic != nullptr) {
        pRxCharacteristic->writeValue(data.c_str(), data.length());
        Serial.print("Sent data: ");
        Serial.println(data);
        return true;
    }
    Serial.println("Not connected to BLE server or RX characteristic not found.");
    return false;
}

void BLEManager::setOnDataReceivedCallback(BleDataReceivedCallback callback) {
    dataReceivedCallback = callback;
}

bool BLEManager::isConnected() {
    return connected;
}

const std::vector<BLEAdvertisedDevice>& BLEManager::getDiscoveredDevices() {
    return discoveredDevices;
}

void BLEManager::notifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
    if (pThisBLEManager != nullptr && pThisBLEManager->dataReceivedCallback != nullptr) {
        String receivedData = "";
        for (int i = 0; i < length; i++) {
            receivedData += (char)pData[i];
        }
        pThisBLEManager->dataReceivedCallback(receivedData);
    }
}
