#ifndef USB_HID_MANAGER_H
#define USB_HID_MANAGER_H

#include <Arduino.h>
#include "USBHIDKeyboard.h" // Include the USBHIDKeyboard definitions
#include "hardware_config.h" // For extern Keyboard object
#include <vector> // Required for std::vector

// Function to initialize USB HID (usually done in hardware_init)
void usb_hid_init();

// Function to send a single key press and release
void usb_hid_send_key(uint8_t keycode);

// Function to send a string of text
void usb_hid_send_string(const String& text);

// Function to press multiple keys (e.g., modifiers + key)
void usb_hid_press_keys(const std::vector<uint8_t>& keycodes);

// Function to release all currently pressed keys
void usb_hid_release_all();

#endif // USB_HID_MANAGER_H
