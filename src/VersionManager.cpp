#include "VersionManager.h"
#include "version.h"
#include <ArduinoJson.h>

String VersionManager::getVersionString() {
    return String(FIRMWARE_VERSION_STRING);
}

uint8_t VersionManager::getMajorVersion() {
    return FIRMWARE_VERSION_MAJOR;
}

uint8_t VersionManager::getMinorVersion() {
    return FIRMWARE_VERSION_MINOR;
}

uint8_t VersionManager::getPatchVersion() {
    return FIRMWARE_VERSION_PATCH;
}

uint32_t VersionManager::getBuildNumber() {
    return 1; // Default build number
}

String VersionManager::getBuildDate() {
    return String(FIRMWARE_BUILD_DATE);
}

String VersionManager::getBuildTime() {
    return String(FIRMWARE_BUILD_TIME);
}

String VersionManager::getDeviceName() {
    return DEVICE_NAME;
}

String VersionManager::getDeviceManufacturer() {
    return DEVICE_MANUFACTURER;
}

String VersionManager::getDeviceModel() {
    return DEVICE_MODEL;
}

String VersionManager::getVersionInfoJson() {
    DynamicJsonDocument doc(512);
    doc["version"] = String(FIRMWARE_VERSION_STRING);
    doc["build"] = 1; // Default build number
    doc["build_date"] = String(FIRMWARE_BUILD_DATE);
    doc["build_time"] = String(FIRMWARE_BUILD_TIME);
    doc["device_name"] = String(DEVICE_NAME);
    doc["device_manufacturer"] = String(DEVICE_MANUFACTURER);
    doc["device_model"] = String(DEVICE_MODEL);
    
    String output;
    serializeJson(doc, output);
    return output;
}

bool VersionManager::isNewerVersion(const String& version) {
    // Parse version string (format: "major.minor.patch")
    int major = 0, minor = 0, patch = 0;
    sscanf(version.c_str(), "%d.%d.%d", &major, &minor, &patch);
    
    // Compare with current version
    if (major > FIRMWARE_VERSION_MAJOR) return true;
    if (major < FIRMWARE_VERSION_MAJOR) return false;
    
    if (minor > FIRMWARE_VERSION_MINOR) return true;
    if (minor < FIRMWARE_VERSION_MINOR) return false;
    
    if (patch > FIRMWARE_VERSION_PATCH) return true;
    return false;
} 