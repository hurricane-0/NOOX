#ifndef HARDWARE_MANAGER_H
#define HARDWARE_MANAGER_H

#include "hardware_config.h"
#include <U8g2lib.h>
#include <FastLED.h>

class HardwareManager {
public:
    HardwareManager();
    void begin();
    void update();

    // OLED Display
    U8G2_SSD1315_128X64_NONAME_F_HW_I2C& getDisplay();

    // Buttons
    bool isButtonPressed(int buttonPin);

    // LEDs
    void setLedState(int ledPin, bool state);
    void setRgbColor(CRGB color);

    // Control for Reserved GPIOs
    void setReservedGpio1State(bool state);
    void setReservedGpio2State(bool state);

private:
    // OLED Display object
    U8G2_SSD1315_128X64_NONAME_F_HW_I2C u8g2;
    
    // RGB LED
    CRGB leds[NUM_LEDS];
};

#endif // HARDWARE_MANAGER_H
