// DisplayHandler.cpp
#include "DisplayHandler.h"
#include "WiFiManager.h"
#include "KeyHandler.h"
#include "MacroHandler.h"
#include "HIDHandler.h"
#include <LittleFS.h>
#include <Arduino.h>
#include <JPEGDEC.h> // Include the JPEG decoder library

// External declaration for USBSerial
extern USBCDC USBSerial;

// External references
extern KeyHandler* keyHandler;
extern MacroHandler* macroHandler;

// Global variables
Adafruit_ST7789* display = nullptr;
bool temporaryMessageActive = false;
uint32_t temporaryMessageTimeout = 0;
static uint32_t lastDisplayUpdate = 0;

// Display configuration
String activeMode = "config";
DisplayMode currentMode;
std::map<String, DisplayMode> displayModes;
String lastNormalContent = "";
static bool screenInitialized = false;

// Add these global variables at the top with other globals
bool backgroundLoaded = false;
uint16_t* backgroundBuffer = nullptr;

// Global JPEG decoder instance
JPEGDEC jpeg;

// Callback function for JPEGDEC
// This function is called for each block of pixels decoded by the library
int jpegDrawCallback(JPEGDRAW *pDraw) {
    // Check if the background buffer exists
    if (!backgroundBuffer) {
        USBSerial.println("ERROR: JPEG callback called but backgroundBuffer is NULL");
        return 0; // Return 0 to stop decoding
    }
    
    USBSerial.printf("JPEG block: x=%d, y=%d, w=%d, h=%d\n", 
                   pDraw->x, pDraw->y, pDraw->iWidth, pDraw->iHeight);
    
    // Get image dimensions from the JPEG decoder
    int jpegWidth = jpeg.getWidth();
    int jpegHeight = jpeg.getHeight();
    
    USBSerial.printf("JPEG dimensions: %dx%d, display buffer: 240x280\n", 
                   jpegWidth, jpegHeight);
    
    // Copy the decoded pixel block to the buffer with dimension correction
    for (int y = 0; y < pDraw->iHeight; y++) {
        for (int x = 0; x < pDraw->iWidth; x++) {
            // Calculate source index in the JPEG decode buffer
            int srcIndex = y * pDraw->iWidth + x;
            
            // Get destination x,y based on actual JPEG dimensions vs. display dimensions
            int destX, destY;
            
            if (jpegWidth == 280 && jpegHeight == 240) {
                // For 280x240 JPEGs (landscape image with swapped dimensions)
                // We need to transpose the coordinates
                destX = pDraw->y + y;
                destY = pDraw->x + x;
                
                // Check bounds - don't write outside our 240x280 buffer
                if (destX >= 280 || destY >= 240) continue;
            } else {
                // Standard mapping for correctly dimensioned images
                destX = pDraw->x + x;
                destY = pDraw->y + y;
                
                // Check bounds
                if (destX >= 240 || destY >= 280) continue;
            }
            
            // Calculate buffer index from destination coordinates
            uint32_t bufferIndex = destY * 240 + destX;
            
            // Final bounds check
            if (bufferIndex < (240 * 280)) {
                // Copy the pixel
                uint16_t pixel = pDraw->pPixels[srcIndex];
                backgroundBuffer[bufferIndex] = pixel;
            } else {
                USBSerial.printf("WARNING: Buffer index out of bounds: %lu\n", bufferIndex);
            }
        }
    }
    
    // Return 1 to continue decoding
    return 1;
}

