// LEDHanler.cpp

#include "LEDHandler.h"
#include "ModuleSetup.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <algorithm> // For std::min
#include <Arduino.h>
#include <USBCDC.h>

#ifdef ENABLE_POWER_MONITORING
#include <driver/adc.h>
#endif

extern USBCDC USBSerial;
extern size_t estimateJsonBufferSize(const String& jsonString, float safetyFactor);

// LED strip will be initialized dynamically based on config
Adafruit_NeoPixel* strip = nullptr;

// Array to store LED configurations - size will be determined from config
LEDConfig* ledConfigs = nullptr;
uint8_t numLEDs = 0;

// Button-LED mapping
std::map<String, ButtonLEDMapping> buttonLEDMap;

// Animation variables
bool animationActive = false;
uint8_t animationMode = 0;
uint32_t lastAnimationUpdate = 0;
uint16_t animationSpeed = 100; // ms between animation frames

#ifdef ENABLE_POWER_MONITORING
// Power management
uint32_t lastPowerCheck = 0;
const uint32_t POWER_CHECK_INTERVAL = 1000; // Check power every second
const float MIN_VOLTAGE = 4.5; // Minimum safe USB voltage
bool lowPowerMode = false;
#endif

// Forward declaration of helper functions
static String readJsonFile(const char* filePath);

void initializeLED(uint8_t numLEDsToInit, uint8_t ledPin, uint8_t brightness) {
    try {
        #ifdef ENABLE_POWER_MONITORING
        // Setup ADC for voltage monitoring (Pin 34 - ADC1 Channel 6)
        adc1_config_width(ADC_WIDTH_BIT_12);
        adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_11);
        #endif
        
        // Use parameters if provided, otherwise defaults
        if (numLEDsToInit > 0) {
            numLEDs = numLEDsToInit;
        } else {
            // Try to load LED configuration
            String ledJson = readJsonFile("/config/leds.json");
            if (ledJson.isEmpty()) {
                USBSerial.println("LED config not found, creating defaults");
                
                // Use defaults from header
                numLEDs = DEFAULT_NUM_LEDS;
                strip = new Adafruit_NeoPixel(numLEDs, ledPin, NEO_GRB + NEO_KHZ800);
                
                // Create and save default configuration
                createDefaultLEDConfig();
            } else {
                // Parse LED configuration
                DynamicJsonDocument doc(8192);
                DeserializationError error = deserializeJson(doc, ledJson);
                
                if (error) {
                    USBSerial.print("Error parsing LED config: ");
                    USBSerial.println(error.c_str());
                    
                    // Use defaults
                    numLEDs = DEFAULT_NUM_LEDS;
                    strip = new Adafruit_NeoPixel(numLEDs, ledPin, NEO_GRB + NEO_KHZ800);
                    createDefaultLEDConfig();
                } else {
                    // Get LED count and pin from config
                    numLEDs = doc["leds"]["config"].size();
                    
                    // Use provided pin or configuration pin or default
                    if (ledPin == 7) { // If default was passed
                        ledPin = doc["leds"]["pin"] | DEFAULT_LED_PIN; // Use from config or default
                    }
                    
                    USBSerial.printf("Initializing %d LEDs on pin %d\n", numLEDs, ledPin);
                    strip = new Adafruit_NeoPixel(numLEDs, ledPin, NEO_GRB + NEO_KHZ800);
                    
                    // Get brightness from parameters or config
                    if (brightness == 30) { // If default was passed
                        brightness = doc["leds"]["brightness"] | 30; // Use from config or default
                    }
                    
                    // Initialize NeoPixel strip
                    strip->begin();
                    setGlobalBrightness(brightness); // Use power-aware brightness setting
                    strip->clear();
                    strip->show();
                    
                    // Create LED configs array
                    ledConfigs = new LEDConfig[numLEDs];
                    
                    // Clear the button LED mapping first
                    buttonLEDMap.clear();
                    
                    // Apply saved LED colors if present
                    if (doc["leds"].containsKey("config")) {
                        JsonArray ledsConfig = doc["leds"]["config"];
                        for (JsonObject led : ledsConfig) {
                            if (led.containsKey("stream_address")) {
                                uint8_t index = led["stream_address"];
                                
                                if (index < numLEDs) {
                                    // Store the LED configuration
                                    ledConfigs[index].r = led["color"]["r"] | 0;
                                    ledConfigs[index].g = led["color"]["g"] | 255;
                                    ledConfigs[index].b = led["color"]["b"] | 0;
                                    ledConfigs[index].brightness = led["brightness"] | 30; // Lower default brightness
                                    ledConfigs[index].mode = led["mode"] | LED_MODE_STATIC;
                                    ledConfigs[index].needsUpdate = true;
                                    ledConfigs[index].isActive = false;
                                    
                                    // Handle pressed colors if available
                                    if (led.containsKey("pressed_color")) {
                                        ledConfigs[index].pressedR = led["pressed_color"]["r"] | 255;
                                        ledConfigs[index].pressedG = led["pressed_color"]["g"] | 255;
                                        ledConfigs[index].pressedB = led["pressed_color"]["b"] | 255;
                                    } else {
                                        // Default to white for pressed color
                                        ledConfigs[index].pressedR = 255;
                                        ledConfigs[index].pressedG = 255;
                                        ledConfigs[index].pressedB = 255;
                                    }
                                    
                                    // Handle button mapping if available
                                    if (led.containsKey("button_id")) {
                                        String buttonId = led["button_id"].as<String>();
                                        ledConfigs[index].buttonId = buttonId;
                                        
                                        // Add this LED to the button mapping
                                        auto it = buttonLEDMap.find(buttonId);
                                        if (it != buttonLEDMap.end()) {
                                            // Button already exists, add this LED to its list
                                            it->second.ledIndices.push_back(index);
                                        } else {
                                            // Create new button mapping
                                            ButtonLEDMapping mapping;
                                            mapping.buttonId = buttonId;
                                            mapping.ledIndices.push_back(index);
                                            
                                            // Default colors
                                            mapping.defaultColor[0] = led["color"]["r"] | 0;
                                            mapping.defaultColor[1] = led["color"]["g"] | 255;
                                            mapping.defaultColor[2] = led["color"]["b"] | 0;
                                            
                                            // Pressed colors - default to white if not specified
                                            if (led.containsKey("pressed_color")) {
                                                mapping.pressedColor[0] = led["pressed_color"]["r"] | 255;
                                                mapping.pressedColor[1] = led["pressed_color"]["g"] | 255;
                                                mapping.pressedColor[2] = led["pressed_color"]["b"] | 255;
                                            }
                                            
                                            buttonLEDMap[buttonId] = mapping;
                                        }
                                    } else if (strncmp(led["id"].as<const char*>(), "led-", 4) == 0) {
                                        // Fallback for older configs - try to derive button ID from LED ID
                                        int ledNum = atoi(led["id"].as<const char*>() + 4);
                                        String buttonId = "button-" + String(ledNum);
                                        ledConfigs[index].buttonId = buttonId;
                                        
                                        // Create button-LED mapping
                                        auto it = buttonLEDMap.find(buttonId);
                                        if (it != buttonLEDMap.end()) {
                                            // Button already exists, add this LED
                                            it->second.ledIndices.push_back(index);
                                        } else {
                                            // Create new mapping
                                            ButtonLEDMapping mapping;
                                            mapping.buttonId = buttonId;
                                            mapping.ledIndices.push_back(index);
                                            mapping.defaultColor[0] = led["color"]["r"] | 0;
                                            mapping.defaultColor[1] = led["color"]["g"] | 255;
                                            mapping.defaultColor[2] = led["color"]["b"] | 0;
                                            
                                            buttonLEDMap[buttonId] = mapping;
                                        }
                                    }
                                    
                                    // Set the LED color
                                    float factor = ledConfigs[index].brightness / 255.0;
                                    strip->setPixelColor(index, strip->Color(
                                        ledConfigs[index].r * factor,
                                        ledConfigs[index].g * factor,
                                        ledConfigs[index].b * factor
                                    ));
                                }
                            }
                        }
                        
                        // Check for animation settings
                        if (doc["leds"]["animation"]["active"] | false) {
                            animationMode = doc["leds"]["animation"]["mode"] | 0;
                            animationSpeed = doc["leds"]["animation"]["speed"] | 100;
                            startAnimation(animationMode, animationSpeed);
                        } else {
                            strip->show(); // Show static colors
                        }
                    }
                }
            }
        }
        
        // If strip not created yet, create it now with provided parameters
        if (!strip) {
            USBSerial.printf("Creating new LED strip with %d LEDs on pin %d\n", numLEDs, ledPin);
            strip = new Adafruit_NeoPixel(numLEDs, ledPin, NEO_GRB + NEO_KHZ800);
            strip->begin();
            strip->setBrightness(brightness);
            strip->clear();
            strip->show();
        }
        
        // If LED configs not created yet, create them now
        if (ledConfigs == nullptr) {
            createDefaultLEDConfig();
        }
        
        USBSerial.println("LED Handler initialized with button mapping");
    } catch (const std::exception& e) {
        USBSerial.printf("LED initialization error: %s\n", e.what());
        // Fallback to safe defaults
        numLEDs = 1;
        strip = new Adafruit_NeoPixel(1, DEFAULT_LED_PIN, NEO_GRB + NEO_KHZ800);
        strip->begin();
        strip->setBrightness(20); // Very low brightness for safety
        strip->setPixelColor(0, strip->Color(255, 0, 0)); // Red error indicator
        strip->show();
    }
}

