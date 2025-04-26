#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <USBCDC.h>
#include <algorithm>
#include <WiFi.h>
#include "OTAUpdateManager.h"
#include "VersionManager.h"
#include "UpdateProgressDisplay.h"
#include "ConfigManager.h"
#include "KeyHandler.h"
#include "LEDHandler.h"

// Forward declaration - make this function available to other cpp files
extern void setupConfigRoutes(AsyncWebServer *server);
extern USBCDC USBSerial; // Global serial reference
extern KeyHandler* keyHandler; // Access to the key handler
extern bool updateLEDConfigFromJson(const String& json); // Access to LED config update function

// Define a smaller document size for safer memory usage
const size_t JSON_DOCUMENT_SIZE = 8192;

// Generalized handler to serve any JSON config file
void handleGetConfigFile(AsyncWebServerRequest *request, const char* filePath, bool allowFailover = true) {
  // First check if the file exists directly in the requested path
  if (!LittleFS.exists(filePath)) {
    // If not found, try in the /data prefix
    String dataPath = String("/data") + filePath;
    
    if (!allowFailover || !LittleFS.exists(dataPath)) {
      USBSerial.printf("Config file not found in either %s or %s\n", filePath, dataPath.c_str());
      request->send(404, "application/json", "{\"error\":\"Config file not found\"}");
      return;
    } else {
      // Found in data directory
      USBSerial.printf("Config file found in alternate location: %s\n", dataPath.c_str());
      filePath = dataPath.c_str();
    }
  }

  File file = LittleFS.open(filePath, "r");
  if (!file) {
    USBSerial.printf("Failed to open config file: %s\n", filePath);
    request->send(500, "application/json", "{\"error\":\"Failed to open config file\"}");
    return;
  }

  // Check file size before reading
  size_t fileSize = file.size();
  USBSerial.printf("Config file size: %d bytes\n", fileSize);
  
  if (fileSize == 0) {
    file.close();
    USBSerial.printf("Config file is empty: %s\n", filePath);
    request->send(500, "application/json", "{\"error\":\"Config file is empty\"}");
    return;
  }
  
  // Read file in small chunks if it's large
  if (fileSize > 16384) {
    // For very large files, stream them instead of loading into memory
    USBSerial.printf("File is large (%d bytes), streaming response\n", fileSize);
    AsyncWebServerResponse *response = request->beginResponse(LittleFS, filePath, "application/json");
    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
    response->addHeader("Access-Control-Allow-Headers", "Content-Type");
    request->send(response);
    return;
  }
  
  // For smaller files, read them into memory
  String content = file.readString();
  file.close();
  
  USBSerial.printf("Successfully read config file %s (%d bytes)\n", filePath, content.length());

  // Create a response and set CORS headers
  AsyncWebServerResponse *response = request->beginResponse(200, "application/json", content);
  response->addHeader("Access-Control-Allow-Origin", "*");
  response->addHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
  response->addHeader("Access-Control-Allow-Headers", "Content-Type");
  request->send(response);
}

// Simplified handleGetActionsConfig function that uses handleGetConfigFile
void handleGetActionsConfig(AsyncWebServerRequest *request) {
  USBSerial.println("API: Requested /api/config/actions");
  
  // Check if file exists in primary location
  String primaryPath = "/config/actions.json";
  String fallbackPath = "/data/config/actions.json";
  String defaultPath = "/config/defaults/actions.json";
  
  // Check in order: primary, fallback, defaults
  if (LittleFS.exists(primaryPath)) {
    USBSerial.printf("Found actions.json in primary location: %s\n", primaryPath.c_str());
    handleGetConfigFile(request, primaryPath.c_str(), false);
  } else if (LittleFS.exists(fallbackPath)) {
    USBSerial.printf("Found actions.json in fallback location: %s\n", fallbackPath.c_str());
    handleGetConfigFile(request, fallbackPath.c_str(), false);
  } else if (LittleFS.exists(defaultPath)) {
    USBSerial.printf("Using default actions.json: %s\n", defaultPath.c_str());
    handleGetConfigFile(request, defaultPath.c_str(), false);
  } else {
    // No action config files found, return an error
    USBSerial.println("API ERROR: actions.json not found in any location");
    request->send(404, "application/json", "{\"error\":\"Actions config not found\"}");
  }
}

void handleGetReportsConfig(AsyncWebServerRequest *request) {
  USBSerial.println("API: Requested /api/config/reports");
  handleGetConfigFile(request, "/config/reports.json");
}

void handleGetComponentsConfig(AsyncWebServerRequest *request) {
  USBSerial.println("API: Requested /api/config/components");
  handleGetConfigFile(request, "/config/components.json");
}

