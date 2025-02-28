#include "ConfigManager.h"

String ConfigManager::readFile(const char* filePath) {
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

std::map<String, ActionConfig> ConfigManager::loadActions(const char* filePath) {
    std::map<String, ActionConfig> actions;
    String jsonStr = readFile(filePath);
    if (jsonStr.isEmpty()) return actions;
    
    DynamicJsonDocument doc(16384);
    DeserializationError error = deserializeJson(doc, jsonStr);
    if (error) {
        Serial.printf("Error parsing %s: %s\n", filePath, error.c_str());
        return actions;
    }
    
    JsonObject layerConfig = doc["actions"]["layer-config"].as<JsonObject>();
    for (JsonPair kv : layerConfig) {
        ActionConfig ac;
        ac.id = String(kv.key().c_str());
        JsonObject cfg = kv.value().as<JsonObject>();
        ac.type = cfg["type"].as<String>();
        
        // For HID actions, parse the report (support nested arrays)
        if (ac.type == "hid") {
            JsonVariant buttonPressVar = cfg["buttonPress"];
            if (buttonPressVar.is<JsonArray>() && buttonPressVar[0].is<JsonArray>()) {
                JsonArray report = buttonPressVar[0].as<JsonArray>();
                for (JsonVariant v : report)
                    ac.hidReport.push_back(v.as<String>());
            } else {
                JsonArray report = buttonPressVar.as<JsonArray>();
                for (JsonVariant v : report)
                    ac.hidReport.push_back(v.as<String>());
            }
            // Handle encoder extra keys if needed
            if (cfg.containsKey("clockwise")) {
                for (JsonVariant v : cfg["clockwise"].as<JsonArray>())
                    ac.clockwise.push_back(v.as<String>());
            }
            if (cfg.containsKey("counterclockwise")) {
                for (JsonVariant v : cfg["counterclockwise"].as<JsonArray>())
                    ac.counterclockwise.push_back(v.as<String>());
            }
        }
        else if (ac.type == "macro") {
            ac.macroId = cfg["macroId"].as<String>();
        }
        else if (ac.type == "layer") {
            ac.targetLayer = cfg["layerId"].as<String>();
        }
        else if (ac.type == "multimedia") {
            JsonArray report = cfg["consumerReport"].as<JsonArray>();
            for (JsonVariant v : report)
                ac.consumerReport.push_back(v.as<String>());
        }
        
        actions[ac.id] = ac;
    }
    return actions;
}
