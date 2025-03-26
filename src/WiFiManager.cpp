#include "WiFiManager.h"
#include "LEDHandler.h"
#include "KeyHandler.h"
#include "DisplayHandler.h"
#include "MacroHandler.h"
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <FS.h>
#include <SPIFFS.h>
#include <ESPmDNS.h>
#include "config.h"

// Forward declarations
String serializedJson(const String& jsonStr);
String lookupKeyName(const uint8_t* report, size_t reportSize, bool isConsumer = false);

extern USBCDC USBSerial;
extern KeyHandler* keyHandler;
extern MacroHandler* macroHandler;

// Static member initialization
String WiFiManager::_ssid = "MacroPad";
String WiFiManager::_password = "macropad123";
bool WiFiManager::_apMode = true;
AsyncWebServer WiFiManager::_server(80);
AsyncWebSocket WiFiManager::_ws("/ws");
bool WiFiManager::_isConnected = false;
uint32_t WiFiManager::_lastStatusBroadcast = 0;
uint32_t WiFiManager::_connectAttemptStart = 0;

// Constants
const uint32_t WiFiManager::STATUS_BROADCAST_INTERVAL = 5000; // Increased from original value
const uint32_t WiFiManager::CONNECT_TIMEOUT = 30000; // 30 seconds

void WiFiManager::begin() {
    // Load WiFi config from SPIFFS
    loadWiFiConfig();
    
    // Setup WiFi
    setupWiFi();
    
    // Setup WebSocket
    setupWebSocket();
    
    // Setup WebServer
    setupWebServer();
    
    USBSerial.println("WiFi Manager initialized");
}

void WiFiManager::setupWiFi() {
    if (_apMode) {
        // Setup AP mode
        USBSerial.println("Setting up WiFi in AP mode");
        WiFi.mode(WIFI_AP);
        WiFi.softAP(_ssid.c_str(), _password.c_str());
        
        IPAddress IP = WiFi.softAPIP();
        USBSerial.print("AP IP address: ");
        USBSerial.println(IP.toString());
        _isConnected = true;
        
        // Show on display
        showTemporaryMessage(("WiFi AP Ready\nSSID: " + _ssid + "\nIP: " + IP.toString()).c_str(), 5000);
    } else {
        // Setup STA mode
        USBSerial.printf("Connecting to WiFi: %s\n", _ssid.c_str());
        showTemporaryMessage(("Connecting to WiFi: " + _ssid).c_str(), 3000);
        
        WiFi.mode(WIFI_STA);
        WiFi.begin(_ssid.c_str(), _password.c_str());
        
        _connectAttemptStart = millis();
        
        // Initial connection attempt
        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 20) {
            delay(500);
            USBSerial.print(".");
            attempts++;
        }
        
        if (WiFi.status() == WL_CONNECTED) {
            _isConnected = true;
            USBSerial.println("");
            USBSerial.print("Connected to ");
            USBSerial.println(_ssid);
            USBSerial.print("IP address: ");
            USBSerial.println(WiFi.localIP());
            
            // Show on display
            showTemporaryMessage(("WiFi Connected\nIP: " + WiFi.localIP().toString()).c_str(), 5000);
            
            // Broadcast connection status
            broadcastStatus();
        } else {
            USBSerial.println("Failed to connect in the initial attempt. Will retry in the background.");
            _isConnected = false;
            
            // Show on display
            showTemporaryMessage("WiFi connection failed.\nRetrying in background...", 3000);
        }
    }
}

void WiFiManager::setupWebSocket() {
    // Attach WebSocket event handler
    _ws.onEvent(onWsEvent);
    _server.addHandler(&_ws);
    
    USBSerial.println("WebSocket server initialized");
}

