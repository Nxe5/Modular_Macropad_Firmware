// KeyHandler.cpp

#include "KeyHandler.h"
#include "HIDHandler.h"
#include "EncoderHandler.h"  // Include for forwarding encoder button events
#include "LEDHandler.h"      // Changed from LightingHandler.h
#include "MacroHandler.h"
#include "ConfigManager.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <USBCDC.h>
#include <algorithm> // For std::sort

extern USBCDC USBSerial;
extern HIDHandler* hidHandler;
// LEDHandler is not a class, it's just a collection of functions
extern MacroHandler* macroHandler;

KeyHandler* keyHandler = nullptr;

KeyHandler::KeyHandler(uint8_t rows, uint8_t cols, 
                     const std::vector<Component>& components,
                     uint8_t* rowsPins, uint8_t* colPins) {
    // Initialize member variables
    numRows = rows;
    numCols = cols;
    rowPins = nullptr;
    this->colPins = nullptr;
    keypad = nullptr;
    actionMap = nullptr;
    keyStates = nullptr;
    lastDebounceTime = nullptr;
    lastAction = nullptr;

    // Validate input parameters
    if (rows > 10 || cols > 10) {
        USBSerial.println("Error: Matrix dimensions too large");
        return;
    }

    try {
        // Allocate pin arrays
        rowPins = new uint8_t[rows];
        this->colPins = new uint8_t[cols];
        
        // Copy pin arrays
        memcpy(rowPins, rowsPins, rows * sizeof(uint8_t));
        memcpy(this->colPins, colPins, cols * sizeof(uint8_t));
        
        // Store component positions for direct lookup
        for (const Component& comp : components) {
            if (comp.type == "button" || 
                (comp.type == "encoder" && comp.withButton)) {
                ComponentPosition pos;
                pos.row = comp.startRow;
                pos.col = comp.startCol;
                pos.id = comp.id;
                componentPositions.push_back(pos);
                
                USBSerial.printf("Mapped component %s to position [%d,%d]\n", 
                              comp.id.c_str(), pos.row, pos.col);
            }
        }
        
        // Sort positions for better lookup performance
        std::sort(componentPositions.begin(), componentPositions.end());
        
        // Create key states array of sufficient size
        uint8_t totalKeys = componentPositions.size();
        keyStates = new bool[totalKeys]();
        lastDebounceTime = new unsigned long[totalKeys]();
        lastAction = new KeyAction[totalKeys]();
        
        // Create action map
        actionMap = new KeyConfig[totalKeys];
        memset(actionMap, 0, totalKeys * sizeof(KeyConfig));
        
        USBSerial.printf("KeyHandler initialized with %d keys\n", totalKeys);
        
        // Debug print all component positions
        USBSerial.println("\nComponent Position Map:");
        for (size_t i = 0; i < componentPositions.size(); i++) {
            USBSerial.printf("%d: %s at [%d,%d]\n", i, 
                          componentPositions[i].id.c_str(),
                          componentPositions[i].row,
                          componentPositions[i].col);
        }
    } 
    catch (const std::exception& e) {
        USBSerial.printf("Exception in constructor: %s\n", e.what());
        cleanup();
    } 
    catch (...) {
        USBSerial.println("Unknown exception in constructor");
        cleanup();
    }
}

KeyHandler::~KeyHandler() {
    cleanup();
}

void KeyHandler::cleanup() {
    if (rowPins) {
        delete[] rowPins;
        rowPins = nullptr;
    }
    
    if (colPins) {
        delete[] colPins;
        colPins = nullptr;
    }
    
    if (keypad) {
        delete keypad;
        keypad = nullptr;
    }
    
    if (actionMap) {
        delete[] actionMap;
        actionMap = nullptr;
    }
    
    if (keyStates) {
        delete[] keyStates;
        keyStates = nullptr;
    }
    
    if (lastDebounceTime) {
        delete[] lastDebounceTime;
        lastDebounceTime = nullptr;
    }
    
    if (lastAction) {
        delete[] lastAction;
        lastAction = nullptr;
    }
    
    componentPositions.clear();
}

void KeyHandler::begin() {
    try {
        USBSerial.println("KeyHandler initialization complete - using configuration from configurePinModes()");
        
        // Set default layer name if none is set
        if (currentLayer == "default") {
            currentLayer = "default-actions-layer";
            USBSerial.printf("Setting default layer name to %s\n", currentLayer.c_str());
        }
        
        // Load the saved layer if available
        loadCurrentLayer();
        
        USBSerial.printf("KeyHandler using layer: %s\n", currentLayer.c_str());
    } 
    catch (...) {
        USBSerial.println("Error in KeyHandler::begin()");
    }
}

uint8_t KeyHandler::getTotalKeys() {
    return componentPositions.size();
}

