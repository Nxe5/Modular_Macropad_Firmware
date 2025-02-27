#include "FS.h"
#include "SPIFFS.h"
#include "ArduinoJson.h"

#include "ModuleConfiguration.h"
#include "ModuleInfo.h"

#include "DisplayHandler.h"
#include "KeyHandler.h"
#include "EncoderHandler.h"
#include "LEDHandler.h"
#include "SerialHandler.h"
#include "WebServer.h"

void listDir(fs::FS &fs, const char * dirname, uint8_t levels) {
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
  Serial.begin(115200);
  delay(8000);
  
    if (!SPIFFS.begin(true)) {
      Serial.println("An error occurred while mounting SPIFFS. Formatting...");
      // Formatting SPIFFS
      if (SPIFFS.format()) {
          Serial.println("SPIFFS formatted successfully. Rebooting...");
          ESP.restart();
      } else {
          Serial.println("SPIFFS formatting failed.");
      }
      return;
  }
  Serial.println("SPIFFS mounted successfully.");

  listDir(SPIFFS, "/", 0);
}

void loop() {
  // Nothing to do here.
}
