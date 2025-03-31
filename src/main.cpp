#include <Arduino.h>
#include <USBCDC.h>
#include "esp_netif.h"
#include "lwip/ip_addr.h"
#include "lwip/inet.h"

#include "FS.h"
// #include "SPIFFS.h" - Replaced with LittleFS
#include <LittleFS.h>
#include "ArduinoJson.h"

// Directly include ESP32 LittleFS implementation headers for more control
#include "esp_littlefs.h"

// Forward declarations for internal LittleFS functions if not already defined
extern "C" {
    esp_err_t esp_littlefs_format(const char* partition_label);
    
    typedef struct {
        const char* partition_label;
        size_t partition_size;
        bool dont_mount;
    } esp_littlefs_format_opts_t;
    
    esp_err_t esp_littlefs_format_opts(const esp_littlefs_format_opts_t* opts);
}

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

#include "FileSystemUtils.h"

// Initialize USB devices
USBHIDKeyboard Keyboard;
USBHIDConsumerControl ConsumerControl;
USBHIDMouse Mouse;  // Optional if you need mouse controls
USBCDC USBSerial;

// Forward declarations for Display functions
extern void updateDisplay();

// LittleFS Diagnostics functions for troubleshooting
// Global state for diagnostics
bool diagnosticsEnabled = false;
unsigned long lastDiagnosticTime = 0;
uint8_t currentTest = 0;
bool testCompleted = false;

// Enhanced filesystem diagnostics with our FileSystemUtils
void checkStorage() {
    size_t totalBytes = LittleFS.totalBytes();
    size_t usedBytes = LittleFS.usedBytes();
    size_t freeBytes = totalBytes - usedBytes;
    
    USBSerial.printf("LittleFS: %u total bytes, %u used bytes, %u free bytes\n", 
                    totalBytes, usedBytes, freeBytes);
    USBSerial.printf("Storage usage: %.1f%%\n", (float)usedBytes * 100 / totalBytes);
    
    if (freeBytes < 50000) {  // Warn if less than 50KB free
        USBSerial.println("WARNING: Low storage space on LittleFS!");
    }
}

// Test if path length is an issue
void testPathLength() {
    USBSerial.println("Testing path length limitations...");
    
    // Test with short path
    if (FileSystemUtils::writeFile("/test_short.txt", "test")) {
        USBSerial.println("Short path file created successfully");
        LittleFS.remove("/test_short.txt");
    } else {
        USBSerial.println("Failed to create short path file");
    }
    
    // Test with path similar to the problematic ones
    if (FileSystemUtils::writeFile("/web/_app/immutable/nodes/test_long.js", "test")) {
        USBSerial.println("Long path file created successfully");
        FileSystemUtils::deleteFileAndDirs("/web/_app/immutable/nodes/test_long.js");
    } else {
        USBSerial.println("Failed to create long path file - PATH LENGTH ISSUE CONFIRMED");
    }
}

// Test if filename restrictions are an issue
void testFilenames() {
    USBSerial.println("Testing filename restrictions...");
    
    // Test with simple filename
    if (FileSystemUtils::writeFile("/simple.js", "test")) {
        USBSerial.println("Simple filename works");
        FileSystemUtils::deleteFileAndDirs("/simple.js");
    } else {
        USBSerial.println("Failed to create simple filename");
    }
    
    // Test with hash-based filename (similar to SvelteKit output)
    if (FileSystemUtils::writeFile("/test.DWAvjrHy.js", "test")) {
        USBSerial.println("Hash-based filename works");
        FileSystemUtils::deleteFileAndDirs("/test.DWAvjrHy.js");
    } else {
        USBSerial.println("Failed to create hash-based filename - FILENAME ISSUE CONFIRMED");
    }
}

// Test for fragmentation by writing and deleting files
void testFragmentation() {
    USBSerial.println("Testing for fragmentation issues...");
    
    // Create a set of small files
    for (int i = 0; i < 10; i++) {
        String filename = "/frag_test_" + String(i) + ".txt";
        String content = "";
        // Write 1KB of data
        for (int j = 0; j < 20; j++) {
            content += "This is test data for fragmentation testing. Line " + String(j) + "\n";
        }
        
        if (!FileSystemUtils::writeFile(filename.c_str(), content)) {
            USBSerial.println("Failed to create test file - possible FRAGMENTATION ISSUE");
            break;
        }
    }
    
    // Try a more comprehensive performance test
    FileSystemUtils::testPerformance("/large_test.bin", 1024, 50);
    
    // Clean up test files
    for (int i = 0; i < 10; i++) {
        String filename = "/frag_test_" + String(i) + ".txt";
        FileSystemUtils::deleteFileAndDirs(filename.c_str());
    }
}

// Run diagnostics sequentially
void runDiagnostics() {
    if (!diagnosticsEnabled || millis() - lastDiagnosticTime < 5000) {
        return;
    }
    
    lastDiagnosticTime = millis();
    
    switch (currentTest) {
        case 0:
            USBSerial.println("\n--- LITTLEFS DIAGNOSTICS: STORAGE CHECK ---");
            checkStorage();
            currentTest++;
            break;
            
        case 1:
            USBSerial.println("\n--- LITTLEFS DIAGNOSTICS: PATH LENGTH TEST ---");
            testPathLength();
            currentTest++;
            break;
            
        case 2:
            USBSerial.println("\n--- LITTLEFS DIAGNOSTICS: FILENAME TEST ---");
            testFilenames();
            currentTest++;
            break;
            
        case 3:
            USBSerial.println("\n--- LITTLEFS DIAGNOSTICS: FRAGMENTATION TEST ---");
            testFragmentation();
            currentTest++;
            break;
            
        default:
            if (!testCompleted) {
                USBSerial.println("\n--- LITTLEFS DIAGNOSTICS COMPLETE ---");
                testCompleted = true;
            }
            break;
    }
}

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

