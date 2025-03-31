// ModuleSetup.cpp
#include "ModuleSetup.h"
#include <ArduinoJson.h>
#include <USBCDC.h>
#include "LittleFS.h"
#include <vector>
#include "FileSystemUtils.h"

ModuleCapabilities currentModule;
ModuleInfo moduleInfo;
extern USBCDC USBSerial;


// Function to read a JSON file from LittleFS with improved error handling
String readJsonFile(const char* filePath) {
    USBSerial.printf("Reading JSON file: %s\n", filePath);
    
    if (!FileSystemUtils::fileExists(filePath)) {
        USBSerial.printf("File not found: %s, returning empty JSON\n", filePath);
        return "{}";
    }
    
    String content = FileSystemUtils::readFile(filePath);
    
    if (content.isEmpty()) {
        USBSerial.println("File was empty or read failed, returning empty JSON");
        return "{}";
    }
    
    // Validate JSON
    DynamicJsonDocument validator(16);
    DeserializationError error = deserializeJson(validator, content);
    if (error) {
        USBSerial.printf("Warning: File does not contain valid JSON: %s\n", error.c_str());
        return "{}";
    }
    
    return content;
}

// Function to write a JSON file to LittleFS with improved error handling
bool writeJsonFile(const char* filePath, const String& content) {
    // Validate the JSON before writing
    DynamicJsonDocument validator(16);
    DeserializationError error = deserializeJson(validator, content);
    if (error) {
        USBSerial.printf("Refusing to write invalid JSON to %s: %s\n", filePath, error.c_str());
        return false;
    }
    
    return FileSystemUtils::writeFile(filePath, content);
}

// Count components of a specific type
uint8_t countComponentsByType(const String& componentsJson, const char* componentType) {
    DynamicJsonDocument doc(8192);
    DeserializationError error = deserializeJson(doc, componentsJson);
    
    if (error) {
        Serial.print("Error parsing components JSON: ");
        Serial.println(error.c_str());
        return 0;
    }
    
    uint8_t count = 0;
    JsonArray components = doc["components"].as<JsonArray>();
    
    for (JsonObject component : components) {
        if (strcmp(component["type"], componentType) == 0) {
            count++;
        }
    }
    
    return count;
}

