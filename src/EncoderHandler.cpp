#include "EncoderHandler.h"
#include "HIDHandler.h"  // Include for hidHandler
#include "ConfigManager.h"  // For loading encoder actions

extern USBCDC USBSerial;
extern HIDHandler* hidHandler;  // Access to the global HID handler

// Global encoder handler instance
EncoderHandler* encoderHandler = nullptr;

// Map to store encoder actions loaded from the config
std::map<String, EncoderAction> encoderActions;

EncoderHandler::EncoderHandler(uint8_t numEncoders) 
    : numEncoders(numEncoders), 
      mechanicalEncoders(nullptr), 
      as5600Encoders(nullptr),
      encoderConfigs(nullptr)
{
    // Validate input parameters
    if (numEncoders == 0 || numEncoders > MAX_ENCODERS) {
        numEncoders = 1; // Default to at least one encoder
    }
    
    try {
        // Allocate configuration array
        encoderConfigs = new EncoderConfig[numEncoders]();

        // Allocate encoders based on type
        mechanicalEncoders = new Encoder*[numEncoders]();
        as5600Encoders = new AS5600[numEncoders]();

        // Initialize encoder pointers to nullptr
        for (uint8_t i = 0; i < numEncoders; i++) {
            mechanicalEncoders[i] = nullptr;
        }
        
        USBSerial.printf("Encoder Handler initialized with %d encoders\n", numEncoders);
    }
    catch (const std::exception& e) {
        USBSerial.printf("Exception in encoder constructor: %s\n", e.what());
        cleanup();
    }
    catch (...) {
        USBSerial.println("Unknown exception in encoder constructor");
        cleanup();
    }
}

EncoderHandler::~EncoderHandler() {
    cleanup();
}

void EncoderHandler::cleanup() {
    // Free dynamically allocated memory
    if (mechanicalEncoders) {
        for (uint8_t i = 0; i < numEncoders; i++) {
            if (mechanicalEncoders[i]) {
                delete mechanicalEncoders[i];
                mechanicalEncoders[i] = nullptr;
            }
        }
        delete[] mechanicalEncoders;
        mechanicalEncoders = nullptr;
    }

    // Free AS5600 encoders
    if (as5600Encoders) {
        delete[] as5600Encoders;
        as5600Encoders = nullptr;
    }

    // Free configuration array
    if (encoderConfigs) {
        delete[] encoderConfigs;
        encoderConfigs = nullptr;
    }
}

void EncoderHandler::configureEncoder(
    uint8_t encoderIndex, 
    EncoderType type, 
    uint8_t pinA, 
    uint8_t pinB, 
    int8_t direction, 
    uint16_t zeroPosition
) {
    if (encoderIndex >= numEncoders) {
        USBSerial.printf("Error: Invalid encoder index %d\n", encoderIndex);
        return;
    }
    
    EncoderConfig& config = encoderConfigs[encoderIndex];
    
    config.type = type;
    config.pinA = pinA;
    config.pinB = pinB;
    config.direction = direction;
    config.zeroPosition = zeroPosition;
    
    // Reset position tracking
    config.absolutePosition = 0;
    config.lastReportedPosition = 0;
    config.lastRawPosition = 0;
    
    USBSerial.printf("Configured encoder %d: Type=%d, PinA=%d, PinB=%d, Dir=%d\n",
                 encoderIndex, type, pinA, pinB, direction);
}

