// LEDHandler.h

#ifndef LED_HANDLER_H
#define LED_HANDLER_H

#include <Adafruit_NeoPixel.h>
#include <ArduinoJson.h>

// Default LED PIN and count (used if config file is missing)
#define DEFAULT_LED_PIN 38
#define DEFAULT_NUM_LEDS 18

// LED Modes
#define LED_MODE_STATIC    0 // Static color
#define LED_MODE_ANIMATION 1 // Part of an animation

// Animation Modes
#define LED_ANIM_RAINBOW     0
#define LED_ANIM_CHASE       1
#define LED_ANIM_BREATH      2
#define LED_ANIM_ALTERNATING 3

// LED Configuration structure
struct LEDConfig {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t brightness;
    uint8_t mode;
};

// Basic functions
void initializeLED();
void setLEDColor(uint8_t index, uint8_t r, uint8_t g, uint8_t b);
void setLEDColorWithBrightness(uint8_t index, uint8_t r, uint8_t g, uint8_t b, uint8_t brightness);
void setLEDColorHex(uint8_t index, uint32_t hexColor);
void setAllLEDs(uint8_t r, uint8_t g, uint8_t b);
void clearAllLEDs();
void setBrightness(uint8_t brightness);

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
void cleanupLED();

// External variables
extern Adafruit_NeoPixel* strip;
extern LEDConfig* ledConfigs;
extern uint8_t numLEDs;
extern bool animationActive;
extern uint8_t animationMode;
extern uint16_t animationSpeed;

#endif