void initializeModuleInfo() {
    // Create default files if they don't exist
    if (!LittleFS.exists("/config/info.json")) {
        USBSerial.println("info.json not found, creating default");
        String defaultInfo = R"({
            "name": "Modular Macropad",
            "version": "1.0.0",
            "author": "User",
            "description": "Default configuration",
            "module-size": "full",
            "gridSize": { "rows": 3, "columns": 4 },
            "defaults": {},
            "settings": {},
            "supportedComponentTypes": ["button", "encoder", "display"]
        })";
        writeJsonFile("/config/info.json", defaultInfo);
    }
    
    if (!LittleFS.exists("/config/components.json")) {
        USBSerial.println("components.json not found, creating default");
        String defaultComponents = R"({
            "components": [
                {
                    "id": "button-0",
                    "type": "button",
                    "size": { "rows": 1, "columns": 1 },
                    "start_location": { "row": 0, "column": 0 }
                }
            ]
        })";
        writeJsonFile("/config/components.json", defaultComponents);
    }
    
    if (!LittleFS.exists("/config/LEDs.json")) {
        USBSerial.println("LEDs.json not found, creating default");
        String defaultLEDs = R"({
            "leds": {
                "pin": 7,
                "brightness": 30,
                "config": [
                    {
                        "id": "led-0",
                        "stream_address": 0,
                        "button_id": "button-0",
                        "color": {"r": 0, "g": 255, "b": 0},
                        "pressed_color": {"r": 255, "g": 255, "b": 255},
                        "brightness": 30,
                        "mode": 0
                    }
                ]
            }
        })";
        writeJsonFile("/config/LEDs.json", defaultLEDs);
    }
    
    // Load configuration files
    moduleInfo.infoJson = readJsonFile("/config/info.json");
    moduleInfo.componentsJson = readJsonFile("/config/components.json");
    moduleInfo.ledsJson = readJsonFile("/config/LEDs.json");
    
    // Get unique ID from ESP32
    uint64_t chipId = ESP.getEfuseMac();
    char uniqueId[13];
    snprintf(uniqueId, 13, "%04X%08X", (uint16_t)(chipId >> 32), (uint32_t)chipId);
    moduleInfo.macAddress = String(uniqueId);
    
    // Count components
    moduleInfo.numButtons = countComponentsByType(moduleInfo.componentsJson, "button");
    moduleInfo.numRotaryEncoders = countComponentsByType(moduleInfo.componentsJson, "encoder");
    moduleInfo.numSliders = countComponentsByType(moduleInfo.componentsJson, "slider");
    moduleInfo.hasDisplay = countComponentsByType(moduleInfo.componentsJson, "display") > 0;
    
    // Count LEDs
    DynamicJsonDocument ledsDoc(8192);
    DeserializationError error = deserializeJson(ledsDoc, moduleInfo.ledsJson);
    if (!error) {
        JsonArray leds = ledsDoc["leds"]["config"].as<JsonArray>();
        moduleInfo.numLEDs = leds.size();
    } else {
        moduleInfo.numLEDs = 0;
        Serial.print("Error parsing LEDs JSON: ");
        Serial.println(error.c_str());
    }
    
    // Initialize module capabilities based on loaded information
    currentModule = {
        .type = ModuleType::FULL_MAIN,
        .size = ModuleSize::FULL,
        .hasDisplay = moduleInfo.hasDisplay,
        .numButtons = moduleInfo.numButtons,
        .numLEDs = moduleInfo.numLEDs,
        .numRotaryEncoders = moduleInfo.numRotaryEncoders,
        .numSliders = moduleInfo.numSliders,
        .numAS5600Encoders = 0,  // Not currently used
        .moduleVersion = "1.0.0",
        .uniqueId = moduleInfo.macAddress,
        .customName = "Main Module",
        .numLayers = 4
    };
    
    // Merge configuration files into a single config.json
    mergeConfigFiles();
    
    Serial.println("Module info initialized");
    Serial.print("Module ID: ");
    Serial.println(currentModule.uniqueId);
    Serial.print("Buttons: ");
    Serial.println(currentModule.numButtons);
    Serial.print("LEDs: ");
    Serial.println(currentModule.numLEDs);
    Serial.print("Encoders: ");
    Serial.println(currentModule.numRotaryEncoders);
    Serial.print("Display: ");
    Serial.println(currentModule.hasDisplay ? "Yes" : "No");
}

bool mergeConfigFiles() {
    // Parse individual configuration files
    DynamicJsonDocument infoDoc(4096);
    DynamicJsonDocument componentsDoc(8192);
    DynamicJsonDocument ledsDoc(8192);
    
    DeserializationError infoError = deserializeJson(infoDoc, moduleInfo.infoJson);
    DeserializationError componentsError = deserializeJson(componentsDoc, moduleInfo.componentsJson);
    DeserializationError ledsError = deserializeJson(ledsDoc, moduleInfo.ledsJson);
    
    if (infoError || componentsError || ledsError) {
        Serial.println("Error parsing one or more config files");
        return false;
    }
    
    // Create merged configuration document
    DynamicJsonDocument configDoc(16384);
    
    // Set ID as the ESP32's MAC address
    configDoc["id"] = moduleInfo.macAddress;
    
    // Copy data from info.json
    configDoc["name"] = infoDoc["name"];
    configDoc["version"] = infoDoc["version"];
    configDoc["author"] = infoDoc["author"];
    configDoc["description"] = infoDoc["description"];
    configDoc["module-size"] = infoDoc["module-size"];
    configDoc["gridSize"] = infoDoc["gridSize"];
    configDoc["defaults"] = infoDoc["defaults"];
    configDoc["settings"] = infoDoc["settings"];
    configDoc["supportedComponentTypes"] = infoDoc["supportedComponentTypes"];
    
    // Copy components array
    configDoc["components"] = componentsDoc["components"];
    
    // Copy LEDs configuration
    configDoc["leds"] = ledsDoc["leds"];
    
    // Add capabilities information
    JsonObject capabilities = configDoc.createNestedObject("capabilities");
    capabilities["numButtons"] = currentModule.numButtons;
    capabilities["numLEDs"] = currentModule.numLEDs;
    capabilities["numRotaryEncoders"] = currentModule.numRotaryEncoders;
    capabilities["numSliders"] = currentModule.numSliders;
    capabilities["hasDisplay"] = currentModule.hasDisplay;
    capabilities["numLayers"] = currentModule.numLayers;
    
    // Serialize the merged configuration
    String configJson;
    serializeJsonPretty(configDoc, configJson);
    
    // Save to config.json
    return writeJsonFile("/config/config.json", configJson);
}

