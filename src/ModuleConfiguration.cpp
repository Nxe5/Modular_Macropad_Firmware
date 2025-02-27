#include <WiFi.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>

// Struct for key configuration
struct KeyConfig {
  String key;       // The key assignment (e.g., "A", "Ctrl+Z", "Volume Up")
  String action;    // Action (if applicable, like long press)
};

// Struct for module configuration
struct ModuleConfig {
  String id;                // MAC address as ID (unique)
  String type;              // Module type (button, rotary-encoder, etc.)
  KeyConfig keys[25];       // Key assignments for a 5x5 grid or customizable
  String additionalSettings; // Additional settings (e.g., backlight)
};

// Function to export configuration to a JSON file
void exportConfig(ModuleConfig moduleConfig) {
  // Open a file to store the configuration
  File configFile = SPIFFS.open("/module_config.json", FILE_WRITE);
  if (!configFile) {
    Serial.println("Failed to open config file for writing");
    return;
  }

  // Create a JSON document
  DynamicJsonDocument doc(1024);

  // Populate the JSON document with module configuration
  doc["id"] = moduleConfig.id;
  doc["type"] = moduleConfig.type;

  // Loop through keys and add them to the JSON
  JsonArray keysArray = doc.createNestedArray("keys");
  for (int i = 0; i < 25; i++) {  // assuming a 5x5 grid, you can adjust based on your layout
    JsonObject keyObj = keysArray.createNestedObject();
    keyObj["key"] = moduleConfig.keys[i].key;
    keyObj["action"] = moduleConfig.keys[i].action;
  }

  // Add additional settings (optional)
  doc["additionalSettings"] = moduleConfig.additionalSettings;

  // Serialize the JSON document to the file
  serializeJson(doc, configFile);
  configFile.close();

  Serial.println("Configuration saved to module_config.json");
}

void initializeModuleConfig() {
  // Note: SPIFFS is initialized in main.cpp, no need to initialize again
  
  // Retrieve the MAC address of the ESP32
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);  // Get MAC address
  String macAddress = String(mac[0], HEX) + ":" + String(mac[1], HEX) + ":" +
                    String(mac[2], HEX) + ":" + String(mac[3], HEX) + ":" +
                    String(mac[4], HEX) + ":" + String(mac[5], HEX);

  // Example configuration for the module
  ModuleConfig moduleConfig;
  moduleConfig.id = macAddress;  // Use MAC address as unique ID

  // Assigning keys (example for a 5x5 grid)
  for (int i = 0; i < 25; i++) {
    moduleConfig.keys[i].key = "Button " + String(i + 1);  // Placeholder key names
    moduleConfig.keys[i].action = "None";  // Placeholder action
  }

  // Export the configuration
  exportConfig(moduleConfig);
}