String createDefaultLEDConfig() {
    // Generate a default LED configuration
    USBSerial.println("Creating default LED configuration");
    
    // Create a default configuration with one LED
    DynamicJsonDocument doc(1024);
    
    // Set basic LED parameters
    doc["leds"]["pin"] = DEFAULT_LED_PIN;
    doc["leds"]["brightness"] = 30;
    
    // Create the animation settings object
    doc["leds"]["animation"]["active"] = false;
    doc["leds"]["animation"]["mode"] = 0;
    doc["leds"]["animation"]["speed"] = 100;
    
    // Create array for LED configurations
    JsonArray ledsConfig = doc["leds"].createNestedArray("config");
    
    // Add a default LED
    JsonObject led = ledsConfig.createNestedObject();
    led["id"] = "led-0";
    led["stream_address"] = 0;
    led["button_id"] = "button-0";
    
    JsonObject color = led.createNestedObject("color");
    color["r"] = 0;
    color["g"] = 255;
    color["b"] = 0;
    
    JsonObject pressedColor = led.createNestedObject("pressed_color");
    pressedColor["r"] = 255;
    pressedColor["g"] = 255;
    pressedColor["b"] = 255;
    
    led["brightness"] = 30;
    led["mode"] = 0;
    
    // Serialize to a string
    String output;
    serializeJsonPretty(doc, output);
    
    USBSerial.println("Created default LED configuration with green leds");
    USBSerial.println(output);
    
    return output;
}

