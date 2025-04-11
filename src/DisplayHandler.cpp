// DisplayHandler.cpp
#include "DisplayHandler.h"
#include "WiFiManager.h"
#include "KeyHandler.h"
#include "MacroHandler.h"
#include "HIDHandler.h"
#include <LittleFS.h>
#include <Arduino.h>

// External declaration for USBSerial
extern USBCDC USBSerial;

// External references
extern KeyHandler* keyHandler;
extern MacroHandler* macroHandler;

// Global variables
Adafruit_ST7789* display = nullptr;
bool temporaryMessageActive = false;
uint32_t temporaryMessageTimeout = 0;
static uint32_t lastDisplayUpdate = 0;

// Display configuration
String activeMode = "config";
DisplayMode currentMode;
std::map<String, DisplayMode> displayModes;
String lastNormalContent = "";
static bool screenInitialized = false;

// Add these global variables at the top with other globals
bool backgroundLoaded = false;
uint16_t* backgroundBuffer = nullptr;

void initializeDisplay() {
    USBSerial.println("Starting display initialization...");
    
    // Initialize SPI with HSPI
    SPIClass* spi = new SPIClass(HSPI);
    spi->begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
    
    // Create display with SPI instance
    display = new Adafruit_ST7789(spi, TFT_CS, TFT_DC, TFT_RST);
    
    USBSerial.println("Display object created");
    
    // Set SPI speed and initialize
    display->setSPISpeed(80000000);
    display->init(240, 280, SPI_MODE0);
    
    USBSerial.println("Display initialized");
    
    // Set rotation to landscape mode (1 = 90 degrees, 3 = 270 degrees)
    display->setRotation(1);
    display->fillScreen(ST77XX_BLACK);
    
    // Mark the screen as initialized before doing anything else
    screenInitialized = true;
    
    // Load display configuration first
    loadDisplayConfig();
    
    // Load background image
    loadBackgroundImage();
    
    // Show welcome splash screen
    display->fillScreen(ST77XX_BLACK);
    display->setTextSize(4);
    display->setTextColor(ST77XX_WHITE);
    
    // Center "Welcome!" text
    int16_t x1, y1;
    uint16_t w, h;
    const char* welcomeText = "Welcome!";
    display->getTextBounds(welcomeText, 0, 0, &x1, &y1, &w, &h);
    display->setCursor((240 - w) / 2, 100);
    display->println(welcomeText);
    
    // Delay to show welcome screen
    delay(2000);
    
    // Skip all intermediate screens and go directly to the main display
    USBSerial.println("Going directly to main display");
    
    // Make sure to completely disable temporary message handling
    temporaryMessageActive = false;
    
    // Set the active mode to "main" to use our new layout
    activeMode = "main";
    
    // Display the main layout directly
    displayMainLayout();
    
    USBSerial.println("Display setup complete");
}

Adafruit_ST7789* getDisplay() {
    return display;
}

void printText(const char* text, int x, int y, uint16_t color, uint8_t size) {
    if (!display) return;
    display->setTextSize(size);
    display->setTextColor(color);
    display->setCursor(x, y);
    display->print(text);
}

void drawTestPattern() {
    if (!display) {
        USBSerial.println("Cannot draw test pattern - display not initialized");
        return;
    }
    
    // Force screen initialization if display exists
    if (!screenInitialized) {
        screenInitialized = true;
    }
    
    USBSerial.println("Drawing diagnostic test pattern");
    
    // Save the current state for restoration
    bool wasTemporaryMessageActive = temporaryMessageActive;
    
    // Clear the screen
    display->fillScreen(ST77XX_BLACK);
    
    // Draw some basic shapes
    display->fillRect(10, 10, 50, 50, ST77XX_RED);
    display->fillRect(70, 10, 50, 50, ST77XX_GREEN);
    display->fillRect(130, 10, 50, 50, ST77XX_BLUE);
    
    // Draw some text
    display->setTextColor(ST77XX_WHITE);
    display->setTextSize(2);
    display->setCursor(50, 80);
    display->println("DIAGNOSTIC TEST");
    
    display->setTextSize(1);
    display->setCursor(20, 120);
    display->println("Display is working correctly");
    
    // Draw a line
    display->drawLine(10, 150, 200, 150, ST77XX_YELLOW);
    
    USBSerial.println("Diagnostic test pattern drawn");
    
    // Note: This is only for diagnostics, we don't set temporaryMessageActive
    // to avoid interfering with normal operation
}

