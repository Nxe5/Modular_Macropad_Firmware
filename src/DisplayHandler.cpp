#include "DisplayHandler.h"
#include <USBCDC.h>
#include "WiFiManager.h"
#include <WiFi.h>
#include "KeyHandler.h"
#include "MacroHandler.h"

extern USBCDC USBSerial;
extern KeyHandler* keyHandler;
extern MacroHandler* macroHandler;

static Adafruit_ST7789* display = nullptr;
static uint32_t temporaryMessageTimeout = 0;
static bool temporaryMessageActive = false;
static String lastNormalContent = "";
static unsigned long lastDisplayUpdate = 0;
static const unsigned long DISPLAY_UPDATE_INTERVAL = 250; // Rate limit to 4 updates per second
static bool screenInitialized = false; // Track if the base screen has been initialized

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
    display->init(240, 280, SPI_MODE0); // Use 280 height as in working example
    
    USBSerial.println("Display initialized");
    
    // Set rotation and clear screen
    display->setRotation(1); // Use rotation 1 as in working example
    display->fillScreen(ST77XX_BLACK);
    
    USBSerial.println("Screen cleared");
    
    // Draw a test pattern to verify display is working
    display->fillRect(0, 0, 240, 40, ST77XX_RED);
    display->fillRect(0, 40, 240, 40, ST77XX_GREEN);
    display->fillRect(0, 80, 240, 40, ST77XX_BLUE);
    
    display->setTextColor(ST77XX_WHITE);
    display->setTextSize(3);
    display->setCursor(40, 140);
    display->println("Hello");
    display->setCursor(40, 180);
    display->println("Andrew!");
    
    USBSerial.println("Test pattern drawn - display ready");
    
    // Skip temporary message for now
    // showTemporaryMessage("MacroPad Starting...", 2000);
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

void updateDisplay() {
    // Rate limit display updates to prevent flickering
    unsigned long currentTime = millis();
    if (currentTime - lastDisplayUpdate < DISPLAY_UPDATE_INTERVAL) {
        return;
    }
    lastDisplayUpdate = currentTime;
    
    // Check if we need to clear a temporary message
    if (temporaryMessageActive) {
        checkTemporaryMessage();
        return; // Don't update display when showing temporary message
    }
    
    // Only initialize the screen once at startup or after temp message
    if (!display) return;
    
    // Only draw background and static elements when needed
    if (!screenInitialized) {
        display->fillScreen(ST77XX_BLACK);
        
        // Draw title - static
        display->setCursor(10, 10);
        display->setTextSize(2);
        display->setTextColor(ST77XX_WHITE);
        display->println("MacroPad");
        
        display->drawLine(10, 35, 230, 35, ST77XX_BLUE);
        display->drawLine(10, 95, 230, 95, ST77XX_BLUE);
        
        screenInitialized = true;
    }
    
    // Update dynamic content by clearing and redrawing just those sections
    
    // Clear and update layer info
    display->fillRect(10, 45, 220, 15, ST77XX_BLACK);
    display->setCursor(10, 45);
    display->setTextSize(1);
    display->setTextColor(ST77XX_CYAN);
    String activeLayer = "Default";
    if (keyHandler) {
        activeLayer = keyHandler->getCurrentLayer();
    }
    display->print("Active Layer: " + activeLayer);
    
    // Clear and update macro status
    display->fillRect(10, 60, 220, 15, ST77XX_BLACK);
    display->setCursor(10, 60);
    display->setTextColor(ST77XX_GREEN);
    if (macroHandler && macroHandler->isExecuting()) {
        display->print("Macro: Running");
    } else {
        display->print("Macro: Ready");
    }
    
    // Clear and update mode
    display->fillRect(10, 75, 220, 15, ST77XX_BLACK);
    display->setCursor(10, 75);
    display->setTextColor(ST77XX_YELLOW);
    display->println("Mode: Normal");
    
    // Show WiFi information only when changed
    static uint32_t lastWifiUpdate = 0;
    if (currentTime - lastWifiUpdate > 60000) { // Update WiFi info at most once per minute
        bool isAPMode = WiFiManager::isAPMode();
        String ipAddress = isAPMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
        String ssid = WiFiManager::getSSID();
        
        // Display the WiFi info
        displayWiFiInfo(isAPMode, ipAddress, ssid);
        lastWifiUpdate = currentTime;
    }
}

void toggleMode() {
    // Add mode toggling logic
}

void handleEncoder(int encoderPosition) {
    // Update display based on encoder position
}

