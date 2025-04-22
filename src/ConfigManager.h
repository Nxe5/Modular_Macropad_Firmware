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

// Nested encoder action structure to support new format
struct EncoderActionConfig {
    String type;
    std::vector<String> report;
};

// Action configuration structure
struct ActionConfig {
    String id;
    String type;
    std::vector<String> report;      // Standard report field for unified format
    std::vector<String> hidReport;
    std::vector<String> consumerReport;
    String macroId;
    String targetLayer;
    std::vector<String> clockwise;      // Legacy format for encoder support
    std::vector<String> counterclockwise; // Legacy format for encoder support
    std::vector<String> buttonPress;    // Legacy format for encoder button
    
    // Mouse action fields
    String action;  // "click", "press", "release", "move", "scroll"
    int button;     // 1=left, 2=right, 3=middle
    int x;          // X movement
    int y;          // Y movement
    int wheel;      // Wheel movement
    int clicks;     // Number of clicks
    
    // New format for encoder actions with nested types
    EncoderActionConfig clockwiseAction;
    EncoderActionConfig counterclockwiseAction;
    EncoderActionConfig buttonPressAction;
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
    String backgroundImage;
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