#ifndef OTA_UPDATE_MANAGER_H
#define OTA_UPDATE_MANAGER_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <Update.h>
#include <ArduinoJson.h>
#include "VersionManager.h"

class OTAUpdateManager {
public:
    // Initialize the OTA update manager
    static void begin();
    
    // Check for updates from GitHub
    static bool checkForUpdates();
    
    // Perform the update
    static bool performUpdate(const String& url);
    
    // Get update status
    static String getUpdateStatus();
    
    // Get last error message
    static String getLastError();
    
    // Check if update is available
    static bool isUpdateAvailable();
    
    // Get available version
    static String getAvailableVersion();
    
    // Get release notes
    static String getReleaseNotes();
    
    // Get firmware URL
    static String getFirmwareUrl();
    
private:
    // GitHub API configuration
    static const char* GITHUB_API_URL;
    static const char* GITHUB_REPO_OWNER;
    static const char* GITHUB_REPO_NAME;
    
    // Update status
    static String _updateStatus;
    static bool _updateAvailable;
    static String _availableVersion;
    static String _releaseNotes;
    static String _firmwareUrl;
    static String _lastError;
    
    // Helper methods
    static bool parseGitHubRelease(const String& json);
    static void updateProgress(size_t current, size_t total);
};

#endif // OTA_UPDATE_MANAGER_H 