void updateDisplay() {
    // Only update if display is initialized
    if (!display) {
        USBSerial.println("ERROR: Display object is NULL");
        return;
    }
    
    // Ensure screen is marked as initialized if display exists
    if (!screenInitialized && display) {
        screenInitialized = true;
    }
    
    // Rate limit display updates to prevent flickering
    uint32_t currentTime = millis();
    if (currentTime - lastDisplayUpdate < 500) { // Increased to 500ms to reduce flickering
        return;
    }
    lastDisplayUpdate = currentTime;
    
    // Check if we need to clear a temporary message
    if (temporaryMessageActive) {
        checkTemporaryMessage();
        if (!temporaryMessageActive) {
            // Switch to main layout after temporary message
            displayMainLayout();
        }
        return;
    }
    
    // Store current state values to detect changes
    static String lastLayer = "";
    static String lastWifiStatus = "";
    static String lastIpAddress = "";
    static String lastMacroStatus = "";
    
    // Get current state values
    String currentLayer = keyHandler ? keyHandler->getCurrentLayer() : "default";
    String currentWifiStatus = WiFiManager::isConnected() ? "Connected" : "Disconnected";
    String currentIpAddress = WiFiManager::getLocalIP().toString();
    String currentMacroStatus = (macroHandler && macroHandler->isExecuting()) ? "Running" : "Ready";
    
    // Check if any state has changed
    bool stateChanged = (lastLayer != currentLayer) || 
                        (lastWifiStatus != currentWifiStatus) || 
                        (lastIpAddress != currentIpAddress) || 
                        (lastMacroStatus != currentMacroStatus);
    
    // Update last state values
    lastLayer = currentLayer;
    lastWifiStatus = currentWifiStatus;
    lastIpAddress = currentIpAddress;
    lastMacroStatus = currentMacroStatus;
    
    // If no state has changed, skip redrawing to prevent flicker
    if (!stateChanged) {
        return;
    }
    
    // Display the main layout
    displayMainLayout();
}

