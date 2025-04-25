# Firmware Update System Documentation

## Overview

The Modular Macropad firmware update system provides a robust mechanism for over-the-air (OTA) updates with recovery capabilities. This document outlines the components, processes, and features of the firmware update system.

## Key Components

### 1. OTA Update Manager

The `OTAUpdateManager` class is responsible for:
- Checking for firmware updates from GitHub releases
- Downloading and installing updates
- Verifying update integrity through MD5 checksums
- Managing update states and progress reporting
- Handling rollback in case of failed updates

### 2. Recovery Bootloader

The `RecoveryBootloader` class provides:
- Safe mode boot sequence
- Detection of boot loops and failed updates
- Recovery from interrupted or corrupted updates
- Factory reset capabilities
- Boot integrity verification

### 3. Partition Verifier

The `PartitionVerifier` class ensures:
- Verification of bootloader and OTA partitions
- Calculation of partition hashes for integrity checks
- Validation of partition magic bytes and CRC
- Partition information reporting for diagnostics

### 4. Update Progress Display

The `UpdateProgressDisplay` class provides:
- Visual feedback during the update process
- Progress bar and percentage display
- Error and success screens
- Recovery mode indication

## Update Process

1. **Update Check**:
   - The system periodically checks for updates from GitHub
   - Compares available version with current version
   - Reports if updates are available

2. **Update Initiation**:
   - Updates can be initiated manually or automatically
   - Current state is saved for recovery purposes

3. **Download & Install**:
   - Firmware binary is downloaded from GitHub
   - MD5 checksum is calculated during download
   - Progress is reported visually and via logs
   - Integrity is verified before applying

4. **Verification & Boot**:
   - After installation, the ESP32 restarts
   - Boot integrity is verified
   - If successful, normal operation resumes
   - If failed, recovery mode is entered

## Recovery Process

1. **Recovery Detection**:
   - System detects failed updates during boot
   - Also detects boot loops and manual recovery triggers

2. **Recovery Mode**:
   - Limited functionality focused on recovery
   - Visual indication on display
   - WiFi remains active for remote recovery

3. **Rollback**:
   - System attempts to rollback to the previous firmware
   - Uses ESP32's dual OTA partition scheme
   - If successful, normal operation resumes
   - If failed, remains in recovery mode for manual intervention

## Partition Scheme

The firmware uses a custom partition scheme with dual OTA slots:

```
# Name,    Type,   SubType,    Offset,     Size,       Flags
nvs,       data,   nvs,        0x9000,     0x5000,
otadata,   data,   ota,        0xe000,     0x2000,
app0,      app,    ota_0,      0x10000,    0x140000,
app1,      app,    ota_1,      0x150000,   0x140000,
coredump,  data,   coredump,   0x290000,   0x10000,
spiffs,    data,   spiffs,     0x2A0000,   0x160000,
```

This scheme provides:
- Two application partitions for OTA updates and rollback
- Sufficient space for each firmware version
- Coredump partition for crash analysis
- SPIFFS partition for configuration and data storage

## Error Handling

The system handles various error conditions:
- Network connectivity issues during download
- Interrupted downloads or power loss
- Corrupted firmware files
- Verification failures
- Boot failures

Each error is logged and recovery procedures are initiated automatically.

## Manual Recovery

If automatic recovery fails, manual recovery can be initiated by:
1. Holding the BOOT button during power-up to enter recovery mode
2. Connecting via USB and using the serial monitor to diagnose issues
3. Using the web interface (if accessible) to upload a new firmware

## Future Enhancements

Planned improvements to the update system:
1. Incremental updates to reduce download size
2. Compressed firmware updates
3. Enhanced validation with digital signatures
4. Remote diagnostics during recovery
5. Update scheduling for controlled deployment 