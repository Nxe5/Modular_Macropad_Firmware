#include "MacroHandler.h"
#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>
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
    // Constructor - all members are already initialized in the class definition
    // No additional initialization needed
}

bool MacroHandler::begin() {
    // LittleFS should already be initialized in main.cpp
    if (!LittleFS.exists("/macros")) {
        USBSerial.println("Creating macros directory");
        if (!LittleFS.mkdir("/macros")) {
            USBSerial.println("Failed to create macros directory");
            return false;
        }
    }
    
    // Load all existing macros
    return loadMacros();
}

void MacroHandler::ensureMacroDirectoryExists() {
    if (!LittleFS.exists("/macros")) {
        USBSerial.println("Creating macros directory");
        LittleFS.mkdir("/macros");
    }
}

bool MacroHandler::loadMacros() {
    USBSerial.println("Loading macros from filesystem...");
    
    macros.clear();
    
    // Ensure the macros directory exists
    ensureMacroDirectoryExists();
    
    // Scan the macros directory directly without using index.json
    File root = LittleFS.open("/macros");
    if (!root) {
        USBSerial.println("Failed to open macros directory");
        return false;
    }
    
    if (!root.isDirectory()) {
        USBSerial.println("Not a directory");
        return false;
    }
    
    File file = root.openNextFile();
    
    // Count of successfully loaded macros
    int loadedCount = 0;
    
    while (file) {
        String path = file.name();
        
        // Skip index.json file if it exists and non-json files
        if (!path.endsWith(".json") || path.endsWith("index.json")) {
            file = root.openNextFile();
            continue;
        }
        
        USBSerial.print("Loading macro from file: ");
        USBSerial.println(path);
        
        // Load the macro file
        String macroJson = file.readString();
        file.close();
        
        // Parse the macro JSON
        DynamicJsonDocument doc(8192);
        DeserializationError error = deserializeJson(doc, macroJson);
        
        if (error) {
            USBSerial.print("Failed to parse macro JSON: ");
            USBSerial.println(error.c_str());
            file = root.openNextFile();
            continue;
        }
        
        // Create a macro from the JSON
        Macro macro;
        if (parseMacroFromJson(doc.as<JsonObject>(), macro)) {
            // Add to the macros map
            macros[macro.id] = macro;
            loadedCount++;
            USBSerial.print("Loaded macro: ");
            USBSerial.println(macro.id);
        } else {
            USBSerial.print("Failed to parse macro: ");
            USBSerial.println(path);
        }
        
        // Move to next file
        file = root.openNextFile();
    }
    
    USBSerial.print("Loaded ");
    USBSerial.print(loadedCount);
    USBSerial.println(" macros");
    
    return true;
}

