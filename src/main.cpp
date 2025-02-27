#include "ModuleConfiguration.h"
#include "ModuleInfo.h"

#include "DisplayHandler.h"
#include "KeyHandler.h"
#include "EncoderHandler.h"
#include "LEDHandler.h"
#include "SerialHandler.h"
#include "WebServer.h"
#include "FS.h"
#include "LittleFS.h"
#include "ArduinoJson.h"

void setup() {
    // Initialize serial first for debugging
    Serial.begin(115200);
    delay(8000);
    Serial.println("Starting initialization...");
    
    // Initialize SPIFFS (only once for all components)
    if (!SPIFFS.begin(true)) {
        Serial.println("SPIFFS mount failed");
        return;
    }
    
    // Initialize module info - this will read the component files and create config.json
    initializeModuleInfo();
    Serial.println("Module info initialized");
    
    // Initialize serial handler
    // initializeSerialHandler();
    // Serial.println("Serial handler initialized");
    
    // Initialize key matrix
    // initializeKeys();
    // Serial.println("Keys initialized");

    // Initialize encoder
    // initializeEncoder();
    // Serial.println("Encoder initialized");
    
    // Initialize LEDs
    // initializeLED();
    // Serial.println("LEDs initialized");

    // Initialize display
    // initializeDisplay();
    // Serial.println("Display initialized");
    
    // Initialize web server
    // webServer = new WebServer();
    // webServer->begin();
    // Serial.println("Web server initialized");
    
    Serial.println("Initialization complete");
}

void loop() {
    // Heartbeat
    Serial.println("Heartbeat..."); 
    delay(5000);


    // Handle serial commands
    // handleSerialCommands();
    
    // Handle encoder
    // handleEncoderMovement();

    // Handle key presses
    // handleKeyPresses();
    
    // Update display
    // updateDisplay();
}