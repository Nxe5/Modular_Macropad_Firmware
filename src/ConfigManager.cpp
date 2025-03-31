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

std::map<String, ActionConfig> ConfigManager::loadActions(const String &filePath) {
    std::map<String, ActionConfig> actions;
    
    // Debug output - exactly what file we're trying to open
    USBSerial.println("==== ACTION LOADING DEBUG ====");
    USBSerial.printf("Attempting to load actions from: %s\n", filePath.c_str());
    
    // Check if file exists
    if (!LittleFS.exists(filePath.c_str())) {
        USBSerial.printf("ERROR: File %s does not exist!\n", filePath.c_str());
        // Try to list the /config directory
        USBSerial.println("Listing /config directory contents:");
        File dir = LittleFS.open("/config");
        if (dir && dir.isDirectory()) {
            File file = dir.openNextFile();
            while (file) {
                USBSerial.printf("  File: %s, Size: %d bytes\n", file.path(), file.size());
                file = dir.openNextFile();
            }
        } else {
            USBSerial.println("  Failed to open /config directory or it doesn't exist");
        }
    }
    
    // Try to open the specified file
    File file = LittleFS.open(filePath.c_str(), "r");
    if (!file) {
        USBSerial.printf("Could not open file: %s\n", filePath.c_str());
    } else {
        USBSerial.printf("Actions file found! Size: %d bytes\n", file.size());
        
        // Read first 100 characters to see content
        if (file.size() > 0) {
            char preview[101] = {0};
            size_t bytesRead = file.readBytes(preview, min(100, (int)file.size()));
            preview[bytesRead] = 0;
            USBSerial.printf("File preview: %s\n", preview);
            file.seek(0); // Reset file position
        }
        
        // Original code continues...
        USBSerial.printf("Free heap before parsing: %d bytes\n", ESP.getFreeHeap());
        
        // Parse the file and populate actions
        std::map<String, ActionConfig> primaryActions = parseActionsFile(file);
        file.close();
        
        // If we found actions, use them
        if (!primaryActions.empty()) {
            USBSerial.printf("Found %d actions in %s\n", primaryActions.size(), filePath.c_str());
            return primaryActions;
        } else {
            USBSerial.printf("No actions found in %s, it may be in wrong format\n", filePath.c_str());
        }
    }
    
    // If we didn't find/load any actions from the primary path, try the alternate path
    String altPath;
    if (filePath.startsWith("/config/")) {
        altPath = "/data" + filePath;
    } else if (filePath.startsWith("/data/config/")) {
        altPath = filePath.substring(5); // Remove "/data" prefix
    } else {
        altPath = "/data/config/" + filePath.substring(filePath.lastIndexOf('/') + 1);
    }
    
    USBSerial.printf("Trying alternate path: %s\n", altPath.c_str());
    
    if (!LittleFS.exists(altPath.c_str())) {
        USBSerial.printf("Alternate file %s does not exist either!\n", altPath.c_str());
        return actions; // Return empty actions
    }
    
    file = LittleFS.open(altPath.c_str(), "r");
    if (!file) {
        USBSerial.printf("Could not open alternate file: %s\n", altPath.c_str());
        return actions; // Return empty actions
    }
    
    USBSerial.printf("Alternate actions file found! Size: %d bytes\n", file.size());
    
    // Read first 100 characters to see content
    if (file.size() > 0) {
        char preview[101] = {0};
        size_t bytesRead = file.readBytes(preview, min(100, (int)file.size()));
        preview[bytesRead] = 0;
        USBSerial.printf("File preview: %s\n", preview);
        file.seek(0); // Reset file position
    }
    
    // Parse the alternate file
    actions = parseActionsFile(file);
    file.close();
    
    USBSerial.printf("Found %d actions in alternate path %s\n", actions.size(), altPath.c_str());
    return actions;
}

