#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>

AsyncWebServer configServer(80);

void initializeConfigServer() {
  // Note: SPIFFS is initialized in main.cpp, no need to initialize again

  // Serve the configuration file over HTTP
  configServer.on("/download_config", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/module_config.json", "application/json");
  });

  configServer.begin();
  
  Serial.println("Configuration server started");
}