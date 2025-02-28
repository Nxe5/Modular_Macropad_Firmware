#include "KeyHandler.h"
#include "LEDHandler.h"
#include <SPIFFS.h>
#include <ArduinoJson.h>

// Global key handler instance
KeyHandler* keyHandler = nullptr;

// Current key states for event detection
static bool keyStates[MAX_KEYS];
static unsigned long lastDebounceTime[MAX_KEYS];
static KeyAction lastAction[MAX_KEYS];

// Forward declaration
static String readJsonFile(const char* filePath);

KeyHandler::KeyHandler(uint8_t rows, uint8_t cols, char** keyMapping, 
                      uint8_t* rows_pins, uint8_t* cols_pins) {
    numRows = rows;
    numCols = cols;
    
    // Allocate memory for keyMap
    keyMap = new char*[rows];
    for(uint8_t i = 0; i < rows; i++) {
        keyMap[i] = new char[cols];
        for(uint8_t j = 0; j < cols; j++) {
            keyMap[i][j] = keyMapping[i][j];
        }
    }
    
    // Allocate and copy pin arrays
    rowPins = new uint8_t[rows];
    colPins = new uint8_t[cols];
    memcpy(rowPins, rows_pins, rows * sizeof(uint8_t));
    memcpy(colPins, cols_pins, cols * sizeof(uint8_t));
    
    // Create keypad instance
    keypad = new Keypad(makeKeymap((char*)keyMap), rowPins, colPins, rows, cols);
    
    // Initialize action map
    actionMap = new KeyConfig[getTotalKeys()];
    
    // Initialize key state tracking
    for (int i = 0; i < MAX_KEYS; i++) {
        keyStates[i] = false;
        lastDebounceTime[i] = 0;
        lastAction[i] = KEY_NONE;
    }
}

KeyHandler::~KeyHandler() {
    // Clean up dynamic memory
    for(uint8_t i = 0; i < numRows; i++) {
        delete[] keyMap[i];
    }
    delete[] keyMap;
    delete[] rowPins;
    delete[] colPins;
    delete[] actionMap;
    delete keypad;
}

void KeyHandler::begin() {
    // Set debounce time
    keypad->setDebounceTime(DEBOUNCE_TIME);
    
    // Try to load button configuration
    loadKeyConfiguration();
    
    Serial.println("Key Handler initialized");
}

char KeyHandler::getKey() {
    char key = keypad->getKey();
    // Don't return 'X' keys (unused keys)
    return (key == 'X') ? NO_KEY : key;
}

bool KeyHandler::isKeyPressed(char key) {
    // Check if specific key is currently pressed
    return keypad->isPressed(key) && key != 'X';
}

uint8_t KeyHandler::getTotalKeys() {
    uint8_t count = 0;
    for(uint8_t r = 0; r < numRows; r++) {
        for(uint8_t c = 0; c < numCols; c++) {
            if(keyMap[r][c] != 'X') { // 'X' represents no key
                count++;
            }
        }
    }
    return count;
}