void handleGetLedsConfig(AsyncWebServerRequest *request) {
  USBSerial.println("API: Requested /api/config/leds");
  
  // Check if file exists in primary location
  String primaryPath = "/config/leds.json";
  String fallbackPath = "/data/config/leds.json";
  String defaultPath = "/config/defaults/leds.json";
  
  // Check in order: primary, fallback, defaults
  if (LittleFS.exists(primaryPath)) {
    USBSerial.printf("Found leds.json in primary location: %s\n", primaryPath.c_str());
    handleGetConfigFile(request, primaryPath.c_str(), false);
  } else if (LittleFS.exists(fallbackPath)) {
    USBSerial.printf("Found leds.json in fallback location: %s\n", fallbackPath.c_str());
    handleGetConfigFile(request, fallbackPath.c_str(), false);
  } else if (LittleFS.exists(defaultPath)) {
    USBSerial.printf("Using default leds.json: %s\n", defaultPath.c_str());
    handleGetConfigFile(request, defaultPath.c_str(), false);
  } else {
    // Create an empty LED config file with default structure
    USBSerial.println("No leds.json found, creating empty one");
    
    DynamicJsonDocument doc(JSON_DOCUMENT_SIZE);
    doc["leds"] = JsonObject();
    doc["leds"]["mode"] = "static";
    
    // Create a default LED array with all LEDs off
    JsonArray ledsArray = doc["leds"].createNestedArray("leds");
    for (int i = 0; i < 16; i++) {
      JsonObject led = ledsArray.createNestedObject();
      led["r"] = 0;
      led["g"] = 0;
      led["b"] = 0;
    }
    
    // Save to file
    File file = LittleFS.open(primaryPath, "w");
    if (file) {
      serializeJson(doc, file);
      file.close();
      handleGetConfigFile(request, primaryPath.c_str(), false);
    } else {
      request->send(500, "application/json", "{\"error\":\"Failed to create LEDs config\"}");
    }
  }
}