void KeyHandler::configureMouseAction(const ActionConfig& actionConfig, KeyConfig& keyConfig, const String& componentId, const String& layerName) {
    keyConfig.type = ACTION_MOUSE;
    
    // Initialize the report with the report ID
    keyConfig.mouseReport[0] = 1; // Report ID 1
    
    // First check for direct report field (new standardized format)
    if (!actionConfig.report.empty()) {
        // Parse mouse report from standardized format
        // Skip the first byte (report ID) and copy up to 4 bytes of data
        for (size_t i = 0; i < std::min(actionConfig.report.size(), (size_t)4); i++) {
            keyConfig.mouseReport[i + 1] = strtoul(actionConfig.report[i].c_str(), nullptr, 16);
        }
        USBSerial.printf("  Loaded mouse config for '%s'%s using standardized format\n", 
                        componentId.c_str(), layerName.isEmpty() ? "" : (" in layer '" + layerName + "'").c_str());
    }
    // Then check for new format with buttonPress.report
    else if (actionConfig.buttonPressAction.type == "mouse" && !actionConfig.buttonPressAction.report.empty()) {
        // Parse mouse report from new format
        // Skip the first byte (report ID) and copy up to 4 bytes of data
        for (size_t i = 0; i < std::min(actionConfig.buttonPressAction.report.size(), (size_t)4); i++) {
            keyConfig.mouseReport[i + 1] = strtoul(actionConfig.buttonPressAction.report[i].c_str(), nullptr, 16);
        }
        USBSerial.printf("  Loaded mouse config for '%s'%s using nested format\n", 
                        componentId.c_str(), layerName.isEmpty() ? "" : (" in layer '" + layerName + "'").c_str());
    }
    // Then check for action and button fields
    else if (actionConfig.action == "click" || actionConfig.action == "press") {
        // Set up mouse report based on action and button
        uint8_t button = 0;
        if (actionConfig.button == 1) button = 1;  // Left button
        else if (actionConfig.button == 2) button = 2;  // Right button
        else if (actionConfig.button == 3) button = 4;  // Middle button
        
        keyConfig.mouseReport[1] = button;  // Button state
        keyConfig.mouseReport[2] = 0;  // X movement
        keyConfig.mouseReport[3] = 0;  // Y movement
        keyConfig.mouseReport[4] = 0;  // Wheel
        
        USBSerial.printf("  Loaded mouse config for '%s'%s using action/button format: action=%s, button=%d\n", 
                        componentId.c_str(), layerName.isEmpty() ? "" : (" in layer '" + layerName + "'").c_str(),
                        actionConfig.action.c_str(), actionConfig.button);
    }
    // Add support for mouse movement
    else if (actionConfig.action == "move") {
        keyConfig.mouseReport[1] = 0;  // No buttons pressed
        
        // Handle X movement
        if (actionConfig.action == "move-right") {
            keyConfig.mouseReport[2] = 10;  // Move right
        } else if (actionConfig.action == "move-left") {
            keyConfig.mouseReport[2] = -10;  // Move left
        } else {
            keyConfig.mouseReport[2] = 0;
        }
        
        // Handle Y movement
        if (actionConfig.action == "move-down") {
            keyConfig.mouseReport[3] = 10;  // Move down
        } else if (actionConfig.action == "move-up") {
            keyConfig.mouseReport[3] = -10;  // Move up
        } else {
            keyConfig.mouseReport[3] = 0;
        }
        
        keyConfig.mouseReport[4] = 0;  // No wheel movement
        
        USBSerial.printf("  Loaded mouse movement config for '%s'%s: action=%s\n",
                        componentId.c_str(), layerName.isEmpty() ? "" : (" in layer '" + layerName + "'").c_str(),
                        actionConfig.action.c_str());
    }
    // Then check legacy formats
    else if (!actionConfig.hidReport.empty()) {
        // Parse mouse report from legacy format
        // Skip the first byte (report ID) and copy up to 4 bytes of data
        for (size_t i = 0; i < std::min(actionConfig.hidReport.size(), (size_t)4); i++) {
            keyConfig.mouseReport[i + 1] = strtoul(actionConfig.hidReport[i].c_str(), nullptr, 16);
        }
        USBSerial.printf("  Loaded mouse config for '%s'%s using legacy hidReport format\n", 
                        componentId.c_str(), layerName.isEmpty() ? "" : (" in layer '" + layerName + "'").c_str());
    }
    // Check buttonPress as fallback for compatibility
    else if (!actionConfig.buttonPress.empty()) {
        // Parse mouse report from buttonPress field
        // Skip the first byte (report ID) and copy up to 4 bytes of data
        for (size_t i = 0; i < std::min(actionConfig.buttonPress.size(), (size_t)4); i++) {
            keyConfig.mouseReport[i + 1] = strtoul(actionConfig.buttonPress[i].c_str(), nullptr, 16);
        }
        USBSerial.printf("  Loaded mouse config for '%s'%s from legacy buttonPress field\n", 
                        componentId.c_str(), layerName.isEmpty() ? "" : (" in layer '" + layerName + "'").c_str());
    }
}

