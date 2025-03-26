#include "MacroHandler.h"
#include <Arduino.h>
#include <FS.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include "HIDHandler.h"
#include <USB.h>
#include <USBHID.h>
#include <USBHIDMouse.h>

// Global instance
MacroHandler* macroHandler = nullptr;

extern HIDHandler* hidHandler;
extern USBCDC USBSerial;
extern USBHIDMouse Mouse;

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

bool MacroHandler::parseMacroFromJson(const JsonObject& macroObj, Macro& macro) {
    // Clear existing macro data
    macro.commands.clear();
    
    if (!macroObj.containsKey("id") || !macroObj.containsKey("name") || !macroObj.containsKey("commands")) {
        USBSerial.println("Error: Macro missing required fields");
        return false;
    }
    
    macro.id = macroObj["id"].as<String>();
    macro.name = macroObj["name"].as<String>();
    
    if (macroObj.containsKey("description")) {
        macro.description = macroObj["description"].as<String>();
    } else {
        macro.description = "";
    }
    
    JsonArray commands = macroObj["commands"].as<JsonArray>();
    
    for (JsonObject cmdObj : commands) {
        if (!cmdObj.containsKey("type")) {
            USBSerial.println("Error: Command missing type field");
            continue;
        }
        
        String cmdType = cmdObj["type"].as<String>();
        MacroCommand cmd;
        
        if (cmdType == "key_press" || cmdType == "key_down" || cmdType == "key_up") {
            if (!cmdObj.containsKey("report")) {
                USBSerial.println("Error: Key command missing report field");
                continue;
            }
            
            // Set command type
            if (cmdType == "key_press") {
                cmd.type = MACRO_CMD_KEY_PRESS;
            } else if (cmdType == "key_down") {
                cmd.type = MACRO_CMD_KEY_DOWN;
            } else {
                cmd.type = MACRO_CMD_KEY_UP;
            }
            
            // Parse the HID report
            JsonArray reportArr = cmdObj["report"].as<JsonArray>();
            if (reportArr.size() > 8) {
                USBSerial.println("Error: HID report too long");
                continue;
            }
            
            memset(cmd.data.keyPress.report, 0, 8);
            int i = 0;
            for (JsonVariant reportVal : reportArr) {
                if (i < 8) {
                    cmd.data.keyPress.report[i++] = reportVal.as<uint8_t>();
                }
            }
            
        } else if (cmdType == "consumer_press") {
            cmd.type = MACRO_CMD_CONSUMER_PRESS;
            
            if (!cmdObj.containsKey("report")) {
                USBSerial.println("Error: Consumer command missing report field");
                continue;
            }
            
            // Parse the consumer report
            JsonArray reportArr = cmdObj["report"].as<JsonArray>();
            if (reportArr.size() > 4) {
                USBSerial.println("Error: Consumer report too long");
                continue;
            }
            
            memset(cmd.data.consumerPress.report, 0, 4);
            int i = 0;
            for (JsonVariant reportVal : reportArr) {
                if (i < 4) {
                    cmd.data.consumerPress.report[i++] = reportVal.as<uint8_t>();
                }
            }
            
        } else if (cmdType == "delay") {
            cmd.type = MACRO_CMD_DELAY;
            
            if (!cmdObj.containsKey("milliseconds")) {
                USBSerial.println("Error: Delay command missing milliseconds field");
                continue;
            }
            
            cmd.data.delay.milliseconds = cmdObj["milliseconds"].as<uint32_t>();
            
        } else if (cmdType == "type_text") {
            cmd.type = MACRO_CMD_TYPE_TEXT;
            
            if (!cmdObj.containsKey("text")) {
                USBSerial.println("Error: Type text command missing text field");
                continue;
            }
            
            String textStr = cmdObj["text"].as<String>();
            cmd.data.typeText.length = textStr.length();
            cmd.data.typeText.text = (char*)malloc(cmd.data.typeText.length + 1);
            if (cmd.data.typeText.text) {
                strcpy(cmd.data.typeText.text, textStr.c_str());
            } else {
                USBSerial.println("Error: Failed to allocate memory for text");
                continue;
            }
            
        } else if (cmdType == "execute_macro") {
            cmd.type = MACRO_CMD_EXECUTE_MACRO;
            
            if (!cmdObj.containsKey("macro_id")) {
                USBSerial.println("Error: Execute macro command missing macro_id field");
                continue;
            }
            
            String macroIdStr = cmdObj["macro_id"].as<String>();
            cmd.data.executeMacro.macroId = (char*)malloc(macroIdStr.length() + 1);
            if (cmd.data.executeMacro.macroId) {
                strcpy(cmd.data.executeMacro.macroId, macroIdStr.c_str());
            } else {
                USBSerial.println("Error: Failed to allocate memory for macro ID");
                continue;
            }
            
        } else if (cmdType == "mouse_move") {
            cmd.type = MACRO_CMD_MOUSE_MOVE;
            
            if (!cmdObj.containsKey("x") || !cmdObj.containsKey("y")) {
                USBSerial.println("Error: Mouse move command missing x or y coordinates");
                continue;
            }
            
            cmd.data.mouseMove.x = cmdObj["x"].as<int16_t>();
            cmd.data.mouseMove.y = cmdObj["y"].as<int16_t>();
            
            // Speed is optional, default to medium speed (5)
            cmd.data.mouseMove.speed = cmdObj.containsKey("speed") ? 
                                      cmdObj["speed"].as<uint8_t>() : 5;
            
            // Clamp speed to valid range (1-10)
            if (cmd.data.mouseMove.speed < 1) cmd.data.mouseMove.speed = 1;
            if (cmd.data.mouseMove.speed > 10) cmd.data.mouseMove.speed = 10;
            
        } else if (cmdType == "mouse_click") {
            cmd.type = MACRO_CMD_MOUSE_CLICK;
            
            if (!cmdObj.containsKey("button")) {
                USBSerial.println("Error: Mouse click command missing button field");
                continue;
            }
            
            // Handle button as either string name or numeric value
            if (cmdObj["button"].is<String>()) {
                String buttonStr = cmdObj["button"].as<String>();
                if (buttonStr == "left") {
                    cmd.data.mouseClick.button = MOUSE_LEFT;
                } else if (buttonStr == "right") {
                    cmd.data.mouseClick.button = MOUSE_RIGHT;
                } else if (buttonStr == "middle") {
                    cmd.data.mouseClick.button = MOUSE_MIDDLE;
                } else if (buttonStr == "back") {
                    cmd.data.mouseClick.button = MOUSE_BACK;
                } else if (buttonStr == "forward") {
                    cmd.data.mouseClick.button = MOUSE_FORWARD;
                } else {
                    USBSerial.println("Error: Unknown mouse button name");
                    continue;
                }
            } else {
                cmd.data.mouseClick.button = cmdObj["button"].as<uint8_t>();
            }
            
            // Clicks is optional, default to 1
            cmd.data.mouseClick.clicks = cmdObj.containsKey("clicks") ? 
                                       cmdObj["clicks"].as<uint8_t>() : 1;
            
            // Clamp clicks to valid range (1-3)
            if (cmd.data.mouseClick.clicks < 1) cmd.data.mouseClick.clicks = 1;
            if (cmd.data.mouseClick.clicks > 3) cmd.data.mouseClick.clicks = 3;
            
        } else if (cmdType == "mouse_scroll") {
            cmd.type = MACRO_CMD_MOUSE_SCROLL;
            
            if (!cmdObj.containsKey("amount")) {
                USBSerial.println("Error: Mouse scroll command missing amount field");
                continue;
            }
            
            cmd.data.mouseScroll.amount = cmdObj["amount"].as<int8_t>();
            
        } else if (cmdType == "repeat_start") {
            cmd.type = MACRO_CMD_REPEAT_START;
            
            if (!cmdObj.containsKey("count")) {
                USBSerial.println("Error: Repeat start command missing count field");
                continue;
            }
            
            cmd.data.repeatStart.count = cmdObj["count"].as<uint16_t>();
            
            // Ensure count is at least 2
            if (cmd.data.repeatStart.count < 2) cmd.data.repeatStart.count = 2;
            
        } else if (cmdType == "repeat_end") {
            cmd.type = MACRO_CMD_REPEAT_END;
            // No additional data needed for repeat end
            
        } else if (cmdType == "random_delay") {
            cmd.type = MACRO_CMD_RANDOM_DELAY;
            
            if (!cmdObj.containsKey("min_time") || !cmdObj.containsKey("max_time")) {
                USBSerial.println("Error: Random delay command missing time range fields");
                continue;
            }
            
            cmd.data.randomDelay.minTime = cmdObj["min_time"].as<uint32_t>();
            cmd.data.randomDelay.maxTime = cmdObj["max_time"].as<uint32_t>();
            
            // Ensure min_time <= max_time
            if (cmd.data.randomDelay.minTime > cmd.data.randomDelay.maxTime) {
                uint32_t temp = cmd.data.randomDelay.minTime;
                cmd.data.randomDelay.minTime = cmd.data.randomDelay.maxTime;
                cmd.data.randomDelay.maxTime = temp;
            }
            
        } else {
            USBSerial.printf("Error: Unknown command type: %s\n", cmdType.c_str());
            continue;
        }
        
        // Add the command to the macro
        macro.commands.push_back(cmd);
    }
    
    return true;
}

