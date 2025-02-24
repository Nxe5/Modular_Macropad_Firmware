
// WebServer.cpp
#include "WebServer.h"

WebServer::WebServer() : server(80) {}

void WebServer::begin() {
    if (!SPIFFS.begin(true)) {
        Serial.println("SPIFFS Mount Failed");
        return;
    }

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

    // API endpoints
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

    server.begin();
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
    loadConfiguration(); // From your existing code

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