void handlePostLedsConfig(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
  static String accumulatedData = "";
  
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

  // Accumulate the data
  if (index == 0) {
    accumulatedData = ""; // Reset for new request
    USBSerial.println("\n===== [LED CONFIG] Starting new LED config update =====");
  }
  accumulatedData += String((char*)data, len);
  USBSerial.printf("[LED CONFIG] Accumulated %d/%d bytes of data\n", index + len, total);
  
  // Only process the complete data in the last chunk
  if (index + len < total) {
    return; // Wait for more data
  }

  USBSerial.printf("[LED CONFIG] Received complete LED config update, %d bytes\n", accumulatedData.length());
  
  // Debugging: print memory status
  USBSerial.printf("[LED CONFIG] Free heap: %d bytes\n", ESP.getFreeHeap());
  
  // Ensure the config directory exists
  if (!LittleFS.exists("/config")) {
    USBSerial.println("[LED CONFIG] Config directory doesn't exist, creating it");
    if (!LittleFS.mkdir("/config")) {
      USBSerial.println("[LED CONFIG] ERROR: Failed to create config directory");
      request->send(500, "application/json", "{\"error\":\"Failed to create config directory\"}");
      accumulatedData = ""; // Reset for next request
      return;
    }
  }

  // Parse the JSON data
  USBSerial.println("[LED CONFIG] Parsing JSON data");
  DynamicJsonDocument doc(JSON_DOCUMENT_SIZE);
  DeserializationError error = deserializeJson(doc, accumulatedData);
  if (error) {
    USBSerial.printf("[LED CONFIG] ERROR: Invalid JSON in LED config: %s\n", error.c_str());
    
    // Create properly formatted error message
    String errorMsg = "{\"error\":\"Invalid JSON in request body: ";
    errorMsg += error.c_str();
    errorMsg += "\"}";
    
    request->send(400, "application/json", errorMsg);
    accumulatedData = ""; // Reset for next request
    return;
  }
  USBSerial.println("[LED CONFIG] JSON parsing successful");

  // Log some details about the LED config
  USBSerial.println("[LED CONFIG] JSON structure overview:");
  if (doc.containsKey("leds")) {
    USBSerial.println("[LED CONFIG] - Contains 'leds' key");
    if (doc["leds"].containsKey("pin")) {
      USBSerial.printf("[LED CONFIG] - LED Pin: %d\n", doc["leds"]["pin"].as<int>());
    }
    if (doc["leds"].containsKey("brightness")) {
      USBSerial.printf("[LED CONFIG] - Brightness: %d\n", doc["leds"]["brightness"].as<int>());
    }
    if (doc["leds"].containsKey("config") && doc["leds"]["config"].is<JsonArray>()) {
      USBSerial.printf("[LED CONFIG] - LED count (config): %d\n", doc["leds"]["config"].size());
    }
    if (doc["leds"].containsKey("layers") && doc["leds"]["layers"].is<JsonArray>()) {
      USBSerial.printf("[LED CONFIG] - Layer count: %d\n", doc["leds"]["layers"].size());
    }
  }

  // Write the config directly to file
  USBSerial.println("[LED CONFIG] Opening file for writing: /config/leds.json");
  File file = LittleFS.open("/config/leds.json", "w");
  if (!file) {
    USBSerial.println("[LED CONFIG] ERROR: Failed to open LEDs config file for writing");
    request->send(500, "application/json", "{\"error\":\"Failed to open LEDs config for writing\"}");
    accumulatedData = ""; // Reset for next request
    return;
  }

  // Serialize the updated config
  USBSerial.println("[LED CONFIG] Serializing JSON to file");
  size_t serializedSize = serializeJson(doc, file);
  file.close();
  
  if (serializedSize == 0) {
    USBSerial.println("[LED CONFIG] ERROR: Failed to write LED config - serialized size is 0");
    request->send(500, "application/json", "{\"error\":\"Failed to write LEDs config\"}");
    accumulatedData = ""; // Reset for next request
    return;
  }

  USBSerial.printf("[LED CONFIG] Successfully wrote %d bytes to file\n", serializedSize);
  
  // Verify the file was written correctly by reading it back
  USBSerial.println("[LED CONFIG] Verifying file was written correctly");
  file = LittleFS.open("/config/leds.json", "r");
  if (!file) {
    USBSerial.println("[LED CONFIG] ERROR: Could not open file for verification");
  } else {
    size_t fileSize = file.size();
    USBSerial.printf("[LED CONFIG] File size after write: %d bytes\n", fileSize);
    file.close();
  }
  
  // Add a short delay before reloading configuration
  USBSerial.println("[LED CONFIG] Waiting 1 second before applying LED configuration...");
  delay(1000); // 1 second delay to ensure file operations are complete
  
  // IMPORTANT: Apply the new configuration to the in-memory LED state
  USBSerial.println("[LED CONFIG] Applying configuration to in-memory state via updateLEDConfigFromJson()");
  bool configApplied = updateLEDConfigFromJson(accumulatedData);
  if (configApplied) {
    USBSerial.println("[LED CONFIG] Successfully applied LED configuration to in-memory state");
  } else {
    USBSerial.println("[LED CONFIG] ERROR: Failed to apply LED configuration to in-memory state");
    
    // Try to diagnose why it failed
    USBSerial.println("[LED CONFIG] Attempting to diagnose failure:");
    
    // Check if strip is initialized
    extern Adafruit_NeoPixel* strip;
    if (!strip) {
      USBSerial.println("[LED CONFIG] - LED strip is not initialized (strip is NULL)");
    } else {
      USBSerial.printf("[LED CONFIG] - LED strip is initialized with %d LEDs\n", strip->numPixels());
    }
    
    // Check if LEDHandler is accessible
    extern LEDConfig* ledConfigs;
    extern uint8_t numLEDs;
    USBSerial.printf("[LED CONFIG] - numLEDs = %d\n", numLEDs);
    if (!ledConfigs) {
      USBSerial.println("[LED CONFIG] - ledConfigs array is NULL");
    } else {
      USBSerial.println("[LED CONFIG] - ledConfigs array is initialized");
    }
  }
  
  // Force an LED update
  USBSerial.println("[LED CONFIG] Forcing LED update by calling updateLEDs()");
  extern void updateLEDs();
  updateLEDs();
  
  USBSerial.println("[LED CONFIG] LED configuration update complete\n");
  
  request->send(200, "application/json", "{\"message\":\"LEDs config updated successfully\"}");
  accumulatedData = ""; // Reset for next request
}

void handleGetInfoConfig(AsyncWebServerRequest *request) {
  USBSerial.println("API: Requested /api/config/info");
  handleGetConfigFile(request, "/config/info.json");
}

void handleGetDisplayConfig(AsyncWebServerRequest *request) {
  USBSerial.println("API: Requested /api/config/display");
  handleGetConfigFile(request, "/config/display.json");
}

void handleGetExampleConfig(AsyncWebServerRequest *request) {
  USBSerial.println("API: Requested /api/config/example");
  handleGetConfigFile(request, "/config/example.json");
}