bool saveDefaultLEDConfig() {
    DynamicJsonDocument doc(16384);
    
    // Create the basic structure
    doc["leds"]["pin"] = DEFAULT_LED_PIN;
    doc["leds"]["brightness"] = 30;
    doc["leds"]["animation"]["active"] = false;
    doc["leds"]["animation"]["mode"] = 0;
    doc["leds"]["animation"]["speed"] = 100;
    
    // Create the LED configurations array
    JsonArray ledsConfig = doc["leds"].createNestedArray("config");
    
    // Add each LED configuration
    for (int i = 0; i < numLEDs; i++) {
        JsonObject led = ledsConfig.createNestedObject();
        led["id"] = ledConfigs[i].id.isEmpty() ? "led-" + String(i) : ledConfigs[i].id;
        led["stream_address"] = ledConfigs[i].streamAddress;
        led["button_id"] = ledConfigs[i].buttonId;
        
        JsonObject color = led.createNestedObject("color");
        color["r"] = ledConfigs[i].r;
        color["g"] = ledConfigs[i].g;
        color["b"] = ledConfigs[i].b;
        
        JsonObject pressedColor = led.createNestedObject("pressed_color");
        pressedColor["r"] = ledConfigs[i].pressedR;
        pressedColor["g"] = ledConfigs[i].pressedG;
        pressedColor["b"] = ledConfigs[i].pressedB;
        
        led["brightness"] = ledConfigs[i].brightness;
        led["mode"] = ledConfigs[i].mode;
    }
    
    // Serialize to JSON
    String jsonString;
    serializeJson(doc, jsonString);
    
    // Save the full configuration to the default file
    File file = LittleFS.open("/config/defaults/leds.json", "w");
    if (!file) {
        USBSerial.println("Failed to open default LED config file for writing");
        return false;
    }
    
    if (file.print(jsonString) != jsonString.length()) {
        file.close();
        USBSerial.println("Failed to write to default LED config file");
        return false;
    }
    
    file.close();
    USBSerial.println("Default LED configuration saved");
    return true;
}

void setGlobalBrightness(uint8_t brightness) {
    if (!strip) return;
    
    // Limit maximum brightness based on number of LEDs for power management
    // Each LED can draw up to 60mA at full brightness
    // USB-C VBUS provides ~500mA, so we need to limit based on LED count
    uint8_t maxBrightness = std::min<uint8_t>(255u, (uint8_t)(500 / (numLEDs * 0.06)));
    
    // Apply the limit
    brightness = std::min<uint8_t>(brightness, maxBrightness);
    
    // Apply the brightness to the strip
    strip->setBrightness(brightness);
    strip->show();
    
    USBSerial.printf("LED brightness set to %d (max allowed: %d)\n", brightness, maxBrightness);
}

void setLEDColor(uint8_t index, uint8_t r, uint8_t g, uint8_t b) {
    if (!strip || index >= numLEDs) {
        USBSerial.printf("Invalid LED index: %d\n", index);
        return;
    }
    
    try {
        // Validate color values
        r = std::min<uint8_t>(r, 255u);
        g = std::min<uint8_t>(g, 255u);
        b = std::min<uint8_t>(b, 255u);
        
        // Update the configuration
        ledConfigs[index].r = r;
        ledConfigs[index].g = g;
        ledConfigs[index].b = b;
        ledConfigs[index].mode = LED_MODE_STATIC;
        ledConfigs[index].needsUpdate = true; // Mark for batch update
        
        // Set the actual LED color immediately if in direct mode
        strip->setPixelColor(index, strip->Color(r, g, b));
        strip->show();
    } catch (const std::exception& e) {
        USBSerial.printf("Error in setLEDColor: %s\n", e.what());
    }
}

void setLEDColorWithBrightness(uint8_t index, uint8_t r, uint8_t g, uint8_t b, uint8_t brightness, bool isPressedState) {
    if (!strip || index >= numLEDs) return;
    
    try {
        // Validate values
        r = std::min<uint8_t>(r, 255u);
        g = std::min<uint8_t>(g, 255u);
        b = std::min<uint8_t>(b, 255u);
        brightness = std::min<uint8_t>(brightness, 255u);
        
        // Store color values based on state
        if (isPressedState) {
            // Store as pressed color
            ledConfigs[index].pressedR = r;
            ledConfigs[index].pressedG = g;
            ledConfigs[index].pressedB = b;
        } else {
            // Store as default color
            ledConfigs[index].r = r;
            ledConfigs[index].g = g;
            ledConfigs[index].b = b;
        }
        
        // Always update brightness and flags
        ledConfigs[index].brightness = brightness;
        ledConfigs[index].mode = LED_MODE_STATIC;
        ledConfigs[index].needsUpdate = true;
        
        // Apply brightness factor to RGB values for this specific LED
        float factor = brightness / 255.0;
        uint8_t adjustedR = r * factor;
        uint8_t adjustedG = g * factor;
        uint8_t adjustedB = b * factor;
        
        strip->setPixelColor(index, strip->Color(adjustedR, adjustedG, adjustedB));
        strip->show();
    } catch (const std::exception& e) {
        USBSerial.printf("Error in setLEDColorWithBrightness: %s\n", e.what());
    }
}

void setLEDColorHex(uint8_t index, uint32_t hexColor) {
    if (!strip || index >= numLEDs) return;
    
    // Extract RGB components
    uint8_t r = (hexColor >> 16) & 0xFF;
    uint8_t g = (hexColor >> 8) & 0xFF;
    uint8_t b = hexColor & 0xFF;
    
    setLEDColor(index, r, g, b);
}

void setAllLEDs(uint8_t r, uint8_t g, uint8_t b) {
    if (!strip) return;
    
    for (int i = 0; i < numLEDs; i++) {
        // Update the configuration
        ledConfigs[i].r = r;
        ledConfigs[i].g = g;
        ledConfigs[i].b = b;
        ledConfigs[i].mode = LED_MODE_STATIC;
        
        strip->setPixelColor(i, strip->Color(r, g, b));
    }
    strip->show();
}

