#include "ConfigManager.h"

String ConfigManager::readFile(const char* filePath) {
    if (!LittleFS.exists(filePath)) {
        Serial.printf("File not found: %s\n", filePath);
        return "";
    }
    File file = LittleFS.open(filePath, "r");
    if (!file) {
        Serial.printf("Failed to open file: %s\n", filePath);
        return "";
    }
    String content = file.readString();
    file.close();
    return content;
}

std::vector<Component> ConfigManager::loadComponents(const char* filePath) {
    std::vector<Component> components;
    String jsonStr = readFile(filePath);
    if (jsonStr.isEmpty()) return components;
    
    DynamicJsonDocument doc(8192);
    DeserializationError error = deserializeJson(doc, jsonStr);
    if (error) {
        Serial.printf("Error parsing %s: %s\n", filePath, error.c_str());
        return components;
    }
    
    JsonArray comps = doc["components"].as<JsonArray>();
    for (JsonObject comp : comps) {
        Component c;
        c.id = comp["id"].as<String>();
        c.type = comp["type"].as<String>();
        c.rows = comp["size"]["rows"];
        c.cols = comp["size"]["columns"];
        c.startRow = comp["start_location"]["row"];
        c.startCol = comp["start_location"]["column"];
        if (c.type == "encoder" && comp.containsKey("with_button"))
            c.withButton = comp["with_button"];
        components.push_back(c);
    }
    return components;
}