void loadDisplayConfig() {
    if (!LittleFS.exists(DISPLAY_CONFIG_PATH)) {
        USBSerial.println("Display config not found, using defaults");
        return;
    }
    
    File file = LittleFS.open(DISPLAY_CONFIG_PATH, "r");
    if (!file) {
        USBSerial.println("Failed to open display config");
        return;
    }
    
    // Parse JSON configuration
    DynamicJsonDocument doc(4096);
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    
    if (error) {
        USBSerial.print("Failed to parse display config: ");
        USBSerial.println(error.c_str());
        return;
    }
    
    // Clear existing modes
    displayModes.clear();
    
    // Load active mode
    activeMode = doc["active_mode"].as<String>();
    
    // Load all display modes
    JsonObject modes = doc["modes"];
    for (JsonPair mode : modes) {
        DisplayMode displayMode;
        displayMode.name = mode.value()["name"].as<String>();
        displayMode.description = mode.value()["description"].as<String>();
        displayMode.refresh_rate = mode.value()["refresh_rate"] | DISPLAY_UPDATE_INTERVAL;
        
        USBSerial.printf("Loading mode: %s with refresh rate: %d\n", 
                        displayMode.name.c_str(), displayMode.refresh_rate);
        
        // Load elements
        JsonArray elements = mode.value()["elements"].as<JsonArray>();
        USBSerial.printf("Found %d elements in JSON array\n", elements.size());
        
        for (JsonVariant element : elements) {
            DisplayElement displayElement;
            JsonObject elementObj = element.as<JsonObject>();
            USBSerial.printf("Loading element of type: %s\n", elementObj["type"].as<String>().c_str());
            
            // Common properties
            displayElement.x = elementObj["x"] | 0;
            displayElement.y = elementObj["y"] | 0;
            
            // Parse color (default to white)
            String colorStr = elementObj["color"] | "white";
            USBSerial.printf("Color string: %s\n", colorStr.c_str());
            
            // Check if it's a hex value (starts with 0x)
            if (colorStr.startsWith("0x")) {
                // Convert hex string to integer
                char* endPtr;
                uint32_t colorValue = strtol(colorStr.c_str(), &endPtr, 16);
                displayElement.color = colorValue;
                USBSerial.printf("Using hex color: 0x%X\n", displayElement.color);
            }
            else if (colorStr == "white") {
                displayElement.color = ST77XX_WHITE;
                USBSerial.println("Using WHITE color");
            }
            else if (colorStr == "red") {
                displayElement.color = ST77XX_RED;
                USBSerial.println("Using RED color");
            }
            else if (colorStr == "green") {
                displayElement.color = ST77XX_GREEN;
                USBSerial.println("Using GREEN color");
            }
            else if (colorStr == "blue") {
                displayElement.color = ST77XX_BLUE;
                USBSerial.println("Using BLUE color");
            }
            else if (colorStr == "yellow") {
                displayElement.color = ST77XX_YELLOW;
                USBSerial.println("Using YELLOW color");
            }
            else if (colorStr == "cyan") {
                displayElement.color = ST77XX_CYAN;
                USBSerial.println("Using CYAN color");
            }
            else {
                displayElement.color = ST77XX_WHITE;
                USBSerial.println("Using default WHITE color");
            }
            
            // Type specific properties
            String type = elementObj["type"].as<String>();
            if (type == "text") {
                displayElement.type = ELEMENT_TEXT;
                displayElement.text = elementObj["text"].as<String>();
                displayElement.size = elementObj["size"] | 1;
            } else if (type == "line") {
                displayElement.type = ELEMENT_LINE;
                displayElement.end_x = elementObj["end_x"] | displayElement.x;
                displayElement.end_y = elementObj["end_y"] | displayElement.y;
            } else if (type == "rect") {
                displayElement.type = ELEMENT_RECT;
                displayElement.width = elementObj["width"] | 10;
                displayElement.height = elementObj["height"] | 10;
                displayElement.filled = elementObj["filled"] | false;
            } else if (type == "circle") {
                displayElement.type = ELEMENT_CIRCLE;
                displayElement.width = elementObj["diameter"] | 10;
                displayElement.filled = elementObj["filled"] | false;
            }
            
            USBSerial.printf("Adding element type %d at position %d,%d with color %d\n", 
                           displayElement.type, displayElement.x, displayElement.y, displayElement.color);
            
            // Add element to mode
            displayMode.elements.push_back(displayElement);
            USBSerial.printf("Mode now has %d elements\n", displayMode.elements.size());
        }
        
        // Add mode to map
        displayModes[mode.key().c_str()] = displayMode;
    }
    
    // Activate the default mode
    if (!activeMode.isEmpty() && displayModes.count(activeMode) > 0) {
        activateDisplayMode(activeMode);
    }
}