void clearAllLEDs() {
    setAllLEDs(0, 0, 0);
}

void setBrightness(uint8_t brightness) {
    if (!strip) return;
    
    strip->setBrightness(brightness);
    strip->show();
}

// Animation functions
void startAnimation(uint8_t mode, uint16_t speed) {
    if (!strip) return;
    
    animationMode = mode;
    animationSpeed = speed;
    animationActive = true;
    lastAnimationUpdate = millis();
    
    // Set all LEDs to animation mode
    for (int i = 0; i < numLEDs; i++) {
        ledConfigs[i].mode = LED_MODE_ANIMATION;
    }
}

void stopAnimation() {
    if (!strip) return;
    
    animationActive = false;
    
    // Restore all LEDs to their static colors
    for (int i = 0; i < numLEDs; i++) {
        ledConfigs[i].mode = LED_MODE_STATIC;
        strip->setPixelColor(i, strip->Color(
            ledConfigs[i].r,
            ledConfigs[i].g,
            ledConfigs[i].b
        ));
    }
    strip->show();
}

void updateAnimation() {
    if (!strip || !animationActive) return;
    
    uint32_t currentTime = millis();
    if (currentTime - lastAnimationUpdate < animationSpeed) return;
    
    lastAnimationUpdate = currentTime;
    
    switch (animationMode) {
        case LED_ANIM_RAINBOW:
            animateRainbow();
            break;
        case LED_ANIM_CHASE:
            animateChase();
            break;
        case LED_ANIM_BREATH:
            animateBreath();
            break;
        case LED_ANIM_ALTERNATING:
            animateAlternating();
            break;
        default:
            // Unknown animation mode
            stopAnimation();
            break;
    }
}

// Rainbow animation across all LEDs
void animateRainbow() {
    if (!strip) return;
    
    static uint16_t j = 0;
    
    for (int i = 0; i < numLEDs; i++) {
        strip->setPixelColor(i, wheel((i + j) & 255));
    }
    strip->show();
    
    j = (j + 1) % 256;
}

// Chase animation (one color moving through the strip)
void animateChase() {
    if (!strip) return;
    
    static uint8_t step = 0;
    
    for (int i = 0; i < numLEDs; i++) {
        if (i % 6 == step) {
            strip->setPixelColor(i, strip->Color(255, 0, 0)); // Red
        } else {
            strip->setPixelColor(i, strip->Color(0, 0, 0)); // Off
        }
    }
    strip->show();
    
    step = (step + 1) % 6;
}

// Breathing animation (fade in and out)
void animateBreath() {
    if (!strip) return;
    
    static uint8_t brightness = 0;
    static bool increasing = true;
    
    if (increasing) {
        brightness += 5;
        if (brightness >= 255) {
            brightness = 255;
            increasing = false;
        }
    } else {
        brightness -= 5;
        if (brightness <= 0) {
            brightness = 0;
            increasing = true;
        }
    }
    
    strip->setBrightness(brightness);
    strip->show();
}

// Alternating LEDs animation
void animateAlternating() {
    if (!strip) return;
    
    static bool state = false;
    
    state = !state;
    
    for (int i = 0; i < numLEDs; i++) {
        if ((i % 2 == 0) == state) {
            strip->setPixelColor(i, strip->Color(255, 0, 0)); // Red
        } else {
            strip->setPixelColor(i, strip->Color(0, 0, 255)); // Blue
        }
    }
    strip->show();
}

// Helper function for rainbow animation
uint32_t wheel(byte wheelPos) {
    if (!strip) return 0;
    
    wheelPos = 255 - wheelPos;
    if (wheelPos < 85) {
        return strip->Color(255 - wheelPos * 3, 0, wheelPos * 3);
    }
    if (wheelPos < 170) {
        wheelPos -= 85;
        return strip->Color(0, wheelPos * 3, 255 - wheelPos * 3);
    }
    wheelPos -= 170;
    return strip->Color(wheelPos * 3, 255 - wheelPos * 3, 0);
}

// Improved button-LED synchronization
void syncLEDsWithButtons(const char* buttonId, bool pressed) {
    // Add debug output to track button events
    USBSerial.printf("Button %s %s\n", buttonId, pressed ? "PRESSED" : "RELEASED");
    
    auto it = buttonLEDMap.find(buttonId);
    if (it != buttonLEDMap.end()) {
        const ButtonLEDMapping& mapping = it->second;
        
        try {
            // Update all LEDs assigned to this button
            for (uint8_t index : mapping.ledIndices) {
                if (index >= numLEDs) {
                    USBSerial.printf("Error: Invalid LED index %d for button %s\n", index, buttonId);
                    continue;
                }
                
                if (pressed) {
                    // Button pressed - use pressed color
                    USBSerial.printf("  Setting LED %d to pressed color (%d,%d,%d)\n", 
                                   index, 
                                   ledConfigs[index].pressedR, 
                                   ledConfigs[index].pressedG, 
                                   ledConfigs[index].pressedB);
                    
                    setLEDColorWithBrightness(index,
                        ledConfigs[index].pressedR,
                        ledConfigs[index].pressedG,
                        ledConfigs[index].pressedB,
                        ledConfigs[index].brightness,
                        true);
                } else {
                    // Button released - restore default color
                    USBSerial.printf("  Setting LED %d to default color (%d,%d,%d)\n", 
                                   index, 
                                   ledConfigs[index].r, 
                                   ledConfigs[index].g, 
                                   ledConfigs[index].b);
                    
                    setLEDColorWithBrightness(index,
                        ledConfigs[index].r,
                        ledConfigs[index].g,
                        ledConfigs[index].b,
                        ledConfigs[index].brightness,
                        false);
                }
                
                ledConfigs[index].isActive = pressed;
            }
        } catch (const std::exception& e) {
            USBSerial.printf("Error in syncLEDsWithButtons: %s\n", e.what());
        }
    } else {
        // If no explicit mapping exists, try to derive the LED index from button ID
        if (strncmp(buttonId, "button-", 7) == 0) {
            int buttonNum = atoi(buttonId + 7) - 1; // Convert to 0-based index
            
            if (buttonNum >= 0 && buttonNum < numLEDs) {
                // Use pressed colors for pressed, default colors for released
                if (pressed) {
                    USBSerial.printf("  Setting fallback LED %d to pressed color\n", buttonNum);
                    setLEDColorWithBrightness(buttonNum, 
                        ledConfigs[buttonNum].pressedR,
                        ledConfigs[buttonNum].pressedG,
                        ledConfigs[buttonNum].pressedB,
                        ledConfigs[buttonNum].brightness,
                        true);
                } else {
                    USBSerial.printf("  Setting fallback LED %d to default color\n", buttonNum);
                    setLEDColorWithBrightness(buttonNum,
                        ledConfigs[buttonNum].r,
                        ledConfigs[buttonNum].g,
                        ledConfigs[buttonNum].b,
                        ledConfigs[buttonNum].brightness,
                        false);
                }
                ledConfigs[buttonNum].isActive = pressed;
            }
        }
    }
}

