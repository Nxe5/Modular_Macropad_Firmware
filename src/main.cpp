#include <Arduino.h>
#include <USB.h>
#include <USBCDC.h>
#include <USBHIDMouse.h>

#include "FS.h"
#include "SPIFFS.h"
#include "ArduinoJson.h"

// Modules
#include "ModuleSetup.h"
#include "ConfigManager.h"
#include "KeyHandler.h"  
#include "LEDHandler.h"


USBHIDMouse Mouse;
USBCDC USBSerial;

#define TAG "HID+CDC Esp32-s3 Macropad"

//  Will be moved to info.json
#define ROW0 3  // Kept (safe GPIO pin)
#define ROW1 5  // Kept (safe GPIO pin)
#define ROW2 8  // ROW2 26
#define ROW3 9  // ROW3 47
#define ROW4 10 // ROW4 33
#define COL0 11 // COL0 34
#define COL1 21 // Kept (safe GPIO pin)
#define COL2 13 // Kept (safe GPIO pin)
#define COL3 6  // Kept (safe GPIO pin)
#define COL4 12 // Kept (safe GPIO pin)

// Validate GPIO pins for ESP32-S3
bool validateGpioPins(uint8_t* pins, uint8_t count) {
    // Valid GPIO pins for ESP32-S3 are 0-21
    const int validESP32S3Pins[] = {
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 
        15, 16, 17, 18, 19, 20, 21
    };
    
    for (uint8_t i = 0; i < count; i++) {
        bool isValid = false;
        for (int validPin : validESP32S3Pins) {
            if (pins[i] == validPin) {
                isValid = true;
                break;
            }
        }
        
        if (!isValid) {
            Serial.printf("Invalid GPIO pin for ESP32-S3: %d\n", pins[i]);
            return false;
        }
    }
    return true;
}

// Consistent pin configuration for key matrix
void configurePinModes(uint8_t* rowPins, uint8_t* colPins, uint8_t rows, uint8_t cols) {
    // Validate pins first with detailed ESP32-S3 validation
    if (!validateGpioPins(rowPins, rows) || !validateGpioPins(colPins, cols)) {
        Serial.println("Invalid GPIO pins detected for ESP32-S3!");
        return;
    }

    // IMPORTANT: For a matrix keyboard setup:
    // For a typical matrix keyboard setup:
    // - Configure rows as INPUT_PULLUP (will be read)
    // - Configure columns as OUTPUT (will be driven)
    
    // First, print the pin assignments for clarity
    Serial.println("\n--- Pin Configuration ---");
    Serial.println("Row pins (configured as INPUT_PULLUP):");
    for (uint8_t i = 0; i < rows; i++) {
        Serial.printf("  Row %d: GPIO %d\n", i, rowPins[i]);
    }
    
    Serial.println("Column pins (configured as OUTPUT):");
    for (uint8_t j = 0; j < cols; j++) {
        Serial.printf("  Column %d: GPIO %d\n", j, colPins[j]);
    }
    
    // Now configure the pins
    for (uint8_t i = 0; i < rows; i++) {
        pinMode(rowPins[i], INPUT_PULLUP);
    }

    for (uint8_t j = 0; j < cols; j++) {
        pinMode(colPins[j], OUTPUT);
        digitalWrite(colPins[j], HIGH);  // Start with HIGH (inactive)
    }
    
    Serial.println("Pin configuration complete\n");
}

