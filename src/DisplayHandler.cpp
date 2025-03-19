#include "DisplayHandler.h"
#include <USBCDC.h>
#include "WiFiManager.h"
#include <WiFi.h>

extern USBCDC USBSerial;

static Adafruit_ST7789* display = nullptr;
static uint32_t temporaryMessageTimeout = 0;
static bool temporaryMessageActive = false;
static String lastNormalContent = "";

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
    display->println("World!");
    
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
    // For now, just return to leave the test pattern visible
    USBSerial.println("updateDisplay called but skipped to preserve test pattern");
    return;
    
    // Original code temporarily disabled
    /*
    // Check if we need to clear a temporary message
    if (temporaryMessageActive) {
        checkTemporaryMessage();
        return; // Don't update display when showing temporary message
    }
    
    // Clear the display and draw standard interface
    if (!display) return;
    
    display->fillScreen(ST77XX_BLACK);
    
    // Draw title
    display->setCursor(10, 10);
    display->setTextSize(2);
    display->setTextColor(ST77XX_WHITE);
    display->println("MacroPad");
    
    display->drawLine(10, 35, 230, 35, ST77XX_BLUE);
    
    // Draw status
    display->setCursor(10, 40);
    display->setTextSize(1);
    display->setTextColor(ST77XX_GREEN);
    display->println("Ready");
    
    // Show WiFi information
    bool isAPMode = WiFiManager::isAPMode();
    String ipAddress = isAPMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
    String ssid = WiFiManager::getSSID();
    
    // Display the WiFi info
    displayWiFiInfo(isAPMode, ipAddress, ssid);
    */
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
    
    // If this is the first temporary message, save the current display content
    if (!temporaryMessageActive) {
        temporaryMessageActive = true;
        // In a more advanced implementation, you might want to save the current screen content
    }
    
    // Clear the display
    display->fillScreen(ST77XX_BLACK);
    
    // Print the new message
    display->setCursor(10, 10);
    display->setTextSize(2);
    display->setTextColor(ST77XX_WHITE);
    display->println("MacroPad");
    
    display->drawLine(10, 35, 230, 35, ST77XX_BLUE);
    
    display->setCursor(10, 50);
    display->setTextSize(1);
    display->println(message);
    
    // Set the timeout for when to clear this message
    temporaryMessageTimeout = millis() + duration;
}

// Check if it's time to clear the temporary message
void checkTemporaryMessage() {
    if (temporaryMessageActive && millis() > temporaryMessageTimeout) {
        // Clear the temporary message
        temporaryMessageActive = false;
        display->fillScreen(ST77XX_BLACK);
        
        // Restore the previous content (in a more advanced implementation)
        // For now, just show a default screen
        display->setCursor(10, 10);
        display->setTextSize(2);
        display->setTextColor(ST77XX_WHITE);
        display->println("MacroPad");
        
        display->setCursor(10, 40);
        display->setTextSize(1);
        display->setTextColor(ST77XX_GREEN);
        display->println("Ready");
        
        // Show WiFi information
        bool isAPMode = WiFiManager::isAPMode();
        String ipAddress = isAPMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
        String ssid = WiFiManager::getSSID();
        
        // Display the WiFi info
        displayWiFiInfo(isAPMode, ipAddress, ssid);
    }
}

// Add a function to display WiFi information
void displayWiFiInfo(bool isAPMode, const String& ipAddress, const String& ssid) {
    if (!display) return;
    
    // Clear the bottom part of screen
    display->fillRect(0, 120, 240, 120, ST77XX_BLACK);
    
    display->setCursor(10, 120);
    display->setTextSize(1);
    display->setTextColor(ST77XX_YELLOW);
    display->println("WiFi Status:");
    
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
    
    display->setCursor(10, 165);
    display->println("IP: " + ipAddress);
    
    display->setCursor(10, 185);
    display->setTextColor(ST77XX_GREEN);
    display->println("Visit this IP in a browser");
    display->setCursor(10, 200);
    display->println("to configure your MacroPad");
}