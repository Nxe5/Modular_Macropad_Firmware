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

// First include USB and HID libraries
#include <USB.h>
#include <USBHID.h>
#include <USBHIDKeyboard.h>
#include <USBHIDConsumerControl.h>
#include <USBHIDMouse.h>
#include <USBCDC.h>

// Then include custom modules that might use USB definitions
#include "ModuleSetup.h"
#include "ConfigManager.h"
#include "KeyHandler.h"  
#include "LEDHandler.h"
#include "EncoderHandler.h"
#include "HIDHandler.h"
#include "DisplayHandler.h"
#include "MacroHandler.h"

#include "WiFiManager.h"

#include "FileSystemUtils.h"

#include "VersionManager.h"

#include "OTAUpdateManager.h"
#include "RecoveryBootloader.h"
#include "PartitionVerifier.h"
#include "UpdateProgressDisplay.h"

// Forward declarations
void createWorkingActionsFile();
bool shouldAutoUpdate();
void performFirmwareUpdate();
void handleRecoveryMode();

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

// Helper function to create a working actions.json file
void createWorkingActionsFile() {
    USBSerial.println("\n==== CREATING WORKING ACTIONS FILE ====");
    
    // Create a simple but working actions.json file with common buttons configured
    // Using a simpler structure with just enough buttons to test
    String workingActions = R"({
  "actions": {
    "layers": [
      {
        "layer-name": "default-actions-layer",
        "active": true,
        "layer-config": {
          "button-1": {"type": "cycle-layer"},
          "button-2": {"type": "hid", "buttonPress": ["0x00", "0x00", "0x04", "0x00", "0x00", "0x00", "0x00", "0x00"]},
          "button-3": {"type": "hid", "buttonPress": ["0x00", "0x00", "0x05", "0x00", "0x00", "0x00", "0x00", "0x00"]},
          "button-4": {"type": "hid", "buttonPress": ["0x00", "0x00", "0x06", "0x00", "0x00", "0x00", "0x00", "0x00"]}
        }
      },
      {
        "layer-name": "Nxe5-actions-layer",
        "active": false,
        "layer-config": {
          "button-1": {"type": "cycle-layer"},
          "button-2": {"type": "hid", "buttonPress": ["0x00", "0x00", "0x07", "0x00", "0x00", "0x00", "0x00", "0x00"]},
          "button-3": {"type": "hid", "buttonPress": ["0x00", "0x00", "0x08", "0x00", "0x00", "0x00", "0x00", "0x00"]},
          "button-4": {"type": "hid", "buttonPress": ["0x00", "0x00", "0x09", "0x00", "0x00", "0x00", "0x00", "0x00"]}
        }
      }
    ]
  }
})";

    // Create both possible directory paths
    FileSystemUtils::createDirPath("/data/config");
    FileSystemUtils::createDirPath("/config");
    
    // Save to both possible locations to ensure it's found
    bool success1 = FileSystemUtils::writeFile("/data/config/actions.json", workingActions);
    bool success2 = FileSystemUtils::writeFile("/config/actions.json", workingActions);
    
    if (success1 && success2) {
        USBSerial.println("Created working actions.json files in both locations");
    } else if (success1) {
        USBSerial.println("Created working actions.json file in /data/config only");
    } else if (success2) {
        USBSerial.println("Created working actions.json file in /config only");
    } else {
        USBSerial.println("Failed to create working actions.json file in either location");
    }
}

void keyboardTask(void *pvParameters) {
    while (true) {
        if (keyHandler) {
            keyHandler->updateKeys();
        }
        vTaskDelay(pdMS_TO_TICKS(10));  // Scan every 10ms (adjust as needed)
    }
}

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
        
        if (actions.empty()) {
            USBSerial.println("WARNING: No actions loaded from config file!");
            
            // Try the backup default configuration or create a simple one
            USBSerial.println("Creating default working actions configuration");
            createWorkingActionsFile();
            actions = ConfigManager::loadActions("/data/config/actions.json");
        }
        
        keyHandler->loadKeyConfiguration(actions);
        
        // Make sure to apply the current layer configurations
        String currentLayer = keyHandler->getCurrentLayer();
        USBSerial.printf("Applying current layer: %s\n", currentLayer.c_str());
        keyHandler->applyLayerToActionMap(currentLayer);
        
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
    
    // No longer create files in /data/config
    
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

