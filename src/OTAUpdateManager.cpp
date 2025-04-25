#include "OTAUpdateManager.h"
#include <ArduinoJson.h>
#include <WiFi.h>
#include <Update.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>

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
OTAUpdateManager::UpdateState OTAUpdateManager::_updateState = OTAUpdateManager::IDLE;
int OTAUpdateManager::_updateProgress = 0;
bool OTAUpdateManager::_recoveryMode = false;
MD5Builder OTAUpdateManager::_md5Builder;
Preferences OTAUpdateManager::_prefs;

void OTAUpdateManager::begin() {
    _updateStatus = "Ready";
    _updateState = IDLE;
    _updateProgress = 0;
    
    // Initialize preferences
    _prefs.begin("otaupdate", false);
    
    // Check if we're in recovery mode
    _recoveryMode = _prefs.getBool("recovery_mode", false);
    
    // If coming from a failed update, try to rollback
    if (_prefs.getBool("update_failed", false)) {
        if (rollbackFirmware()) {
            _prefs.putBool("update_failed", false);
            _prefs.putBool("recovery_mode", false);
            _updateStatus = "Rollback successful";
        } else {
            enterRecoveryMode();
        }
    }
    
    // Register update progress callback
    Update.onProgress([](size_t progress, size_t total) {
        updateProgress(progress, total);
    });
}

bool OTAUpdateManager::checkForUpdates() {
    // Check if WiFi is connected before proceeding
    if (WiFi.status() != WL_CONNECTED) {
        _lastError = "WiFi not connected. Please connect to a network first.";
        _updateStatus = _lastError;
        setUpdateState(FAILED);
        return false;
    }
    
    setUpdateState(CHECKING);
    _updateProgress = 10;
    
    HTTPClient http;
    String url = String(GITHUB_API_URL) + "/repos/" + GITHUB_REPO_OWNER + "/" + GITHUB_REPO_NAME + "/releases/latest";
    http.begin(url);
    
    // Add user agent to avoid GitHub API rate limiting
    http.addHeader("User-Agent", "ESP32-ModularMacropad");
    
    _updateStatus = "Checking for updates...";
    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK) {
        _lastError = "Failed to check for updates. HTTP error: " + String(httpCode) + ". Make sure WiFi is connected and stable.";
        _updateStatus = _lastError;
        setUpdateState(FAILED);
        http.end();
        return false;
    }

    String payload = http.getString();
    http.end();
    
    // Log some debug info
    Serial.println("GitHub API response received: " + String(payload.length()) + " bytes");
    Serial.println("First 100 chars: " + payload.substring(0, 100));
    
    _updateProgress = 30;

    if (!parseGitHubRelease(payload)) {
        _lastError = "Failed to parse update information";
        _updateStatus = _lastError;
        setUpdateState(FAILED);
        return false;
    }
    
    _updateProgress = 50;

    String currentVersion = VersionManager::getVersionString();
    if (VersionManager::isNewerVersion(_availableVersion)) {
        _updateAvailable = true;
        _updateStatus = "Update available: " + _availableVersion;
        _updateProgress = 100;
        setUpdateState(IDLE);
    } else {
        _updateAvailable = false;
        _updateStatus = "No updates available";
        _updateProgress = 100;
        setUpdateState(IDLE);
    }

    return true;
}

bool OTAUpdateManager::performUpdate(const String& url) {
    return performUpdate(url, nullptr);
}

