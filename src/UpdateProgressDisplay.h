#ifndef UPDATE_PROGRESS_DISPLAY_H
#define UPDATE_PROGRESS_DISPLAY_H

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include "OTAUpdateManager.h"

class UpdateProgressDisplay {
public:
    // Initialize the display
    static void begin(Adafruit_ST7789* display);
    
    // Update the progress display
    static void updateProgress(size_t current, size_t total, int percentage);
    
    // Draw progress screen
    static void drawProgressScreen(const String& title, int percentage, const String& message);
    
    // Draw error screen
    static void drawErrorScreen(const String& errorMessage);
    
    // Draw success screen
    static void drawSuccessScreen(const String& message);
    
    // Draw recovery mode screen
    static void drawRecoveryScreen(const String& message);
    
    // Process update display (call this in the main loop)
    static void process();
    
    // Check if update display is active
    static bool isActive();
    
    // Set whether the display is active
    static void setActive(bool active);

private:
    // Display reference
    static Adafruit_ST7789* _display;
    
    // State variables
    static bool _isActive;
    static int _progressPercentage;
    static String _progressTitle;
    static String _progressMessage;
    static unsigned long _lastUpdateTime;
    
    // Animation states
    static int _animationFrame;
    static const int ANIMATION_FRAMES = 4;
    static const char* ANIMATION_FRAMES_CHARS[ANIMATION_FRAMES];
    
    // UI parameters
    static const int PROGRESS_BAR_HEIGHT = 10;
    static const int PROGRESS_BAR_WIDTH = 100;
    static const int PROGRESS_BAR_X = 14;
    static const int PROGRESS_BAR_Y = 30;
    static const int PROGRESS_TEXT_Y = 45;
    static const int TITLE_Y = 10;
    static const int MESSAGE_Y = 55;
    
    // Colors
    static const uint16_t COLOR_BACKGROUND = 0x0000; // Black
    static const uint16_t COLOR_FOREGROUND = 0xFFFF; // White
    static const uint16_t COLOR_HIGHLIGHT = 0x07E0;  // Green
    static const uint16_t COLOR_ERROR = 0xF800;      // Red
};

#endif // UPDATE_PROGRESS_DISPLAY_H 