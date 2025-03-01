#include "KeyHandler.h"
#include <SPIFFS.h>
#include <ArduinoJson.h>

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
        Serial.println("Error: Matrix dimensions too large");
        return;
    }

    // Validate input pointers
    if (!keyMapping || !rows_pins || !colPins) {
        Serial.println("Error: Null input pointers");
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
        
        Serial.printf("KeyHandler initialized successfully. Total keys: %d\n", totalKeys);
    } 
    catch (const std::exception& e) {
        Serial.printf("Exception in constructor: %s\n", e.what());
        cleanup();
    } 
    catch (...) {
        Serial.println("Unknown exception in constructor");
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
        Serial.println("Error: Keypad not initialized in begin()");
        return;
    }
    
    try {
        // NOTE: We're not changing pin modes here anymore since they're already
        // configured in configurePinModes() - this prevents conflicts
        
        Serial.println("KeyHandler initialization complete - using configuration from configurePinModes()");
        
        // Configure the keypad library's settings
        if (keypad) {
            keypad->setDebounceTime(DEBOUNCE_TIME);
            keypad->setHoldTime(500);
            Serial.println("Keypad library configured with debounce time: " + String(DEBOUNCE_TIME) + "ms");
        }
    } 
    catch (...) {
        Serial.println("Error in KeyHandler::begin()");
    }
}

char KeyHandler::getKey() {
    if (!keypad) return NO_KEY;
    
    char key = keypad->getKey();
    return (key == 'X') ? NO_KEY : key;
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

void KeyHandler::loadKeyConfiguration(const std::map<String, ActionConfig>& actions) {
    uint8_t totalKeys = getTotalKeys();
    
    // Extensive logging and error checking
    Serial.printf("Loading key configuration for %d keys\n", totalKeys);
    
    for (uint8_t i = 0; i < totalKeys; i++) {
        String buttonId = "button-" + String(i + 1);
        
        try {
            if (actions.find(buttonId) != actions.end()) {
                ActionConfig ac = actions.at(buttonId);
                
                // Detailed action type logging
                Serial.printf("Configuring %s with action type: %s\n", 
                              buttonId.c_str(), ac.type.c_str());
                
                if (ac.type == "hid") {
                    actionMap[i].type = ACTION_HID;
                    // Convert hex strings to byte values
                    if (ac.hidReport.size() >= 8) {
                        for (int j = 0; j < 8; j++) {
                            actionMap[i].hidReport[j] = strtol(ac.hidReport[j].c_str() + 2, NULL, 16);
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
                else if (ac.type == "multimedia") {
                    actionMap[i].type = ACTION_MULTIMEDIA;
                    // Similar conversion for consumerReport if needed
                }
            }
        } 
        catch (const std::exception& e) {
            Serial.printf("Error loading configuration for %s: %s\n", 
                          buttonId.c_str(), e.what());
        }
    }
    
    Serial.println("Key configuration loaded successfully");
}

void KeyHandler::updateKeys() {
    if (!keypad) {
        Serial.println("Error: Keypad not initialized in updateKeys()");
        return;
    }
    
    try {
        keypad->getKeys();
        
        for (int i = 0; i < LIST_MAX; i++) {
            if (keypad->key[i].stateChanged && keypad->key[i].kchar != 'X') {
                char key = keypad->key[i].kchar;
                int keyIndex = -1;
                
                // Find key index in the matrix
                for (int r = 0; r < numRows; r++) {
                    for (int c = 0; c < numCols; c++) {
                        if (keyMap[r][c] == key) {
                            keyIndex = r * numCols + c;
                            break;
                        }
                    }
                    if (keyIndex >= 0) break;
                }
                
                if (keyIndex >= 0 && keyIndex < getTotalKeys()) {
                    bool pressed = (keypad->key[i].kstate == PRESSED || 
                                    keypad->key[i].kstate == HOLD);
                    
                    handleKeyEvent(keyIndex, pressed);
                }
            }
        }
    } 
    catch (const std::exception& e) {
        Serial.printf("Exception in updateKeys: %s\n", e.what());
    } 
    catch (...) {
        Serial.println("Unknown exception in updateKeys");
    }
}

void KeyHandler::handleKeyEvent(uint8_t keyIndex, bool pressed) {
    unsigned long currentTime = millis();
    
    // Ensure keyIndex is valid
    if (keyIndex >= MAX_KEYS) {
        Serial.printf("Invalid key index: %d (exceeds MAX_KEYS)\n", keyIndex);
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
    if (keyChar >= '1' && keyChar <= '9') {
        buttonId = "button-" + String(keyChar - '0');
    }
    else if (keyChar >= 'A' && keyChar <= 'Z') {
        // Handle buttons 10+ or special keys
        if (keyChar >= 'A' && keyChar <= 'Z') {
            int buttonNum = (keyChar - 'A') + 10;  // A=10, B=11, etc.
            buttonId = "button-" + String(buttonNum);
        }
    }
    else if (keyChar >= 'E' && keyChar <= 'J') {  // Assuming encoders E-J
        int encoderNum = keyChar - 'E' + 1;
        buttonId = "encoder-" + String(encoderNum);
    }
    else {
        // Fallback - use key index directly if no mapping found
        buttonId = "button-" + String(keyIndex + 1);
    }
    
    // Log detailed information for debugging
    Serial.printf("Key event: Index=%d, Char='%c', ID=%s, State=%s\n", 
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

void KeyHandler::executeAction(uint8_t keyIndex, KeyAction action) {
    // Null check for safety
    if (keyIndex >= getTotalKeys()) {
        Serial.printf("Invalid key index for action: %d\n", keyIndex);
        return;
    }
    
    KeyConfig& config = actionMap[keyIndex];
    
    Serial.printf("Executing action for key index %d (Type: %d, Action: %d)\n", 
                 keyIndex, config.type, action);
    
    switch (config.type) {
        case ACTION_HID:
            if (action == KEY_PRESS) {
                Serial.print("HID report: ");
                for (int i = 0; i < 8; i++) {
                    Serial.printf("%02X ", config.hidReport[i]);
                }
                Serial.println();
                // TODO: Send the HID report via USB/BLE
            } else {
                Serial.printf("HID key released\n");
                // TODO: Send a release HID report (zeroed)
            }
            break;
        case ACTION_MACRO:
            if (action == KEY_PRESS) {
                Serial.printf("Trigger macro: %s\n", config.macroId.c_str());
                // TODO: Execute macro
            }
            break;
        case ACTION_LAYER:
            if (action == KEY_PRESS) {
                Serial.printf("Change layer to: %s\n", config.targetLayer.c_str());
                // TODO: Switch layer configuration
            }
            break;
        case ACTION_MULTIMEDIA:
            if (action == KEY_PRESS) {
                Serial.printf("Multimedia action triggered\n");
                // TODO: Send consumer control report
            }
            break;
        default:
            Serial.printf("No action mapped for key index %d\n", keyIndex);
            break;
    }
}

void KeyHandler::printKeyboardState() {
    Serial.println("\n--- Keyboard Matrix State ---");
    for (uint8_t r = 0; r < numRows; r++) {
        for (uint8_t c = 0; c < numCols; c++) {
            char key = keyMap[r][c];
            bool isPressed = (key != 'X') && isKeyPressed(key);
            Serial.printf("[%c:%s] ", key, isPressed ? "ON" : "  ");
        }
        Serial.println();
    }
    Serial.println("----------------------------\n");
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