// Improved key mapping creation from components.json with no character overlaps
char** createKeyMappingFromComponents(const String& componentsJson, uint8_t rows, uint8_t cols) {
    // Allocate the key mapping grid
    char** keyMapping = new char*[rows];
    for (uint8_t i = 0; i < rows; i++) {
        keyMapping[i] = new char[cols];
        for (uint8_t j = 0; j < cols; j++) {
            keyMapping[i][j] = 'X';  // Initialize all positions as 'X'
        }
    }

    // Parse the components JSON
    DynamicJsonDocument doc(8192);
    DeserializationError error = deserializeJson(doc, componentsJson);
    
    if (error) {
        Serial.print("Error parsing components JSON: ");
        Serial.println(error.c_str());
        return keyMapping;
    }

    // Iterate through components
    JsonArray components = doc["components"].as<JsonArray>();
    
    for (JsonObject component : components) {
        String type = component["type"].as<String>();
        String id = component["id"].as<String>();
        
        // Check if component is a button or an encoder with a button
        if (type == "button" || 
            (type == "encoder" && 
             component.containsKey("with_button") && 
             component["with_button"].as<bool>())) {
            
            // Get the component's location
            uint8_t startRow = component["start_location"]["row"];
            uint8_t startCol = component["start_location"]["column"];
            
            // Extract the component number and create a unique key character
            char keyChar = 'X';
            
            if (type == "button") {
                // For buttons, extract number from id (e.g., "button-1" -> '1')
                int dashPos = id.indexOf('-');
                if (dashPos >= 0 && dashPos < id.length() - 1) {
                    String numStr = id.substring(dashPos + 1);
                    int buttonNum = numStr.toInt();
                    
                    if (buttonNum > 0 && buttonNum <= 9) {
                        // Buttons 1-9 -> '1'-'9'
                        keyChar = '0' + buttonNum;
                    } else if (buttonNum >= 10 && buttonNum <= 35) {
                        // Buttons 10-35 -> 'A'-'Z'
                        keyChar = 'A' + (buttonNum - 10);
                    }
                }
            } else if (type == "encoder" && component["with_button"].as<bool>()) {
                // For encoder buttons, use special characters that won't overlap with buttons
                // Use characters outside the normal ASCII range for better separation
                int dashPos = id.indexOf('-');
                if (dashPos >= 0 && dashPos < id.length() - 1) {
                    String numStr = id.substring(dashPos + 1);
                    int encoderNum = numStr.toInt();
                    // Use 'a'-'z' range for encoders to differentiate from buttons
                    keyChar = 'a' + (encoderNum - 1);  // encoder-1 -> 'a', encoder-2 -> 'b', etc.
                }
            }
            
            // Validate the location is within grid bounds
            if (startRow < rows && startCol < cols && keyChar != 'X') {
                // Assign the key marker based on component ID
                keyMapping[startRow][startCol] = keyChar;
                
                Serial.printf("Mapped %s at [%d,%d] with key %c\n", 
                              id.c_str(), startRow, startCol, keyChar);
            }
        }
    }

    // Debug print the entire key mapping
    Serial.println("Key mapping matrix:");
    for (uint8_t i = 0; i < rows; i++) {
        Serial.print("Row ");
        Serial.print(i);
        Serial.print(": ");
        for (uint8_t j = 0; j < cols; j++) {
            Serial.print(keyMapping[i][j]);
            Serial.print(" ");
        }
        Serial.println();
    }

    return keyMapping;
}