// Helper function to dump actions.json file content
void dumpActionsFile() {
    USBSerial.println("\n==== ACTIONS FILE DUMP ====");
    
    // Check both possible locations
    const char* paths[] = {"/config/actions.json", "/data/config/actions.json"};
    
    for (const char* path : paths) {
        USBSerial.printf("Checking path: %s\n", path);
        
        if (FileSystemUtils::fileExists(path)) {
            USBSerial.printf("File exists at %s\n", path);
            
            // Get file size
            File file = LittleFS.open(path, "r");
            if (file) {
                size_t fileSize = file.size();
                USBSerial.printf("File size: %d bytes\n", fileSize);
                
                // Read and print file contents
                if (fileSize > 0) {
                    String content = file.readString();
                    USBSerial.println("File content:");
                    USBSerial.println(content);
                } else {
                    USBSerial.println("File is empty!");
                }
                file.close();
            } else {
                USBSerial.println("Failed to open file");
            }
        } else {
            USBSerial.printf("File does not exist at %s\n", path);
        }
    }
    
    USBSerial.println("==== END OF DUMP ====\n");
}

// Helper function for detailed filesystem diagnostics
void runFilesystemDiagnostics() {
    USBSerial.println("\n==== DETAILED FILESYSTEM DIAGNOSTICS ====");
    
    // Check if LittleFS is mounted
    if (!LittleFS.begin(false)) {
        USBSerial.println("ERROR: LittleFS not mounted!");
        return;
    }
    
    // Get filesystem info - this is different for ESP32
    USBSerial.printf("Filesystem Info:\n");
    USBSerial.printf("  Total space: %u bytes\n", LittleFS.totalBytes());
    USBSerial.printf("  Used space: %u bytes\n", LittleFS.usedBytes());
    USBSerial.printf("  Free space: %u bytes\n", LittleFS.totalBytes() - LittleFS.usedBytes());
    
    // Check and list root directory
    File root = LittleFS.open("/");
    if (!root) {
        USBSerial.println("ERROR: Failed to open root directory");
        return;
    }
    
    if (!root.isDirectory()) {
        USBSerial.println("ERROR: Root is not a directory");
        return;
    }
    
    // List base directories
    USBSerial.println("\nBase directories:");
    String baseDirs[] = {"/data", "/data/config", "/config", "/web", "/macros"};
    for (const String& dir : baseDirs) {
        if (LittleFS.exists(dir)) {
            USBSerial.printf("  %s - EXISTS\n", dir.c_str());
            
            // Check if really a directory
            File testDir = LittleFS.open(dir);
            if (!testDir) {
                USBSerial.printf("    ERROR: Could not open %s\n", dir.c_str());
                continue;
            }
            
            if (!testDir.isDirectory()) {
                USBSerial.printf("    WARNING: %s exists but is not a directory!\n", dir.c_str());
                continue;
            }
            
            // List files in this directory
            USBSerial.printf("    Files in %s:\n", dir.c_str());
            File dirObj = LittleFS.open(dir);
            File file = dirObj.openNextFile();
            bool filesFound = false;
            
            while (file) {
                filesFound = true;
                String fileName = file.name();
                if (fileName.startsWith("/")) {
                    fileName = fileName.substring(1); // Remove leading slash
                }
                
                if (file.isDirectory()) {
                    USBSerial.printf("      DIR: %s\n", fileName.c_str());
                } else {
                    USBSerial.printf("      FILE: %s (%u bytes)\n", fileName.c_str(), file.size());
                }
                file = dirObj.openNextFile();
            }
            
            if (!filesFound) {
                USBSerial.println("      No files found");
            }
            
        } else {
            USBSerial.printf("  %s - MISSING\n", dir.c_str());
        }
    }
    
    // Check specific important files
    USBSerial.println("\nImportant configuration files:");
    String important[] = {
        "/data/config/actions.json", 
        "/config/actions.json",
        "/data/config/components.json",
        "/config/components.json"
    };
    
    for (const String& path : important) {
        if (LittleFS.exists(path)) {
            File file = LittleFS.open(path, "r");
            if (!file) {
                USBSerial.printf("  %s - EXISTS but cannot be opened\n", path.c_str());
                continue;
            }
            
            size_t size = file.size();
            USBSerial.printf("  %s - EXISTS (%u bytes)\n", path.c_str(), size);
            
            // Print file preview (first 200 chars)
            if (size > 0) {
                USBSerial.println("  --- File Content Preview: ---");
                size_t previewLen = min(200, (int)size);
                char preview[201] = {0};
                size_t read = file.readBytes(preview, previewLen);
                preview[read] = '\0';
                USBSerial.println(preview);
                USBSerial.println("  --- End Preview ---");
                
                // Check if it's valid JSON
                if (path.endsWith(".json")) {
                    file.seek(0); // Reset to beginning
                    String content = file.readString();
                    
                    // Try to parse with DynamicJsonDocument
                    USBSerial.println("  Checking JSON validity...");
                    DynamicJsonDocument doc(4096);
                    DeserializationError error = deserializeJson(doc, content);
                    
                    if (error) {
                        USBSerial.printf("  JSON INVALID: %s\n", error.c_str());
                    } else {
                        USBSerial.println("  JSON is valid");
                        
                        // For actions.json, check layer-config
                        if (path.endsWith("actions.json")) {
                            if (doc.containsKey("actions") && doc["actions"].containsKey("layer-config")) {
                                JsonObject layerConfig = doc["actions"]["layer-config"];
                                int buttonCount = 0;
                                for (JsonPair p : layerConfig) {
                                    buttonCount++;
                                }
                                USBSerial.printf("  Contains layer-config with %d button configurations\n", buttonCount);
                            } else {
                                USBSerial.println("  WARNING: Missing actions or layer-config structure!");
                            }
                        }
                    }
                }
            } else {
                USBSerial.println("  File is empty!");
            }
            file.close();
        } else {
            USBSerial.printf("  %s - MISSING\n", path.c_str());
        }
    }
    
    USBSerial.println("==== END DIAGNOSTICS ====\n");
}

