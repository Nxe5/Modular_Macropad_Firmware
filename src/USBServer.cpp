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

//
// Helper function: Configures the USB CDC network interface with a static IP in the 192.168.7.x range.
//
void configureUsbStaticIP() {
  // The correct interface key for ESP32-S3 with USB RNDIS mode enabled is "USBNetwork" or "RNDIS"
  // Try multiple possible interface keys as they might vary based on Arduino core version
  const char* possibleKeys[] = {"USBNetwork", "RNDIS", "CDC", "TinyUSB"};
  esp_netif_t* usb_netif = nullptr;
  
  // Try each key until we find a valid interface
  for (const char* key : possibleKeys) {
    usb_netif = esp_netif_get_handle_from_ifkey(key);
    if (usb_netif != nullptr) {
      USBSerial.printf("Found USB network interface with key: %s\n", key);
      break;
    }
  }
  
  if (usb_netif == nullptr) {
    USBSerial.println("USB network interface not found. Attempting to create it manually...");
    
    // Attempt to initialize RNDIS interface manually
    // This requires proper initialization of TinyUSB with RNDIS enabled in main.cpp
    esp_err_t err = ESP_OK;
    
    // Wait a bit for the USB interface to be ready
    delay(1000);
    
    // Get the default USB interface - this is a fallback approach
    usb_netif = esp_netif_next(nullptr);
    while (usb_netif != nullptr) {
      char ifname[16];
      if (esp_netif_get_netif_impl_name(usb_netif, ifname) == ESP_OK) {
        USBSerial.printf("Found network interface: %s\n", ifname);
        // Look for USB or RNDIS in the interface name
        if (strstr(ifname, "USB") != nullptr || strstr(ifname, "usb") != nullptr ||
            strstr(ifname, "RNDIS") != nullptr || strstr(ifname, "rndis") != nullptr) {
          USBSerial.printf("Using network interface: %s\n", ifname);
          break;
        }
      }
      usb_netif = esp_netif_next(usb_netif);
    }
    
    if (usb_netif == nullptr) {
      USBSerial.println("Failed to find any USB network interface. Check USB configuration.");
      return;
    }
  }

  // Clear the IP configuration structure.
  esp_netif_ip_info_t ipInfo;
  memset(&ipInfo, 0, sizeof(ipInfo));

  // Set a fixed IP for the device - we'll use 192.168.7.1 as it's the gateway
  // This makes it easier for users to connect to a known IP
  const char* deviceIp = "192.168.7.1";
  USBSerial.printf("Setting static IP: %s\n", deviceIp);

  // Convert string IP addresses to ip4_addr_t values.
  if (!ip4addr_aton(deviceIp, (ip4_addr_t *)&ipInfo.ip)) {
    USBSerial.println("Invalid IP address format");
    return;
  }
  if (!ip4addr_aton(deviceIp, (ip4_addr_t *)&ipInfo.gw)) {
    USBSerial.println("Invalid gateway address format");
    return;
  }
  if (!ip4addr_aton("255.255.255.0", (ip4_addr_t *)&ipInfo.netmask)) {
    USBSerial.println("Invalid netmask address format");
    return;
  }

  // Stop the DHCP client (if running) on this interface.
  if (esp_netif_dhcpc_stop(usb_netif) != ESP_OK) {
    USBSerial.println("Failed to stop DHCP client (this is normal if DHCP was not running)");
  }

  // Apply the static IP configuration.
  esp_err_t err = esp_netif_set_ip_info(usb_netif, &ipInfo);
  if (err == ESP_OK) {
    USBSerial.printf("Static IP %s configured on USB network interface\n", deviceIp);
    
    // Configure DHCP server for client connections (optional)
    // This allows connected computers to get an IP address automatically
    esp_err_t dhcps_err = esp_netif_dhcps_start(usb_netif);
    if (dhcps_err == ESP_OK) {
      USBSerial.println("DHCP server started");
    } else {
      USBSerial.printf("Failed to start DHCP server: %d\n", dhcps_err);
    }
  } else {
    USBSerial.printf("Failed to set static IP: %d\n", err);
  }
}

//
// Initializes the USB CDC HTTP server.
// Mounts SPIFFS, sets hostname, configures the network, and sets up HTTP endpoints (including a login endpoint).
//
void initUSBServer() {
  // Mount SPIFFS. Make sure your built Svelte app is in the /web folder.
  if (!SPIFFS.begin(true)) {
    USBSerial.println("SPIFFS mount failed");
    return;
  }
  USBSerial.println("SPIFFS mounted successfully");

  // Set the hostname for mDNS and NetBIOS (optional).
  WiFi.setHostname(HOSTNAME);
  USBSerial.printf("Hostname set to: %s\n", HOSTNAME);

  // Initialize the NetBIOS responder so Windows machines can find the device.
  netbiosns_init();
  netbiosns_set_name(HOSTNAME);
  USBSerial.printf("NetBIOS name set to: %s\n", HOSTNAME);

  // Configure the USB CDC network with a static IP.
  configureUsbStaticIP();

  // Serve static files from the /web folder. Ensure that an index.html file exists.
  server.serveStatic("/", SPIFFS, "/web/").setDefaultFile("index.html");

  // Add a simple API endpoint to check server status.
  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "USB CDC server is running");
  });

  // Add an API endpoint to display basic network info.
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

  // Add an admin endpoint that requires login.
  server.on("/admin", HTTP_GET, [](AsyncWebServerRequest *request) {
    // If authentication fails, prompt the user.
    if (!request->authenticate("admin", "mypassword")) {
      return request->requestAuthentication();
    }
    // If successful, send a welcome message.
    request->send(200, "text/plain", "Welcome, admin! You are logged in.");
  });

  // Start the HTTP server.
  server.begin();
  USBSerial.println("USB HTTP server started");
  USBSerial.printf("Connect to this device via USB at http://%s.local or http://192.168.7.1\n", HOSTNAME);
}