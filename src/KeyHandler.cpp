// KeyHandler.cpp

#include "KeyHandler.h"
#include "HIDHandler.h"
#include "MacroHandler.h"
#include "ConfigManager.h"
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <USBCDC.h>
#include <algorithm> // For std::sort

extern USBCDC USBSerial;
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
        
        // Load the saved layer if available
        loadCurrentLayer();
    } 
    catch (...) {
        USBSerial.println("Error in KeyHandler::begin()");
    }
}

uint8_t KeyHandler::getTotalKeys() {
    return componentPositions.size();
}

void KeyHandler::loadKeyConfiguration(const std::map<String, ActionConfig>& actions) {
    uint8_t totalKeys = componentPositions.size();
    
    USBSerial.printf("Loading key configuration for %d keys to the default layer\n", totalKeys);
    
    // Load configuration to default layer
    loadLayerConfiguration("default", actions);
    
    // If we don't have a current layer yet, set it to default
    if (!isLayerAvailable(currentLayer)) {
        currentLayer = "default";
    }
    
    // Apply current layer configuration to actionMap
    if (layerConfigs.find(currentLayer) != layerConfigs.end()) {
        std::map<String, KeyConfig>& configs = layerConfigs[currentLayer];
        for (size_t i = 0; i < componentPositions.size(); i++) {
            String componentId = componentPositions[i].id;
            if (configs.find(componentId) != configs.end()) {
                actionMap[i] = configs[componentId];
            } else {
                // Clear configuration if not found in layer
                actionMap[i].type = ACTION_NONE;
                memset(actionMap[i].hidReport, 0, sizeof(actionMap[i].hidReport));
                memset(actionMap[i].consumerReport, 0, sizeof(actionMap[i].consumerReport));
                actionMap[i].macroId = "";
                actionMap[i].targetLayer = "";
            }
        }
    } else {
        USBSerial.printf("No configuration found for layer '%s', using default\n", currentLayer.c_str());
    }
    
    USBSerial.printf("Using layer: %s\n", currentLayer.c_str());
    USBSerial.println("Key configuration loaded successfully");
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
        USBSerial.printf("Invalid key index: %d\n", keyIndex);
        return;
    }
    
    const ComponentPosition& pos = componentPositions[keyIndex];
    const KeyConfig& config = actionMap[keyIndex];
    
    // Print detailed action info
    USBSerial.printf("KeyHandler: Executing action for %s at [%d,%d], type=%d (%s), action=%s\n", 
                  pos.id.c_str(), pos.row, pos.col, config.type,
                  config.type == ACTION_HID ? "HID" : 
                  config.type == ACTION_MULTIMEDIA ? "MULTIMEDIA" : 
                  config.type == ACTION_MACRO ? "MACRO" : 
                  config.type == ACTION_LAYER ? "LAYER" : "UNKNOWN",
                  action == KEY_PRESS ? "PRESS" : "RELEASE");
    
    switch (config.type) {
        case ACTION_HID:
            if (action == KEY_PRESS) {
                USBSerial.print("HID Report: ");
                for (int i = 0; i < 8; i++) {
                    USBSerial.printf("%02X ", config.hidReport[i]);
                }
                USBSerial.println();
                
                if (hidHandler) {
                    bool sent = hidHandler->sendKeyboardReport(config.hidReport);
                    USBSerial.printf("HID report sent: %s\n", sent ? "SUCCESS" : "FAILED");
                }
            } else if (action == KEY_RELEASE) {
                if (hidHandler) {
                    hidHandler->sendEmptyKeyboardReport();
                }
            }
            break;
            
        case ACTION_MULTIMEDIA:
            if (action == KEY_PRESS) {
                USBSerial.print("Multimedia Report: ");
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
            
        case ACTION_MACRO:
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
            if (action == KEY_PRESS && !config.targetLayer.isEmpty()) {
                USBSerial.printf("Switching to layer: %s\n", config.targetLayer.c_str());
                bool success = switchToLayer(config.targetLayer);
                
                USBSerial.printf("Layer switch %s\n", success ? "succeeded" : "failed");
            }
            break;
            
        case ACTION_NONE:
        default:
            USBSerial.printf("No action configured for key %d\n", keyIndex);
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
    
    // Save the current layer configs if needed
    if (layerConfigs.find(currentLayer) == layerConfigs.end()) {
        // Create a mapping from component ID to current config
        std::map<String, KeyConfig> currentConfigs;
        for (size_t i = 0; i < componentPositions.size(); i++) {
            currentConfigs[componentPositions[i].id] = actionMap[i];
        }
        layerConfigs[currentLayer] = currentConfigs;
    }
    
    // Update the current layer
    currentLayer = layerName;
    
    // Apply the new layer configuration
    std::map<String, KeyConfig>& configs = layerConfigs[layerName];
    for (size_t i = 0; i < componentPositions.size(); i++) {
        String componentId = componentPositions[i].id;
        if (configs.find(componentId) != configs.end()) {
            actionMap[i] = configs[componentId];
        }
    }
    
    // Save the layer change to persist across reboots
    saveCurrentLayer();
    
    return true;
}

bool KeyHandler::loadLayerConfiguration(const String& layerName, const std::map<String, ActionConfig>& actions) {
    // Create a new layer configuration
    std::map<String, KeyConfig> configs;
    
    // Process each action and store in the layer
    for (const auto& actionPair : actions) {
        const String& componentId = actionPair.first;
        const ActionConfig& actionConfig = actionPair.second;
        
        KeyConfig keyConfig;
        
        // Set action type based on the action config
        if (actionConfig.type == "hid") {
            keyConfig.type = ACTION_HID;
            
            // Parse HID report
            for (size_t i = 0; i < std::min(actionConfig.hidReport.size(), (size_t)8); i++) {
                keyConfig.hidReport[i] = strtoul(actionConfig.hidReport[i].c_str(), nullptr, 16);
            }
        } else if (actionConfig.type == "multimedia") {
            keyConfig.type = ACTION_MULTIMEDIA;
            
            // Parse consumer report
            for (size_t i = 0; i < std::min(actionConfig.consumerReport.size(), (size_t)4); i++) {
                keyConfig.consumerReport[i] = strtoul(actionConfig.consumerReport[i].c_str(), nullptr, 16);
            }
        } else if (actionConfig.type == "macro") {
            keyConfig.type = ACTION_MACRO;
            keyConfig.macroId = actionConfig.macroId;
        } else if (actionConfig.type == "layer") {
            keyConfig.type = ACTION_LAYER;
            keyConfig.targetLayer = actionConfig.targetLayer;
        } else {
            keyConfig.type = ACTION_NONE;
        }
        
        // Store the key configuration
        configs[componentId] = keyConfig;
    }
    
    // Store the layer configuration
    layerConfigs[layerName] = configs;
    
    // If this is the current layer, apply it now
    if (layerName == currentLayer) {
        for (size_t i = 0; i < componentPositions.size(); i++) {
            String componentId = componentPositions[i].id;
            if (configs.find(componentId) != configs.end()) {
                actionMap[i] = configs[componentId];
            }
        }
    }
    
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

void KeyHandler::saveCurrentLayer() {
    DynamicJsonDocument doc(1024);
    doc["current_layer"] = currentLayer;
    
    File file = SPIFFS.open("/config/current_layer.json", "w");
    if (file) {
        serializeJson(doc, file);
        file.close();
        USBSerial.printf("Current layer '%s' saved to SPIFFS\n", currentLayer.c_str());
    } else {
        USBSerial.println("Failed to save current layer to SPIFFS");
    }
}

bool KeyHandler::loadCurrentLayer() {
    if (!SPIFFS.exists("/config/current_layer.json")) {
        USBSerial.println("No saved layer found, using default");
        return false;
    }
    
    File file = SPIFFS.open("/config/current_layer.json", "r");
    if (!file) {
        USBSerial.println("Failed to open layer file");
        return false;
    }
    
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    
    if (error) {
        USBSerial.printf("Failed to parse layer file: %s\n", error.c_str());
        return false;
    }
    
    String savedLayer = doc["current_layer"].as<String>();
    if (savedLayer.isEmpty()) {
        USBSerial.println("No layer specified in file");
        return false;
    }
    
    // Only switch if the layer is available
    if (isLayerAvailable(savedLayer)) {
        currentLayer = savedLayer;
        USBSerial.printf("Loaded saved layer: %s\n", currentLayer.c_str());
        return true;
    } else {
        USBSerial.printf("Saved layer '%s' not available\n", savedLayer.c_str());
        return false;
    }
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