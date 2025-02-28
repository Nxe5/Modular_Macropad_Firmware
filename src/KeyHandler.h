#ifndef KEY_HANDLER_H
#define KEY_HANDLER_H

#include <Arduino.h>
#include <Keypad.h>

// Constants
#define MAX_KEYS 25       // Maximum number of keys to track
#define DEBOUNCE_TIME 50  // Default debounce time in ms
#define LIST_MAX 10       // Keypad library's LIST_MAX value

// Action types
enum ActionType {
    ACTION_NONE,
    ACTION_HID,
    ACTION_MULTIMEDIA,
    ACTION_MACRO,
    ACTION_LAYER
};

// Key action events
enum KeyAction {
    KEY_NONE,
    KEY_PRESS,
    KEY_RELEASE,
    KEY_HOLD
};

// Structure for key action configuration
struct KeyConfig {
    ActionType type = ACTION_NONE;
    
    // For HID keyboard reports
    uint8_t hidReport[8] = {0};
    
    // For consumer control reports
    uint8_t consumerReport[4] = {0};
    
    // For macros
    String macroId = "";
    
    // For layer switching
    String targetLayer = "";
};

class KeyHandler {
public:
    KeyHandler(uint8_t rows, uint8_t cols, char** keyMapping, 
               uint8_t* rows_pins, uint8_t* cols_pins);
    ~KeyHandler();
    
    void begin();
    char getKey();
    bool isKeyPressed(char key);
    uint8_t getTotalKeys();
    void loadKeyConfiguration();
    void updateKeys();
    
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
};

// Global key handler instance (extern declaration)
extern KeyHandler* keyHandler;

// Initialization and update functions
void initializeKeyHandler();
void updateKeyHandler();
void cleanupKeyHandler();

// Forward declaration for LED synchronization function (defined in LEDHandler.cpp)
void syncLEDsWithButtons(const char* buttonId, bool pressed);

#endif // KEY_HANDLER_H