void initializeDisplay() {
    USBSerial.println("Starting display initialization...");
    
    // Initialize SPI with HSPI
    SPIClass* spi = new SPIClass(HSPI);
    spi->begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
    
    // Create display with SPI instance
    display = new Adafruit_ST7789(spi, TFT_CS, TFT_DC, TFT_RST);
    
    USBSerial.println("Display object created");
    
    // Set SPI speed and initialize
    display->setSPISpeed(80000000);
    display->init(240, 280, SPI_MODE0);
    
    USBSerial.println("Display initialized");
    
    // Set rotation to landscape mode (1 = 90 degrees, 3 = 270 degrees)
    display->setRotation(1);
    display->fillScreen(ST77XX_BLACK);
    
    // Mark the screen as initialized before doing anything else
    screenInitialized = true;
    
    // Load display configuration first
    loadDisplayConfig();
    
    // Load background image
    loadBackgroundImage();
    
    // Show welcome splash screen
    display->fillScreen(ST77XX_BLACK);
    display->setTextSize(4);
    display->setTextColor(ST77XX_WHITE);
    
    // Center "Welcome!" text
    int16_t x1, y1;
    uint16_t w, h;
    const char* welcomeText = "Welcome!";
    display->getTextBounds(welcomeText, 0, 0, &x1, &y1, &w, &h);
    display->setCursor((240 - w) / 2, 100);
    display->println(welcomeText);
    
    // Delay to show welcome screen
    delay(2000);
    
    // Skip all intermediate screens and go directly to the main display
    USBSerial.println("Going directly to main display");
    
    // Make sure to completely disable temporary message handling
    temporaryMessageActive = false;
    
    // Set the active mode to "main" to use our new layout
    activeMode = "main";
    
    // Display the main layout directly
    displayMainLayout();
    
    USBSerial.println("Display setup complete");
}

Adafruit_ST7789* getDisplay() {
    return display;
}

void printText(const char* text, int x, int y, uint16_t color, uint8_t size) {
    if (!display) return;
    display->setTextSize(size);
    display->setTextColor(color);
    display->setCursor(x, y);
    display->print(text);
}

void drawTestPattern() {
    if (!display) {
        USBSerial.println("Cannot draw test pattern - display not initialized");
        return;
    }
    
    // Force screen initialization if display exists
    if (!screenInitialized) {
        screenInitialized = true;
    }
    
    USBSerial.println("Drawing diagnostic test pattern");
    
    // Save the current state for restoration
    bool wasTemporaryMessageActive = temporaryMessageActive;
    
    // Clear the screen
    display->fillScreen(ST77XX_BLACK);
    
    // Draw some basic shapes
    display->fillRect(10, 10, 50, 50, ST77XX_RED);
    display->fillRect(70, 10, 50, 50, ST77XX_GREEN);
    display->fillRect(130, 10, 50, 50, ST77XX_BLUE);
    
    // Draw some text
    display->setTextColor(ST77XX_WHITE);
    display->setTextSize(2);
    display->setCursor(50, 80);
    display->println("DIAGNOSTIC TEST");
    
    display->setTextSize(1);
    display->setCursor(20, 120);
    display->println("Display is working correctly");
    
    // Draw a line
    display->drawLine(10, 150, 200, 150, ST77XX_YELLOW);
    
    USBSerial.println("Diagnostic test pattern drawn");
    
    // Note: This is only for diagnostics, we don't set temporaryMessageActive
    // to avoid interfering with normal operation
}

void updateDisplay() {
    // Only update if display is initialized
    if (!display) {
        USBSerial.println("ERROR: Display object is NULL");
        return;
    }
    
    // Ensure screen is marked as initialized if display exists
    if (!screenInitialized && display) {
        screenInitialized = true;
    }
    
    // Rate limit display updates to prevent flickering
    uint32_t currentTime = millis();
    if (currentTime - lastDisplayUpdate < 500) { // Increased to 500ms to reduce flickering
        return;
    }
    lastDisplayUpdate = currentTime;
    
    // Check if we need to clear a temporary message
    if (temporaryMessageActive) {
        checkTemporaryMessage();
        if (!temporaryMessageActive) {
            // Switch to main layout after temporary message
            displayMainLayout();
        }
        return;
    }
    
    // Store current state values to detect changes
    static String lastLayer = "";
    static String lastWifiStatus = "";
    static String lastIpAddress = "";
    static String lastMacroStatus = "";
    
    // Get current state values
    String currentLayer = keyHandler ? keyHandler->getCurrentLayer() : "default";
    String currentWifiStatus = WiFiManager::isConnected() ? "Connected" : "Disconnected";
    String currentIpAddress = WiFiManager::getLocalIP().toString();
    String currentMacroStatus = (macroHandler && macroHandler->isExecuting()) ? "Running" : "Ready";
    
    // Check if any state has changed
    bool stateChanged = (lastLayer != currentLayer) || 
                        (lastWifiStatus != currentWifiStatus) || 
                        (lastIpAddress != currentIpAddress) || 
                        (lastMacroStatus != currentMacroStatus);
    
    // Update last state values
    lastLayer = currentLayer;
    lastWifiStatus = currentWifiStatus;
    lastIpAddress = currentIpAddress;
    lastMacroStatus = currentMacroStatus;
    
    // If no state has changed, skip redrawing to prevent flicker
    if (!stateChanged) {
        return;
    }
    
    // Display the main layout
    displayMainLayout();
}

