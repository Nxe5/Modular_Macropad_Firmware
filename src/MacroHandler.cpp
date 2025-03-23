#include "MacroHandler.h"
#include <Arduino.h>
#include <FS.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include "HIDHandler.h"

// Global instance
MacroHandler* macroHandler = nullptr;

extern HIDHandler* hidHandler;
extern USBCDC USBSerial;

MacroHandler::MacroHandler() {
    isExecutingMacro = false;
    nextCommandTime = 0;
    currentCommandIndex = 0;
    currentMacro = nullptr;
}

MacroHandler::~MacroHandler() {
    // Clean up any allocated memory for macros
    for (auto& macroPair : macros) {
        for (auto& cmd : macroPair.second.commands) {
            cleanupMacroCommand(cmd);
        }
    }
    macros.clear();
}

bool MacroHandler::begin() {
    if (!SPIFFS.begin(true)) {
        Serial.println("Failed to initialize SPIFFS for macro storage");
        return false;
    }
    
    // Create the macro directory if it doesn't exist
    if (!ensureMacroDirectoryExists()) {
        Serial.println("Failed to create macro directory");
        return false;
    }
    
    // Load all existing macros
    return loadMacros();
}

bool MacroHandler::ensureMacroDirectoryExists() {
    if (!SPIFFS.exists(MACRO_DIRECTORY)) {
        // Create the directory
        File root = SPIFFS.open(MACRO_DIRECTORY, FILE_WRITE);
        if (!root) {
            Serial.printf("Failed to create macro directory: %s\n", MACRO_DIRECTORY);
            return false;
        }
        root.close();
        
        // Create an empty index file
        File indexFile = SPIFFS.open(MACRO_INDEX_FILE, FILE_WRITE);
        if (!indexFile) {
            Serial.printf("Failed to create macro index file: %s\n", MACRO_INDEX_FILE);
            return false;
        }
        
        // Write an empty JSON array to the file
        indexFile.println("{ \"macros\": [] }");
        indexFile.close();
    }
    return true;
}

String MacroHandler::getMacroFilePath(const String& macroId) {
    // Sanitize the macroId (remove invalid characters)
    String sanitized = macroId;
    sanitized.replace("/", "_");
    sanitized.replace("\\", "_");
    
    return String(MACRO_DIRECTORY) + "/" + sanitized + ".json";
}

bool MacroHandler::loadMacros() {
    // Clear existing macros
    macros.clear();
    
    // Check if the index file exists
    if (!SPIFFS.exists(MACRO_INDEX_FILE)) {
        Serial.printf("Macro index file not found: %s\n", MACRO_INDEX_FILE);
        return false;
    }
    
    // Read the index file
    File indexFile = SPIFFS.open(MACRO_INDEX_FILE, "r");
    if (!indexFile) {
        Serial.printf("Failed to open macro index file: %s\n", MACRO_INDEX_FILE);
        return false;
    }
    
    // Parse the JSON index
    DynamicJsonDocument indexDoc(4096);
    DeserializationError error = deserializeJson(indexDoc, indexFile);
    indexFile.close();
    
    if (error) {
        Serial.printf("Failed to parse macro index: %s\n", error.c_str());
        return false;
    }
    
    // Iterate through the macro list
    JsonArray macroArray = indexDoc["macros"].as<JsonArray>();
    for (JsonObject macroInfo : macroArray) {
        String macroId = macroInfo["id"].as<String>();
        
        // Load each macro file
        String macroPath = getMacroFilePath(macroId);
        if (!SPIFFS.exists(macroPath)) {
            Serial.printf("Macro file not found: %s\n", macroPath.c_str());
            continue;
        }
        
        File macroFile = SPIFFS.open(macroPath, "r");
        if (!macroFile) {
            Serial.printf("Failed to open macro file: %s\n", macroPath.c_str());
            continue;
        }
        
        // Parse the macro JSON
        DynamicJsonDocument macroDoc(8192);
        error = deserializeJson(macroDoc, macroFile);
        macroFile.close();
        
        if (error) {
            Serial.printf("Failed to parse macro file %s: %s\n", macroPath.c_str(), error.c_str());
            continue;
        }
        
        // Convert to a Macro object
        Macro macro;
        if (parseMacroFromJson(macroDoc.as<JsonObject>(), macro)) {
            // Add to the map
            macros[macro.id] = macro;
            Serial.printf("Loaded macro: %s (%s)\n", macro.id.c_str(), macro.name.c_str());
        }
    }
    
    Serial.printf("Loaded %d macros\n", macros.size());
    return true;
}