// initializeKeyHandler() using a 5x5 grid and loading actions via ConfigManager
void initializeKeyHandler() {
  const uint8_t rows = 5;
  const uint8_t cols = 5;
  uint8_t rowPins[rows] = {ROW0, ROW1, ROW2, ROW3, ROW4};
  uint8_t colPins[cols] = {COL0, COL1, COL2, COL3, COL4};
  
  USBSerial.println("\n=== Initializing Keyboard Matrix ===");
  USBSerial.println("Matrix dimensions: 5x5");
  
  // Log pin assignments for clarity  
  USBSerial.println("Row pins:");
  for (int i = 0; i < rows; i++) {
    USBSerial.printf("  Row %d: GPIO %d\n", i, rowPins[i]);
  }
  
  USBSerial.println("Column pins:");
  for (int i = 0; i < cols; i++) {
    USBSerial.printf("  Column %d: GPIO %d\n", i, colPins[i]);
  }
  
  // Configure pin modes
  configurePinModes(rowPins, colPins, rows, cols);
  
  // Read components JSON from the file
  String componentsJson = ConfigManager::readFile("/config/components.json");
  USBSerial.println("Loading components from JSON...");
  
  // Create key mapping dynamically from components
  USBSerial.println("Creating key mapping from components...");
  char** keyMapping = createKeyMappingFromComponents(componentsJson, rows, cols);
  
  // Create and initialize the key handler instance
  USBSerial.println("Initializing key handler instance...");
  keyHandler = new KeyHandler(rows, cols, keyMapping, rowPins, colPins);
  
  if (keyHandler) {
    keyHandler->begin();
    
    // Load the actions configuration from SPIFFS
    USBSerial.println("Loading key action configuration...");
    auto actions = ConfigManager::loadActions("/config/actions.json");
    keyHandler->loadKeyConfiguration(actions);
    
    USBSerial.println("Key handler initialization complete");
  } else {
    USBSerial.println("ERROR: Failed to create key handler instance!");
  }
  
  // Clean up temporary keyMapping allocation
  for (uint8_t i = 0; i < rows; i++) {
    delete[] keyMapping[i];
  }
  delete[] keyMapping;
  
  USBSerial.println("=== Keyboard Matrix Initialization Complete ===\n");
}

// Function to debug actions configuration
void debugActionsConfig() {
    auto actions = ConfigManager::loadActions("/config/actions.json");
    
    USBSerial.println("\n=== Actions Configuration Debug ===");
    for (const auto& pair : actions) {
        USBSerial.printf("Button ID: %s, Type: %s\n", 
                    pair.first.c_str(), 
                    pair.second.type.c_str());
        
        if (pair.second.type == "multimedia" && !pair.second.consumerReport.empty()) {
            USBSerial.print("  Consumer Report: ");
            for (const auto& hex : pair.second.consumerReport) {
                USBSerial.printf("%s ", hex.c_str());
            }
            USBSerial.println();
        }
    }
    USBSerial.println("==================================\n");
}

void setup() {
    // Initialize USB with both HID and CDC
    USB.begin();
    USBSerial.begin();
    delay(10000);

    USBSerial.println(TAG);

    // Mount SPIFFS for configuration files
    if (!SPIFFS.begin(true)) {
        USBSerial.println("Failed to mount SPIFFS");
    } else {
        USBSerial.println("SPIFFS mounted successfully");
    }
    
    // Initialize module configuration
    USBSerial.println("Initializing module configuration...");
    initializeModuleInfo();
    // Optionally, print the merged module info for debugging
    String moduleInfo = getModuleInfoJson();
    USBSerial.println("Module Info:");
    USBSerial.println(moduleInfo);
    
    USBSerial.println("USB HID Mouse and CDC initialized");
    USBSerial.println("USB CDC Serial initialized");

    USBSerial.println("Initializing KeyHandler...");
    initializeKeyHandler();  // Make sure this function is defined and linked

    USBSerial.println("Initialize LEDs");
    initializeLED();

    // Debug actions configuration
    debugActionsConfig();

     // Set initial LED colors
  if (strip) {
    // Make a startup animation: all LEDs light up in sequence
    for (int i = 0; i < numLEDs; i++) {
      setLEDColor(i, 0, 255, 0);  // Green
      delay(50);
    }
    delay(500);
    
    // Then set them to their configured colors
    for (int i = 0; i < numLEDs; i++) {
      float factor = ledConfigs[i].brightness / 255.0;
      strip->setPixelColor(i, strip->Color(
        ledConfigs[i].r * factor,
        ledConfigs[i].g * factor,
        ledConfigs[i].b * factor
      ));
    }
    strip->show();
  }
    // (Optionally, if you have additional tasks to create, add them here)
}

void loop() {
    // Minimal loop - print a heartbeat every 5 seconds
    static unsigned long lastPrint = 0;
    if (millis() - lastPrint > 5000) {
        lastPrint = millis();
        USBSerial.println("Heartbeat...");
    }

    // Update hardware handlers
    if (keyHandler) {
        keyHandler->updateKeys();
    }
    
    delay(1);
}

