// DisplayHandler.h
#ifndef DISPLAY_HANDLER_H
#define DISPLAY_HANDLER_H

#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include <ArduinoJson.h>
#include "ConfigManager.h" // Include this for DisplayElement and DisplayMode structs

// Pin definitions for the display
#define TFT_CS    37   // Chip select pin
#define TFT_DC    39   // Data/command pin, using miso pin
#define TFT_SCLK  40   // Serial clock pin
#define TFT_MOSI  38   // Master out, slave in pin also known as DIN
#define TFT_RST   41   // Reset pin

// Display configuration constants
#define DISPLAY_CONFIG_PATH "/config/display.json"
#define DISPLAY_UPDATE_INTERVAL 1000 // Default update interval in ms

// Display element types
enum DisplayElementType {
    ELEMENT_TEXT,
    ELEMENT_LINE,
    ELEMENT_RECT,
    ELEMENT_CIRCLE
};

// Function declarations
void initializeDisplay();
Adafruit_ST7789* getDisplay();
void printText(const char* text, int x, int y, uint16_t color, uint8_t size);
void drawTestPattern();
void updateDisplay();
void loadDisplayConfig();
void activateDisplayMode(const String& modeName);
void handleEncoder(int encoderPosition);

// Function to show a temporary message on the display
void showTemporaryMessage(const char* message, uint32_t duration = 3000);
void checkTemporaryMessage(); // Check if temporary message should be cleared

// Function to display WiFi information
void displayWiFiInfo(bool isAPMode, const String& ipAddress, const String& ssid);

// Getters for display state
String getCurrentMode();
bool isTemporaryMessageActive();

#endif // DISPLAY_HANDLER_H