bool MacroHandler::saveMacro(const Macro& macro) {
    // Convert to JSON
    DynamicJsonDocument macroDoc(8192);
    JsonObject macroJson = macroDoc.to<JsonObject>();
    
    if (!macroToJson(macro, macroJson)) {
        Serial.printf("Failed to convert macro to JSON: %s\n", macro.id.c_str());
        return false;
    }
    
    // Save to file
    String macroPath = getMacroFilePath(macro.id);
    File macroFile = SPIFFS.open(macroPath, FILE_WRITE);
    if (!macroFile) {
        Serial.printf("Failed to open macro file for writing: %s\n", macroPath.c_str());
        return false;
    }
    
    if (serializeJson(macroDoc, macroFile) == 0) {
        Serial.printf("Failed to write JSON to macro file: %s\n", macroPath.c_str());
        macroFile.close();
        return false;
    }
    
    macroFile.close();
    
    // Update the index
    return updateMacroIndex();
}

bool MacroHandler::updateMacroIndex() {
    DynamicJsonDocument indexDoc(4096);
    JsonArray macroArray = indexDoc.createNestedArray("macros");
    
    // Add each macro to the array
    for (const auto& macroPair : macros) {
        const Macro& macro = macroPair.second;
        JsonObject macroInfo = macroArray.createNestedObject();
        macroInfo["id"] = macro.id;
        macroInfo["name"] = macro.name;
    }
    
    // Save to file
    File indexFile = SPIFFS.open(MACRO_INDEX_FILE, FILE_WRITE);
    if (!indexFile) {
        Serial.printf("Failed to open macro index file for writing: %s\n", MACRO_INDEX_FILE);
        return false;
    }
    
    if (serializeJson(indexDoc, indexFile) == 0) {
        Serial.printf("Failed to write JSON to macro index file: %s\n", MACRO_INDEX_FILE);
        indexFile.close();
        return false;
    }
    
    indexFile.close();
    return true;
}