void KeyHandler::loadKeyConfiguration() {
    // Try to load actions configuration
    if (SPIFFS.begin(false)) {
        String actionsJson = readJsonFile("/config/actions.json");
        if (!actionsJson.isEmpty()) {
            DynamicJsonDocument doc(16384); // Larger size for the potentially big actions.json
            DeserializationError error = deserializeJson(doc, actionsJson);
            
            if (!error) {
                Serial.println("Loaded actions.json");
                JsonObject actions = doc["actions"]["layer-config"];
                
                // Process each button's configuration
                int buttonCount = 0;
                for (JsonPair kv : actions) {
                    String buttonId = kv.key().c_str();
                    JsonObject buttonConfig = kv.value().as<JsonObject>();
                    
                    // Extract index from button ID (e.g., "button-5" -> 4)
                    int buttonIndex = -1;
                    if (buttonId.startsWith("button-")) {
                        buttonIndex = buttonId.substring(7).toInt() - 1;
                    } else if (buttonId.startsWith("encoder-")) {
                        // Encoders have special handling, we'll handle them separately
                        continue;
                    } else {
                        // Unknown button ID format
                        continue;
                    }
                    
                    if (buttonIndex >= 0 && buttonIndex < getTotalKeys()) {
                        // Store the action type
                        String actionType = buttonConfig["type"].as<String>();
                        
                        // Store "hid" action data
                        if (actionType == "hid") {
                            actionMap[buttonIndex].type = ACTION_HID;
                            
                            // Check if it's a single keycode or an array
                            if (buttonConfig["buttonPress"].is<JsonArray>()) {
                                // Handle multi-key sequence
                                JsonArray keySequence = buttonConfig["buttonPress"].as<JsonArray>();
                                
                                // Process first keycode (we only store one for now)
                                if (keySequence.size() > 0) {
                                    JsonArray firstKey = keySequence[0].as<JsonArray>();
                                    
                                    if (firstKey.size() >= 8) {
                                        // Parse HID report values
                                        actionMap[buttonIndex].hidReport[0] = strtol(firstKey[0].as<const char*>() + 2, NULL, 16);
                                        actionMap[buttonIndex].hidReport[1] = strtol(firstKey[1].as<const char*>() + 2, NULL, 16);
                                        actionMap[buttonIndex].hidReport[2] = strtol(firstKey[2].as<const char*>() + 2, NULL, 16);
                                        actionMap[buttonIndex].hidReport[3] = strtol(firstKey[3].as<const char*>() + 2, NULL, 16);
                                        actionMap[buttonIndex].hidReport[4] = strtol(firstKey[4].as<const char*>() + 2, NULL, 16);
                                        actionMap[buttonIndex].hidReport[5] = strtol(firstKey[5].as<const char*>() + 2, NULL, 16);
                                        actionMap[buttonIndex].hidReport[6] = strtol(firstKey[6].as<const char*>() + 2, NULL, 16);
                                        actionMap[buttonIndex].hidReport[7] = strtol(firstKey[7].as<const char*>() + 2, NULL, 16);
                                    }
                                }
                            } else if (buttonConfig["buttonPress"].is<JsonArray>()) {
                                // Handle single keycode array
                                JsonArray singleKey = buttonConfig["buttonPress"].as<JsonArray>();
                                
                                if (singleKey.size() >= 8) {
                                    // Parse HID report values
                                    actionMap[buttonIndex].hidReport[0] = strtol(singleKey[0].as<const char*>() + 2, NULL, 16);
                                    actionMap[buttonIndex].hidReport[1] = strtol(singleKey[1].as<const char*>() + 2, NULL, 16);
                                    actionMap[buttonIndex].hidReport[2] = strtol(singleKey[2].as<const char*>() + 2, NULL, 16);
                                    actionMap[buttonIndex].hidReport[3] = strtol(singleKey[3].as<const char*>() + 2, NULL, 16);
                                    actionMap[buttonIndex].hidReport[4] = strtol(singleKey[4].as<const char*>() + 2, NULL, 16);
                                    actionMap[buttonIndex].hidReport[5] = strtol(singleKey[5].as<const char*>() + 2, NULL, 16);
                                    actionMap[buttonIndex].hidReport[6] = strtol(singleKey[6].as<const char*>() + 2, NULL, 16);
                                    actionMap[buttonIndex].hidReport[7] = strtol(singleKey[7].as<const char*>() + 2, NULL, 16);
                                }
                            }
                        } 
                        // Handle other action types (macro, layer, etc.) as needed
                        else if (actionType == "macro") {
                            actionMap[buttonIndex].type = ACTION_MACRO;
                            // Store macro ID or content
                            actionMap[buttonIndex].macroId = buttonConfig["macroId"].as<String>();
                        }
                        else if (actionType == "layer") {
                            actionMap[buttonIndex].type = ACTION_LAYER;
                            // Store target layer
                            actionMap[buttonIndex].targetLayer = buttonConfig["layerId"].as<String>();
                        }
                        else if (actionType == "multimedia") {
                            actionMap[buttonIndex].type = ACTION_MULTIMEDIA;
                            // Parse consumer report
                            JsonArray report = buttonConfig["consumerReport"].as<JsonArray>();
                            if (report.size() >= 4) {
                                actionMap[buttonIndex].consumerReport[0] = strtol(report[0].as<const char*>() + 2, NULL, 16);
                                actionMap[buttonIndex].consumerReport[1] = strtol(report[1].as<const char*>() + 2, NULL, 16);
                                actionMap[buttonIndex].consumerReport[2] = strtol(report[2].as<const char*>() + 2, NULL, 16);
                                actionMap[buttonIndex].consumerReport[3] = strtol(report[3].as<const char*>() + 2, NULL, 16);
                            }
                        }
                        
                        buttonCount++;
                    }
                }
                
                Serial.printf("Loaded %d button configurations\n", buttonCount);
            } else {
                Serial.print("Error parsing actions.json: ");
                Serial.println(error.c_str());
            }
        } else {
            Serial.println("No actions.json found, using defaults");
        }
    } else {
        Serial.println("SPIFFS not available, using default key configurations");
    }
}

