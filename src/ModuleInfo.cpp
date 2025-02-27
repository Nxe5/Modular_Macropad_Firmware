// ModuleInfo.cpp
#include "ModuleInfo.h"
#include <ArduinoJson.h>
#include "FS.h"
#include "SPIFFS.h"

ModuleCapabilities currentModule;
ModuleInfo moduleInfo;

// Function to read a JSON file from SPIFFS
String readJsonFile(const char* filePath) {
    if (!SPIFFS.exists(filePath)) {
        Serial.printf("File not found: %s\n", filePath);
        return "";
    }
    
    File file = SPIFFS.open(filePath, "r");
    if (!file) {
        Serial.printf("Failed to open file: %s\n", filePath);
        return "";
    }
    
    String content = file.readString();
    file.close();
    return content;
}

// Function to write a JSON file to SPIFFS
bool writeJsonFile(const char* filePath, const String& content) {
    File file = SPIFFS.open(filePath, "w");
    if (!file) {
        Serial.printf("Failed to create file: %s\n", filePath);
        return false;
    }
    
    size_t bytesWritten = file.print(content);
    file.close();
    
    if (bytesWritten == content.length()) {
        Serial.printf("File written successfully: %s\n", filePath);
        return true;
    } else {
        Serial.printf("Write failed: %s\n", filePath);
        return false;
    }
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
    // Load configuration files
    moduleInfo.infoJson = readJsonFile("/data/info.json");
    moduleInfo.componentsJson = readJsonFile("/data/components.json");
    moduleInfo.ledsJson = readJsonFile("/data/LEDs.json");
    
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
    return writeJsonFile("/data/config.json", configJson);
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
    String configJson = readJsonFile("/data/config.json");
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