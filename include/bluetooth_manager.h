#ifndef BLUETOOTH_MANAGER_H
#define BLUETOOTH_MANAGER_H

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

extern bool deviceConnected; // Declare deviceConnected as extern

void bluetooth_init();
void bluetooth_scan_devices();
void bluetooth_start_gatt_server();
void bluetooth_start_gatt_client(const std::string& deviceAddress);
void bluetooth_send_data(const std::string& serviceUUID, const std::string& charUUID, const std::string& data);
void bluetooth_set_hid_keyboard_mode();

#endif // BLUETOOTH_MANAGER_H
