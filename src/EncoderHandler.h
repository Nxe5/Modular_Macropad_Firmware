#ifndef ENCODER_HANDLER_H
#define ENCODER_HANDLER_H

#include <Arduino.h>
#include <Wire.h>
#include <AS5600.h>
#include <Encoder.h>
#include <map>
#include <vector>
#include "ConfigManager.h"

// Forward declaration:
class EncoderHandler;

extern EncoderHandler* encoderHandler;

// Maximum number of encoders supported
#define MAX_ENCODERS 6

// Encoder Types
enum EncoderType {
    ENCODER_TYPE_MECHANICAL,  // Standard rotary encoder
    ENCODER_TYPE_AS5600       // Magnetic encoder
};

// Configuration for each encoder
struct EncoderConfig {
    EncoderType type = ENCODER_TYPE_MECHANICAL;
    
    // Pins for different encoder types
    uint8_t pinA = 0;        // Mechanical encoder or I2C SDA for AS5600
    uint8_t pinB = 0;        // Mechanical encoder or I2C SCL for AS5600
    
    // AS5600 specific configuration
    uint16_t zeroPosition = 0;  // Calibration zero point
    uint16_t steps = 4096;      // Total steps for AS5600 (12-bit)
    int8_t direction = 1;       // 1 or -1 to invert rotation
    
    // Detailed tracking
    long absolutePosition = 0;
    long lastReportedPosition = 0;
    uint16_t lastRawPosition = 0;
};

// Structure to store encoder actions from configuration
struct EncoderAction {
    String type;  // "hid" or "multimedia"
    
    // HID keyboard reports
    std::vector<uint8_t> cwHidReport;     // Clockwise HID report
    std::vector<uint8_t> ccwHidReport;    // Counter-clockwise HID report
    
    // Consumer reports
    std::vector<uint8_t> cwConsumerReport;   // Clockwise consumer report
    std::vector<uint8_t> ccwConsumerReport;  // Counter-clockwise consumer report
    std::vector<uint8_t> buttonPressConsumerReport;  // Button press consumer report
};

class EncoderHandler {
public:
    EncoderHandler(uint8_t numEncoders);
    ~EncoderHandler();

    void begin();
    void updateEncoders();
    
    // Getter methods for encoder information
    long getEncoderPosition(uint8_t encoderIndex) const;
    long getEncoderChange(uint8_t encoderIndex) const;
    EncoderType getEncoderType(uint8_t encoderIndex) const;
    
    // Configuration methods
    void configureEncoder(
        uint8_t encoderIndex, 
        EncoderType type, 
        uint8_t pinA, 
        uint8_t pinB, 
        int8_t direction = 1, 
        uint16_t zeroPosition = 0
    );
    
    void loadEncoderActions(const std::map<String, ActionConfig>& actions);
    
    // Diagnostic methods
    void printEncoderStates();
    void diagnostics();

    void executeEncoderAction(uint8_t encoderIndex, bool clockwise);
    void executeEncoderButtonAction(uint8_t encoderIndex, bool pressed);

private:
    void cleanup();
    void handleMechanicalEncoder(uint8_t encoderIndex);
    void handleAS5600Encoder(uint8_t encoderIndex);

    // Encoder tracking variables
    uint8_t numEncoders;
    Encoder** mechanicalEncoders;
    AS5600* as5600Encoders;

    // Configuration for each encoder
    EncoderConfig* encoderConfigs;
};

extern EncoderHandler* encoderHandler;

void initializeEncoderHandler();
void updateEncoderHandler();
void cleanupEncoderHandler();

#endif // ENCODER_HANDLER_H