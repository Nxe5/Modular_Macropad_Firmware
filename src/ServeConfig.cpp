#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>

AsyncWebServer server(80);

void setup() {
  Serial.begin(115200);

  // Initialize SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS mount failed");
    return;
  }

  // Serve the configuration file over HTTP
  server.on("/download_config", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/module_config.json", "application/json");
  });

  server.begin();
}

void loop() {
  // Main code loop
}