// Function to show a temporary message on the display
void showTemporaryMessage(const char* message, uint32_t duration) {
    if (!display) return;
    
    USBSerial.printf("Display: %s\n", message);
    
    temporaryMessageActive = true;
    
    // Clear the display
    display->fillScreen(ST77XX_BLACK);
    
    // Print the new message
    display->setCursor(10, 10);
    display->setTextSize(2);
    display->setTextColor(ST77XX_WHITE);
    display->println("MacroPad");
    
    display->drawLine(10, 35, 230, 35, ST77XX_BLUE);
    
    // Display multiline message with word wrapping
    display->setCursor(10, 50);
    display->setTextSize(1);
    
    // Simple word wrap for the message
    String msgStr = message;
    int lineWidth = 0;
    int spaceWidth = 6; // Approximate width of space
    int charWidth = 6;  // Approximate width of characters
    int lineHeight = 10; // Line height
    int yPos = 50;
    int maxWidth = 220;
    
    String word = "";
    for (unsigned int i = 0; i < msgStr.length(); i++) {
        char c = msgStr[i];
        
        if (c == '\n') {
            // Direct newline
            display->print(word);
            word = "";
            lineWidth = 0;
            yPos += lineHeight;
            display->setCursor(10, yPos);
            continue;
        }
        
        if (c == ' ') {
            // Check if adding this word would exceed the line width
            if (lineWidth + word.length() * charWidth > maxWidth) {
                yPos += lineHeight;
                display->setCursor(10, yPos);
                lineWidth = 0;
            }
            
            display->print(word + " ");
            lineWidth += word.length() * charWidth + spaceWidth;
            word = "";
        } else {
            word += c;
        }
    }
    
    // Print the last word
    if (word.length() > 0) {
        if (lineWidth + word.length() * charWidth > maxWidth) {
            yPos += lineHeight;
            display->setCursor(10, yPos);
        }
        display->print(word);
    }
    
    // Set the timeout for when to clear this message
    temporaryMessageTimeout = millis() + duration;
}

// Check if it's time to clear the temporary message
void checkTemporaryMessage() {
    if (temporaryMessageActive && millis() > temporaryMessageTimeout) {
        // Clear the temporary message flag
        temporaryMessageActive = false;
        
        // Force a complete redraw on next update
        screenInitialized = false; // Reset screen initialization flag to force redraw
        lastDisplayUpdate = 0; // Force next updateDisplay to run
    }
}

// Add a function to display WiFi information
void displayWiFiInfo(bool isAPMode, const String& ipAddress, const String& ssid) {
    if (!display) return;
    
    // Cache the WiFi info to prevent redrawing if nothing changed
    static String lastIPAddress = "";
    static String lastSSID = "";
    static bool lastAPMode = false;
    
    // Only update if information has actually changed
    bool ipChanged = (lastIPAddress != ipAddress);
    bool ssidChanged = (lastSSID != ssid);
    bool modeChanged = (lastAPMode != isAPMode);
    
    if (!ipChanged && !ssidChanged && !modeChanged) {
        return; // Nothing changed, don't redraw
    }
    
    // Update cached values
    lastIPAddress = ipAddress;
    lastSSID = ssid;
    lastAPMode = isAPMode;
    
    // Only clear and redraw the relevant parts that changed
    
    // Always draw the header (first time or if anything changed)
    display->fillRect(0, 120, 240, 15, ST77XX_BLACK);
    display->setCursor(10, 120);
    display->setTextSize(1);
    display->setTextColor(ST77XX_YELLOW);
    display->println("WiFi Status:");
    
    // Update mode and SSID if changed
    if (modeChanged || ssidChanged) {
        display->fillRect(0, 135, 240, 30, ST77XX_BLACK);
        display->setCursor(10, 135);
        display->setTextColor(ST77XX_WHITE);
        
        if (isAPMode) {
            display->println("Mode: Access Point");
            display->setCursor(10, 150);
            display->println("SSID: " + ssid);
        } else {
            display->println("Mode: Connected to");
            display->setCursor(10, 150);
            display->println(ssid);
        }
    }
    
    // Update IP if changed
    if (ipChanged) {
        display->fillRect(0, 165, 240, 15, ST77XX_BLACK);
        display->setCursor(10, 165);
        display->setTextColor(ST77XX_WHITE);
        display->println("IP: " + ipAddress);
    }
    
    // These instructions are static, but update if anything else changed
    if (ipChanged || modeChanged || ssidChanged) {
        display->fillRect(0, 185, 240, 30, ST77XX_BLACK);
        display->setCursor(10, 185);
        display->setTextColor(ST77XX_GREEN);
        display->println("Visit this IP in a browser");
        display->setCursor(10, 200);
        display->println("to configure your MacroPad");
    }
}