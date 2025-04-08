// KeyHandler.h

#ifndef KEY_HANDLER_H
#define KEY_HANDLER_H
#include <Arduino.h>
#include <Keypad.h>
#include <map>
#include <vector>
#include "ConfigManager.h"
#include "HIDHandler.h"  // Include for key definitions

// Constants
#define MAX_KEYS 25 // Maximum number of keys
#define DEBOUNCE_TIME 50 // Debounce time in ms
#define LIST_MAX 10 // As defined by Keypad library
#define NO_KEY '\0' // No key pressed

// Action types for key events
enum ActionType {
    ACTION_NONE = 0,
    ACTION_HID = 1,
    ACTION_MULTIMEDIA = 2,
    ACTION_MACRO = 3,
    ACTION_LAYER = 4,
    ACTION_CYCLE_LAYER = 5
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
    KeyHandler(uint8_t rows, uint8_t cols, 
              const std::vector<Component>& components,
              uint8_t* rowPins, uint8_t* colPins);
    ~KeyHandler();
    
    void begin();
    uint8_t getTotalKeys();
    void updateKeys();
    void loadKeyConfiguration(const std::map<String, ActionConfig>& actions);
    
    // Add diagnostic methods
    void printKeyboardState();
    void diagnostics();
    
    // Layer management
    bool switchToLayer(const String& layerName);
    String getCurrentLayer() const { return currentLayer; }
    bool loadLayerConfiguration(const String& layerName, const std::map<String, ActionConfig>& actions);
    bool isLayerAvailable(const String& layerName) const;
    std::vector<String> getAvailableLayers() const;
    
    // Add these function declarations:
    void setCpuFrequencyMhz(uint8_t mhz);
    void performHighPowerTask();
    
    bool assignMacroToButton(const String& buttonId, const String& macroId);
    
    // Add getKeyConfig method
    const KeyConfig& getKeyConfig(size_t index) const {
        if (index < componentPositions.size() && actionMap) {
            return actionMap[index];
        }
        static KeyConfig emptyConfig;
        return emptyConfig;
    }
    
    // Add the displayKeyConfig method declaration
    void displayKeyConfig(const KeyConfig& config);

    // Add method to cycle to next layer
    bool cycleToNextLayer();
    
private:
    void cleanup();
    void executeAction(uint8_t keyIndex, KeyAction action);
    
    uint8_t numRows;
    uint8_t numCols;
    uint8_t* rowPins;
    uint8_t* colPins;
    Keypad* keypad;
    KeyConfig* actionMap;
    
    // Layer management
    String currentLayer = "default";
    std::map<String, std::map<String, KeyConfig>> layerConfigs; // Layer -> ComponentID -> Config
    bool saveCurrentLayer();
    bool loadCurrentLayer();
    
    // Add helper method to get next layer name
    String getNextLayerName() const;
    
    // Direct position mapping
    struct ComponentPosition {
        uint8_t row;
        uint8_t col;
        String id;
        bool operator<(const ComponentPosition& other) const {
            if (row != other.row) return row < other.row;
            return col < other.col;
        }
    };
    std::vector<ComponentPosition> componentPositions;
    
    // Dynamic arrays for key states
    bool* keyStates;
    unsigned long* lastDebounceTime;
    KeyAction* lastAction;
    const unsigned long debounceDelay = 50;
};

extern KeyHandler* keyHandler;

void initializeKeyHandler();
void updateKeyHandler();
void cleanupKeyHandler();

// For LED sync (defined elsewhere)
void syncLEDsWithButtons(const char* buttonId, bool pressed);

#endif // KEY_HANDLER_H