// Create default configuration files if they don't exist using improved approach
void createDefaultConfigFiles() {
    USBSerial.println("Creating default config files...");
    
    // Default components.json
    String defaultComponents = R"({
        "components": [
            {
                "id": "button-0",
                "type": "button",
                "size": { "rows": 1, "columns": 1 },
                "start_location": { "row": 0, "column": 0 }
            }
        ]
    })";
    
    // Default actions.json
    String defaultActions = R"({
        "actions": {
            "layer-config": {
                "button-0": {
                    "type": "hid",
                    "buttonPress": ["0x00", "0x00", "0x04", "0x00", "0x00", "0x00", "0x00", "0x00"]
                }
            }
        }
    })";
    
    // Default reports.json
    String defaultReports = R"({
        "reports": {
            "hid": {
                "0x00_0x00_0x04_0x00_0x00_0x00_0x00_0x00": "a"
            },
            "consumer": {}
        }
    })";
    
    // Default info.json
    String defaultInfo = R"({
        "name": "Modular Macropad",
        "version": "1.0.0",
        "author": "User",
        "description": "Default configuration",
        "module-size": "full",
        "gridSize": { "rows": 3, "columns": 4 },
        "defaults": {},
        "settings": {},
        "supportedComponentTypes": ["button", "encoder", "display"]
    })";

    // Create config files using our new utility class with automatic directory creation
    bool success = true;
    
    if (!FileSystemUtils::fileExists("/config/components.json")) {
        success &= FileSystemUtils::writeFile("/config/components.json", defaultComponents);
    }
    
    if (!FileSystemUtils::fileExists("/config/actions.json")) {
        success &= FileSystemUtils::writeFile("/config/actions.json", defaultActions);
    }
    
    if (!FileSystemUtils::fileExists("/config/reports.json")) {
        success &= FileSystemUtils::writeFile("/config/reports.json", defaultReports);
    }
    
    if (!FileSystemUtils::fileExists("/config/info.json")) {
        success &= FileSystemUtils::writeFile("/config/info.json", defaultInfo);
    }
    
    // Also create compatibility files in /data/config
    if (!FileSystemUtils::fileExists("/data/config/reports.json")) {
        success &= FileSystemUtils::writeFile("/data/config/reports.json", defaultReports);
    }
    
    if (!FileSystemUtils::fileExists("/data/config/actions.json")) {
        success &= FileSystemUtils::writeFile("/data/config/actions.json", defaultActions);
    }
    
    if (success) {
        USBSerial.println("All default config files created successfully");
    } else {
        USBSerial.println("Some config files could not be created");
    }
}

// List all files in a directory recursively using the new utility class
void listDir(const char * dirname, uint8_t levels) {
    FileSystemUtils::listDir(dirname, levels);
}

void setup() {
    // Initialize USB in Serial mode
    USB.begin();
    USBSerial.begin();

    // Wait a bit for Serial to initialize
    delay(8000);

    USBSerial.println(TAG);
    USBSerial.println("Starting device initialization");
    
    // Initialize HID components
    ConsumerControl.begin();
    USBSerial.println("HID Consumer Control initialized");
    
    // Initialize LittleFS with improved approach
    USBSerial.println("Initializing filesystem...");
    if (FileSystemUtils::begin(true)) {
        USBSerial.println("LittleFS filesystem is operational");
        
        // Create base directories
        FileSystemUtils::createDirPath("/config");
        FileSystemUtils::createDirPath("/data/config");
        FileSystemUtils::createDirPath("/web");
        FileSystemUtils::createDirPath("/macros");
        
        // Create default configuration files
        createDefaultConfigFiles();
        
        // List all files in the filesystem
        USBSerial.println("Filesystem contents:");
        FileSystemUtils::listDir("/", 2);
    } else {
        USBSerial.println("WARNING: Continuing without functional filesystem");
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

    // Run LittleFS diagnostics if enabled
    if (diagnosticsEnabled) {
        runDiagnostics();
    }

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
    
    // Check Serial for commands
    if (USBSerial.available()) {
        String command = USBSerial.readStringUntil('\n');
        command.trim();
        
        if (command == "diagnostics") {
            USBSerial.println("Starting LittleFS diagnostics...");
            diagnosticsEnabled = true;
            currentTest = 0;
            testCompleted = false;
            lastDiagnosticTime = 0; // Start immediately
        } else if (command == "stop") {
            USBSerial.println("Stopping diagnostics...");
            diagnosticsEnabled = false;
        } else if (command == "storage") {
            checkStorage();
        } else if (command == "pathtest") {
            testPathLength();
        } else if (command == "filenametest") {
            testFilenames();
        } else if (command == "fragtest") {
            testFragmentation();
        } else if (command == "format") {
            USBSerial.println("Formatting LittleFS...");
            LittleFS.format();
            USBSerial.println("LittleFS formatted. Restarting...");
            ESP.restart();
        } else if (command == "help") {
            USBSerial.println("\nAvailable commands:");
            USBSerial.println("  diagnostics - Run all filesystem tests in sequence");
            USBSerial.println("  stop - Stop running diagnostics");
            USBSerial.println("  storage - Check available storage space");
            USBSerial.println("  pathtest - Test path length limitations");
            USBSerial.println("  filenametest - Test filename restrictions");
            USBSerial.println("  fragtest - Test for filesystem fragmentation");
            USBSerial.println("  format - Format LittleFS filesystem (Warning: Deletes all files!)");
            USBSerial.println("  help - Show this help message");
        }
    }
    
    // No need to call updateKeyHandler here - the task is handling it
    
    // Give other tasks time to run
    delay(20); // Increased delay to reduce update frequency
}

