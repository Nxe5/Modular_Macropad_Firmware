#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include "ModuleSetup.h"

class WebServer {
private:
    AsyncWebServer server;
    const char* CONFIG_FILE = "/data/config/config.json";
    
    // Basic configuration handlers
    void handleGetConfig(AsyncWebServerRequest* request);
    void handleSetConfig(AsyncWebServerRequest* request, uint8_t* data, size_t len);
    void handleGetDeviceInfo(AsyncWebServerRequest* request);
    bool loadDefaultConfig();

    // LED API handlers
    void handleGetLEDsConfig(AsyncWebServerRequest* request);
    void handleSetLEDsConfig(AsyncWebServerRequest* request, uint8_t* data, size_t len);
    void handleGetLEDConfig(AsyncWebServerRequest* request, int ledIndex);
    void handleSetLEDConfig(AsyncWebServerRequest* request, uint8_t* data, size_t len, int ledIndex);
    void handleLEDAnimation(AsyncWebServerRequest* request, uint8_t* data, size_t len);
    void handleSetAllLEDs(AsyncWebServerRequest* request, uint8_t* data, size_t len);

public:
    WebServer();
    ~WebServer();
    void begin();
};

#endif // WEB_SERVER_H