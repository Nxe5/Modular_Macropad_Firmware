#include "KeyHandler.h"
#include <SPIFFS.h>
#include <ArduinoJson.h>

KeyHandler* keyHandler = nullptr;

KeyHandler::KeyHandler(uint8_t rows, uint8_t cols, char** keyMapping, 
                       uint8_t* rows_pins, uint8_t* colPins) : 
    numRows(rows), numCols(cols) 
{
    // Allocate and copy key mapping
    keyMap = new char*[rows];
    for (uint8_t i = 0; i < rows; i++) {
        keyMap[i] = new char[cols];
        for (uint8_t j = 0; j < cols; j++) {
            keyMap[i][j] = keyMapping[i][j];
        }
    }
    
    // Copy pin arrays
    rowPins = new uint8_t[rows];
    colPins = new uint8_t[cols];
    memcpy(rowPins, rows_pins, rows * sizeof(uint8_t));
    memcpy(colPins, colPins, cols * sizeof(uint8_t));
    
    // Initialize the keypad instance
    keypad = new Keypad(makeKeymap((char*)keyMap), rowPins, colPins, rows, cols);
    
    // Allocate the action map based on total keys
    actionMap = new KeyConfig[getTotalKeys()];
    
    // Initialize key state arrays
    for (int i = 0; i < MAX_KEYS; i++) {
        keyStates[i] = false;
        lastDebounceTime[i] = 0;
        lastAction[i] = KEY_NONE;
    }
}

KeyHandler::~KeyHandler() {
    for (uint8_t i = 0; i < numRows; i++) {
        delete[] keyMap[i];
    }
    delete[] keyMap;
    delete[] rowPins;
    delete[] colPins;
    delete[] actionMap;
    delete keypad;
}

void KeyHandler::begin() {
    keypad->setDebounceTime(DEBOUNCE_TIME);
    Serial.println("KeyHandler initialized");
}

char KeyHandler::getKey() {
    char key = keypad->getKey();
    return (key == 'X') ? NO_KEY : key;
}

bool KeyHandler::isKeyPressed(char key) {
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
    // For each key in our matrix, assign action based on button id:
    // Our convention: key index = (row * numCols + col), and button id = "button-" + (index+1)
    uint8_t totalKeys = getTotalKeys();
    for (uint8_t i = 0; i < totalKeys; i++) {
        String buttonId = "button-" + String(i + 1);
        if (actions.find(buttonId) != actions.end()) {
            ActionConfig ac = actions.at(buttonId);
            // For now, only handling HID actions; add other cases as needed.
            if (ac.type == "hid") {
                actionMap[i].type = ACTION_HID;
                // Convert the hex strings to byte values
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
                // Similar conversion for consumerReport if needed.
            }
        }
    }
    Serial.println("Loaded key configuration from actions.json");
}

void KeyHandler::updateKeys() {
    keypad->getKeys();
    for (int i = 0; i < LIST_MAX; i++) {
        if (keypad->key[i].stateChanged && keypad->key[i].kchar != 'X') {
            char key = keypad->key[i].kchar;
            int keyIndex = -1;
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
                bool pressed = (keypad->key[i].kstate == PRESSED || keypad->key[i].kstate == HOLD);
                handleKeyEvent(keyIndex, pressed);
            }
        }
    }
}

void KeyHandler::handleKeyEvent(uint8_t keyIndex, bool pressed) {
    unsigned long currentTime = millis();
    if (currentTime - lastDebounceTime[keyIndex] < DEBOUNCE_TIME) return;
    lastDebounceTime[keyIndex] = currentTime;
    if (keyStates[keyIndex] == pressed) return;
    keyStates[keyIndex] = pressed;
    
    String buttonId = "button-" + String(keyIndex + 1);
    syncLEDsWithButtons(buttonId.c_str(), pressed);
    Serial.printf("Key %s %s\n", buttonId.c_str(), pressed ? "pressed" : "released");
    KeyAction action = pressed ? KEY_PRESS : KEY_RELEASE;
    if (lastAction[keyIndex] != action) {
        lastAction[keyIndex] = action;
        executeAction(keyIndex, action);
    }
}

void KeyHandler::executeAction(uint8_t keyIndex, KeyAction action) {
    KeyConfig& config = actionMap[keyIndex];
    Serial.printf("Executing action for button %d (Type: %d)\n", keyIndex + 1, config.type);
    switch (config.type) {
        case ACTION_HID:
            if (action == KEY_PRESS) {
                Serial.printf("HID report for button %d: ", keyIndex + 1);
                for (int i = 0; i < 8; i++) {
                    Serial.printf("%02X ", config.hidReport[i]);
                }
                Serial.println();
                // TODO: Send the HID report via USB/BLE
            } else {
                Serial.printf("HID button %d released\n", keyIndex + 1);
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
            Serial.printf("No action mapped for button %d\n", keyIndex + 1);
            break;
    }
}