void WiFiManager::setupWebServer() {
    // Serve static files from SPIFFS
    _server.serveStatic("/", SPIFFS, "/web/").setDefaultFile("index.html");
    
    // Serve CSS file
    _server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/web/style.css", "text/css");
    });
    
    // Serve assets files
    _server.serveStatic("/assets/", SPIFFS, "/web/assets/");
    
    // API endpoints
    
    // LED Configuration
    _server.on("/api/config/led", HTTP_GET, [](AsyncWebServerRequest *request) {
        String config = getLEDConfigJson();
        request->send(200, "application/json", config);
    });
    
    _server.on("/api/config/led", HTTP_POST, 
        [](AsyncWebServerRequest *request) {
            request->send(200, "text/plain", "Configuration received");
        },
        NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            String json = String((char*)data, len);
            bool success = updateLEDConfigFromJson(json);
            if (success) {
                saveLEDConfig();
            }
        }
    );
    
    // WiFi Configuration
    _server.on("/api/config/wifi", HTTP_GET, [](AsyncWebServerRequest *request) {
        DynamicJsonDocument doc(1024);
        doc["ssid"] = WiFiManager::_ssid;
        doc["password"] = ""; // Don't send the password for security
        doc["ap_mode"] = WiFiManager::_apMode;
        
        String output;
        serializeJson(doc, output);
        request->send(200, "application/json", output);
    });
    
    _server.on("/api/config/wifi", HTTP_POST, 
        [](AsyncWebServerRequest *request) {
            request->send(200, "text/plain", "WiFi configuration received, restarting...");
        },
        NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            String json = String((char*)data, len);
            DynamicJsonDocument doc(1024);
            DeserializationError error = deserializeJson(doc, json);
            
            if (!error) {
                if (doc.containsKey("ssid")) {
                    WiFiManager::_ssid = doc["ssid"].as<String>();
                }
                
                if (doc.containsKey("password") && !doc["password"].as<String>().isEmpty()) {
                    WiFiManager::_password = doc["password"].as<String>();
                }
                
                if (doc.containsKey("ap_mode")) {
                    WiFiManager::_apMode = doc["ap_mode"].as<bool>();
                }
                
                WiFiManager::saveWiFiConfig();
                
                // Schedule restart after response sent
                delay(1000);
                ESP.restart();
            }
        }
    );
    
    // System Status
    _server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request) {
        DynamicJsonDocument doc(1024);
        doc["wifi"]["connected"] = WiFiManager::isConnected();
        doc["wifi"]["ip"] = WiFiManager::getLocalIP().toString();
        doc["wifi"]["ssid"] = WiFiManager::_ssid;
        doc["wifi"]["ap_mode"] = WiFiManager::_apMode;
        
        // Add more status info here as needed
        
        String output;
        serializeJson(doc, output);
        request->send(200, "application/json", output);
    });
    
    // Reset to defaults
    _server.on("/api/reset", HTTP_POST, [](AsyncWebServerRequest *request) {
        resetToDefaults();
        request->send(200, "text/plain", "Reset to defaults complete. Restarting...");
        
        // Restart after sending response
        delay(1000);
        ESP.restart();
    });
    
    // Config File API Endpoints
    
    // Get components.json config file
    _server.on("/api/config/components", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/config/components.json", "application/json");
    });
    
    // Get actions.json config file
    _server.on("/api/config/actions", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/config/actions.json", "application/json");
    });
    
    // Update actions.json config file
    _server.on("/api/config/actions", HTTP_POST, 
        [](AsyncWebServerRequest *request) {},
        NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
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

            if (!SPIFFS.exists("/data/config/actions.json")) {
                request->send(404, "application/json", "{\"error\":\"Actions config not found\"}");
                return;
            }

            // Read current actions config
            File file = SPIFFS.open("/data/config/actions.json", "r");
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
            file = SPIFFS.open("/data/config/actions.json", "w");
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
    );

    // Add CORS preflight handler for actions endpoint
    _server.on("/api/config/actions", HTTP_OPTIONS, [](AsyncWebServerRequest *request) {
        AsyncWebServerResponse *response = request->beginResponse(200);
        response->addHeader("Access-Control-Allow-Origin", "*");
        response->addHeader("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
        response->addHeader("Access-Control-Allow-Headers", "Content-Type");
        request->send(response);
    });
    
    // Get info.json config file
    _server.on("/api/config/info", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/config/info.json", "application/json");
    });
    
    // Get reports.json config file
    _server.on("/api/config/reports", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/config/reports.json", "application/json");
    });
    
    // Get LEDs.json config file
    _server.on("/api/config/LEDs", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/config/LEDs.json", "application/json");
    });
    
    // Macro API Endpoints
    
    // Get list of all macros
    _server.on("/api/macros", HTTP_GET, [](AsyncWebServerRequest *request) {
        DynamicJsonDocument doc(4096);
        JsonArray macroArray = doc.createNestedArray("macros");
        
        if (macroHandler) {
            std::vector<String> macroIds = macroHandler->getAvailableMacros();
            
            for (const String& macroId : macroIds) {
                Macro macro;
                if (macroHandler->getMacro(macroId, macro)) {
                    JsonObject macroObj = macroArray.createNestedObject();
                    macroObj["id"] = macro.id;
                    macroObj["name"] = macro.name;
                    macroObj["description"] = macro.description;
                    // Don't include commands for the listing
                }
            }
        }
        
        String output;
        serializeJson(doc, output);
        request->send(200, "application/json", output);
    });
    
    // Get a specific macro by ID
    _server.on("/api/macros/^([a-zA-Z0-9_-]+)$", HTTP_GET, [](AsyncWebServerRequest *request) {
        String macroId = request->pathArg(0);
        
        if (macroHandler) {
            Macro macro;
            if (macroHandler->getMacro(macroId, macro)) {
                DynamicJsonDocument doc(8192);
                JsonObject macroObj = doc.to<JsonObject>();
                
                macroObj["id"] = macro.id;
                macroObj["name"] = macro.name;
                macroObj["description"] = macro.description;
                
                JsonArray cmdsArray = macroObj.createNestedArray("commands");
                for (const MacroCommand& cmd : macro.commands) {
                    JsonObject cmdObj = cmdsArray.createNestedObject();
                    
                    // Format command based on type
                    JsonArray reportArray;
                    
                    switch (cmd.type) {
                        case MACRO_CMD_KEY_PRESS:
                            cmdObj["type"] = "key_press";
                            reportArray = cmdObj.createNestedArray("report");
                            for (int i = 0; i < 8; i++) {
                                reportArray.add(cmd.data.keyPress.report[i]);
                            }
                            break;
                        
                        case MACRO_CMD_KEY_DOWN:
                            cmdObj["type"] = "key_down";
                            reportArray = cmdObj.createNestedArray("report");
                            for (int i = 0; i < 8; i++) {
                                reportArray.add(cmd.data.keyPress.report[i]);
                            }
                            break;
                            
                        case MACRO_CMD_KEY_UP:
                            cmdObj["type"] = "key_up";
                            reportArray = cmdObj.createNestedArray("report");
                            for (int i = 0; i < 8; i++) {
                                reportArray.add(cmd.data.keyPress.report[i]);
                            }
                            break;
                            
                        case MACRO_CMD_TYPE_TEXT:
                            cmdObj["type"] = "type_text";
                            cmdObj["text"] = cmd.data.typeText.text;
                            break;
                            
                        case MACRO_CMD_DELAY:
                            cmdObj["type"] = "delay";
                            cmdObj["milliseconds"] = cmd.data.delay.milliseconds;
                            break;
                            
                        case MACRO_CMD_CONSUMER_PRESS:
                            cmdObj["type"] = "consumer_press";
                            reportArray = cmdObj.createNestedArray("report");
                            for (int i = 0; i < 4; i++) {
                                reportArray.add(cmd.data.consumerPress.report[i]);
                            }
                            break;
                            
                        case MACRO_CMD_EXECUTE_MACRO:
                            cmdObj["type"] = "execute_macro";
                            cmdObj["macroId"] = cmd.data.executeMacro.macroId;
                            break;
                    }
                }
                
                String output;
                serializeJson(doc, output);
                request->send(200, "application/json", output);
                return;
            }
        }
        
        request->send(404, "application/json", "{\"error\":\"Macro not found\"}");
    });
    
    // Create or update a macro
    _server.on("/api/macros", HTTP_POST, 
        [](AsyncWebServerRequest *request) {
            request->send(200, "application/json", "{\"status\":\"processing\"}");
        },
        NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            DynamicJsonDocument doc(8192);
            DeserializationError error = deserializeJson(doc, (const char*)data, len);
            
            if (error) {
                request->send(400, "application/json", 
                             "{\"error\":\"JSON parsing failed: " + String(error.c_str()) + "\"}");
                return;
            }
            
            if (!macroHandler) {
                request->send(500, "application/json", "{\"error\":\"MacroHandler not initialized\"}");
                return;
            }
            
            // Parse the macro from JSON
            Macro macro;
            if (macroHandler->parseMacroFromJson(doc.as<JsonObject>(), macro)) {
                if (macroHandler->saveMacro(macro)) {
                    request->send(200, "application/json", "{\"status\":\"ok\",\"id\":\"" + macro.id + "\"}");
                } else {
                    request->send(500, "application/json", "{\"error\":\"Failed to save macro\"}");
                }
            } else {
                request->send(400, "application/json", "{\"error\":\"Invalid macro format\"}");
            }
        }
    );
    
    // Delete a macro
    _server.on("/api/macros/^([a-zA-Z0-9_-]+)$", HTTP_DELETE, [](AsyncWebServerRequest *request) {
        String macroId = request->pathArg(0);
        
        if (macroHandler) {
            if (macroHandler->deleteMacro(macroId)) {
                request->send(200, "application/json", "{\"status\":\"ok\"}");
                return;
            }
        }
        
        request->send(404, "application/json", "{\"error\":\"Macro not found\"}");
    });
    
    // Layer API Endpoints
    
    // Get current layer and available layers
    _server.on("/api/layers", HTTP_GET, [](AsyncWebServerRequest *request) {
        DynamicJsonDocument doc(4096);
        
        if (keyHandler) {
            doc["current_layer"] = keyHandler->getCurrentLayer();
            
            JsonArray layersArray = doc.createNestedArray("available_layers");
            std::vector<String> layers = keyHandler->getAvailableLayers();
            for (const String& layer : layers) {
                layersArray.add(layer);
            }
        } else {
            doc["current_layer"] = "default";
            JsonArray layersArray = doc.createNestedArray("available_layers");
            layersArray.add("default");
        }
        
        String output;
        serializeJson(doc, output);
        request->send(200, "application/json", output);
    });
    
    // Switch to a different layer
    _server.on("/api/layers/switch", HTTP_POST,
        [](AsyncWebServerRequest *request) {
            request->send(200, "application/json", "{\"status\":\"processing\"}");
        },
        NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            DynamicJsonDocument doc(1024);
            DeserializationError error = deserializeJson(doc, (const char*)data, len);
            
            if (error) {
                request->send(400, "application/json", 
                             "{\"error\":\"JSON parsing failed: " + String(error.c_str()) + "\"}");
                return;
            }
            
            if (!doc.containsKey("layer")) {
                request->send(400, "application/json", "{\"error\":\"Missing layer parameter\"}");
                return;
            }
            
            String targetLayer = doc["layer"].as<String>();
            
            if (keyHandler) {
                if (keyHandler->switchToLayer(targetLayer)) {
                    request->send(200, "application/json", 
                                 "{\"status\":\"ok\",\"layer\":\"" + targetLayer + "\"}");
                } else {
                    request->send(404, "application/json", 
                                 "{\"error\":\"Layer not found: " + targetLayer + "\"}");
                }
            } else {
                request->send(500, "application/json", "{\"error\":\"KeyHandler not initialized\"}");
            }
        }
    );
    
    // 404 handler
    _server.onNotFound([](AsyncWebServerRequest *request) {
        request->send(404, "text/plain", "Not found");
    });
    
    // Start server
    _server.begin();
    
    USBSerial.println("Web server started");
}

