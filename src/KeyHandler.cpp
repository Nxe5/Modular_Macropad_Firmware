// KeyHandler.cpp

#include "KeyHandler.h"
#include "HIDHandler.h"
#include "ConfigManager.h"
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <USBCDC.h>

extern USBCDC USBSerial;

KeyHandler* keyHandler = nullptr;

KeyHandler::KeyHandler(uint8_t rows, uint8_t cols, char** keyMapping, 
                       uint8_t* rows_pins, uint8_t* colPins) 
{
    // Initialize member variables in the constructor body
    numRows = rows;
    numCols = cols;
    keyMap = nullptr;
    rowPins = nullptr;
    this->colPins = nullptr;
    keypad = nullptr;
    actionMap = nullptr;

    // Strict bounds checking
    if (rows > 10 || cols > 10) {
        USBSerial.println("Error: Matrix dimensions too large");
        return;
    }

    // Validate input pointers
    if (!keyMapping || !rows_pins || !colPins) {
        USBSerial.println("Error: Null input pointers");
        return;
    }

    // Use placement new or carefully managed dynamic allocation
    try {
        // Allocate key map with careful bounds checking
        keyMap = new char*[rows];
        for (uint8_t i = 0; i < rows; i++) {
            keyMap[i] = new char[cols];
            
            // Deep copy with bounds checking
            for (uint8_t j = 0; j < cols; j++) {
                keyMap[i][j] = (keyMapping[i][j] != '\0') ? keyMapping[i][j] : 'X';
            }
        }
        
        // Allocate pin arrays
        rowPins = new uint8_t[rows];
        this->colPins = new uint8_t[cols];
        
        // Safely copy pin arrays
        memcpy(rowPins, rows_pins, rows * sizeof(uint8_t));
        memcpy(this->colPins, colPins, cols * sizeof(uint8_t));
        
        // Create flattened keymap for Keypad library
        char* flatKeymap = new char[rows * cols];
        
        for (uint8_t i = 0; i < rows; i++) {
            for (uint8_t j = 0; j < cols; j++) {
                flatKeymap[i * cols + j] = keyMap[i][j];
            }
        }
        
        // Initialize keypad with careful error handling
        keypad = new Keypad(flatKeymap, rowPins, this->colPins, rows, cols);
        
        // Configure keypad with safe defaults
        keypad->setDebounceTime(50);
        keypad->setHoldTime(500);
        
        // Allocate action map with careful sizing
        uint8_t totalKeys = getTotalKeys();
        actionMap = new KeyConfig[totalKeys];
        
        // Zero-initialize key states
        memset(keyStates, 0, sizeof(keyStates));
        memset(lastDebounceTime, 0, sizeof(lastDebounceTime));
        memset(lastAction, 0, sizeof(lastAction));
        
        USBSerial.printf("KeyHandler initialized successfully. Total keys: %d\n", totalKeys);
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
    // Free dynamically allocated memory
    if (keyMap) {
        for (uint8_t i = 0; i < numRows; i++) {
            delete[] keyMap[i];
        }
        delete[] keyMap;
        keyMap = nullptr;
    }
    
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
}

void KeyHandler::begin() {
    if (!keypad) {
        USBSerial.println("Error: Keypad not initialized in begin()");
        return;
    }
    
    try {
        // NOTE: We're not changing pin modes here anymore since they're already
        // configured in configurePinModes() - this prevents conflicts
        
        USBSerial.println("KeyHandler initialization complete - using configuration from configurePinModes()");
        
        // Configure the keypad library's settings
        if (keypad) {
            keypad->setDebounceTime(DEBOUNCE_TIME);
            keypad->setHoldTime(500);
            USBSerial.println("Keypad library configured with debounce time: " + String(DEBOUNCE_TIME) + "ms");
        }
    } 
    catch (...) {
        USBSerial.println("Error in KeyHandler::begin()");
    }
}

char KeyHandler::getKey() {
    if (!keypad) return NO_KEY;
    
    char key = keypad->getKey();
    return (key == 'X') ? NO_KEY : key;
}

void KeyHandler::executeAction(uint8_t keyIndex, KeyAction action) {
    if (keyIndex >= getTotalKeys() || !actionMap) {
        return;
    }
    
    const KeyConfig& config = actionMap[keyIndex];
    
    // Enhanced debug information
    USBSerial.printf("KeyHandler: Executing action for key %d, type=%d (%s), action=%s\n", 
                  keyIndex, config.type, 
                  config.type == ACTION_HID ? "HID" : 
                  config.type == ACTION_MULTIMEDIA ? "MULTIMEDIA" : 
                  config.type == ACTION_MACRO ? "MACRO" : 
                  config.type == ACTION_LAYER ? "LAYER" : "UNKNOWN",
                  action == KEY_PRESS ? "PRESS" : "RELEASE");
    
    // Process based on action type and key action (press/release)
    switch (config.type) {
            
        case ACTION_MULTIMEDIA:
            if (action == KEY_PRESS) {
                // More detailed debugging
                USBSerial.print("Multimedia Report (Press): ");
                for (int i = 0; i < 4; i++) {
                    USBSerial.printf("%02X ", config.consumerReport[i]);
                }
                USBSerial.println();
                USBSerial.println("Sending to HIDHandler::sendConsumerReport...");

                // For key press, send the configured consumer report
                if (hidHandler) {
                    bool sent = hidHandler->sendConsumerReport(config.consumerReport);
                    USBSerial.printf("Consumer report sent: %s\n", sent ? "SUCCESS" : "FAILED");
                } else {
                    USBSerial.println("ERROR: hidHandler is NULL!");
                }
            }
            break;  // Add this break statement to prevent fall-through

        case ACTION_MACRO:
            if (action == KEY_PRESS && !config.macroId.isEmpty()) {
                // Only execute macro on key press
                USBSerial.printf("Executing macro: %s\n", config.macroId.c_str());
                
                if (hidHandler) {
                    hidHandler->executeMacro(config.macroId.c_str());
                }
            }
            break;
            
        case ACTION_LAYER:
            // Layer switching would be handled by your layer management logic
            if (action == KEY_PRESS && !config.targetLayer.isEmpty()) {
                USBSerial.printf("Switching to layer: %s\n", config.targetLayer.c_str());
                // Call your layer switching function here
                // switchToLayer(config.targetLayer);
            }
            break;
            
        case ACTION_NONE:
        default:
            // No action configured or unknown action type
            USBSerial.printf("No action configured for key %d\n", keyIndex);
            break;
    }
}


bool KeyHandler::isKeyPressed(char key) {
    if (!keypad) return false;
    return keypad->isPressed(key) && key != 'X';
}

uint8_t KeyHandler::getTotalKeys() {
    uint8_t count = 0;
    for (uint8_t r = 0; r < numRows; r++) {
        for (uint8_t c = 0; c < numCols; c++) {
            if (keyMap[r][c] != 'X') {
                count++;
            }
        }
    }
    return count;
}

// Updated method for KeyHandler class to properly handle different JSON formats
// Add this to your KeyHandler.cpp file

void KeyHandler::loadKeyConfiguration(const std::map<String, ActionConfig>& actions) {
    uint8_t totalKeys = getTotalKeys();
    
    // Extensive logging and error checking
    USBSerial.printf("Loading key configuration for %d keys\n", totalKeys);
    
    for (uint8_t i = 0; i < totalKeys; i++) {
        String buttonId = "button-" + String(i + 1);
        
        try {
            if (actions.find(buttonId) != actions.end()) {
                ActionConfig ac = actions.at(buttonId);
                
                // Detailed action type logging
                USBSerial.printf("Configuring %s with action type: %s\n", 
                              buttonId.c_str(), ac.type.c_str());
                
                if (ac.type == "hid") {
                    actionMap[i].type = ACTION_HID;
                    
                    // Build the HID report from the parsed action
                    uint8_t hidReport[8] = {0};
                    
                    // First try to convert directly from our vector of hex strings
                    if (!ac.hidReport.empty()) {
                        if (HIDHandler::hexReportToBinary(ac.hidReport, hidReport, 8)) {
                            // Copy to our action map
                            memcpy(actionMap[i].hidReport, hidReport, 8);
                            
                            // Debug output
                            USBSerial.printf("HID report for %s: ", buttonId.c_str());
                            for (int j = 0; j < 8; j++) {
                                USBSerial.printf("%02X ", actionMap[i].hidReport[j]);
                            }
                            USBSerial.println();
                        } else {
                            USBSerial.printf("Failed to convert HID report for %s\n", buttonId.c_str());
                        }
                    } else {
                        USBSerial.printf("No HID report data for %s\n", buttonId.c_str());
                    }
                }
                else if (ac.type == "multimedia") {
                    actionMap[i].type = ACTION_MULTIMEDIA;
                    
                    // Similar conversion for consumer report
                    uint8_t consumerReport[4] = {0};
                    
                    if (!ac.consumerReport.empty()) {
                        if (HIDHandler::hexReportToBinary(ac.consumerReport, consumerReport, 4)) {
                            memcpy(actionMap[i].consumerReport, consumerReport, 4);
                            
                            // Debug output
                            USBSerial.printf("Consumer report for %s: ", buttonId.c_str());
                            for (int j = 0; j < 4; j++) {
                                USBSerial.printf("%02X ", actionMap[i].consumerReport[j]);
                            }
                            USBSerial.println();
                        } else {
                            USBSerial.printf("Failed to convert Consumer report for %s\n", buttonId.c_str());
                        }
                    } else {
                        USBSerial.printf("No Consumer report data for %s\n", buttonId.c_str());
                    }
                }
                else if (ac.type == "macro") {
                    actionMap[i].type = ACTION_MACRO;
                    actionMap[i].macroId = ac.macroId;
                    USBSerial.printf("Set macro ID for %s: %s\n", 
                                  buttonId.c_str(), actionMap[i].macroId.c_str());
                }
                else if (ac.type == "layer") {
                    actionMap[i].type = ACTION_LAYER;
                    actionMap[i].targetLayer = ac.targetLayer;
                    USBSerial.printf("Set target layer for %s: %s\n", 
                                  buttonId.c_str(), actionMap[i].targetLayer.c_str());
                }
            }
        } 
        catch (const std::exception& e) {
            USBSerial.printf("Error loading configuration for %s: %s\n", 
                          buttonId.c_str(), e.what());
        }
    }
    
    USBSerial.println("Key configuration loaded successfully");
}

void KeyHandler::updateKeys() {
    // Use a reversed scanning method that works well with diode–protected keys.
    static unsigned long lastScan = 0;
    const unsigned long scanInterval = 20; // scan every 20ms
    if (millis() - lastScan < scanInterval) return;
    lastScan = millis();
    
    // Configure rows as OUTPUT and columns as INPUT_PULLUP:
    for (uint8_t r = 0; r < numRows; r++) {
        pinMode(rowPins[r], OUTPUT);
        digitalWrite(rowPins[r], HIGH); // Initially high (inactive)
    }
    for (uint8_t c = 0; c < numCols; c++) {
        pinMode(colPins[c], INPUT_PULLUP);
    }
    
    // Scan one row at a time:
    for (uint8_t r = 0; r < numRows; r++) {
        // Drive current row LOW
        digitalWrite(rowPins[r], LOW);
        delayMicroseconds(50); // Allow signals to stabilize
        
        // Check each column on this row:
        for (uint8_t c = 0; c < numCols; c++) {
            uint8_t keyIndex = r * numCols + c;
            bool currentReading = (digitalRead(colPins[c]) == LOW);
            
            // Debouncing: check if the reading remains consistent for DEBOUNCE_TIME
            if (millis() - lastDebounceTime[keyIndex] < DEBOUNCE_TIME) {
                continue;
            }
            
            // If the key state has changed, update our state and handle the event
            if (currentReading != keyStates[keyIndex]) {
                lastDebounceTime[keyIndex] = millis();
                keyStates[keyIndex] = currentReading;
                
                // Map the matrix coordinate (r,c) to a button ID using your keyMap
                char keyChar = keyMap[r][c];
                String buttonId;
                if (keyChar >= '1' && keyChar <= '9') {
                    buttonId = "button-" + String(keyChar - '0');
                } else if (keyChar >= 'A' && keyChar <= 'Z') {
                    int buttonNum = (keyChar - 'A') + 10;
                    buttonId = "button-" + String(buttonNum);
                } else if (keyChar >= 'a' && keyChar <= 'z') {
                    int encoderNum = (keyChar - 'a') + 1;
                    buttonId = "encoder-" + String(encoderNum);
                } else {
                    buttonId = "unknown-" + String(keyIndex);
                }
                
                // Log and sync LED feedback
                USBSerial.printf("Key event: Row %d, Col %d, ID=%s, State=%s\n", 
                              r, c, buttonId.c_str(), currentReading ? "PRESSED" : "RELEASED");
                syncLEDsWithButtons(buttonId.c_str(), currentReading);
                
                // Determine the action type based on press/release
                KeyAction action = currentReading ? KEY_PRESS : KEY_RELEASE;
                if (lastAction[keyIndex] != action) {
                    lastAction[keyIndex] = action;
                    executeAction(keyIndex, action);
                }
            }
        }
        // Set the row back to inactive
        digitalWrite(rowPins[r], HIGH);
    }
}

void KeyHandler::handleKeyEvent(uint8_t keyIndex, bool pressed) {
    unsigned long currentTime = millis();
    
    // Ensure keyIndex is valid
    if (keyIndex >= MAX_KEYS) {
        USBSerial.printf("Invalid key index: %d (exceeds MAX_KEYS)\n", keyIndex);
        return;
    }
    
    // Debounce logic
    if (currentTime - lastDebounceTime[keyIndex] < DEBOUNCE_TIME) {
        return;
    }
    lastDebounceTime[keyIndex] = currentTime;
    
    // State change check - only react to changes
    if (keyStates[keyIndex] == pressed) {
        return;
    }
    keyStates[keyIndex] = pressed;
    
    // Find the actual button ID from the component configuration for this key
    // This ensures we're using the correct button ID for actions
    char keyChar = '\0';
    for (int r = 0; r < numRows; r++) {
        for (int c = 0; c < numCols; c++) {
            if (r * numCols + c == keyIndex && keyMap[r][c] != 'X') {
                keyChar = keyMap[r][c];
                break;
            }
        }
        if (keyChar != '\0') break;
    }
    
    String buttonId;
    
    // Handle different character ranges for different component types
    if (keyChar >= '1' && keyChar <= '9') {
        // Regular buttons 1-9
        buttonId = "button-" + String(keyChar - '0');
    } 
    else if (keyChar >= 'A' && keyChar <= 'Z') {
        // Buttons 10-35
        int buttonNum = (keyChar - 'A') + 10;
        buttonId = "button-" + String(buttonNum);
    } 
    else if (keyChar >= 'a' && keyChar <= 'z') {
        // Encoders (lowercase letters represent encoders)
        int encoderNum = (keyChar - 'a') + 1;
        buttonId = "encoder-" + String(encoderNum);
    } 
    else {
        // Fallback for any unexpected characters
        buttonId = "unknown-" + String(keyIndex);
    }
    
    // Log detailed information for debugging
    USBSerial.printf("Key event: Index=%d, Char='%c', ID=%s, State=%s\n", 
                 keyIndex, keyChar ? keyChar : 'X', buttonId.c_str(), 
                 pressed ? "PRESSED" : "RELEASED");
    
    // Sync LEDs with button state if function is available
    syncLEDsWithButtons(buttonId.c_str(), pressed);
    
    // Determine action
    KeyAction action = pressed ? KEY_PRESS : KEY_RELEASE;
    if (lastAction[keyIndex] != action) {
        lastAction[keyIndex] = action;
        executeAction(keyIndex, action);
    }
}

void KeyHandler::printKeyboardState() {
    USBSerial.println("\n--- Keyboard Matrix State ---");
    for (uint8_t r = 0; r < numRows; r++) {
        for (uint8_t c = 0; c < numCols; c++) {
            char key = keyMap[r][c];
            bool isPressed = (key != 'X') && isKeyPressed(key);
            USBSerial.printf("[%c:%s] ", key, isPressed ? "ON" : "  ");
        }
        USBSerial.println();
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