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

void EncoderHandler::updateEncoders() {
    if (!encoderConfigs) return;

    // Update encoders
    for (uint8_t i = 0; i < numEncoders; i++) {
        // Store previous position to detect change
        long prevPosition = encoderConfigs[i].absolutePosition;
        
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
        
        // Check if encoder position changed
        long newPosition = encoderConfigs[i].absolutePosition;
        if (newPosition != prevPosition) {
            // Determine direction of rotation
            bool clockwise = (newPosition > prevPosition);
            
            // Log the event
            USBSerial.printf("Encoder %d rotated %s (position: %ld)\n", 
                           i + 1, clockwise ? "clockwise" : "counterclockwise", newPosition);
            
            // Send appropriate HID report based on direction
            executeEncoderAction(i, clockwise);
            
            // Update last reported position
            encoderConfigs[i].lastReportedPosition = newPosition;
        }
    }
}

void EncoderHandler::executeEncoderAction(uint8_t encoderIndex, bool clockwise) {
    if (!hidHandler) {
        USBSerial.println("ERROR: hidHandler is NULL in executeEncoderAction!");
        return;
    }
    
    // Form the encoder ID string
    String encoderId = "encoder-" + String(encoderIndex + 1);
    
    // Look up the encoder action
    auto it = encoderActions.find(encoderId);
    if (it == encoderActions.end()) {
        // Fallback to default volume control if no action is defined
        USBSerial.printf("No action defined for %s, using default volume control\n", encoderId.c_str());
        
        // Default volume control
        const uint8_t volumeUp[HID_CONSUMER_REPORT_SIZE] = {0x00, 0xEA, 0x00, 0x00};
        const uint8_t volumeDown[HID_CONSUMER_REPORT_SIZE] = {0x00, 0xE9, 0x00, 0x00};
        
        if (clockwise) {
            USBSerial.println("Sending Volume UP command (default)");
            hidHandler->sendConsumerReport(volumeUp);
        } else {
            USBSerial.println("Sending Volume DOWN command (default)");
            hidHandler->sendConsumerReport(volumeDown);
        }
        
        // Release the control after a short delay
        delay(20);
        hidHandler->sendEmptyConsumerReport();
        return;
    }
    
    // Get the encoder action
    const EncoderAction& action = it->second;
    
    if (action.type == "hid") {
        // Check if this is actually a consumer report (volume/media controls)
        const std::vector<uint8_t>& report = clockwise ? action.cwHidReport : action.ccwHidReport;
        
        // Special case for consumer control codes sent as HID reports
        if (report.size() == 4 && (report[1] == 0xE9 || report[1] == 0xEA || report[1] == 0xE2)) {
            USBSerial.print("Sending Consumer report (from HID type): ");
            for (size_t i = 0; i < report.size(); i++) {
                USBSerial.printf("%02X ", report[i]);
            }
            USBSerial.println();
            
            hidHandler->sendConsumerReport(report.data());
            delay(20);
            hidHandler->sendEmptyConsumerReport();
        }
        else if (report.size() == HID_KEYBOARD_REPORT_SIZE) {
            USBSerial.print("Sending HID Keyboard report: ");
            for (size_t i = 0; i < report.size(); i++) {
                USBSerial.printf("%02X ", report[i]);
            }
            USBSerial.println();
            
            hidHandler->sendKeyboardReport(report.data());
            delay(20);
            hidHandler->sendEmptyKeyboardReport();
        } else {
            USBSerial.println("Error: Invalid HID report size");
        }
    }
    else if (action.type == "multimedia") {
        // Multimedia consumer action
        const std::vector<uint8_t>& report = clockwise ? action.cwConsumerReport : action.ccwConsumerReport;
        
        if (report.size() == HID_CONSUMER_REPORT_SIZE) {
            USBSerial.print("Sending Consumer report: ");
            for (size_t i = 0; i < report.size(); i++) {
                USBSerial.printf("%02X ", report[i]);
            }
            USBSerial.println();
            
            hidHandler->sendConsumerReport(report.data());
            delay(20);
            hidHandler->sendEmptyConsumerReport();
        } else {
            USBSerial.println("Error: Invalid Consumer report size");
        }
    }
}
void EncoderHandler::handleMechanicalEncoder(uint8_t encoderIndex) {
    if (!mechanicalEncoders[encoderIndex]) return;

    EncoderConfig& config = encoderConfigs[encoderIndex];
    
    // Read current position
    long currentPosition = mechanicalEncoders[encoderIndex]->read();
    
    // Update absolute position with direction
    config.absolutePosition = currentPosition * config.direction;
}

void EncoderHandler::handleAS5600Encoder(uint8_t encoderIndex) {
    EncoderConfig& config = encoderConfigs[encoderIndex];
    
    // Check if sensor is responding
    if (!as5600Encoders[encoderIndex].isConnected()) {
        USBSerial.printf("Warning: AS5600 encoder %d not connected\n", encoderIndex);
        return;
    }
    
    // Get current raw position (0-4095)
    uint16_t currentRawPosition = as5600Encoders[encoderIndex].rawAngle();
    
    // On first read, just store the position
    if (config.lastRawPosition == 0 && config.absolutePosition == 0) {
        config.lastRawPosition = currentRawPosition;
        return;
    }
    
    // Calculate movement considering wrap-around
    const uint16_t MAX_POSITION = 4096; // 12-bit encoder
    int16_t rawDiff;
    
    // Handle wrap-around in both directions
    if (currentRawPosition >= config.lastRawPosition) {
        // Normal case - no wrap-around
        rawDiff = currentRawPosition - config.lastRawPosition;
        
        // Check if this might be a wrap-around from high to low
        if (rawDiff > MAX_POSITION / 2) {
            rawDiff = -(MAX_POSITION - rawDiff);
        }
    } else {
        // currentRawPosition < lastRawPosition
        rawDiff = -(config.lastRawPosition - currentRawPosition);
        
        // Check if this might be a wrap-around from low to high
        if (-rawDiff > MAX_POSITION / 2) {
            rawDiff = MAX_POSITION + rawDiff;
        }
    }
    
    // Apply direction and update position
    config.absolutePosition += rawDiff * config.direction;
    config.lastRawPosition = currentRawPosition;
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