std::map<String, ActionConfig> ConfigManager::loadActions(const String &filePath) {
    std::map<String, ActionConfig> actions;
    
    // Use the readJsonFile function from ModuleSetup.cpp
    String jsonContent = readJsonFile(filePath.c_str());
    if (jsonContent.isEmpty()) {
        Serial.printf("Could not read file: %s\n", filePath.c_str());
        return actions;
    }
    
    
    // Parse JSON
    DynamicJsonDocument doc(16384); // Increased size to handle larger files
    DeserializationError error = deserializeJson(doc, jsonContent);
    
    if (error) {
        Serial.printf("Error parsing JSON: %s\n", error.c_str());
        return actions;
    }
    
    // Extract layer config
    if (!doc.containsKey("actions") || 
        !doc["actions"].containsKey("layer-config")) {
        Serial.println("Invalid actions.json format: missing actions or layer-config");
        return actions;
    }
    
    JsonObject layerConfig = doc["actions"]["layer-config"];
    
    // Process each button configuration
    for (JsonPair kv : layerConfig) {
        String buttonId = kv.key().c_str();
        JsonObject buttonConfig = kv.value();
        
        ActionConfig action;
        action.type = buttonConfig["type"].as<String>();
        
        // Debug
        Serial.printf("Loading action for %s, type: %s\n", buttonId.c_str(), action.type.c_str());
        
        if (action.type == "hid") {
            // Check if buttonPress exists
            if (buttonConfig.containsKey("buttonPress")) {
                JsonVariant buttonPress = buttonConfig["buttonPress"];
                
                // Handle both array formats
                if (buttonPress.is<JsonArray>()) {
                    JsonArray pressArray = buttonPress.as<JsonArray>();
                    
                    // Check if we have a nested array format [["0x00", "0x01", ...]]
                    if (pressArray.size() > 0 && pressArray[0].is<JsonArray>()) {
                        // Handle nested array format
                        JsonArray innerArray = pressArray[0].as<JsonArray>();
                        
                        action.hidReport.clear();
                        for (size_t i = 0; i < innerArray.size() && i < 8; i++) {
                            action.hidReport.push_back(innerArray[i].as<String>());
                        }
                    } 
                    else {
                        // Handle flat array format ["0x00", "0x01", ...]
                        action.hidReport.clear();
                        for (size_t i = 0; i < pressArray.size() && i < 8; i++) {
                            action.hidReport.push_back(pressArray[i].as<String>());
                        }
                    }
                    
                    Serial.printf("Loaded HID report with %d bytes for %s\n", 
                                 action.hidReport.size(), buttonId.c_str());
                }
            }
            
            // Check for encoder rotation actions (clockwise/counterclockwise)
            if (buttonId.startsWith("encoder-")) {
                // Clockwise action
                if (buttonConfig.containsKey("clockwise")) {
                    JsonVariant cwAction = buttonConfig["clockwise"];
                    if (cwAction.is<JsonArray>()) {
                        JsonArray cwArray = cwAction.as<JsonArray>();
                        action.clockwise.clear();
                        for (size_t i = 0; i < cwArray.size() && i < 8; i++) {
                            action.clockwise.push_back(cwArray[i].as<String>());
                        }
                        Serial.printf("Loaded clockwise HID report with %d bytes for %s\n", 
                                     action.clockwise.size(), buttonId.c_str());
                    }
                }
                
                // Counterclockwise action
                if (buttonConfig.containsKey("counterclockwise")) {
                    JsonVariant ccwAction = buttonConfig["counterclockwise"];
                    if (ccwAction.is<JsonArray>()) {
                        JsonArray ccwArray = ccwAction.as<JsonArray>();
                        action.counterclockwise.clear();
                        for (size_t i = 0; i < ccwArray.size() && i < 8; i++) {
                            action.counterclockwise.push_back(ccwArray[i].as<String>());
                        }
                        Serial.printf("Loaded counterclockwise HID report with %d bytes for %s\n", 
                                     action.counterclockwise.size(), buttonId.c_str());
                    }
                }
            }
        }
        else if (action.type == "multimedia") {
            // Similar processing for multimedia reports
            if (buttonConfig.containsKey("consumerReport")) {
                JsonVariant consumerReport = buttonConfig["consumerReport"];
                
                if (consumerReport.is<JsonArray>()) {
                    JsonArray reportArray = consumerReport.as<JsonArray>();
                    
                    // Check if we have nested array
                    if (reportArray.size() > 0 && reportArray[0].is<JsonArray>()) {
                        JsonArray innerArray = reportArray[0].as<JsonArray>();
                        
                        action.consumerReport.clear();
                        for (size_t i = 0; i < innerArray.size() && i < 4; i++) {
                            action.consumerReport.push_back(innerArray[i].as<String>());
                        }
                    } 
                    else {
                        // Handle flat array format
                        action.consumerReport.clear();
                        for (size_t i = 0; i < reportArray.size() && i < 4; i++) {
                            action.consumerReport.push_back(reportArray[i].as<String>());
                        }
                    }
                    
                    Serial.printf("Loaded Consumer report with %d bytes for %s\n", 
                                 action.consumerReport.size(), buttonId.c_str());
                }
            }
            
            // Check for encoder rotation actions for multimedia type
            if (buttonId.startsWith("encoder-")) {
                // Clockwise action
                if (buttonConfig.containsKey("clockwise")) {
                    JsonVariant cwAction = buttonConfig["clockwise"];
                    if (cwAction.is<JsonArray>()) {
                        JsonArray cwArray = cwAction.as<JsonArray>();
                        action.clockwise.clear();
                        for (size_t i = 0; i < cwArray.size() && i < 4; i++) {
                            action.clockwise.push_back(cwArray[i].as<String>());
                        }
                        Serial.printf("Loaded clockwise consumer report with %d bytes for %s\n", 
                                     action.clockwise.size(), buttonId.c_str());
                    }
                }
                
                // Counterclockwise action
                if (buttonConfig.containsKey("counterclockwise")) {
                    JsonVariant ccwAction = buttonConfig["counterclockwise"];
                    if (ccwAction.is<JsonArray>()) {
                        JsonArray ccwArray = ccwAction.as<JsonArray>();
                        action.counterclockwise.clear();
                        for (size_t i = 0; i < ccwArray.size() && i < 4; i++) {
                            action.counterclockwise.push_back(ccwArray[i].as<String>());
                        }
                        Serial.printf("Loaded counterclockwise consumer report with %d bytes for %s\n", 
                                     action.counterclockwise.size(), buttonId.c_str());
                    }
                }
            }
        }
        else if (action.type == "macro") {
            action.macroId = buttonConfig["macroId"].as<String>();
            Serial.printf("Loaded macro ID: %s for %s\n", 
                         action.macroId.c_str(), buttonId.c_str());
        }
        else if (action.type == "layer") {
            action.targetLayer = buttonConfig["targetLayer"].as<String>();
            Serial.printf("Loaded target layer: %s for %s\n", 
                         action.targetLayer.c_str(), buttonId.c_str());
        }
        
        // Store the action configuration
        actions[buttonId] = action;
    }
    
    Serial.printf("Loaded %d button actions successfully\n", actions.size());
    return actions;
}