void KeyHandler::updateKeys() {
    // Check for key events
    keypad->getKeys();
    
    // Process all keys in the list
    for (int i = 0; i < LIST_MAX; i++) {
        // Check if key is active and a real key (not 'X')
        if (keypad->key[i].stateChanged && keypad->key[i].kchar != 'X') {
            char key = keypad->key[i].kchar;
            KeyState state = keypad->key[i].kstate;
            
            // Find key index in our mapping
            int keyIndex = -1;
            for (int r = 0; r < numRows; r++) {
                for (int c = 0; c < numCols; c++) {
                    if (keyMap[r][c] == key) {
                        // Convert from key matrix position to key index
                        // Assuming a 1D mapping of the matrix positions
                        keyIndex = r * numCols + c;
                        break;
                    }
                }
                if (keyIndex >= 0) break;
            }
            
            // If key found, handle its event
            if (keyIndex >= 0 && keyIndex < getTotalKeys()) {
                handleKeyEvent(keyIndex, state == PRESSED || state == HOLD);
            }
        }
    }
}

void KeyHandler::handleKeyEvent(uint8_t keyIndex, bool pressed) {
    // Debounce check
    unsigned long currentTime = millis();
    if (currentTime - lastDebounceTime[keyIndex] < DEBOUNCE_TIME) {
        return;
    }
    lastDebounceTime[keyIndex] = currentTime;
    
    // Check if the key state has actually changed
    if (keyStates[keyIndex] == pressed) {
        return; // No change
    }
    
    // Update key state
    keyStates[keyIndex] = pressed;
    
    // Create button ID (1-based index)
    String buttonId = "button-" + String(keyIndex + 1);
    
    // Sync with LED if available
    syncLEDsWithButtons(buttonId.c_str(), pressed);
    
    // For debug
    Serial.printf("Key %s %s\n", buttonId.c_str(), pressed ? "pressed" : "released");
    
    // Determine key action based on press/release
    KeyAction action = pressed ? KEY_PRESS : KEY_RELEASE;
    
    // Only process if action changed (to avoid repeat processing)
    if (lastAction[keyIndex] != action) {
        lastAction[keyIndex] = action;
        
        // Execute the mapped action
        executeAction(keyIndex, action);
    }
}