void WiFiManager::onWsEvent(AsyncWebSocket* server, AsyncWebSocketClient* client, 
                        AwsEventType type, void* arg, uint8_t* data, size_t len) {
    // Handle WebSocket events
    if (type == WS_EVT_CONNECT) {
        // Client connected
        USBSerial.printf("WebSocket client #%u connected from %s\n", 
                      client->id(), client->remoteIP().toString().c_str());
        
        // Send initial state
        DynamicJsonDocument doc(4096);
        doc["type"] = "init";
        doc["data"]["led_config"] = getLEDConfigJson();
        doc["data"]["wifi"]["connected"] = isConnected();
        doc["data"]["wifi"]["ip"] = getLocalIP().toString();
        doc["data"]["wifi"]["ssid"] = _ssid;
        doc["data"]["wifi"]["ap_mode"] = _apMode;
        
        String message;
        serializeJson(doc, message);
        client->text(message);
    } else if (type == WS_EVT_DISCONNECT) {
        // Client disconnected
        USBSerial.printf("WebSocket client #%u disconnected\n", client->id());
    } else if (type == WS_EVT_DATA) {
        // Data received
        AwsFrameInfo* info = (AwsFrameInfo*)arg;
        if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
            // Complete text message received
            data[len] = 0; // Null terminator
            String message = String((char*)data);
            
            // Process command
            DynamicJsonDocument doc(4096);
            DeserializationError error = deserializeJson(doc, message);
            
            if (!error) {
                String command = doc["command"].as<String>();
                
                if (command == "update_led") {
                    // Update LED
                    uint8_t index = doc["index"];
                    uint8_t r = doc["r"];
                    uint8_t g = doc["g"];
                    uint8_t b = doc["b"];
                    
                    setLEDColor(index, r, g, b);
                    
                    // Send confirmation
                    client->text("{\"status\":\"ok\",\"command\":\"update_led\"}");
                } else if (command == "save_config") {
                    // Save current configuration
                    bool success = saveLEDConfig();
                    
                    // Send confirmation
                    client->text("{\"status\":\"" + String(success ? "ok" : "error") + 
                              "\",\"command\":\"save_config\"}");
                } else if (command == "assign_macro") {
                    // Handle macro assignment
                    String macroId = doc["macroId"].as<String>();
                    String buttonId = doc["buttonId"].as<String>();
                    
                    if (keyHandler) {
                        // Update the button's key binding
                        bool success = keyHandler->assignMacroToButton(buttonId, macroId);
                        
                        // Send confirmation
                        client->text("{\"status\":\"" + String(success ? "ok" : "error") + 
                                  "\",\"command\":\"assign_macro\",\"buttonId\":\"" + buttonId + 
                                  "\",\"macroId\":\"" + macroId + "\"}");
                    } else {
                        client->text("{\"status\":\"error\",\"command\":\"assign_macro\",\"error\":\"KeyHandler not initialized\"}");
                    }
                } else if (command == "get_all_configs") {
                    // Send all configurations
                    // Use a smaller document size and more efficient JSON handling
                    DynamicJsonDocument configDoc(8192); // Reduced from 16384
                    
                    // Get module info
                    JsonObject moduleObj = configDoc.createNestedObject("module");
                    moduleObj["type"] = "main";
                    moduleObj["size"] = "4x4";
                    moduleObj["components"] = 16;
                    
                    // Get LED config - use serialized string instead of nesting another large JSON
                    configDoc["led_config"] = serializedJson(getLEDConfigJson());
                    
                    // Get current layer
                    if (keyHandler) {
                        configDoc["current_layer"] = keyHandler->getCurrentLayer();
                        
                        // Get key bindings from the current layer - stream directly to avoid memory issues
                        JsonArray bindingsArray = configDoc.createNestedArray("key_bindings");
                        
                        // Debug log to diagnose the issue
                        USBSerial.printf("Total keys: %d\n", keyHandler->getTotalKeys());
                        
                        // Log component positions for debugging
                        for (size_t i = 0; i < keyHandler->getTotalKeys(); i++) {
                            const KeyConfig& config = keyHandler->getKeyConfig(i);
                            USBSerial.printf("Component %d: type=%d, macroId=%s\n", 
                                         i, (int)config.type, config.macroId.c_str());
                            
                            // Create binding object for all components
                            JsonObject bindingObj = bindingsArray.createNestedObject();
                            bindingObj["component_id"] = String(i);
                            
                            // Add binding information based on type
                            switch (config.type) {
                                case ACTION_MACRO:
                                    bindingObj["type"] = "macro";
                                    bindingObj["macro_id"] = config.macroId;
                                    bindingObj["display_name"] = config.macroId.isEmpty() ? "None" : config.macroId;
                                    break;
                                    
                                case ACTION_HID:
                                    bindingObj["type"] = "hid";
                                    bindingObj["display_name"] = lookupKeyName(config.hidReport, 8, false);
                                    break;
                                    
                                case ACTION_MULTIMEDIA:
                                    bindingObj["type"] = "multimedia";
                                    bindingObj["display_name"] = lookupKeyName(config.consumerReport, 4, true);
                                    break;
                                    
                                case ACTION_LAYER:
                                    bindingObj["type"] = "layer";
                                    bindingObj["target_layer"] = config.targetLayer;
                                    bindingObj["display_name"] = "Layer: " + config.targetLayer;
                                    break;
                                    
                                default:
                                    bindingObj["type"] = "none";
                                    bindingObj["display_name"] = "None";
                                    break;
                            }
                            
                            // If this is an encoder component, add special properties
                            // Get the component ID from the keyHandler if available
                            String componentId = ""; // We would need to add mapping from index to component ID
                            
                            // For now, let's check if it's encoder-1 based on the current structure
                            if (componentId == "encoder-1" || i == 19 || i == 20 || i == 21) { // Assume encoder indices
                                // Determine if this is encoder left, right, or button
                                if (i == 19) { // Left rotation
                                    bindingObj["encoder_direction"] = "left"; 
                                } else if (i == 20) { // Right rotation
                                    bindingObj["encoder_direction"] = "right";
                                } else if (i == 21) { // Button press
                                    bindingObj["encoder_direction"] = "button";
                                }
                            }
                        }
                    }
                    
                    // Get available macros (limited to names/ids only to save memory)
                    if (macroHandler) {
                        JsonArray macrosArray = configDoc.createNestedArray("macros");
                        std::vector<String> macroIds = macroHandler->getAvailableMacros();
                        
                        // Debug log
                        USBSerial.printf("Available macros: %d\n", macroIds.size());
                        
                        for (const String& macroId : macroIds) {
                            Macro macro;
                            if (macroHandler->getMacro(macroId, macro)) {
                                JsonObject macroObj = macrosArray.createNestedObject();
                                macroObj["id"] = macro.id;
                                macroObj["name"] = macro.name;
                                // Only include essential information
                            }
                        }
                    }
                    
                    // Stream to string and check size before sending
                    String message;
                    serializeJson(configDoc, message);
                    USBSerial.printf("Config message size: %d bytes\n", message.length());
                    
                    // Send in chunks if too large
                    if (message.length() > 4096) {
                        USBSerial.println("Large message, sending in chunks...");
                        // Send first part
                        client->text(message.substring(0, 4096));
                        delay(10); // Small delay to allow processing
                        // Send second part
                        client->text(message.substring(4096));
                    } else {
                        client->text(message);
                    }
                }
                // Additional commands can be added here
            }
        }
    }
}

