// KeyHandler.cpp

#include "KeyHandler.h"
#include "HIDHandler.h"
#include "ConfigManager.h"
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <USBCDC.h>
#include <algorithm> // For std::sort

extern USBCDC USBSerial;

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
    
    // Clear all configurations
    for (uint8_t i = 0; i < totalKeys; i++) {
        actionMap[i].type = ACTION_NONE;
        memset(actionMap[i].hidReport, 0, sizeof(actionMap[i].hidReport));
        memset(actionMap[i].consumerReport, 0, sizeof(actionMap[i].consumerReport));
        actionMap[i].macroId = "";
        actionMap[i].targetLayer = "";
    }
    
    USBSerial.printf("Loading key configuration for %d keys\n", totalKeys);
    
    // Load configurations for each component
    for (uint8_t i = 0; i < totalKeys; i++) {
        String id = componentPositions[i].id;
        
        // Check if we have actions for this component
        if (actions.find(id) != actions.end()) {
            const ActionConfig& ac = actions.at(id);
            
            USBSerial.printf("Configuring %s with action type: %s\n", 
                          id.c_str(), ac.type.c_str());
            
            if (ac.type == "hid") {
                actionMap[i].type = ACTION_HID;
                
                if (!ac.hidReport.empty()) {
                    if (HIDHandler::hexReportToBinary(ac.hidReport, actionMap[i].hidReport, 8)) {
                        USBSerial.printf("HID report for %s: ", id.c_str());
                        for (int j = 0; j < 8; j++) {
                            USBSerial.printf("%02X ", actionMap[i].hidReport[j]);
                        }
                        USBSerial.println();
                    } else {
                        USBSerial.printf("Failed to convert HID report for %s\n", id.c_str());
                    }
                }
            } 
            else if (ac.type == "multimedia") {
                actionMap[i].type = ACTION_MULTIMEDIA;
                
                if (!ac.consumerReport.empty()) {
                    if (HIDHandler::hexReportToBinary(ac.consumerReport, actionMap[i].consumerReport, 4)) {
                        USBSerial.printf("Consumer report for %s: ", id.c_str());
                        for (int j = 0; j < 4; j++) {
                            USBSerial.printf("%02X ", actionMap[i].consumerReport[j]);
                        }
                        USBSerial.println();
                    } else {
                        USBSerial.printf("Failed to convert Consumer report for %s\n", id.c_str());
                    }
                }
            }
            else if (ac.type == "macro") {
                actionMap[i].type = ACTION_MACRO;
                actionMap[i].macroId = ac.macroId;
            }
            else if (ac.type == "layer") {
                actionMap[i].type = ACTION_LAYER;
                actionMap[i].targetLayer = ac.targetLayer;
            }
        } else {
            USBSerial.printf("No action configured for %s\n", id.c_str());
        }
    }
    
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
                if (hidHandler) {
                    hidHandler->executeMacro(config.macroId.c_str());
                }
            }
            break;
            
        case ACTION_LAYER:
            if (action == KEY_PRESS && !config.targetLayer.isEmpty()) {
                // Layer switching code goes here
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