bool MacroHandler::saveMacro(const Macro& macro) {
    // Convert to JSON
    DynamicJsonDocument macroDoc(16384);
    
    // Fill in the JSON document
    macroDoc["id"] = macro.id;
    macroDoc["name"] = macro.name;
    macroDoc["description"] = macro.description;
    
    JsonArray commandsArray = macroDoc.createNestedArray("commands");
    
    for (const MacroCommand& cmd : macro.commands) {
        JsonObject cmdObj = commandsArray.createNestedObject();
        
        switch (cmd.type) {
            case MACRO_CMD_KEY_PRESS:
                cmdObj["type"] = "key_press";
                {
                    JsonArray reportArray = cmdObj.createNestedArray("report");
                    for (int i = 0; i < 8; i++) {
                        char hexStr[8];
                        sprintf(hexStr, "0x%02X", cmd.data.keyPress.report[i]);
                        reportArray.add(hexStr);
                    }
                }
                break;
                
            case MACRO_CMD_KEY_DOWN:
                cmdObj["type"] = "key_down";
                {
                    JsonArray reportArray = cmdObj.createNestedArray("report");
                    for (int i = 0; i < 8; i++) {
                        char hexStr[8];
                        sprintf(hexStr, "0x%02X", cmd.data.keyPress.report[i]);
                        reportArray.add(hexStr);
                    }
                }
                break;
                
            case MACRO_CMD_KEY_UP:
                cmdObj["type"] = "key_up";
                {
                    JsonArray reportArray = cmdObj.createNestedArray("report");
                    for (int i = 0; i < 8; i++) {
                        char hexStr[8];
                        sprintf(hexStr, "0x%02X", cmd.data.keyPress.report[i]);
                        reportArray.add(hexStr);
                    }
                }
                break;
                
            case MACRO_CMD_CONSUMER_PRESS:
                cmdObj["type"] = "consumer_press";
                {
                    JsonArray reportArray = cmdObj.createNestedArray("report");
                    for (int i = 0; i < 4; i++) {
                        char hexStr[8];
                        sprintf(hexStr, "0x%02X", cmd.data.consumerPress.report[i]);
                        reportArray.add(hexStr);
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
                    case MB_LEFT:
                        cmdObj["button"] = "left";
                        break;
                    case MB_RIGHT:
                        cmdObj["button"] = "right";
                        break;
                    case MB_MIDDLE:
                        cmdObj["button"] = "middle";
                        break;
                    case MB_BACK:
                        cmdObj["button"] = "back";
                        break;
                    case MB_FORWARD:
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
    
    // Save to file
    String macroPath = getMacroFilePath(macro.id);
    File macroFile = LittleFS.open(macroPath, FILE_WRITE);
    if (!macroFile) {
        USBSerial.printf("Failed to open macro file for writing: %s\n", macroPath.c_str());
        return false;
    }
    
    if (serializeJson(macroDoc, macroFile) == 0) {
        USBSerial.printf("Failed to write JSON to macro file: %s\n", macroPath.c_str());
        macroFile.close();
        return false;
    }
    
    macroFile.close();
    
    // Update the macros map
    macros[macro.id] = macro;
    
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
            JsonArray reportArray = cmdObj["report"].as<JsonArray>();
            for (size_t i = 0; i < min(reportArray.size(), (size_t)8); i++) {
                // Handle both numeric and hex string representations
                if (reportArray[i].is<int>()) {
                    cmd.data.keyPress.report[i] = reportArray[i].as<uint8_t>();
                } else if (reportArray[i].is<const char*>()) {
                    // Parse hex string (format: 0x00)
                    String hexValue = reportArray[i].as<String>();
                    if (hexValue.startsWith("0x")) {
                        cmd.data.keyPress.report[i] = strtol(hexValue.c_str(), NULL, 16);
                    } else {
                        // Try to parse as decimal
                        cmd.data.keyPress.report[i] = hexValue.toInt();
                    }
                }
            }
            
        } else if (cmdType == "consumer_press") {
            cmd.type = MACRO_CMD_CONSUMER_PRESS;
            
            if (!cmdObj.containsKey("report")) {
                USBSerial.println("Error: Consumer command missing report field");
                continue;
            }
            
            // Parse the consumer report
            JsonArray consumerReportArray = cmdObj["report"].as<JsonArray>();
            for (size_t i = 0; i < min(consumerReportArray.size(), (size_t)4); i++) {
                // Handle both numeric and hex string representations
                if (consumerReportArray[i].is<int>()) {
                    cmd.data.consumerPress.report[i] = consumerReportArray[i].as<uint8_t>();
                } else if (consumerReportArray[i].is<const char*>()) {
                    // Parse hex string (format: 0x00)
                    String hexValue = consumerReportArray[i].as<String>();
                    if (hexValue.startsWith("0x")) {
                        cmd.data.consumerPress.report[i] = strtol(hexValue.c_str(), NULL, 16);
                    } else {
                        // Try to parse as decimal
                        cmd.data.consumerPress.report[i] = hexValue.toInt();
                    }
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
                    cmd.data.mouseClick.button = MB_LEFT;
                } else if (buttonStr == "right") {
                    cmd.data.mouseClick.button = MB_RIGHT;
                } else if (buttonStr == "middle") {
                    cmd.data.mouseClick.button = MB_MIDDLE;
                } else if (buttonStr == "back") {
                    cmd.data.mouseClick.button = MB_BACK;
                } else if (buttonStr == "forward") {
                    cmd.data.mouseClick.button = MB_FORWARD;
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

// Helper function to clean up dynamically allocated memory in commands
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
        USBSerial.printf("Macro not found: %s\n", macroId.c_str());
        return false;
    }
    
    // If we're already executing a macro, we can't nest them in this simplified version
    if (executing) {
        USBSerial.println("Already executing a macro, can't start another");
        return false;
    }
    
    // Start executing the new macro
    currentMacro = it->second;
    currentCommandIndex = 0;
    executing = true;
    lastExecTime = millis();
    delayUntil = 0;
    
    USBSerial.printf("Starting execution of macro: %s\n", macroId.c_str());
    USBSerial.printf("Macro contains %d commands\n", currentMacro.commands.size());
    return true;
}

void MacroHandler::executeCommand(const MacroCommand& cmd) {
    USBSerial.printf("Executing command type: %d\n", cmd.type);
    
    switch (cmd.type) {
        case MACRO_CMD_KEY_PRESS: {
            if (hidHandler) {
                USBSerial.println("Executing key press command");
                USBSerial.print("Report: ");
                for (int i = 0; i < 8; i++) {
                    USBSerial.printf("0x%02X ", cmd.data.keyPress.report[i]);
                }
                USBSerial.println();
                
                hidHandler->sendKeyboardReport(cmd.data.keyPress.report);
                delay(50); // Small delay to ensure keypress is registered
                hidHandler->sendEmptyKeyboardReport();
            } else {
                USBSerial.println("Error: HID handler not available");
            }
            break;
        }
            
        case MACRO_CMD_KEY_DOWN: {
            if (hidHandler) {
                USBSerial.println("Executing key down command");
                USBSerial.print("Report: ");
                for (int i = 0; i < 8; i++) {
                    USBSerial.printf("0x%02X ", cmd.data.keyPress.report[i]);
                }
                USBSerial.println();
                
                hidHandler->sendKeyboardReport(cmd.data.keyPress.report);
            } else {
                USBSerial.println("Error: HID handler not available");
            }
            break;
        }
            
        case MACRO_CMD_KEY_UP: {
            if (hidHandler) {
                USBSerial.println("Executing key up command");
                hidHandler->sendEmptyKeyboardReport();
            } else {
                USBSerial.println("Error: HID handler not available");
            }
            break;
        }
            
        case MACRO_CMD_CONSUMER_PRESS: {
            if (hidHandler) {
                USBSerial.println("Executing consumer control command");
                USBSerial.print("Report: ");
                for (int i = 0; i < 4; i++) {
                    USBSerial.printf("0x%02X ", cmd.data.consumerPress.report[i]);
                }
                USBSerial.println();
                
                hidHandler->sendConsumerReport(cmd.data.consumerPress.report);
                delay(50); // Small delay to ensure press is registered
                hidHandler->sendEmptyConsumerReport();
            } else {
                USBSerial.println("Error: HID handler not available");
            }
            break;
        }
            
        case MACRO_CMD_DELAY: {
            USBSerial.printf("Executing delay: %d ms\n", cmd.data.delay.milliseconds);
            delayUntil = millis() + cmd.data.delay.milliseconds;
            break;
        }
            
        case MACRO_CMD_TYPE_TEXT: {
            if (cmd.data.typeText.text) {
                USBSerial.printf("Typing text: %s\n", cmd.data.typeText.text);
                USBSerial.printf("Text length: %d\n", cmd.data.typeText.length);
                
                // Type each character
                const char* text = cmd.data.typeText.text;
                for (size_t i = 0; i < cmd.data.typeText.length; i++) {
                    char c = text[i];
                    USBSerial.printf("Processing character: '%c' (ASCII: %d)\n", c, (int)c);
                    
                    // Convert character to keypress and send
                    uint8_t report[8] = {0};
                    
                    if (c >= 'a' && c <= 'z') {
                        report[2] = 4 + (c - 'a'); // USB HID code for a-z is 4-29
                        USBSerial.printf("Lowercase letter, HID code: %d\n", report[2]);
                    } else if (c >= 'A' && c <= 'Z') {
                        report[0] = 0x02; // Left shift modifier
                        report[2] = 4 + (c - 'A'); // USB HID code for a-z is 4-29
                        USBSerial.printf("Uppercase letter, HID code: %d with shift\n", report[2]);
                    } else if (c >= '1' && c <= '9') {
                        report[2] = 30 + (c - '1'); // USB HID code for 1-9 is 30-38
                        USBSerial.printf("Number 1-9, HID code: %d\n", report[2]);
                    } else if (c == '0') {
                        report[2] = 39; // USB HID code for 0 is 39
                        USBSerial.printf("Zero, HID code: %d\n", report[2]);
                    } else if (c == ' ') {
                        report[2] = 44; // USB HID code for space is 44
                        USBSerial.printf("Space, HID code: %d\n", report[2]);
                    } else if (c == ',') {
                        report[0] = 0x02; // Left shift modifier
                        report[2] = 54; // USB HID code for comma is 54
                        USBSerial.printf("Comma, HID code: %d with shift\n", report[2]);
                    } else if (c == '.') {
                        report[2] = 55; // USB HID code for period is 55
                        USBSerial.printf("Period, HID code: %d\n", report[2]);
                    } else if (c == '!') {
                        report[0] = 0x02; // Left shift modifier
                        report[2] = 30; // USB HID code for 1 is 30
                        USBSerial.printf("Exclamation mark, HID code: %d with shift\n", report[2]);
                    } else {
                        USBSerial.printf("Unsupported character: '%c'\n", c);
                    }
                    
                    if (report[2] != 0 && hidHandler) {
                        USBSerial.print("Sending report: ");
                        for (int j = 0; j < 8; j++) {
                            USBSerial.printf("0x%02X ", report[j]);
                        }
                        USBSerial.println();
                        
                        hidHandler->sendKeyboardReport(report);
                        delay(10); // Small delay between keypresses
                        hidHandler->sendEmptyKeyboardReport();
                        delay(5);
                    } else if (report[2] == 0) {
                        USBSerial.println("No valid HID code for this character");
                    } else {
                        USBSerial.println("Error: HID handler not available");
                    }
                }
            } else {
                USBSerial.println("Error: Text is null");
            }
            break;
        }
            
        case MACRO_CMD_EXECUTE_MACRO: {
            if (cmd.data.executeMacro.macroId) {
                USBSerial.printf("Ignoring nested macro execution in simplified version: %s\n", 
                          cmd.data.executeMacro.macroId);
                // We don't support nested macros in this simplified version
            }
            break;
        }
            
        case MACRO_CMD_MOUSE_MOVE: {
            USBSerial.printf("Moving mouse: x=%d, y=%d, speed=%d\n", 
                          cmd.data.mouseMove.x, 
                          cmd.data.mouseMove.y,
                          cmd.data.mouseMove.speed);
                          
            // Apply the movement with the specified speed
            int speed = cmd.data.mouseMove.speed;
            if (speed <= 5) {
                // For slower speeds, move multiple times with small increments
                int steps = 10 - speed; // 5->5 steps, 1->9 steps
                int x_step = cmd.data.mouseMove.x / steps;
                int y_step = cmd.data.mouseMove.y / steps;
                
                for (int i = 0; i < steps; i++) {
                    Mouse.move(x_step, y_step);
                    delay(10);
                }
                
                // Handle any remainder
                int x_remainder = cmd.data.mouseMove.x % steps;
                int y_remainder = cmd.data.mouseMove.y % steps;
                if (x_remainder || y_remainder) {
                    Mouse.move(x_remainder, y_remainder);
                }
            } else {
                // For faster speeds, multiply the movement
                int multiplier = speed - 4; // 6->2x, 10->6x
                Mouse.move(cmd.data.mouseMove.x * multiplier, 
                          cmd.data.mouseMove.y * multiplier);
            }
            break;
        }
            
        case MACRO_CMD_MOUSE_CLICK: {
            USBSerial.printf("Mouse click: button=%d, clicks=%d\n", 
                          cmd.data.mouseClick.button, 
                          cmd.data.mouseClick.clicks);
                          
            // Execute single or multiple clicks
            uint8_t button = cmd.data.mouseClick.button;
            for (uint8_t i = 0; i < cmd.data.mouseClick.clicks; i++) {
                Mouse.click(button);
                
                // Add a small delay between multiple clicks
                if (i < cmd.data.mouseClick.clicks - 1) {
                    delay(50);
                }
            }
            break;
        }
            
        case MACRO_CMD_MOUSE_SCROLL: {
            USBSerial.printf("Mouse scroll: amount=%d\n", cmd.data.mouseScroll.amount);
            
            // Scroll the specified amount - USBHIDMouse uses move(x, y, wheel)
            // where wheel is the scroll amount
            Mouse.move(0, 0, cmd.data.mouseScroll.amount);
            break;
        }
            
        case MACRO_CMD_REPEAT_START: {
            USBSerial.printf("Starting repeat block: count=%d\n", cmd.data.repeatStart.count);
            // Handle repeat start
            inRepeat = true;
            repeatCount = cmd.data.repeatStart.count;
            currentRepeatCount = 0;
            repeatStartIndex = currentCommandIndex;
            break;
        }
            
        case MACRO_CMD_REPEAT_END: {
            USBSerial.println("End of repeat block");
            if (inRepeat && currentRepeatCount < repeatCount - 1) {
                // Jump back to the repeat start command
                currentRepeatCount++;
                USBSerial.printf("Repeating block: iteration %d/%d\n", 
                              currentRepeatCount + 1, repeatCount);
                currentCommandIndex = repeatStartIndex;
            } else {
                // Reset repeat state
                inRepeat = false;
                repeatCount = 0;
                currentRepeatCount = 0;
            }
            break;
        }
            
        case MACRO_CMD_RANDOM_DELAY: {
            // Generate a random delay between min and max
            uint32_t minTime = cmd.data.randomDelay.minTime;
            uint32_t maxTime = cmd.data.randomDelay.maxTime;
            uint32_t randomDelay = random(minTime, maxTime + 1);
            
            USBSerial.printf("Random delay: %d ms (range: %d-%d ms)\n", 
                          randomDelay, minTime, maxTime);
                          
            delayUntil = millis() + randomDelay;
            break;
        }
            
        default: {
            USBSerial.printf("Unknown command type: %d\n", cmd.type);
            break;
        }
    }
}

bool MacroHandler::deleteMacro(const String& macroId) {
    // Check if macro exists
    auto it = macros.find(macroId);
    if (it == macros.end()) {
        USBSerial.printf("Macro not found: %s\n", macroId.c_str());
        return false;
    }
    
    // Free any dynamically allocated memory in the macro
    for (auto& cmd : it->second.commands) {
        // Use the helper function to clean up allocated memory
        cleanupMacroCommand(cmd);
    }
    
    // Remove from the map
    macros.erase(it);
    
    // Delete the file
    String macroPath = getMacroFilePath(macroId);
    if (LittleFS.exists(macroPath)) {
        if (!LittleFS.remove(macroPath)) {
            USBSerial.printf("Failed to delete macro file: %s\n", macroPath.c_str());
            return false;
        }
    }
    
    return true;
}

bool MacroHandler::getMacro(const String& macroId, Macro& macro) {
    // Check if macro exists
    auto it = macros.find(macroId);
    if (it == macros.end()) {
        USBSerial.printf("Macro not found: %s\n", macroId.c_str());
        return false;
    }
    
    // Copy the macro
    macro = it->second;
    return true;
}

void MacroHandler::update() {
    if (!executing) {
        return;
    }
    
    uint32_t currentTime = millis();
    
    // If we're in a delay, wait for it to complete
    if (delayUntil > 0) {
        if (currentTime < delayUntil) {
            return; // Still waiting
        }
        // Delay completed
        USBSerial.printf("Delay completed at %lu ms\n", currentTime);
        delayUntil = 0;
    }
    
    // Check if we've reached the end of the macro
    if (currentCommandIndex >= currentMacro.commands.size()) {
        // Macro complete
        executing = false;
        USBSerial.println("Macro execution complete");
        return;
    }
    
    // Execute the current command
    USBSerial.printf("Executing command %d of %d\n", 
                     currentCommandIndex + 1, 
                     currentMacro.commands.size());
    
    const MacroCommand& cmd = currentMacro.commands[currentCommandIndex];
    executeCommand(cmd);
    
    // If not in a delay, move to the next command
    if (delayUntil == 0) {
        currentCommandIndex++;
        USBSerial.printf("Moving to next command: %d\n", currentCommandIndex);
    }
    
    lastExecTime = currentTime;
}

std::vector<String> MacroHandler::getAvailableMacros() {
    std::vector<String> result;
    
    // Ensure the directory exists
    ensureMacroDirectoryExists();
    
    // Scan the directory directly
    File root = LittleFS.open("/macros");
    if (!root || !root.isDirectory()) {
        USBSerial.println("Failed to open macros directory in getAvailableMacros");
        return result;
    }
    
    File file = root.openNextFile();
    while (file) {
        String path = file.name();
        // Skip index.json if it exists and non-json files
        if (path.endsWith(".json") && !path.endsWith("index.json")) {
            // Extract the macro ID from the filename (remove .json extension)
            String fileName = path.substring(path.lastIndexOf('/') + 1);
            String macroId = fileName.substring(0, fileName.lastIndexOf('.'));
            result.push_back(macroId);
        }
        file = root.openNextFile();
    }
    
    return result;
}

// Global function implementations
void initializeMacroHandler() {
    if (!macroHandler) {
        macroHandler = new MacroHandler();
        if (macroHandler) {
            if (!macroHandler->begin()) {
                USBSerial.println("Failed to initialize macro handler");
                delete macroHandler;
                macroHandler = nullptr;
            } else {
                USBSerial.println("Macro handler initialized");
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