void KeyHandler::loadKeyConfiguration(const std::map<String, ActionConfig>& actions) {
    uint8_t totalKeys = componentPositions.size();
    
    USBSerial.printf("Loading key configuration for %d keys\n", totalKeys);
    USBSerial.println("==== BUTTON CONFIG DEBUG ====");
    USBSerial.printf("Actions map contains %d entries\n", actions.size());
    
    // Check if we need to extract the default layer name
    String defaultLayerName = "default-actions-layer";  // Use the name from the JSON
    
    // Look for special key with default layer name
    if (actions.find("__default_layer_name__") != actions.end()) {
        const ActionConfig& layerNameConfig = actions.at("__default_layer_name__");
        if (layerNameConfig.type == "default-layer-name" && !layerNameConfig.targetLayer.isEmpty()) {
            defaultLayerName = layerNameConfig.targetLayer;
            USBSerial.printf("Found specified default layer name: %s\n", defaultLayerName.c_str());
        }
    }
    
    // Print every action to diagnose the issue
    USBSerial.println("All actions in map:");
    for (const auto& entry : actions) {
        if (entry.first == "__default_layer_name__") continue;
        
        USBSerial.printf("  Action: ID=%s, Type=%s\n", 
                     entry.first.c_str(), 
                     entry.second.type.c_str());
    }
    
    // Debug: Print all component positions
    USBSerial.println("Component positions in matrix:");
    for (size_t i = 0; i < componentPositions.size(); i++) {
        USBSerial.printf("Position %d: ID=%s, row=%d, col=%d\n", 
                     i, 
                     componentPositions[i].id.c_str(),
                     componentPositions[i].row,
                     componentPositions[i].col);
    }
    
    // First, load the default layer configuration directly from the actions map
    std::map<String, KeyConfig> defaultConfigs;
    
    // Process all non-layer-specific actions directly for the default layer
    for (const auto& entry : actions) {
        String componentId = entry.first;
        
        // Skip the special layer name entry
        if (componentId == "__default_layer_name__") continue;
        
        // Check if this is a layer-specific action (format: "layerName:componentId")
        int colonPos = componentId.indexOf(':');
        if (colonPos <= 0) {
            // This is a default layer action
            const ActionConfig& actionConfig = entry.second;
            KeyConfig keyConfig;
            
            // Set action type based on the action config
            if (actionConfig.type == "hid") {
                keyConfig.type = ACTION_HID;
                
                // First check for direct report field (new standardized format)
                if (!actionConfig.report.empty()) {
                    // Parse HID report from standardized format
                    for (size_t i = 0; i < std::min(actionConfig.report.size(), (size_t)8); i++) {
                        keyConfig.hidReport[i] = strtoul(actionConfig.report[i].c_str(), nullptr, 16);
                    }
                    USBSerial.printf("  Loaded HID config for '%s' using standardized format\n", componentId.c_str());
                }
                // Then check for new format with buttonPress.report
                else if (actionConfig.buttonPressAction.type == "hid" && !actionConfig.buttonPressAction.report.empty()) {
                    // Parse HID report from new format
                    for (size_t i = 0; i < std::min(actionConfig.buttonPressAction.report.size(), (size_t)8); i++) {
                        keyConfig.hidReport[i] = strtoul(actionConfig.buttonPressAction.report[i].c_str(), nullptr, 16);
                    }
                    USBSerial.printf("  Loaded HID config for '%s' using nested format\n", componentId.c_str());
                }
                // Then check legacy formats
                else if (!actionConfig.hidReport.empty()) {
                    // Parse HID report from legacy format
                    for (size_t i = 0; i < std::min(actionConfig.hidReport.size(), (size_t)8); i++) {
                        keyConfig.hidReport[i] = strtoul(actionConfig.hidReport[i].c_str(), nullptr, 16);
                    }
                    USBSerial.printf("  Loaded HID config for '%s' using legacy hidReport format\n", componentId.c_str());
                }
                // Check buttonPress as fallback for compatibility
                else if (!actionConfig.buttonPress.empty()) {
                    // Parse HID report from buttonPress field
                    for (size_t i = 0; i < std::min(actionConfig.buttonPress.size(), (size_t)8); i++) {
                        keyConfig.hidReport[i] = strtoul(actionConfig.buttonPress[i].c_str(), nullptr, 16);
                    }
                    USBSerial.printf("  Loaded HID config for '%s' from legacy buttonPress field\n", componentId.c_str());
                }
            } else if (actionConfig.type == "multimedia") {
                keyConfig.type = ACTION_MULTIMEDIA;
                
                // First check for direct report field (new standardized format)
                if (!actionConfig.report.empty()) {
                    // Parse consumer report from standardized format
                    for (size_t i = 0; i < std::min(actionConfig.report.size(), (size_t)4); i++) {
                        keyConfig.consumerReport[i] = strtoul(actionConfig.report[i].c_str(), nullptr, 16);
                    }
                    USBSerial.printf("  Loaded multimedia config for '%s' using standardized format\n", componentId.c_str());
                }
                // Then check for new format with buttonPress.report
                else if (actionConfig.buttonPressAction.type == "multimedia" && !actionConfig.buttonPressAction.report.empty()) {
                    // Parse consumer report from new format
                    for (size_t i = 0; i < std::min(actionConfig.buttonPressAction.report.size(), (size_t)4); i++) {
                        keyConfig.consumerReport[i] = strtoul(actionConfig.buttonPressAction.report[i].c_str(), nullptr, 16);
                    }
                    USBSerial.printf("  Loaded multimedia config for '%s' using nested format\n", componentId.c_str());
                }
                // Then check legacy consumerReport field
                else if (!actionConfig.consumerReport.empty()) {
                    // Parse consumer report from legacy format
                    for (size_t i = 0; i < std::min(actionConfig.consumerReport.size(), (size_t)4); i++) {
                        keyConfig.consumerReport[i] = strtoul(actionConfig.consumerReport[i].c_str(), nullptr, 16);
                    }
                    USBSerial.printf("  Loaded multimedia config for '%s' using legacy consumerReport\n", componentId.c_str());
                }
                // Check buttonPress as fallback for compatibility
                else if (!actionConfig.buttonPress.empty()) {
                    // Parse consumer report from buttonPress field
                    for (size_t i = 0; i < std::min(actionConfig.buttonPress.size(), (size_t)4); i++) {
                        keyConfig.consumerReport[i] = strtoul(actionConfig.buttonPress[i].c_str(), nullptr, 16);
                    }
                    USBSerial.printf("  Loaded multimedia config for '%s' from legacy buttonPress field\n", componentId.c_str());
                }
            } else if (actionConfig.type == "mouse") {
                configureMouseAction(actionConfig, keyConfig, componentId, "");
            } else if (actionConfig.type == "macro") {
                keyConfig.type = ACTION_MACRO;
                keyConfig.macroId = actionConfig.macroId;
                USBSerial.printf("  Loaded macro config for '%s': %s\n", 
                               componentId.c_str(), keyConfig.macroId.c_str());
            } else if (actionConfig.type == "layer") {
                keyConfig.type = ACTION_LAYER;
                keyConfig.targetLayer = actionConfig.targetLayer;
                USBSerial.printf("  Loaded layer config for '%s': %s\n", 
                               componentId.c_str(), keyConfig.targetLayer.c_str());
            } else if (actionConfig.type == "cycle-layer") {
                keyConfig.type = ACTION_CYCLE_LAYER;
                USBSerial.printf("  Loaded cycle-layer config for '%s'\n", componentId.c_str());
            } else {
                keyConfig.type = ACTION_NONE;
                USBSerial.printf("  Unknown action type for '%s': %s\n", 
                               componentId.c_str(), actionConfig.type.c_str());
            }
            
            // Store the key configuration for this layer
            defaultConfigs[componentId] = keyConfig;
            USBSerial.printf("  Stored config for '%s' in default layer\n", componentId.c_str());
        }
    }
    
    // Store the default layer configuration
    layerConfigs[defaultLayerName] = defaultConfigs;
    USBSerial.printf("Stored default layer '%s' with %d configurations\n", 
                 defaultLayerName.c_str(), defaultConfigs.size());
    
    // Now process layer-specific actions
    for (const auto& entry : actions) {
        String componentId = entry.first;
        
        // Skip the special layer name entry
        if (componentId == "__default_layer_name__") continue;
        
        // Check if this is a layer-specific action (format: "layerName:componentId")
        int colonPos = componentId.indexOf(':');
        if (colonPos > 0) {
            // This is a layer-specific action
            String layerName = componentId.substring(0, colonPos);
            String actualComponentId = componentId.substring(colonPos + 1);
            const ActionConfig& actionConfig = entry.second;
            
            // Create or get the layer configuration
            std::map<String, KeyConfig>& layerConfigs = this->layerConfigs[layerName];
            
            // Create key configuration
            KeyConfig keyConfig;
            
            // Set action type based on the action config
            if (actionConfig.type == "hid") {
                keyConfig.type = ACTION_HID;
                
                // First check for direct report field (new standardized format)
                if (!actionConfig.report.empty()) {
                    // Parse HID report from standardized format
                    for (size_t i = 0; i < std::min(actionConfig.report.size(), (size_t)8); i++) {
                        keyConfig.hidReport[i] = strtoul(actionConfig.report[i].c_str(), nullptr, 16);
                    }
                    USBSerial.printf("  Loaded HID config for '%s' in layer '%s' using standardized format\n", 
                                   actualComponentId.c_str(), layerName.c_str());
                }
                // Then check for new format with buttonPress.report
                else if (actionConfig.buttonPressAction.type == "hid" && !actionConfig.buttonPressAction.report.empty()) {
                    // Parse HID report from new format
                    for (size_t i = 0; i < std::min(actionConfig.buttonPressAction.report.size(), (size_t)8); i++) {
                        keyConfig.hidReport[i] = strtoul(actionConfig.buttonPressAction.report[i].c_str(), nullptr, 16);
                    }
                    USBSerial.printf("  Loaded HID config for '%s' in layer '%s' using nested format\n", 
                                   actualComponentId.c_str(), layerName.c_str());
                }
                // Then check legacy formats
                else if (!actionConfig.hidReport.empty()) {
                    // Parse HID report from legacy format
                    for (size_t i = 0; i < std::min(actionConfig.hidReport.size(), (size_t)8); i++) {
                        keyConfig.hidReport[i] = strtoul(actionConfig.hidReport[i].c_str(), nullptr, 16);
                    }
                    USBSerial.printf("  Loaded HID config for '%s' in layer '%s' using legacy format\n", 
                                   actualComponentId.c_str(), layerName.c_str());
                }
                // Check buttonPress as fallback for compatibility
                else if (!actionConfig.buttonPress.empty()) {
                    // Parse HID report from buttonPress field
                    for (size_t i = 0; i < std::min(actionConfig.buttonPress.size(), (size_t)8); i++) {
                        keyConfig.hidReport[i] = strtoul(actionConfig.buttonPress[i].c_str(), nullptr, 16);
                    }
                    USBSerial.printf("  Loaded HID config for '%s' in layer '%s' from legacy buttonPress field\n", 
                                   actualComponentId.c_str(), layerName.c_str());
                }
            } else if (actionConfig.type == "multimedia") {
                keyConfig.type = ACTION_MULTIMEDIA;
                
                // First check for direct report field (new standardized format)
                if (!actionConfig.report.empty()) {
                    // Parse consumer report from standardized format
                    for (size_t i = 0; i < std::min(actionConfig.report.size(), (size_t)4); i++) {
                        keyConfig.consumerReport[i] = strtoul(actionConfig.report[i].c_str(), nullptr, 16);
                    }
                    USBSerial.printf("  Loaded multimedia config for '%s' in layer '%s' using standardized format\n", 
                                   actualComponentId.c_str(), layerName.c_str());
                }
                // Then check for new format with buttonPress.report
                else if (actionConfig.buttonPressAction.type == "multimedia" && !actionConfig.buttonPressAction.report.empty()) {
                    // Parse consumer report from new format
                    for (size_t i = 0; i < std::min(actionConfig.buttonPressAction.report.size(), (size_t)4); i++) {
                        keyConfig.consumerReport[i] = strtoul(actionConfig.buttonPressAction.report[i].c_str(), nullptr, 16);
                    }
                    USBSerial.printf("  Loaded multimedia config for '%s' in layer '%s' using nested format\n", 
                                   actualComponentId.c_str(), layerName.c_str());
                }
                // Then check legacy consumerReport field
                else if (!actionConfig.consumerReport.empty()) {
                    // Parse consumer report from legacy format
                    for (size_t i = 0; i < std::min(actionConfig.consumerReport.size(), (size_t)4); i++) {
                        keyConfig.consumerReport[i] = strtoul(actionConfig.consumerReport[i].c_str(), nullptr, 16);
                    }
                    USBSerial.printf("  Loaded multimedia config for '%s' in layer '%s' using legacy consumerReport\n", 
                                   actualComponentId.c_str(), layerName.c_str());
                }
                // Check buttonPress as fallback for compatibility
                else if (!actionConfig.buttonPress.empty()) {
                    // Parse consumer report from buttonPress field
                    for (size_t i = 0; i < std::min(actionConfig.buttonPress.size(), (size_t)4); i++) {
                        keyConfig.consumerReport[i] = strtoul(actionConfig.buttonPress[i].c_str(), nullptr, 16);
                    }
                    USBSerial.printf("  Loaded multimedia config for '%s' in layer '%s' from legacy buttonPress field\n", 
                                   actualComponentId.c_str(), layerName.c_str());
                }
            } else if (actionConfig.type == "mouse") {
                configureMouseAction(actionConfig, keyConfig, actualComponentId, layerName);
            } else if (actionConfig.type == "macro") {
                keyConfig.type = ACTION_MACRO;
                keyConfig.macroId = actionConfig.macroId;
                USBSerial.printf("  Loaded macro config for '%s' in layer '%s': %s\n", 
                               actualComponentId.c_str(), layerName.c_str(), keyConfig.macroId.c_str());
            } else if (actionConfig.type == "layer") {
                keyConfig.type = ACTION_LAYER;
                keyConfig.targetLayer = actionConfig.targetLayer;
                USBSerial.printf("  Loaded layer config for '%s' in layer '%s': %s\n", 
                               actualComponentId.c_str(), layerName.c_str(), keyConfig.targetLayer.c_str());
            } else if (actionConfig.type == "cycle-layer") {
                keyConfig.type = ACTION_CYCLE_LAYER;
                USBSerial.printf("  Loaded cycle-layer config for '%s' in layer '%s'\n", 
                               actualComponentId.c_str(), layerName.c_str());
            } else {
                keyConfig.type = ACTION_NONE;
                USBSerial.printf("  Unknown action type for '%s' in layer '%s': %s\n", 
                               actualComponentId.c_str(), layerName.c_str(), actionConfig.type.c_str());
            }
            
            // Store the key configuration for this layer
            layerConfigs[actualComponentId] = keyConfig;
            USBSerial.printf("  Stored config for '%s' in layer '%s'\n", 
                         actualComponentId.c_str(), layerName.c_str());
        }
    }
    
    // If we don't have a current layer yet, set it to the default
    if (!isLayerAvailable(currentLayer)) {
        currentLayer = defaultLayerName;
        USBSerial.printf("Current layer not available, setting to default: %s\n", currentLayer.c_str());
    }
    
    // List all available layers and their configs
    USBSerial.println("All layers after loading:");
    for (const auto& layer : layerConfigs) {
        USBSerial.printf("  Layer: %s with %d configurations\n", 
                     layer.first.c_str(), layer.second.size());
        
        for (const auto& config : layer.second) {
            USBSerial.printf("    Component %s: type=%d\n", 
                         config.first.c_str(), config.second.type);
        }
    }
    
    // Apply current layer configuration to actionMap
    applyLayerToActionMap(currentLayer);
    
    USBSerial.printf("Using layer: %s\n", currentLayer.c_str());
    USBSerial.println("Key configuration loaded successfully");
}