void WiFiManager::update() {
    // Check WiFi connection status (for STA mode)
    if (!_apMode && !_isConnected) {
        if (WiFi.status() == WL_CONNECTED) {
            _isConnected = true;
            USBSerial.println("WiFi connected");
            USBSerial.print("IP address: ");
            USBSerial.println(WiFi.localIP());
            
            // Show on display
            showTemporaryMessage(("WiFi Connected\nIP: " + WiFi.localIP().toString()).c_str(), 5000);
            
            // Broadcast connection status
            broadcastStatus();
        } else {
            // Check if connection attempt timed out
            if (millis() - _connectAttemptStart > CONNECT_TIMEOUT) {
                USBSerial.println("WiFi connection timed out. Switching to AP mode.");
                
                // Show on display
                showTemporaryMessage("WiFi connection timed out.\nSwitching to AP mode...", 3000);
                
                // Switch to AP mode
                _apMode = true;
                saveWiFiConfig();
                
                // Restart to apply changes
                ESP.restart();
            }
        }
    }
    
    // Periodic status broadcast
    if (millis() - _lastStatusBroadcast > STATUS_BROADCAST_INTERVAL) {
        broadcastStatus();
        _lastStatusBroadcast = millis();
    }
    
    // Clean up disconnected clients
    _ws.cleanupClients();
}