void EncoderHandler::loadEncoderActions(const std::map<String, ActionConfig>& actions) {
    USBSerial.println("Loading encoder actions from configuration");
    
    // Clear previous actions
    encoderActions.clear();
    
    // Process each action
    for (const auto& pair : actions) {
        const String& id = pair.first;
        const ActionConfig& config = pair.second;
        
        // Process only encoder components
        if (id.startsWith("encoder-")) {
            EncoderAction action;
            action.type = config.type;
            
            if (config.type == "hid") {
                // For HID type
                action.cwHidReport.resize(HID_KEYBOARD_REPORT_SIZE, 0);
                action.ccwHidReport.resize(HID_KEYBOARD_REPORT_SIZE, 0);
                
                // Load clockwise action - check if exists in struct
                if (config.hidReport.size() > 0) {
                    // Backward compatibility with older config format that uses hidReport
                    uint8_t report[HID_KEYBOARD_REPORT_SIZE] = {0};
                    if (HIDHandler::hexReportToBinary(config.hidReport, report, HID_KEYBOARD_REPORT_SIZE)) {
                        for (int i = 0; i < HID_KEYBOARD_REPORT_SIZE; i++) {
                            action.cwHidReport[i] = report[i];
                        }
                    }
                }
                else if (config.clockwise.size() > 0) {
                    // Use new clockwise field if available
                    uint8_t report[HID_KEYBOARD_REPORT_SIZE] = {0};
                    if (HIDHandler::hexReportToBinary(config.clockwise, report, HID_KEYBOARD_REPORT_SIZE)) {
                        for (int i = 0; i < HID_KEYBOARD_REPORT_SIZE; i++) {
                            action.cwHidReport[i] = report[i];
                        }
                    }
                }
                
                // Load counterclockwise action
                if (config.counterclockwise.size() > 0) {
                    uint8_t report[HID_KEYBOARD_REPORT_SIZE] = {0};
                    if (HIDHandler::hexReportToBinary(config.counterclockwise, report, HID_KEYBOARD_REPORT_SIZE)) {
                        for (int i = 0; i < HID_KEYBOARD_REPORT_SIZE; i++) {
                            action.ccwHidReport[i] = report[i];
                        }
                    }
                }
            }
            else if (config.type == "multimedia") {
                // For multimedia type
                action.cwConsumerReport.resize(HID_CONSUMER_REPORT_SIZE, 0);
                action.ccwConsumerReport.resize(HID_CONSUMER_REPORT_SIZE, 0);
                
                // Load clockwise action
                if (config.consumerReport.size() > 0) {
                    // Backward compatibility with older config format
                    uint8_t report[HID_CONSUMER_REPORT_SIZE] = {0};
                    if (HIDHandler::hexReportToBinary(config.consumerReport, report, HID_CONSUMER_REPORT_SIZE)) {
                        for (int i = 0; i < HID_CONSUMER_REPORT_SIZE; i++) {
                            action.cwConsumerReport[i] = report[i];
                        }
                    }
                }
                else if (config.clockwise.size() > 0) {
                    uint8_t report[HID_CONSUMER_REPORT_SIZE] = {0};
                    if (HIDHandler::hexReportToBinary(config.clockwise, report, HID_CONSUMER_REPORT_SIZE)) {
                        for (int i = 0; i < HID_CONSUMER_REPORT_SIZE; i++) {
                            action.cwConsumerReport[i] = report[i];
                        }
                    }
                }
                
                // Load counterclockwise action
                if (config.counterclockwise.size() > 0) {
                    uint8_t report[HID_CONSUMER_REPORT_SIZE] = {0};
                    if (HIDHandler::hexReportToBinary(config.counterclockwise, report, HID_CONSUMER_REPORT_SIZE)) {
                        for (int i = 0; i < HID_CONSUMER_REPORT_SIZE; i++) {
                            action.ccwConsumerReport[i] = report[i];
                        }
                    }
                }
            }
            
            // Store the action for this encoder ID
            encoderActions[id] = action;
            
            USBSerial.printf("Loaded actions for %s, type: %s\n", id.c_str(), action.type.c_str());
        }
    }
    
    USBSerial.printf("Loaded actions for %d encoders\n", encoderActions.size());
}

void EncoderHandler::begin() {
    if (!encoderConfigs) {
        USBSerial.println("Error: Encoders not initialized in begin()");
        return;
    }

    // Initialize encoders based on their type
    for (uint8_t i = 0; i < numEncoders; i++) {
        EncoderConfig& config = encoderConfigs[i];

        // Initialize based on encoder type
        switch (config.type) {
            case ENCODER_TYPE_MECHANICAL:
                // Create mechanical encoder if pins are valid
                if (config.pinA > 0 && config.pinB > 0) {
                    mechanicalEncoders[i] = new Encoder(config.pinA, config.pinB);
                }
                break;

            case ENCODER_TYPE_AS5600:
                // Initialize I2C for AS5600
                Wire.begin(config.pinA, config.pinB); // SDA, SCL
                as5600Encoders[i].begin();
                
                // Optional: Check magnet detection
                if (!as5600Encoders[i].detectMagnet()) {
                    USBSerial.printf("No magnet detected for encoder %d\n", i);
                }
                
                // Configure initial state
                config.lastRawPosition = as5600Encoders[i].rawAngle();
                break;

            default:
                USBSerial.printf("Unsupported encoder type for encoder %d\n", i);
                break;
        }
    }

    // Load the encoder actions
    auto actions = ConfigManager::loadActions("/config/actions.json");
    loadEncoderActions(actions);

    USBSerial.println("Encoder Handler initialization complete");
}