void KeyHandler::applyLayerToActionMap(const String& layerName) {
    USBSerial.printf("Applying layer '%s' to actionMap\n", layerName.c_str());
    
    if (layerConfigs.find(layerName) != layerConfigs.end()) {
        std::map<String, KeyConfig>& configs = layerConfigs[layerName];
        USBSerial.printf("Layer '%s' has %d configurations\n", 
                     layerName.c_str(), configs.size());
        
        // Clear all existing configurations first
        for (size_t i = 0; i < componentPositions.size(); i++) {
            actionMap[i].type = ACTION_NONE;
            memset(actionMap[i].hidReport, 0, sizeof(actionMap[i].hidReport));
            memset(actionMap[i].consumerReport, 0, sizeof(actionMap[i].consumerReport));
            actionMap[i].macroId = "";
            actionMap[i].targetLayer = "";
        }
        
        // Apply the layer configurations
        int configsApplied = 0;
        for (size_t i = 0; i < componentPositions.size(); i++) {
            String componentId = componentPositions[i].id;
            if (configs.find(componentId) != configs.end()) {
                actionMap[i] = configs[componentId];
                configsApplied++;
                USBSerial.printf("  Applied config for '%s' (type: %d)\n", 
                             componentId.c_str(), actionMap[i].type);
            } else {
                USBSerial.printf("  No configuration found for '%s'\n", componentId.c_str());
            }
        }
        
        USBSerial.printf("Applied %d configurations from layer '%s'\n", 
                     configsApplied, layerName.c_str());
    } else {
        USBSerial.printf("Layer '%s' does not exist\n", layerName.c_str());
    }
}

