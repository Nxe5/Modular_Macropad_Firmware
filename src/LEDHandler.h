// LEDHandler.h

#ifndef LED_HANDLER_H
#define LED_HANDLER_H

#include <Adafruit_NeoPixel.h>
#include <ArduinoJson.h>
#include <map>
#include <vector>
#include <string>

// Define to enable power monitoring (requires pin 34 connected to VBUS via voltage divider)
// #define ENABLE_POWER_MONITORING

// Default LED PIN and count (used if config file is missing)
#define DEFAULT_LED_PIN 38
#define DEFAULT_NUM_LEDS 18

// LED Modes
#define LED_MODE_STATIC    0 // Static color
#define LED_MODE_ANIMATION 1 // Part of an animation
#define LED_MODE_BUTTON    2 // Button-controlled mode

// Animation Modes
#define LED_ANIM_RAINBOW     0
#define LED_ANIM_CHASE       1
#define LED_ANIM_BREATH      2
#define LED_ANIM_ALTERNATING 3

// LED Configuration structure
struct LEDConfig {
    uint8_t r;                // Default red value
    uint8_t g;                // Default green value
    uint8_t b;                // Default blue value
    uint8_t brightness;       // LED brightness (0-255)
    uint8_t mode;             // LED mode (static, animated, etc.)
    String buttonId;          // ID of button that controls this LED
    uint8_t pressedR;         // Red value when pressed
    uint8_t pressedG;         // Green value when pressed
    uint8_t pressedB;         // Blue value when pressed
    bool needsUpdate;         // Flag for batch updates
    bool isActive;            // Whether the LED is currently active
};

// Button-LED mapping structure
struct ButtonLEDMapping {
    String buttonId;
    std::vector<uint8_t> ledIndices; // Multiple LED indices for one button
    uint8_t defaultColor[3];         // RGB
    uint8_t pressedColor[3];         // RGB when button is pressed
    
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
void initializeLED();
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
void createDefaultLEDConfig();
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

#endif