bool OTAUpdateManager::performUpdate(const String& url, UpdateProgressCallback callback) {
    if (url.isEmpty()) {
        _lastError = "No firmware URL provided";
        _updateStatus = _lastError;
        setUpdateState(FAILED);
        return false;
    }
    
    // Save update metadata before starting
    _prefs.putString("current_version", VersionManager::getVersionString());
    _prefs.putString("update_version", _availableVersion);
    _prefs.putBool("update_in_progress", true);
    
    setUpdateState(DOWNLOADING);
    _updateProgress = 0;
    _updateStatus = "Downloading firmware...";
    
    // Validate certificate if HTTPS
    if (url.startsWith("https://") && !validateCertificate(url)) {
        _lastError = "Invalid certificate";
        _updateStatus = _lastError;
        setUpdateState(FAILED);
        return false;
    }

    HTTPClient http;
    http.begin(url);
    
    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK) {
        _lastError = "Failed to download firmware. HTTP error: " + String(httpCode);
        _updateStatus = _lastError;
        setUpdateState(FAILED);
        http.end();
        return false;
    }

    int contentLength = http.getSize();
    if (contentLength <= 0) {
        _lastError = "Invalid firmware size";
        _updateStatus = _lastError;
        setUpdateState(FAILED);
        http.end();
        return false;
    }
    
    // Start MD5 calculation
    _md5Builder.begin();
    
    // Check if enough space is available
    if (!Update.begin(contentLength)) {
        _lastError = "Not enough space for update";
        _updateStatus = _lastError;
        setUpdateState(FAILED);
        http.end();
        return false;
    }
    
    setUpdateState(INSTALLING);
    _updateStatus = "Installing firmware...";
    
    WiFiClient* stream = http.getStreamPtr();
    size_t written = 0;
    uint8_t buff[1024] = { 0 };
    
    while (http.connected() && (written < contentLength)) {
        size_t size = stream->available();
        
        if (size) {
            size_t c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));
            
            // Update MD5 checksum
            _md5Builder.add(buff, c);
            
            // Write data to flash
            size_t w = Update.write(buff, c);
            if (w != c) {
                _lastError = "Write error";
                _updateStatus = _lastError;
                setUpdateState(FAILED);
                http.end();
                return false;
            }
            
            written += c;
            
            // Update progress
            _updateProgress = (written * 100) / contentLength;
            _updateStatus = "Installing: " + String(_updateProgress) + "%";
            
            // Call the progress callback if provided
            if (callback != nullptr) {
                callback(written, contentLength, _updateProgress);
            }
        }
        
        delay(1); // Small delay to prevent watchdog timeouts
    }
    
    http.end();
    
    // Finalize MD5 calculation
    _md5Builder.calculate();
    String md5 = _md5Builder.toString();
    
    // Store MD5 for verification after reboot
    _prefs.putString("update_md5", md5);
    
    setUpdateState(VERIFYING);
    _updateStatus = "Verifying update...";
    
    if (Update.end()) {
        _updateStatus = "Update successful, restarting...";
        setUpdateState(COMPLETE);
        
        // Clear the update_failed flag before restarting
        _prefs.putBool("update_failed", false);
        _prefs.putBool("update_in_progress", false);
        _prefs.end();
        
        delay(1000);
        ESP.restart();
        return true;
    } else {
        _lastError = "Update failed: " + String(Update.getError());
        _updateStatus = _lastError;
        setUpdateState(FAILED);
        
        // Mark update as failed
        _prefs.putBool("update_failed", true);
        return false;
    }
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
    // Increase buffer size to handle larger GitHub responses
    DynamicJsonDocument doc(4096);
    DeserializationError error = deserializeJson(doc, json);
    
    if (error) {
        _lastError = "Failed to parse JSON: " + String(error.c_str());
        Serial.println("JSON parse error: " + String(error.c_str()));
        Serial.println("JSON sample: " + json.substring(0, 200));
        return false;
    }

    // Check if we have a valid response with required fields
    if (!doc.containsKey("tag_name") || !doc.containsKey("assets")) {
        _lastError = "Invalid GitHub release format: missing required fields";
        Serial.println("Invalid GitHub response: missing required fields");
        return false;
    }

    // Extract version from tag_name, removing 'v' prefix if present
    String tagName = doc["tag_name"].as<String>();
    _availableVersion = tagName.startsWith("v") ? tagName.substring(1) : tagName;
    
    // Safely extract release notes
    _releaseNotes = doc.containsKey("body") ? doc["body"].as<String>() : "No release notes available";

    // Find the firmware binary asset
    bool foundBinary = false;
    JsonArray assets = doc["assets"];
    for (JsonObject asset : assets) {
        if (!asset.containsKey("name") || !asset.containsKey("browser_download_url")) {
            continue;
        }
        
        String name = asset["name"].as<String>();
        if (name.endsWith(".bin")) {
            _firmwareUrl = asset["browser_download_url"].as<String>();
            foundBinary = true;
            Serial.println("Found firmware binary: " + name);
            Serial.println("Download URL: " + _firmwareUrl);
            break;
        }
    }

    if (!foundBinary) {
        _lastError = "No firmware binary found in release";
        Serial.println("No firmware binary found in release assets");
        return false;
    }

    return true;
}

void OTAUpdateManager::updateProgress(size_t current, size_t total) {
    int progress = (current * 100) / total;
    _updateProgress = progress;
    _updateStatus = "Updating: " + String(progress) + "%";
}

