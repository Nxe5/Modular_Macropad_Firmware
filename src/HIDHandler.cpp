// HIDHandler.cpp
#include <ctype.h>
#include <stdlib.h>
#include "HIDHandler.h"
#include <tusb.h>  // Include the TinyUSB header

extern USBCDC USBSerial;

#ifndef TUD_HID_REPORT_DESC_KEYBOARD
#define TUD_HID_REPORT_DESC_KEYBOARD() { \
  0x05, 0x01,       /* Usage Page (Generic Desktop) */ \
  0x09, 0x06,       /* Usage (Keyboard) */ \
  0xA1, 0x01,       /* Collection (Application) */ \
  0x05, 0x07,       /*   Usage Page (Key Codes) */ \
  0x19, 0xE0,       /*   Usage Minimum (224) */ \
  0x29, 0xE7,       /*   Usage Maximum (231) */ \
  0x15, 0x00,       /*   Logical Minimum (0) */ \
  0x25, 0x01,       /*   Logical Maximum (1) */ \
  0x75, 0x01,       /*   Report Size (1) */ \
  0x95, 0x08,       /*   Report Count (8) */ \
  0x81, 0x02,       /*   Input (Data, Variable, Absolute) */ \
  0x95, 0x01,       /*   Report Count (1) */ \
  0x75, 0x08,       /*   Report Size (8) */ \
  0x81, 0x01,       /*   Input (Constant) reserved byte */ \
  0x95, 0x05,       /*   Report Count (5) */ \
  0x75, 0x01,       /*   Report Size (1) */ \
  0x05, 0x08,       /*   Usage Page (LEDs) */ \
  0x19, 0x01,       /*   Usage Minimum (1) */ \
  0x29, 0x05,       /*   Usage Maximum (5) */ \
  0x91, 0x02,       /*   Output (Data, Variable, Absolute), LED report */ \
  0x95, 0x01,       /*   Report Count (1) */ \
  0x75, 0x03,       /*   Report Size (3) */ \
  0x91, 0x01,       /*   Output (Constant), LED report padding */ \
  0x95, 0x06,       /*   Report Count (6) */ \
  0x75, 0x08,       /*   Report Size (8) */ \
  0x15, 0x00,       /*   Logical Minimum (0) */ \
  0x25, 0x65,       /*   Logical Maximum (101) */ \
  0x05, 0x07,       /*   Usage Page (Key Codes) */ \
  0x19, 0x00,       /*   Usage Minimum (0) */ \
  0x29, 0x65,       /*   Usage Maximum (101) */ \
  0x81, 0x00,       /*   Input (Data, Array) */ \
  0xC0              /* End Collection */ \
}
#endif

#ifndef TUD_HID_REPORT_DESC_CONSUMER
#define TUD_HID_REPORT_DESC_CONSUMER() { \
  0x05, 0x0C,       /* Usage Page (Consumer) */ \
  0x09, 0x01,       /* Usage (Consumer Control) */ \
  0xA1, 0x01,       /* Collection (Application) */ \
  0x15, 0x00,       /* Logical Minimum (0) */ \
  0x25, 0x01,       /* Logical Maximum (1) */ \
  0x75, 0x01,       /* Report Size (1) */ \
  0x95, 0x07,       /* Report Count (7) */ \
  0x09, 0xB5,       /* Usage (Scan Next Track) */ \
  0x09, 0xB6,       /* Usage (Scan Previous Track) */ \
  0x09, 0xB7,       /* Usage (Stop) */ \
  0x09, 0xB8,       /* Usage (Play/Pause) */ \
  0x09, 0xCD,       /* Usage (Mute) */ \
  0x09, 0xE2,       /* Usage (Volume Up) */ \
  0x09, 0xE9,       /* Usage (Volume Down) */ \
  0x81, 0x02,       /* Input (Data, Variable, Absolute) */ \
  0x95, 0x01,       /* Report Count (1) */ \
  0x75, 0x01,       /* Report Size (1) */ \
  0x81, 0x03,       /* Input (Constant, Variable, Absolute) */ \
  0xC0              /* End Collection */ \
}
#endif

// Global HID handler instance
HIDHandler* hidHandler = nullptr;

HIDHandler::HIDHandler() {
    memset(&keyboardState, 0, sizeof(keyboardState));
    memset(&consumerState, 0, sizeof(consumerState));
}

HIDHandler::~HIDHandler() {
    // Nothing to free
}

// New methods for multiple key support

bool HIDHandler::isModifier(uint8_t key) {
    return (key >= KEY_LEFT_CTRL && key <= KEY_RIGHT_GUI);
}

