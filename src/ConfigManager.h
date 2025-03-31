#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <ArduinoJson.h>
#include <FS.h>
#include <LittleFS.h>
#include <map>
#include <vector>

// Declare the external function from ModuleSetup.cpp
extern String readJsonFile(const char* filePath);

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
    static std::map<String, ActionConfig> loadActions(const String& filePath);    
    static String readFile(const char* filePath);
    
    // Parse the actions file
    static std::map<String, ActionConfig> parseActionsFile(File &file);
};

#endif // CONFIG_MANAGER_H