void loadDisplayConfig() {
    if (!LittleFS.exists(DISPLAY_CONFIG_PATH)) {
        USBSerial.println("Display config not found, using defaults");
        return;
    }
    
    File file = LittleFS.open(DISPLAY_CONFIG_PATH, "r");
    if (!file) {
        USBSerial.println("Failed to open display config");
        return;
    }
    
    // Parse JSON configuration
    DynamicJsonDocument doc(4096);
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    
    if (error) {
        USBSerial.print("Failed to parse display config: ");
        USBSerial.println(error.c_str());
        return;
    }
    
    // Clear existing modes
    displayModes.clear();
    
    // Load active mode
    activeMode = doc["active_mode"].as<String>();
    
    // Load all display modes
    JsonObject modes = doc["modes"];
    for (JsonPair mode : modes) {
        DisplayMode displayMode;
        displayMode.name = mode.value()["name"].as<String>();
        displayMode.description = mode.value()["description"].as<String>();
        displayMode.refresh_rate = mode.value()["refresh_rate"] | DISPLAY_UPDATE_INTERVAL;
        
        // Properly load the background image path
        if (mode.value().containsKey("backgroundImage")) {
            displayMode.backgroundImage = mode.value()["backgroundImage"].as<String>();
            USBSerial.printf("Loading mode: %s with background image: %s\n", 
                           displayMode.name.c_str(), displayMode.backgroundImage.c_str());
        } else {
            displayMode.backgroundImage = "";
            USBSerial.printf("Loading mode: %s with no background image\n", displayMode.name.c_str());
        }
        
        USBSerial.printf("Loading mode: %s with refresh rate: %d\n", 
                        displayMode.name.c_str(), displayMode.refresh_rate);
        
        // Load elements
        JsonArray elements = mode.value()["elements"].as<JsonArray>();
        USBSerial.printf("Found %d elements in JSON array\n", elements.size());
        
        for (JsonVariant element : elements) {
            DisplayElement displayElement;
            JsonObject elementObj = element.as<JsonObject>();
            USBSerial.printf("Loading element of type: %s\n", elementObj["type"].as<String>().c_str());
            
            // Common properties
            displayElement.x = elementObj["x"] | 0;
            displayElement.y = elementObj["y"] | 0;
            
            // Parse color (default to white)
            String colorStr = elementObj["color"] | "white";
            USBSerial.printf("Color string: %s\n", colorStr.c_str());
            
            // Check if it's a hex value (starts with 0x)
            if (colorStr.startsWith("0x")) {
                // Convert hex string to integer
                char* endPtr;
                uint32_t colorValue = strtol(colorStr.c_str(), &endPtr, 16);
                displayElement.color = colorValue;
                USBSerial.printf("Using hex color: 0x%X\n", displayElement.color);
            }
            else if (colorStr == "white") {
                displayElement.color = ST77XX_WHITE;
                USBSerial.println("Using WHITE color");
            }
            else if (colorStr == "red") {
                displayElement.color = ST77XX_RED;
                USBSerial.println("Using RED color");
            }
            else if (colorStr == "green") {
                displayElement.color = ST77XX_GREEN;
                USBSerial.println("Using GREEN color");
            }
            else if (colorStr == "blue") {
                displayElement.color = ST77XX_BLUE;
                USBSerial.println("Using BLUE color");
            }
            else if (colorStr == "yellow") {
                displayElement.color = ST77XX_YELLOW;
                USBSerial.println("Using YELLOW color");
            }
            else if (colorStr == "cyan") {
                displayElement.color = ST77XX_CYAN;
                USBSerial.println("Using CYAN color");
            }
            else {
                displayElement.color = ST77XX_WHITE;
                USBSerial.println("Using default WHITE color");
            }
            
            // Type specific properties
            String type = elementObj["type"].as<String>();
            if (type == "text") {
                displayElement.type = ELEMENT_TEXT;
                displayElement.text = elementObj["text"].as<String>();
                displayElement.size = elementObj["size"] | 1;
            } else if (type == "line") {
                displayElement.type = ELEMENT_LINE;
                displayElement.end_x = elementObj["end_x"] | displayElement.x;
                displayElement.end_y = elementObj["end_y"] | displayElement.y;
            } else if (type == "rect") {
                displayElement.type = ELEMENT_RECT;
                displayElement.width = elementObj["width"] | 10;
                displayElement.height = elementObj["height"] | 10;
                displayElement.filled = elementObj["filled"] | false;
            } else if (type == "circle") {
                displayElement.type = ELEMENT_CIRCLE;
                displayElement.width = elementObj["diameter"] | 10;
                displayElement.filled = elementObj["filled"] | false;
            }
            
            USBSerial.printf("Adding element type %d at position %d,%d with color %d\n", 
                           displayElement.type, displayElement.x, displayElement.y, displayElement.color);
            
            // Add element to mode
            displayMode.elements.push_back(displayElement);
            USBSerial.printf("Mode now has %d elements\n", displayMode.elements.size());
        }
        
        // Add mode to map
        displayModes[mode.key().c_str()] = displayMode;
    }
    
    // Activate the default mode
    if (!activeMode.isEmpty() && displayModes.count(activeMode) > 0) {
        activateDisplayMode(activeMode);
    }
}

