#pragma once

#include <Arduino.h>

class VersionManager {
public:
    // Get version string in format "major.minor.patch"
    static String getVersionString();
    
    // Get individual version components
    static uint8_t getMajorVersion();
    static uint8_t getMinorVersion();
    static uint8_t getPatchVersion();
    static uint32_t getBuildNumber();
    
    // Get build information
    static String getBuildDate();
    static String getBuildTime();
    
    // Get device information
    static String getDeviceName();
    static String getDeviceManufacturer();
    static String getDeviceModel();
    
    // Get version info as JSON string
    static String getVersionInfoJson();
    
    // Check if a given version string is newer than current version
    static bool isNewerVersion(const String& version);
}; 