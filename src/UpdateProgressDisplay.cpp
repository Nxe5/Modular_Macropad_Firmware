#include "UpdateProgressDisplay.h"

// Static member initialization
Adafruit_ST7789* UpdateProgressDisplay::_display = nullptr;
bool UpdateProgressDisplay::_isActive = false;
int UpdateProgressDisplay::_progressPercentage = 0;
String UpdateProgressDisplay::_progressTitle = "";
String UpdateProgressDisplay::_progressMessage = "";
unsigned long UpdateProgressDisplay::_lastUpdateTime = 0;
int UpdateProgressDisplay::_animationFrame = 0;
const char* UpdateProgressDisplay::ANIMATION_FRAMES_CHARS[UpdateProgressDisplay::ANIMATION_FRAMES] = {"|", "/", "-", "\\"};

void UpdateProgressDisplay::begin(Adafruit_ST7789* display) {
    _display = display;
    _isActive = false;
    _progressPercentage = 0;
    _progressTitle = "Firmware Update";
    _progressMessage = "Ready";
    _lastUpdateTime = 0;
}

void UpdateProgressDisplay::updateProgress(size_t current, size_t total, int percentage) {
    _progressPercentage = percentage;
    _progressMessage = String(current / 1024) + "/" + String(total / 1024) + " KB";
    _lastUpdateTime = millis();
    
    // Set display as active if it's not already
    if (!_isActive) {
        setActive(true);
    }
    
    // Update the display
    drawProgressScreen(_progressTitle, _progressPercentage, _progressMessage);
}

void UpdateProgressDisplay::drawProgressScreen(const String& title, int percentage, const String& message) {
    if (_display == nullptr) return;
    
    // Clear the display
    _display->fillScreen(COLOR_BACKGROUND);
    
    // Draw title
    _display->setTextSize(1);
    _display->setTextColor(COLOR_FOREGROUND);
    _display->setCursor(0, TITLE_Y);
    _display->println(title);
    
    // Draw progress bar outline
    _display->drawRect(PROGRESS_BAR_X, PROGRESS_BAR_Y, PROGRESS_BAR_WIDTH, PROGRESS_BAR_HEIGHT, COLOR_FOREGROUND);
    
    // Draw progress bar fill
    int fillWidth = (percentage * PROGRESS_BAR_WIDTH) / 100;
    _display->fillRect(PROGRESS_BAR_X, PROGRESS_BAR_Y, fillWidth, PROGRESS_BAR_HEIGHT, COLOR_HIGHLIGHT);
    
    // Draw percentage text
    _display->setTextSize(1);
    _display->setCursor(PROGRESS_BAR_X + PROGRESS_BAR_WIDTH + 5, PROGRESS_BAR_Y + 8);
    _display->print(String(percentage) + "%");
    
    // Draw status message
    _display->setCursor(0, MESSAGE_Y);
    _display->println(message);
}

void UpdateProgressDisplay::drawErrorScreen(const String& errorMessage) {
    if (_display == nullptr) return;
    
    // Clear the display
    _display->fillScreen(COLOR_BACKGROUND);
    
    // Draw error title
    _display->setTextSize(1);
    _display->setTextColor(COLOR_ERROR);
    _display->setCursor(0, TITLE_Y);
    _display->println("Update Error");
    
    // Draw error icon (X)
    _display->drawRect(PROGRESS_BAR_X + 40, PROGRESS_BAR_Y, 20, 20, COLOR_ERROR);
    _display->drawLine(PROGRESS_BAR_X + 45, PROGRESS_BAR_Y + 5, PROGRESS_BAR_X + 55, PROGRESS_BAR_Y + 15, COLOR_ERROR);
    _display->drawLine(PROGRESS_BAR_X + 45, PROGRESS_BAR_Y + 15, PROGRESS_BAR_X + 55, PROGRESS_BAR_Y + 5, COLOR_ERROR);
    
    // Draw error message
    _display->setTextColor(COLOR_FOREGROUND);
    _display->setCursor(0, MESSAGE_Y);
    
    // Trim message if too long
    String displayMessage = errorMessage;
    if (displayMessage.length() > 20) {
        displayMessage = displayMessage.substring(0, 17) + "...";
    }
    
    _display->println(displayMessage);
}

void UpdateProgressDisplay::drawSuccessScreen(const String& message) {
    if (_display == nullptr) return;
    
    // Clear the display
    _display->fillScreen(COLOR_BACKGROUND);
    
    // Draw success title
    _display->setTextSize(1);
    _display->setTextColor(COLOR_HIGHLIGHT);
    _display->setCursor(0, TITLE_Y);
    _display->println("Update Success");
    
    // Draw checkmark icon
    _display->drawLine(PROGRESS_BAR_X + 40, PROGRESS_BAR_Y + 10, PROGRESS_BAR_X + 45, PROGRESS_BAR_Y + 15, COLOR_HIGHLIGHT);
    _display->drawLine(PROGRESS_BAR_X + 45, PROGRESS_BAR_Y + 15, PROGRESS_BAR_X + 55, PROGRESS_BAR_Y + 5, COLOR_HIGHLIGHT);
    
    // Draw message
    _display->setTextColor(COLOR_FOREGROUND);
    _display->setCursor(0, MESSAGE_Y);
    
    // Trim message if too long
    String displayMessage = message;
    if (displayMessage.length() > 20) {
        displayMessage = displayMessage.substring(0, 17) + "...";
    }
    
    _display->println(displayMessage);
}

void UpdateProgressDisplay::drawRecoveryScreen(const String& message) {
    if (_display == nullptr) return;
    
    // Clear the display
    _display->fillScreen(COLOR_BACKGROUND);
    
    // Draw recovery title
    _display->setTextSize(1);
    _display->setTextColor(COLOR_FOREGROUND);
    _display->setCursor(0, TITLE_Y);
    _display->println("Recovery Mode");
    
    // Draw recovery symbol (lifebuoy)
    int centerX = PROGRESS_BAR_X + 50;
    int centerY = PROGRESS_BAR_Y + 10;
    _display->drawCircle(centerX, centerY, 10, COLOR_FOREGROUND);
    _display->drawCircle(centerX, centerY, 5, COLOR_FOREGROUND);
    
    // Draw message
    _display->setCursor(0, MESSAGE_Y);
    
    // Trim message if too long
    String displayMessage = message;
    if (displayMessage.length() > 20) {
        displayMessage = displayMessage.substring(0, 17) + "...";
    }
    
    _display->println(displayMessage);
}

void UpdateProgressDisplay::process() {
    // Simple implementation to reduce code size
    // No animation to reduce complexity
    if (!_isActive || _display == nullptr) return;
}

bool UpdateProgressDisplay::isActive() {
    return _isActive;
}

void UpdateProgressDisplay::setActive(bool active) {
    _isActive = active;
    _lastUpdateTime = millis();
} 