uint8_t HIDHandler::keyToModifier(uint8_t key) {
    if (!HIDHandler::isModifier(key)) return 0;
    
    // Convert modifier key code to bit mask
    switch (key) {
        case KEY_LEFT_CTRL:   return KEY_MOD_LCTRL;
        case KEY_LEFT_SHIFT:  return KEY_MOD_LSHIFT;
        case KEY_LEFT_ALT:    return KEY_MOD_LALT;
        case KEY_LEFT_GUI:    return KEY_MOD_LGUI;
        case KEY_RIGHT_CTRL:  return KEY_MOD_RCTRL;
        case KEY_RIGHT_SHIFT: return KEY_MOD_RSHIFT;
        case KEY_RIGHT_ALT:   return KEY_MOD_RALT;
        case KEY_RIGHT_GUI:   return KEY_MOD_RGUI;
        default:              return 0;
    }
}

bool HIDHandler::pressKey(uint8_t key) {
    std::lock_guard<std::mutex> lock(reportMutex);
    
    // Check if key is already pressed
    if (pressedKeys.find(key) != pressedKeys.end()) {
        return true; // Key is already pressed
    }
    
    // Add to pressed keys
    pressedKeys.insert(key);
    
    // If it's a modifier key, update the modifier state
    if (HIDHandler::isModifier(key)) {
        activeModifiers |= keyToModifier(key);
    }
    
    // Update the HID report
    return updateKeyboardReportFromState();
}

bool HIDHandler::releaseKey(uint8_t key) {
    std::lock_guard<std::mutex> lock(reportMutex);
    
    // Check if key is pressed
    auto it = pressedKeys.find(key);
    if (it == pressedKeys.end()) {
        return true; // Key is not pressed, nothing to do
    }
    
    // Remove from pressed keys
    pressedKeys.erase(it);
    
    // If it's a modifier key, update the modifier state
    if (HIDHandler::isModifier(key)) {
        activeModifiers &= ~keyToModifier(key);
    }
    
    // If no keys are pressed, send empty report
    if (pressedKeys.empty()) {
        return sendEmptyKeyboardReport();
    }
    
    // Otherwise update the HID report with remaining keys
    return updateKeyboardReportFromState();
}

bool HIDHandler::isKeyPressed(uint8_t key) const {
    return pressedKeys.find(key) != pressedKeys.end();
}

bool HIDHandler::areAnyKeysPressed() const {
    return !pressedKeys.empty();
}

void HIDHandler::clearAllKeys() {
    std::lock_guard<std::mutex> lock(reportMutex);
    pressedKeys.clear();
    activeModifiers = 0;
    sendEmptyKeyboardReport();
}

bool HIDHandler::updateKeyboardReportFromState() {
    // Create a fresh report
    uint8_t report[HID_KEYBOARD_REPORT_SIZE] = {0};
    
    // Set modifier byte
    report[0] = activeModifiers;
    
    // Add regular keys (up to 6)
    int keyIndex = 2; // First key goes in byte 2 (after modifiers and reserved byte)
    
    for (uint8_t key : pressedKeys) {
        // Skip modifiers as they're handled separately
        if (!HIDHandler::isModifier(key)) {
            // Make sure we don't exceed the report size
            if (keyIndex < HID_KEYBOARD_REPORT_SIZE) {
                report[keyIndex++] = key;
            } else {
                // No more room in the report - this is N-key rollover limitation
                USBSerial.println("Warning: Too many keys pressed, some ignored");
                break;
            }
        }
    }
    
    // Send the updated report
    return sendKeyboardReport(report, HID_KEYBOARD_REPORT_SIZE);
}

// Fixed sendKeyboardReport with proper const casting
bool HIDHandler::sendKeyboardReport(const uint8_t* report, size_t length) {
    if (!report || length != HID_KEYBOARD_REPORT_SIZE) {
        USBSerial.println("Invalid keyboard report");
        return false;
    }
    
    // Only send report if it's different from current state
    bool hasChanges = false;
    for (size_t i = 0; i < HID_KEYBOARD_REPORT_SIZE; i++) {
        if (keyboardState.report[i] != report[i]) {
            hasChanges = true;
            break;
        }
    }
    
    if (!hasChanges) {
        return true; // No changes needed
    }
    
    // Copy report to our state
    memcpy(keyboardState.report, report, HID_KEYBOARD_REPORT_SIZE);
    
    if (!tud_mounted()) {
        USBSerial.println("USB device not mounted");
        return false;
    }
    
    if (tud_hid_ready()) {
        // Extract the modifier byte from report[0]
        uint8_t modifier = report[0];
        
        // Create a non-const copy of the keycodes portion of the report
        uint8_t keycodes[6];
        memcpy(keycodes, &report[2], 6);
        
        // For keyboard report, we send modifier, reserved byte, and up to 6 key codes
        bool success = tud_hid_keyboard_report(1, modifier, keycodes);
        
        if (success) {
            USBSerial.print("Keyboard report sent: ");
            for (size_t i = 0; i < HID_KEYBOARD_REPORT_SIZE; i++) {
                USBSerial.printf("%02X ", report[i]);
            }
            USBSerial.println();
            return true;
        } else {
            USBSerial.println("Failed to send keyboard report");
            return false;
        }
    } else {
        USBSerial.println("HID not ready to send keyboard report");
        return false;
    }
}

