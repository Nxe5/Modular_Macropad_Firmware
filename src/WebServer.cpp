#include "WebServer.h"
#include "LEDHandler.h"
#include "ModuleSetup.h"

WebServer::WebServer() : server(80) {}

WebServer::~WebServer() {
    // Cleanup code if needed
}

void WebServer::begin() {
    // Note: SPIFFS is initialized in main.cpp, no need to initialize again

    // Load default config if none exists
    if (!SPIFFS.exists(CONFIG_FILE)) {
        loadDefaultConfig();
    }

    // CORS headers for web access
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, OPTIONS");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Content-Type");

    // Route to serve the web interface
    server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");

    // API endpoints for general configuration
    server.on("/api/config", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleGetConfig(request);
    });

    server.on("/api/config", HTTP_POST, 
        [](AsyncWebServerRequest* request) {}, 
        NULL,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            handleSetConfig(request, data, len);
        }
    );

    server.on("/api/device", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleGetDeviceInfo(request);
    });

    // LED API ENDPOINTS

    // Get all LED configurations
    server.on("/api/leds", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleGetLEDsConfig(request);
    });

    // Update LED configurations
    server.on("/api/leds", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        NULL,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            handleSetLEDsConfig(request, data, len);
        }
    );

    // Get specific LED configuration
    server.on("^\\/api\\/leds\\/([0-9]+)$", HTTP_GET, [this](AsyncWebServerRequest* request) {
        String url = request->url();
        int ledIndex = url.substring(url.lastIndexOf('/') + 1).toInt();
        handleGetLEDConfig(request, ledIndex);
    });

    // Update specific LED configuration
    server.on("^\\/api\\/leds\\/([0-9]+)$", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        NULL,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            String url = request->url();
            int ledIndex = url.substring(url.lastIndexOf('/') + 1).toInt();
            handleSetLEDConfig(request, data, len, ledIndex);
        }
    );

    // Control LED animations
    server.on("/api/leds/animation", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        NULL,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            handleLEDAnimation(request, data, len);
        }
    );

    // Set a color for all LEDs
    server.on("/api/leds/all", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        NULL,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            handleSetAllLEDs(request, data, len);
        }
    );

    // Save LED configuration
    server.on("/api/leds/save", HTTP_POST, [this](AsyncWebServerRequest* request) {
        bool success = saveLEDConfig();
        if (success) {
            request->send(200, "application/json", "{\"status\": \"LED configuration saved\"}");
        } else {
            request->send(500, "application/json", "{\"error\": \"Failed to save LED configuration\"}");
        }
    });

    // OPTIONS request handling for CORS
    server.onNotFound([](AsyncWebServerRequest* request) {
        if (request->method() == HTTP_OPTIONS) {
            request->send(200);
        } else {
            request->send(404, "text/plain", "Not found");
        }
    });

    server.begin();
    
    Serial.println("Web server started");
}

void WebServer::handleGetConfig(AsyncWebServerRequest* request) {
    File file = SPIFFS.open(CONFIG_FILE, "r");
    if (!file) {
        request->send(404, "application/json", "{\"error\": \"Config not found\"}");
        return;
    }
    
    request->send(SPIFFS, CONFIG_FILE, "application/json");
}

void WebServer::handleSetConfig(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    DynamicJsonDocument doc(4096);
    DeserializationError error = deserializeJson(doc, (char*)data);
    
    if (error) {
        request->send(400, "application/json", "{\"error\": \"Invalid JSON\"}");
        return;
    }

    File file = SPIFFS.open(CONFIG_FILE, "w");
    if (!file) {
        request->send(500, "application/json", "{\"error\": \"Failed to open config file\"}");
        return;
    }

    serializeJson(doc, file);
    file.close();

    // Trigger config reload
    // loadConfiguration(); // From your existing code

    request->send(200, "application/json", "{\"status\": \"Configuration updated\"}");
}

void WebServer::handleGetDeviceInfo(AsyncWebServerRequest* request) {
    String deviceInfo = getModuleInfoJson(); // From your existing ModuleInfo
    request->send(200, "application/json", deviceInfo);
}