void KeyHandler::updateKeys() {
    static unsigned long lastScan = 0;
    const unsigned long scanInterval = 20; // scan every 20ms
    unsigned long now = millis();
    
    if (now - lastScan < scanInterval) return;
    lastScan = now;
    
    // Configure rows as OUTPUT and columns as INPUT_PULLUP
    for (uint8_t r = 0; r < numRows; r++) {
        pinMode(rowPins[r], OUTPUT);
        digitalWrite(rowPins[r], HIGH); // Initially high (inactive)
    }
    for (uint8_t c = 0; c < numCols; c++) {
        pinMode(colPins[c], INPUT_PULLUP);
    }
    
    // Scan all rows
    for (uint8_t r = 0; r < numRows; r++) {
        // Drive current row LOW
        digitalWrite(rowPins[r], LOW);
        delayMicroseconds(50); // Allow signals to stabilize
        
        // Check columns
        for (uint8_t c = 0; c < numCols; c++) {
            // Find component at this position
            int componentIndex = -1;
            for (size_t i = 0; i < componentPositions.size(); i++) {
                if (componentPositions[i].row == r && componentPositions[i].col == c) {
                    componentIndex = i;
                    break;
                }
            }
            
            // Skip positions without components
            if (componentIndex == -1) continue;
            
            bool currentReading = (digitalRead(colPins[c]) == LOW);
            
            // Debouncing
            if (now - lastDebounceTime[componentIndex] < DEBOUNCE_TIME) {
                continue;
            }
            
            // Process state change
            if (currentReading != keyStates[componentIndex]) {
                lastDebounceTime[componentIndex] = now;
                keyStates[componentIndex] = currentReading;
                
                String componentId = componentPositions[componentIndex].id;
                
                // Log the event
                USBSerial.printf("Key event: Row %d, Col %d, ID=%s, State=%s\n", 
                              r, c, componentId.c_str(), 
                              currentReading ? "PRESSED" : "RELEASED");
                
                // Sync LEDs
                syncLEDsWithButtons(componentId.c_str(), currentReading);
                
                // Execute action
                KeyAction action = currentReading ? KEY_PRESS : KEY_RELEASE;
                if (lastAction[componentIndex] != action) {
                    lastAction[componentIndex] = action;
                    executeAction(componentIndex, action);
                }
            }
        }
        
        // Set row back to inactive
        digitalWrite(rowPins[r], HIGH);
    }
}