// JSON utility function for generating LED configuration JSON
String getLEDConfigJson() {
    DynamicJsonDocument doc(4096);
    JsonArray leds = doc.createNestedArray("leds");
    
    for (int i = 0; i < numLEDs; i++) {
        JsonObject led = leds.createNestedObject();
        led["index"] = i;
        led["mode"] = ledConfigs[i].mode;
        led["r"] = ledConfigs[i].r;
        led["g"] = ledConfigs[i].g;
        led["b"] = ledConfigs[i].b;
        led["brightness"] = ledConfigs[i].brightness;
        
        // Button mapping if exists
        if (ledConfigs[i].mode == LED_MODE_BUTTON) {
            led["button_id"] = ledConfigs[i].buttonId;
            led["pressed_r"] = ledConfigs[i].pressedR;
            led["pressed_g"] = ledConfigs[i].pressedG;
            led["pressed_b"] = ledConfigs[i].pressedB;
        }
    }
    
    // Add global brightness setting
    doc["global_brightness"] = strip ? strip->getBrightness() : 50;
    
    // Add button-LED mappings
    JsonArray mappings = doc.createNestedArray("button_led_mappings");
    for (const auto& mapping : buttonLEDMap) {
        JsonObject mapObj = mappings.createNestedObject();
        mapObj["button_id"] = mapping.first;
        
        // Create array of LED indices for this button
        JsonArray ledIndices = mapObj.createNestedArray("led_indices");
        for (uint8_t idx : mapping.second.ledIndices) {
            ledIndices.add(idx);
        }
        
        // Add default and pressed colors
        JsonObject defaultColor = mapObj.createNestedObject("default_color");
        defaultColor["r"] = mapping.second.defaultColor[0];
        defaultColor["g"] = mapping.second.defaultColor[1];
        defaultColor["b"] = mapping.second.defaultColor[2];
        
        JsonObject pressedColor = mapObj.createNestedObject("pressed_color");
        pressedColor["r"] = mapping.second.pressedColor[0];
        pressedColor["g"] = mapping.second.pressedColor[1];
        pressedColor["b"] = mapping.second.pressedColor[2];
    }
    
    String jsonString;
    serializeJson(doc, jsonString);
    return jsonString;
}