void handlePostActionsConfig(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
  static String accumulatedData = "";
  
  // Add CORS headers for when pre-flight check is done
  AsyncWebServerResponse *response = request->beginResponse(200);
  response->addHeader("Access-Control-Allow-Origin", "*");
  response->addHeader("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
  response->addHeader("Access-Control-Allow-Headers", "Content-Type");
  
  // Handle OPTIONS request for CORS preflight
  if (request->method() == HTTP_OPTIONS) {
    request->send(response);
    return;
  }
  
  // Accumulate the data
  if (index == 0) {
    accumulatedData = ""; // Reset for new request
    USBSerial.println("\n===== [ACTIONS CONFIG] Starting new actions config update =====");
  }
  accumulatedData += String((char*)data, len);
  USBSerial.printf("[ACTIONS CONFIG] Accumulated %d/%d bytes of data\n", index + len, total);
  
  // Only process the complete data in the last chunk
  if (index + len < total) {
    return; // Wait for more data
  }

  USBSerial.printf("[ACTIONS CONFIG] Received complete actions config update, %d bytes\n", accumulatedData.length());
  
  // Debugging: print memory status
  USBSerial.printf("[ACTIONS CONFIG] Free heap: %d bytes\n", ESP.getFreeHeap());
  
  // Ensure the config directory exists
  if (!LittleFS.exists("/config")) {
    USBSerial.println("[ACTIONS CONFIG] Config directory doesn't exist, creating it");
    if (!LittleFS.mkdir("/config")) {
      USBSerial.println("[ACTIONS CONFIG] ERROR: Failed to create config directory");
      request->send(500, "application/json", "{\"error\":\"Failed to create config directory\"}");
      accumulatedData = ""; // Reset for next request
      return;
    }
  }
  
  // Validate JSON data
  USBSerial.println("[ACTIONS CONFIG] Parsing JSON data");
  DynamicJsonDocument doc(JSON_DOCUMENT_SIZE);
  DeserializationError error = deserializeJson(doc, accumulatedData);
  if (error) {
    USBSerial.printf("[ACTIONS CONFIG] ERROR: Failed to parse actions JSON: %s\n", error.c_str());
    request->send(400, "application/json", "{\"error\":\"Invalid JSON format in request\"}");
    accumulatedData = ""; // Reset for next request
    return;
  }
  USBSerial.println("[ACTIONS CONFIG] JSON parsing successful");
  
  // Log some action configuration details
  USBSerial.println("[ACTIONS CONFIG] JSON structure overview:");
  if (doc.containsKey("layers") && doc["layers"].is<JsonObject>()) {
    USBSerial.println("[ACTIONS CONFIG] - Contains 'layers' key as object");
    int layerCount = 0;
    for (JsonPair layer : doc["layers"].as<JsonObject>()) {
      layerCount++;
    }
    USBSerial.printf("[ACTIONS CONFIG] - Found %d action layers\n", layerCount);
  }
  
  // Write to file
  USBSerial.println("[ACTIONS CONFIG] Opening file for writing: /config/actions.json");
  File file = LittleFS.open("/config/actions.json", "w");
  if (!file) {
    USBSerial.println("[ACTIONS CONFIG] ERROR: Failed to open actions config file for writing");
    request->send(500, "application/json", "{\"error\":\"Failed to open actions config for writing\"}");
    accumulatedData = ""; // Reset for next request
    return;
  }
  
  USBSerial.println("[ACTIONS CONFIG] Writing config to file");
  size_t bytesWritten = file.print(accumulatedData);
  file.close();
  
  if (bytesWritten != accumulatedData.length()) {
    USBSerial.printf("[ACTIONS CONFIG] ERROR: Incomplete write - wrote %d of %d bytes\n", 
                    bytesWritten, accumulatedData.length());
    request->send(500, "application/json", "{\"error\":\"Failed to write complete actions config\"}");
    accumulatedData = ""; // Reset for next request
    return;
  }
  
  USBSerial.printf("[ACTIONS CONFIG] Successfully wrote %d bytes to file\n", bytesWritten);
  
  // Verify the file was written correctly by reading it back
  USBSerial.println("[ACTIONS CONFIG] Verifying file was written correctly");
  file = LittleFS.open("/config/actions.json", "r");
  if (!file) {
    USBSerial.println("[ACTIONS CONFIG] ERROR: Could not open file for verification");
  } else {
    size_t fileSize = file.size();
    USBSerial.printf("[ACTIONS CONFIG] File size after write: %d bytes\n", fileSize);
    file.close();
  }
  
  // Add a short delay before reloading configuration
  USBSerial.println("[ACTIONS CONFIG] Waiting 1 second before applying actions configuration...");
  delay(1000); // 1 second delay to ensure file operations are complete
  
  // IMPORTANT: Reload and apply the new action configuration to in-memory state
  if (keyHandler) {
    USBSerial.println("[ACTIONS CONFIG] KeyHandler instance exists, proceeding with reload");
    
    // Load actions from the newly saved configuration file
    USBSerial.println("[ACTIONS CONFIG] Loading actions from configuration file");
    auto actions = ConfigManager::loadActions("/config/actions.json");
    if (!actions.empty()) {
      USBSerial.printf("[ACTIONS CONFIG] Successfully loaded %d actions\n", actions.size());
      
      // Update key configuration with new actions
      USBSerial.println("[ACTIONS CONFIG] Calling keyHandler->loadKeyConfiguration()");
      keyHandler->loadKeyConfiguration(actions);
      
      // Apply current layer to update the action map
      String currentLayer = keyHandler->getCurrentLayer();
      USBSerial.printf("[ACTIONS CONFIG] Current layer: %s\n", currentLayer.c_str());
      USBSerial.println("[ACTIONS CONFIG] Calling keyHandler->applyLayerToActionMap()");
      keyHandler->applyLayerToActionMap(currentLayer);
      
      USBSerial.printf("[ACTIONS CONFIG] Successfully applied actions to in-memory state (layer: %s)\n", 
                     currentLayer.c_str());
    } else {
      USBSerial.println("[ACTIONS CONFIG] ERROR: Failed to load actions - returned empty collection");
      
      // Try to diagnose why it failed
      USBSerial.println("[ACTIONS CONFIG] Diagnosing failure:");
      file = LittleFS.open("/config/actions.json", "r");
      if (!file) {
        USBSerial.println("[ACTIONS CONFIG] - File doesn't exist after writing!");
      } else {
        USBSerial.printf("[ACTIONS CONFIG] - File exists, size: %d bytes\n", file.size());
        
        // Try to read a sample
        if (file.size() > 0) {
          String sample = file.readString().substring(0, 100);
          USBSerial.println("[ACTIONS CONFIG] - File content (first 100 chars): " + sample);
        }
        file.close();
      }
    }
  } else {
    USBSerial.println("[ACTIONS CONFIG] ERROR: KeyHandler is NULL, cannot apply action configuration");
  }
  
  USBSerial.println("[ACTIONS CONFIG] Actions configuration update complete\n");
  
  request->send(200, "application/json", "{\"message\":\"Actions config updated successfully\"}");
  accumulatedData = ""; // Reset for next request
}

void handleGetWiFiConfig(AsyncWebServerRequest *request) {
  USBSerial.println("API: Requested /api/config/wifi");
  
  // Create JSON response with WiFi configuration
  DynamicJsonDocument doc(1024);
  doc["wifi"]["connected"] = WiFi.status() == WL_CONNECTED;
  doc["wifi"]["ssid"] = WiFi.SSID();
  doc["wifi"]["ap_mode"] = WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA;
  doc["wifi"]["ip"] = WiFi.localIP().toString();
  
  if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA) {
    doc["wifi"]["ap_ssid"] = WiFi.softAPSSID();
    doc["wifi"]["ap_ip"] = WiFi.softAPIP().toString();
  }
  
  String output;
  serializeJson(doc, output);
  request->send(200, "application/json", output);
}