void WiFiManager::broadcastStatus() {
    // Only broadcast if we have clients
    if (_ws.count() > 0) {
        DynamicJsonDocument doc(2048);
        doc["type"] = "status";
        doc["data"]["wifi"]["connected"] = isConnected();
        doc["data"]["wifi"]["ip"] = getLocalIP().toString();
        doc["data"]["time"] = millis();
        
        // Add layer status
        if (keyHandler) {
            doc["data"]["layer"] = keyHandler->getCurrentLayer();
        } else {
            doc["data"]["layer"] = "default";
        }
        
        // Add macro execution status
        if (macroHandler) {
            doc["data"]["macro_running"] = macroHandler->isExecuting();
        } else {
            doc["data"]["macro_running"] = false;
        }
        
        String message;
        serializeJson(doc, message);
        _ws.textAll(message);
    }
}

void WiFiManager::loadWiFiConfig() {
    if (SPIFFS.exists("/config/wifi.json")) {
        File file = SPIFFS.open("/config/wifi.json", "r");
        if (file) {
            String json = file.readString();
            file.close();
            
            DynamicJsonDocument doc(1024);
            DeserializationError error = deserializeJson(doc, json);
            
            if (!error) {
                _ssid = doc["ssid"] | "MacroPad";
                _password = doc["password"] | "macropad123";
                _apMode = doc["ap_mode"] | true;
                
                USBSerial.println("WiFi configuration loaded");
            }
        }
    } else {
        USBSerial.println("WiFi config not found, using defaults");
        saveWiFiConfig(); // Create the default config file
    }
}