// Helper to check if automatic updates are enabled
bool shouldAutoUpdate() {
    // This could be a setting in the configuration
    // For now, return false (manual updates only)
    return false;
}

// Perform the firmware update
void performFirmwareUpdate() {
    if (!OTAUpdateManager::isUpdateAvailable()) {
        USBSerial.println("No update available to perform");
        return;
    }
    
    USBSerial.println("Starting firmware update to version " + OTAUpdateManager::getAvailableVersion());
    
    // Set up a progress callback for display updates
    auto progressCallback = [](size_t current, size_t total, int percentage) {
        UpdateProgressDisplay::updateProgress(current, total, percentage);
    };
    
    // Show initial progress screen
    UpdateProgressDisplay::drawProgressScreen("Firmware Update", 0, "Starting update...");
    
    // Perform the update with progress callback
    if (OTAUpdateManager::performUpdate(OTAUpdateManager::getFirmwareUrl(), progressCallback)) {
        // Update successful - this will only be reached if the restart after update fails
        UpdateProgressDisplay::drawSuccessScreen("Update complete");
    } else {
        // Update failed
        USBSerial.println("Update failed: " + OTAUpdateManager::getLastError());
        UpdateProgressDisplay::drawErrorScreen(OTAUpdateManager::getLastError());
    }
}

// Handle recovery mode
void handleRecoveryMode() {
    static bool recoveryScreenShown = false;
    
    if (!recoveryScreenShown) {
        // Show recovery screen
        UpdateProgressDisplay::drawRecoveryScreen(RecoveryBootloader::getStatusMessage());
        recoveryScreenShown = true;
        
        USBSerial.println("Device is in recovery mode");
        USBSerial.println("Reason: " + RecoveryBootloader::getStatusMessage());
        
        // Attempt recovery from failed update if needed
        if (OTAUpdateManager::isInRecoveryMode()) {
            USBSerial.println("Attempting to recover from failed update...");
            if (OTAUpdateManager::rollbackFirmware()) {
                USBSerial.println("Rollback successful, restarting...");
                delay(1000);
                ESP.restart();
            } else {
                USBSerial.println("Rollback failed: " + OTAUpdateManager::getLastError());
            }
        }
    }
    
    // Handle WiFi in recovery mode to allow OTA updates
    WiFiManager::update();
    
    // Minimal recovery mode loop
    delay(100);
}