long EncoderHandler::getEncoderPosition(uint8_t encoderIndex) const {
    if (encoderIndex >= numEncoders) return 0;
    return encoderConfigs[encoderIndex].absolutePosition;
}

long EncoderHandler::getEncoderChange(uint8_t encoderIndex) const {
    if (encoderIndex >= numEncoders) return 0;
    
    const EncoderConfig& config = encoderConfigs[encoderIndex];
    return config.absolutePosition - config.lastReportedPosition;
}

EncoderType EncoderHandler::getEncoderType(uint8_t encoderIndex) const {
    if (encoderIndex >= numEncoders) return ENCODER_TYPE_MECHANICAL;
    return encoderConfigs[encoderIndex].type;
}

void EncoderHandler::printEncoderStates() {
    USBSerial.println("\n--- Encoder States ---");
    for (uint8_t i = 0; i < numEncoders; i++) {
        const EncoderConfig& config = encoderConfigs[i];
        USBSerial.printf("Encoder %d: ", i + 1);
        
        if (config.type == ENCODER_TYPE_MECHANICAL) {
            USBSerial.print("Mechanical, ");
        } else if (config.type == ENCODER_TYPE_AS5600) {
            USBSerial.print("AS5600, ");
        }
        
        USBSerial.printf("Position: %ld\n", config.absolutePosition);
    }
    USBSerial.println("----------------------------\n");
}

void EncoderHandler::diagnostics() {
    static unsigned long lastDiagTime = 0;
    const unsigned long diagInterval = 5000; // Every 5 seconds
    
    unsigned long now = millis();
    if (now - lastDiagTime >= diagInterval) {
        lastDiagTime = now;
        printEncoderStates();
    }
}

// Modify the executeEncoderAction function in EncoderHandler.cpp
// to invert the clockwise parameter

void EncoderHandler::executeEncoderAction(uint8_t encoderIndex, bool clockwise) {
    if (!hidHandler) {
        USBSerial.println("ERROR: HID Handler not available");
        return;
    }

    // Reduce the delay from 500ms to 50ms
    delay(50);
    
    // Predefined volume control reports
    const uint8_t volumeUp[4] = {0x00, 0x00, 0xE9, 0x00};
    const uint8_t volumeDown[4] = {0x00, 0x00, 0xEA, 0x00};
    const uint8_t emptyReport[4] = {0x00, 0x00, 0x00, 0x00};
    
    // INVERT the direction by toggling the clockwise parameter
    bool actualDirection = !clockwise;  // Invert the direction
    
    // Select report based on INVERTED rotation direction
    const uint8_t* report = actualDirection ? volumeUp : volumeDown;
    const char* actionName = actualDirection ? "Volume UP" : "Volume DOWN";
    
    USBSerial.printf("Encoder %d Action: %s\n", encoderIndex, actionName);
    
    // Send consumer report with multiple error checks
    bool reportSent = false;
    for (int attempts = 0; attempts < 3; attempts++) {
        if (hidHandler->sendConsumerReport(report, 4)) {
            reportSent = true;
            break;
        }
        delay(10);  // Short delay between attempts
    }
    
    if (!reportSent) {
        USBSerial.printf("FAILED to send %s command\n", actionName);
        return;
    }
    
    // Reduce delay from 600ms to 100ms
    delay(100);
    
    // Release key
    for (int attempts = 0; attempts < 3; attempts++) {
        if (hidHandler->sendConsumerReport(emptyReport, 4)) {
            break;
        }
        delay(10);
    }
}


