#include "FS.h"
#include "SPIFFS.h"
#include "ArduinoJson.h"

#include "ModuleSetup.h"

#include "DisplayHandler.h"
#include "KeyHandler.h"
#include "EncoderHandler.h"
#include "LEDHandler.h"

// Function to list files in SPIFFS directory
void listDir(fs::FS &fs, const char* dirname, uint8_t levels) {
  Serial.printf("Listing directory: %s\n", dirname);

  File root = fs.open(dirname);
  if(!root){
    Serial.println("Failed to open directory");
    return;
  }
  if(!root.isDirectory()){
    Serial.println("Not a directory");
    return;
  }

  File file = root.openNextFile();
  while(file){
    if(file.isDirectory()){
      Serial.print("  DIR : ");
      Serial.println(file.name());
      if(levels){
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

void setup() {
  // Start serial communication
  Serial.begin(115200);
  delay(10000); // Give time for serial monitor to connect
  
  Serial.println("\n\n==== ESP32 Macropad Initializing ====");
  
  // Try mounting with basic error handling
  bool spiffsStarted = false;
  try {
    spiffsStarted = SPIFFS.begin(false);  // Try without auto-format first
    Serial.println("SPIFFS.begin executed without crashing");
  } catch (...) {
    Serial.println("SPIFFS.begin caused an exception");
  }
  
  if (!spiffsStarted) {
    Serial.println("SPIFFS mount failed, will try explicit format");
    delay(100);
    
    bool formatSuccess = false;
    try {
      formatSuccess = SPIFFS.format();
      Serial.println("Format operation completed");
    } catch (...) {
      Serial.println("Format operation caused an exception");
    }
    
    if (formatSuccess) {
      Serial.println("Format successful, trying to mount again");
      delay(100);
      
      try {
        spiffsStarted = SPIFFS.begin(false);
        Serial.println("Second mount attempt completed");
      } catch (...) {
        Serial.println("Second mount attempt caused an exception");
      }
      
      if (spiffsStarted) {
        Serial.println("SPIFFS mounted successfully after formatting");
      } else {
        Serial.println("SPIFFS still won't mount even after formatting");
      }
    } else {
      Serial.println("Format failed");
    }
  } else {
    Serial.println("SPIFFS mounted successfully on first try");
  }
  
  // Continue with or without SPIFFS
  Serial.println("Continuing with initialization...");
  
  // List files in storage
  listDir(SPIFFS, "/", 0);
  
  // Initialize module information first
  Serial.println("Initializing module info...");
  initializeModuleInfo();
  
  // Initialize hardware components
  Serial.println("Initializing LED handler...");
  initializeLED();
  
  // Initialize keys (this was missing)
  Serial.println("Initializing key handler...");
  initializeKeyHandler();
  
  delay(3000);
  
  Serial.println("Initialization complete!");
}

void loop() {
  // Heartbeat 
  Serial.println("Heartbeat..");

  // Handle LED animations and updates
  updateLEDs();
  
  // Handle key inputs (this was missing)
  updateKeyHandler();
  
  // Small delay to prevent CPU hogging
  delay(10000);
}