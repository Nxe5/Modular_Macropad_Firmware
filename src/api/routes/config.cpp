#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>

void handleGetReportsConfig(AsyncWebServerRequest *request) {
  if (!SPIFFS.exists("/data/config/reports.json")) {
    request->send(404, "application/json", "{\"error\":\"Reports config not found\"}");
    return;
  }

  File file = SPIFFS.open("/data/config/reports.json", "r");
  if (!file) {
    request->send(500, "application/json", "{\"error\":\"Failed to open reports config\"}");
    return;
  }

  String content = file.readString();
  file.close();

  request->send(200, "application/json", content);
}

void setupConfigRoutes(AsyncWebServer *server) {
  // ... existing routes ...

  // Add reports configuration endpoint
  server->on("/api/config/reports", HTTP_GET, handleGetReportsConfig);

  // ... rest of existing code ...
} 