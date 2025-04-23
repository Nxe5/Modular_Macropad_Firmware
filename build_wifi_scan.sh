#!/bin/bash
# Script to build and upload the Modular Macropad firmware with WiFi scanning support

set -e  # Exit on any error

# Display banner
echo "======================================================"
echo "  Modular Macropad Firmware Build & Upload Script"
echo "  WiFi Scanning and Dual Mode Support"
echo "======================================================"

# Check if PlatformIO is installed
if ! command -v pio &> /dev/null; then
    echo "Error: PlatformIO is not installed or not in PATH"
    echo "Please install PlatformIO: https://platformio.org/install"
    exit 1
fi

# Check if a device is connected
echo "Checking for connected ESP32 device..."
if ! pio device list | grep -q "CP210"; then
    echo "Warning: No ESP32 device detected. Please connect your macropad."
    echo "Continue anyway? [y/N]"
    read continue_anyway
    if [[ ! "$continue_anyway" =~ ^[Yy]$ ]]; then
        echo "Build canceled."
        exit 0
    fi
fi

# Build the firmware
echo "Building firmware..."
cd Modular_Macropad_Firmware
pio run

# Upload the firmware if a device is connected
if pio device list | grep -q "CP210"; then
    echo "Uploading firmware..."
    pio run --target upload
    
    echo "Uploading filesystem..."
    pio run --target uploadfs
    
    echo "Firmware upload complete!"
else
    echo "No device connected for upload."
    echo "Firmware build complete. Use the following command to upload when device is connected:"
    echo "  cd Modular_Macropad_Firmware && pio run --target upload && pio run --target uploadfs"
fi

echo "Done!" 