// JSON utility function for updating LED configuration from JSON
bool updateLEDConfigFromJson(const String& json) {
    USBSerial.println("\n===== [updateLEDConfigFromJson] Starting LED config update =====");
    
    // Check if LED handler is initialized
    if (!strip) {
        USBSerial.println("[updateLEDConfigFromJson] ERROR: LED strip is not initialized (strip is NULL)");
        return false;
    }
    
    USBSerial.printf("[updateLEDConfigFromJson] LED strip initialized with %d LEDs\n", strip->numPixels());
    
    if (!ledConfigs) {
        USBSerial.println("[updateLEDConfigFromJson] ERROR: ledConfigs array is NULL");
        return false;
    }
    
    // Debug before parsing
    USBSerial.printf("[updateLEDConfigFromJson] JSON size: %d bytes\n", json.length());
    USBSerial.printf("[updateLEDConfigFromJson] Free heap before parsing: %d bytes\n", ESP.getFreeHeap());
    
    // Dump first 100 characters of JSON for verification
    if (json.length() > 0) {
        USBSerial.println("[updateLEDConfigFromJson] JSON preview (first 100 chars): " + 
                         json.substring(0, std::min(100, (int)json.length())));
    }
    
    // Estimate buffer size
    size_t bufferSize = estimateJsonBufferSize(json, 1.5);
    USBSerial.printf("[updateLEDConfigFromJson] Estimated buffer size: %d bytes\n", bufferSize);
    
    DynamicJsonDocument doc(bufferSize);
    DeserializationError error = deserializeJson(doc, json);
    
    // Debug after parsing
    USBSerial.printf("[updateLEDConfigFromJson] Free heap after parsing: %d bytes\n", ESP.getFreeHeap());
    
    if (error) {
        USBSerial.printf("[updateLEDConfigFromJson] ERROR: JSON parse error: %s\n", error.c_str());
        
        if (error.code() == DeserializationError::NoMemory) {
            USBSerial.printf("[updateLEDConfigFromJson] Memory allocation failed. Tried buffer size: %u bytes\n", bufferSize);
            // Use estimation instead of measureJson
            size_t requiredSize = json.length() * 2; // Simple estimation
            USBSerial.printf("[updateLEDConfigFromJson] Estimated required size: %u bytes\n", requiredSize);
            
            // If we can measure and allocate the required size, retry
            if (requiredSize > 0 && requiredSize <= ESP.getFreeHeap() / 2) {
                USBSerial.printf("[updateLEDConfigFromJson] Retrying with estimated size: %u bytes\n", requiredSize);
                DynamicJsonDocument retryDoc(requiredSize);
                error = deserializeJson(retryDoc, json);
                
                if (!error) {
                    USBSerial.println("[updateLEDConfigFromJson] Retry parsing succeeded");
                    // Use this document instead
                    
                    // Check for expected structure
                    if (!retryDoc.containsKey("leds")) {
                        USBSerial.println("[updateLEDConfigFromJson] ERROR: JSON missing 'leds' key");
                        return false;
                    }
                    
                    // Update global brightness if present
                    if (retryDoc["leds"].containsKey("brightness")) {
                        uint8_t brightness = retryDoc["leds"]["brightness"];
                        USBSerial.printf("[updateLEDConfigFromJson] Setting global brightness to %d\n", brightness);
                        strip->setBrightness(brightness);
                    }
                    
                    // Check for expected LED array
                    bool usedLayersFormat = false;
                    JsonArray ledsArray;
                    
                    // First check if we're using the layers format
                    if (retryDoc["leds"].containsKey("layers") && retryDoc["leds"]["layers"].is<JsonArray>()) {
                        USBSerial.println("[updateLEDConfigFromJson] Using layers format");
                        usedLayersFormat = true;
                        
                        // Find the active layer
                        JsonArray layers = retryDoc["leds"]["layers"];
                        for (JsonObject layer : layers) {
                            if (layer.containsKey("active") && layer["active"].as<bool>()) {
                                if (layer.containsKey("layer-config") && layer["layer-config"].is<JsonArray>()) {
                                    ledsArray = layer["layer-config"].as<JsonArray>();
                                    USBSerial.printf("[updateLEDConfigFromJson] Found active layer with %d LEDs\n", ledsArray.size());
                                    break;
                                }
                            }
                        }
                    } 
                    // Then check for direct config array
                    else if (retryDoc["leds"].containsKey("config") && retryDoc["leds"]["config"].is<JsonArray>()) {
                        USBSerial.println("[updateLEDConfigFromJson] Using direct config format");
                        ledsArray = retryDoc["leds"]["config"].as<JsonArray>();
                        USBSerial.printf("[updateLEDConfigFromJson] Found LED config with %d LEDs\n", ledsArray.size());
                    }
                    
                    // Process LEDs if we found them
                    if (ledsArray.size() > 0) {
                        int processedCount = 0;
                        for (JsonObject led : ledsArray) {
                            if (led.containsKey("stream_address")) {
                                uint8_t index = led["stream_address"];
                                if (index < numLEDs) {
                                    // Update color if present
                                    if (led.containsKey("color")) {
                                        ledConfigs[index].r = led["color"]["r"] | 0;
                                        ledConfigs[index].g = led["color"]["g"] | 0;
                                        ledConfigs[index].b = led["color"]["b"] | 0;
                                    }
                                    
                                    // Update pressed color if present
                                    if (led.containsKey("pressed_color")) {
                                        ledConfigs[index].pressedR = led["pressed_color"]["r"] | 0;
                                        ledConfigs[index].pressedG = led["pressed_color"]["g"] | 0;
                                        ledConfigs[index].pressedB = led["pressed_color"]["b"] | 0;
                                    }
                                    
                                    // Update brightness if present
                                    if (led.containsKey("brightness")) {
                                        ledConfigs[index].brightness = led["brightness"];
                                    }
                                    
                                    // Update button ID if present
                                    if (led.containsKey("button_id")) {
                                        ledConfigs[index].buttonId = led["button_id"].as<String>();
                                    }
                                    
                                    // Mark for update
                                    ledConfigs[index].needsUpdate = true;
                                    processedCount++;
                                }
                            }
                        }
                        
                        USBSerial.printf("[updateLEDConfigFromJson] Successfully processed %d LEDs\n", processedCount);
                        
                        // Force an update of all LEDs
                        updateLEDs();
                        USBSerial.println("[updateLEDConfigFromJson] LED update completed successfully\n");
                        return true;
                    } else {
                        USBSerial.println("[updateLEDConfigFromJson] ERROR: No LEDs found in the config");
                        return false;
                    }
                } else {
                    USBSerial.printf("[updateLEDConfigFromJson] ERROR: Retry parsing failed: %s\n", error.c_str());
                }
            }
        }
        return false;
    }
    
    // Check for expected structure
    if (!doc.containsKey("leds")) {
        USBSerial.println("[updateLEDConfigFromJson] ERROR: JSON missing 'leds' key");
        return false;
    }
    
    // Update global brightness if present
    if (doc["leds"].containsKey("brightness")) {
        uint8_t brightness = doc["leds"]["brightness"];
        USBSerial.printf("[updateLEDConfigFromJson] Setting global brightness to %d\n", brightness);
        strip->setBrightness(brightness);
    }
    
    // Check for layers format first
    bool usedLayersFormat = false;
    JsonArray ledsArray;
    
    if (doc["leds"].containsKey("layers") && doc["leds"]["layers"].is<JsonArray>()) {
        USBSerial.println("[updateLEDConfigFromJson] Using layers format");
        usedLayersFormat = true;
        
        // Find the active layer
        JsonArray layers = doc["leds"]["layers"];
        for (JsonObject layer : layers) {
            if (layer.containsKey("active") && layer["active"].as<bool>()) {
                if (layer.containsKey("layer-config") && layer["layer-config"].is<JsonArray>()) {
                    ledsArray = layer["layer-config"].as<JsonArray>();
                    USBSerial.printf("[updateLEDConfigFromJson] Found active layer with %d LEDs\n", ledsArray.size());
                    break;
                }
            }
        }
    } 
    // Then check for direct config array
    else if (doc["leds"].containsKey("config") && doc["leds"]["config"].is<JsonArray>()) {
        USBSerial.println("[updateLEDConfigFromJson] Using direct config format");
        ledsArray = doc["leds"]["config"].as<JsonArray>();
        USBSerial.printf("[updateLEDConfigFromJson] Found LED config with %d LEDs\n", ledsArray.size());
    }
    
    // Process LEDs if we found them
    if (ledsArray.size() > 0) {
        int processedCount = 0;
        for (JsonObject led : ledsArray) {
            if (led.containsKey("stream_address")) {
                uint8_t index = led["stream_address"];
                if (index < numLEDs) {
                    // Update color if present
                    if (led.containsKey("color")) {
                        ledConfigs[index].r = led["color"]["r"] | 0;
                        ledConfigs[index].g = led["color"]["g"] | 0;
                        ledConfigs[index].b = led["color"]["b"] | 0;
                    }
                    
                    // Update pressed color if present
                    if (led.containsKey("pressed_color")) {
                        ledConfigs[index].pressedR = led["pressed_color"]["r"] | 0;
                        ledConfigs[index].pressedG = led["pressed_color"]["g"] | 0;
                        ledConfigs[index].pressedB = led["pressed_color"]["b"] | 0;
                    }
                    
                    // Update brightness if present
                    if (led.containsKey("brightness")) {
                        ledConfigs[index].brightness = led["brightness"];
                    }
                    
                    // Update button ID if present
                    if (led.containsKey("button_id")) {
                        ledConfigs[index].buttonId = led["button_id"].as<String>();
                    }
                    
                    // Mark for update
                    ledConfigs[index].needsUpdate = true;
                    processedCount++;
                }
            }
        }
        
        USBSerial.printf("[updateLEDConfigFromJson] Successfully processed %d LEDs\n", processedCount);
        
        // Force an update of all LEDs
        updateLEDs();
        USBSerial.println("[updateLEDConfigFromJson] LED update completed successfully\n");
        return true;
    }
    
    USBSerial.println("[updateLEDConfigFromJson] ERROR: No LEDs found in the config");
    return false;
}

