// LEDHanler.cpp

#include "LEDHandler.h"
#include "ModuleSetup.h"
#include <SPIFFS.h>
#include <ArduinoJson.h>

extern USBCDC USBSerial;

// LED strip will be initialized dynamically based on config
Adafruit_NeoPixel* strip = nullptr;

// Array to store LED configurations - size will be determined from config
LEDConfig* ledConfigs = nullptr;
uint8_t numLEDs = 0;

// Animation variables
bool animationActive = false;
uint8_t animationMode = 0;
uint32_t lastAnimationUpdate = 0;
uint16_t animationSpeed = 100; // ms between animation frames

// Forward declaration of helper functions
static String readJsonFile(const char* filePath);

void initializeLED() {
    // Try to load LED configuration
    String ledJson = readJsonFile("/config/LEDs.json");
    if (ledJson.isEmpty()) {
        USBSerial.println("LED config not found, using defaults");
        
        // Use defaults from header
        numLEDs = DEFAULT_NUM_LEDS;
        strip = new Adafruit_NeoPixel(numLEDs, DEFAULT_LED_PIN, NEO_GRB + NEO_KHZ800);
    } else {
        // Parse LED configuration
        DynamicJsonDocument doc(8192);
        DeserializationError error = deserializeJson(doc, ledJson);
        
        if (error) {
            USBSerial.print("Error parsing LED config: ");
            USBSerial.println(error.c_str());
            
            // Use defaults
            numLEDs = DEFAULT_NUM_LEDS;
            strip = new Adafruit_NeoPixel(numLEDs, DEFAULT_LED_PIN, NEO_GRB + NEO_KHZ800);
        } else {
            // Get LED count and pin from config
            numLEDs = doc["leds"]["config"].size();
            uint8_t ledPin = doc["leds"]["pin"] | DEFAULT_LED_PIN; // Use default if not specified
            
            USBSerial.printf("Initializing %d LEDs on pin %d\n", numLEDs, ledPin);
            strip = new Adafruit_NeoPixel(numLEDs, ledPin, NEO_GRB + NEO_KHZ800);
            
            // Get global brightness
            uint8_t brightness = doc["leds"]["brightness"] | 50;
            
            // Initialize NeoPixel strip
            strip->begin();
            strip->setBrightness(brightness);
            strip->clear();
            strip->show();
            
            // Apply saved LED colors if present
            if (doc["leds"].containsKey("config")) {
                // Create LED configs array
                ledConfigs = new LEDConfig[numLEDs];
                
                JsonArray ledsConfig = doc["leds"]["config"];
                for (JsonObject led : ledsConfig) {
                    if (led.containsKey("stream_address") && led.containsKey("color")) {
                        uint8_t index = led["stream_address"];
                        
                        if (index < numLEDs) {
                            // Store the LED configuration
                            ledConfigs[index].r = led["color"]["r"] | 0;
                            ledConfigs[index].g = led["color"]["g"] | 255;
                            ledConfigs[index].b = led["color"]["b"] | 0;
                            ledConfigs[index].brightness = led["brightness"] | 50;
                            ledConfigs[index].mode = led["mode"] | LED_MODE_STATIC;
                            
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
    
    // If LED configs not created yet, create them now
    if (ledConfigs == nullptr) {
        ledConfigs = new LEDConfig[numLEDs];
        
        // Default all LEDs to green
        for (int i = 0; i < numLEDs; i++) {
            ledConfigs[i].r = 0;
            ledConfigs[i].g = 255;
            ledConfigs[i].b = 0;
            ledConfigs[i].brightness = 50;
            ledConfigs[i].mode = LED_MODE_STATIC;
            
            strip->setPixelColor(i, strip->Color(0, 255, 0));
        }
        strip->show();
    }
    
    USBSerial.println("LED Handler initialized");
}

void setLEDColor(uint8_t index, uint8_t r, uint8_t g, uint8_t b) {
    if (!strip || index >= numLEDs) return;
    
    // Update the configuration
    ledConfigs[index].r = r;
    ledConfigs[index].g = g;
    ledConfigs[index].b = b;
    ledConfigs[index].mode = LED_MODE_STATIC;
    
    // Set the actual LED color
    strip->setPixelColor(index, strip->Color(r, g, b));
    strip->show();
}

void setLEDColorWithBrightness(uint8_t index, uint8_t r, uint8_t g, uint8_t b, uint8_t brightness) {
    if (!strip || index >= numLEDs) return;
    
    // Store original RGB values
    ledConfigs[index].r = r;
    ledConfigs[index].g = g;
    ledConfigs[index].b = b;
    ledConfigs[index].brightness = brightness;
    ledConfigs[index].mode = LED_MODE_STATIC;
    
    // Apply brightness factor to RGB values for this specific LED
    float factor = brightness / 255.0;
    uint8_t adjustedR = r * factor;
    uint8_t adjustedG = g * factor;
    uint8_t adjustedB = b * factor;
    
    strip->setPixelColor(index, strip->Color(adjustedR, adjustedG, adjustedB));
    strip->show();
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

// Synchronize LEDs with button presses
void syncLEDsWithButtons(const char* buttonId, bool pressed) {
    if (!strip) return;
    
    // Find the button index from its ID
    // Format is "button-X" where X is a number starting from 1
    int buttonIndex = -1;
    if (strncmp(buttonId, "button-", 7) == 0) {
        buttonIndex = atoi(buttonId + 7) - 1;
    } else {
        return; // Not a valid button ID
    }
    
    if (buttonIndex >= 0 && buttonIndex < numLEDs) {
        if (pressed) {
            // Button pressed - highlight with white
            strip->setPixelColor(buttonIndex, strip->Color(255, 255, 255));
        } else {
            // Button released - restore original color
            float factor = ledConfigs[buttonIndex].brightness / 255.0;
            strip->setPixelColor(buttonIndex, strip->Color(
                ledConfigs[buttonIndex].r * factor,
                ledConfigs[buttonIndex].g * factor,
                ledConfigs[buttonIndex].b * factor
            ));
        }
        strip->show();
    }
}

// // Handle LED updates in the main loop
// void updateLEDs() {
//     if (animationActive) {
//         updateAnimation();
//     }
// }

// Get LED configuration as JSON string
String getLEDConfigJson() {
    if (!strip) return "{}";
    
    DynamicJsonDocument doc(8192);
    
    doc["leds"]["layer-name"] = "default-led-layer";
    doc["leds"]["active"] = true;
    doc["leds"]["pin"] = strip->getPin();
    doc["leds"]["type"] = "sk6812";
    doc["leds"]["brightness"] = strip->getBrightness();
    
    // Animation settings
    JsonObject animation = doc["leds"].createNestedObject("animation");
    animation["active"] = animationActive;
    animation["mode"] = animationMode;
    animation["speed"] = animationSpeed;
    
    // LED configuration array
    JsonArray config = doc["leds"].createNestedArray("config");
    
    for (int i = 0; i < numLEDs; i++) {
        JsonObject led = config.createNestedObject();
        led["id"] = "led-" + String(i + 1);
        led["stream_address"] = i;
        led["type"] = "sk6812";
        
        // Size and location (would ideally come from the components.json)
        JsonObject size = led.createNestedObject("size");
        size["rows"] = 1;
        size["columns"] = 1;
        
        JsonObject location = led.createNestedObject("start_location");
        // Assuming 5x5 grid for main module
        location["row"] = i / 5;
        location["column"] = i % 5;
        
        // Color information
        JsonObject color = led.createNestedObject("color");
        color["r"] = ledConfigs[i].r;
        color["g"] = ledConfigs[i].g;
        color["b"] = ledConfigs[i].b;
        
        led["brightness"] = ledConfigs[i].brightness;
        led["mode"] = ledConfigs[i].mode;
    }
    
    String output;
    serializeJson(doc, output);
    return output;
}

// Update LED configuration from JSON string
bool updateLEDConfigFromJson(const String& json) {
    if (!strip) return false;
    
    DynamicJsonDocument doc(8192);
    DeserializationError error = deserializeJson(doc, json);
    
    if (error) {
        USBSerial.print("Error parsing LED config JSON: ");
        USBSerial.println(error.c_str());
        return false;
    }
    
    // Update global brightness if present
    if (doc["leds"].containsKey("brightness")) {
        strip->setBrightness(doc["leds"]["brightness"].as<uint8_t>());
    }
    
    // Update animation settings if present
    if (doc["leds"].containsKey("animation")) {
        JsonObject animation = doc["leds"]["animation"];
        if (animation.containsKey("active")) {
            if (animation["active"].as<bool>()) {
                // Start animation with specified mode and speed
                uint8_t mode = animation.containsKey("mode") ? 
                    animation["mode"].as<uint8_t>() : LED_ANIM_RAINBOW;
                uint16_t speed = animation.containsKey("speed") ? 
                    animation["speed"].as<uint16_t>() : 100;
                startAnimation(mode, speed);
            } else {
                stopAnimation();
            }
        }
    }
    
    // Update individual LEDs if present
    if (doc["leds"].containsKey("config")) {
        JsonArray config = doc["leds"]["config"].as<JsonArray>();
        
        for (JsonObject led : config) {
            if (led.containsKey("stream_address")) {
                uint8_t index = led["stream_address"].as<uint8_t>();
                
                if (index >= numLEDs) continue;
                
                // Update from color object if present
                if (led.containsKey("color")) {
                    JsonObject color = led["color"];
                    if (color.containsKey("r") && color.containsKey("g") && color.containsKey("b")) {
                        uint8_t r = color["r"].as<uint8_t>();
                        uint8_t g = color["g"].as<uint8_t>();
                        uint8_t b = color["b"].as<uint8_t>();
                        
                        if (led.containsKey("brightness")) {
                            uint8_t brightness = led["brightness"].as<uint8_t>();
                            setLEDColorWithBrightness(index, r, g, b, brightness);
                        } else {
                            setLEDColor(index, r, g, b);
                        }
                    }
                }
            }
        }
    }
    
    return true;
}

// Save LED configuration to file
bool saveLEDConfig() {
    if (!strip) return false;
    
    String config = getLEDConfigJson();
    
    // Open the file for writing
    File file = SPIFFS.open("/config/LEDs.json", "w");
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
        
        // After saving LED config, trigger a config file merge to update the combined config
        mergeConfigFiles();
        
        return true;
    } else {
        USBSerial.println("Failed to save LED configuration");
        return false;
    }
}

// Helper function to read a file as string (similar to the one in ModuleSetup.cpp)
String readJsonFile(const char* filePath) {
    if (!SPIFFS.exists(filePath)) {
        USBSerial.printf("File not found: %s\n", filePath);
        return "";
    }
    
    File file = SPIFFS.open(filePath, "r");
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