void handleWiFiScan(AsyncWebServerRequest *request) {
  USBSerial.println("API: Requested /api/wifi/scan");
  
  // Scan for networks
  int n = WiFi.scanNetworks();
  
  DynamicJsonDocument doc(4096);
  JsonArray networks = doc.createNestedArray("networks");
  
  for (int i = 0; i < n; i++) {
    JsonObject network = networks.createNestedObject();
    network["ssid"] = WiFi.SSID(i);
    network["rssi"] = WiFi.RSSI(i);
    network["encryption"] = WiFi.encryptionType(i);
    network["channel"] = WiFi.channel(i);
  }
  
  String output;
  serializeJson(doc, output);
  request->send(200, "application/json", output);
}

void handleGetStatus(AsyncWebServerRequest *request) {
  USBSerial.println("API: Requested /api/status");
  
  DynamicJsonDocument doc(1024);
  doc["status"] = "ok";
  doc["version"] = VersionManager::getVersionString();
  doc["uptime"] = millis() / 1000;
  doc["memory"]["free"] = ESP.getFreeHeap();
  doc["memory"]["total"] = ESP.getHeapSize();
  
  // Add WiFi information
  doc["wifi"]["connected"] = WiFi.status() == WL_CONNECTED;
  if (WiFi.status() == WL_CONNECTED) {
    doc["wifi"]["ssid"] = WiFi.SSID();
    doc["wifi"]["rssi"] = WiFi.RSSI();
  }
  
  String output;
  serializeJson(doc, output);
  request->send(200, "application/json", output);
}