void activateDisplayMode(const String& modeName) {
    // Load modes from config
    std::map<String, DisplayMode> modes = ConfigManager::loadDisplayModes(DISPLAY_CONFIG_PATH);
    
    USBSerial.printf("Activating display mode: %s\n", modeName.c_str());
    
    // Check if mode exists
    if (modes.find(modeName) == modes.end()) {
        USBSerial.println("Mode not found, falling back to default");
        
        // Default to first mode if available
        if (!modes.empty()) {
            activeMode = modes.begin()->first;
        } else {
            activeMode = "test"; // Fallback to hardcoded test mode
        }
    } else {
        // Switch to requested mode
        activeMode = modeName;
        
        // Reset the background flag when changing modes to ensure
        // the correct background is loaded for the new mode
        backgroundLoaded = false;
        if (backgroundBuffer != nullptr) {
            free(backgroundBuffer);
            backgroundBuffer = nullptr;
        }
    }
    
    // Update config file with active mode
    DynamicJsonDocument doc(4096);
    
    // Load current config
    String configJson = readJsonFile(DISPLAY_CONFIG_PATH);
    if (configJson.length() > 0) {
        deserializeJson(doc, configJson);
    }
    
    // Update active mode
    doc["active_mode"] = activeMode;
    
    // Save updated config
    File configFile = LittleFS.open(DISPLAY_CONFIG_PATH, "w");
    if (configFile) {
        serializeJson(doc, configFile);
        configFile.close();
        USBSerial.println("Updated active mode in config file");
    } else {
        USBSerial.println("Failed to open config file for writing");
    }
    
    // Reset last update time
    lastDisplayUpdate = 0;
    
    // Force immediate display update
    updateDisplay();
}

String getCurrentMode() {
    return activeMode;
}

bool isTemporaryMessageActive() {
    return temporaryMessageActive;
}

void handleEncoder(int encoderPosition) {
    // TODO: Implement encoder handling for display mode switching
    // This will be implemented when we add support for multiple display modes
}