void KeyHandler::executeAction(uint8_t keyIndex, KeyAction action) {
    if (keyIndex >= componentPositions.size() || !actionMap) {
        USBSerial.printf("EXECUTE ERROR: Invalid keyIndex %d (max: %d) or actionMap is null\n", 
                     keyIndex, componentPositions.size() - 1);
        return;
    }

    const KeyConfig& config = actionMap[keyIndex];
    String componentId = componentPositions[keyIndex].id;
    
    USBSerial.printf("Executing action for %s: type=%d, action=%s\n", 
                 componentId.c_str(), config.type, 
                 action == KEY_PRESS ? "PRESS" : "RELEASE");
    
    // Check if this is an encoder button - if so, route to EncoderHandler
    if (componentId.startsWith("encoder-")) {
        if (encoderHandler) {
            // Extract encoder index from ID (encoder-1 -> index 0)
            int encoderNum = componentId.substring(8).toInt();
            if (encoderNum > 0) {
                uint8_t encoderIndex = encoderNum - 1; // Convert 1-based to 0-based index
                
                // Forward button event to EncoderHandler
                USBSerial.printf("Button %s %s - forwarding to EncoderHandler\n", 
                             componentId.c_str(), 
                             action == KEY_PRESS ? "PRESSED" : "RELEASED");
                
                encoderHandler->executeEncoderButtonAction(encoderIndex, action == KEY_PRESS);
                
                // Skip normal KeyHandler processing
                return;
            }
        } else {
            USBSerial.println("ERROR: encoderHandler is null, can't forward encoder button event");
        }
    }
    
    // Normal KeyHandler action processing for non-encoder buttons
    if (config.type == ACTION_NONE) {
        USBSerial.printf("DEBUG: No action configured for %s (layer: %s)\n", 
                     componentId.c_str(), currentLayer.c_str());
        // Print available layers and current configs for debugging
        std::vector<String> layers = getAvailableLayers();
        USBSerial.printf("Available layers (%d): ", layers.size());
        for (const auto& layer : layers) {
            USBSerial.printf("%s, ", layer.c_str());
        }
        USBSerial.println();
        
        // Print current layer configs
        if (layerConfigs.find(currentLayer) != layerConfigs.end()) {
            std::map<String, KeyConfig>& configs = layerConfigs[currentLayer];
            USBSerial.printf("Current layer '%s' has %d configurations:\n", 
                         currentLayer.c_str(), configs.size());
            for (const auto& config : configs) {
                USBSerial.printf("  %s: type=%d\n", config.first.c_str(), config.second.type);
            }
        }
    }
    
    switch (config.type) {
        case ACTION_HID:
            // Send HID report
            if (action == KEY_PRESS) {
                USBSerial.print("HID Report: ");
                for (int i = 0; i < 8; i++) {
                    USBSerial.printf("%02X ", config.hidReport[i]);
                }
                USBSerial.println();
                
                if (hidHandler) {
                    // Process modifier keys (byte 0)
                    if (config.hidReport[0] != 0) {
                        for (uint8_t i = 0; i < 8; i++) {
                            if (config.hidReport[0] & (1 << i)) {
                                // Convert bit position to modifier key code (0xE0-0xE7)
                                uint8_t modifierKey = KEY_LEFT_CTRL + i;
                                hidHandler->pressKey(modifierKey);
                            }
                        }
                    }
                    
                    // Process regular keys (bytes 2-7)
                    for (int i = 2; i < 8; i++) {
                        if (config.hidReport[i] != 0) {
                            hidHandler->pressKey(config.hidReport[i]);
                        }
                    }
                    
                    USBSerial.println("HID key press processed");
                }
            } else if (action == KEY_RELEASE) {
                if (hidHandler) {
                    // Release modifier keys
                    if (config.hidReport[0] != 0) {
                        for (uint8_t i = 0; i < 8; i++) {
                            if (config.hidReport[0] & (1 << i)) {
                                // Convert bit position to modifier key code (0xE0-0xE7)
                                uint8_t modifierKey = KEY_LEFT_CTRL + i;
                                hidHandler->releaseKey(modifierKey);
                            }
                        }
                    }
                    
                    // Release regular keys
                    for (int i = 2; i < 8; i++) {
                        if (config.hidReport[i] != 0) {
                            hidHandler->releaseKey(config.hidReport[i]);
                        }
                    }
                    
                    USBSerial.println("HID key release processed");
                }
            }
            break;
            
        case ACTION_MULTIMEDIA:
            // Send consumer report
            if (action == KEY_PRESS) {
                USBSerial.print("Consumer Report: ");
                for (int i = 0; i < 4; i++) {
                    USBSerial.printf("%02X ", config.consumerReport[i]);
                }
                USBSerial.println();
                
                if (hidHandler) {
                    bool sent = hidHandler->sendConsumerReport(config.consumerReport);
                    USBSerial.printf("Consumer report sent: %s\n", sent ? "SUCCESS" : "FAILED");
                }
            } else if (action == KEY_RELEASE) {
                if (hidHandler) {
                    hidHandler->sendEmptyConsumerReport();
                }
            }
            break;
            
        case ACTION_MOUSE:
            if (action == KEY_PRESS) {
                // Format mouse report according to HID descriptor:
                // - Report ID (1 byte)
                // - 5 buttons (1 bit each)
                // - 3 padding bits
                // - X, Y, Wheel (8 bits each, signed)
                uint8_t mouseReport[HID_MOUSE_REPORT_SIZE] = {1}; // Report ID 1
                
                // Set button state (using first 5 bits)
                if (config.mouseReport[1] == 1) {
                    mouseReport[1] = 0x01;  // Left button
                } else if (config.mouseReport[1] == 2) {
                    mouseReport[1] = 0x02;  // Right button
                } else if (config.mouseReport[1] == 4) {
                    mouseReport[1] = 0x04;  // Middle button
                } else {
                    mouseReport[1] = config.mouseReport[1];  // Use the value directly if not a standard button
                }
                
                // Copy movement and wheel values
                mouseReport[2] = config.mouseReport[2];  // X
                mouseReport[3] = config.mouseReport[3];  // Y
                mouseReport[4] = config.mouseReport[4];  // Wheel
                
                USBSerial.print("Mouse Report: ");
                for (int i = 0; i < HID_MOUSE_REPORT_SIZE; i++) {
                    USBSerial.printf("%02X ", mouseReport[i]);
                }
                USBSerial.println();
                
                if (hidHandler) {
                    bool sent = hidHandler->sendMouseReport(mouseReport, HID_MOUSE_REPORT_SIZE);
                    USBSerial.printf("Mouse report sent: %s\n", sent ? "SUCCESS" : "FAILED");
                    
                    // Add a small delay for click detection
                    if (sent && mouseReport[1] != 0) {  // Only delay if a button was pressed
                        delay(50);  // 50ms delay
                    }
                }
            } else if (action == KEY_RELEASE) {
                if (hidHandler) {
                    hidHandler->sendEmptyMouseReport();
                }
            }
            break;
            
        case ACTION_MACRO:
            // Execute macro
            if (action == KEY_PRESS && !config.macroId.isEmpty()) {
                USBSerial.printf("Executing macro: %s\n", config.macroId.c_str());
                if (macroHandler) {
                    // Use MacroHandler instead of HIDHandler for macro execution
                    bool success = macroHandler->executeMacro(config.macroId);
                    USBSerial.printf("Macro execution %s\n", success ? "started" : "failed");
                } else {
                    USBSerial.println("MacroHandler not initialized");
                }
            }
            break;
            
        case ACTION_LAYER:
            // Switch to target layer
            if (action == KEY_PRESS && !config.targetLayer.isEmpty()) {
                USBSerial.printf("Switching to layer: %s\n", config.targetLayer.c_str());
                bool success = switchToLayer(config.targetLayer);
                
                USBSerial.printf("Layer switch %s\n", success ? "succeeded" : "failed");
            }
            break;
            
        case ACTION_CYCLE_LAYER:
            // Cycle to next layer
            if (action == KEY_PRESS) {
                USBSerial.println("Cycling to next layer");
                bool success = cycleToNextLayer();
                
                USBSerial.printf("Layer cycle %s\n", success ? "succeeded" : "failed");
            }
            break;
            
        default:
            USBSerial.printf("No action configured for component '%s' (key index %d)\n", 
                         componentPositions[keyIndex].id.c_str(), keyIndex);
            break;
    }
}

