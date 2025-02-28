#ifndef KEY_HANDLER_H
#define KEY_HANDLER_H

#include <Arduino.h>
#include <Keypad.h>
#include <map>
#include "ConfigManager.h"

// Constants
#define MAX_KEYS 25       // Maximum number of keys
#define DEBOUNCE_TIME 50  // Debounce time in ms
#define LIST_MAX 10       // As defined by Keypad library

// Action types for key events
enum ActionType {
    ACTION_NONE,
    ACTION_HID,
    ACTION_MULTIMEDIA,
    ACTION_MACRO,
    ACTION_LAYER
};

enum KeyAction {
    KEY_NONE,
    KEY_PRESS,
    KEY_RELEASE
};

struct KeyConfig {
    ActionType type = ACTION_NONE;
    uint8_t hidReport[8] = {0};
    uint8_t consumerReport[4] = {0};
    String macroId = "";
    String targetLayer = "";
};

class KeyHandler {
public:
    KeyHandler(uint8_t rows, uint8_t cols, char** keyMapping, 
               uint8_t* rowPins, uint8_t* colPins);
    ~KeyHandler();
    
    void begin();
    char getKey();
    bool isKeyPressed(char key);
    uint8_t getTotalKeys();
    void updateKeys();
    void loadKeyConfiguration(const std::map<String, ActionConfig>& actions);
    
private:
    void handleKeyEvent(uint8_t keyIndex, bool pressed);
    void executeAction(uint8_t keyIndex, KeyAction action);
    
    uint8_t numRows;
    uint8_t numCols;
    char** keyMap;
    uint8_t* rowPins;
    uint8_t* colPins;
    Keypad* keypad;
    KeyConfig* actionMap;
    
    bool keyStates[MAX_KEYS];
    unsigned long lastDebounceTime[MAX_KEYS];
    KeyAction lastAction[MAX_KEYS];
};

extern KeyHandler* keyHandler;

void initializeKeyHandler();
void updateKeyHandler();
void cleanupKeyHandler();

// For LED sync (defined elsewhere)
void syncLEDsWithButtons(const char* buttonId, bool pressed);

#endif // KEY_HANDLER_H
