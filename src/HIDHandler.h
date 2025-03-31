#ifndef HID_HANDLER_H
#define HID_HANDLER_H

#include <Arduino.h>
#include <vector>
#include <queue>
#include <mutex>
#include <map>
#include <set>
#include <algorithm>
#include <ArduinoJson.h>

// HID Report Descriptors
#define HID_KEYBOARD_REPORT_SIZE 8
#define HID_CONSUMER_REPORT_SIZE 4
#define HID_MAX_KEYS 6  // Maximum keys in one report (excluding modifiers)

// Report types
enum HIDReportType {
    HID_REPORT_KEYBOARD,
    HID_REPORT_CONSUMER,
    HID_REPORT_SYSTEM
};

// HID key codes for modifiers
#define KEY_MOD_LCTRL  0x01
#define KEY_MOD_LSHIFT 0x02
#define KEY_MOD_LALT   0x04
#define KEY_MOD_LGUI   0x08
#define KEY_MOD_RCTRL  0x10
#define KEY_MOD_RSHIFT 0x20
#define KEY_MOD_RALT   0x40
#define KEY_MOD_RGUI   0x80

// Modifier key range
#define KEY_LEFT_CTRL   0xE0
#define KEY_LEFT_SHIFT  0xE1
#define KEY_LEFT_ALT    0xE2
#define KEY_LEFT_GUI    0xE3
#define KEY_RIGHT_CTRL  0xE4
#define KEY_RIGHT_SHIFT 0xE5
#define KEY_RIGHT_ALT   0xE6
#define KEY_RIGHT_GUI   0xE7

// HID Report structure
struct HIDReport {
    HIDReportType type;
    uint8_t data[16]; // Buffer large enough for any report type
    uint8_t length;
};

// Macro structure
struct MacroSequence {
    std::vector<HIDReport> reports;
    std::vector<uint16_t> delays;
};

class HIDHandler {
public:
    HIDHandler();
    ~HIDHandler();

    // Initialize HID functionality
    bool begin();

    // Enhanced key handling methods
    bool pressKey(uint8_t key);
    bool releaseKey(uint8_t key);
    bool isKeyPressed(uint8_t key) const;
    bool areAnyKeysPressed() const;
    void clearAllKeys();

    // Send HID reports
    bool sendKeyboardReport(const uint8_t* report, size_t length = HID_KEYBOARD_REPORT_SIZE);
    bool sendConsumerReport(const uint8_t* report, size_t length = HID_CONSUMER_REPORT_SIZE);
    bool sendEmptyKeyboardReport(); // Release all keys
    bool sendEmptyConsumerReport(); // Release all consumer controls

    // Update composite keyboard report from current key state
    bool updateKeyboardReportFromState();

    // Macro handling
    bool executeMacro(const char* macroId);
    bool registerMacro(const char* macroId, const MacroSequence& sequence);

    // Update function for main loop
    void update();

    // Convert hex string report to binary
    static bool hexReportToBinary(const char* hexReport[], size_t count, uint8_t* binaryReport, size_t maxLength);
    static bool hexReportToBinary(const std::vector<String>& hexReport, uint8_t* binaryReport, size_t maxLength);

    // Process button actions from different JSON formats
    static bool processButtonAction(const JsonVariant& buttonPress, uint8_t* binaryReport, size_t maxLength);

private:
    // Helper to convert key code to modifier bit
    static uint8_t keyToModifier(uint8_t key);
    static bool isModifier(uint8_t key);

    // Keyboard and Consumer Control instances
    class KeyboardReportDescriptor {
    public:
        uint8_t report[HID_KEYBOARD_REPORT_SIZE] = {0};
    };

    class ConsumerReportDescriptor {
    public:
        uint8_t report[HID_CONSUMER_REPORT_SIZE] = {0};
    };

    // Current report state
    KeyboardReportDescriptor keyboardState;
    ConsumerReportDescriptor consumerState;

    // Track pressed keys
    std::set<uint8_t> pressedKeys;
    uint8_t activeModifiers = 0;

    // Macro execution variables
    bool executingMacro = false;
    unsigned long nextMacroStepTime = 0;
    size_t currentMacroStep = 0;
    MacroSequence* currentMacro = nullptr;

    // Thread safety for reports
    std::mutex reportMutex;
    std::queue<HIDReport> reportQueue;

    // Helper to process the next report in the queue
    bool processNextReport();

    // Map to store registered macros
    std::map<String, MacroSequence> macros;
};

// Global HID handler instance
extern HIDHandler* hidHandler;

// Helper functions
void initializeHIDHandler();
void updateHIDHandler();
void cleanupHIDHandler();

#endif // HID_HANDLER_H