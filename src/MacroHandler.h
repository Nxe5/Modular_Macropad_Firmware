#ifndef MACRO_HANDLER_H
#define MACRO_HANDLER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>
#include <map>
#include <string>
#include <SPIFFS.h>
#include "HIDHandler.h"

// Macro command types
enum MacroCommandType {
    MACRO_CMD_KEY_PRESS,      // Press and release key(s)
    MACRO_CMD_KEY_DOWN,       // Press key(s) without releasing
    MACRO_CMD_KEY_UP,         // Release previously pressed key(s)
    MACRO_CMD_TYPE_TEXT,      // Type a string of text
    MACRO_CMD_DELAY,          // Wait for specified time
    MACRO_CMD_CONSUMER_PRESS, // Press and release consumer control
    MACRO_CMD_EXECUTE_MACRO,  // Execute another macro (for macro chaining)
    MACRO_CMD_MOUSE_MOVE,     // Move the mouse cursor
    MACRO_CMD_MOUSE_CLICK,    // Click a mouse button
    MACRO_CMD_MOUSE_SCROLL,   // Scroll the mouse wheel
    MACRO_CMD_REPEAT_START,   // Start a repeat block
    MACRO_CMD_REPEAT_END,     // End a repeat block
    MACRO_CMD_RANDOM_DELAY    // Wait for a random time within a range
};

// Mouse button definitions
enum MouseButton {
    MOUSE_LEFT = 1,
    MOUSE_RIGHT = 2,
    MOUSE_MIDDLE = 4,
    MOUSE_BACK = 8,
    MOUSE_FORWARD = 16
};

// Structure to represent a macro command
struct MacroCommand {
    MacroCommandType type;
    union {
        struct {
            uint8_t report[8];  // HID keyboard report
        } keyPress;
        struct {
            uint8_t report[4];  // HID consumer report
        } consumerPress;
        struct {
            uint32_t milliseconds; // Delay duration
        } delay;
        struct {
            char* text;         // Text to type
            size_t length;      // Length of the text
        } typeText;
        struct {
            char* macroId;      // ID of another macro to execute
        } executeMacro;
        struct {
            int16_t x;          // X movement (positive = right, negative = left)
            int16_t y;          // Y movement (positive = down, negative = up)
            uint8_t speed;      // Movement speed (1-10)
        } mouseMove;
        struct {
            uint8_t button;     // Button mask (see MouseButton enum)
            uint8_t clicks;     // Number of clicks (1 for single, 2 for double)
        } mouseClick;
        struct {
            int8_t amount;      // Scroll amount (positive = up, negative = down)
        } mouseScroll;
        struct {
            uint16_t count;     // Number of times to repeat
        } repeatStart;
        struct {
            uint32_t minTime;   // Minimum delay time
            uint32_t maxTime;   // Maximum delay time
        } randomDelay;
    } data;
};

// Structure to represent a complete macro
struct Macro {
    String id;
    String name;
    String description;
    std::vector<MacroCommand> commands;
};

class MacroHandler {
public:
    MacroHandler();
    ~MacroHandler();

    // Initialize the handler
    bool begin();

    // Load all macros from SPIFFS
    bool loadMacros();

    // Save macro to SPIFFS
    bool saveMacro(const Macro& macro);

    // Delete macro from SPIFFS
    bool deleteMacro(const String& macroId);

    // Execute a macro by ID
    bool executeMacro(const String& macroId);

    // Create a new macro
    bool createMacro(const String& id, const String& name, const String& description);

    // Add a command to an existing macro
    bool addCommandToMacro(const String& macroId, const MacroCommand& command);

    // Parse a JSON macro definition
    bool parseMacroFromJson(const JsonObject& macroJson, Macro& outMacro);

    // Convert a macro to JSON for storage
    bool macroToJson(const Macro& macro, JsonObject& outJson);

    // Get a list of all available macros
    std::vector<String> getAvailableMacros();

    // Get a specific macro by ID
    bool getMacro(const String& macroId, Macro& outMacro);

    // Check if any macro is currently executing
    bool isExecuting() const { return isExecutingMacro; }

    // Update method called from the main loop
    void update();

private:
    // Map to store all loaded macros
    std::map<String, Macro> macros;

    // Execution state
    bool isExecutingMacro;
    unsigned long nextCommandTime;
    size_t currentCommandIndex;
    Macro* currentMacro;
    std::vector<Macro*> macroExecutionStack; // For handling nested macros
    
    // Repeat state
    struct RepeatState {
        size_t startIndex;
        uint16_t count;
        uint16_t executed;
    };
    std::vector<RepeatState> repeatStack;

    // Path to macro storage directory
    const char* MACRO_DIRECTORY = "/macros";
    
    // Path to macro index file (stores list of all macros)
    const char* MACRO_INDEX_FILE = "/macros/index.json";

    // Helper methods
    String getMacroFilePath(const String& macroId);
    bool ensureMacroDirectoryExists();
    bool updateMacroIndex();
    bool executeCommand(const MacroCommand& command);
    void cleanupMacroCommand(MacroCommand& command);
    MacroCommand createKeyPressCommand(const uint8_t* report);
    MacroCommand createConsumerPressCommand(const uint8_t* report);
    MacroCommand createDelayCommand(uint32_t milliseconds);
    MacroCommand createTypeTextCommand(const char* text);
    MacroCommand createExecuteMacroCommand(const char* macroId);
    MacroCommand createMouseMoveCommand(int16_t x, int16_t y, uint8_t speed);
    MacroCommand createMouseClickCommand(uint8_t button, uint8_t clicks);
    MacroCommand createMouseScrollCommand(int8_t amount);
    MacroCommand createRepeatStartCommand(uint16_t count);
    MacroCommand createRepeatEndCommand();
    MacroCommand createRandomDelayCommand(uint32_t minTime, uint32_t maxTime);
};

// Global macro handler instance
extern MacroHandler* macroHandler;

// Helper functions
void initializeMacroHandler();
void updateMacroHandler();
void cleanupMacroHandler();

#endif // MACRO_HANDLER_H 