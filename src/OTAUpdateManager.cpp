#include "OTAUpdateManager.h"
#include <ArduinoJson.h>

// Static member initialization
const char* OTAUpdateManager::GITHUB_API_URL = "https://api.github.com";
const char* OTAUpdateManager::GITHUB_REPO_OWNER = "Nxe5";
const char* OTAUpdateManager::GITHUB_REPO_NAME = "Modular_Macropad_Firmware";
String OTAUpdateManager::_updateStatus = "Idle";
bool OTAUpdateManager::_updateAvailable = false;
String OTAUpdateManager::_availableVersion = "";
String OTAUpdateManager::_releaseNotes = "";
String OTAUpdateManager::_firmwareUrl = "";
String OTAUpdateManager::_lastError = "";

void OTAUpdateManager::begin() {
    _updateStatus = "Ready";
}

bool OTAUpdateManager::checkForUpdates() {
    HTTPClient http;
    String url = String(GITHUB_API_URL) + "/repos/" + GITHUB_REPO_OWNER + "/" + GITHUB_REPO_NAME + "/releases/latest";
    http.begin(url);
    
    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK) {
        _updateStatus = "Failed to check for updates. HTTP error: " + String(httpCode);
        http.end();
        return false;
    }

    String payload = http.getString();
    http.end();

    if (!parseGitHubRelease(payload)) {
        _updateStatus = "Failed to parse update information";
        return false;
    }

    String currentVersion = VersionManager::getVersionString();
    if (_availableVersion > currentVersion) {
        _updateAvailable = true;
        _updateStatus = "Update available: " + _availableVersion;
    } else {
        _updateAvailable = false;
        _updateStatus = "No updates available";
    }

    return true;
}

bool OTAUpdateManager::performUpdate(const String& url) {
    if (url.isEmpty()) {
        _updateStatus = "No firmware URL provided";
        return false;
    }

    HTTPClient http;
    http.begin(url);
    
    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK) {
        _updateStatus = "Failed to download firmware. HTTP error: " + String(httpCode);
        http.end();
        return false;
    }

    int contentLength = http.getSize();
    if (contentLength <= 0) {
        _updateStatus = "Invalid firmware size";
        http.end();
        return false;
    }

    if (!Update.begin(contentLength)) {
        _updateStatus = "Not enough space for update";
        http.end();
        return false;
    }

    size_t written = Update.writeStream(http.getStream());
    if (written != contentLength) {
        _updateStatus = "Failed to write firmware";
        Update.end();
        http.end();
        return false;
    }

    if (!Update.end()) {
        _updateStatus = "Update failed: " + String(Update.getError());
        http.end();
        return false;
    }

    http.end();
    _updateStatus = "Update successful, restarting...";
    ESP.restart();
    return true;
}

String OTAUpdateManager::getUpdateStatus() {
    return _updateStatus;
}

String OTAUpdateManager::getLastError() {
    return _lastError;
}

bool OTAUpdateManager::isUpdateAvailable() {
    return _updateAvailable;
}

String OTAUpdateManager::getAvailableVersion() {
    return _availableVersion;
}

String OTAUpdateManager::getReleaseNotes() {
    return _releaseNotes;
}

String OTAUpdateManager::getFirmwareUrl() {
    return _firmwareUrl;
}

bool OTAUpdateManager::parseGitHubRelease(const String& json) {
    StaticJsonDocument<2048> doc;
    DeserializationError error = deserializeJson(doc, json);
    
    if (error) {
        _updateStatus = "Failed to parse JSON: " + String(error.c_str());
        return false;
    }

    _availableVersion = doc["tag_name"].as<String>();
    _releaseNotes = doc["body"].as<String>();

    // Find the firmware binary asset
    JsonArray assets = doc["assets"];
    for (JsonObject asset : assets) {
        String name = asset["name"].as<String>();
        if (name.endsWith(".bin")) {
            _firmwareUrl = asset["browser_download_url"].as<String>();
            break;
        }
    }

    if (_firmwareUrl.isEmpty()) {
        _updateStatus = "No firmware binary found in release";
        return false;
    }

    return true;
}

void OTAUpdateManager::updateProgress(size_t current, size_t total) {
    int progress = (current * 100) / total;
    _updateStatus = "Updating: " + String(progress) + "%";
} 