// OTA Update API handlers
void handleCheckForUpdates(AsyncWebServerRequest *request) {
  USBSerial.println("API: Requested /api/firmware/check");
  
  // Start the update check process
  bool checkStarted = OTAUpdateManager::checkForUpdates();
  
  // Create response JSON
  String response;
  if (checkStarted) {
    StaticJsonDocument<512> doc;
    doc["status"] = "checking";
    doc["message"] = "Checking for updates...";
    serializeJson(doc, response);
  } else {
    StaticJsonDocument<512> doc;
    doc["status"] = "error";
    doc["message"] = "Failed to start update check: " + OTAUpdateManager::getLastError();
    serializeJson(doc, response);
  }
  
  // Send response
  request->send(200, "application/json", response);
}

void handleGetUpdateStatus(AsyncWebServerRequest *request) {
  USBSerial.println("API: Requested /api/firmware/status");
  
  // Create response JSON
  StaticJsonDocument<1024> doc;
  doc["status"] = OTAUpdateManager::getUpdateStatus();
  doc["state"] = static_cast<int>(OTAUpdateManager::getUpdateState());
  doc["progress"] = OTAUpdateManager::getUpdateProgress();
  doc["updateAvailable"] = OTAUpdateManager::isUpdateAvailable();
  
  if (OTAUpdateManager::isUpdateAvailable()) {
    doc["availableVersion"] = OTAUpdateManager::getAvailableVersion();
    doc["releaseNotes"] = OTAUpdateManager::getReleaseNotes();
    doc["currentVersion"] = VersionManager::getVersionString();
  }
  
  if (OTAUpdateManager::getLastError().length() > 0) {
    doc["error"] = OTAUpdateManager::getLastError();
  }
  
  String response;
  serializeJson(doc, response);
  
  // Send response
  request->send(200, "application/json", response);
}

void handlePerformUpdate(AsyncWebServerRequest *request) {
  USBSerial.println("API: Requested /api/firmware/update");
  
  // Check if update is available
  if (!OTAUpdateManager::isUpdateAvailable()) {
    request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"No update available\"}");
    return;
  }
  
  // Get the firmware URL
  String firmwareUrl = OTAUpdateManager::getFirmwareUrl();
  if (firmwareUrl.isEmpty()) {
    request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"No firmware URL available\"}");
    return;
  }
  
  // Start the update process asynchronously (the actual update will happen after this response)
  request->send(200, "application/json", "{\"status\":\"updating\",\"message\":\"Starting update process\"}");
  
  // Schedule the update to be performed after sending the response
  // This is done because the update process will restart the device
  static String pendingUpdateUrl = firmwareUrl;
  USBSerial.println("Scheduling firmware update to: " + pendingUpdateUrl);
  
  // Use a task to perform the update after the response is sent
  xTaskCreate([](void* parameter) {
    String* updateUrl = (String*)parameter;
    
    // Short delay to ensure the HTTP response is sent
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // Show update progress on display if available
    UpdateProgressDisplay::drawProgressScreen("Firmware Update", 0, "Starting update...");
    
    // Set up a progress callback for display updates
    auto progressCallback = [](size_t current, size_t total, int percentage) {
      UpdateProgressDisplay::updateProgress(current, total, percentage);
    };
    
    // Perform the update
    OTAUpdateManager::performUpdate(*updateUrl, progressCallback);
    
    // This task should delete itself when done
    vTaskDelete(NULL);
  }, "update_task", 8192, &pendingUpdateUrl, 1, NULL);
}