void showTemporaryMessage(const char* message, uint32_t duration) {
    if (!display || !screenInitialized) {
        USBSerial.println("Cannot show temporary message - display not initialized");
        return;
    }
    
    // Save current state
    temporaryMessageActive = true;
    
    // Calculate message bounds for centering
    int16_t x1, y1;
    uint16_t msgWidth, msgHeight;
    int fontSize = 2;  // Larger font size for better visibility
    
    // Prepare for text measurement
    display->setTextSize(fontSize);
    
    // Get actual text bounds
    display->getTextBounds(message, 0, 0, &x1, &y1, &msgWidth, &msgHeight);
    
    // Calculate centered position
    int msgX = (240 - msgWidth) / 2;
    int msgY = 110;  // Vertically centered position
    
    // Clear previous message area - use whole screen width to be safe
    display->fillRect(0, msgY - msgHeight - 10, 240, msgHeight * 2 + 20, ST77XX_BLACK);
    
    // Draw a background box for better visibility - make it wider than the text
    int boxPadding = 10;
    display->fillRect(msgX - boxPadding, msgY - boxPadding, 
                     msgWidth + (boxPadding * 2), msgHeight + (boxPadding * 2), 
                     ST77XX_BLUE);
    
    // Now draw the message with white text
    display->setTextColor(ST77XX_WHITE);
    display->setCursor(msgX, msgY);
    display->println(message);
    
    // Update the timeout
    temporaryMessageTimeout = millis() + duration;
}

void checkTemporaryMessage() {
    if (temporaryMessageActive && millis() > temporaryMessageTimeout) {
        // Clear the temporary message flag
        temporaryMessageActive = false;
        
        // We don't want to reset screenInitialized to false here
        // as it's causing the "ERROR: Screen not initialized" errors
        
        // Don't reset lastDisplayUpdate - allow the next updateDisplay call to evaluate
        // the refresh rate interval on its own to avoid quick successive redraws
    }
}

void displayWiFiInfo(bool isAPMode, const String& ipAddress, const String& ssid) {
    // Just a stub - we don't want this function to do anything
    // as it was causing the screen refresh issues
    return;
}

void loadBackgroundImage() {
    USBSerial.println("Starting background image loading...");
    
    if (backgroundLoaded) {
        USBSerial.println("Background already loaded, skipping");
        return;
    }
    
    // Allocate buffer for the background (240x280 pixels)
    USBSerial.println("Allocating background buffer...");
    backgroundBuffer = (uint16_t*)malloc(240 * 280 * sizeof(uint16_t));
    if (!backgroundBuffer) {
        USBSerial.println("ERROR: Failed to allocate background buffer");
        return;
    }
    USBSerial.printf("Background buffer allocated at address: %p\n", (void*)backgroundBuffer);
    
    // Get current display mode from our local cache (not ConfigManager)
    String currentModeName = getCurrentMode();
    bool customImageLoaded = false;
    
    // Check if current mode has a background image
    if (displayModes.count(currentModeName) > 0) {
        String backgroundImage = displayModes[currentModeName].backgroundImage;
        
        USBSerial.printf("Current mode: %s, Background image path: %s\n", 
                       currentModeName.c_str(), backgroundImage.c_str());
        
        if (backgroundImage.length() > 0) {
            USBSerial.printf("Attempting to load background image: %s\n", backgroundImage.c_str());
            customImageLoaded = loadBackgroundImageFromFile(backgroundImage);
            
            if (customImageLoaded) {
                USBSerial.println("Successfully loaded custom background image");
            } else {
                USBSerial.println("Failed to load custom background image, falling back to gradient");
            }
        } else {
            USBSerial.println("No background image specified for current mode");
        }
    } else {
        USBSerial.printf("Current mode '%s' not found in loaded modes\n", currentModeName.c_str());
    }
    
    // If no custom image was loaded, create a gradient background
    if (!customImageLoaded) {
        USBSerial.println("Creating gradient background as fallback...");
        createGradientBackground();
    }
    
    backgroundLoaded = true;
}

