#ifndef VERSION_H
#define VERSION_H

// Version information - Update these values for each release
#define FIRMWARE_VERSION_MAJOR 1
#define FIRMWARE_VERSION_MINOR 0
#define FIRMWARE_VERSION_PATCH 0

// Build number - This will be automatically incremented by PlatformIO
#ifndef FIRMWARE_BUILD_NUMBER
#define FIRMWARE_BUILD_NUMBER 1
#endif

// Version string - Automatically generated from major.minor.patch
#define FIRMWARE_VERSION_STRING STRINGIFY(FIRMWARE_VERSION_MAJOR) "." \
                               STRINGIFY(FIRMWARE_VERSION_MINOR) "." \
                               STRINGIFY(FIRMWARE_VERSION_PATCH)

// Build date and time (automatically updated during build)
#define FIRMWARE_BUILD_DATE __DATE__
#define FIRMWARE_BUILD_TIME __TIME__

// Device information
#define DEVICE_NAME "Modular Macropad"
#define DEVICE_MANUFACTURER "Nxe5"
#define DEVICE_MODEL "MM-1"

// Helper macro for stringification
#define STRINGIFY(x) #x
#define STRINGIFY2(x) STRINGIFY(x)

// Full version string including build number
#define FIRMWARE_FULL_VERSION FIRMWARE_VERSION_STRING "." STRINGIFY2(FIRMWARE_BUILD_NUMBER)

#endif // VERSION_H 