// Improved AS5600 encoder handling with better noise filtering
void EncoderHandler::handleAS5600Encoder(uint8_t encoderIndex) {
    EncoderConfig& config = encoderConfigs[encoderIndex];
    
    // Enhanced robustness for magnetic encoder
    static const uint16_t MAX_POSITION = 4096; // 12-bit encoder
    static const int MAX_STEPS_PER_CYCLE = 50;  // Limit sudden movements
    static const int MIN_STEPS_THRESHOLD = 3;  // Minimum steps to consider a movement
    
    // Check sensor connectivity
    if (!as5600Encoders[encoderIndex].isConnected()) {
        USBSerial.printf("Warning: AS5600 encoder %d disconnected\n", encoderIndex);
        return;
    }
    
    // Get current raw position with multiple readings for stability
    uint16_t readings[3];
    for (int i = 0; i < 3; i++) {
        readings[i] = as5600Encoders[encoderIndex].rawAngle();
        delayMicroseconds(200); // Short delay between readings
    }
    
    // Calculate median value to filter outliers
    uint16_t currentRawPosition;
    
    // Simple median calculation
    if ((readings[0] <= readings[1] && readings[1] <= readings[2]) || 
        (readings[2] <= readings[1] && readings[1] <= readings[0])) {
        currentRawPosition = readings[1]; // Middle value is median
    } else if ((readings[1] <= readings[0] && readings[0] <= readings[2]) || 
               (readings[2] <= readings[0] && readings[0] <= readings[1])) {
        currentRawPosition = readings[0]; // First value is median
    } else {
        currentRawPosition = readings[2]; // Last value is median
    }
    
    // First-time initialization
    if (config.lastRawPosition == 0) {
        config.lastRawPosition = currentRawPosition;
        return;
    }
    
    // Calculate position difference with wrap-around handling
    int16_t rawDiff;
    if (currentRawPosition >= config.lastRawPosition) {
        rawDiff = currentRawPosition - config.lastRawPosition;
        if (rawDiff > MAX_POSITION / 2) {
            rawDiff -= MAX_POSITION;
        }
    } else {
        rawDiff = -(config.lastRawPosition - currentRawPosition);
        if (-rawDiff > MAX_POSITION / 2) {
            rawDiff += MAX_POSITION;
        }
    }
    
    // Sanity check movement
    if (abs(rawDiff) > MAX_STEPS_PER_CYCLE) {
        USBSerial.printf("Warning: Excessive movement on AS5600 encoder %d\n", encoderIndex);
        return;
    }
    
    // Only update position if movement exceeds the threshold
    if (abs(rawDiff) >= MIN_STEPS_THRESHOLD) {
        // Update position with direction
        config.absolutePosition += rawDiff * config.direction;
        config.lastRawPosition = currentRawPosition;
        
        USBSerial.printf("AS5600 Encoder %d: Raw Diff = %d, Total Position = %ld\n", 
                      encoderIndex, rawDiff, config.absolutePosition);
    }
}

// Improved mechanical encoder handling
void EncoderHandler::handleMechanicalEncoder(uint8_t encoderIndex) {
    if (!mechanicalEncoders[encoderIndex]) return;

    EncoderConfig& config = encoderConfigs[encoderIndex];
    
    // Read current position
    long currentPosition = mechanicalEncoders[encoderIndex]->read();
    
    // For mechanical encoders, often they trigger multiple steps at once
    // Use a minimum change threshold of 2 steps for better reliability
    static const int MECHANICAL_CHANGE_THRESHOLD = 2;
    
    // Calculate the raw change (use absolutePosition as reference)
    // This avoids adding a new struct member
    long newAbsolutePosition = currentPosition * config.direction;
    long positionChange = newAbsolutePosition - config.absolutePosition;
    
    // Only register a movement if it exceeds our threshold
    if (abs(positionChange) >= MECHANICAL_CHANGE_THRESHOLD) {
        // Update the absolute position
        config.absolutePosition = newAbsolutePosition;
        
        USBSerial.printf("Mechanical Encoder %d: Position Change = %ld, Total Position = %ld\n", 
                      encoderIndex, positionChange, config.absolutePosition);
    }
}


void EncoderHandler::updateEncoders() {
    if (!encoderConfigs) return;

    // Static variables to track previous positions and debounce encoders
    static long prevPositions[MAX_ENCODERS] = {0};
    static unsigned long lastActionTime[MAX_ENCODERS] = {0};
    const unsigned long ENCODER_DEBOUNCE_TIME = 150; // 150ms debounce time
    unsigned long currentTime = millis();

    for (uint8_t i = 0; i < numEncoders; i++) {
        // Update encoder position based on type
        if (encoderConfigs[i].type == ENCODER_TYPE_MECHANICAL) {
            handleMechanicalEncoder(i);
        } else if (encoderConfigs[i].type == ENCODER_TYPE_AS5600) {
            handleAS5600Encoder(i);
        }
        
        // Get current position
        long currentPosition = encoderConfigs[i].absolutePosition;
        
        // Detect meaningful position change with debouncing
        if (currentPosition != prevPositions[i] && 
            (currentTime - lastActionTime[i] > ENCODER_DEBOUNCE_TIME)) {
            
            // Determine rotation direction
            bool clockwise = (currentPosition > prevPositions[i]);
            
            USBSerial.printf("Encoder %d rotated %s (position: %ld)\n", 
                          i, clockwise ? "clockwise" : "counterclockwise", currentPosition);
            
            // Send HID report for rotation
            executeEncoderAction(i, clockwise);
            
            // Update previous position and action time
            prevPositions[i] = currentPosition;
            lastActionTime[i] = currentTime;
        }
    }
}