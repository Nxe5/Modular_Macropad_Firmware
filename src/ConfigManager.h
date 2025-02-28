#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <ArduinoJson.h>
#include <FS.h>
#include <SPIFFS.h>
#include <map>
#include <vector>

struct Component {
  String id;
  String type;
  uint8_t startRow;
  uint8_t startCol;
  uint8_t rows;
  uint8_t cols;
  bool withButton = false; // for encoder with a button
};

struct ActionConfig {
  String id;
  String type;
  // For HID reports, consumer reports, macro info, etc.
  std::vector<String> hidReport; // Can store as hex strings for conversion later
  std::vector<String> consumerReport;
  String macroId;
  String targetLayer;
  std::vector<String> clockwise;
  std::vector<String> counterclockwise;
};

class ConfigManager {
public:
    // Reads and parses components.json
    static std::vector<Component> loadComponents(const char* filePath);
    // Reads and parses actions.json and returns a mapping from button id to ActionConfig
    static std::map<String, ActionConfig> loadActions(const char* filePath);
    // Add this method to the public section
    static String readFile(const char* filePath);
    
};

#endif // CONFIG_MANAGER_H