void KeyHandler::printKeyboardState() {
    USBSerial.println("\n--- Keyboard Matrix State ---");
    for (size_t i = 0; i < componentPositions.size(); i++) {
        const ComponentPosition& pos = componentPositions[i];
        USBSerial.printf("%s at [%d,%d]: %s\n", 
                      pos.id.c_str(), 
                      pos.row, 
                      pos.col, 
                      keyStates[i] ? "PRESSED" : "RELEASED");
    }
    USBSerial.println("----------------------------\n");
}

void KeyHandler::diagnostics() {
    static unsigned long lastDiagTime = 0;
    const unsigned long diagInterval = 5000; // Every 5 seconds
    
    unsigned long now = millis();
    if (now - lastDiagTime >= diagInterval) {
        lastDiagTime = now;
        printKeyboardState();
    }
}

bool KeyHandler::switchToLayer(const String& layerName) {
    // Check if the layer is available
    if (!isLayerAvailable(layerName)) {
        USBSerial.printf("Layer '%s' is not available\n", layerName.c_str());
        return false;
    }
    
    USBSerial.printf("Switching from layer '%s' to '%s'\n", currentLayer.c_str(), layerName.c_str());
    
    // Store the current cycle-layer button configuration so we can preserve it
    KeyConfig cycleLayerConfig;
    bool foundCycleLayer = false;
    
    for (size_t i = 0; i < componentPositions.size(); i++) {
        if (actionMap[i].type == ACTION_CYCLE_LAYER) {
            cycleLayerConfig = actionMap[i];
            foundCycleLayer = true;
            USBSerial.printf("Found cycle-layer button at position %d (id: %s)\n", 
                         i, componentPositions[i].id.c_str());
            break;
        }
    }
    
    // Update the current layer
    currentLayer = layerName;
    
    // Apply the new layer configuration using our helper method
    applyLayerToActionMap(currentLayer);
    
    // Make sure the cycle-layer button is preserved if it was present in the source layer
    if (foundCycleLayer) {
        for (size_t i = 0; i < componentPositions.size(); i++) {
            if (componentPositions[i].id == "button-1") {  // Typically button-1 is the cycle button
                actionMap[i] = cycleLayerConfig;
                USBSerial.printf("Preserved cycle-layer functionality on button %s\n", 
                               componentPositions[i].id.c_str());
                break;
            }
        }
    } else {
        // If no cycle layer was found, add one to button-1
        for (size_t i = 0; i < componentPositions.size(); i++) {
            if (componentPositions[i].id == "button-1") {
                actionMap[i].type = ACTION_CYCLE_LAYER;
                USBSerial.printf("Added cycle-layer functionality to button %s\n", 
                               componentPositions[i].id.c_str());
                
                // Also add it to the layer configuration for persistence
                if (layerConfigs.find(currentLayer) != layerConfigs.end()) {
                    layerConfigs[currentLayer]["button-1"].type = ACTION_CYCLE_LAYER;
                }
                break;
            }
        }
    }
    
    // Save the layer change to persist across reboots
    saveCurrentLayer();
    
    return true;
}

bool KeyHandler::isLayerAvailable(const String& layerName) const {
    return layerConfigs.find(layerName) != layerConfigs.end();
}

std::vector<String> KeyHandler::getAvailableLayers() const {
    std::vector<String> layers;
    for (const auto& layer : layerConfigs) {
        layers.push_back(layer.first);
    }
    return layers;
}

bool KeyHandler::saveCurrentLayer() {
    DynamicJsonDocument doc(256);
    doc["currentLayer"] = currentLayer;
    
    String jsonStr;
    serializeJson(doc, jsonStr);
    
    File file = LittleFS.open("/config/current_layer.json", "w");
    if (file) {
        file.print(jsonStr);
        file.close();
        USBSerial.printf("Current layer '%s' saved to LittleFS\n", currentLayer.c_str());
        return true;
    } else {
        USBSerial.println("Failed to save current layer to LittleFS");
        return false;
    }
}

bool KeyHandler::loadCurrentLayer() {
    if (!LittleFS.exists("/config/current_layer.json")) {
        // Default to the first available layer if none is specified
        std::vector<String> layers = getAvailableLayers();
        if (!layers.empty()) {
            currentLayer = layers[0];
            USBSerial.printf("No saved layer found, using first available: %s\n", currentLayer.c_str());
        } else {
            currentLayer = "default-actions-layer"; // Fall back to known default
            USBSerial.println("No layers available, using built-in default");
        }
        
        // Save this choice for next time
        saveCurrentLayer();
        return true;
    }
    
    File file = LittleFS.open("/config/current_layer.json", "r");
    if (!file) {
        USBSerial.println("Failed to open current_layer.json file");
        return false;
    }
    
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    
    if (error) {
        USBSerial.printf("Failed to parse layer file: %s\n", error.c_str());
        return false;
    }
    
    String savedLayer = doc["currentLayer"].as<String>();
    if (savedLayer.isEmpty()) {
        USBSerial.println("No layer specified in file");
        return false;
    }
    
    USBSerial.printf("Loaded saved layer name: %s\n", savedLayer.c_str());
    
    // Only switch if the layer is available, otherwise we'll keep the default
    // until the configurations are loaded
    currentLayer = savedLayer;
    USBSerial.printf("Set current layer to: %s\n", currentLayer.c_str());
    return true;
}

void KeyHandler::setCpuFrequencyMhz(uint8_t mhz) {
    // Implementation of setCpuFrequencyMhz function
}