bool MacroHandler::parseMacroFromJson(const JsonObject& macroJson, Macro& outMacro) {
    if (!macroJson.containsKey("id") || !macroJson.containsKey("name") || !macroJson.containsKey("commands")) {
        Serial.println("Invalid macro format: missing required fields");
        return false;
    }
    
    outMacro.id = macroJson["id"].as<String>();
    outMacro.name = macroJson["name"].as<String>();
    
    if (macroJson.containsKey("description")) {
        outMacro.description = macroJson["description"].as<String>();
    } else {
        outMacro.description = "";
    }
    
    // Clear any existing commands
    for (auto& cmd : outMacro.commands) {
        cleanupMacroCommand(cmd);
    }
    outMacro.commands.clear();
    
    // Parse commands
    JsonArray commandsArray = macroJson["commands"].as<JsonArray>();
    for (JsonObject cmdJson : commandsArray) {
        if (!cmdJson.containsKey("type")) {
            Serial.println("Invalid command format: missing type");
            continue;
        }
        
        String cmdType = cmdJson["type"].as<String>();
        MacroCommand cmd;
        
        if (cmdType == "key_press") {
            cmd.type = MACRO_CMD_KEY_PRESS;
            if (cmdJson.containsKey("report")) {
                JsonArray reportArray = cmdJson["report"].as<JsonArray>();
                if (reportArray.size() > 8) {
                    Serial.println("Invalid key report: too large");
                    continue;
                }
                
                // Parse the hex values
                uint8_t report[8] = {0};
                for (size_t i = 0; i < reportArray.size() && i < 8; i++) {
                    report[i] = strtol(reportArray[i].as<const char*>(), NULL, 16);
                }
                
                // Copy to the command
                memcpy(cmd.data.keyPress.report, report, 8);
            }
        }
        else if (cmdType == "key_down") {
            cmd.type = MACRO_CMD_KEY_DOWN;
            if (cmdJson.containsKey("report")) {
                JsonArray reportArray = cmdJson["report"].as<JsonArray>();
                if (reportArray.size() > 8) {
                    Serial.println("Invalid key report: too large");
                    continue;
                }
                
                // Parse the hex values
                uint8_t report[8] = {0};
                for (size_t i = 0; i < reportArray.size() && i < 8; i++) {
                    report[i] = strtol(reportArray[i].as<const char*>(), NULL, 16);
                }
                
                // Copy to the command
                memcpy(cmd.data.keyPress.report, report, 8);
            }
        }
        else if (cmdType == "key_up") {
            cmd.type = MACRO_CMD_KEY_UP;
            // No additional data needed for key up
        }
        else if (cmdType == "consumer_press") {
            cmd.type = MACRO_CMD_CONSUMER_PRESS;
            if (cmdJson.containsKey("report")) {
                JsonArray reportArray = cmdJson["report"].as<JsonArray>();
                if (reportArray.size() > 4) {
                    Serial.println("Invalid consumer report: too large");
                    continue;
                }
                
                // Parse the hex values
                uint8_t report[4] = {0};
                for (size_t i = 0; i < reportArray.size() && i < 4; i++) {
                    report[i] = strtol(reportArray[i].as<const char*>(), NULL, 16);
                }
                
                // Copy to the command
                memcpy(cmd.data.consumerPress.report, report, 4);
            }
        }
        else if (cmdType == "delay") {
            cmd.type = MACRO_CMD_DELAY;
            if (cmdJson.containsKey("ms")) {
                cmd.data.delay.milliseconds = cmdJson["ms"].as<uint32_t>();
            } else {
                cmd.data.delay.milliseconds = 100; // Default delay
            }
        }
        else if (cmdType == "type_text") {
            cmd.type = MACRO_CMD_TYPE_TEXT;
            if (cmdJson.containsKey("text")) {
                String text = cmdJson["text"].as<String>();
                cmd.data.typeText.length = text.length();
                cmd.data.typeText.text = (char*)malloc(text.length() + 1);
                if (cmd.data.typeText.text) {
                    strcpy(cmd.data.typeText.text, text.c_str());
                } else {
                    Serial.println("Failed to allocate memory for text");
                    continue;
                }
            }
        }
        else if (cmdType == "execute_macro") {
            cmd.type = MACRO_CMD_EXECUTE_MACRO;
            if (cmdJson.containsKey("macro_id")) {
                String macroId = cmdJson["macro_id"].as<String>();
                cmd.data.executeMacro.macroId = (char*)malloc(macroId.length() + 1);
                if (cmd.data.executeMacro.macroId) {
                    strcpy(cmd.data.executeMacro.macroId, macroId.c_str());
                } else {
                    Serial.println("Failed to allocate memory for macro_id");
                    continue;
                }
            }
        }
        else {
            Serial.printf("Unknown command type: %s\n", cmdType.c_str());
            continue;
        }
        
        outMacro.commands.push_back(cmd);
    }
    
    return true;
}

bool MacroHandler::macroToJson(const Macro& macro, JsonObject& outJson) {
    outJson["id"] = macro.id;
    outJson["name"] = macro.name;
    outJson["description"] = macro.description;
    
    JsonArray commandsArray = outJson.createNestedArray("commands");
    
    for (const MacroCommand& cmd : macro.commands) {
        JsonObject cmdJson = commandsArray.createNestedObject();
        
        switch (cmd.type) {
            case MACRO_CMD_KEY_PRESS:
                cmdJson["type"] = "key_press";
                {
                    JsonArray reportArray = cmdJson.createNestedArray("report");
                    for (int i = 0; i < 8; i++) {
                        char hexValue[8];
                        sprintf(hexValue, "0x%02X", cmd.data.keyPress.report[i]);
                        reportArray.add(hexValue);
                    }
                }
                break;
                
            case MACRO_CMD_KEY_DOWN:
                cmdJson["type"] = "key_down";
                {
                    JsonArray reportArray = cmdJson.createNestedArray("report");
                    for (int i = 0; i < 8; i++) {
                        char hexValue[8];
                        sprintf(hexValue, "0x%02X", cmd.data.keyPress.report[i]);
                        reportArray.add(hexValue);
                    }
                }
                break;
                
            case MACRO_CMD_KEY_UP:
                cmdJson["type"] = "key_up";
                break;
                
            case MACRO_CMD_CONSUMER_PRESS:
                cmdJson["type"] = "consumer_press";
                {
                    JsonArray reportArray = cmdJson.createNestedArray("report");
                    for (int i = 0; i < 4; i++) {
                        char hexValue[8];
                        sprintf(hexValue, "0x%02X", cmd.data.consumerPress.report[i]);
                        reportArray.add(hexValue);
                    }
                }
                break;
                
            case MACRO_CMD_DELAY:
                cmdJson["type"] = "delay";
                cmdJson["ms"] = cmd.data.delay.milliseconds;
                break;
                
            case MACRO_CMD_TYPE_TEXT:
                cmdJson["type"] = "type_text";
                if (cmd.data.typeText.text) {
                    cmdJson["text"] = cmd.data.typeText.text;
                }
                break;
                
            case MACRO_CMD_EXECUTE_MACRO:
                cmdJson["type"] = "execute_macro";
                if (cmd.data.executeMacro.macroId) {
                    cmdJson["macro_id"] = cmd.data.executeMacro.macroId;
                }
                break;
                
            default:
                Serial.printf("Unknown command type: %d\n", cmd.type);
                continue;
        }
    }
    
    return true;
}