void setupConfigRoutes(AsyncWebServer *server) {
  // Log when this function is called
  USBSerial.println("INFO: Setting up API config routes");
  
  // IMPORTANT: These routes should not be duplicated in WiFiManager.cpp
  // After merging branches, make sure no duplicate routes exist!
  
  // GET handlers for all config files
  server->on("/api/config/reports", HTTP_GET, handleGetReportsConfig);
  server->on("/api/config/actions", HTTP_GET, handleGetActionsConfig);
  server->on("/api/config/components", HTTP_GET, handleGetComponentsConfig);
  server->on("/api/config/leds", HTTP_GET, handleGetLedsConfig);
  server->on("/api/config/info", HTTP_GET, handleGetInfoConfig);
  server->on("/api/config/display", HTTP_GET, handleGetDisplayConfig);
  server->on("/api/config/example", HTTP_GET, handleGetExampleConfig);
  server->on("/api/config/wifi", HTTP_GET, handleGetWiFiConfig);
  server->on("/api/wifi/scan", HTTP_GET, handleWiFiScan);
  server->on("/api/status", HTTP_GET, handleGetStatus);
  
  // Print route registrations
  USBSerial.println("  - Registered /api/config/reports (GET)");
  USBSerial.println("  - Registered /api/config/actions (GET)");
  USBSerial.println("  - Registered /api/config/components (GET)");
  USBSerial.println("  - Registered /api/config/leds (GET)");
  USBSerial.println("  - Registered /api/config/info (GET)");
  USBSerial.println("  - Registered /api/config/display (GET)");
  USBSerial.println("  - Registered /api/config/example (GET)");
  USBSerial.println("  - Registered /api/config/wifi (GET)");
  USBSerial.println("  - Registered /api/wifi/scan (GET)");
  USBSerial.println("  - Registered /api/status (GET)");
  
  // POST handlers
  server->on(
    "/api/config/actions", 
    HTTP_POST, 
    [](AsyncWebServerRequest *request) {
      // Empty handler for the request completion
    }, 
    NULL,  // Upload handler is null as we handle everything in the body handler
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      handlePostActionsConfig(request, data, len, index, total);
    }
  );
  USBSerial.println("  - Registered /api/config/actions (POST)");
  
  // Add POST handler for LEDs config
  server->on(
    "/api/config/leds", 
    HTTP_POST, 
    [](AsyncWebServerRequest *request) {
      // Empty handler for the request completion
    }, 
    NULL,  // Upload handler is null as we handle everything in the body handler
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      handlePostLedsConfig(request, data, len, index, total);
    }
  );
  USBSerial.println("  - Registered /api/config/leds (POST)");

  // CORS preflight handlers
  server->on("/api/config/reports", HTTP_OPTIONS, [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse(200);
    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
    response->addHeader("Access-Control-Allow-Headers", "Content-Type");
    request->send(response);
  });
  
  server->on("/api/config/actions", HTTP_OPTIONS, [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse(200);
    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
    response->addHeader("Access-Control-Allow-Headers", "Content-Type");
    request->send(response);
  });
  
  server->on("/api/config/components", HTTP_OPTIONS, [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse(200);
    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
    response->addHeader("Access-Control-Allow-Headers", "Content-Type");
    request->send(response);
  });
  
  server->on("/api/config/leds", HTTP_OPTIONS, [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse(200);
    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
    response->addHeader("Access-Control-Allow-Headers", "Content-Type");
    request->send(response);
    USBSerial.println("  - Handled OPTIONS preflight for /api/config/leds");
  });
  
  server->on("/api/config/info", HTTP_OPTIONS, [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse(200);
    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
    response->addHeader("Access-Control-Allow-Headers", "Content-Type");
    request->send(response);
  });
  
  server->on("/api/config/display", HTTP_OPTIONS, [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse(200);
    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
    response->addHeader("Access-Control-Allow-Headers", "Content-Type");
    request->send(response);
  });
  
  server->on("/api/config/example", HTTP_OPTIONS, [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse(200);
    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
    response->addHeader("Access-Control-Allow-Headers", "Content-Type");
    request->send(response);
  });
  
  server->on("/api/config/wifi", HTTP_OPTIONS, [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse(200);
    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
    response->addHeader("Access-Control-Allow-Headers", "Content-Type");
    request->send(response);
  });
  
  server->on("/api/wifi/scan", HTTP_OPTIONS, [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse(200);
    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
    response->addHeader("Access-Control-Allow-Headers", "Content-Type");
    request->send(response);
  });
  
  server->on("/api/status", HTTP_OPTIONS, [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse(200);
    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
    response->addHeader("Access-Control-Allow-Headers", "Content-Type");
    request->send(response);
  });
  
  USBSerial.println("  - Registered OPTIONS handlers for CORS preflight");
  USBSerial.println("Config routes successfully registered");

  // Add a debug endpoint that returns the status of all registered routes
  server->on("/api/debug/routes", HTTP_GET, [server](AsyncWebServerRequest *request) {
    USBSerial.println("API: Requested /api/debug/routes");
    
    // Create a JSON response with information about all API routes
    DynamicJsonDocument doc(JSON_DOCUMENT_SIZE);
    doc["status"] = "ok";
    doc["message"] = "API route info";
    
    JsonObject routes = doc.createNestedObject("routes");
    routes["config"] = true;
    routes["config_routes_registered"] = true;
    
    // Add server uptime and memory info
    JsonObject system = doc.createNestedObject("system");
    system["uptime_ms"] = millis();
    system["free_heap"] = ESP.getFreeHeap();
    system["config_dir_exists"] = LittleFS.exists("/config");
    system["data_config_dir_exists"] = LittleFS.exists("/data/config");
    
    // Add path existence info
    JsonObject paths = doc.createNestedObject("paths");
    String configFiles[] = {"actions.json", "reports.json", "components.json", "leds.json", "info.json", "display.json"};
    for (const String& file : configFiles) {
      String path = "/config/" + file;
      String dataPath = "/data/config/" + file;
      String defaultPath = "/config/defaults/" + file;
      
      JsonObject fileObj = paths.createNestedObject(file);
      fileObj["config"] = LittleFS.exists(path);
      fileObj["data_config"] = LittleFS.exists(dataPath);
      fileObj["defaults"] = LittleFS.exists(defaultPath);
      
      // If the file exists, add its size
      if (LittleFS.exists(path)) {
        File f = LittleFS.open(path, "r");
        if (f) {
          fileObj["size"] = f.size();
          f.close();
        }
      }
    }
    
    // Serialize the JSON document
    String response;
    serializeJson(doc, response);
    
    request->send(200, "application/json", response);
  });
  USBSerial.println("  - Registered /api/debug/routes (GET)");
  
  // Add debug handlers for raw data saving (no validation)
  server->on(
    "/api/debug/raw/leds", 
    HTTP_POST, 
    [](AsyncWebServerRequest *request) {
      // Empty handler for request completion
    },
    NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      USBSerial.printf("Raw LED data received: %d bytes\n", len);
      
      // Create file without validation
      File file = LittleFS.open("/config/leds.json", "w");
      if (!file) {
        request->send(500, "application/json", "{\"error\":\"Failed to open file\"}");
        return;
      }
      
      size_t written = file.write(data, len);
      file.close();
      
      if (written != len) {
        request->send(500, "application/json", "{\"error\":\"Failed to write data\"}");
        return;
      }
      
      request->send(200, "application/json", "{\"success\":true}");
    }
  );
  
  server->on(
    "/api/debug/raw/actions", 
    HTTP_POST, 
    [](AsyncWebServerRequest *request) {
      // Empty handler for request completion
    },
    NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      USBSerial.printf("Raw actions data received: %d bytes\n", len);
      
      // Create file without validation
      File file = LittleFS.open("/config/actions.json", "w");
      if (!file) {
        request->send(500, "application/json", "{\"error\":\"Failed to open file\"}");
        return;
      }
      
      size_t written = file.write(data, len);
      file.close();
      
      if (written != len) {
        request->send(500, "application/json", "{\"error\":\"Failed to write data\"}");
        return;
      }
      
      request->send(200, "application/json", "{\"success\":true}");
    }
  );
  
  // CORS handlers for debug endpoints
  server->on("/api/debug/raw/leds", HTTP_OPTIONS, [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse(200);
    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
    response->addHeader("Access-Control-Allow-Headers", "Content-Type");
    request->send(response);
  });
  
  server->on("/api/debug/raw/actions", HTTP_OPTIONS, [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse(200);
    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
    response->addHeader("Access-Control-Allow-Headers", "Content-Type");
    request->send(response);
  });
  
  USBSerial.println("  - Registered debug raw data handlers");

  // Register OTA update endpoints
  server->on("/api/firmware/check", HTTP_GET, handleCheckForUpdates);
  server->on("/api/firmware/status", HTTP_GET, handleGetUpdateStatus);
  server->on("/api/firmware/update", HTTP_GET, handlePerformUpdate);
  
  USBSerial.println("  - Registered /api/firmware/check (GET)");
  USBSerial.println("  - Registered /api/firmware/status (GET)");
  USBSerial.println("  - Registered /api/firmware/update (GET)");
  
  // CORS preflight handlers for OTA endpoints
  server->on("/api/firmware/check", HTTP_OPTIONS, [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse(200);
    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
    response->addHeader("Access-Control-Allow-Headers", "Content-Type");
    request->send(response);
  });
  
  server->on("/api/firmware/status", HTTP_OPTIONS, [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse(200);
    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
    response->addHeader("Access-Control-Allow-Headers", "Content-Type");
    request->send(response);
  });
  
  server->on("/api/firmware/update", HTTP_OPTIONS, [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse(200);
    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
    response->addHeader("Access-Control-Allow-Headers", "Content-Type");
    request->send(response);
  });
  
  USBSerial.println("  - Registered OTA update endpoints");
} 