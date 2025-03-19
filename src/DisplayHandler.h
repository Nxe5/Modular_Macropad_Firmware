// DisplayHandler.h
#ifndef DISPLAY_HANDLER_H
#define DISPLAY_HANDLER_H

#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include "EncoderHandler.h" // Include this for EncoderType and EncoderConfig

// Pin definitions for the display
#define TFT_CS    37   // Chip select pin
#define TFT_DC    39   // Data/command pin, using miso pin
#define TFT_SCLK  40   // Serial clock pin
#define TFT_MOSI  38   // Master out, slave in pin also known as DIN
#define TFT_RST   41   // Reset pin


// Function declarations
void initializeDisplay();
Adafruit_ST7789* getDisplay();
void printText(const char* text, int x, int y, uint16_t color, uint8_t size);
void updateDisplay();
void toggleMode();
void handleEncoder(int encoderPosition);

// Function to show a temporary message on the display
void showTemporaryMessage(const char* message, uint32_t duration = 3000);
void checkTemporaryMessage(); // Check if temporary message should be cleared

// Function to display WiFi information
void displayWiFiInfo(bool isAPMode, const String& ipAddress, const String& ssid);

// NOTE: EncoderType and EncoderConfig are now included from EncoderHandler.h

#endif // DISPLAY_HANDLER_H
