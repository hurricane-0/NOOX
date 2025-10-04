#ifndef HARDWARE_CONFIG_H
#define HARDWARE_CONFIG_H

#include <Arduino.h>

// --- I2C (OLED Screen) ---
// Using Wire (default I2C bus)
#define OLED_SDA_PIN 4
#define OLED_SCL_PIN 5
#define OLED_ADDR    0x3C // Common address for 128x64 OLEDs, verify with your module

// --- I2C (Reserved) ---
// Using Wire1 (second I2C bus)
#define I2C1_SDA_PIN 1
#define I2C1_SCL_PIN 2

// --- SPI (TF Card) ---
#define SPI_CS_PIN   6
#define SPI_SCK_PIN  15
#define SPI_MISO_PIN 16
#define SPI_MOSI_PIN 7

// --- Button Inputs (Active High) ---
// Note: Configure with internal pull-down resistors.
#define BUTTON_1_PIN 47 // OK
#define BUTTON_2_PIN 21 // Up
#define BUTTON_3_PIN 38 // Down

// --- LED Outputs (Active High) ---
#define LED_1_PIN 41
#define LED_2_PIN 40
#define LED_3_PIN 39

// --- Additional UART ---
#define UART1_TX_PIN 17
#define UART1_RX_PIN 18

// --- Reserved General Purpose GPIO ---
#define RESERVED_GPIO_1 8
#define RESERVED_GPIO_2 9
#define RESERVED_GPIO_3 10

// --- Onboard RGB LED (WS2812) ---
#define RGB_LED_PIN 48
#define NUM_LEDS    1

// --- USB OTG Pins ---
// These are handled by the ESP32-S3's native USB peripheral.
// No direct pin definition is needed for standard USB HID usage.
// #define USB_DM_PIN 19
// #define USB_DP_PIN 20

#endif // HARDWARE_CONFIG_H