bool WebServer::loadDefaultConfig() {
    DynamicJsonDocument doc(4096);
    
    // Create default configuration
    doc["id"] = getModuleCapabilities().uniqueId;
    doc["name"] = "Default Module";
    doc["type"] = getModuleTypeName(getModuleCapabilities().type);
    
    // Add default components based on module capabilities
    JsonArray components = doc.createNestedArray("components");
    // Add components based on your module type...

    File file = SPIFFS.open(CONFIG_FILE, "w");
    if (!file) {
        return false;
    }

    serializeJson(doc, file);
    file.close();
    return true;
}

// LED API handlers

void WebServer::handleGetLEDsConfig(AsyncWebServerRequest* request) {
    String ledConfig = getLEDConfigJson();
    request->send(200, "application/json", ledConfig);
}

void WebServer::handleSetLEDsConfig(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    if (len == 0) {
        request->send(400, "application/json", "{\"error\": \"Empty request\"}");
        return;
    }

    // Create a null-terminated string from the data
    char* jsonStr = new char[len + 1];
    memcpy(jsonStr, data, len);
    jsonStr[len] = '\0';

    // Update LED configuration
    bool success = updateLEDConfigFromJson(String(jsonStr));
    delete[] jsonStr;

    if (success) {
        request->send(200, "application/json", "{\"status\": \"LED configuration updated\"}");
    } else {
        request->send(400, "application/json", "{\"error\": \"Invalid LED configuration\"}");
    }
}

