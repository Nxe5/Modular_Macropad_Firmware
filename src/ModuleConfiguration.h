#ifndef MODULE_CONFIGURATION_H
#define MODULE_CONFIGURATION_H

#include <Arduino.h>

// Struct for key configuration
struct KeyConfig {
  String key;       // The key assignment (e.g., "A", "Ctrl+Z", "Volume Up")
  String action;    // Action (if applicable, like long press)
};

// Struct for module configuration
struct ModuleConfig {
  String id;                // MAC address as ID (unique)
  String type;              // Module type (button, rotary-encoder, etc.)
  KeyConfig keys[25];       // Key assignments for a 5x5 grid or customizable
  String additionalSettings; // Additional settings (e.g., backlight)
};

// Function declarations
void initializeModuleConfig();
void exportConfig(ModuleConfig moduleConfig);

#endif // MODULE_CONFIGURATION_H