// Helper function to process a single LED config object
void processLEDConfig(JsonObject& led) {
    if (led.containsKey("index")) {
        int index = led["index"];
        if (index >= 0 && index < numLEDs) {
            // Update color if present
            if (led.containsKey("color")) {
                ledConfigs[index].r = led["color"]["r"] | 0;
                ledConfigs[index].g = led["color"]["g"] | 0;
                ledConfigs[index].b = led["color"]["b"] | 0;
            }
            
            // Update pressed color if present
            if (led.containsKey("pressed_color")) {
                ledConfigs[index].pressedR = led["pressed_color"]["r"] | 0;
                ledConfigs[index].pressedG = led["pressed_color"]["g"] | 0;
                ledConfigs[index].pressedB = led["pressed_color"]["b"] | 0;
            }
            
            // Update brightness if present
            if (led.containsKey("brightness")) {
                ledConfigs[index].brightness = led["brightness"];
            }
            
            // Update button ID if present
            if (led.containsKey("button_id")) {
                ledConfigs[index].buttonId = led["button_id"].as<String>();
            }
            
            // Apply the updates to the LED
            updateLED(index);
        }
    }
}

// Helper function to read a file as string (similar to the one in ModuleSetup.cpp)
String readJsonFile(const char* filePath) {
    if (!LittleFS.exists(filePath)) {
        USBSerial.printf("File not found: %s\n", filePath);
        return "";
    }
    
    File file = LittleFS.open(filePath, "r");
    if (!file) {
        USBSerial.printf("Failed to open file: %s\n", filePath);
        return "";
    }
    
    String content = file.readString();
    file.close();
    return content;
}

// Clean up resources
void cleanupLED() {
    if (ledConfigs) {
        delete[] ledConfigs;
        ledConfigs = nullptr;
    }
    
    if (strip) {
        delete strip;
        strip = nullptr;
    }
}