// Function to load a background image from a file
bool loadBackgroundImageFromFile(const String& imagePath) {
    if (!backgroundBuffer) {
        USBSerial.println("ERROR: Background buffer not allocated before loading image.");
        return false;
    }

    USBSerial.printf("Attempting to load image from path: %s\n", imagePath.c_str());

    // Check if file exists with detailed path info
    if (!LittleFS.exists(imagePath)) {
        // Log error with more diagnostic info
        USBSerial.printf("ERROR: Background image file not found: %s\n", imagePath.c_str());
        
        // List files in the /images directory to help debugging
        if (imagePath.startsWith("/images/")) {
            File dir = LittleFS.open("/images", "r");
            if (dir && dir.isDirectory()) {
                USBSerial.println("Available files in /images directory:");
                File file = dir.openNextFile();
                while (file) {
                    USBSerial.printf(" - %s (%d bytes)\n", file.name(), file.size());
                    file = dir.openNextFile();
                }
            } else {
                USBSerial.println("WARNING: /images directory does not exist or cannot be opened");
            }
        }
        return false;
    }

    // Get file info for debugging
    File fileInfo = LittleFS.open(imagePath, "r");
    if (fileInfo) {
        USBSerial.printf("File exists: %s (size: %d bytes)\n", imagePath.c_str(), fileInfo.size());
        fileInfo.close();
    }

    // Check if file is a JPEG (by extension)
    bool isJpeg = imagePath.endsWith(".jpg") || imagePath.endsWith(".jpeg");

    if (isJpeg) {
        USBSerial.println("JPEG format detected. Initializing decoder.");
        
        // Open the JPEG file for decoding
        File jpegFile = LittleFS.open(imagePath, "r");
        if (!jpegFile) {
            USBSerial.printf("ERROR: Failed to open JPEG file: %s\n", imagePath.c_str());
            return false;
        }
        
        // Get file size for debugging
        size_t fileSize = jpegFile.size();
        USBSerial.printf("JPEG file size: %u bytes\n", fileSize);
        
        // Check if file size is reasonable
        if (fileSize < 100) {
            USBSerial.println("ERROR: JPEG file is too small, likely invalid");
            jpegFile.close();
            return false;
        }
        
        // Read the entire file into a buffer for decoding
        uint8_t* jpegBuffer = (uint8_t*)malloc(fileSize);
        if (!jpegBuffer) {
            USBSerial.println("ERROR: Failed to allocate JPEG buffer");
            jpegFile.close();
            return false;
        }
        
        // Read the file into the buffer
        size_t bytesRead = jpegFile.read(jpegBuffer, fileSize);
        jpegFile.close();
        
        if (bytesRead != fileSize) {
            USBSerial.printf("ERROR: Failed to read JPEG file. Read %u of %u bytes\n", bytesRead, fileSize);
            free(jpegBuffer);
            return false;
        }
        
        // Check first bytes to confirm it's a JPEG
        if (jpegBuffer[0] != 0xFF || jpegBuffer[1] != 0xD8) {
            USBSerial.println("ERROR: File doesn't have JPEG header signature (0xFF 0xD8)");
            USBSerial.printf("First bytes: 0x%02X 0x%02X 0x%02X 0x%02X\n", 
                           jpegBuffer[0], jpegBuffer[1], jpegBuffer[2], jpegBuffer[3]);
            free(jpegBuffer);
            return false;
        }
        
        // Initialize the JPEG decoder with the file data
        if (!jpeg.openRAM(jpegBuffer, fileSize, jpegDrawCallback)) {
            USBSerial.println("ERROR: Failed to initialize JPEG decoder");
            free(jpegBuffer);
            return false;
        }
        
        // Get the image information for debugging
        int width = jpeg.getWidth();
        int height = jpeg.getHeight();
        int bpp = jpeg.getBpp();
        USBSerial.printf("JPEG info: %dx%d, %d bpp\n", width, height, bpp);
        
        // Check if the image dimensions are valid
        if (width <= 0 || height <= 0) {
            USBSerial.println("ERROR: Invalid JPEG dimensions");
            free(jpegBuffer);
            return false;
        }
        
        // Note: we now support both 240x280 and 280x240 images in the jpegDrawCallback function
        // Just log a warning if the dimensions don't match exactly
        if ((width != 240 || height != 280) && (width != 280 || height != 240)) {
            USBSerial.printf("WARNING: JPEG dimensions (%dx%d) don't match display (240x280)\n", width, height);
            USBSerial.println("The image may be stretched or cropped when displayed");
        } else {
            USBSerial.printf("Image dimensions (%dx%d) detected - will transpose if needed\n", width, height);
        }
        
        // Clear the background buffer to ensure no artifacts from previous images
        memset(backgroundBuffer, 0, 240 * 280 * sizeof(uint16_t));
        
        // Decode the JPEG into our buffer (the callback will handle filling the buffer)
        USBSerial.println("Starting JPEG decoding into background buffer...");
        int decoded = jpeg.decode(0, 0, 0); // Scale to fit in the display
        
        // Clean up the JPEG buffer
        free(jpegBuffer);
        
        if (decoded <= 0) {
            USBSerial.printf("ERROR: JPEG decoding failed! Error code: %d\n", decoded);
            // Don't fallback to gradient here - return false to let the caller handle it
            return false;
        }
        
        USBSerial.println("JPEG decoding successful!");
        
        // Verify buffer contents (first and last pixels)
        USBSerial.printf("First pixel color: 0x%04X\n", backgroundBuffer[0]);
        USBSerial.printf("Last pixel color: 0x%04X\n", backgroundBuffer[240 * 280 - 1]);
        
        // Check if the entire buffer seems to be empty (all black)
        bool emptyBuffer = true;
        for (int i = 0; i < min(1000, 240 * 280); i++) {
            if (backgroundBuffer[i] != 0) {
                emptyBuffer = false;
                break;
            }
        }
        
        if (emptyBuffer) {
            USBSerial.println("WARNING: Background buffer appears to be empty after decoding");
            USBSerial.println("This might indicate an issue with the JPEG decoding process");
        }
        
        return true;
    } else { 
        // Handle binary RGB565 files (or other formats if needed)
        USBSerial.println("Non-JPEG format detected. Assuming RGB565 binary file.");
        File imageFile = LittleFS.open(imagePath, "r");
        if (!imageFile) {
            USBSerial.printf("ERROR: Failed to open binary image file: %s\n", imagePath.c_str());
            return false;
        }
        
        // Check file size - each pixel is 2 bytes for RGB565 format
        size_t fileSize = imageFile.size();
        size_t expectedSize = 240 * 280 * 2; // width * height * 2 bytes per pixel
        USBSerial.printf("Binary file size: %u bytes. Expected size: %u bytes.\n", fileSize, expectedSize);

        if (fileSize != expectedSize) {
            USBSerial.printf("ERROR: Binary image file size mismatch. Expected %u bytes, got %u bytes\n", 
                            expectedSize, fileSize);
            imageFile.close();
            return false;
        }

        USBSerial.println("Reading binary RGB565 data into background buffer...");
        // Read the file in chunks
        const size_t CHUNK_SIZE = 1024;
        uint8_t buffer[CHUNK_SIZE];
        size_t totalRead = 0;
        size_t pixelIndex = 0;

        while (totalRead < fileSize) {
            size_t bytesToRead = min((size_t)CHUNK_SIZE, fileSize - totalRead);
            size_t bytesRead = imageFile.read(buffer, bytesToRead);

            if (bytesRead != bytesToRead) {
                USBSerial.printf("ERROR: Failed to read binary image data. Expected %u bytes, got %u bytes\n", 
                               bytesToRead, bytesRead);
                imageFile.close();
                return false;
            }

            // Process the data - each pixel is 2 bytes in RGB565 format (assuming big-endian in file)
            for (size_t i = 0; i < bytesRead; i += 2) {
                if (pixelIndex < 240 * 280) {
                    // Combine two bytes into a 16-bit pixel value
                    // Order depends on how the .bin file was created (little vs big endian)
                    // Assuming Big Endian (common for image tools): buffer[i] is high byte, buffer[i+1] is low byte
                    backgroundBuffer[pixelIndex++] = (buffer[i] << 8) | buffer[i+1];
                    // If Little Endian: backgroundBuffer[pixelIndex++] = (buffer[i+1] << 8) | buffer[i];
                }
            }
            totalRead += bytesRead;
        }

        imageFile.close();

        // Verify buffer contents
        USBSerial.printf("First pixel color after binary load: 0x%04X\n", backgroundBuffer[0]);
        USBSerial.printf("Last pixel color after binary load: 0x%04X\n", backgroundBuffer[240 * 280 - 1]);

        USBSerial.println("Binary background image loaded successfully");
        return true;
    }
}

