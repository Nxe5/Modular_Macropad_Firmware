#include "RecoveryBootloader.h"
#include "OTAUpdateManager.h"
#include "ConfigManager.h"
#include <LittleFS.h>

// Static member initialization
RecoveryBootloader::BootloaderState RecoveryBootloader::_bootloaderState = RecoveryBootloader::NORMAL_BOOT;
Preferences RecoveryBootloader::_prefs;
String RecoveryBootloader::_statusMessage = "Normal boot";

void RecoveryBootloader::begin() {
    // Initialize preferences
    _prefs.begin("recovery", false);
    
    // Increment boot count to detect boot loops
    incrementBootCount();
    
    // Check if physical recovery button is pressed (GPIO0/BOOT button)
    pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);
    
    // Determine if we should enter recovery mode
    if (shouldEnterRecoveryMode()) {
        enterRecoveryMode();
    } else {
        _bootloaderState = NORMAL_BOOT;
        
        // If we've booted successfully, reset the boot count after a delay
        // We use a delay to ensure we've fully booted before resetting
        // This is non-blocking, just schedules the reset
        static unsigned long resetTime = millis() + 10000; // 10 seconds after boot
        
        // This would need to be checked in the main loop:
        // if (millis() > resetTime) { resetBootCount(); }
    }
}

bool RecoveryBootloader::shouldEnterRecoveryMode() {
    // Check for physical button press (BOOT/GPIO0 button held during startup)
    if (isRecoveryButtonPressed()) {
        _statusMessage = "Recovery mode triggered by button press";
        return true;
    }
    
    // Check for boot loop
    if (isInBootLoop()) {
        _statusMessage = "Recovery mode triggered by boot loop detection";
        return true;
    }
    
    // Check if previous update failed
    bool updateFailed = _prefs.getBool("update_failed", false);
    if (updateFailed) {
        _statusMessage = "Recovery mode triggered by failed update";
        return true;
    }
    
    // Check for explicit recovery flag
    bool recoveryFlag = _prefs.getBool("force_recovery", false);
    if (recoveryFlag) {
        _statusMessage = "Recovery mode triggered by recovery flag";
        _prefs.putBool("force_recovery", false); // Clear the flag
        return true;
    }
    
    return false;
}

void RecoveryBootloader::enterRecoveryMode() {
    _bootloaderState = RECOVERY_MODE;
    _statusMessage = "Entering recovery mode";
    
    // We can try to recover from a failed update first
    if (_prefs.getBool("update_failed", false)) {
        _bootloaderState = UPDATE_RECOVERY;
        if (recoverFromFailedUpdate()) {
            _statusMessage = "Recovered from failed update";
        }
    }
}

bool RecoveryBootloader::performFactoryReset() {
    _bootloaderState = FACTORY_RESET;
    _statusMessage = "Factory reset...";
    
    // Clear all preferences
    _prefs.clear();
    
    // Reset all preferences namespaces
    Preferences configPrefs;
    configPrefs.begin("config", false);
    configPrefs.clear();
    configPrefs.end();
    
    Preferences otaPrefs;
    otaPrefs.begin("otaupdate", false);
    otaPrefs.clear();
    otaPrefs.end();
    
    // Remove all config files - simplified to reduce code size
    if (LittleFS.begin()) {
        if (LittleFS.exists("/config")) {
            File root = LittleFS.open("/config");
            if (root && root.isDirectory()) {
                File file = root.openNextFile();
                while (file) {
                    String path = "/config/" + String(file.name());
                    LittleFS.remove(path);
                    file = root.openNextFile();
                }
            }
        }
        
        LittleFS.end();
    }
    
    _statusMessage = "Reset complete";
    delay(1000);
    ESP.restart();
    return true;
}

bool RecoveryBootloader::recoverFromFailedUpdate() {
    _statusMessage = "Attempting to recover from failed update...";
    
    // Try to rollback to the previous firmware
    if (OTAUpdateManager::rollbackFirmware()) {
        _statusMessage = "Recovery successful. Restarting...";
        _prefs.putBool("update_failed", false);
        delay(1000);
        ESP.restart();
        return true;
    } else {
        _statusMessage = "Recovery failed, cannot rollback";
        return false;
    }
}

RecoveryBootloader::BootloaderState RecoveryBootloader::getBootloaderState() {
    return _bootloaderState;
}

void RecoveryBootloader::setBootloaderState(BootloaderState state) {
    _bootloaderState = state;
    
    switch (state) {
        case NORMAL_BOOT:
            _statusMessage = "Normal boot";
            break;
        case RECOVERY_MODE:
            _statusMessage = "Recovery mode";
            break;
        case FACTORY_RESET:
            _statusMessage = "Factory reset";
            break;
        case UPDATE_RECOVERY:
            _statusMessage = "Update recovery";
            break;
    }
}

bool RecoveryBootloader::isRecoveryButtonPressed() {
    // Check if the BOOT button (GPIO0) is held down during startup
    // We check multiple times to ensure it's a deliberate press
    int buttonPressCount = 0;
    for (int i = 0; i < 10; i++) {
        if (digitalRead(BOOT_BUTTON_PIN) == LOW) {
            buttonPressCount++;
        }
        delay(50);
    }
    
    // Return true if button was pressed for at least 8 of 10 checks
    return buttonPressCount >= 8;
}

bool RecoveryBootloader::isInBootLoop() {
    int bootCount = getBootCount();
    unsigned long lastBootTime = getLastBootTime();
    unsigned long currentTime = millis();
    
    // Check if we've rebooted multiple times in a short period
    if (bootCount >= BOOT_COUNT_THRESHOLD) {
        // Only consider it a boot loop if the reboots happened within the timeout window
        if (currentTime - lastBootTime < BOOT_COUNT_TIMEOUT) {
            return true;
        } else {
            // If timeout has passed, reset boot count
            resetBootCount();
        }
    }
    
    return false;
}

void RecoveryBootloader::resetBootCount() {
    _prefs.putInt("boot_count", 0);
    _prefs.putULong("last_boot_time", millis());
}

void RecoveryBootloader::incrementBootCount() {
    int bootCount = getBootCount();
    bootCount++;
    _prefs.putInt("boot_count", bootCount);
    
    // Update last boot time
    setLastBootTime(millis());
}

int RecoveryBootloader::getBootCount() {
    return _prefs.getInt("boot_count", 0);
}

unsigned long RecoveryBootloader::getLastBootTime() {
    return _prefs.getULong("last_boot_time", 0);
}

void RecoveryBootloader::setLastBootTime(unsigned long time) {
    _prefs.putULong("last_boot_time", time);
}

String RecoveryBootloader::getStatusMessage() {
    return _statusMessage;
} 