// Optimized LED update function for main loop
void updateLEDs() {
    if (!strip) return;
    
    static unsigned long lastUpdate = 0;
    const unsigned long updateInterval = 16; // ~60fps max update rate
    unsigned long currentTime = millis();
    
    #ifdef ENABLE_POWER_MONITORING
    // Check power status regularly
    checkPowerStatus();
    #endif
    
    // Skip update if it's too soon since the last one (rate limiting)
    if (currentTime - lastUpdate < updateInterval) return;
    lastUpdate = currentTime;
    
    // Update animations if active
    if (animationActive) {
        updateAnimation();
        return; // Skip normal LED updates during animation
    }
    
    // Check for LEDs that need updates
    bool needsUpdate = false;
    
    // Process button LEDs first
    for (const auto& pair : buttonLEDMap) {
        const ButtonLEDMapping& mapping = pair.second;
        
        for (uint8_t index : mapping.ledIndices) {
            if (index >= numLEDs) continue;
            
            if (ledConfigs[index].needsUpdate) {
                float factor = ledConfigs[index].brightness / 255.0;
                
                if (ledConfigs[index].isActive) {
                    // Use pressed color
                    strip->setPixelColor(index, strip->Color(
                        ledConfigs[index].pressedR * factor,
                        ledConfigs[index].pressedG * factor,
                        ledConfigs[index].pressedB * factor
                    ));
                } else {
                    // Use default color
                    strip->setPixelColor(index, strip->Color(
                        ledConfigs[index].r * factor,
                        ledConfigs[index].g * factor,
                        ledConfigs[index].b * factor
                    ));
                }
                
                ledConfigs[index].needsUpdate = false;
                needsUpdate = true;
            }
        }
    }
    
    // Process any remaining LEDs
    for (int i = 0; i < numLEDs; i++) {
        if (ledConfigs[i].needsUpdate) {
            float factor = ledConfigs[i].brightness / 255.0;
            
            if (ledConfigs[i].isActive) {
                // Use pressed color if active
                strip->setPixelColor(i, strip->Color(
                    ledConfigs[i].pressedR * factor,
                    ledConfigs[i].pressedG * factor,
                    ledConfigs[i].pressedB * factor
                ));
            } else {
                // Use default color otherwise
                strip->setPixelColor(i, strip->Color(
                    ledConfigs[i].r * factor,
                    ledConfigs[i].g * factor,
                    ledConfigs[i].b * factor
                ));
            }
            
            ledConfigs[i].needsUpdate = false;
            needsUpdate = true;
        }
    }
    
    // Only show changes if something was updated
    if (needsUpdate) {
        strip->show();
    }
}

#ifdef ENABLE_POWER_MONITORING
// Check power status using ADC
void checkPowerStatus() {
    if (!strip) return;
    
    uint32_t currentTime = millis();
    if (currentTime - lastPowerCheck < POWER_CHECK_INTERVAL) return;
    lastPowerCheck = currentTime;
    
    // Read voltage from ADC
    int adcValue = adc1_get_raw(ADC1_CHANNEL_6);
    
    // Convert ADC reading to voltage (depends on voltage divider circuit)
    // Assuming 3.3V reference and a 1:2 voltage divider
    float voltage = (adcValue * 3.3 / 4095.0) * 2.0;
    
    if (voltage < MIN_VOLTAGE && !lowPowerMode) {
        handleLowPower();
        lowPowerMode = true;
    } else if (voltage >= MIN_VOLTAGE && lowPowerMode) {
        // Restore normal operation
        lowPowerMode = false;
        USBSerial.println("Power level normal - restoring brightness");
    }
}

// Handle low power situations
void handleLowPower() {
    // Reduce brightness to minimum safe level
    uint8_t emergencyBrightness = 20;
    strip->setBrightness(emergencyBrightness);
    
    // Turn off animations
    if (animationActive) {
        stopAnimation();
    }
    
    // Flash all LEDs red to indicate low power
    for (int i = 0; i < 3; i++) {
        // Red flash
        for (int j = 0; j < numLEDs; j++) {
            strip->setPixelColor(j, strip->Color(255, 0, 0));
        }
        strip->show();
        delay(100);
        
        // Off
        for (int j = 0; j < numLEDs; j++) {
            strip->setPixelColor(j, strip->Color(0, 0, 0));
        }
        strip->show();
        delay(100);
    }
    
    // Set a few indicator LEDs to red (if we have enough)
    if (numLEDs >= 3) {
        strip->setPixelColor(0, strip->Color(255, 0, 0));
        strip->setPixelColor(numLEDs/2, strip->Color(255, 0, 0));
        strip->setPixelColor(numLEDs-1, strip->Color(255, 0, 0));
        strip->show();
    } else if (numLEDs > 0) {
        strip->setPixelColor(0, strip->Color(255, 0, 0));
        strip->show();
    }
    
    USBSerial.println("WARNING: Low USB voltage detected! Entering power-saving mode.");
}
#endif

// Add implementation of saveLEDConfig if it doesn't exist
bool saveLEDConfig() {
    if (!strip) return false;
    
    String config = getLEDConfigJson();
    
    // Make sure config directory exists
    if (!LittleFS.exists("/config")) {
        if (!LittleFS.mkdir("/config")) {
            USBSerial.println("Failed to create config directory");
            return false;
        }
    }
    
    // Open the file for writing
    File file = LittleFS.open("/config/leds.json", "w");
    if (!file) {
        USBSerial.println("Failed to open LED config file for writing");
        return false;
    }
    
    // Write the JSON data
    size_t bytesWritten = file.print(config);
    file.close();
    
    // Check if the write was successful
    if (bytesWritten == config.length()) {
        USBSerial.println("LED configuration saved successfully");
        return true;
    } else {
        USBSerial.println("Failed to save LED configuration");
        return false;
    }
}

// Function to update a single LED's color based on its configuration
void updateLED(uint8_t index) {
    if (!strip || index >= numLEDs) {
        USBSerial.printf("Invalid LED index: %d\n", index);
        return;
    }
    
    try {
        float factor = ledConfigs[index].brightness / 255.0;
        
        if (ledConfigs[index].isActive) {
            // LED is active (button pressed) - use pressed color
            strip->setPixelColor(index, strip->Color(
                ledConfigs[index].pressedR * factor,
                ledConfigs[index].pressedG * factor,
                ledConfigs[index].pressedB * factor
            ));
        } else {
            // LED is inactive (normal state) - use default color
            strip->setPixelColor(index, strip->Color(
                ledConfigs[index].r * factor,
                ledConfigs[index].g * factor, 
                ledConfigs[index].b * factor
            ));
        }
        
        strip->show();
        ledConfigs[index].needsUpdate = false;
    } catch (const std::exception& e) {
        USBSerial.printf("Error in updateLED: %s\n", e.what());
    }
}