#include "EncoderHandler.h"

extern USBCDC USBSerial;

// Global encoder handler instance
EncoderHandler* encoderHandler = nullptr;

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

void EncoderHandler::updateEncoders() {
    if (!encoderConfigs) return;

    // Update encoders
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