ModuleCapabilities getModuleCapabilities() {
    return currentModule;
}

String getModuleInfoJson() {
    DynamicJsonDocument doc(512);
    
    doc["type"] = static_cast<int>(currentModule.type);
    doc["typeName"] = getModuleTypeName(currentModule.type);
    doc["size"] = static_cast<int>(currentModule.size);
    doc["sizeName"] = getModuleSizeName(currentModule.size);
    doc["hasDisplay"] = currentModule.hasDisplay;
    doc["numButtons"] = currentModule.numButtons;
    doc["numLEDs"] = currentModule.numLEDs;
    doc["numRotaryEncoders"] = currentModule.numRotaryEncoders;
    doc["numSliders"] = currentModule.numSliders;
    doc["numAS5600Encoders"] = currentModule.numAS5600Encoders;
    doc["moduleVersion"] = currentModule.moduleVersion;
    doc["uniqueId"] = currentModule.uniqueId;
    doc["customName"] = currentModule.customName;
    doc["numLayers"] = currentModule.numLayers;
    
    String output;
    serializeJson(doc, output);
    return output;
}

const char* getModuleTypeName(ModuleType type) {
    switch(type) {
        // Full-size modules
        case ModuleType::FULL_MAIN:
            return "Main Control";
        case ModuleType::FULL_SLIDER:
            return "Slider Bank";
        case ModuleType::FULL_ENCODER:
            return "Encoder Bank";
        
        // Half-size modules
        case ModuleType::HALF_SLIDER:
            return "Compact Sliders";
        case ModuleType::HALF_ENCODER:
            return "Compact Encoders";
        case ModuleType::HALF_BUTTON:
            return "Button Grid";
        
        // Quarter-size modules
        case ModuleType::QUARTER_BUTTON:
            return "Mini Buttons";
        case ModuleType::QUARTER_ENCODER:
            return "Single Encoder";
        
        // Special
        case ModuleType::CUSTOM:
            return "Custom Module";
            
        default:
            return "Unknown Module";
    }
}

const char* getModuleSizeName(ModuleSize size) {
    switch(size) {
        case ModuleSize::FULL:
            return "Full (1x1)";
        case ModuleSize::HALF:
            return "Half (1x0.5)";
        case ModuleSize::QUARTER:
            return "Quarter (0.5x0.5)";
        default:
            return "Unknown Size";
    }
}

bool loadModuleConfiguration() {
    String configJson = readJsonFile("/config/config.json");
    if (configJson.isEmpty()) {
        return false;
    }
    
    DynamicJsonDocument doc(16384);
    DeserializationError error = deserializeJson(doc, configJson);
    
    if (error) {
        Serial.print("Error parsing config.json: ");
        Serial.println(error.c_str());
        return false;
    }
    
    // Update module information from config
    if (doc.containsKey("id")) {
        currentModule.uniqueId = doc["id"].as<String>();
    }
    
    if (doc.containsKey("capabilities")) {
        JsonObject capabilities = doc["capabilities"];
        currentModule.numButtons = capabilities["numButtons"].as<uint8_t>();
        currentModule.numLEDs = capabilities["numLEDs"].as<uint8_t>();
        currentModule.numRotaryEncoders = capabilities["numRotaryEncoders"].as<uint8_t>();
        currentModule.numSliders = capabilities["numSliders"].as<uint8_t>();
        currentModule.hasDisplay = capabilities["hasDisplay"].as<bool>();
        currentModule.numLayers = capabilities["numLayers"].as<uint8_t>();
    }
    
    return true;
}

bool saveModuleConfiguration() {
    // This function would update config.json with any changes made during runtime
    return mergeConfigFiles();
}