#include "hardware_init.h"
#include "hardware_config.h"

// Define global hardware objects
Adafruit_ST7735 tft = Adafruit_ST7735(&SPI, TFT_CS, TFT_DC, TFT_RST);
USBHIDKeyboard Keyboard;
CRGB leds[NUM_LEDS];

void initHardware() {
  // Initialize Serial for debugging
  Serial.begin(115200);
  Serial.println("ESP32-S3 AI Hardware booting...");

  // Initialize LCD backlight pin
  pinMode(TFT_BL, OUTPUT);
  // Ensure USB is initialized before Keyboard.begin()
  USB.begin(); // Ensure USB stack starts
  Keyboard.begin(); // Start keyboard simulation
  digitalWrite(TFT_BL, HIGH); // Turn on backlight

  // Initialize SPI bus
  // SCK=GPIO18, MOSI=GPIO23, MISO=GPIO17 (MISO is not used here, but needs a placeholder)
  SPI.begin(18, 17, 23, -1); 

  // Initialize LCD
  tft.initR(INITR_BLACKTAB); // Initialize ST7735S screen, use black tab initialization sequence
  tft.setRotation(1); // Set screen orientation, adjust as needed

  tft.fillScreen(ST77XX_BLACK); // Fill black background
  tft.setTextWrap(false); // No automatic line wrapping
  tft.setTextSize(2); // Set font size
  tft.setTextColor(ST77XX_WHITE); // Set font color
  tft.setCursor(0, 0); // Set cursor position
  tft.println("Hello, ESP32-S3!");
  tft.println("AI Hardware");

  // Initialize button pins as pulldown inputs
  pinMode(KEY1_PIN, INPUT_PULLDOWN);
  pinMode(KEY2_PIN, INPUT_PULLDOWN);
  pinMode(KEY3_PIN, INPUT_PULLDOWN);

  // Initialize LED pins as outputs
  pinMode(LED1_PIN, OUTPUT);
  pinMode(LED2_PIN, OUTPUT);

  // Initialize WS2812
  FastLED.addLeds<WS2812B, WS2812_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(64); // Set brightness

  // Initialize SD card
  if (!SD.begin(SD_CS)) {
    Serial.println("Card Mount Failed");
    tft.setCursor(0, 100);
    tft.setTextColor(ST77XX_RED);
    tft.println("SD Card Failed!");
    // return; // Do not return, continue initializing other hardware
  } else {
    Serial.println("SD Card Mounted.");
    tft.setCursor(0, 100);
    tft.setTextColor(ST77XX_GREEN);
    tft.println("SD Card OK!");

    // List files in SD card root directory (example)
    File root = SD.open("/");
    if (root) {
      Serial.println("Files on SD Card:");
      while (File entry = root.openNextFile()) {
        if (entry.isDirectory()) {
          Serial.print("  DIR : ");
          Serial.println(entry.name());
        } else {
          Serial.print("  FILE: ");
          Serial.print(entry.name());
          Serial.print("\tSIZE: ");
          Serial.println(entry.size());
        }
        entry.close();
      }
      root.close();
    } else {
      Serial.println("Failed to open root directory");
    }
  }

  // Initialize I2C
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Serial.println("I2C Initialized.");
  tft.setCursor(0, 120);
  tft.setTextColor(ST77XX_BLUE);
  tft.println("I2C Init OK!");

  // Scan I2C devices (example)
  byte error, address;
  int nDevices;
  Serial.println("Scanning I2C devices...");
  nDevices = 0;
  for (address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();
    if (error == 0) {
      Serial.print("I2C device found at address 0x");
      if (address < 16) Serial.print("0");
      Serial.print(address, HEX);
      Serial.println(" !");
      nDevices++;
    } else if (error == 4) {
      Serial.print("Unknown error at address 0x");
      if (address < 16) Serial.print("0");
      Serial.println(address, HEX);
    }
  }
  if (nDevices == 0) {
    Serial.println("No I2C devices found.");
  } else {
    Serial.println("I2C scan complete.");
  }

  // Initialize UART1
  Serial1.begin(115200, SERIAL_8N1, UART1_RX_PIN, UART1_TX_PIN);
  Serial.println("UART1 Initialized.");
  tft.setCursor(0, 140);
  tft.setTextColor(ST77XX_ORANGE);
  tft.println("UART1 Init OK!");

  // Initialize UART2
  Serial2.begin(115200, SERIAL_8N1, UART2_RX_PIN, UART2_TX_PIN);
  Serial.println("UART2 Initialized.");

  // All hardware initialization complete
  Serial.println("All hardware initialized.");
}
