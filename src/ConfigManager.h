#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>
#include <map>

// Declare the external function from ModuleSetup.cpp
extern String readJsonFile(const char* filePath);

// Component structure matching the JSON format
struct Component {
    String id;
    String type;
    int startRow;
    int startCol;
    int rows;
    int cols;
    bool withButton;
};

// Action configuration structure
struct ActionConfig {
    String id;
    String type;
    std::vector<String> hidReport;
    std::vector<String> consumerReport;
    String macroId;
    String targetLayer;
    std::vector<String> clockwise;      // Add for encoder support
    std::vector<String> counterclockwise; // Add for encoder support
};

// Display Element structure - MUST BE DEFINED BEFORE DisplayMode
struct DisplayElement {
    int type;
    int x;
    int y;
    int width;
    int height;
    String text;
    String variable;
    String alignment;
    uint16_t color;              
    int size;
    int end_x;
    int end_y;
    bool filled;
};

// Display Mode structure
struct DisplayMode {
    String name;
    bool active;
    String template_file;
    String description;           
    unsigned long refresh_rate;
    std::vector<DisplayElement> elements;
};

// Module Info structure
struct ModuleInfo {
    String name;
    String version;
    String author;
    String description;
    String moduleSize;
    int gridRows;
    int gridCols;
};

class ConfigManager {
public:
    // Component-related functions
    static std::vector<Component> loadComponents(const char* filePath);
    
    // Actions-related functions
    static std::map<String, ActionConfig> loadActions(const char* filePath);
    
    // Display-related functions
    static std::map<String, DisplayMode> loadDisplayModes(const char* filePath);
    static std::vector<DisplayElement> loadDisplayElements(const char* filePath, const String& modeName);
    
    // Module Info functions
    static ModuleInfo loadModuleInfo(const char* filePath);
    
    // Utility functions
    static String readFile(const char* filePath);

private:
    // Private helper functions
    static bool parseComponent(JsonObjectConst obj, Component& component);
};

#endif // CONFIG_MANAGER_H