bool MacroHandler::macroToJson(const Macro& macro, JsonObject& macroObj) {
    macroObj["id"] = macro.id;
    macroObj["name"] = macro.name;
    macroObj["description"] = macro.description;
    
    JsonArray commandsArray = macroObj.createNestedArray("commands");
    
    for (const MacroCommand& cmd : macro.commands) {
        JsonObject cmdObj = commandsArray.createNestedObject();
        
        switch (cmd.type) {
            case MACRO_CMD_KEY_PRESS:
                cmdObj["type"] = "key_press";
                {
                    JsonArray reportArray = cmdObj.createNestedArray("report");
                    for (int i = 0; i < 8; i++) {
                        reportArray.add(cmd.data.keyPress.report[i]);
                    }
                }
                break;
                
            case MACRO_CMD_KEY_DOWN:
                cmdObj["type"] = "key_down";
                {
                    JsonArray reportArray = cmdObj.createNestedArray("report");
                    for (int i = 0; i < 8; i++) {
                        reportArray.add(cmd.data.keyPress.report[i]);
                    }
                }
                break;
                
            case MACRO_CMD_KEY_UP:
                cmdObj["type"] = "key_up";
                break;
                
            case MACRO_CMD_CONSUMER_PRESS:
                cmdObj["type"] = "consumer_press";
                {
                    JsonArray reportArray = cmdObj.createNestedArray("report");
                    for (int i = 0; i < 4; i++) {
                        reportArray.add(cmd.data.consumerPress.report[i]);
                    }
                }
                break;
                
            case MACRO_CMD_DELAY:
                cmdObj["type"] = "delay";
                cmdObj["milliseconds"] = cmd.data.delay.milliseconds;
                break;
                
            case MACRO_CMD_TYPE_TEXT:
                cmdObj["type"] = "type_text";
                if (cmd.data.typeText.text) {
                    cmdObj["text"] = cmd.data.typeText.text;
                }
                break;
                
            case MACRO_CMD_EXECUTE_MACRO:
                cmdObj["type"] = "execute_macro";
                if (cmd.data.executeMacro.macroId) {
                    cmdObj["macro_id"] = cmd.data.executeMacro.macroId;
                }
                break;
                
            case MACRO_CMD_MOUSE_MOVE:
                cmdObj["type"] = "mouse_move";
                cmdObj["x"] = cmd.data.mouseMove.x;
                cmdObj["y"] = cmd.data.mouseMove.y;
                cmdObj["speed"] = cmd.data.mouseMove.speed;
                break;
                
            case MACRO_CMD_MOUSE_CLICK:
                cmdObj["type"] = "mouse_click";
                
                // Convert numeric button to string name for better readability
                switch (cmd.data.mouseClick.button) {
                    case MOUSE_LEFT:
                        cmdObj["button"] = "left";
                        break;
                    case MOUSE_RIGHT:
                        cmdObj["button"] = "right";
                        break;
                    case MOUSE_MIDDLE:
                        cmdObj["button"] = "middle";
                        break;
                    case MOUSE_BACK:
                        cmdObj["button"] = "back";
                        break;
                    case MOUSE_FORWARD:
                        cmdObj["button"] = "forward";
                        break;
                    default:
                        cmdObj["button"] = cmd.data.mouseClick.button;
                        break;
                }
                
                cmdObj["clicks"] = cmd.data.mouseClick.clicks;
                break;
                
            case MACRO_CMD_MOUSE_SCROLL:
                cmdObj["type"] = "mouse_scroll";
                cmdObj["amount"] = cmd.data.mouseScroll.amount;
                break;
                
            case MACRO_CMD_REPEAT_START:
                cmdObj["type"] = "repeat_start";
                cmdObj["count"] = cmd.data.repeatStart.count;
                break;
                
            case MACRO_CMD_REPEAT_END:
                cmdObj["type"] = "repeat_end";
                break;
                
            case MACRO_CMD_RANDOM_DELAY:
                cmdObj["type"] = "random_delay";
                cmdObj["min_time"] = cmd.data.randomDelay.minTime;
                cmdObj["max_time"] = cmd.data.randomDelay.maxTime;
                break;
                
            default:
                USBSerial.printf("Error: Unknown command type: %d\n", cmd.type);
                return false;
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
    // Reset any execution flags for new execution
    static int repeatStartIndex = -1;
    static int repeatCount = 0;
    static int repeatCounter = 0;
    
    // Pre-declare all variables used in case statements to fix scope issues
    int speed = 0;
    int steps = 0;
    int x_step = 0;
    int y_step = 0;
    int x_remainder = 0;
    int y_remainder = 0;
    int multiplier = 0;
    uint8_t button = 0;
    uint32_t minTime = 0;
    uint32_t maxTime = 0;
    uint32_t randomDelay = 0;
    
    switch (command.type) {
        case MACRO_CMD_KEY_PRESS:
            if (hidHandler) {
                USBSerial.println("Executing key press command");
                hidHandler->sendKeyboardReport(command.data.keyPress.report);
                delay(50); // Small delay to ensure keypress is registered
                hidHandler->sendEmptyKeyboardReport();
                return true;
            }
            break;
            
        case MACRO_CMD_KEY_DOWN:
            if (hidHandler) {
                USBSerial.println("Executing key down command");
                hidHandler->sendKeyboardReport(command.data.keyPress.report);
                return true;
            }
            break;
            
        case MACRO_CMD_KEY_UP:
            if (hidHandler) {
                USBSerial.println("Executing key up command");
                hidHandler->sendEmptyKeyboardReport();
                return true;
            }
            break;
            
        case MACRO_CMD_CONSUMER_PRESS:
            if (hidHandler) {
                USBSerial.println("Executing consumer control command");
                hidHandler->sendConsumerReport(command.data.consumerPress.report);
                delay(50); // Small delay to ensure press is registered
                hidHandler->sendEmptyConsumerReport();
                return true;
            }
            break;
            
        case MACRO_CMD_DELAY:
            USBSerial.printf("Executing delay: %d ms\n", command.data.delay.milliseconds);
            delay(command.data.delay.milliseconds);
            return true;
            
        case MACRO_CMD_TYPE_TEXT:
            if (command.data.typeText.text) {
                USBSerial.printf("Typing text: %s\n", command.data.typeText.text);
                // Implement text typing using individual key presses
                // This is a placeholder for actual implementation
                const char* text = command.data.typeText.text;
                for (size_t i = 0; i < command.data.typeText.length; i++) {
                    char c = text[i];
                    // Convert character to keypress and send
                    // This is simplified - a real implementation would map characters to key codes
                    uint8_t report[8] = {0};
                    
                    if (c >= 'a' && c <= 'z') {
                        report[2] = 4 + (c - 'a'); // USB HID code for a-z is 4-29
                    } else if (c >= 'A' && c <= 'Z') {
                        report[0] = 0x02; // Left shift modifier
                        report[2] = 4 + (c - 'A'); // USB HID code for a-z is 4-29
                    } else if (c >= '1' && c <= '9') {
                        report[2] = 30 + (c - '1'); // USB HID code for 1-9 is 30-38
                    } else if (c == '0') {
                        report[2] = 39; // USB HID code for 0 is 39
                    } else if (c == ' ') {
                        report[2] = 44; // USB HID code for space is 44
                    }
                    // Add more character mappings as needed
                    
                    if (report[2] != 0 && hidHandler) {
                        hidHandler->sendKeyboardReport(report);
                        delay(10); // Small delay between keypresses
                        hidHandler->sendEmptyKeyboardReport();
                        delay(5);
                    }
                }
                return true;
            }
            break;
            
        case MACRO_CMD_EXECUTE_MACRO:
            if (command.data.executeMacro.macroId) {
                USBSerial.printf("Executing macro: %s\n", command.data.executeMacro.macroId);
                executeMacro(command.data.executeMacro.macroId);
                return true;
            }
            break;
            
        case MACRO_CMD_MOUSE_MOVE:
            USBSerial.printf("Moving mouse: x=%d, y=%d, speed=%d\n", 
                          command.data.mouseMove.x, 
                          command.data.mouseMove.y,
                          command.data.mouseMove.speed);
                          
            // Apply the movement with the specified speed
            // Speed is used to determine how many times to move (for smaller, smoother movements)
            // or as a multiplier (for faster, larger movements)
            speed = command.data.mouseMove.speed;
            if (speed <= 5) {
                // For slower speeds, move multiple times with small increments
                steps = 10 - speed; // 5->5 steps, 1->9 steps
                x_step = command.data.mouseMove.x / steps;
                y_step = command.data.mouseMove.y / steps;
                
                for (int i = 0; i < steps; i++) {
                    Mouse.move(x_step, y_step);
                    delay(10);
                }
                
                // Handle any remainder
                x_remainder = command.data.mouseMove.x % steps;
                y_remainder = command.data.mouseMove.y % steps;
                if (x_remainder || y_remainder) {
                    Mouse.move(x_remainder, y_remainder);
                }
            } else {
                // For faster speeds, multiply the movement
                multiplier = speed - 4; // 6->2x, 10->6x
                Mouse.move(command.data.mouseMove.x * multiplier, 
                          command.data.mouseMove.y * multiplier);
            }
            return true;
            
        case MACRO_CMD_MOUSE_CLICK:
            USBSerial.printf("Mouse click: button=%d, clicks=%d\n", 
                          command.data.mouseClick.button, 
                          command.data.mouseClick.clicks);
                          
            // Convert button name to button value if needed
            button = command.data.mouseClick.button;
            
            // Execute single or multiple clicks
            for (uint8_t i = 0; i < command.data.mouseClick.clicks; i++) {
                Mouse.click(button);
                
                // Add a small delay between multiple clicks
                if (i < command.data.mouseClick.clicks - 1) {
                    delay(50);
                }
            }
            return true;
            
        case MACRO_CMD_MOUSE_SCROLL:
            USBSerial.printf("Mouse scroll: amount=%d\n", command.data.mouseScroll.amount);
            
            // Scroll the specified amount - USBHIDMouse uses move(x, y, wheel)
            // where wheel is the scroll amount
            Mouse.move(0, 0, command.data.mouseScroll.amount);
            return true;
            
        case MACRO_CMD_REPEAT_START:
            USBSerial.printf("Starting repeat block: count=%d\n", command.data.repeatStart.count);
            repeatStartIndex = currentCommandIndex;
            repeatCount = command.data.repeatStart.count;
            repeatCounter = 0;
            return true;
            
        case MACRO_CMD_REPEAT_END:
            USBSerial.println("End of repeat block");
            if (repeatStartIndex >= 0 && repeatCounter < repeatCount - 1) {
                // Jump back to the repeat start command
                repeatCounter++;
                USBSerial.printf("Repeating block: iteration %d/%d\n", 
                              repeatCounter + 1, repeatCount);
                currentCommandIndex = repeatStartIndex;
            } else {
                // Reset repeat state
                repeatStartIndex = -1;
                repeatCount = 0;
                repeatCounter = 0;
            }
            return true;
            
        case MACRO_CMD_RANDOM_DELAY:
            // Generate a random delay between min and max
            minTime = command.data.randomDelay.minTime;
            maxTime = command.data.randomDelay.maxTime;
            randomDelay = random(minTime, maxTime + 1);
            
            USBSerial.printf("Random delay: %d ms (range: %d-%d ms)\n", 
                          randomDelay, minTime, maxTime);
                          
            delay(randomDelay);
            return true;
            
        default:
            USBSerial.printf("Unknown command type: %d\n", command.type);
            break;
    }
    
    return false;
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

// Create mouse move command
MacroCommand MacroHandler::createMouseMoveCommand(int16_t x, int16_t y, uint8_t speed) {
    MacroCommand cmd;
    cmd.type = MACRO_CMD_MOUSE_MOVE;
    cmd.data.mouseMove.x = x;
    cmd.data.mouseMove.y = y;
    cmd.data.mouseMove.speed = speed;
    return cmd;
}

// Create mouse click command
MacroCommand MacroHandler::createMouseClickCommand(uint8_t button, uint8_t clicks) {
    MacroCommand cmd;
    cmd.type = MACRO_CMD_MOUSE_CLICK;
    cmd.data.mouseClick.button = button;
    cmd.data.mouseClick.clicks = clicks;
    return cmd;
}

// Create mouse scroll command
MacroCommand MacroHandler::createMouseScrollCommand(int8_t amount) {
    MacroCommand cmd;
    cmd.type = MACRO_CMD_MOUSE_SCROLL;
    cmd.data.mouseScroll.amount = amount;
    return cmd;
}

// Create repeat start command
MacroCommand MacroHandler::createRepeatStartCommand(uint16_t count) {
    MacroCommand cmd;
    cmd.type = MACRO_CMD_REPEAT_START;
    cmd.data.repeatStart.count = count;
    return cmd;
}

// Create repeat end command
MacroCommand MacroHandler::createRepeatEndCommand() {
    MacroCommand cmd;
    cmd.type = MACRO_CMD_REPEAT_END;
    return cmd;
}

// Create random delay command
MacroCommand MacroHandler::createRandomDelayCommand(uint32_t minTime, uint32_t maxTime) {
    MacroCommand cmd;
    cmd.type = MACRO_CMD_RANDOM_DELAY;
    cmd.data.randomDelay.minTime = minTime;
    cmd.data.randomDelay.maxTime = maxTime;
    return cmd;
} 