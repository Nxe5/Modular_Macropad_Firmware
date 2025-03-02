#ifndef HID_HANDLER_H
#define HID_HANDLER_H

#include <Arduino.h>
#include <vector>
#include <queue>
#include <mutex>
#include <map>
#include <ArduinoJson.h>

// HID Report Descriptors
#define HID_KEYBOARD_REPORT_SIZE 8
#define HID_CONSUMER_REPORT_SIZE 4

// Report types
enum HIDReportType {
    HID_REPORT_KEYBOARD,
    HID_REPORT_CONSUMER,
    HID_REPORT_SYSTEM
};

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

    // Send HID reports
    bool sendKeyboardReport(const uint8_t* report, size_t length = HID_KEYBOARD_REPORT_SIZE);
    bool sendConsumerReport(const uint8_t* report, size_t length = HID_CONSUMER_REPORT_SIZE);
    bool sendEmptyKeyboardReport(); // Release all keys
    bool sendEmptyConsumerReport(); // Release all consumer controls

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