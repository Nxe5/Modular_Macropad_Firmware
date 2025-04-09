#include "JsonUtils.h"

// Implementation of the JSON buffer size estimation function
size_t estimateJsonBufferSize(const String& jsonString, float safetyFactor) {
    size_t len = jsonString.length();
    
    // Count the number of objects, arrays, and string literals to help with the estimate
    int numObjects = 0;
    int numArrays = 0;
    int numStrings = 0;
    
    for (size_t i = 0; i < len; i++) {
        char c = jsonString[i];
        if (c == '{') numObjects++;
        else if (c == '[') numArrays++;
        else if (c == '"') numStrings++;
    }
    
    // Base size (each object or array adds some overhead)
    size_t baseSize = len;
    
    // Add overhead for objects and arrays
    baseSize += (numObjects * 20); // Approximate overhead per object
    baseSize += (numArrays * 10);  // Approximate overhead per array
    
    // Apply the safety factor
    size_t estimatedSize = baseSize * safetyFactor;
    
    // Ensure a minimum size and maximum size
    const size_t MIN_JSON_BUFFER = 2048;    // Minimum 2KB
    const size_t MAX_JSON_BUFFER = 64 * 1024; // Maximum 64KB
    
    if (estimatedSize < MIN_JSON_BUFFER) {
        estimatedSize = MIN_JSON_BUFFER;
    } else if (estimatedSize > MAX_JSON_BUFFER) {
        estimatedSize = MAX_JSON_BUFFER;
    }
    
    return estimatedSize;
} 