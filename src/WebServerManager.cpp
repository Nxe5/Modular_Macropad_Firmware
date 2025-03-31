#include "WebServerManager.h"
#include <Arduino.h>
#include <LittleFS.h>

WebServerManager::WebServerManager(AsyncWebServer& server, uint16_t port)
    : _server(server), _port(port), _basePath("/web") {
}

void WebServerManager::begin() {
    configureCommonHeaders();
    setupDefaultRoutes();
}

void WebServerManager::configureSvelteKit(const char* basePath) {
    _basePath = basePath;
    
    // Configure static file serving for SvelteKit structure
    _server.serveStatic("/_app/", LittleFS, (_basePath + "/_app/").c_str());
    _server.serveStatic("/favicon.png", LittleFS, (_basePath + "/favicon.png").c_str());
    
    // Handle assets in the root path
    _server.serveStatic("/assets/", LittleFS, (_basePath + "/assets/").c_str());
    
    // Setup SPA routing (fallback to index.html for client-side routing)
    setupSPAFallback();
}

void WebServerManager::setupAPIEndpoints() {
    // Add basic system status endpoint
    _server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request) {
        DynamicJsonDocument doc(1024);
        doc["uptime"] = millis() / 1000;
        doc["heap"] = ESP.getFreeHeap();
        
        String output;
        serializeJson(doc, output);
        request->send(200, "application/json", output);
    });
}

void WebServerManager::addAPIEndpoint(const String& path, ArRequestHandlerFunction handler) {
    _server.on(("/api/" + path).c_str(), HTTP_GET, handler);
}

void WebServerManager::addRoute(const String& uri, WebRequestMethodComposite method, 
                             ArRequestHandlerFunction handler) {
    _server.on(uri.c_str(), method, handler);
}

void WebServerManager::serveGzippedFile(AsyncWebServerRequest* request, const String& path, 
                                     const String& contentType) {
    String fullPath = _basePath + path;
    String gzPath = fullPath + ".gz";
    
    if (LittleFS.exists(gzPath)) {
        AsyncWebServerResponse *response = request->beginResponse(LittleFS, gzPath, contentType);
        response->addHeader("Content-Encoding", "gzip");
        request->send(response);
    } else if (LittleFS.exists(fullPath)) {
        request->send(LittleFS, fullPath, contentType);
    } else {
        request->send(404, "text/plain", "File Not Found");
    }
}

String WebServerManager::getMimeType(const String& path) {
    if (path.endsWith(".html")) return "text/html";
    if (path.endsWith(".css")) return "text/css";
    if (path.endsWith(".js")) return "application/javascript";
    if (path.endsWith(".json")) return "application/json";
    if (path.endsWith(".png")) return "image/png";
    if (path.endsWith(".jpg") || path.endsWith(".jpeg")) return "image/jpeg";
    if (path.endsWith(".svg")) return "image/svg+xml";
    if (path.endsWith(".ico")) return "image/x-icon";
    if (path.endsWith(".woff")) return "font/woff";
    if (path.endsWith(".woff2")) return "font/woff2";
    if (path.endsWith(".ttf")) return "font/ttf";
    if (path.endsWith(".eot")) return "font/eot";
    return "text/plain";
}

void WebServerManager::configureCommonHeaders() {
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Accept, Content-Type, Authorization");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Credentials", "true");
    DefaultHeaders::Instance().addHeader("Cache-Control", "no-cache");
}

void WebServerManager::setupDefaultRoutes() {
    // Handle OPTIONS method for CORS preflight requests
    _server.onNotFound([this](AsyncWebServerRequest *request) {
        if (request->method() == HTTP_OPTIONS) {
            request->send(200);
            return;
        }
        
        // If not an OPTIONS request and not a recognized route, use SPA fallback
        if (request->method() == HTTP_GET) {
            // Log request path
            Serial.printf("SPA route: %s\n", request->url().c_str());
            
            // Serve index.html for all non-asset requests to support SPA routing
            serveGzippedFile(request, "/index.html", "text/html");
        } else {
            request->send(404);
        }
    });
}

void WebServerManager::setupSPAFallback() {
    // This is handled in the onNotFound handler
    // SPA routing needs all unrecognized paths to serve the index.html file
} 