bool HIDHandler::sendEmptyKeyboardReport() {
    uint8_t emptyReport[HID_KEYBOARD_REPORT_SIZE] = {0};
    return sendKeyboardReport(emptyReport, HID_KEYBOARD_REPORT_SIZE);
}

bool HIDHandler::sendConsumerReport(const uint8_t* report, size_t length) {
    if (!report || length != HID_CONSUMER_REPORT_SIZE) {
        USBSerial.println("Invalid consumer report");
        return false;
    }
    
    // Copy report to our state
    memcpy(consumerState.report, report, HID_CONSUMER_REPORT_SIZE);
    
    if (!tud_mounted()) {
        USBSerial.println("USB device not mounted");
        return false;
    }
    
    if (tud_hid_ready()) {
        // For consumer report, we send the 16-bit usage code
        uint16_t consumerCode = 0x0000;
        
        // Map the consumer codes
        switch(report[2]) {
            case 0xE9: // Volume Up
                consumerCode = 0x00E9;
                USBSerial.println("Preparing Volume UP Command");
                break;
            case 0xEA: // Volume Down
                consumerCode = 0x00EA;
                USBSerial.println("Preparing Volume DOWN Command");
                break;
            case 0xE2: // Mute
                consumerCode = 0x00E2;
                USBSerial.println("Preparing Mute Command");
                break;
            case 0xCD: // Play/Pause
                consumerCode = 0x00CD;
                USBSerial.println("Preparing Play/Pause Command");
                break;
            case 0xB5: // Next Track
                consumerCode = 0x00B5;
                USBSerial.println("Preparing Next Track Command");
                break;
            case 0xB6: // Previous Track
                consumerCode = 0x00B6;
                USBSerial.println("Preparing Previous Track Command");
                break;
            case 0xB7: // Stop
                consumerCode = 0x00B7;
                USBSerial.println("Preparing Stop Command");
                break;
            case 0xB8: // Play
                consumerCode = 0x00B8;
                USBSerial.println("Preparing Play Command");
                break;
        }
        
        USBSerial.printf("Raw Consumer Report: %02X %02X %02X %02X\n",
                         report[0], report[1], report[2], report[3]);
        USBSerial.printf("Consumer Code: 0x%04X\n", consumerCode);
        
        bool success = false;
        // Use Report ID 0x04 as defined in your HID descriptor
        for (int attempts = 0; attempts < 3; attempts++) {
            success = tud_hid_report(0x04, reinterpret_cast<uint8_t*>(&consumerCode), sizeof(consumerCode));
            if (success) {
                USBSerial.printf("Consumer Report Sent Successfully (Attempt %d)\n", attempts + 1);
                break;
            } else {
                USBSerial.printf("Consumer Report Send Failed (Attempt %d)\n", attempts + 1);
                delay(10);
            }
        }
        return success;
    } else {
        USBSerial.println("HID not ready to send consumer report");
        return false;
    }
}

bool HIDHandler::sendEmptyConsumerReport() {
    uint8_t emptyReport[HID_CONSUMER_REPORT_SIZE] = {0};
    return sendConsumerReport(emptyReport, HID_CONSUMER_REPORT_SIZE);
}

bool HIDHandler::executeMacro(const char* macroId) {
    if (!macroId) return false;
    String macroKey = String(macroId);
    auto it = macros.find(macroKey);
    if (it == macros.end()) {
        USBSerial.printf("Macro '%s' not found\n", macroId);
        return false;
    }
    if (executingMacro) {
        USBSerial.println("Already executing a macro, ignoring request");
        return false;
    }
    currentMacro = &(it->second);
    currentMacroStep = 0;
    executingMacro = true;
    nextMacroStepTime = millis();
    USBSerial.printf("Starting execution of macro '%s'\n", macroId);
    return true;
}