// Helper function to create a gradient background
void createGradientBackground() {
    if (!backgroundBuffer) {
        USBSerial.println("ERROR: Background buffer not allocated");
        return;
    }
    
    USBSerial.println("Creating gradient background...");
    for (int y = 0; y < 280; y++) {
        for (int x = 0; x < 240; x++) {
            // Create a more visible gradient for testing
            // Red increases from left to right
            uint8_t red = map(x, 0, 240, 0, 31);
            // Green increases from top to bottom
            uint8_t green = map(y, 0, 280, 0, 31);
            // Blue stays constant for a base color
            uint8_t blue = 16;
            
            // Convert to RGB565 format
            uint16_t color = ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | (blue >> 3);
            backgroundBuffer[y * 240 + x] = color;
        }
    }
    
    // Verify buffer contents
    USBSerial.printf("First pixel color: 0x%04X\n", backgroundBuffer[0]);
    USBSerial.printf("Last pixel color: 0x%04X\n", backgroundBuffer[240 * 280 - 1]);
    
    USBSerial.println("Background gradient created successfully");
}

void displayBackgroundImage() {
    USBSerial.println("Starting background display...");
    
    if (!backgroundLoaded) {
        USBSerial.println("ERROR: Background not loaded");
        return;
    }
    
    if (!backgroundBuffer) {
        USBSerial.println("ERROR: Background buffer is null");
        return;
    }
    
    if (!display) {
        USBSerial.println("ERROR: Display not initialized");
        return;
    }
    
    USBSerial.println("Copying background to display...");
    
    // Always use rotation 1 (landscape orientation) for consistent display
    display->setRotation(1);
    
    // Clear the screen first
    display->fillScreen(ST77XX_BLACK);
    
    // Set up the display window for full screen
    display->startWrite();
    display->setAddrWindow(0, 0, display->width(), display->height());
    
    // Get display dimensions in current orientation
    int dispWidth = display->width();
    int dispHeight = display->height();
    USBSerial.printf("Display dimensions in current rotation: %dx%d\n", dispWidth, dispHeight);
    
    // Write the pixels directly to the display - this improves performance
    // and eliminates potential issues with buffer alignment
    display->writePixels(backgroundBuffer, dispWidth * dispHeight);
    
    display->endWrite();
    USBSerial.println("Background displayed successfully");
}

