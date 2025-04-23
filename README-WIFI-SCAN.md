# WiFi Scanning Functionality for Modular Macropad

This document outlines the changes needed in the firmware to implement WiFi network scanning functionality.

## Overview

To support the new WiFi management features in the front-end application, we need to add a new API endpoint to the firmware that will scan for available WiFi networks and return them in a JSON format.

## Implementation Steps

### 1. Add WiFi Scan API Endpoint

Add the following code to `WiFiManager.cpp` in the `setupWebServer()` method:

```cpp
// WiFi Scan
_server.on("/api/wifi/scan", HTTP_GET, [](AsyncWebServerRequest *request) {
    USBSerial.println("Scanning for WiFi networks...");
    
    int networksFound = WiFi.scanNetworks();
    USBSerial.printf("Found %d networks\n", networksFound);
    
    DynamicJsonDocument doc(4096); // Increase size if needed for many networks
    JsonArray networks = doc.createNestedArray();
    
    for (int i = 0; i < networksFound; i++) {
        JsonObject network = networks.createNestedObject();
        network["ssid"] = WiFi.SSID(i);
        network["rssi"] = WiFi.RSSI(i);
        
        // Convert encryption type to string
        String encryptionType = "Unknown";
        switch (WiFi.encryptionType(i)) {
            case WIFI_AUTH_OPEN:
                encryptionType = "OPEN";
                break;
            case WIFI_AUTH_WEP:
                encryptionType = "WEP";
                break;
            case WIFI_AUTH_WPA_PSK:
                encryptionType = "WPA";
                break;
            case WIFI_AUTH_WPA2_PSK:
                encryptionType = "WPA2";
                break;
            case WIFI_AUTH_WPA_WPA2_PSK:
                encryptionType = "WPA/WPA2";
                break;
            case WIFI_AUTH_WPA2_ENTERPRISE:
                encryptionType = "WPA2-Enterprise";
                break;
            default:
                encryptionType = "Unknown";
                break;
        }
        network["encryption"] = encryptionType;
        
        // Optional: add the channel
        network["channel"] = WiFi.channel(i);
        
        // Optional: add BSSID (MAC address)
        char bssid[18] = {0};
        sprintf(bssid, "%02X:%02X:%02X:%02X:%02X:%02X",
                WiFi.BSSID(i)[0], WiFi.BSSID(i)[1], WiFi.BSSID(i)[2],
                WiFi.BSSID(i)[3], WiFi.BSSID(i)[4], WiFi.BSSID(i)[5]);
        network["bssid"] = bssid;
    }
    
    // Free memory used by WiFi scan
    WiFi.scanDelete();
    
    String output;
    serializeJson(networks, output);
    
    // Add CORS headers
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", output);
    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
    response->addHeader("Access-Control-Allow-Headers", "Content-Type");
    request->send(response);
});

// CORS pre-flight for WiFi scan
_server.on("/api/wifi/scan", HTTP_OPTIONS, [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse(200);
    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
    response->addHeader("Access-Control-Allow-Headers", "Content-Type");
    request->send(response);
});
```

### 2. Important Considerations

1. **Memory Usage**: WiFi scanning can use significant memory. If you encounter issues, you may need to optimize the JSON document size or the scanning process.

2. **Scanning During Connection**: Be careful when scanning while connected to WiFi in STA mode, as scanning can briefly interrupt the connection.

3. **Performance**: Scanning can take several seconds. Consider adding a loading indicator in the UI during scanning.

4. **Power Consumption**: WiFi scanning uses more power. For battery-powered devices, consider this impact.

## Testing

After implementing these changes, you can test the API endpoint using curl:

```bash
curl -X GET http://<your-device-ip>/api/wifi/scan
```

The response should be a JSON array of available networks with their details. 