void KeyHandler::executeAction(uint8_t keyIndex, KeyAction action) {
    // Get the key's configuration
    KeyConfig& config = actionMap[keyIndex];
    
    switch(config.type) {
        case ACTION_HID:
            // Handle HID keycode
            if (action == KEY_PRESS) {
                // For now, we'll just print the HID report
                Serial.printf("HID Key %d pressed: [%02X %02X %02X %02X %02X %02X %02X %02X]\n", 
                    keyIndex + 1,
                    config.hidReport[0], config.hidReport[1], config.hidReport[2], config.hidReport[3],
                    config.hidReport[4], config.hidReport[5], config.hidReport[6], config.hidReport[7]);
                
                // TODO: Send actual HID report
                // sendHIDReport(config.hidReport);
            } else {
                // Release key
                Serial.printf("HID Key %d released\n", keyIndex + 1);
                
                // TODO: Send all zeros HID report
                // clearHIDReport();
            }
            break;
            
        case ACTION_MACRO:
            // Handle macro
            if (action == KEY_PRESS) {
                Serial.printf("Macro %s triggered\n", config.macroId.c_str());
                // TODO: Execute macro sequence
            }
            break;
            
        case ACTION_LAYER:
            // Handle layer change
            if (action == KEY_PRESS) {
                Serial.printf("Layer change to %s\n", config.targetLayer.c_str());
                // TODO: Implement layer change
            }
            break;
            
        case ACTION_MULTIMEDIA:
            // Handle consumer control
            if (action == KEY_PRESS) {
                Serial.printf("Consumer control: [%02X %02X %02X %02X]\n",
                    config.consumerReport[0], config.consumerReport[1],
                    config.consumerReport[2], config.consumerReport[3]);
                // TODO: Send consumer control report
            } else {
                // Release consumer control
                Serial.println("Consumer control released");
                // TODO: Clear consumer report
            }
            break;
            
        default:
            // Unknown action type
            Serial.printf("Unknown action type for key %d\n", keyIndex + 1);
            break;
    }
}

// Helper function to read a file as string (similar to the one in ModuleSetup.cpp)
// Made static to avoid name collision
static String readJsonFile(const char* filePath) {
    if (!SPIFFS.exists(filePath)) {
        Serial.printf("File not found: %s\n", filePath);
        return "";
    }
    
    File file = SPIFFS.open(filePath, "r");
    if (!file) {
        Serial.printf("Failed to open file: %s\n", filePath);
        return "";
    }
    
    String content = file.readString();
    file.close();
    return content;
}

// Initialize the key handler with default key matrix configuration
void initializeKeyHandler() {
    // Default row and column pins
    uint8_t rows = 5;
    uint8_t cols = 5;
    
    // Define GPIO pins for rows and columns
    uint8_t rowPins[5] = {13, 12, 14, 27, 26};  // Update with your actual pin numbers
    uint8_t colPins[5] = {25, 33, 32, 35, 34};  // Update with your actual pin numbers
    
    // Create key mapping (X means no key)
    char** keyMapping = new char*[rows];
    for (int i = 0; i < rows; i++) {
        keyMapping[i] = new char[cols];
        for (int j = 0; j < cols; j++) {
            // Default to no key
            keyMapping[i][j] = 'X';
        }
    }
    
    // Set actual key positions based on your components.json
    // We're assuming a 5x5 grid with keys as defined in the earlier files
    keyMapping[0][3] = '1';  // button-1
    keyMapping[1][3] = '2';  // button-2
    keyMapping[1][4] = '3';  // button-3
    keyMapping[2][0] = '4';  // button-4
    keyMapping[2][1] = '5';  // button-5
    keyMapping[2][2] = '6';  // button-6
    keyMapping[2][3] = '7';  // button-7
    keyMapping[2][4] = '8';  // button-8
    keyMapping[3][0] = '9';  // button-9
    keyMapping[3][1] = 'A';  // button-10
    keyMapping[3][2] = 'B';  // button-11
    keyMapping[3][3] = 'C';  // button-12
    keyMapping[3][4] = 'D';  // button-13
    keyMapping[4][0] = 'E';  // button-14
    keyMapping[4][1] = 'F';  // button-15
    keyMapping[4][2] = 'G';  // button-16
    keyMapping[4][3] = 'H';  // button-17
    keyMapping[4][4] = 'I';  // button-18
    
    // Create and initialize the key handler
    keyHandler = new KeyHandler(rows, cols, keyMapping, rowPins, colPins);
    keyHandler->begin();
    
    // Clean up allocated memory
    for (int i = 0; i < rows; i++) {
        delete[] keyMapping[i];
    }
    delete[] keyMapping;
}

// Update function called from main loop
void updateKeyHandler() {
    if (keyHandler) {
        keyHandler->updateKeys();
    }
}

// Clean up resources
void cleanupKeyHandler() {
    if (keyHandler) {
        delete keyHandler;
        keyHandler = nullptr;
    }
}