void MacroHandler::cleanupMacroCommand(MacroCommand& command) {
    switch (command.type) {
        case MACRO_CMD_TYPE_TEXT:
            if (command.data.typeText.text) {
                free(command.data.typeText.text);
                command.data.typeText.text = nullptr;
                command.data.typeText.length = 0;
            }
            break;
            
        case MACRO_CMD_EXECUTE_MACRO:
            if (command.data.executeMacro.macroId) {
                free(command.data.executeMacro.macroId);
                command.data.executeMacro.macroId = nullptr;
            }
            break;
            
        default:
            // No dynamic memory to free for other types
            break;
    }
}

bool MacroHandler::executeMacro(const String& macroId) {
    // Check if macro exists
    auto it = macros.find(macroId);
    if (it == macros.end()) {
        Serial.printf("Macro not found: %s\n", macroId.c_str());
        return false;
    }
    
    // If we're already executing a macro, push to the stack
    if (isExecutingMacro) {
        // Check for stack overflow or recursion
        if (macroExecutionStack.size() >= 5) {
            Serial.println("Macro execution stack overflow");
            return false;
        }
        
        // Check for recursive calls
        for (auto* m : macroExecutionStack) {
            if (m->id == macroId) {
                Serial.printf("Recursive macro call detected: %s\n", macroId.c_str());
                return false;
            }
        }
        
        if (currentMacro) {
            macroExecutionStack.push_back(currentMacro);
        }
    }
    
    // Start executing the new macro
    currentMacro = &(it->second);
    currentCommandIndex = 0;
    isExecutingMacro = true;
    nextCommandTime = millis();
    
    Serial.printf("Starting execution of macro: %s\n", macroId.c_str());
    return true;
}

