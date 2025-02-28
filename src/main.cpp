#include "FS.h"
#include "SPIFFS.h"
#include "ArduinoJson.h"

// Include your project modules
#include "ModuleSetup.h"
#include "DisplayHandler.h"
#include "EncoderHandler.h"
#include "LEDHandler.h"
#include "ConfigManager.h"
#include "KeyHandler.h"

#define ROW0 3
#define ROW1 5
#define ROW2 26
#define ROW3 47
#define ROW4 33
#define COL0 34
#define COL1 21
#define COL2 13
#define COL3 6
#define COL4 12

// Function to list files in SPIFFS directory (for debugging)
void listDir(fs::FS &fs, const char* dirname, uint8_t levels) {
  Serial.printf("Listing directory: %s\n", dirname);
  File root = fs.open(dirname);
  if (!root) {
    Serial.println("Failed to open directory");
    return;
  }
  if (!root.isDirectory()) {
    Serial.println("Not a directory");
    return;
  }
  File file = root.openNextFile();
  while (file) {
    if (file.isDirectory()) {
      Serial.print("  DIR : ");
      Serial.println(file.name());
      if (levels) {
        listDir(fs, file.name(), levels - 1);
      }
    } else {
      Serial.print("  FILE: ");
      Serial.print(file.name());
      Serial.print("\tSIZE: ");
      Serial.println(file.size());
    }
    file = root.openNextFile();
  }
}

// Dynamic key mapping creation from components.json
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
    char nextKey = '1';  // Start with '1' as the first key marker

    for (JsonObject component : components) {
        String type = component["type"].as<String>();
        
        // Check if component is a button or an encoder with a button
        if (type == "button" || 
            (type == "encoder" && 
             component.containsKey("with_button") && 
             component["with_button"].as<bool>())) {
            
            // Get the component's location
            uint8_t startRow = component["start_location"]["row"];
            uint8_t startCol = component["start_location"]["column"];
            
            // Validate the location is within grid bounds
            if (startRow < rows && startCol < cols) {
                // Assign a unique key marker
                keyMapping[startRow][startCol] = nextKey++;
                
                Serial.printf("Mapped %s at [%d,%d] with key %c\n", 
                              component["id"].as<String>().c_str(), 
                              startRow, startCol, 
                              keyMapping[startRow][startCol]);
            }
        }
    }

    return keyMapping;
}

// initializeKeyHandler() using a 5x5 grid and loading actions via ConfigManager
void initializeKeyHandler() {
  const uint8_t rows = 5;
  const uint8_t cols = 5;
  uint8_t rowPins[rows] = {ROW0, ROW1, ROW2, ROW3, ROW4};    // Update with your actual row pins
  uint8_t colPins[cols] = {COL0, COL1, COL2, COL3, COL4};     // Update with your actual column pins
  
  // Read components JSON from the file
  String componentsJson = ConfigManager::readFile("/config/components.json");
  
  // Create key mapping dynamically from components
  char** keyMapping = createKeyMappingFromComponents(componentsJson, rows, cols);
  
  // Create and initialize the key handler instance.
  keyHandler = new KeyHandler(rows, cols, keyMapping, rowPins, colPins);
  keyHandler->begin();
  
  // Load the actions configuration from SPIFFS (from /config/actions.json)
  auto actions = ConfigManager::loadActions("/config/actions.json");
  keyHandler->loadKeyConfiguration(actions);
  
  // Clean up temporary keyMapping allocation.
  for (uint8_t i = 0; i < rows; i++) {
    delete[] keyMapping[i];
  }
  delete[] keyMapping;
}

unsigned long previousMillis = 0;
const long interval = 10000; // 10 seconds heartbeat

void setup() {
  // Start serial communication
  Serial.begin(115200);
  delay(1000);
  
  // Mount SPIFFS with error handling
  if (!SPIFFS.begin(false)) {
    Serial.println("SPIFFS mount failed!");
    // Attempt a format if desired.
  } else {
    Serial.println("SPIFFS mounted successfully.");
  }
  
  // Optionally list files in SPIFFS for debugging.
  listDir(SPIFFS, "/", 0);
  
  // Initialize module-specific setups.
  Serial.println("Initializing module info...");
  initializeModuleInfo();
  
  Serial.println("Initializing LED handler...");
  initializeLED();
  
  Serial.println("Initializing key handler...");
  initializeKeyHandler();
  
  delay(3000);
  Serial.println("Initialization complete!");
}

void loop() {
  unsigned long currentMillis = millis();
  
  // Non-blocking heartbeat.
  if (currentMillis - previousMillis >= interval) {
    Serial.println("Heartbeat..");
    previousMillis = currentMillis;
  }
  
  // Update key states.
  if (keyHandler) {
    keyHandler->updateKeys();
  }
  
  // Other loop operations can be added here...
}