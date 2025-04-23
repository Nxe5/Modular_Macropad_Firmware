#include "VersionManager.h"
#include "version.h"
#include <ArduinoJson.h>

String VersionManager::getVersionString() {
    return FIRMWARE_VERSION_STRING;
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
    return FIRMWARE_BUILD_NUMBER;
}

String VersionManager::getBuildDate() {
    return String(FIRMWARE_BUILD_DATE);
}

String VersionManager::getBuildTime() {
    return String(FIRMWARE_BUILD_TIME);
}

String VersionManager::getDeviceName() {
    return String(DEVICE_NAME);
}

String VersionManager::getDeviceManufacturer() {
    return String(DEVICE_MANUFACTURER);
}

String VersionManager::getDeviceModel() {
    return String(DEVICE_MODEL);
}

String VersionManager::getVersionInfoJson() {
    StaticJsonDocument<512> doc;
    doc["version"] = FIRMWARE_VERSION_STRING;
    doc["major"] = FIRMWARE_VERSION_MAJOR;
    doc["minor"] = FIRMWARE_VERSION_MINOR;
    doc["patch"] = FIRMWARE_VERSION_PATCH;
    doc["build"] = FIRMWARE_BUILD_NUMBER;
    doc["buildDate"] = FIRMWARE_BUILD_DATE;
    doc["buildTime"] = FIRMWARE_BUILD_TIME;
    doc["deviceName"] = DEVICE_NAME;
    doc["manufacturer"] = DEVICE_MANUFACTURER;
    doc["model"] = DEVICE_MODEL;
    
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