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
    
    // Draw initial test pattern for landscape orientation
    display->fillRect(0, 0, 93, 240, ST77XX_RED);
    display->fillRect(93, 0, 93, 240, ST77XX_GREEN);
    display->fillRect(186, 0, 94, 240, ST77XX_BLUE);
    
    display->setTextColor(ST77XX_WHITE);
    display->setTextSize(3);
    display->setCursor(100, 100);
    display->println("Hello");
    display->setCursor(100, 140);
    display->println("Andrew!");
    
    USBSerial.println("Test pattern drawn - display ready");
    
    // Load display configuration
    loadDisplayConfig();
    
    // Set initial delay before switching to active mode
    temporaryMessageTimeout = millis() + 3000;
    temporaryMessageActive = true;
    screenInitialized = true;
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
    if (!display || !screenInitialized) {
        USBSerial.println("Cannot draw test pattern - display not initialized");
        return;
    }
    
    USBSerial.println("Drawing manual test pattern");
    
    // Clear the screen
    display->fillScreen(ST77XX_BLACK);
    
    // Draw some basic shapes
    display->fillRect(10, 10, 50, 50, ST77XX_RED);
    display->fillRect(70, 10, 50, 50, ST77XX_GREEN);
    display->fillRect(130, 10, 50, 50, ST77XX_BLUE);
    
    // Draw some text
    display->setTextColor(ST77XX_WHITE);
    display->setTextSize(2);
    display->setCursor(60, 80);
    display->println("TEST PATTERN");
    
    display->setTextSize(1);
    display->setCursor(20, 120);
    display->println("If you can see this, display works!");
    
    // Draw a line
    display->drawLine(10, 150, 200, 150, ST77XX_YELLOW);
    
    USBSerial.println("Test pattern drawn");
}

