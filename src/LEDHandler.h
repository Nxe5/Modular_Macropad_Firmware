// LEDHandler.h

#pragma once

#include <Adafruit_NeoPixel.h>
#include <ArduinoJson.h>
#include <map>
#include <vector>
#include <string>
#include <USBCDC.h>
#include "JsonUtils.h" // Include the centralized JSON utility functions

// Define to enable power monitoring (requires pin 34 connected to VBUS via voltage divider)
// #define ENABLE_POWER_MONITORING

// Default LED PIN and count (used if config file is missing)
#define DEFAULT_LED_PIN 38
#define DEFAULT_NUM_LEDS 18

// LED Modes
#define LED_MODE_STATIC    0 // Static color
#define LED_MODE_ANIMATION 1 // Part of an animation
#define LED_MODE_BUTTON    2 // Button-controlled mode
#define LED_MODE_PULSE     3 // Pulse mode
#define LED_MODE_RAINBOW   4 // Rainbow mode
#define LED_MODE_BREATHING 5 // Breathing mode

// Animation Modes
#define LED_ANIM_RAINBOW     0
#define LED_ANIM_CHASE       1
#define LED_ANIM_BREATH      2
#define LED_ANIM_ALTERNATING 3

// LED Configuration structure
struct LEDConfig {
    String id = "";
    uint8_t streamAddress = 0;
    String buttonId = "";
    uint8_t r = 0;                // Default red value
    uint8_t g = 255;              // Default green value
    uint8_t b = 0;                // Default blue value
    uint8_t pressedR = 255;        // Red value when pressed
    uint8_t pressedG = 255;        // Green value when pressed
    uint8_t pressedB = 255;        // Blue value when pressed
    uint8_t brightness = 30;       // LED brightness (0-255)
    uint8_t mode = LED_MODE_BUTTON; // LED mode (static, animated, etc.)
    bool needsUpdate = true;        // Flag for batch updates
    bool isActive = true;           // Whether the LED is currently active
};

// Button-LED mapping structure
struct ButtonLEDMapping {
    String buttonId;
    std::vector<uint8_t> ledIndices; // Multiple LED indices for one button
    uint8_t defaultColor[3] = {0, 255, 0};       // Default color when button is not pressed (r,g,b)
    uint8_t pressedColor[3] = {255, 255, 255};   // Color when button is pressed (r,g,b)
    
    // Constructor for default initialization
    ButtonLEDMapping() {
        defaultColor[0] = 0;
        defaultColor[1] = 255;
        defaultColor[2] = 0;
        pressedColor[0] = 255;
        pressedColor[1] = 255;
        pressedColor[2] = 255;
    }
};

// Basic functions
void initializeLED(uint8_t numLEDsToInit = 0, uint8_t ledPin = 7, uint8_t brightness = 30);
void setLEDColor(uint8_t index, uint8_t r, uint8_t g, uint8_t b);
void setLEDColorWithBrightness(uint8_t index, uint8_t r, uint8_t g, uint8_t b, uint8_t brightness, bool isPressedState = false);
void setLEDColorHex(uint8_t index, uint32_t hexColor);
void setAllLEDs(uint8_t r, uint8_t g, uint8_t b);
void clearAllLEDs();
void setBrightness(uint8_t brightness);
void setGlobalBrightness(uint8_t brightness);

// Button-LED sync function
void syncLEDsWithButtons(const char* buttonId, bool pressed);

// Animation functions
void startAnimation(uint8_t mode, uint16_t speed);
void stopAnimation();
void updateAnimation();
void animateRainbow();
void animateChase();
void animateBreath();
void animateAlternating();
uint32_t wheel(byte wheelPos);

// Update function for main loop
void updateLEDs();

// Configuration management
String getLEDConfigJson();
bool updateLEDConfigFromJson(const String& json);
bool saveLEDConfig();
String createDefaultLEDConfig();
bool saveDefaultLEDConfig();
void cleanupLED();

// Power management functions
#ifdef ENABLE_POWER_MONITORING
void checkPowerStatus();
void handleLowPower();
#endif

// External variables
extern Adafruit_NeoPixel* strip;
extern LEDConfig* ledConfigs;
extern uint8_t numLEDs;
extern bool animationActive;
extern uint8_t animationMode;
extern uint16_t animationSpeed;
extern std::map<String, ButtonLEDMapping> buttonLEDMap;

// Function declarations
void updateLED(uint8_t index);
void updateAllLEDs();
void setLEDButtonState(const String& buttonId, bool pressed);
void processLEDConfig(JsonObject& led);