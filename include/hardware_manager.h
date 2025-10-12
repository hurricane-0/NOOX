#ifndef HARDWARE_MANAGER_H
#define HARDWARE_MANAGER_H

#include "hardware_config.h"
#include <U8g2lib.h>
#include <FastLED.h>

class HardwareManager {
public:
    HardwareManager();
    void begin();

    // OLED Display
    U8G2_SSD1315_128X64_NONAME_F_HW_I2C& getDisplay();

    // LEDs
    void setLedState(int ledPin, bool state);
    void setRgbColor(CRGB color);

    // Control for Reserved GPIOs
    void setGpio1State(bool state);
    void setGpio2State(bool state);

    // Button Inputs
    bool getButtonA(); // Corresponds to BUTTON_1_PIN (OK/Select)
    bool getButtonB(); // Corresponds to BUTTON_2_PIN (Up/Scroll Up)
    bool getButtonC(); // Corresponds to BUTTON_3_PIN (Down/Scroll Down)

    // LLM Integration: Unified GPIO control interface
    bool setGpioOutput(const String& gpioName, bool state); // Set GPIO by name
    String getAvailableGpios(); // Get list of available GPIOs

private:
    // OLED Display object
    U8G2_SSD1315_128X64_NONAME_F_HW_I2C u8g2;
    
    // Button pins (initialized in constructor)
    int button1Pin;
    int button2Pin;
    int button3Pin;
    
    // RGB LED
    CRGB leds[NUM_LEDS];
};

#endif // HARDWARE_MANAGER_H
