#include "usb_hid_manager.h"
#include "hardware_config.h" // For extern Keyboard object
#include <vector> // Required for std::vector

void usb_hid_init() {
  // USB HID Keyboard is usually initialized as part of USB.begin()
  // which is typically called in hardware_init().
  // No explicit initialization needed here beyond ensuring USB is started.
  Serial.println("USB HID Manager initialized.");
}

void usb_hid_send_key(uint8_t keycode) {
  Keyboard.press(keycode);
  delay(50); // Small delay for key press
  Keyboard.release(keycode);
  delay(50); // Small delay for key release
  Serial.printf("Sent key: 0x%02X\n", keycode);
}

void usb_hid_send_string(const String& text) {
  Keyboard.print(text);
  Serial.print("Sent string: ");
  Serial.println(text);
}

void usb_hid_press_keys(const std::vector<uint8_t>& keycodes) {
  for (uint8_t keycode : keycodes) {
    Keyboard.press(keycode);
  }
  Serial.print("Pressed keys: ");
  for (uint8_t keycode : keycodes) {
    Serial.printf("0x%02X ", keycode);
  }
  Serial.println();
}

void usb_hid_release_all() {
  Keyboard.releaseAll();
  Serial.println("Released all keys.");
}