bool MacroHandler::executeCommand(const MacroCommand& command) {
    bool result = false;
    
    switch (command.type) {
        case MACRO_CMD_KEY_PRESS:
            // Send the key press, then immediately release
            if (hidHandler) {
                hidHandler->sendKeyboardReport(command.data.keyPress.report, 8);
                delay(10); // Small delay to ensure the press is registered
                hidHandler->sendEmptyKeyboardReport();
                result = true;
            }
            break;
            
        case MACRO_CMD_KEY_DOWN:
            // Send the key press without releasing
            if (hidHandler) {
                result = hidHandler->sendKeyboardReport(command.data.keyPress.report, 8);
            }
            break;
            
        case MACRO_CMD_KEY_UP:
            // Release all keys
            if (hidHandler) {
                result = hidHandler->sendEmptyKeyboardReport();
            }
            break;
            
        case MACRO_CMD_CONSUMER_PRESS:
            // Send the consumer control press
            if (hidHandler) {
                hidHandler->sendConsumerReport(command.data.consumerPress.report, 4);
                delay(10); // Small delay to ensure the press is registered
                hidHandler->sendEmptyConsumerReport();
                result = true;
            }
            break;
            
        case MACRO_CMD_DELAY:
            // No action needed - the delay is handled by the update function
            // Just set the result to true to indicate success
            result = true;
            break;
            
        case MACRO_CMD_TYPE_TEXT: {
            // Type each character in the text
            if (command.data.typeText.text && hidHandler) {
                result = true; // Assume success until proven otherwise
                const char* text = command.data.typeText.text;
                for (size_t i = 0; i < command.data.typeText.length; i++) {
                    char c = text[i];
                    // Convert character to HID report and send
                    // This is a simplified version - actual implementation would need a proper
                    // ASCII to HID keyboard code mapping
                    uint8_t keyboardReport[8] = {0};
                    
                    // Simple ASCII to keyboard code conversion for demonstration
                    // A proper implementation would handle modifiers, special characters, etc.
                    if (c >= 'a' && c <= 'z') {
                        keyboardReport[2] = (c - 'a') + 4; // 'a' is keycode 4
                    } else if (c >= 'A' && c <= 'Z') {
                        keyboardReport[0] = 0x02; // Left shift
                        keyboardReport[2] = (c - 'A') + 4; // 'A' is keycode 4
                    } else if (c >= '1' && c <= '9') {
                        keyboardReport[2] = (c - '1') + 30; // '1' is keycode 30
                    } else if (c == '0') {
                        keyboardReport[2] = 39; // '0' is keycode 39
                    } else if (c == ' ') {
                        keyboardReport[2] = 44; // Space is keycode 44
                    }
                    // Additional characters would need to be mapped here
                    
                    hidHandler->sendKeyboardReport(keyboardReport, 8);
                    delay(10); // Small delay to ensure the key is registered
                    hidHandler->sendEmptyKeyboardReport();
                    delay(10); // Small delay between keypresses
                }
            }
            break;
        }
            
        case MACRO_CMD_EXECUTE_MACRO:
            // Execute another macro
            if (command.data.executeMacro.macroId) {
                result = executeMacro(String(command.data.executeMacro.macroId));
            }
            break;
            
        default:
            Serial.printf("Unknown command type: %d\n", command.type);
            break;
    }
    
    return result;
}

MacroCommand MacroHandler::createKeyPressCommand(const uint8_t* report) {
    MacroCommand cmd;
    cmd.type = MACRO_CMD_KEY_PRESS;
    memcpy(cmd.data.keyPress.report, report, 8);
    return cmd;
}

MacroCommand MacroHandler::createConsumerPressCommand(const uint8_t* report) {
    MacroCommand cmd;
    cmd.type = MACRO_CMD_CONSUMER_PRESS;
    memcpy(cmd.data.consumerPress.report, report, 4);
    return cmd;
}

MacroCommand MacroHandler::createDelayCommand(uint32_t milliseconds) {
    MacroCommand cmd;
    cmd.type = MACRO_CMD_DELAY;
    cmd.data.delay.milliseconds = milliseconds;
    return cmd;
}

MacroCommand MacroHandler::createTypeTextCommand(const char* text) {
    MacroCommand cmd;
    cmd.type = MACRO_CMD_TYPE_TEXT;
    
    size_t len = strlen(text);
    cmd.data.typeText.length = len;
    cmd.data.typeText.text = (char*)malloc(len + 1);
    
    if (cmd.data.typeText.text) {
        strcpy(cmd.data.typeText.text, text);
    } else {
        Serial.println("Failed to allocate memory for text command");
    }
    
    return cmd;
}

MacroCommand MacroHandler::createExecuteMacroCommand(const char* macroId) {
    MacroCommand cmd;
    cmd.type = MACRO_CMD_EXECUTE_MACRO;
    
    size_t len = strlen(macroId);
    cmd.data.executeMacro.macroId = (char*)malloc(len + 1);
    
    if (cmd.data.executeMacro.macroId) {
        strcpy(cmd.data.executeMacro.macroId, macroId);
    } else {
        Serial.println("Failed to allocate memory for macro ID");
    }
    
    return cmd;
}

bool MacroHandler::createMacro(const String& id, const String& name, const String& description) {
    // Check if macro already exists
    if (macros.find(id) != macros.end()) {
        Serial.printf("Macro already exists: %s\n", id.c_str());
        return false;
    }
    
    // Create a new macro
    Macro macro;
    macro.id = id;
    macro.name = name;
    macro.description = description;
    
    // Add to the map
    macros[id] = macro;
    
    // Save to storage
    return saveMacro(macro);
}