void activateDisplayMode(const String& modeName) {
    USBSerial.println("Activating display mode: " + modeName);
    
    // Set the active mode
    activeMode = modeName;
    
    // If it's the main mode, display our new layout
    if (modeName == "main" || modeName == "config") {
        // Make sure background is loaded
        if (!backgroundLoaded) {
            loadBackgroundImage();
        }
        
        // Display the main layout
        displayMainLayout();
        return;
    }
    
    // For other modes, use the existing display logic
    if (displayModes.count(modeName) > 0) {
        // Make a deep copy of the mode
        currentMode = displayModes[modeName];
        
        // Clear the screen before switching modes for clean transition
        display->fillScreen(ST77XX_BLACK);
        
        // Always ensure the title is set and centered correctly
        // - This is critical to prevent title from disappearing on refresh
        bool titleFound = false;
        
        for (auto& element : currentMode.elements) {
            if (element.type == ELEMENT_TEXT && element.y <= 30) {
                // This is the title element
                element.text = "0cho Labs Macropad";
                element.size = 2;
                titleFound = true;
                
                // Center the text perfectly
                int16_t x1, y1;
                uint16_t w, h;
                display->setTextSize(element.size);
                display->getTextBounds(element.text.c_str(), 0, 0, &x1, &y1, &w, &h);
                element.x = (240 - w) / 2; // Set x position to center text exactly
                
                USBSerial.printf("Centered title at x=%d with width=%d\n", element.x, w);
            }
        }
        
        // If no title was found, add one
        if (!titleFound && modeName == "config") {
            DisplayElement titleElement;
            titleElement.type = ELEMENT_TEXT;
            titleElement.text = "0cho Labs Macropad";
            titleElement.size = 2;
            titleElement.y = 25;
            titleElement.color = ST77XX_WHITE;
            
            // Center the text perfectly
            int16_t x1, y1;
            uint16_t w, h;
            display->setTextSize(titleElement.size);
            display->getTextBounds(titleElement.text.c_str(), 0, 0, &x1, &y1, &w, &h);
            titleElement.x = (240 - w) / 2; // Set x position to center text exactly
            
            USBSerial.printf("Added centered title at x=%d with width=%d\n", titleElement.x, w);
            
            // Add to the beginning of elements so it's drawn first
            currentMode.elements.insert(currentMode.elements.begin(), titleElement);
        }
        
        // Force a full redraw of all elements
        for (const auto& element : currentMode.elements) {
            switch (element.type) {
                case ELEMENT_TEXT: {
                    String processedText = element.text;
                    
                    // Process any variables in the text
                    if (element.text.indexOf("{layer}") >= 0) {
                        String layer = keyHandler ? keyHandler->getCurrentLayer() : "default";
                        processedText.replace("{layer}", layer);
                    }
                    
                    if (element.text.indexOf("{wifi_status}") >= 0) {
                        String wifiStatus = WiFiManager::isConnected() ? "Connected" : "Disconnected";
                        processedText.replace("{wifi_status}", wifiStatus);
                    }
                    
                    if (element.text.indexOf("{ip_address}") >= 0) {
                        String ipAddress = WiFiManager::getLocalIP().toString();
                        processedText.replace("{ip_address}", ipAddress);
                    }
                    
                    if (element.text.indexOf("{macro_status}") >= 0) {
                        String macroStatus = (macroHandler && macroHandler->isExecuting()) ? "Running" : "Ready";
                        processedText.replace("{macro_status}", macroStatus);
                    }
                    
                    if (element.text.indexOf("{current_mode}") >= 0) {
                        processedText.replace("{current_mode}", currentMode.name);
                    }
                    
                    // Draw the text
                    display->setTextSize(element.size);
                    display->setTextColor(element.color);
                    display->setCursor(element.x, element.y);
                    display->println(processedText);
                    break;
                }
                case ELEMENT_LINE:
                    display->drawLine(element.x, element.y, element.end_x, element.end_y, element.color);
                    break;
                case ELEMENT_RECT:
                    if (element.filled) {
                        display->fillRect(element.x, element.y, element.width, element.height, element.color);
                    } else {
                        display->drawRect(element.x, element.y, element.width, element.height, element.color);
                    }
                    break;
                case ELEMENT_CIRCLE:
                    if (element.filled) {
                        display->fillCircle(element.x, element.y, element.width/2, element.color);
                    } else {
                        display->drawCircle(element.x, element.y, element.width/2, element.color);
                    }
                    break;
            }
        }
        
        USBSerial.println("Main display activated");
    } else {
        USBSerial.println("Display mode not found: " + modeName);
    }
}

