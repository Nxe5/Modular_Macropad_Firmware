#ifndef WEB_SERVER_MANAGER_H
#define WEB_SERVER_MANAGER_H

#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <functional>

/**
 * WebServerManager
 * 
 * A specialized class for serving SvelteKit applications on ESP32
 * with proper SPA routing support and efficient static file serving.
 */
class WebServerManager {
public:
    // Initialize with server reference and port
    WebServerManager(AsyncWebServer& server, uint16_t port = 80);
    
    // Start the web server
    void begin();
    
    // Configure the web server for SvelteKit
    void configureSvelteKit(const char* basePath = "/web");
    
    // Add API endpoints
    void setupAPIEndpoints();
    
    // Register a new API endpoint handler
    void addAPIEndpoint(const String& path, ArRequestHandlerFunction handler);
    
    // Add a custom HTTP route handler
    void addRoute(const String& uri, WebRequestMethodComposite method, 
                 ArRequestHandlerFunction handler);
    
    // Handle gzipped file responses correctly
    void serveGzippedFile(AsyncWebServerRequest* request, const String& path, 
                         const String& contentType);
                         
private:
    AsyncWebServer& _server;
    uint16_t _port;
    String _basePath;
    
    // Internal methods
    String getMimeType(const String& path);
    void configureCommonHeaders();
    void setupDefaultRoutes();
    void setupSPAFallback();
};

#endif // WEB_SERVER_MANAGER_H 