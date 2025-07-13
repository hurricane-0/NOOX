#ifndef BLE_MANAGER_H
#define BLE_MANAGER_H

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BLEClient.h>
#include <BLE2902.h> // For client callbacks

// Nordic UART Service (NUS) UUIDs
#define SERVICE_UUID        "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

// Callback for received data
typedef void (*BleDataReceivedCallback)(String data);

class BLEManager : public BLEAdvertisedDeviceCallbacks {
public:
    BLEManager();
    void begin();
    void startScan(int scanTimeSeconds = 5);
    void connectToServer(BLEAdvertisedDevice* pDevice); // Keep for internal use if needed
    void connectToAddress(const String& macAddress); // New: Connect by MAC address string
    void disconnect(); // New: Disconnect from current server
    bool sendData(const String& data);
    void setOnDataReceivedCallback(BleDataReceivedCallback callback);
    bool isConnected(); // New: Check connection status
    const std::vector<BLEAdvertisedDevice>& getDiscoveredDevices(); // New: Get discovered devices (returns const reference)

private:
    BLEScan* pBLEScan;
    BLEClient* pClient;
    BLERemoteService* pRemoteService;
    BLERemoteCharacteristic* pRxCharacteristic;
    BLERemoteCharacteristic* pTxCharacteristic;

    bool doConnect = false; // Flag to indicate connection needed
    BLEAdvertisedDevice* myDevice; // Device to connect to
    BleDataReceivedCallback dataReceivedCallback = nullptr;
    std::vector<BLEAdvertisedDevice> discoveredDevices; // Store all discovered devices

    void onResult(BLEAdvertisedDevice advertisedDevice); // Override from BLEAdvertisedDeviceCallbacks
    static void notifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify);
public: // Move connected to public
    bool connected = false;
    friend class MyClientCallbacks; // Declare MyClientCallbacks as a friend class
};

// Forward declaration for MyClientCallbacks (needed for friend declaration)
class MyClientCallbacks;

#endif // BLE_MANAGER_H
