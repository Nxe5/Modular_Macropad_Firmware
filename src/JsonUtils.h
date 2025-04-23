#pragma once

#include <Arduino.h>

// JSON utility functions
#ifdef __cplusplus
extern "C" {
#endif

// Centralized declaration of the JSON buffer size estimation function
// This function estimates the required buffer size for JSON parsing
// It's implemented in ModuleSetup.cpp
size_t estimateJsonBufferSize(const String& jsonString, float safetyFactor = 1.5);

#ifdef __cplusplus
}
#endif 