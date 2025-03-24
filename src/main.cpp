#include <Arduino.h>
#include <USBCDC.h>
#include "esp_netif.h"
#include "lwip/ip_addr.h"
#include "lwip/inet.h"

#include "FS.h"
#include "SPIFFS.h"
#include "ArduinoJson.h"

// Modules
#include "ModuleSetup.h"
#include "ConfigManager.h"
#include "KeyHandler.h"  
#include "LEDHandler.h"
#include "EncoderHandler.h"
#include "HIDHandler.h"
#include "DisplayHandler.h"
#include "MacroHandler.h"

#include <USB.h>
#include <USBHID.h>
#include <USBHIDKeyboard.h>
#include <USBHIDConsumerControl.h>
#include <USBHIDMouse.h>
#include <USBCDC.h>

#include "WiFiManager.h"

// Forward declarations for Display functions
extern void updateDisplay();

USBHIDKeyboard Keyboard;
USBHIDConsumerControl ConsumerControl;
USBHIDMouse Mouse;  // Optional if you need mouse controls
USBCDC USBSerial;

// Flag to indicate if USB server should be initialized
// IMPORTANT: Set this to false to disable USB server completely

#define TAG "HID+CDC Esp32-s3 Macropad"

//  Will be moved to info.json
#define ROW0 3  // Kept (safe GPIO pin)
#define ROW1 5  // Kept (safe GPIO pin)
#define ROW2 8  // ROW2 26
#define ROW3 9  // ROW3 47
#define ROW4 10 // ROW4 33
#define COL0 11 // COL0 34
#define COL1 21 // Kept (safe GPIO pin)
#define COL2 13 // Kept (safe GPIO pin)
#define COL3 6  // Kept (safe GPIO pin)
#define COL4 12 // Kept (safe GPIO pin)

// Validate GPIO pins for ESP32-S3
bool validateGpioPins(uint8_t* pins, uint8_t count) {
    // Valid GPIO pins for ESP32-S3 are 0-21
    const int validESP32S3Pins[] = {
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 
        15, 16, 17, 18, 19, 20, 21
    };
    
    for (uint8_t i = 0; i < count; i++) {
        bool isValid = false;
        for (int validPin : validESP32S3Pins) {
            if (pins[i] == validPin) {
                isValid = true;
                break;
            }
        }
        
        if (!isValid) {
            Serial.printf("Invalid GPIO pin for ESP32-S3: %d\n", pins[i]);
            return false;
        }
    }
    return true;
}

// Consistent pin configuration for key matrix
void configurePinModes(uint8_t* rowPins, uint8_t* colPins, uint8_t rows, uint8_t cols) {
    // Validate pins first with detailed ESP32-S3 validation
    if (!validateGpioPins(rowPins, rows) || !validateGpioPins(colPins, cols)) {
        Serial.println("Invalid GPIO pins detected for ESP32-S3!");
        return;
    }

    // IMPORTANT: For a matrix keyboard setup:
    // For a typical matrix keyboard setup:
    // - Configure rows as INPUT_PULLUP (will be read)
    // - Configure columns as OUTPUT (will be driven)
    
    // First, print the pin assignments for clarity
    Serial.println("\n--- Pin Configuration ---");
    Serial.println("Row pins (configured as INPUT_PULLUP):");
    for (uint8_t i = 0; i < rows; i++) {
        Serial.printf("  Row %d: GPIO %d\n", i, rowPins[i]);
    }
    
    Serial.println("Column pins (configured as OUTPUT):");
    for (uint8_t j = 0; j < cols; j++) {
        Serial.printf("  Column %d: GPIO %d\n", j, colPins[j]);
    }
    
    // Now configure the pins
    for (uint8_t i = 0; i < rows; i++) {
        pinMode(rowPins[i], INPUT_PULLUP);
    }

    for (uint8_t j = 0; j < cols; j++) {
        pinMode(colPins[j], OUTPUT);
        digitalWrite(colPins[j], HIGH);  // Start with HIGH (inactive)
    }
    
    Serial.println("Pin configuration complete\n");
}

// initializeKeyHandler() using a 5x5 grid and loading actions via ConfigManager
void initializeKeyHandler() {
    const uint8_t rows = 5;
    const uint8_t cols = 5;
    uint8_t rowPins[rows] = {ROW0, ROW1, ROW2, ROW3, ROW4};
    uint8_t colPins[cols] = {COL0, COL1, COL2, COL3, COL4};
    
    USBSerial.println("\n=== Initializing Keyboard Matrix ===");
    USBSerial.println("Matrix dimensions: 5x5");
    
    // Log pin assignments for clarity  
    USBSerial.println("Row pins:");
    for (int i = 0; i < rows; i++) {
        USBSerial.printf("  Row %d: GPIO %d\n", i, rowPins[i]);
    }
    
    USBSerial.println("Column pins:");
    for (int i = 0; i < cols; i++) {
        USBSerial.printf("  Column %d: GPIO %d\n", i, colPins[i]);
    }
    
    // Configure pin modes
    configurePinModes(rowPins, colPins, rows, cols);
    
    // Load components from JSON
    USBSerial.println("Loading components from JSON...");
    std::vector<Component> components = ConfigManager::loadComponents("/config/components.json");
    
    // Create and initialize key handler with components
    USBSerial.println("Initializing key handler instance...");
    keyHandler = new KeyHandler(rows, cols, components, rowPins, colPins);
    
    if (keyHandler) {
        keyHandler->begin();
        
        // Load actions configuration
        USBSerial.println("Loading key action configuration...");
        auto actions = ConfigManager::loadActions("/config/actions.json");
        keyHandler->loadKeyConfiguration(actions);
        
        USBSerial.println("Key handler initialization complete");
    } else {
        USBSerial.println("ERROR: Failed to create key handler instance!");
    }
    
    USBSerial.println("=== Keyboard Matrix Initialization Complete ===\n");
}