void WebServer::handleGetLEDConfig(AsyncWebServerRequest* request, int ledIndex) {
    if (ledIndex < 0 || ledIndex >= numLEDs) {
        request->send(400, "application/json", "{\"error\": \"Invalid LED index\"}");
        return;
    }

    // Create JSON document with just this LED's info
    DynamicJsonDocument doc(256);
    doc["index"] = ledIndex;
    doc["r"] = ledConfigs[ledIndex].r;
    doc["g"] = ledConfigs[ledIndex].g;
    doc["b"] = ledConfigs[ledIndex].b;
    doc["brightness"] = ledConfigs[ledIndex].brightness;
    doc["mode"] = ledConfigs[ledIndex].mode;
    
    // Create hex color string
    char hexColor[8];
    sprintf(hexColor, "#%02X%02X%02X", ledConfigs[ledIndex].r, ledConfigs[ledIndex].g, ledConfigs[ledIndex].b);
    doc["hexColor"] = hexColor;

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void WebServer::handleSetLEDConfig(AsyncWebServerRequest* request, uint8_t* data, size_t len, int ledIndex) {
    if (ledIndex < 0 || ledIndex >= numLEDs) {
        request->send(400, "application/json", "{\"error\": \"Invalid LED index\"}");
        return;
    }

    if (len == 0) {
        request->send(400, "application/json", "{\"error\": \"Empty request\"}");
        return;
    }

    // Create a null-terminated string from the data
    char* jsonStr = new char[len + 1];
    memcpy(jsonStr, data, len);
    jsonStr[len] = '\0';

    // Parse the JSON
    DynamicJsonDocument doc(256);
    DeserializationError error = deserializeJson(doc, jsonStr);
    delete[] jsonStr;

    if (error) {
        request->send(400, "application/json", "{\"error\": \"Invalid JSON\"}");
        return;
    }

    // Update LED based on received data
    bool updated = false;

    // Update from hex color if present
    if (doc.containsKey("hexColor")) {
        String hexColor = doc["hexColor"].as<String>();
        
        // Remove '#' if present
        if (hexColor.startsWith("#")) {
            hexColor = hexColor.substring(1);
        }
        
        // Convert hex string to uint32_t
        uint32_t color = strtoul(hexColor.c_str(), NULL, 16);
        setLEDColorHex(ledIndex, color);
        updated = true;
    }
    // Otherwise update from RGB
    else if (doc.containsKey("r") && doc.containsKey("g") && doc.containsKey("b")) {
        uint8_t r = doc["r"].as<uint8_t>();
        uint8_t g = doc["g"].as<uint8_t>();
        uint8_t b = doc["b"].as<uint8_t>();
        
        if (doc.containsKey("brightness")) {
            uint8_t brightness = doc["brightness"].as<uint8_t>();
            setLEDColorWithBrightness(ledIndex, r, g, b, brightness);
        } else {
            setLEDColor(ledIndex, r, g, b);
        }
        updated = true;
    }

    if (updated) {
        request->send(200, "application/json", "{\"status\": \"LED configuration updated\"}");
    } else {
        request->send(400, "application/json", "{\"error\": \"No valid LED data provided\"}");
    }
}

void WebServer::handleLEDAnimation(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    if (len == 0) {
        request->send(400, "application/json", "{\"error\": \"Empty request\"}");
        return;
    }

    // Create a null-terminated string from the data
    char* jsonStr = new char[len + 1];
    memcpy(jsonStr, data, len);
    jsonStr[len] = '\0';

    // Parse the JSON
    DynamicJsonDocument doc(256);
    DeserializationError error = deserializeJson(doc, jsonStr);
    delete[] jsonStr;

    if (error) {
        request->send(400, "application/json", "{\"error\": \"Invalid JSON\"}");
        return;
    }

    // Check if we should start or stop an animation
    if (doc.containsKey("active")) {
        bool active = doc["active"].as<bool>();
        
        if (active) {
            // Start animation
            uint8_t mode = doc.containsKey("mode") ? 
                doc["mode"].as<uint8_t>() : LED_ANIM_RAINBOW;
            uint16_t speed = doc.containsKey("speed") ? 
                doc["speed"].as<uint16_t>() : 100;
            
            startAnimation(mode, speed);
            request->send(200, "application/json", "{\"status\": \"Animation started\"}");
        } else {
            // Stop animation
            stopAnimation();
            request->send(200, "application/json", "{\"status\": \"Animation stopped\"}");
        }
    } else {
        request->send(400, "application/json", "{\"error\": \"Missing 'active' field\"}");
    }
}

void WebServer::handleSetAllLEDs(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    if (len == 0) {
        request->send(400, "application/json", "{\"error\": \"Empty request\"}");
        return;
    }

    // Create a null-terminated string from the data
    char* jsonStr = new char[len + 1];
    memcpy(jsonStr, data, len);
    jsonStr[len] = '\0';

    // Parse the JSON
    DynamicJsonDocument doc(256);
    DeserializationError error = deserializeJson(doc, jsonStr);
    delete[] jsonStr;

    if (error) {
        request->send(400, "application/json", "{\"error\": \"Invalid JSON\"}");
        return;
    }

    bool updated = false;

    // Update from hex color if present
    if (doc.containsKey("hexColor")) {
        String hexColor = doc["hexColor"].as<String>();
        
        // Remove '#' if present
        if (hexColor.startsWith("#")) {
            hexColor = hexColor.substring(1);
        }
        
        // Convert hex string to RGB
        uint32_t colorVal = strtoul(hexColor.c_str(), NULL, 16);
        uint8_t r = (colorVal >> 16) & 0xFF;
        uint8_t g = (colorVal >> 8) & 0xFF;
        uint8_t b = colorVal & 0xFF;
        
        setAllLEDs(r, g, b);
        updated = true;
    }
    // Otherwise update from RGB
    else if (doc.containsKey("r") && doc.containsKey("g") && doc.containsKey("b")) {
        uint8_t r = doc["r"].as<uint8_t>();
        uint8_t g = doc["g"].as<uint8_t>();
        uint8_t b = doc["b"].as<uint8_t>();
        
        setAllLEDs(r, g, b);
        updated = true;
    }

    if (updated) {
        request->send(200, "application/json", "{\"status\": \"All LEDs updated\"}");
    } else {
        request->send(400, "application/json", "{\"error\": \"No valid color data provided\"}");
    }
}