#include "USBServer.h"
#include <Arduino.h>
#include <SPIFFS.h>
#include <ESPAsyncWebServer.h>
#include <lwip/ip_addr.h>
#include <lwip/inet.h>
#include "esp_netif.h"
#include <stdlib.h>
#include <lwip/apps/netbiosns.h>

// USBCDC serial for debugging output
extern USBCDC USBSerial;


// Create an asynchronous web server on port 80.
AsyncWebServer server(80);

// Flag to track if server is initialized
bool serverInitialized = false;
bool networkConfigured = false;

// Setup server routes
void setupServerRoutes() {
    // Only set up routes once
    static bool routesConfigured = false;
    if (routesConfigured) return;
    
    // Serve static files from the /web folder
    server.serveStatic("/", SPIFFS, "/web/").setDefaultFile("index.html");

    // Add a simple API endpoint to check server status
    server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/plain", "USB CDC server is running");
    });

    // Add an API endpoint to display basic network info
    server.on("/api/network", HTTP_GET, [](AsyncWebServerRequest *request) {
        String networkInfo = "Device Hostname: ";
        networkInfo += HOSTNAME;
        networkInfo += "\nStatic IP: 192.168.7.1";
        networkInfo += "\nAccess methods:";
        networkInfo += "\n  - http://192.168.7.1 (direct IP)";
        networkInfo += "\n  - http://";
        networkInfo += HOSTNAME;
        networkInfo += ".local (requires mDNS support)";
        networkInfo += "\n  - http://";
        networkInfo += HOSTNAME;
        networkInfo += " (requires NetBIOS support)";
        request->send(200, "text/plain", networkInfo);
    });

    // Add an admin endpoint that requires login
    server.on("/admin", HTTP_GET, [](AsyncWebServerRequest *request) {
        // If authentication fails, prompt the user
        if (!request->authenticate("admin", "mypassword")) {
            return request->requestAuthentication();
        }
        // If successful, send a welcome message
        request->send(200, "text/plain", "Welcome, admin! You are logged in.");
    });
    
    routesConfigured = true;
}

// Configure USB network with static IP
bool configureUsbStaticIP() {
    // Only configure once
    if (networkConfigured) {
        return true;
    }
    
    // Find the USB network interface
    esp_netif_t* usb_netif = nullptr;
    
    // Try multiple possible interface keys as they might vary based on Arduino core version
    const char* possibleKeys[] = {"USBNetwork", "RNDIS", "CDC", "TinyUSB"};
    
    for (const char* key : possibleKeys) {
        usb_netif = esp_netif_get_handle_from_ifkey(key);
        if (usb_netif != nullptr) {
            USBSerial.printf("Found USB network interface with key: %s\n", key);
            break;
        }
    }
    
    if (usb_netif == nullptr) {
        // If we couldn't find the interface by key, try to find it by scanning all interfaces
        usb_netif = esp_netif_next(nullptr);
        while (usb_netif != nullptr) {
            char ifname[16];
            if (esp_netif_get_netif_impl_name(usb_netif, ifname) == ESP_OK) {
                USBSerial.printf("Found network interface: %s\n", ifname);
                if (strstr(ifname, "USB") || strstr(ifname, "usb") || 
                    strstr(ifname, "RNDIS") || strstr(ifname, "rndis")) {
                    USBSerial.printf("Using network interface: %s\n", ifname);
                    break;
                }
            }
            usb_netif = esp_netif_next(usb_netif);
        }
    }
    
    if (usb_netif == nullptr) {
        USBSerial.println("USB network interface not found. Will retry later.");
        return false;
    }

    // Set up NetBIOS (if not already set up)
    static bool netbios_initialized = false;
    if (!netbios_initialized) {
        try {
            netbiosns_init();
            netbiosns_set_name(HOSTNAME);
            USBSerial.printf("NetBIOS name set to: %s\n", HOSTNAME);
            netbios_initialized = true;
        } catch (...) {
            USBSerial.println("Error initializing NetBIOS");
        }
    }

    // Set a fixed IP for the device
    esp_netif_ip_info_t ipInfo;
    memset(&ipInfo, 0, sizeof(ipInfo));
    
    // We'll use 192.168.7.1 as it's the gateway
    IP4_ADDR(&ipInfo.ip, 192, 168, 7, 1);
    IP4_ADDR(&ipInfo.gw, 192, 168, 7, 1);
    IP4_ADDR(&ipInfo.netmask, 255, 255, 255, 0);
    
    USBSerial.println("Setting static IP: 192.168.7.1");

    // Stop the DHCP client (if running) on this interface
    esp_err_t dhcp_stop_result = esp_netif_dhcpc_stop(usb_netif);
    if (dhcp_stop_result != ESP_OK && dhcp_stop_result != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED) {
        USBSerial.printf("Failed to stop DHCP client: %d\n", dhcp_stop_result);
        // Continue anyway - this might not be fatal
    }

    // Apply the static IP configuration
    esp_err_t set_ip_result = esp_netif_set_ip_info(usb_netif, &ipInfo);
    if (set_ip_result == ESP_OK) {
        USBSerial.println("Static IP configured on USB network interface");
        
        // Configure DHCP server for client connections (optional)
        esp_err_t dhcps_result = esp_netif_dhcps_start(usb_netif);
        if (dhcps_result == ESP_OK) {
            USBSerial.println("DHCP server started");
        } else {
            USBSerial.printf("Failed to start DHCP server: %d (non-fatal)\n", dhcps_result);
        }
        
        networkConfigured = true;
        return true;
    } else {
        USBSerial.printf("Failed to set static IP: %d\n", set_ip_result);
        return false;
    }
}

// Initialize the USB CDC HTTP server (non-blocking)
void initUSBServer() {
    // Only initialize once
    if (serverInitialized) {
        return;
    }
    
    // Set up the server routes
    setupServerRoutes();
    
    // Try to configure the USB network
    if (configureUsbStaticIP()) {
        USBSerial.println("USB network configured successfully");
    } else {
        USBSerial.println("Will try to configure USB network later in updateUSBServer()");
    }
    
    // Start the HTTP server
    try {
        server.begin();
        USBSerial.println("USB HTTP server started");
        USBSerial.printf("Connect to this device via USB at http://%s.local or http://192.168.7.1\n", HOSTNAME);
        serverInitialized = true;
    } catch (...) {
        USBSerial.println("Failed to start HTTP server - will retry later");
    }
}

// Update the USB server (call from loop)
void updateUSBServer() {
    static unsigned long lastRetryTime = 0;
    const unsigned long RETRY_INTERVAL = 10000; // 10 seconds
    
    // If everything is set up, nothing to do
    if (serverInitialized && networkConfigured) {
        return;
    }
    
    // Only retry periodically
    unsigned long now = millis();
    if (now - lastRetryTime < RETRY_INTERVAL) {
        return;
    }
    
    lastRetryTime = now;
    USBSerial.println("Trying to set up USB server...");
    
    // Try to set up the network if not yet configured
    if (!networkConfigured) {
        if (configureUsbStaticIP()) {
            USBSerial.println("USB network configured successfully on retry");
        } else {
            USBSerial.println("USB network configuration failed - will retry later");
        }
    }
    
    // Try to start the server if not yet initialized
    if (!serverInitialized) {
        try {
            setupServerRoutes();
            server.begin();
            USBSerial.println("USB HTTP server started on retry");
            serverInitialized = true;
        } catch (...) {
            USBSerial.println("Failed to start HTTP server - will retry later");
        }
    }
}