String getCurrentMode() {
    return activeMode;
}

bool isTemporaryMessageActive() {
    return temporaryMessageActive;
}

void handleEncoder(int encoderPosition) {
    // TODO: Implement encoder handling for display mode switching
    // This will be implemented when we add support for multiple display modes
}

void showTemporaryMessage(const char* message, uint32_t duration) {
    if (!display || !screenInitialized) {
        USBSerial.println("Cannot show temporary message - display not initialized");
        return;
    }
    
    // Save current state
    temporaryMessageActive = true;
    
    // Calculate message bounds for centering
    int16_t x1, y1;
    uint16_t msgWidth, msgHeight;
    int fontSize = 2;  // Larger font size for better visibility
    
    // Prepare for text measurement
    display->setTextSize(fontSize);
    
    // Get actual text bounds
    display->getTextBounds(message, 0, 0, &x1, &y1, &msgWidth, &msgHeight);
    
    // Calculate centered position
    int msgX = (240 - msgWidth) / 2;
    int msgY = 110;  // Vertically centered position
    
    // Clear previous message area - use whole screen width to be safe
    display->fillRect(0, msgY - msgHeight - 10, 240, msgHeight * 2 + 20, ST77XX_BLACK);
    
    // Draw a background box for better visibility - make it wider than the text
    int boxPadding = 10;
    display->fillRect(msgX - boxPadding, msgY - boxPadding, 
                     msgWidth + (boxPadding * 2), msgHeight + (boxPadding * 2), 
                     ST77XX_BLUE);
    
    // Now draw the message with white text
    display->setTextColor(ST77XX_WHITE);
    display->setCursor(msgX, msgY);
    display->println(message);
    
    // Update the timeout
    temporaryMessageTimeout = millis() + duration;
}

void checkTemporaryMessage() {
    if (temporaryMessageActive && millis() > temporaryMessageTimeout) {
        // Clear the temporary message flag
        temporaryMessageActive = false;
        
        // We don't want to reset screenInitialized to false here
        // as it's causing the "ERROR: Screen not initialized" errors
        
        // Don't reset lastDisplayUpdate - allow the next updateDisplay call to evaluate
        // the refresh rate interval on its own to avoid quick successive redraws
    }
}

void displayWiFiInfo(bool isAPMode, const String& ipAddress, const String& ssid) {
    // Just a stub - we don't want this function to do anything
    // as it was causing the screen refresh issues
    return;
}

void loadBackgroundImage() {
    USBSerial.println("Starting background image loading...");
    
    if (backgroundLoaded) {
        USBSerial.println("Background already loaded, skipping");
        return;
    }
    
    // Allocate buffer for the background (240x280 pixels)
    USBSerial.println("Allocating background buffer...");
    backgroundBuffer = (uint16_t*)malloc(240 * 280 * sizeof(uint16_t));
    if (!backgroundBuffer) {
        USBSerial.println("ERROR: Failed to allocate background buffer");
        return;
    }
    USBSerial.printf("Background buffer allocated at address: %p\n", (void*)backgroundBuffer);
    
    // Create a more visible gradient background
    USBSerial.println("Creating gradient background...");
    for (int y = 0; y < 280; y++) {
        for (int x = 0; x < 240; x++) {
            // Create a more visible gradient for testing
            // Red increases from left to right
            uint8_t red = map(x, 0, 240, 0, 31);
            // Green increases from top to bottom
            uint8_t green = map(y, 0, 280, 0, 31);
            // Blue stays constant for a base color
            uint8_t blue = 16;
            
            // Convert to RGB565 format
            uint16_t color = ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | (blue >> 3);
            backgroundBuffer[y * 240 + x] = color;
        }
    }
    
    // Verify buffer contents
    USBSerial.printf("First pixel color: 0x%04X\n", backgroundBuffer[0]);
    USBSerial.printf("Last pixel color: 0x%04X\n", backgroundBuffer[240 * 280 - 1]);
    
    backgroundLoaded = true;
    USBSerial.println("Background gradient created successfully");
}