void initializeEncoderHandler() {
    // Read components JSON from the file
    String componentsJson = ConfigManager::readFile("/config/components.json");
    USBSerial.println("Loading components from JSON for encoders...");
    
    // Parse the components to get encoder configurations
    std::vector<Component> components = ConfigManager::loadComponents("/config/components.json");
    
    // Count encoders
    uint8_t encoderCount = 0;
    for (const Component& comp : components) {
        if (comp.type == "encoder") {
            encoderCount++;
        }
    }
    
    USBSerial.printf("Found %d encoders in configuration\n", encoderCount);
    
    // Create handler if we have encoders
    if (encoderCount > 0) {
        encoderHandler = new EncoderHandler(encoderCount);
        
        // Configure each encoder
        uint8_t encoderIndex = 0;
        for (const Component& comp : components) {
            if (comp.type == "encoder") {
                // Use a larger capacity for JSON parsing here
                DynamicJsonDocument doc(8192);
                DeserializationError error = deserializeJson(doc, componentsJson);
                if (error) {
                    USBSerial.printf("Error parsing components JSON: %s\n", error.c_str());
                    continue;
                }
                
                // Find this encoder in the parsed JSON
                JsonArray jsonComponents = doc["components"].as<JsonArray>();
                JsonObject encoderConfig;
                for (JsonObject component : jsonComponents) {
                    if (component["id"].as<String>() == comp.id) {
                        encoderConfig = component;
                        break;
                    }
                }
                
                if (!encoderConfig.isNull()) {
                    // Determine encoder type (default mechanical unless configured as as5600)
                    EncoderType type = ENCODER_TYPE_MECHANICAL;
                    if (encoderConfig.containsKey("configuration") && 
                        encoderConfig["configuration"].containsKey("type") &&
                        encoderConfig["configuration"]["type"].as<String>() == "as5600") {
                        type = ENCODER_TYPE_AS5600;
                    }
                    
                    // Get pins and configuration
                    uint8_t pinA = 0, pinB = 0;
                    int8_t direction = 1;
                    
                    if (encoderConfig.containsKey("mechanical")) {
                        pinA = encoderConfig["mechanical"]["pin_a"] | 0;
                        pinB = encoderConfig["mechanical"]["pin_b"] | 0;
                    }
                    
                    if (encoderConfig.containsKey("configuration") && 
                        encoderConfig["configuration"].containsKey("direction")) {
                        direction = encoderConfig["configuration"]["direction"] | 1;
                    }
                    
                    // Configure this encoder
                    USBSerial.printf("Configuring %s: type=%d, pinA=%d, pinB=%d, direction=%d\n", 
                                  comp.id.c_str(), type, pinA, pinB, direction);
                    
                    encoderHandler->configureEncoder(
                        encoderIndex++,
                        type,
                        pinA,
                        pinB,
                        direction,
                        0 // zeroPosition
                    );
                }
            }
        }
        
        // Initialize the configured encoders
        encoderHandler->begin();
        USBSerial.println("Encoder handler initialized successfully");
    } else {
        USBSerial.println("No encoders found in configuration");
    }
}


// Function to debug actions configuration
void debugActionsConfig() {
    auto actions = ConfigManager::loadActions("/config/actions.json");
    
    USBSerial.println("\n=== Actions Configuration Debug ===");
    for (const auto& pair : actions) {
        USBSerial.printf("Button ID: %s, Type: %s\n", 
                    pair.first.c_str(), 
                    pair.second.type.c_str());
        
        if (pair.second.type == "multimedia" && !pair.second.consumerReport.empty()) {
            USBSerial.print("  Consumer Report: ");
            for (const auto& hex : pair.second.consumerReport) {
                USBSerial.printf("%s ", hex.c_str());
            }
            USBSerial.println();
        }
    }
    USBSerial.println("==================================\n");
}

void keyboardTask(void *pvParameters) {
    while (true) {
        if (keyHandler) {
            keyHandler->updateKeys();
        }
        vTaskDelay(pdMS_TO_TICKS(10));  // Scan every 10ms (adjust as needed)
    }
}

void encoderTask(void *pvParameters) {
    while (true) {
        if (encoderHandler) {
            encoderHandler->updateEncoders();
        }
        vTaskDelay(pdMS_TO_TICKS(10));  // Adjust the delay as needed (here 10ms)
    }
}