void HIDHandler::update() {
    std::lock_guard<std::mutex> lock(reportMutex);
    while (!reportQueue.empty()) {
        if (!processNextReport()) {
            break;
        }
    }
    if (executingMacro && currentMacro) {
        unsigned long currentTime = millis();
        if (currentTime >= nextMacroStepTime) {
            if (currentMacroStep < currentMacro->reports.size()) {
                const HIDReport& report = currentMacro->reports[currentMacroStep];
                bool success = false;
                switch (report.type) {
                    case HID_REPORT_KEYBOARD:
                        success = sendKeyboardReport(report.data, report.length);
                        break;
                    case HID_REPORT_CONSUMER:
                        success = sendConsumerReport(report.data, report.length);
                        break;
                    default:
                        USBSerial.println("Unknown report type in macro");
                        break;
                }
                uint16_t delayTime = (currentMacroStep < currentMacro->delays.size()) ? currentMacro->delays[currentMacroStep] : 50;
                nextMacroStepTime = currentTime + delayTime;
                currentMacroStep++;
                USBSerial.printf("Executed macro step %d/%d\n", currentMacroStep, currentMacro->reports.size());
            } else {
                USBSerial.println("Macro execution complete");
                executingMacro = false;
                currentMacro = nullptr;
                currentMacroStep = 0;
                sendEmptyKeyboardReport();
                sendEmptyConsumerReport();
            }
        }
    }
}

bool HIDHandler::processNextReport() {
    if (reportQueue.empty()) return false;
    HIDReport report = reportQueue.front();
    reportQueue.pop();
    bool success = false;
    switch (report.type) {
        case HID_REPORT_KEYBOARD:
            success = sendKeyboardReport(report.data, report.length);
            break;
        case HID_REPORT_CONSUMER:
            success = sendConsumerReport(report.data, report.length);
            break;
        default:
            USBSerial.println("Unknown report type");
            break;
    }
    return success;
}

bool HIDHandler::hexReportToBinary(const char* hexReport[], size_t count, uint8_t* binaryReport, size_t maxLength) {
    if (!hexReport || !binaryReport || maxLength < count) {
        return false;
    }
    for (size_t i = 0; i < count && i < maxLength; i++) {
        const char* hexStr = hexReport[i];
        if (hexStr[0] == '0' && (hexStr[1] == 'x' || hexStr[1] == 'X')) {
            hexStr += 2;
        }
        char* endPtr;
        binaryReport[i] = (uint8_t)strtol(hexStr, &endPtr, 16);
        if (*endPtr != '\0') {
            USBSerial.printf("Invalid hex value: %s\n", hexReport[i]);
            return false;
        }
    }
    return true;
}

bool HIDHandler::hexReportToBinary(const std::vector<String>& hexReport, uint8_t* binaryReport, size_t maxLength) {
    if (hexReport.size() > maxLength) {
        return false;
    }
    for (size_t i = 0; i < hexReport.size(); i++) {
        const char* hexStr = hexReport[i].c_str();
        if (hexStr[0] == '0' && (hexStr[1] == 'x' || hexStr[1] == 'X')) {
            hexStr += 2;
        }
        char* endPtr;
        long value = strtol(hexStr, &endPtr, 16);
        if (*endPtr != '\0') {
            USBSerial.printf("Invalid hex value: %s\n", hexReport[i].c_str());
            return false;
        }
        binaryReport[i] = (uint8_t)value;
    }
    return true;
}

// Initialize HID functionality
bool HIDHandler::begin() {
    // This is done by TinyUSB, just return true
    return true;
}

// Example for HID handler initialization
void initializeHIDHandler() {
    if (hidHandler != nullptr) {
        // Clean up existing instance to avoid memory leaks
        delete hidHandler;
        hidHandler = nullptr;
    }
    
    USBSerial.println("Creating new HID handler instance...");
    hidHandler = new HIDHandler();
    
    if (hidHandler == nullptr) {
        USBSerial.println("CRITICAL ERROR: Failed to allocate memory for HID handler!");
        return;
    }
    
    bool initSuccess = hidHandler->begin();
    USBSerial.printf("HID handler initialization %s\n", 
                 initSuccess ? "SUCCESSFUL" : "FAILED");
                 
    if (!initSuccess) {
        // Try alternative initialization approach
        USBSerial.println("Attempting alternative HID initialization...");
        // Add any alternative approach here
    }
}

void updateHIDHandler() {
    if (hidHandler) {
        hidHandler->update();
    }
}

void cleanupHIDHandler() {
    if (hidHandler) {
        delete hidHandler;
        hidHandler = nullptr;
    }
}