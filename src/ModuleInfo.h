// ModuleInfo.h
#ifndef MODULE_INFO_H
#define MODULE_INFO_H

#include <Arduino.h>

enum class ModuleType {
    FULL_MAIN,
    FULL_SLIDER,
    FULL_ENCODER,
    HALF_SLIDER,
    HALF_ENCODER,
    HALF_BUTTON,
    QUARTER_BUTTON,
    QUARTER_ENCODER,
    CUSTOM
};

enum class ModuleSize {
    FULL,     // 1x1
    HALF,     // 1x0.5
    QUARTER   // 0.5x0.5
};

struct ModuleCapabilities {
    ModuleType type;
    ModuleSize size;
    bool hasDisplay;
    uint8_t numButtons;
    uint8_t numLEDs;
    uint8_t numRotaryEncoders;  // Standard rotary encoders
    uint8_t numSliders;         // Linear potentiometers
    uint8_t numAS5600Encoders;  // AS5600 12-bit magnetic encoders
    String moduleVersion;
    String uniqueId;
    String customName;
    uint8_t numLayers;
};

struct ModuleInfo {
    String macAddress;
    String infoJson;
    String componentsJson;
    String ledsJson;
    bool hasDisplay;
    uint8_t numButtons;
    uint8_t numLEDs;
    uint8_t numRotaryEncoders;
    uint8_t numSliders;
    uint8_t numAS5600Encoders;
};

void initializeModuleInfo();
ModuleCapabilities getModuleCapabilities();
String getModuleInfoJson();
const char* getModuleTypeName(ModuleType type);
const char* getModuleSizeName(ModuleSize size);
bool loadModuleConfiguration();
bool saveModuleConfiguration();
bool mergeConfigFiles();

#endif // MODULE_INFO_H