void updateDisplay() {
    // Rate limit display updates to prevent flickering
    uint32_t currentTime = millis();
    if (currentTime - lastDisplayUpdate < currentMode.refresh_rate) {
        return;
    }
    lastDisplayUpdate = currentTime;
    
    // Variables for display are available but won't be logged every time
    // Only uncomment for debugging if needed
    /*
    USBSerial.println("\n=== AVAILABLE VARIABLES FOR DISPLAY ====");
    USBSerial.printf("current_mode: %s\n", currentMode.name.c_str());
    USBSerial.printf("wifi_status: %s\n", WiFiManager::isConnected() ? "Connected" : "Disconnected");
    USBSerial.printf("ip_address: %s\n", WiFiManager::getLocalIP().toString().c_str());
    USBSerial.printf("layer: %s\n", keyHandler ? keyHandler->getCurrentLayer().c_str() : "default");
    USBSerial.printf("macro_status: %s\n", (macroHandler && macroHandler->isExecuting()) ? "Running" : "Ready");
    USBSerial.printf("SSID: %s\n", WiFiManager::getSSID().c_str());
    USBSerial.printf("Is AP Mode: %s\n", WiFiManager::isAPMode() ? "Yes" : "No");
    USBSerial.println("========================================\n");
    */
    
    // Check if we need to clear a temporary message
    if (temporaryMessageActive) {
        checkTemporaryMessage();
        if (!temporaryMessageActive && activeMode != "") {
            // Switch to active mode after temporary message
            activateDisplayMode(activeMode);
        }
        return;
    }
    
    // Only update if display is initialized
    if (!display) {
        USBSerial.println("ERROR: Display object is NULL");
        return;
    }
    
    if (!screenInitialized) {
        USBSerial.println("ERROR: Screen not initialized");
        return;
    }
    
    // Clear the screen for redraw without logging it every time
    display->fillScreen(ST77XX_BLACK);
    
    // Draw all elements from current mode
    // USBSerial.printf("Drawing %d elements from current mode\n", currentMode.elements.size());
    
    if (currentMode.elements.empty()) {
        USBSerial.println("WARNING: No elements to draw!");
    }
    
    for (const auto& element : currentMode.elements) {
        // Removed repetitive element drawing logs
        switch (element.type) {
            case ELEMENT_TEXT: {
                String processedText = element.text;
                // USBSerial.printf("Processing text: %s\n", element.text.c_str());
                
                // Check for and replace variables without logging each replacement
                if (element.text.indexOf("{current_mode}") >= 0) {
                    processedText.replace("{current_mode}", currentMode.name);
                }
                
                if (element.text.indexOf("{wifi_status}") >= 0) {
                    String wifiStatus = WiFiManager::isConnected() ? "Connected" : "Disconnected";
                    processedText.replace("{wifi_status}", wifiStatus);
                }
                
                if (element.text.indexOf("{ip_address}") >= 0) {
                    String ipAddress = WiFiManager::getLocalIP().toString();
                    processedText.replace("{ip_address}", ipAddress);
                }
                
                if (element.text.indexOf("{layer}") >= 0) {
                    String layer = keyHandler ? keyHandler->getCurrentLayer() : "default";
                    processedText.replace("{layer}", layer);
                }
                
                if (element.text.indexOf("{macro_status}") >= 0) {
                    String macroStatus = (macroHandler && macroHandler->isExecuting()) ? "Running" : "Ready";
                    processedText.replace("{macro_status}", macroStatus);
                }
                
                // Removed "After processing" log
                
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
    USBSerial.printf("Activating display mode: %s\n", modeName.c_str());
    
    if (displayModes.count(modeName) > 0) {
        USBSerial.printf("Mode found in displayModes map\n");
        USBSerial.printf("Original mode has %d elements\n", displayModes[modeName].elements.size());
        
        // Make a deep copy of the mode
        currentMode = displayModes[modeName];
        activeMode = modeName;
        
        USBSerial.printf("Activated display mode: %s\n", modeName.c_str());
        USBSerial.printf("Mode has %d elements\n", currentMode.elements.size());
        
        // Debug the elements
        for (const auto& element : currentMode.elements) {
            USBSerial.printf("Element type: %d, x: %d, y: %d\n", element.type, element.x, element.y);
            if (element.type == ELEMENT_TEXT) {
                USBSerial.printf("Text content: %s\n", element.text.c_str());
            }
        }
    } else {
        USBSerial.printf("Display mode not found: %s\n", modeName.c_str());
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
    if (!display) return;
    
    USBSerial.printf("Display: %s\n", message);
    
    temporaryMessageActive = true;
    temporaryMessageTimeout = millis() + duration;
    
    // Clear the display
    display->fillScreen(ST77XX_BLACK);
    
    // Print the message
    display->setTextSize(2);
    display->setTextColor(ST77XX_WHITE);
    display->setCursor(10, 10);
    display->println("MacroPad");
    
    display->drawLine(10, 35, 230, 35, ST77XX_BLUE);
    
    // Display multiline message with word wrapping
    display->setTextSize(1);
    String msg = message;
    int yPos = 50;
    int maxWidth = 220;
    int charWidth = 6; // Approximate width of each character
    int spaceWidth = 6;
    int lineHeight = 15;
    
    String word = "";
    int lineWidth = 0;
    
    display->setCursor(10, yPos);
    
    for (int i = 0; i < msg.length(); i++) {
        char c = msg[i];
        if (c == '\n') {
            if (word.length() > 0) {
                display->print(word);
                word = "";
            }
            yPos += lineHeight;
            display->setCursor(10, yPos);
            lineWidth = 0;
        } else if (c == ' ') {
            if (lineWidth + word.length() * charWidth > maxWidth) {
                yPos += lineHeight;
                display->setCursor(10, yPos);
                lineWidth = 0;
            }
            word += c;
        } else {
            word += c;
        }
    }
    display->print(word);
}

void checkTemporaryMessage() {
    if (!temporaryMessageActive) return;
    
    if (millis() >= temporaryMessageTimeout) {
        temporaryMessageActive = false;
        display->fillScreen(ST77XX_BLACK);
        // Don't reset screenInitialized - this was causing the display to stop working
    }
}

void displayWiFiInfo(bool isAPMode, const String& ipAddress, const String& ssid) {
    if (!display) return;
    
    // Cache the WiFi info to prevent redrawing if nothing changed
    static String lastIPAddress = "";
    static String lastSSID = "";
    static bool lastAPMode = false;
    
    // Only update if information has actually changed
    if (lastIPAddress == ipAddress && lastSSID == ssid && lastAPMode == isAPMode) {
        return;
    }
    
    // Update cache
    lastIPAddress = ipAddress;
    lastSSID = ssid;
    lastAPMode = isAPMode;
    
    // Clear WiFi info area
    display->fillRect(0, 145, 240, 80, ST77XX_BLACK);
    
    // Display connection mode
    display->setTextSize(1);
    display->setCursor(10, 145);
    display->setTextColor(ST77XX_CYAN);
    display->println(isAPMode ? "Mode: Access Point" : "Mode: Station");
    
    // Display SSID
    display->setCursor(10, 155);
    display->setTextColor(ST77XX_WHITE);
    display->println("SSID: " + ssid);
    
    // Display IP Address
    display->setCursor(10, 165);
    display->println("IP: " + ipAddress);
    
    // Display instructions
    display->setCursor(10, 185);
    display->setTextColor(ST77XX_GREEN);
    display->println("Visit this IP in a browser");
    display->setCursor(10, 200);
    display->println("to configure your MacroPad");
}
