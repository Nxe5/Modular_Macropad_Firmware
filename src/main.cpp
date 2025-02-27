#include <Arduino.h>

// Feature flags - change these as you implement features
#define ENABLE_KEYPAD 1
#define ENABLE_ENCODER 1
#define ENABLE_DISPLAY 0
#define ENABLE_LEDS 0
#define ENABLE_WEB_SERVER 0
#define ENABLE_SERIAL_HANDLER 0

// Include only what's needed
#if ENABLE_KEYPAD
  #include <Keypad.h>
#endif

#if ENABLE_ENCODER
  #include <Encoder.h>
#endif

#if ENABLE_DISPLAY
  #include "DisplayHandler.h"
#endif

#if ENABLE_LEDS
  #include "LEDHandler.h"
#endif

#if ENABLE_SERIAL_HANDLER
  #include "SerialHandler.h"
  #include "ModuleInfo.h"
#endif

#if ENABLE_WEB_SERVER
  #include "WebServer.h"
#endif

// Global variables based on enabled features
#if ENABLE_KEYPAD
  // Keypad setup
  const byte ROWS = 3;
  const byte COLS = 6;

  // Define key layout - adjust this based on your physical layout
  char keys[ROWS][COLS] = {
    {'1', '2', '3', '4', '5', '6'},
    {'A', 'B', 'C', 'D', 'E', 'F'},
    {'G', 'H', 'I', 'X', 'X', 'X'}
  };

  // Define row and column pins - adjust to match your hardware
  byte rowPins[ROWS] = {5, 6, 7};           // Example pins - adjust for your hardware
  byte colPins[COLS] = {8, 9, 10, 11, 12, 13}; // Example pins - adjust for your hardware

  Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);
#endif

#if ENABLE_ENCODER
  #define ENCODER_PIN_A 2  // RotaryA
  #define ENCODER_PIN_B 4  // RotaryB
  Encoder encoder(ENCODER_PIN_A, ENCODER_PIN_B);
  long oldPosition = 0;
#endif

#if ENABLE_WEB_SERVER
  WebServer* webServer = nullptr;
#endif

void setup() {
  Serial.begin(115200);
  delay(1000);  // Give serial monitor time to open
  Serial.println("Incremental Test Program");
  Serial.println("------------------------");
  
  #if ENABLE_DISPLAY
    initializeDisplay();
    Serial.println("Display initialized");
  #endif
  
  #if ENABLE_SERIAL_HANDLER
    if (!SPIFFS.begin(true)) {
      Serial.println("SPIFFS mount failed");
      return;
    }
    initializeModuleInfo();
    initializeSerialHandler();
    Serial.println("Serial handler initialized");
  #endif
  
  #if ENABLE_LEDS
    initializeLED();
    Serial.println("LEDs initialized");
  #endif
  
  #if ENABLE_WEB_SERVER
    webServer = new WebServer();
    webServer->begin();
    Serial.println("Web server initialized");
  #endif
  
  Serial.println("Setup complete - starting main loop");
}

void loop() {
  #if ENABLE_KEYPAD
    // Check for key presses
    char key = keypad.getKey();
    if (key != NO_KEY) {
      Serial.print("Key pressed: ");
      Serial.println(key);
      
      #if ENABLE_LEDS
        // Example: Light up LED based on key press
        int keyIndex = -1;
        
        // For digits 1-9
        if (key >= '1' && key <= '9') {
          keyIndex = key - '1';
        } 
        // For letters A-I (indices 9-17)
        else if (key >= 'A' && key <= 'I') {
          keyIndex = key - 'A' + 9;
        }
        
        if (keyIndex >= 0 && keyIndex < 18) {
          // Set LED to a color
          setLEDColor(keyIndex, 255, 0, 0); // Red
        }
      #endif
    }
  #endif
  
  #if ENABLE_ENCODER
    // Check encoder
    long newPosition = encoder.read();
    if (newPosition != oldPosition) {
      Serial.print("Encoder position: ");
      Serial.print(newPosition);
      Serial.print(" (change: ");
      Serial.print(newPosition - oldPosition);
      Serial.println(")");
      oldPosition = newPosition;
    }
  #endif
  
  #if ENABLE_SERIAL_HANDLER
    handleSerialCommands();
  #endif
  
  #if ENABLE_DISPLAY
    updateDisplay();
  #endif
  
  delay(10); // Small delay for stability
}