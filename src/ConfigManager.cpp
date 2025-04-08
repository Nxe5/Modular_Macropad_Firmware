#include "ConfigManager.h"
#include "ModuleSetup.h" // For the readJsonFile function
#include "FileSystemUtils.h"
#include <Arduino.h> // For ESP.getFreeHeap()

extern size_t estimateJsonBufferSize(const String& jsonString, float safetyFactor);

String ConfigManager::readFile(const char* filePath) {
    return FileSystemUtils::readFile(filePath);
}

std::vector<Component> ConfigManager::loadComponents(const char* filePath) {
    std::vector<Component> components;
    String jsonStr = readFile(filePath);
    if (jsonStr.isEmpty()) return components;
    
    // Print debug info
    Serial.printf("Components JSON size: %d bytes\n", jsonStr.length());
    Serial.printf("Free heap before parsing: %d bytes\n", ESP.getFreeHeap());
    
    // Estimate buffer size based on JSON content
    size_t bufferSize = estimateJsonBufferSize(jsonStr, 1.5);
    
    DynamicJsonDocument doc(bufferSize);
    DeserializationError error = deserializeJson(doc, jsonStr);
    
    Serial.printf("Free heap after parsing: %d bytes\n", ESP.getFreeHeap());
    
    if (error) {
        Serial.printf("Error parsing %s: %s\n", filePath, error.c_str());
        if (error.code() == DeserializationError::NoMemory) {
            Serial.printf("Memory allocation failed. Tried buffer size: %u bytes\n", bufferSize);
            // Use estimation instead of measureJson
            size_t requiredSize = jsonStr.length() * 2; // Simple estimation
            Serial.printf("Estimated required size: %u bytes\n", requiredSize);
            
            // If we can estimate and allocate the required size, retry
            if (requiredSize > 0 && requiredSize <= ESP.getFreeHeap() / 2) {
                Serial.printf("Retrying with estimated size: %u bytes\n", requiredSize);
                DynamicJsonDocument retryDoc(requiredSize);
                error = deserializeJson(retryDoc, jsonStr);
                if (!error) {
                    // Process with the retry document
                    JsonArray comps = retryDoc["components"].as<JsonArray>();
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
            }
        }
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

std::map<String, ActionConfig> ConfigManager::loadActions(const String& filePath) {
    std::map<String, ActionConfig> actions;
    
    if (!LittleFS.exists(filePath)) {
        USBSerial.printf("Actions file not found: %s\n", filePath.c_str());
        return actions;
    }
    
    File file = LittleFS.open(filePath, "r");
    if (!file) {
        USBSerial.printf("Failed to open actions file: %s\n", filePath.c_str());
        return actions;
    }
    
    // Try to parse with DynamicJsonDocument first
    DynamicJsonDocument fullDoc(8192);
    DeserializationError fullError = deserializeJson(fullDoc, file);
    
    if (fullError) {
        USBSerial.printf("ERROR: Full JSON parsing failed: %s\n", fullError.c_str());
        USBSerial.println("Will try manual parsing instead");
    } else {
        USBSerial.println("Full JSON parsing successful!");
        
        // Check for expected structure
        if (!fullDoc.containsKey("actions")) {
            USBSerial.println("ERROR: Missing 'actions' key in JSON");
        } else if (!fullDoc["actions"].containsKey("layer-config")) {
            USBSerial.println("ERROR: Missing 'layer-config' key in JSON");
        } else {
            USBSerial.println("JSON structure appears valid");
            
            // Count configurations
            JsonObject layerConfig = fullDoc["actions"]["layer-config"];
            int count = 0;
            for (JsonPair kv : layerConfig) {
                count++;
                USBSerial.printf("Found config for: %s\n", kv.key().c_str());
            }
            USBSerial.printf("Found %d button configurations\n", count);
            
            // Try to extract configurations
            for (JsonPair kv : layerConfig) {
                String componentId = kv.key().c_str();
                JsonObject config = kv.value().as<JsonObject>();
                
                ActionConfig actionConfig;
                actionConfig.id = componentId;
                
                if (config.containsKey("type")) {
                    actionConfig.type = config["type"].as<String>();
                    USBSerial.printf("Component %s has type: %s\n", 
                                 componentId.c_str(), actionConfig.type.c_str());
                    
                    if (config.containsKey("buttonPress") && config["buttonPress"].is<JsonArray>()) {
                        JsonArray codes = config["buttonPress"].as<JsonArray>();
                        for (JsonVariant code : codes) {
                            actionConfig.hidReport.push_back(code.as<String>());
                        }
                        USBSerial.printf("  Added %d button press codes\n", actionConfig.hidReport.size());
                    }
                    
                    // Add configuration to map
                    actions[componentId] = actionConfig;
                }
            }
            
            // Check for additional layers
            if (fullDoc.containsKey("layers")) {
                JsonObject layers = fullDoc["layers"];
                USBSerial.printf("Found %d additional layers\n", layers.size());
                
                // Process each additional layer
                for (JsonPair layerKv : layers) {
                    String layerName = layerKv.key().c_str();
                    JsonObject layerConfig = layerKv.value().as<JsonObject>();
                    
                    USBSerial.printf("Processing layer: %s\n", layerName.c_str());
                    
                    if (layerConfig.containsKey("layer-config")) {
                        JsonObject buttonConfigs = layerConfig["layer-config"];
                        
                        // Process each button in this layer
                        for (JsonPair buttonKv : buttonConfigs) {
                            String componentId = buttonKv.key().c_str();
                            JsonObject config = buttonKv.value().as<JsonObject>();
                            
                            ActionConfig actionConfig;
                            actionConfig.id = componentId;
                            
                            if (config.containsKey("type")) {
                                actionConfig.type = config["type"].as<String>();
                                USBSerial.printf("  Component %s has type: %s\n", 
                                             componentId.c_str(), actionConfig.type.c_str());
                                
                                if (config.containsKey("buttonPress") && config["buttonPress"].is<JsonArray>()) {
                                    JsonArray codes = config["buttonPress"].as<JsonArray>();
                                    for (JsonVariant code : codes) {
                                        actionConfig.hidReport.push_back(code.as<String>());
                                    }
                                    USBSerial.printf("    Added %d button press codes\n", actionConfig.hidReport.size());
                                }
                                
                                // Add configuration to map with layer prefix
                                String layerComponentId = layerName + ":" + componentId;
                                actions[layerComponentId] = actionConfig;
                            }
                        }
                    }
                }
            }
            
            USBSerial.printf("Successfully extracted %d action configurations\n", actions.size());
            return actions;
        }
    }
    
    // If we're here, we need to try the original manual parsing approach
    file.seek(0); // Reset file position
    
    // Use a smaller buffer for the main document structure
    const size_t docSize = 1024;
    StaticJsonDocument<docSize> doc;
    
    // Use a separate, small buffer for each button configuration
    const size_t buttonConfigSize = 512;
    StaticJsonDocument<buttonConfigSize> buttonConfig;
    
    // First read just the structure to find layer-config
    {
        // Buffer for reading small chunks
        char buffer[128];
        size_t bytesRead = 0;
        String jsonHeader;
        
        // Read enough to get the structure (up to 1024 bytes should be enough)
        while (file.available() && bytesRead < 1024) {
            size_t len = file.readBytes(buffer, sizeof(buffer) - 1);
            buffer[len] = '\0';
            jsonHeader += buffer;
            bytesRead += len;
        }
        
        // Reset file position
        file.seek(0);
        
        USBSerial.printf("Read %d bytes of header\n", jsonHeader.length());
        
        // Try to parse just to get the structure
        DeserializationError error = deserializeJson(doc, jsonHeader);
        if (error) {
            USBSerial.printf("Error parsing JSON structure: %s\n", error.c_str());
            return actions;
        }
        
        // Check if the structure is valid
        if (!doc.containsKey("actions") || !doc["actions"].containsKey("layer-config")) {
            USBSerial.println("Invalid actions.json format: missing actions or layer-config");
            return actions;
        } else {
            USBSerial.println("Header structure valid, proceeding with manual parsing");
        }
    }
    
    // Now manually parse the file to extract each button configuration
    // This is a simple state machine to find and extract button configurations
    
    // Buffer for reading line by line
    char buffer[256];
    int bufferPos = 0;
    bool inLayerConfig = false;
    bool inButtonConfig = false;
    String currentButtonId;
    String line;
    
    // Read the file line by line
    while (file.available()) {
        // Read a line
        int bytesRead = file.readBytesUntil('\n', buffer, sizeof(buffer) - 1);
        buffer[bytesRead] = '\0';
        line = String(buffer);
        line.trim();
        
        // Skip empty lines
        if (line.length() == 0) {
            continue;
        }
        
        // Check for layer-config section
        if (line.indexOf("\"layer-config\":") >= 0) {
            inLayerConfig = true;
            continue;
        }
        
        // Check for end of layer-config section
        if (inLayerConfig && line.indexOf("}") == 0) {
            inLayerConfig = false;
            continue;
        }
        
        // Process button configurations
        if (inLayerConfig) {
            // Look for a button ID
            int buttonPos = line.indexOf("\"button-");
            if (buttonPos >= 0) {
                int endQuote = line.indexOf("\"", buttonPos + 1);
                if (endQuote > buttonPos) {
                    currentButtonId = line.substring(buttonPos + 1, endQuote);
                    inButtonConfig = true;
                    USBSerial.printf("Found button config: %s\n", currentButtonId.c_str());
                    
                    // Create the button config entry with default values
                    ActionConfig config;
                    config.id = currentButtonId;
                    actions[currentButtonId] = config;
                }
            }
            
            // Look for an encoder ID
            int encoderPos = line.indexOf("\"encoder-");
            if (encoderPos >= 0) {
                int endQuote = line.indexOf("\"", encoderPos + 1);
                if (endQuote > encoderPos) {
                    currentButtonId = line.substring(encoderPos + 1, endQuote);
                    inButtonConfig = true;
                    USBSerial.printf("Found encoder config: %s\n", currentButtonId.c_str());
                    
                    // Create the encoder config entry with default values
                    ActionConfig config;
                    config.id = currentButtonId;
                    actions[currentButtonId] = config;
                }
            }
        }
        else if (inButtonConfig) {
            // Process button configuration details
            
            // Check for end of button config
            if (line.indexOf("},") == 0 || line.indexOf("}") == 0) {
                inButtonConfig = false;
                currentButtonId = "";
                continue;
            }
            
            // Extract type
            int typePos = line.indexOf("\"type\": \"");
            if (typePos >= 0) {
                int endQuote = line.indexOf("\"", typePos + 9);
                if (endQuote > typePos) {
                    String type = line.substring(typePos + 9, endQuote);
                    actions[currentButtonId].type = type;
                    USBSerial.printf("  Type: %s\n", type.c_str());
                }
            }
            
            // Extract macro ID
            int macroPos = line.indexOf("\"macro\": \"");
            if (macroPos >= 0) {
                int endQuote = line.indexOf("\"", macroPos + 10);
                if (endQuote > macroPos) {
                    String macroId = line.substring(macroPos + 10, endQuote);
                    actions[currentButtonId].macroId = macroId;
                    USBSerial.printf("  Macro: %s\n", macroId.c_str());
                }
            }
            
            // Extract target layer
            int layerPos = line.indexOf("\"targetLayer\": \"");
            if (layerPos >= 0) {
                int endQuote = line.indexOf("\"", layerPos + 15);
                if (endQuote > layerPos) {
                    String targetLayer = line.substring(layerPos + 15, endQuote);
                    actions[currentButtonId].targetLayer = targetLayer;
                    USBSerial.printf("  Target Layer: %s\n", targetLayer.c_str());
                }
            }
            
            // Extract button press (HID report)
            int buttonPressPos = line.indexOf("\"buttonPress\": [");
            if (buttonPressPos >= 0) {
                // Read until the closing bracket
                String buttonPressData = line.substring(buttonPressPos + 15);
                bool foundEnd = buttonPressData.indexOf("]") >= 0;
                
                // Remove trailing comma and spaces
                buttonPressData.trim();
                if (buttonPressData.endsWith(",")) {
                    buttonPressData = buttonPressData.substring(0, buttonPressData.length() - 1);
                }
                
                // Process the HID codes
                int startPos = 0;
                int endPos = buttonPressData.indexOf(",");
                
                while (endPos >= 0) {
                    String code = buttonPressData.substring(startPos, endPos);
                    code.trim();
                    if (code.startsWith("\"") && code.endsWith("\"")) {
                        code = code.substring(1, code.length() - 1);
                    }
                    actions[currentButtonId].hidReport.push_back(code);
                    startPos = endPos + 1;
                    endPos = buttonPressData.indexOf(",", startPos);
                }
                
                // Add the last code
                String lastCode = buttonPressData.substring(startPos);
                lastCode.trim();
                if (lastCode.startsWith("\"") && lastCode.endsWith("\"")) {
                    lastCode = lastCode.substring(1, lastCode.length() - 1);
                }
                if (lastCode.length() > 0) {
                    actions[currentButtonId].hidReport.push_back(lastCode);
                }
                
                USBSerial.printf("  Added %d HID codes\n", actions[currentButtonId].hidReport.size());
            }
        }
    }
    
    file.close();
    return actions;
}