bool OTAUpdateManager::verifyUpdate(const String& md5Hash) {
    _updateStatus = "Verifying firmware integrity...";
    
    // Compare stored MD5 with provided hash
    String storedMD5 = _prefs.getString("update_md5", "");
    
    if (storedMD5.isEmpty()) {
        _lastError = "No stored MD5 hash found";
        return false;
    }
    
    if (storedMD5.equals(md5Hash)) {
        _updateStatus = "Firmware integrity verified";
        return true;
    } else {
        _lastError = "MD5 verification failed";
        _updateStatus = _lastError;
        return false;
    }
}

void OTAUpdateManager::setUpdateState(UpdateState state) {
    _updateState = state;
    
    switch (state) {
        case IDLE:
            _updateStatus = "Idle";
            break;
        case CHECKING:
            _updateStatus = "Checking for updates";
            break;
        case DOWNLOADING:
            _updateStatus = "Downloading firmware";
            break;
        case INSTALLING:
            _updateStatus = "Installing firmware";
            break;
        case VERIFYING:
            _updateStatus = "Verifying firmware";
            break;
        case COMPLETE:
            _updateStatus = "Update complete";
            break;
        case FAILED:
            _updateStatus = "Update failed";
            break;
        case RECOVERY_MODE:
            _updateStatus = "Recovery mode";
            break;
    }
}

OTAUpdateManager::UpdateState OTAUpdateManager::getUpdateState() {
    return _updateState;
}

void OTAUpdateManager::enterRecoveryMode() {
    _recoveryMode = true;
    _prefs.putBool("recovery_mode", true);
    setUpdateState(RECOVERY_MODE);
}

bool OTAUpdateManager::isInRecoveryMode() {
    return _recoveryMode;
}

bool OTAUpdateManager::rollbackFirmware() {
    _updateStatus = "Rolling back to previous firmware...";
    
    const esp_partition_t* running = esp_ota_get_running_partition();
    const esp_partition_t* previous = esp_ota_get_next_update_partition(nullptr);
    
    if (running == nullptr || previous == nullptr) {
        _lastError = "No valid partition found for rollback";
        _updateStatus = _lastError;
        return false;
    }
    
    if (esp_ota_set_boot_partition(previous) != ESP_OK) {
        _lastError = "Failed to set boot partition";
        _updateStatus = _lastError;
        return false;
    }
    
    _updateStatus = "Rollback successful, restarting...";
    delay(1000);
    ESP.restart();
    return true;
}

bool OTAUpdateManager::verifyBootIntegrity() {
    _updateStatus = "Verifying boot integrity...";
    
    // Check if we're in a good state after an update
    bool updateInProgress = _prefs.getBool("update_in_progress", false);
    
    if (updateInProgress) {
        // Update was interrupted or failed
        _lastError = "Update process was interrupted";
        _updateStatus = _lastError;
        _prefs.putBool("update_failed", true);
        return false;
    }
    
    return true;
}

int OTAUpdateManager::getUpdateProgress() {
    return _updateProgress;
}

String OTAUpdateManager::calculateMD5(const String& firmwareUrl) {
    _updateStatus = "Calculating MD5 checksum...";
    
    HTTPClient http;
    http.begin(firmwareUrl);
    
    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK) {
        _lastError = "Failed to download firmware for MD5 calculation";
        return "";
    }
    
    _md5Builder.begin();
    
    WiFiClient* stream = http.getStreamPtr();
    uint8_t buf[1024];
    
    while (http.connected()) {
        size_t size = stream->available();
        if (size) {
            int c = stream->readBytes(buf, ((size > sizeof(buf)) ? sizeof(buf) : size));
            _md5Builder.add(buf, c);
        } else {
            break;
        }
    }
    
    http.end();
    
    _md5Builder.calculate();
    return _md5Builder.toString();
}

bool OTAUpdateManager::validateCertificate(const String& url) {
    // Implement certificate validation for HTTPS
    // For simplicity, we're returning true, but in a production environment
    // you should implement proper certificate validation
    return true;
}

bool OTAUpdateManager::saveUpdateMetadata() {
    _prefs.putString("available_version", _availableVersion);
    _prefs.putString("firmware_url", _firmwareUrl);
    _prefs.putString("release_notes", _releaseNotes);
    return true;
}

bool OTAUpdateManager::loadUpdateMetadata() {
    _availableVersion = _prefs.getString("available_version", "");
    _firmwareUrl = _prefs.getString("firmware_url", "");
    _releaseNotes = _prefs.getString("release_notes", "");
    return !_availableVersion.isEmpty();
} 