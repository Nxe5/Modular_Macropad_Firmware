#include "ConfigManager.h"
#include "ModuleSetup.h" // For the readJsonFile function
#include "FileSystemUtils.h"
#include <Arduino.h> // For ESP.getFreeHeap()
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <USBCDC.h>
#include "JsonUtils.h" // Include centralized JSON utilities

// Global references
extern USBCDC USBSerial;

// Forward declarations
extern void createWorkingActionsFile();
// Function is now declared in JsonUtils.h
// extern size_t estimateJsonBufferSize(const String& filesize, float multiplier);

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
    
    // Estimate buffer size based on JSON content with a 1.5 multiplier
    size_t bufferSize = estimateJsonBufferSize(jsonStr);
    
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

std::map<String, ActionConfig> ConfigManager::loadActions(const char* filePath) {
    std::map<String, ActionConfig> actions;
    
    USBSerial.printf("Loading actions from: %s\n", filePath);
    
    // Try to read the file
    File file = LittleFS.open(filePath, "r");
    if (!file) {
        USBSerial.printf("Failed to open actions file: %s\n", filePath);
        
        // Try alternative location if primary fails
        const char* altPath = (strncmp(filePath, "/config/", 8) == 0) ? 
            "/data/config/actions.json" : "/config/actions.json";
            
        USBSerial.printf("Trying alternative path: %s\n", altPath);
        file = LittleFS.open(altPath, "r");
        
        if (!file) {
            USBSerial.printf("Failed to open alternative actions file: %s\n", altPath);
            
            // Last resort - create a default file
            USBSerial.println("Creating default actions file as last resort");
            createWorkingActionsFile();
            
            // Try one more time with the primary path
            file = LittleFS.open(filePath, "r");
            if (!file) {
                USBSerial.println("Still cannot open actions file after creating default");
                return actions;
            }
        }
    }
    
    if (file.size() > 0) {
        // First try with ArduinoJson library's advanced features
        size_t fileSize = file.size();
        USBSerial.printf("File size: %d bytes\n", fileSize);
        
        // Calculate buffer size with safety margin - use a much larger multiplier for safety
        size_t bufferSize = 16384; // Start with a 16KB buffer
        if (fileSize > 3000) {
            bufferSize = 32768; // Use 32KB for larger files
        }
        
        USBSerial.printf("Allocating JSON buffer of %d bytes (free heap: %d)\n", 
                     bufferSize, ESP.getFreeHeap());
        
        DynamicJsonDocument fullDoc(bufferSize);
        
        DeserializationError error = deserializeJson(fullDoc, file);
        
        if (error) {
            USBSerial.printf("Failed to parse actions.json with error: %s\n", error.c_str());
            
            // Try a simpler memory-efficient approach
            file.seek(0); // Reset file position
            String jsonString = file.readString();
            file.close();
            
            USBSerial.println("Calling createWorkingActionsFile to recover from parse error");
            createWorkingActionsFile();
            
            // Try to load the newly created file
            return loadActions(filePath);
        } else {
            USBSerial.println("Successfully parsed actions.json");
            
            // Extract default layer configuration
            if (fullDoc.containsKey("actions")) {
                JsonObject actionsObj = fullDoc["actions"];
                
                String defaultLayerName = "default-actions-layer";
                
                // Check if a layer name is explicitly provided
                if (actionsObj.containsKey("layer-name")) {
                    defaultLayerName = actionsObj["layer-name"].as<String>();
                    USBSerial.printf("Default layer name: %s\n", defaultLayerName.c_str());
                    
                    // Add the layer name as a special action entry so KeyHandler knows it
                    ActionConfig layerNameConfig;
                    layerNameConfig.type = "default-layer-name";
                    layerNameConfig.targetLayer = defaultLayerName;
                    actions["__default_layer_name__"] = layerNameConfig;
                }
                
                // Extract the layer-config for the default layer
                if (actionsObj.containsKey("layer-config")) {
                    JsonObject layerConfig = actionsObj["layer-config"];
                    USBSerial.printf("Found %d components in default layer\n", layerConfig.size());
                    
                    // Process each component in this layer
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
                            
                            if (config.containsKey("macroId")) {
                                actionConfig.macroId = config["macroId"].as<String>();
                                USBSerial.printf("  Macro ID: %s\n", actionConfig.macroId.c_str());
                            }
                            
                            if (config.containsKey("targetLayer")) {
                                actionConfig.targetLayer = config["targetLayer"].as<String>();
                                USBSerial.printf("  Target Layer: %s\n", actionConfig.targetLayer.c_str());
                            }
                            
                            // Add configuration directly to map for the default layer
                            actions[componentId] = actionConfig;
                        }
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
                                    
                                    if (config.containsKey("macroId")) {
                                        actionConfig.macroId = config["macroId"].as<String>();
                                        USBSerial.printf("    Macro ID: %s\n", actionConfig.macroId.c_str());
                                    }
                                    
                                    if (config.containsKey("targetLayer")) {
                                        actionConfig.targetLayer = config["targetLayer"].as<String>();
                                        USBSerial.printf("    Target Layer: %s\n", actionConfig.targetLayer.c_str());
                                    }
                                    
                                    // Add configuration to map with layer prefix
                                    String layerComponentId = layerName + ":" + componentId;
                                    actions[layerComponentId] = actionConfig;
                                }
                            }
                        }
                    }
                }
            }
            
            USBSerial.printf("Successfully extracted %d action configurations\n", actions.size());
            return actions;
        }
    } else {
        USBSerial.println("Actions file is empty");
        file.close();
        
        // Create default actions
        createWorkingActionsFile();
        return loadActions(filePath);
    }

    file.close();
    return actions;
}