void displayMainLayout() {
    USBSerial.println("Starting main layout display...");
    
    if (!display) {
        USBSerial.println("ERROR: Display not initialized");
        return;
    }
    
    // First display the background
    if (!backgroundLoaded) {
        USBSerial.println("Background not loaded, loading now...");
        loadBackgroundImage();
    }
    
    USBSerial.println("Displaying background...");
    displayBackgroundImage();
    
    // Keep rotation 1 for text overlay (landscape)
    display->setRotation(1);
    
    // Define colors
    uint16_t textColor = ST77XX_WHITE;
    uint16_t accentColor = ST77XX_GREEN;
    uint16_t shadowColor = ST77XX_BLACK;
    
    // Set text properties
    display->setTextColor(textColor);
    display->setTextSize(1);
    
    // Helper function for drawing text with shadow
    auto drawTextWithShadow = [&](const char* text, int16_t x, int16_t y) {
        display->setTextColor(shadowColor);
        display->setCursor(x + 1, y + 1);
        display->print(text);
        display->setTextColor(textColor);
        display->setCursor(x, y);
        display->print(text);
    };
    
    // Draw title - adjusted for landscape orientation
    drawTextWithShadow("Modular Macropad", 10, 10);
    
    // Draw underline - adjusted for landscape orientation
    display->drawFastHLine(10, 25, 220, accentColor);
    
    // Draw info section - adjusted for landscape orientation
    int startY = 40;
    int lineHeight = 20;
    
    // Get current values
    String wifiStatus = WiFiManager::isConnected() ? WiFiManager::getSSID() : "Disconnected";
    String ipAddress = WiFiManager::getLocalIP().toString();
    String macroStatus = (macroHandler && macroHandler->isExecuting()) ? "Running" : "Ready";
    String layerName = keyHandler ? keyHandler->getCurrentLayer() : "default";
    
    // Draw info lines - adjusted for landscape orientation
    drawTextWithShadow(("WiFi: " + wifiStatus).c_str(), 10, startY);
    drawTextWithShadow(("IP: " + ipAddress).c_str(), 10, startY + lineHeight);
    drawTextWithShadow(("Layer: " + layerName).c_str(), 10, startY + lineHeight * 2);
    drawTextWithShadow(("Macro: " + macroStatus).c_str(), 10, startY + lineHeight * 3);
}
