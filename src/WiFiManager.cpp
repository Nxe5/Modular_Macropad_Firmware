#include "WiFiManager.h"
#include "LEDHandler.h"
#include "KeyHandler.h"
#include "DisplayHandler.h"
#include "MacroHandler.h"

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
const uint32_t WiFiManager::STATUS_BROADCAST_INTERVAL = 1000; // 1 second
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