bool ConfigManager::parseComponent(JsonObjectConst obj, Component& component) {
    if (!obj.containsKey("id") || !obj.containsKey("type")) {
        return false;
    }
    
    component.id = obj["id"].as<String>();
    component.type = obj["type"].as<String>();
    
    if (obj.containsKey("size") && obj["size"].is<JsonObjectConst>()) {
        JsonObjectConst size = obj["size"];
        component.rows = size["rows"] | 1;
        component.cols = size["columns"] | 1;
    } else {
        component.rows = 1;
        component.cols = 1;
    }
    
    if (obj.containsKey("start_location") && obj["start_location"].is<JsonObjectConst>()) {
        JsonObjectConst location = obj["start_location"];
        component.startRow = location["row"] | 0;
        component.startCol = location["column"] | 0;
    } else {
        component.startRow = 0;
        component.startCol = 0;
    }
    
    if (component.type == "encoder" && obj.containsKey("with_button")) {
        component.withButton = obj["with_button"] | false;
    } else {
        component.withButton = false;
    }
    
    return true;
}

std::map<String, DisplayMode> ConfigManager::loadDisplayModes(const char* filePath) {
    std::map<String, DisplayMode> displayModes;
    String jsonStr = readFile(filePath);
    
    if (jsonStr.isEmpty()) {
        return displayModes;
    }
    
    // Parse the JSON
    DynamicJsonDocument doc(8192);  // Allocate a large buffer
    DeserializationError error = deserializeJson(doc, jsonStr);
    
    if (error) {
        USBSerial.printf("Error parsing display modes JSON: %s\n", error.c_str());
        return displayModes;
    }
    
    // Check if "modes" key exists
    if (!doc.containsKey("modes")) {
        USBSerial.println("No 'modes' section found in display config");
        return displayModes;
    }
    
    // Process each mode
    JsonObject modes = doc["modes"];
    for (JsonPair pair : modes) {
        String modeName = pair.key().c_str();
        JsonObject modeConfig = pair.value();
        
        DisplayMode mode;
        mode.name = modeName;
        mode.active = modeConfig["active"] | false;
        mode.template_file = modeConfig["template_file"] | "";
        mode.description = modeConfig["description"] | "";
        mode.refresh_rate = modeConfig["refresh_rate"] | 1000;  // Default to 1 second
        mode.backgroundImage = modeConfig["backgroundImage"] | "";  // New field for background image
        
        // Store without loading elements - they'll be loaded on demand
        displayModes[modeName] = mode;
    }
    
    return displayModes;
}

std::vector<DisplayElement> ConfigManager::loadDisplayElements(const char* filePath, const String& modeName) {
    std::vector<DisplayElement> elements;
    String jsonStr = readFile(filePath);
    
    if (jsonStr.isEmpty()) {
        return elements;
    }
    
    // Parse the JSON
    DynamicJsonDocument doc(16384);  // Larger buffer for elements
    DeserializationError error = deserializeJson(doc, jsonStr);
    
    if (error) {
        USBSerial.printf("Error parsing display elements JSON: %s\n", error.c_str());
        return elements;
    }
    
    // Check if mode exists
    if (!doc.containsKey("modes") || !doc["modes"].containsKey(modeName) || 
        !doc["modes"][modeName].containsKey("elements")) {
        USBSerial.printf("No elements found for mode: %s\n", modeName.c_str());
        return elements;
    }
    
    // Process each element
    JsonArray elementsArray = doc["modes"][modeName]["elements"];
    for (JsonObject elementObj : elementsArray) {
        DisplayElement element;
        
        element.type = elementObj["type"] | 0;
        element.x = elementObj["x"] | 0;
        element.y = elementObj["y"] | 0;
        element.width = elementObj["width"] | 0;
        element.height = elementObj["height"] | 0;
        element.text = elementObj["text"] | "";
        element.variable = elementObj["variable"] | "";
        element.alignment = elementObj["alignment"] | "left";
        element.color = elementObj["color"] | 0xFFFF;  // Default white
        element.size = elementObj["size"] | 1;
        element.end_x = elementObj["end_x"] | 0;
        element.end_y = elementObj["end_y"] | 0;
        element.filled = elementObj["filled"] | false;
        
        elements.push_back(element);
    }
    
    return elements;
}

ModuleInfo ConfigManager::loadModuleInfo(const char* filePath) {
    ModuleInfo info;
    String jsonStr = readFile(filePath);
    
    if (jsonStr.isEmpty()) {
        return info;
    }
    
    // Parse the JSON
    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, jsonStr);
    
    if (error) {
        USBSerial.printf("Error parsing module info JSON: %s\n", error.c_str());
        return info;
    }
    
    // Fill in the struct
    info.name = doc["name"] | "Modular Macropad";
    info.version = doc["version"] | "1.0.0";
    info.author = doc["author"] | "User";
    info.description = doc["description"] | "Default configuration";
    info.moduleSize = doc["module-size"] | "full";
    
    // Grid size is nested
    if (doc.containsKey("gridSize")) {
        info.gridRows = doc["gridSize"]["rows"] | 3;
        info.gridCols = doc["gridSize"]["columns"] | 4;
    } else {
        info.gridRows = 3;
        info.gridCols = 4;
    }
    
    return info;
}