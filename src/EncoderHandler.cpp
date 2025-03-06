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
    if (numEncoders > MAX_ENCODERS) {
        USBSerial.println("Invalid encoder initialization parameters");
        return;
    }

    try {
        // Allocate configuration array
        encoderConfigs = new EncoderConfig[numEncoders];

        // Allocate encoders based on type
        mechanicalEncoders = new Encoder*[numEncoders]();
        as5600Encoders = new AS5600[numEncoders]();

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
    if (encoderIndex >= numEncoders) return;

    EncoderConfig& config = encoderConfigs[encoderIndex];
    config.type = type;
    config.pinA = pinA;
    config.pinB = pinB;
    config.direction = direction;
    config.zeroPosition = zeroPosition;
}

void EncoderHandler::loadEncoderActions(const std::map<String, ActionConfig>& actions) {
    // Clear existing encoder actions
    encoderActions.clear();
    
    // Process each action configuration
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
                
                // Load clockwise action
                if (!config.clockwise.empty()) {
                    uint8_t report[HID_KEYBOARD_REPORT_SIZE] = {0};
                    if (HIDHandler::hexReportToBinary(config.clockwise, report, HID_KEYBOARD_REPORT_SIZE)) {
                        for (int i = 0; i < HID_KEYBOARD_REPORT_SIZE; i++) {
                            action.cwHidReport[i] = report[i];
                        }
                    }
                }
                
                // Load counterclockwise action
                if (!config.counterclockwise.empty()) {
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
                if (!config.clockwise.empty()) {
                    uint8_t report[HID_CONSUMER_REPORT_SIZE] = {0};
                    if (HIDHandler::hexReportToBinary(config.clockwise, report, HID_CONSUMER_REPORT_SIZE)) {
                        for (int i = 0; i < HID_CONSUMER_REPORT_SIZE; i++) {
                            action.cwConsumerReport[i] = report[i];
                        }
                    }
                }
                
                // Load counterclockwise action
                if (!config.counterclockwise.empty()) {
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


void EncoderHandler::handleAS5600Encoder(uint8_t encoderIndex) {
    EncoderConfig& config = encoderConfigs[encoderIndex];
    
    // Enhanced robustness for magnetic encoder
    static const uint16_t MAX_POSITION = 4096; // 12-bit encoder
    static const int MAX_STEPS_PER_CYCLE = 50;  // Limit sudden movements
    
    // Check sensor connectivity
    if (!as5600Encoders[encoderIndex].isConnected()) {
        USBSerial.printf("Warning: AS5600 encoder %d disconnected\n", encoderIndex);
        return;
    }
    
    // Get current raw position
    uint16_t currentRawPosition = as5600Encoders[encoderIndex].rawAngle();
    
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
    
    // Update position with direction
    config.absolutePosition += rawDiff * config.direction;
    config.lastRawPosition = currentRawPosition;
    
    USBSerial.printf("AS5600 Encoder %d: Raw Diff = %d, Total Position = %ld\n", 
                   encoderIndex, rawDiff, config.absolutePosition);
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

void EncoderHandler::executeEncoderAction(uint8_t encoderIndex, bool clockwise) {
    if (!hidHandler) {
        USBSerial.println("ERROR: HID Handler not available");
        return;
    }

    delay(500);
    
    // Predefined volume control reports
    const uint8_t volumeUp[4] = {0x00, 0x00, 0xE9, 0x00};
    const uint8_t volumeDown[4] = {0x00, 0x00, 0xEA, 0x00};
    const uint8_t emptyReport[4] = {0x00, 0x00, 0x00, 0x00};
    
    // Select report based on rotation direction
    const uint8_t* report = clockwise ? volumeUp : volumeDown;
    const char* actionName = clockwise ? "Volume UP" : "Volume DOWN";
    
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
    
    // Small delay to simulate key press
    delay(600);
    
    // Release key
    for (int attempts = 0; attempts < 3; attempts++) {
        if (hidHandler->sendConsumerReport(emptyReport, 4)) {
            break;
        }
        delay(10);
    }
}

// Add extra robustness to encoder reading
void EncoderHandler::handleMechanicalEncoder(uint8_t encoderIndex) {
    if (!mechanicalEncoders[encoderIndex]) return;

    EncoderConfig& config = encoderConfigs[encoderIndex];
    
    // Read current position with basic noise filtering
    long currentPosition = mechanicalEncoders[encoderIndex]->read();
    
    // Calculate absolute position with direction
    long newAbsolutePosition = currentPosition * config.direction;
    
    // Only update if significant change
    long positionChange = newAbsolutePosition - config.absolutePosition;
    if (abs(positionChange) >= 1) {
        config.absolutePosition = newAbsolutePosition;
        
        USBSerial.printf("Mechanical Encoder %d: Position Change = %ld, Total Position = %ld\n", 
                       encoderIndex, positionChange, config.absolutePosition);
    }
}

void EncoderHandler::updateEncoders() {
    if (!encoderConfigs) return;

    // Static variables to track previous positions
    static long prevPositions[MAX_ENCODERS] = {0};

    for (uint8_t i = 0; i < numEncoders; i++) {
        // Update encoder position based on type
        switch (encoderConfigs[i].type) {
            case ENCODER_TYPE_MECHANICAL:
                handleMechanicalEncoder(i);
                break;
            case ENCODER_TYPE_AS5600:
                handleAS5600Encoder(i);
                break;
            default:
                break;
        }
        
        // Get current position
        long currentPosition = encoderConfigs[i].absolutePosition;
        
        // Detect meaningful position change
        if (currentPosition != prevPositions[i]) {
            // Determine rotation direction
            bool clockwise = (currentPosition > prevPositions[i]);
            
            USBSerial.printf("Encoder %d rotated %s (position: %ld)\n", 
                           i, clockwise ? "clockwise" : "counterclockwise", currentPosition);
            
            // Send HID report for rotation
            executeEncoderAction(i, clockwise);
            
            // Update previous position
            prevPositions[i] = currentPosition;
        }
    }
}