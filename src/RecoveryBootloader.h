#ifndef RECOVERY_BOOTLOADER_H
#define RECOVERY_BOOTLOADER_H

#include <Arduino.h>
#include <Preferences.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>

class RecoveryBootloader {
public:
    // Bootloader states
    enum BootloaderState {
        NORMAL_BOOT,
        RECOVERY_MODE,
        FACTORY_RESET,
        UPDATE_RECOVERY
    };
    
    // Initialize the recovery bootloader
    static void begin();
    
    // Check if system should boot into recovery mode
    static bool shouldEnterRecoveryMode();
    
    // Enter recovery mode
    static void enterRecoveryMode();
    
    // Perform factory reset
    static bool performFactoryReset();
    
    // Perform recovery from failed update
    static bool recoverFromFailedUpdate();
    
    // Get the current bootloader state
    static BootloaderState getBootloaderState();
    
    // Set the bootloader state
    static void setBootloaderState(BootloaderState state);
    
    // Check if physical recovery button is pressed
    static bool isRecoveryButtonPressed();
    
    // Check boot count to detect boot loops
    static bool isInBootLoop();
    
    // Reset boot count
    static void resetBootCount();
    
    // Get status message
    static String getStatusMessage();

private:
    // Configuration
    static const int BOOT_BUTTON_PIN = 0;     // Boot button pin (GPIO0)
    static const int BOOT_COUNT_THRESHOLD = 3; // Number of rapid reboots to trigger recovery
    static const int BOOT_COUNT_TIMEOUT = 10000; // Time in ms to consider consecutive boots
    
    // State variables
    static BootloaderState _bootloaderState;
    static Preferences _prefs;
    static String _statusMessage;
    
    // Helper methods
    static void incrementBootCount();
    static int getBootCount();
    static unsigned long getLastBootTime();
    static void setLastBootTime(unsigned long time);
};

#endif // RECOVERY_BOOTLOADER_H 