void setup() {
    // Initialize the recovery bootloader first (before any other components)
    RecoveryBootloader::begin();
    
    // Check if we are in recovery mode
    if (RecoveryBootloader::shouldEnterRecoveryMode()) {
        // Recovery mode will be handled in the loop
        // Continue with initialization to ensure critical components are available
    }
    
    // Initialize USB in Serial mode
    USB.begin();
    USBSerial.begin();

    // Wait a bit for Serial to initialize
    delay(8000);

    USBSerial.println(TAG);
    USBSerial.println("Starting device initialization");
    
    // Print memory diagnostics at startup
    USBSerial.printf("Initial free heap: %d bytes\n", ESP.getFreeHeap());
    USBSerial.printf("Total heap size: %d bytes\n", ESP.getHeapSize());
    
    #ifdef BOARD_HAS_PSRAM
    USBSerial.printf("PSRAM size: %d bytes\n", ESP.getPsramSize());
    USBSerial.printf("Free PSRAM: %d bytes\n", ESP.getFreePsram());
    #endif
    
    // Initialize HID components
    ConsumerControl.begin();
    USBSerial.println("HID Consumer Control initialized");
    Mouse.begin();
    USBSerial.println("HID Mouse initialized");
    Keyboard.begin();
    USBSerial.println("HID Keyboard initialized");
    
    // Initialize LittleFS with improved approach
    USBSerial.println("Initializing filesystem...");
    if (FileSystemUtils::begin(true)) {
        USBSerial.println("LittleFS filesystem is operational");
        
        // Create only the necessary directories matching old firmware
        FileSystemUtils::createDirPath("/config");
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
    
    // Initialize update progress display
    USBSerial.println("Initializing update progress display...");
    UpdateProgressDisplay::begin(getDisplay());
    
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

    // Initialize version manager
    Serial.println("=== Device Information ===");
    Serial.println("Device: " + VersionManager::getDeviceName());
    Serial.println("Manufacturer: " + VersionManager::getDeviceManufacturer());
    Serial.println("Model: " + VersionManager::getDeviceModel());
    Serial.println("Firmware Version: " + VersionManager::getVersionString());
    Serial.println("Build Number: " + String(VersionManager::getBuildNumber()));
    Serial.println("Build Date: " + VersionManager::getBuildDate());
    Serial.println("Build Time: " + VersionManager::getBuildTime());
    Serial.println("========================");
    
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

    // Initialize OTA Update Manager
    USBSerial.println("Initializing OTA Update Manager...");
    OTAUpdateManager::begin();
    
    // Verify boot integrity
    if (!OTAUpdateManager::verifyBootIntegrity()) {
        USBSerial.println("Boot integrity check failed: " + OTAUpdateManager::getLastError());
    } else {
        USBSerial.println("Boot integrity verified");
    }
    
    // Verify partition integrity
    USBSerial.println("Verifying partition integrity...");
    if (PartitionVerifier::verifyOTAPartition()) {
        USBSerial.println("OTA partition integrity verified");
    } else {
        USBSerial.println("OTA partition integrity check failed: " + PartitionVerifier::getLastError());
    }
    
    // Print partition information
    USBSerial.println(PartitionVerifier::getAllPartitionsInfo());

    USBSerial.println("Setup complete - entering main loop");
}

void loop() {
    // Check if in recovery mode
    if (RecoveryBootloader::getBootloaderState() == RecoveryBootloader::RECOVERY_MODE) {
        handleRecoveryMode();
        return;  // Skip normal loop processing in recovery mode
    }
    
    // Update OTA progress display
    UpdateProgressDisplay::process();
    
    // Update WiFi Manager
    WiFiManager::update();

    // Update LEDs
    updateLEDs();
    
    // Update display
    updateDisplay();
    
    // Update macro execution
    updateMacroHandler();

    // Update HID handler
    updateHIDHandler();

    // Run LittleFS diagnostics if enabled
    if (diagnosticsEnabled) {
        runDiagnostics();
    }

    // Minimal loop - print a heartbeat every 10 seconds
    static unsigned long lastPrint = 0;
    if (millis() - lastPrint > 10000) {
        lastPrint = millis();
        USBSerial.println("Heartbeat...");
        
        // Enable this section if you need diagnostic information
        /*
        // Available variables for display
        USBSerial.println("\n=== AVAILABLE VARIABLES FOR DISPLAY ====");
        USBSerial.printf("current_mode: %s\n", currentMode.name.c_str());
        USBSerial.printf("wifi_status: %s\n", WiFiManager::isConnected() ? "Connected" : "Disconnected");
        USBSerial.printf("ip_address: %s\n", WiFiManager::getLocalIP().toString().c_str());
        USBSerial.printf("layer: %s\n", keyHandler ? keyHandler->getCurrentLayer().c_str() : "default");
        USBSerial.printf("macro_status: %s\n", (macroHandler && macroHandler->isExecuting()) ? "Running" : "Ready");
        USBSerial.printf("SSID: %s\n", WiFiManager::getSSID().c_str());
        USBSerial.printf("Is AP Mode: %s\n", WiFiManager::isAPMode() ? "Yes" : "No");
        USBSerial.println("========================================\n");
        */
        
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