bool MacroHandler::addCommandToMacro(const String& macroId, const MacroCommand& command) {
    // Check if macro exists
    auto it = macros.find(macroId);
    if (it == macros.end()) {
        Serial.printf("Macro not found: %s\n", macroId.c_str());
        return false;
    }
    
    // Add the command to the macro
    Macro& macro = it->second;
    macro.commands.push_back(command);
    
    // Save the updated macro
    return saveMacro(macro);
}

bool MacroHandler::deleteMacro(const String& macroId) {
    // Check if macro exists
    auto it = macros.find(macroId);
    if (it == macros.end()) {
        Serial.printf("Macro not found: %s\n", macroId.c_str());
        return false;
    }
    
    // Cleanup the macro commands
    for (auto& cmd : it->second.commands) {
        cleanupMacroCommand(cmd);
    }
    
    // Remove from the map
    macros.erase(it);
    
    // Delete the file
    String macroPath = getMacroFilePath(macroId);
    if (SPIFFS.exists(macroPath)) {
        if (!SPIFFS.remove(macroPath)) {
            Serial.printf("Failed to delete macro file: %s\n", macroPath.c_str());
            return false;
        }
    }
    
    // Update the index
    return updateMacroIndex();
}

std::vector<String> MacroHandler::getAvailableMacros() {
    std::vector<String> result;
    for (const auto& macroPair : macros) {
        result.push_back(macroPair.first);
    }
    return result;
}

bool MacroHandler::getMacro(const String& macroId, Macro& outMacro) {
    // Check if macro exists
    auto it = macros.find(macroId);
    if (it == macros.end()) {
        Serial.printf("Macro not found: %s\n", macroId.c_str());
        return false;
    }
    
    // Copy the macro
    outMacro = it->second;
    return true;
}

void MacroHandler::update() {
    if (!isExecutingMacro || !currentMacro) {
        return;
    }
    
    // Check if it's time to execute the next command
    unsigned long currentTime = millis();
    if (currentTime < nextCommandTime) {
        return;
    }
    
    // Check if we've reached the end of the macro
    if (currentCommandIndex >= currentMacro->commands.size()) {
        // Macro complete - check if we have more macros on the stack
        if (!macroExecutionStack.empty()) {
            // Pop the next macro from the stack
            currentMacro = macroExecutionStack.back();
            macroExecutionStack.pop_back();
            currentCommandIndex = 0;
            nextCommandTime = currentTime;
            
            Serial.printf("Resuming macro: %s\n", currentMacro->id.c_str());
        } else {
            // No more macros to execute
            isExecutingMacro = false;
            currentMacro = nullptr;
            Serial.println("Macro execution complete");
        }
        return;
    }
    
    // Execute the current command
    const MacroCommand& cmd = currentMacro->commands[currentCommandIndex];
    bool success = executeCommand(cmd);
    
    if (success) {
        // Move to the next command
        currentCommandIndex++;
        
        // Set the time for the next command execution
        // For delay commands, use the specified delay
        // For other commands, use a small default delay
        if (cmd.type == MACRO_CMD_DELAY) {
            nextCommandTime = currentTime + cmd.data.delay.milliseconds;
        } else {
            nextCommandTime = currentTime + 50; // Default delay between commands
        }
    } else {
        // Command execution failed - stop executing the macro
        Serial.println("Macro command execution failed");
        isExecutingMacro = false;
        currentMacro = nullptr;
        macroExecutionStack.clear();
    }
}

// Global function implementations
void initializeMacroHandler() {
    if (!macroHandler) {
        macroHandler = new MacroHandler();
        if (macroHandler) {
            if (!macroHandler->begin()) {
                Serial.println("Failed to initialize macro handler");
                delete macroHandler;
                macroHandler = nullptr;
            } else {
                Serial.println("Macro handler initialized");
            }
        }
    }
}

void updateMacroHandler() {
    if (macroHandler) {
        macroHandler->update();
    }
}

void cleanupMacroHandler() {
    if (macroHandler) {
        delete macroHandler;
        macroHandler = nullptr;
    }
} 