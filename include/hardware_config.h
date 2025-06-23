#ifndef HARDWARE_CONFIG_H
#define HARDWARE_CONFIG_H

#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <FastLED.h>
#include <Wire.h>   // For I2C
#include <SD.h>     // For SD Card
#include <USB.h>    // For USB functions
#include "USBHIDKeyboard.h" // For USB Keyboard HID (native ESP32)

// LCD 引脚定义 (根据 hardware.md)
#define TFT_CS    5
#define TFT_DC    2
#define TFT_RST   4 // 或者直接连接到 ESP32 的 RST 引脚
#define TFT_BL    45 // 背光控制

// ST7735s 屏幕尺寸
#define TFT_WIDTH  128
#define TFT_HEIGHT 160

// 按键引脚定义 (根据 hardware.md)
#define KEY1_PIN  47
#define KEY2_PIN  41
#define KEY3_PIN  33

// LED 引脚定义 (根据 hardware.md)
#define LED1_PIN  46
#define LED2_PIN  42

// WS2812 RGB LED 定义 (根据 hardware.md)
#define WS2812_PIN  48
#define NUM_LEDS    1 // 只有一个 WS2812 LED

// SD 卡引脚定义 (根据 hardware.md)
#define SD_SCK    36
#define SD_MOSI   35
#define SD_MISO   37
#define SD_CS     34

// I2C 引脚定义 (根据 hardware.md)
#define I2C_SDA_PIN 7
#define I2C_SCL_PIN 17

// UART 引脚定义 (根据 hardware.md)
// UART1
#define UART1_TX_PIN 43
#define UART1_RX_PIN 44
// UART2
#define UART2_TX_PIN 39
#define UART2_RX_PIN 40

// Global hardware objects
extern Adafruit_ST7735 tft;
extern USBHIDKeyboard Keyboard;
extern CRGB leds[NUM_LEDS];

#endif // HARDWARE_CONFIG_H