void displayBackgroundImage() {
    USBSerial.println("Starting background display...");
    
    if (!backgroundLoaded) {
        USBSerial.println("ERROR: Background not loaded");
        return;
    }
    
    if (!backgroundBuffer) {
        USBSerial.println("ERROR: Background buffer is null");
        return;
    }
    
    if (!display) {
        USBSerial.println("ERROR: Display not initialized");
        return;
    }
    
    USBSerial.println("Copying background to display...");
    
    // Keep rotation 2 for background
    display->setRotation(2);  // 180 degrees for background
    
    // Clear the screen first
    display->fillScreen(ST77XX_BLACK);
    
    // Set up the display window
    display->startWrite();
    display->setAddrWindow(0, 0, 240, 280);
    
    // Write the pixels in smaller chunks to avoid potential buffer issues
    const int CHUNK_SIZE = 1024;
    for (int i = 0; i < 240 * 280; i += CHUNK_SIZE) {
        int remaining = (240 * 280) - i;
        int chunk = min(CHUNK_SIZE, remaining);
        display->writePixels(backgroundBuffer + i, chunk);
    }
    
    display->endWrite();
    USBSerial.println("Background displayed successfully");
}

void displayMainLayout() {
    USBSerial.println("Starting main layout display...");
    
    if (!display) {
        USBSerial.println("ERROR: Display not initialized");
        return;
    }
    
    // First display the background with rotation 2
    if (!backgroundLoaded) {
        USBSerial.println("Background not loaded, loading now...");
        loadBackgroundImage();
    }
    
    USBSerial.println("Displaying background...");
    displayBackgroundImage();
    
    // Now set rotation 1 for text (landscape)
    display->setRotation(1);
    
    // Define colors
    uint16_t textColor = ST77XX_WHITE;
    uint16_t accentColor = ST77XX_GREEN;
    uint16_t shadowColor = ST77XX_BLACK;
    
    // Set text properties
    display->setTextColor(textColor);
    display->setTextSize(1);
    
    // Helper function for drawing text with shadow
    auto drawTextWithShadow = [&](const char* text, int16_t x, int16_t y) {
        display->setTextColor(shadowColor);
        display->setCursor(x + 1, y + 1);
        display->print(text);
        display->setTextColor(textColor);
        display->setCursor(x, y);
        display->print(text);
    };
    
    // Draw title - adjusted for landscape orientation
    drawTextWithShadow("Modular Macropad", 10, 10);
    
    // Draw underline - adjusted for landscape orientation
    display->drawFastHLine(10, 25, 220, accentColor);
    
    // Draw info section - adjusted for landscape orientation
    int startY = 40;
    int lineHeight = 20;
    
    // Get current values
    String wifiStatus = WiFiManager::isConnected() ? WiFiManager::getSSID() : "Disconnected";
    String ipAddress = WiFiManager::getLocalIP().toString();
    String macroStatus = (macroHandler && macroHandler->isExecuting()) ? "Running" : "Ready";
    String layerName = keyHandler ? keyHandler->getCurrentLayer() : "default";
    
    // Draw info lines - adjusted for landscape orientation
    drawTextWithShadow(("WiFi: " + wifiStatus).c_str(), 10, startY);
    drawTextWithShadow(("IP: " + ipAddress).c_str(), 10, startY + lineHeight);
    drawTextWithShadow(("Macro: " + macroStatus).c_str(), 10, startY + lineHeight * 2);
    drawTextWithShadow(("Layer: " + layerName).c_str(), 10, startY + lineHeight * 3);
    
    // Update display
    display->endWrite();
    USBSerial.println("Main layout display completed");
}
