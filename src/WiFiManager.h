#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

// Forward declarations of handlers needed from other files
extern String getLEDConfigJson();
extern bool updateLEDConfigFromJson(const String& json);
extern bool saveLEDConfig();

class WiFiManager {
public:
    // Initialization
    static void begin();
    
    // Setup functions
    static void setupWiFi();
    static void setupWebServer();
    static void setupWebSocket();
    
    // Event handlers
    static void onWsEvent(AsyncWebSocket* server, AsyncWebSocketClient* client, 
                        AwsEventType type, void* arg, uint8_t* data, size_t len);
    
    // Update methods
    static void update();
    static void broadcastStatus();
    
    // Configuration methods
    static void loadWiFiConfig();
    static void saveWiFiConfig();
    static void resetToDefaults();
    
    // Status accessors
    static bool isConnected();
    static IPAddress getLocalIP();
    static bool isAPMode() { return _apMode; }
    static String getSSID() { return _ssid; }
    
private:
    // WiFi settings
    static String _ssid;
    static String _password;
    static bool _apMode; // True for AP-only mode, false for dual AP+STA mode
    static String _apName; // Name for the AP when in dual mode
    
    // Server and WebSocket
    static AsyncWebServer _server;
    static AsyncWebSocket _ws;
    
    // State variables
    static bool _isConnected;
    static uint32_t _lastStatusBroadcast;
    static uint32_t _connectAttemptStart;
    
    // Constants
    static const uint32_t STATUS_BROADCAST_INTERVAL;
    static const uint32_t CONNECT_TIMEOUT;
    
    // File serving methods
    static void setupFileRoutes();
};

#endif // WIFI_MANAGER_H 