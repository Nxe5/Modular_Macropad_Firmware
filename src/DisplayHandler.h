// DisplayHandler.h
#ifndef DISPLAY_HANDLER_H
#define DISPLAY_HANDLER_H

#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>

// Pin definitions for the display
#define TFT_CS    37   // Chip select pin
#define TFT_DC    39   // Data/command pin
#define TFT_SCLK  40   // Serial clock pin
#define TFT_MOSI  38   // Master out, slave in pin
#define TFT_RST   15   // Reset pin


// Function declarations
void initializeDisplay();
Adafruit_ST7789* getDisplay();
void printText(const char* text, int x, int y, uint16_t color, uint8_t size);
void updateDisplay();
void toggleMode();
void handleEncoder(int encoderPosition);

// Encoder Types
enum EncoderType {
    ENCODER_TYPE_MECHANICAL,  // Standard rotary encoder
    ENCODER_TYPE_AS5600       // Magnetic encoder
};

// Configuration for each encoder
struct EncoderConfig {    
    // Pins for different encoder types
    uint8_t pinA = 0;        // Mechanical encoder or I2C SDA for AS5600
    uint8_t pinB = 0;        // Mechanical encoder or I2C SCL for AS5600
    
    // AS5600 specific configuration
    uint16_t zeroPosition = 0;  // Calibration zero point
    uint16_t steps = 4096;      // Total steps for AS5600 (12-bit)
    int8_t direction = 1;       // 1 or -1 to invert rotation
    
    // Detailed tracking
    long absolutePosition = 0;      // Current absolute position
    long lastReportedPosition = 0;  // Last reported position
    uint16_t lastRawPosition = 0;   // Last raw AS5600 position
    long lastMechanicalPosition = 0; // Last mechanical encoder position
};

#endif // DISPLAY_HANDLER_H