void KeyHandler::performHighPowerTask() {
    setCpuFrequencyMhz(240);
    // Perform intensive task
    setCpuFrequencyMhz(80);
}

bool KeyHandler::assignMacroToButton(const String& buttonId, const String& macroId) {
    // Find the component in the componentPositions vector
    for (size_t i = 0; i < componentPositions.size(); i++) {
        if (componentPositions[i].id == buttonId) {
            // Update the key binding for this component
            KeyConfig& config = actionMap[i];
            config.type = ACTION_MACRO;
            config.macroId = macroId;
            
            // Save the updated configuration
            saveCurrentLayer();
            
            USBSerial.printf("Assigned macro %s to button %s\n", macroId.c_str(), buttonId.c_str());
            return true;
        }
    }
    
    USBSerial.printf("Button %s not found\n", buttonId.c_str());
    return false;
}

void KeyHandler::displayKeyConfig(const KeyConfig& config) {
    USBSerial.print("  Type: ");
    
    switch (config.type) {
        case ACTION_NONE:
            USBSerial.println("None");
            break;
            
        case ACTION_HID:
            USBSerial.println("HID");
            USBSerial.print("  Report: [");
            for (int i = 0; i < 8; i++) {
                USBSerial.printf("0x%02X", config.hidReport[i]);
                if (i < 7) USBSerial.print(", ");
            }
            USBSerial.println("]");
            break;
            
        case ACTION_MULTIMEDIA:
            USBSerial.println("Multimedia");
            USBSerial.print("  Report: [");
            for (int i = 0; i < 4; i++) {
                USBSerial.printf("0x%02X", config.consumerReport[i]);
                if (i < 3) USBSerial.print(", ");
            }
            USBSerial.println("]");
            break;
            
        case ACTION_MOUSE:
            USBSerial.println("Mouse");
            USBSerial.print("  Report: [");
            for (int i = 0; i < 4; i++) {
                USBSerial.printf("0x%02X", config.mouseReport[i]);
                if (i < 3) USBSerial.print(", ");
            }
            USBSerial.println("]");
            break;
            
        default:
            USBSerial.println("Unknown");
            break;
    }
}

bool KeyHandler::cycleToNextLayer() {
    std::vector<String> layers = getAvailableLayers();
    
    USBSerial.printf("Available layers (%d): ", layers.size());
    for (const auto& layer : layers) {
        USBSerial.printf("%s, ", layer.c_str());
    }
    USBSerial.println();
    
    if (layers.empty()) {
        USBSerial.println("No layers available to cycle to");
        return false;
    }

    // Find current layer index
    auto it = std::find(layers.begin(), layers.end(), currentLayer);
    if (it == layers.end()) {
        USBSerial.printf("Current layer '%s' not found in available layers\n", currentLayer.c_str());
        return switchToLayer(layers[0]); // If current layer not found, switch to first layer
    }

    // Get next layer index
    size_t currentIndex = std::distance(layers.begin(), it);
    size_t nextIndex = (currentIndex + 1) % layers.size();
    
    String nextLayer = layers[nextIndex];
    USBSerial.printf("Cycling from layer '%s' (index %d) to next layer '%s' (index %d)\n", 
                 currentLayer.c_str(), currentIndex, nextLayer.c_str(), nextIndex);
                 
    return switchToLayer(nextLayer);
}

String KeyHandler::getNextLayerName() const {
    std::vector<String> layers = getAvailableLayers();
    
    USBSerial.printf("Available layers (%d): ", layers.size());
    for (const auto& layer : layers) {
        USBSerial.printf("%s, ", layer.c_str());
    }
    USBSerial.println();
    
    if (layers.empty()) {
        USBSerial.println("No layers available");
        return "";
    }

    // Find current layer index
    auto it = std::find(layers.begin(), layers.end(), currentLayer);
    if (it == layers.end()) {
        USBSerial.printf("Current layer '%s' not found in available layers\n", currentLayer.c_str());
        return layers[0]; // If current layer not found, return first layer
    }

    // Get next layer index
    size_t currentIndex = std::distance(layers.begin(), it);
    size_t nextIndex = (currentIndex + 1) % layers.size();
    
    USBSerial.printf("Current layer index: %d, Next layer index: %d\n", 
                 currentIndex, nextIndex);
    
    return layers[nextIndex];
}

void KeyHandler::printKeyConfig(uint8_t keyIndex) {
    if (keyIndex >= componentPositions.size() || !actionMap) {
        USBSerial.printf("Error: Invalid key index %d\n", keyIndex);
        return;
    }

    const KeyConfig& config = actionMap[keyIndex];
    String componentId = componentPositions[keyIndex].id;
    
    USBSerial.printf("Key %s Configuration:\n", componentId.c_str());
    USBSerial.print("  Type: ");
    
    switch (config.type) {
        case ACTION_HID:
            USBSerial.println("HID");
            USBSerial.print("  Report: [");
            for (int i = 0; i < 8; i++) {
                USBSerial.printf("0x%02X", config.hidReport[i]);
                if (i < 7) USBSerial.print(", ");
            }
            USBSerial.println("]");
            break;
            
        case ACTION_MULTIMEDIA:
            USBSerial.println("Multimedia");
            USBSerial.print("  Report: [");
            for (int i = 0; i < 4; i++) {
                USBSerial.printf("0x%02X", config.consumerReport[i]);
                if (i < 3) USBSerial.print(", ");
            }
            USBSerial.println("]");
            break;
            
        case ACTION_MOUSE:
            USBSerial.println("Mouse");
            USBSerial.print("  Report: [");
            for (int i = 0; i < 4; i++) {
                USBSerial.printf("0x%02X", config.mouseReport[i]);
                if (i < 3) USBSerial.print(", ");
            }
            USBSerial.println("]");
            break;
            
        case ACTION_MACRO:
            USBSerial.println("Macro");
            USBSerial.printf("  Macro ID: %s\n", config.macroId.c_str());
            break;
            
        case ACTION_LAYER:
            USBSerial.println("Layer");
            USBSerial.printf("  Target Layer: %s\n", config.targetLayer.c_str());
            break;
            
        case ACTION_CYCLE_LAYER:
            USBSerial.println("Cycle Layer");
            break;
            
        default:
            USBSerial.println("Unknown");
            break;
    }
}