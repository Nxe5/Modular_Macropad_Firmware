#ifndef OTA_UPDATE_MANAGER_H
#define OTA_UPDATE_MANAGER_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <Update.h>
#include <ArduinoJson.h>
#include "VersionManager.h"
#include <ESP32-targz.h>  // For handling compressed updates
#include <MD5Builder.h>   // For update validation
#include <Preferences.h>  // For storing update state

// Update progress callback function pointer
typedef void (*UpdateProgressCallback)(size_t current, size_t total, int percentage);

class OTAUpdateManager {
public:
    // Update states
    enum UpdateState {
        IDLE,
        CHECKING,
        DOWNLOADING,
        INSTALLING,
        VERIFYING,
        COMPLETE,
        FAILED,
        RECOVERY_MODE
    };
    
    // Initialize the OTA update manager
    static void begin();
    
    // Check for updates from GitHub
    static bool checkForUpdates();
    
    // Perform the update
    static bool performUpdate(const String& url);
    
    // Perform update with progress callback
    static bool performUpdate(const String& url, UpdateProgressCallback callback);
    
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
    
    // Verify update integrity
    static bool verifyUpdate(const String& md5Hash);
    
    // Set update state
    static void setUpdateState(UpdateState state);
    
    // Get update state
    static UpdateState getUpdateState();
    
    // Enter recovery mode
    static void enterRecoveryMode();
    
    // Check if in recovery mode
    static bool isInRecoveryMode();
    
    // Rollback to previous firmware
    static bool rollbackFirmware();
    
    // Verify boot integrity
    static bool verifyBootIntegrity();
    
    // Get update progress percentage
    static int getUpdateProgress();
    
private:
    // GitHub API configuration
    static const char* GITHUB_API_URL;
    static const char* GITHUB_REPO_OWNER;
    static const char* GITHUB_REPO_NAME;
    
    // Update status and state
    static String _updateStatus;
    static bool _updateAvailable;
    static String _availableVersion;
    static String _releaseNotes;
    static String _firmwareUrl;
    static String _lastError;
    static UpdateState _updateState;
    static int _updateProgress;
    static bool _recoveryMode;
    static MD5Builder _md5Builder;
    static Preferences _prefs;
    
    // Helper methods
    static bool parseGitHubRelease(const String& json);
    static void updateProgress(size_t current, size_t total);
    static bool saveUpdateMetadata();
    static bool loadUpdateMetadata();
    static String calculateMD5(const String& firmwareUrl);
    static bool validateCertificate(const String& url);
};

#endif // OTA_UPDATE_MANAGER_H 