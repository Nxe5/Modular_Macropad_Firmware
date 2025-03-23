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
    MACRO_CMD_EXECUTE_MACRO   // Execute another macro (for macro chaining)
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
};

// Global macro handler instance
extern MacroHandler* macroHandler;

// Helper functions
void initializeMacroHandler();
void updateMacroHandler();
void cleanupMacroHandler();

#endif // MACRO_HANDLER_H 