#ifndef MACRO_HANDLER_H
#define MACRO_HANDLER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>
#include <map>
#include <string>
#include <functional>
#include <FS.h>
#include <LittleFS.h>
#include "HIDHandler.h"

// Path constants
const char* const MACRO_DIRECTORY = "/macros";

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

// Mouse button definitions - renamed to avoid conflicts with USBHIDMouse.h
enum MouseButton {
    MB_LEFT = 1,
    MB_RIGHT = 2,
    MB_MIDDLE = 4,
    MB_BACK = 8,
    MB_FORWARD = 16
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
private:
    // Map to store all loaded macros, keyed by their IDs
    std::map<String, Macro> macros;
    
    // Currently executing macro (if any)
    bool executing = false;
    size_t currentCommandIndex = 0;
    Macro currentMacro;
    uint32_t lastExecTime = 0;
    uint32_t delayUntil = 0;
    
    // Repeat state
    bool inRepeat = false;
    int repeatCount = 0;
    int currentRepeatCount = 0;
    size_t repeatStartIndex = 0;
    
    // Path functions
    String getMacroFilePath(const String& macroId) {
        // Sanitize the macroId (remove invalid characters)
        String sanitized = macroId;
        sanitized.replace("/", "_");
        sanitized.replace("\\", "_");
        
        return String(MACRO_DIRECTORY) + "/" + sanitized + ".json";
    }
    
    // Ensure directory exists
    void ensureMacroDirectoryExists();
    
    // Helper function to clean up dynamically allocated memory in commands
    void cleanupMacroCommand(MacroCommand& command);
    
public:
    MacroHandler();
    
    // Initialization
    bool begin();
    
    // Macro management
    bool loadMacros();
    bool saveMacro(const Macro& macro);
    bool deleteMacro(const String& macroId);
    bool getMacro(const String& macroId, Macro& macro);
    std::vector<String> getAvailableMacros();
    
    // Macro execution
    bool executeMacro(const String& macroId);
    void executeCommand(const MacroCommand& cmd);
    void update();
    bool isExecuting() const { return executing; }
    
    // Parsing
    bool parseMacroFromJson(const JsonObject& json, Macro& macro);

    // For backward compatibility - now a no-op
    bool saveMacroIndex() { return true; }
};

// Global macro handler instance
extern MacroHandler* macroHandler;

// Helper functions
void initializeMacroHandler();
void updateMacroHandler();
void cleanupMacroHandler();

#endif // MACRO_HANDLER_H 