void WiFiManager::saveWiFiConfig() {
    DynamicJsonDocument doc(1024);
    doc["ssid"] = _ssid;
    doc["password"] = _password;
    doc["ap_mode"] = _apMode;
    
    String json;
    serializeJson(doc, json);
    
    File file = SPIFFS.open("/config/wifi.json", "w");
    if (file) {
        file.print(json);
        file.close();
        USBSerial.println("WiFi configuration saved");
    } else {
        USBSerial.println("Failed to save WiFi configuration");
    }
}

void WiFiManager::resetToDefaults() {
    // Reset WiFi settings
    _ssid = "MacroPad";
    _password = "macropad123";
    _apMode = true;
    saveWiFiConfig();
    
    // Reset other configs as needed
    // e.g., copy default configs to active configs
    if (SPIFFS.exists("/defaults/LEDs.json")) {
        File srcFile = SPIFFS.open("/defaults/LEDs.json", "r");
        File destFile = SPIFFS.open("/config/LEDs.json", "w");
        
        if (srcFile && destFile) {
            uint8_t buffer[512];
            size_t bytesRead;
            
            while ((bytesRead = srcFile.read(buffer, sizeof(buffer))) > 0) {
                destFile.write(buffer, bytesRead);
            }
            
            srcFile.close();
            destFile.close();
            USBSerial.println("LED configuration reset to defaults");
        }
    }
    
    USBSerial.println("All settings reset to defaults");
}

