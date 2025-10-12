#include "hardware_manager.h"
#include <Wire.h>

HardwareManager::HardwareManager() 
    : u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE),
      button1Pin(BUTTON_1_PIN),
      button2Pin(BUTTON_2_PIN),
      button3Pin(BUTTON_3_PIN) {}

void HardwareManager::begin() {
    // Initialize I2C for OLED
    Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);
    u8g2.begin();

    // Initialize Buttons
    pinMode(button1Pin, INPUT_PULLDOWN);
    pinMode(button2Pin, INPUT_PULLDOWN);
    pinMode(button3Pin, INPUT_PULLDOWN);

    // Initialize LEDs
    pinMode(LED_1_PIN, OUTPUT);
    pinMode(LED_2_PIN, OUTPUT);
    pinMode(LED_3_PIN, OUTPUT);

    // Initialize RGB LED
    FastLED.addLeds<NEOPIXEL, RGB_LED_PIN>(leds, NUM_LEDS);

    // Initialize Reserved GPIOs as outputs
    pinMode(RESERVED_GPIO_1, OUTPUT);
    pinMode(RESERVED_GPIO_2, OUTPUT);
}


U8G2_SSD1315_128X64_NONAME_F_HW_I2C& HardwareManager::getDisplay() {
    return u8g2;
}


void HardwareManager::setLedState(int ledPin, bool state) {
    digitalWrite(ledPin, state);
}

void HardwareManager::setRgbColor(CRGB color) {
    leds[0] = color;
    FastLED.show();
}

void HardwareManager::setGpio1State(bool state) {
    digitalWrite(RESERVED_GPIO_1, state);
}

void HardwareManager::setGpio2State(bool state) {
    digitalWrite(RESERVED_GPIO_2, state);
}

bool HardwareManager::getButtonA() {
    return digitalRead(button1Pin) == HIGH;
}

bool HardwareManager::getButtonB() {
    return digitalRead(button2Pin) == HIGH;
}

bool HardwareManager::getButtonC() {
    return digitalRead(button3Pin) == HIGH;
}

// ==================== LLM Integration: Unified GPIO Control ====================

bool HardwareManager::setGpioOutput(const String& gpioName, bool state) {
    String gpio = gpioName;
    gpio.toLowerCase();
    
    // LED控制
    if (gpio == "led1") {
        setLedState(LED_1_PIN, state);
        return true;
    } else if (gpio == "led2") {
        setLedState(LED_2_PIN, state);
        return true;
    } else if (gpio == "led3") {
        setLedState(LED_3_PIN, state);
        return true;
    }
    // 预留GPIO控制
    else if (gpio == "gpio1") {
        setGpio1State(state);
        return true;
    } else if (gpio == "gpio2") {
        setGpio2State(state);
        return true;
    }
    
    // 未知GPIO名称
    return false;
}

String HardwareManager::getAvailableGpios() {
    return "led1, led2, led3, gpio1, gpio2";
}