#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

void handleGetReportsConfig(AsyncWebServerRequest *request) {
  if (!LittleFS.exists("/config/reports.json")) {
    request->send(404, "application/json", "{\"error\":\"Reports config not found\"}");
    Serial.println("Reports config not found at /config/reports.json");
    return;
  }

  File file = LittleFS.open("/config/reports.json", "r");
  if (!file) {
    request->send(500, "application/json", "{\"error\":\"Failed to open reports config\"}");
    Serial.println("Failed to open reports config file");
    return;
  }

  String content = file.readString();
  file.close();
  Serial.printf("Successfully read reports config (%d bytes)\n", content.length());

  request->send(200, "application/json", content);
}

void handleGetActionsConfig(AsyncWebServerRequest *request) {
  if (!LittleFS.exists("/config/actions.json")) {
    request->send(404, "application/json", "{\"error\":\"Actions config not found\"}");
    return;
  }

  File file = LittleFS.open("/config/actions.json", "r");
  if (!file) {
    request->send(500, "application/json", "{\"error\":\"Failed to open actions config\"}");
    return;
  }

  String content = file.readString();
  file.close();

  request->send(200, "application/json", content);
}

void handlePostActionsConfig(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
  // Add CORS headers
  AsyncWebServerResponse *response = request->beginResponse(200);
  response->addHeader("Access-Control-Allow-Origin", "*");
  response->addHeader("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
  response->addHeader("Access-Control-Allow-Headers", "Content-Type");

  // Handle OPTIONS request for CORS preflight
  if (request->method() == HTTP_OPTIONS) {
    request->send(response);
    return;
  }

  if (!LittleFS.exists("/config/actions.json")) {
    request->send(404, "application/json", "{\"error\":\"Actions config not found\"}");
    return;
  }

  // Read current actions config
  File file = LittleFS.open("/config/actions.json", "r");
  if (!file) {
    request->send(500, "application/json", "{\"error\":\"Failed to open actions config\"}");
    return;
  }

  // Parse current config
  StaticJsonDocument<4096> doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();

  if (error) {
    request->send(500, "application/json", "{\"error\":\"Failed to parse current actions config\"}");
    return;
  }

  // Parse the new config from the request body
  StaticJsonDocument<4096> newDoc;
  error = deserializeJson(newDoc, (char*)data);
  if (error) {
    request->send(400, "application/json", "{\"error\":\"Invalid JSON in request body\"}");
    return;
  }

  // Get the new layer config from the request
  JsonObject newConfig = newDoc["actions"]["layer-config"];
  if (!newConfig) {
    request->send(400, "application/json", "{\"error\":\"Invalid request format\"}");
    return;
  }

  // Update only the components that have new values
  JsonObject currentLayerConfig = doc["actions"]["layer-config"];
  for (JsonPair kv : newConfig) {
    const char* componentId = kv.key().c_str();
    JsonObject newComponentConfig = kv.value().as<JsonObject>();
    
    if (newComponentConfig.isNull()) {
      // If the new config is null, remove the component from current config
      currentLayerConfig.remove(componentId);
    } else {
      // Update or add the component config
      currentLayerConfig[componentId] = newComponentConfig;
    }
  }

  // Write the updated config back to the file
  file = LittleFS.open("/config/actions.json", "w");
  if (!file) {
    request->send(500, "application/json", "{\"error\":\"Failed to write actions config\"}");
    return;
  }

  if (serializeJson(doc, file) == 0) {
    file.close();
    request->send(500, "application/json", "{\"error\":\"Failed to write actions config\"}");
    return;
  }

  file.close();
  request->send(200, "application/json", "{\"message\":\"Actions config updated successfully\"}");
}

void setupConfigRoutes(AsyncWebServer *server) {
  // Add reports configuration endpoint
  server->on("/api/config/reports", HTTP_GET, handleGetReportsConfig);

  // Add actions configuration endpoints
  server->on("/api/config/actions", HTTP_GET, handleGetActionsConfig);
  
  // Register POST handler with body handling
  server->on("/api/config/actions", HTTP_POST, 
    [](AsyncWebServerRequest *request) {}, 
    NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      handlePostActionsConfig(request, data, len, index, total);
    }
  );

  // Add CORS preflight handler
  server->on("/api/config/actions", HTTP_OPTIONS, [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse(200);
    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
    response->addHeader("Access-Control-Allow-Headers", "Content-Type");
    request->send(response);
  });
} 