bool WiFiManager::isConnected() {
    return _isConnected;
}

IPAddress WiFiManager::getLocalIP() {
    if (_apMode) {
        return WiFi.softAPIP();
    } else {
        return WiFi.localIP();
    }
}

// Helper function to handle JSON strings
String serializedJson(const String& jsonStr) {
    // Return the JSON string without further parsing
    // This avoids double-parsing large JSON structures
    return jsonStr;
}

// Helper function to lookup key names from reports
String lookupKeyName(const uint8_t* report, size_t reportSize, bool isConsumer) {
    // Load reports.json
    if (!SPIFFS.exists("/config/reports.json")) {
        return "Unknown";
    }
    
    File file = SPIFFS.open("/config/reports.json", "r");
    if (!file) {
        return "Error";
    }
    
    // Convert report to hex string for comparison
    String reportStr = "";
    for (size_t i = 0; i < reportSize; i++) {
        char hexStr[8];
        sprintf(hexStr, "0x%02X", report[i]);
        if (i > 0) reportStr += ", ";
        reportStr += hexStr;
    }
    
    // Read through the file and try to find a match
    // This is not efficient but we don't have enough memory for full JSON parsing
    String line;
    String currentCategory = "";
    String currentKey = "";
    bool inKeyboard = !isConsumer;
    bool inConsumer = isConsumer;
    
    while (file.available()) {
        line = file.readStringUntil('\n');
        line.trim();
        
        // Check for category changes
        if (line.indexOf("\"keyboard\"") >= 0) {
            inKeyboard = true;
            inConsumer = false;
            continue;
        } else if (line.indexOf("\"consumer\"") >= 0) {
            inKeyboard = false;
            inConsumer = true;
            continue;
        } else if (line.indexOf("\"modifiers\"") >= 0 || 
                  line.indexOf("\"basicKeys\"") >= 0 || 
                  line.indexOf("\"functionKeys\"") >= 0 ||
                  line.indexOf("\"fKeys\"") >= 0 ||
                  line.indexOf("\"navigationKeys\"") >= 0 ||
                  line.indexOf("\"numpadKeys\"") >= 0 ||
                  line.indexOf("\"specialKeys\"") >= 0 ||
                  line.indexOf("\"media\"") >= 0 ||
                  line.indexOf("\"commonCombos\"") >= 0 ||
                  line.indexOf("\"macros\"") >= 0) {
            currentCategory = line.substring(line.indexOf("\"") + 1);
            currentCategory = currentCategory.substring(0, currentCategory.indexOf("\""));
            continue;
        }
        
        // Check for key definition
        int keyNameStart = line.indexOf("\"");
        if (keyNameStart >= 0) {
            int keyNameEnd = line.indexOf("\"", keyNameStart + 1);
            if (keyNameEnd > keyNameStart) {
                currentKey = line.substring(keyNameStart + 1, keyNameEnd);
                
                // Check if this line contains our report
                if (line.indexOf(reportStr) >= 0) {
                    file.close();
                    // Format: Category.KeyName
                    return currentCategory + "." + currentKey;
                }
            }
        }
    }
    
    file.close();
    
    // If it's a macro, return "macro"
    if (isConsumer && report[0] == 0 && report[1] == 0 && report[2] == 0 && report[3] == 0) {
        return "None";
    } else if (!isConsumer && report[0] == 0 && report[1] == 0 && report[2] == 0 && 
              report[3] == 0 && report[4] == 0 && report[5] == 0 && report[6] == 0 && report[7] == 0) {
        return "None";
    }
    
    return "Custom";
} 