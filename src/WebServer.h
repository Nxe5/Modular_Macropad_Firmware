// WebServer.h
#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include "ModuleInfo.h"

class WebServer {
private:
    AsyncWebServer server;
    const char* CONFIG_FILE = "/config.json";
    
    void handleGetConfig(AsyncWebServerRequest* request);
    void handleSetConfig(AsyncWebServerRequest* request, uint8_t* data, size_t len);
    void handleGetDeviceInfo(AsyncWebServerRequest* request);
    bool loadDefaultConfig();

public:
    WebServer();
    void begin();
};

#endif // WEB_SERVER_H