// Helper function to parse the actions file
std::map<String, ActionConfig> ConfigManager::parseActionsFile(File &file) {
    std::map<String, ActionConfig> actions;
    
    USBSerial.println("\n==== STARTING ACTIONS FILE PARSING ====");
    size_t fileSize = file.size();
    USBSerial.printf("File size: %d bytes\n", fileSize);
    
    // Verify the file isn't empty
    if (fileSize == 0) {
        USBSerial.println("Error: File is empty");
        return actions;
    }
    
    // Read the entire file content for diagnostics
    String fullContent = file.readString();
    file.seek(0); // Reset file position
    
    USBSerial.printf("Read %d bytes from file\n", fullContent.length());
    USBSerial.println("First 100 characters of content:");
    USBSerial.println(fullContent.substring(0, 100) + "...");
    
    // First, try to parse the entire file as JSON to check structure
    DynamicJsonDocument fullDoc(16384); // Use a large buffer
    DeserializationError fullError = deserializeJson(fullDoc, fullContent);
    
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
        char c = file.read();
        
        if (c == '\n' || c == '\r') {
            if (bufferPos > 0) {
                buffer[bufferPos] = '\0';
                line = buffer;
                bufferPos = 0;
                
                line.trim();
                
                // Process the line based on our state
                if (!inLayerConfig && line.indexOf("\"layer-config\": {") >= 0) {
                    inLayerConfig = true;
                    USBSerial.println("Found layer-config section");
                } 
                else if (inLayerConfig && !inButtonConfig && line.indexOf("}") == 0) {
                    // End of layer-config
                    inLayerConfig = false;
                    USBSerial.println("End of layer-config section");
                }
                else if (inLayerConfig && !inButtonConfig) {
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
                        while (!foundEnd && file.available()) {
                            char c = file.read();
                            if (c == '\n' || c == '\r') {
                                if (bufferPos > 0) {
                                    buffer[bufferPos] = '\0';
                                    line = buffer;
                                    bufferPos = 0;
                                    line.trim();
                                    
                                    // Add this line to our button press data
                                    buttonPressData += " " + line;
                                    
                                    // Check if we found the end
                                    foundEnd = line.indexOf("]") >= 0;
                                }
                            }
                            else {
                                if (bufferPos < sizeof(buffer) - 1) {
                                    buffer[bufferPos++] = c;
                                }
                            }
                        }
                        
                        // Now parse the button press data
                        buttonPressData.replace("]", "");
                        buttonPressData.trim();
                        
                        // Split by commas
                        int startPos = 0;
                        int commaPos;
                        while ((commaPos = buttonPressData.indexOf(",", startPos)) >= 0) {
                            String code = buttonPressData.substring(startPos, commaPos);
                            code.trim();
                            
                            // Remove quotes
                            if (code.startsWith("\"")) {
                                code = code.substring(1);
                            }
                            if (code.endsWith("\"")) {
                                code = code.substring(0, code.length() - 1);
                            }
                            
                            // Add to the appropriate report based on type
                            if (actions[currentButtonId].type == "hid") {
                                actions[currentButtonId].hidReport.push_back(code);
                                USBSerial.printf("  HID Code: %s\n", code.c_str());
                            } else if (actions[currentButtonId].type == "multimedia" || 
                                       actions[currentButtonId].type == "consumer") {
                                actions[currentButtonId].consumerReport.push_back(code);
                                USBSerial.printf("  Consumer Code: %s\n", code.c_str());
                            }
                            
                            startPos = commaPos + 1;
                        }
                        
                        // Process the last code
                        String code = buttonPressData.substring(startPos);
                        code.trim();
                        
                        // Remove quotes
                        if (code.startsWith("\"")) {
                            code = code.substring(1);
                        }
                        if (code.endsWith("\"")) {
                            code = code.substring(0, code.length() - 1);
                        }
                        
                        if (!code.isEmpty()) {
                            if (actions[currentButtonId].type == "hid") {
                                actions[currentButtonId].hidReport.push_back(code);
                                USBSerial.printf("  HID Code: %s\n", code.c_str());
                            } else if (actions[currentButtonId].type == "multimedia" || 
                                       actions[currentButtonId].type == "consumer") {
                                actions[currentButtonId].consumerReport.push_back(code);
                                USBSerial.printf("  Consumer Code: %s\n", code.c_str());
                            }
                        }
                    }
                    
                    // Special processing for encoder buttons
                    if (currentButtonId.startsWith("encoder-") && 
                        (actions[currentButtonId].type == "multimedia" || 
                         actions[currentButtonId].type == "consumer")) {
                        
                        // Create a special button entry for this encoder
                        String buttonId = "button-" + currentButtonId.substring(currentButtonId.indexOf("-")+1);
                        
                        // Only do this if we have consumer report data available
                        if (!actions[currentButtonId].consumerReport.empty()) {
                            ActionConfig btnConfig;
                            btnConfig.id = buttonId;
                            btnConfig.type = actions[currentButtonId].type;
                            btnConfig.consumerReport = actions[currentButtonId].consumerReport;
                            
                            actions[buttonId] = btnConfig;
                            USBSerial.printf("Created button action for encoder: %s -> %s\n", 
                                          currentButtonId.c_str(), buttonId.c_str());
                        }
                    }
                }
            }
        }
        else {
            // Add character to buffer
            if (bufferPos < sizeof(buffer) - 1) {
                buffer[bufferPos++] = c;
            }
        }
    }
    
    return actions;
}