// Separate task for USB Server to avoid blocking the main functionality
void usbServerTask(void *pvParameters) {
    const int retryDelay = 10000; // 10 seconds
    
    // Give the system time to boot up fully
    vTaskDelay(pdMS_TO_TICKS(15000)); // Wait 15 seconds before first attempt
    
    USBSerial.println("Starting USB Server initialization task");
}

void setup() {
    // Initialize USB with both HID and CDC
    USB.begin();
    USBSerial.begin();

    // Wait a bit for Serial to initialize
    delay(8000);

    USBSerial.println(TAG);
    USBSerial.println("Starting device initialization");
    
    // Initialize HID components
    ConsumerControl.begin();
    USBSerial.println("HID Consumer Control initialized");
    
    // Mount SPIFFS for configuration files
    if (!SPIFFS.begin(true)) {
        USBSerial.println("Failed to mount SPIFFS");
    } else {
        USBSerial.println("SPIFFS mounted successfully");
        
        // Create config directory if it doesn't exist
        if (!SPIFFS.exists("/config")) {
            if (SPIFFS.mkdir("/config")) {
                USBSerial.println("Created /config directory");
            } else {
                USBSerial.println("Failed to create /config directory");
            }
        }
        
        // Create web directory if it doesn't exist
        if (!SPIFFS.exists("/web")) {
            if (SPIFFS.mkdir("/web")) {
                USBSerial.println("Created /web directory");
            } else {
                USBSerial.println("Failed to create /web directory");
            }
        }
        
        // Create macros directory if it doesn't exist
        if (!SPIFFS.exists("/macros")) {
            if (SPIFFS.mkdir("/macros")) {
                USBSerial.println("Created /macros directory");
            } else {
                USBSerial.println("Failed to create /macros directory");
            }
        }
    }
    
    // Initialize display
    USBSerial.println("Initializing display...");
    initializeDisplay();
    
    // Initialize module configuration
    USBSerial.println("Initializing module configuration...");
    initializeModuleInfo();
    
    // Optionally, print the merged module info for debugging
    String moduleInfo = getModuleInfoJson();
    USBSerial.println("Module Info:");
    USBSerial.println(moduleInfo);
    
    // Initialize HID handler before other components
    USBSerial.println("Initializing HID Handler...");
    initializeHIDHandler();
    
    // Initialize MacroHandler after HID handler but before other components
    USBSerial.println("Initializing Macro Handler...");
    initializeMacroHandler();
    
    USBSerial.println("Initializing KeyHandler...");
    initializeKeyHandler();
    
    USBSerial.println("Initialize LEDs");
    initializeLED();
    
    USBSerial.println("Initialize Encoders");
    initializeEncoderHandler();
    
    // Initialize WiFi Manager
    USBSerial.println("Initializing WiFi Manager...");
    WiFiManager::begin();

    // Debug actions configuration
    debugActionsConfig();
    
    // Set initial LED colors
    if (strip) {
        // Make a startup animation: all LEDs light up in sequence
        for (int i = 0; i < numLEDs; i++) {
            // Turn on just the current LED
            strip->clear();  // Turn off all LEDs
            strip->setPixelColor(i, strip->Color(0, 255, 0));  // Set just this one green
            strip->show();
            delay(55);  // Slightly longer delay
        }
        delay(500);
        
        // Then set them to their configured colors
        for (int i = 0; i < numLEDs; i++) {
            float factor = ledConfigs[i].brightness / 255.0;
            strip->setPixelColor(i, strip->Color(
                ledConfigs[i].r * factor,
                ledConfigs[i].g * factor,
                ledConfigs[i].b * factor
            ));
        }
        strip->show();
    }
    
    // Create tasks for keyboard and encoder handling
    xTaskCreate(keyboardTask, "keyboard_task", 4096, NULL, 2, NULL);
    xTaskCreate(encoderTask, "encoder_task", 4096, NULL, 2, NULL);

    USBSerial.println("Setup complete - entering main loop");
}

void loop() {
    // Update WiFi Manager
    WiFiManager::update();

    // Update LEDs
    updateLEDs();
    
    // Update display
    static unsigned long lastDisplayUpdate = 0;
    if (millis() - lastDisplayUpdate > 1000) { // Update display only once per second
        updateDisplay();
        lastDisplayUpdate = millis();
    }
    
    // Update macro execution
    updateMacroHandler();

    // Minimal loop - print a heartbeat every 5 seconds
    static unsigned long lastPrint = 0;
    if (millis() - lastPrint > 10000) {
        lastPrint = millis();
        USBSerial.println("Heartbeat...");
        
        // // Optionally print diagnostics every 8 seconds
        // if (keyHandler) {
        //     keyHandler->diagnostics();
        // }
        // if (encoderHandler) {
        //     encoderHandler->diagnostics();
        // }
    }
    
    // No need to call updateKeyHandler here - the task is handling it
    
    // Give other tasks time to run
    delay(20); // Increased delay to reduce update frequency
}