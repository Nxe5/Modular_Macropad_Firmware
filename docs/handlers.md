# Handler Documentation

This document provides a comprehensive overview of the handlers used in the Modular Macropad firmware, including their purpose, functionality, and implementation details.

## HIDHandler

The `HIDHandler` class manages all Human Interface Device (HID) communications between the Modular Macropad and the host computer. It handles keyboard, consumer, and mouse reports, as well as macro execution.

### Key Features

- Manages keyboard, consumer, and mouse HID reports
- Handles key press and release events
- Supports modifier keys (Shift, Ctrl, Alt, etc.)
- Manages macro execution
- Provides methods for sending HID reports to the host

### Class Structure

```cpp
class HIDHandler {
public:
  // Constructor and initialization
  HIDHandler();
  void begin();
  
  // Keyboard methods
  void pressKey(uint8_t keycode, uint8_t modifier = 0);
  void releaseKey(uint8_t keycode, uint8_t modifier = 0);
  void pressModifier(uint8_t modifier);
  void releaseModifier(uint8_t modifier);
  
  // Consumer methods
  void pressConsumer(uint16_t usage_code);
  void releaseConsumer(uint16_t usage_code);
  
  // Mouse methods
  void moveMouse(int8_t x, int8_t y);
  void clickMouse(uint8_t buttons);
  void releaseMouse(uint8_t buttons);
  
  // Macro methods
  void executeMacro(const Macro& macro);
  void stopMacro();
  
  // Report management
  void sendKeyboardReport();
  void sendConsumerReport();
  void sendMouseReport();
  
private:
  // Internal state
  uint8_t _keyboardReport[8];
  uint16_t _consumerReport;
  uint8_t _mouseReport[3];
  std::set<uint8_t> _pressedKeys;
  bool _macroRunning;
  Macro _currentMacro;
  size_t _currentCommandIndex;
  
  // Helper methods
  void updateKeyboardReport();
  void updateConsumerReport();
  void updateMouseReport();
  void processMacroCommand(const MacroCommand& command);
};
```

### Key Methods

#### Keyboard Methods

- `pressKey(uint8_t keycode, uint8_t modifier = 0)`: Presses a key with an optional modifier
- `releaseKey(uint8_t keycode, uint8_t modifier = 0)`: Releases a key with an optional modifier
- `pressModifier(uint8_t modifier)`: Presses a modifier key
- `releaseModifier(uint8_t modifier)`: Releases a modifier key

#### Consumer Methods

- `pressConsumer(uint16_t usage_code)`: Presses a consumer control (e.g., volume up, play/pause)
- `releaseConsumer(uint16_t usage_code)`: Releases a consumer control

#### Mouse Methods

- `moveMouse(int8_t x, int8_t y)`: Moves the mouse cursor
- `clickMouse(uint8_t buttons)`: Clicks mouse buttons
- `releaseMouse(uint8_t buttons)`: Releases mouse buttons

#### Macro Methods

- `executeMacro(const Macro& macro)`: Executes a macro
- `stopMacro()`: Stops the currently running macro

#### Report Management

- `sendKeyboardReport()`: Sends the current keyboard report to the host
- `sendConsumerReport()`: Sends the current consumer report to the host
- `sendMouseReport()`: Sends the current mouse report to the host

### Implementation Details

#### Report Structure

The HIDHandler manages three types of reports:

1. Keyboard Report (8 bytes):
   ```
   Byte 0: Modifier keys (Shift, Ctrl, Alt, etc.)
   Byte 1: Reserved (always 0)
   Bytes 2-7: Key codes (up to 6 keys)
   ```

2. Consumer Report (2 bytes):
   ```
   Bytes 0-1: Consumer usage code (e.g., volume up, play/pause)
   ```

3. Mouse Report (3 bytes):
   ```
   Byte 0: Button states
   Byte 1: X movement
   Byte 2: Y movement
   ```

#### Key Press and Release

When a key is pressed:
1. The key code is added to the pressed keys set
2. The keyboard report is updated
3. The report is sent to the host

When a key is released:
1. The key code is removed from the pressed keys set
2. The keyboard report is updated
3. The report is sent to the host

#### Macro Execution

When a macro is executed:
1. The macro is stored in the `_currentMacro` variable
2. The `_macroRunning` flag is set to true
3. The `_currentCommandIndex` is initialized to 0
4. Each command in the macro is processed in sequence
5. Delays between commands are handled using the Arduino `delay()` function
6. When all commands are processed, the macro is stopped

### Example Usage

```cpp
// Initialize the HID handler
HIDHandler hid;
hid.begin();

// Press and release a key
hid.pressKey(0x04);  // 'a' key
delay(100);
hid.releaseKey(0x04);

// Press a key with a modifier
hid.pressKey(0x04, 0x02);  // Shift + 'a'
delay(100);
hid.releaseKey(0x04, 0x02);

// Execute a macro
Macro macro = {
  .id = "test_macro",
  .name = "Test Macro",
  .description = "A test macro",
  .commands = {
    {
      .type = "key_press",
      .report = {0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00}
    },
    {
      .type = "delay",
      .ms = 100
    },
    {
      .type = "key_press",
      .report = {0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00}
    }
  }
};
hid.executeMacro(macro);
```

## KeyHandler

The `KeyHandler` class manages the physical keys and encoders on the Modular Macropad. It handles key events, debouncing, and action execution.

### Key Features

- Manages physical keys and encoders
- Handles key press and release events
- Implements debouncing for keys
- Executes actions based on key events
- Supports multiple layers

### Class Structure

```cpp
class KeyHandler {
public:
  // Constructor and initialization
  KeyHandler();
  void begin();
  
  // Key event handling
  void handleKeyEvent(uint8_t keyId, bool pressed);
  void handleEncoderEvent(uint8_t encoderId, bool clockwise);
  
  // Layer management
  void setLayer(const String& layerName);
  String getCurrentLayer() const;
  
  // Action execution
  void executeAction(const ActionConfig& action);
  
private:
  // Internal state
  std::map<uint8_t, bool> _keyStates;
  std::map<uint8_t, unsigned long> _lastDebounceTime;
  String _currentLayer;
  
  // Helper methods
  bool isDebounced(uint8_t keyId);
  void updateKeyState(uint8_t keyId, bool pressed);
};
```

### Key Methods

#### Key Event Handling

- `handleKeyEvent(uint8_t keyId, bool pressed)`: Handles a key press or release event
- `handleEncoderEvent(uint8_t encoderId, bool clockwise)`: Handles an encoder turn event

#### Layer Management

- `setLayer(const String& layerName)`: Sets the current layer
- `getCurrentLayer() const`: Gets the current layer name

#### Action Execution

- `executeAction(const ActionConfig& action)`: Executes an action based on a key event

### Implementation Details

#### Debouncing

The KeyHandler implements debouncing to prevent false triggers from key bounce:

1. When a key event is received, the current time is compared to the last debounce time
2. If the time difference is less than the debounce time (default: 50ms), the event is ignored
3. Otherwise, the key state is updated and the action is executed

#### Layer Management

The KeyHandler supports multiple layers, allowing different actions to be assigned to the same key:

1. Each layer has a unique name
2. Actions are associated with a specific layer
3. When a key event occurs, the action for the current layer is executed

#### Action Execution

When a key event occurs:
1. The key state is updated
2. If the key is pressed, the action for the current layer is executed
3. If the key is released, any necessary cleanup is performed

### Example Usage

```cpp
// Initialize the key handler
KeyHandler keyHandler;
keyHandler.begin();

// Set the current layer
keyHandler.setLayer("default");

// Handle a key press event
keyHandler.handleKeyEvent(1, true);  // Key 1 pressed

// Handle a key release event
keyHandler.handleKeyEvent(1, false);  // Key 1 released

// Handle an encoder event
keyHandler.handleEncoderEvent(1, true);  // Encoder 1 turned clockwise
```

## MacroHandler

The `MacroHandler` class manages macros in the Modular Macropad firmware. It handles loading, saving, and executing macros.

### Key Features

- Loads and saves macros from/to files
- Executes macros
- Manages macro state
- Provides methods for macro manipulation

### Class Structure

```cpp
class MacroHandler {
public:
  // Constructor and initialization
  MacroHandler();
  void begin();
  
  // Macro management
  bool loadMacro(const String& id, Macro& macro);
  bool saveMacro(const Macro& macro);
  bool deleteMacro(const String& id);
  
  // Macro execution
  void executeMacro(const String& id);
  void stopMacro();
  
  // Macro listing
  std::vector<Macro> listMacros();
  
private:
  // Internal state
  std::map<String, Macro> _macros;
  bool _macroRunning;
  String _currentMacroId;
  
  // Helper methods
  bool parseMacroFromJson(const JsonDocument& doc, Macro& macro);
  void saveMacroToJson(const Macro& macro, JsonDocument& doc);
};
```

### Key Methods

#### Macro Management

- `loadMacro(const String& id, Macro& macro)`: Loads a macro from a file
- `saveMacro(const Macro& macro)`: Saves a macro to a file
- `deleteMacro(const String& id)`: Deletes a macro file

#### Macro Execution

- `executeMacro(const String& id)`: Executes a macro by ID
- `stopMacro()`: Stops the currently running macro

#### Macro Listing

- `listMacros()`: Returns a list of all available macros

### Implementation Details

#### Macro Storage

Macros are stored as JSON files in the LittleFS filesystem:

1. Each macro has a unique ID
2. Macro files are stored in the `/macros` directory
3. File names are in the format `{id}.json`

#### Macro Execution

When a macro is executed:
1. The macro is loaded from the file
2. The macro is passed to the HIDHandler for execution
3. The macro state is tracked until execution is complete

#### Macro File Format

Macro files are stored in JSON format:

```json
{
  "id": "macro_id",
  "name": "Macro Name",
  "description": "Macro Description",
  "commands": [
    {
      "type": "key_press",
      "report": ["0x00", "0x00", "0x04", "0x00", "0x00", "0x00", "0x00", "0x00"]
    },
    {
      "type": "delay",
      "ms": 100
    },
    {
      "type": "key_press",
      "report": ["0x00", "0x00", "0x05", "0x00", "0x00", "0x00", "0x00", "0x00"]
    }
  ]
}
```

### Example Usage

```cpp
// Initialize the macro handler
MacroHandler macroHandler;
macroHandler.begin();

// Load a macro
Macro macro;
if (macroHandler.loadMacro("test_macro", macro)) {
  // Macro loaded successfully
}

// Save a macro
macro.id = "new_macro";
macro.name = "New Macro";
macro.description = "A new macro";
macro.commands = {
  {
    .type = "key_press",
    .report = {0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00}
  },
  {
    .type = "delay",
    .ms = 100
  },
  {
    .type = "key_press",
    .report = {0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00}
  }
};
macroHandler.saveMacro(macro);

// Execute a macro
macroHandler.executeMacro("test_macro");

// List all macros
std::vector<Macro> macros = macroHandler.listMacros();
for (const auto& macro : macros) {
  Serial.println(macro.name);
}
```

## LEDHandler

The `LEDHandler` class manages the LEDs on the Modular Macropad. It handles LED control, animations, and effects.

### Key Features

- Controls individual LEDs
- Manages LED animations
- Handles LED effects
- Supports LED patterns
- Provides methods for LED manipulation

### Class Structure

```cpp
class LEDHandler {
public:
  // Constructor and initialization
  LEDHandler();
  void begin();
  
  // LED control
  void setLED(uint8_t ledId, uint8_t red, uint8_t green, uint8_t blue);
  void setLED(uint8_t ledId, uint32_t color);
  void setAllLEDs(uint32_t color);
  
  // LED animations
  void startAnimation(uint8_t ledId, const Animation& animation);
  void stopAnimation(uint8_t ledId);
  
  // LED effects
  void setEffect(uint8_t ledId, const Effect& effect);
  void clearEffect(uint8_t ledId);
  
  // LED patterns
  void setPattern(const Pattern& pattern);
  void clearPattern();
  
  // LED brightness
  void setBrightness(uint8_t brightness);
  uint8_t getBrightness() const;
  
private:
  // Internal state
  std::map<uint8_t, uint32_t> _ledStates;
  std::map<uint8_t, Animation> _animations;
  std::map<uint8_t, Effect> _effects;
  Pattern _currentPattern;
  uint8_t _brightness;
  
  // Helper methods
  void updateLED(uint8_t ledId);
  void updateAnimations();
  void updateEffects();
  void updatePattern();
};
```

### Key Methods

#### LED Control

- `setLED(uint8_t ledId, uint8_t red, uint8_t green, uint8_t blue)`: Sets an LED to a specific color
- `setLED(uint8_t ledId, uint32_t color)`: Sets an LED to a specific color (packed RGB)
- `setAllLEDs(uint32_t color)`: Sets all LEDs to a specific color

#### LED Animations

- `startAnimation(uint8_t ledId, const Animation& animation)`: Starts an animation on an LED
- `stopAnimation(uint8_t ledId)`: Stops an animation on an LED

#### LED Effects

- `setEffect(uint8_t ledId, const Effect& effect)`: Sets an effect on an LED
- `clearEffect(uint8_t ledId)`: Clears an effect on an LED

#### LED Patterns

- `setPattern(const Pattern& pattern)`: Sets a pattern for all LEDs
- `clearPattern()`: Clears the current pattern

#### LED Brightness

- `setBrightness(uint8_t brightness)`: Sets the global LED brightness
- `getBrightness() const`: Gets the current LED brightness

### Implementation Details

#### LED Control

The LEDHandler uses the FastLED library to control the LEDs:

1. Each LED has a unique ID
2. LED colors are stored as 32-bit RGB values
3. The brightness is applied globally to all LEDs

#### LED Animations

Animations are implemented as state machines:

1. Each animation has a type, duration, and parameters
2. Animations are updated in the main loop
3. When an animation completes, it is automatically stopped

#### LED Effects

Effects are special animations that can be applied to LEDs:

1. Each effect has a type and parameters
2. Effects are updated in the main loop
3. Effects can be cleared at any time

#### LED Patterns

Patterns are predefined arrangements of colors for all LEDs:

1. Each pattern has a name and a list of colors
2. Patterns are applied to all LEDs at once
3. Patterns can be cleared at any time

### Example Usage

```cpp
// Initialize the LED handler
LEDHandler ledHandler;
ledHandler.begin();

// Set an LED to a specific color
ledHandler.setLED(1, 255, 0, 0);  // Red
ledHandler.setLED(2, 0, 255, 0);  // Green
ledHandler.setLED(3, 0, 0, 255);  // Blue

// Set all LEDs to a specific color
ledHandler.setAllLEDs(0xFFFFFF);  // White

// Start an animation
Animation animation = {
  .type = "fade",
  .duration = 1000,
  .parameters = {
    {"start_color", 0xFF0000},
    {"end_color", 0x00FF00}
  }
};
ledHandler.startAnimation(1, animation);

// Set an effect
Effect effect = {
  .type = "blink",
  .parameters = {
    {"color", 0xFF0000},
    {"interval", 500}
  }
};
ledHandler.setEffect(1, effect);

// Set a pattern
Pattern pattern = {
  .name = "rainbow",
  .colors = {
    0xFF0000, 0xFF7F00, 0xFFFF00,
    0x00FF00, 0x0000FF, 0x4B0082,
    0x9400D3
  }
};
ledHandler.setPattern(pattern);

// Set the brightness
ledHandler.setBrightness(128);  // 50% brightness
```

## DisplayHandler

The `DisplayHandler` class manages the display on the Modular Macropad. It handles display control, text rendering, and graphics.

### Key Features

- Controls the display
- Renders text and graphics
- Manages display layouts
- Handles display updates
- Provides methods for display manipulation

### Class Structure

```cpp
class DisplayHandler {
public:
  // Constructor and initialization
  DisplayHandler();
  void begin();
  
  // Display control
  void clear();
  void update();
  
  // Text rendering
  void drawText(const String& text, int x, int y, uint8_t size = 1);
  void drawTextCentered(const String& text, int y, uint8_t size = 1);
  
  // Graphics
  void drawPixel(int x, int y, uint16_t color);
  void drawLine(int x0, int y0, int x1, int y1, uint16_t color);
  void drawRect(int x, int y, int w, int h, uint16_t color);
  void fillRect(int x, int y, int w, int h, uint16_t color);
  
  // Display layouts
  void setLayout(const Layout& layout);
  void clearLayout();
  
  // Display brightness
  void setBrightness(uint8_t brightness);
  uint8_t getBrightness() const;
  
private:
  // Internal state
  Adafruit_SSD1306 _display;
  Layout _currentLayout;
  uint8_t _brightness;
  
  // Helper methods
  void updateLayout();
  void renderLayout();
};
```

### Key Methods

#### Display Control

- `clear()`: Clears the display
- `update()`: Updates the display with the current content

#### Text Rendering

- `drawText(const String& text, int x, int y, uint8_t size = 1)`: Draws text at a specific position
- `drawTextCentered(const String& text, int y, uint8_t size = 1)`: Draws centered text at a specific y-position

#### Graphics

- `drawPixel(int x, int y, uint16_t color)`: Draws a pixel at a specific position
- `drawLine(int x0, int y0, int x1, int y1, uint16_t color)`: Draws a line between two points
- `drawRect(int x, int y, int w, int h, uint16_t color)`: Draws a rectangle
- `fillRect(int x, int y, int w, int h, uint16_t color)`: Fills a rectangle

#### Display Layouts

- `setLayout(const Layout& layout)`: Sets the current display layout
- `clearLayout()`: Clears the current layout

#### Display Brightness

- `setBrightness(uint8_t brightness)`: Sets the display brightness
- `getBrightness() const`: Gets the current display brightness

### Implementation Details

#### Display Control

The DisplayHandler uses the Adafruit_SSD1306 library to control the display:

1. The display is initialized with the appropriate settings
2. The display is updated in the main loop
3. The brightness is applied globally to the display

#### Text Rendering

Text rendering is handled by the Adafruit_GFX library:

1. Text can be drawn at specific positions
2. Text can be centered horizontally
3. Text size can be adjusted

#### Graphics

Graphics are drawn using the Adafruit_GFX library:

1. Basic shapes can be drawn (pixels, lines, rectangles)
2. Shapes can be filled or outlined
3. Colors are specified as 16-bit RGB values

#### Display Layouts

Layouts are predefined arrangements of elements for the display:

1. Each layout has a name and a list of elements
2. Elements can be text, graphics, or custom drawings
3. Layouts are rendered in the main loop

### Example Usage

```cpp
// Initialize the display handler
DisplayHandler displayHandler;
displayHandler.begin();

// Clear the display
displayHandler.clear();

// Draw text
displayHandler.drawText("Hello, World!", 0, 0);
displayHandler.drawTextCentered("Centered Text", 16);

// Draw graphics
displayHandler.drawPixel(64, 32, 0xFFFF);  // White pixel
displayHandler.drawLine(0, 0, 127, 63, 0xFFFF);  // White line
displayHandler.drawRect(10, 10, 20, 20, 0xFFFF);  // White rectangle
displayHandler.fillRect(40, 40, 20, 20, 0xFFFF);  // Filled white rectangle

// Set a layout
Layout layout = {
  .name = "default",
  .elements = {
    {
      .type = "text",
      .text = "Hello, World!",
      .x = 0,
      .y = 0,
      .size = 1
    },
    {
      .type = "graphics",
      .shape = "rect",
      .x = 10,
      .y = 10,
      .w = 20,
      .h = 20,
      .color = 0xFFFF
    }
  }
};
displayHandler.setLayout(layout);

// Set the brightness
displayHandler.setBrightness(128);  // 50% brightness

// Update the display
displayHandler.update();
```

## WiFiHandler

The `WiFiHandler` class manages the WiFi connectivity of the Modular Macropad. It handles WiFi connections, access point mode, and web server functionality.

### Key Features

- Manages WiFi connections
- Handles access point mode
- Provides web server functionality
- Manages WiFi configuration
- Handles OTA updates

### Class Structure

```cpp
class WiFiHandler {
public:
  // Constructor and initialization
  WiFiHandler();
  void begin();
  
  // WiFi connection
  bool connect(const String& ssid, const String& password);
  void disconnect();
  bool isConnected() const;
  
  // Access point mode
  void startAP(const String& ssid, const String& password = "");
  void stopAP();
  bool isAPMode() const;
  
  // Web server
  void startServer();
  void stopServer();
  bool isServerRunning() const;
  
  // WiFi configuration
  bool loadConfig();
  bool saveConfig();
  void resetConfig();
  
  // OTA updates
  bool startOTA();
  void stopOTA();
  bool isOTAInProgress() const;
  
private:
  // Internal state
  WiFiManager _wifiManager;
  AsyncWebServer _server;
  bool _apMode;
  bool _serverRunning;
  bool _otaInProgress;
  
  // Helper methods
  void setupRoutes();
  void handleNotFound(AsyncWebServerRequest *request);
  void handleCORS(AsyncWebServerRequest *request);
};
```

### Key Methods

#### WiFi Connection

- `connect(const String& ssid, const String& password)`: Connects to a WiFi network
- `disconnect()`: Disconnects from the current WiFi network
- `isConnected() const`: Checks if connected to a WiFi network

#### Access Point Mode

- `startAP(const String& ssid, const String& password = "")`: Starts access point mode
- `stopAP()`: Stops access point mode
- `isAPMode() const`: Checks if in access point mode

#### Web Server

- `startServer()`: Starts the web server
- `stopServer()`: Stops the web server
- `isServerRunning() const`: Checks if the web server is running

#### WiFi Configuration

- `loadConfig()`: Loads the WiFi configuration from a file
- `saveConfig()`: Saves the WiFi configuration to a file
- `resetConfig()`: Resets the WiFi configuration to defaults

#### OTA Updates

- `startOTA()`: Starts an OTA update
- `stopOTA()`: Stops an OTA update
- `isOTAInProgress() const`: Checks if an OTA update is in progress

### Implementation Details

#### WiFi Connection

The WiFiHandler uses the ESP8266WiFi library to manage WiFi connections:

1. WiFi credentials are stored in a configuration file
2. The device attempts to connect to the configured network on startup
3. If the connection fails, the device enters access point mode

#### Access Point Mode

Access point mode is implemented using the ESP8266WiFi library:

1. The device creates a WiFi access point with the configured SSID
2. The default IP address is 192.168.4.1
3. The web server is started to allow configuration

#### Web Server

The web server is implemented using the ESPAsyncWebServer library:

1. The server provides a RESTful API for configuration
2. CORS is enabled for web applications
3. Routes are defined for various endpoints

#### WiFi Configuration

WiFi configuration is stored in a JSON file:

```json
{
  "ssid": "YourWiFiName",
  "password": "YourWiFiPassword",
  "ap_mode": false,
  "ap_ssid": "ModularMacropad",
  "ap_password": ""
}
```

#### OTA Updates

OTA updates are implemented using the ESP8266HTTPUpdate library:

1. Updates are downloaded from a configured URL
2. The device reboots after a successful update
3. Update progress is reported via the web server

### Example Usage

```cpp
// Initialize the WiFi handler
WiFiHandler wifiHandler;
wifiHandler.begin();

// Connect to a WiFi network
if (wifiHandler.connect("YourWiFiName", "YourWiFiPassword")) {
  Serial.println("Connected to WiFi");
} else {
  Serial.println("Failed to connect to WiFi");
  // Start access point mode
  wifiHandler.startAP("ModularMacropad");
}

// Start the web server
wifiHandler.startServer();

// Load the WiFi configuration
if (wifiHandler.loadConfig()) {
  Serial.println("WiFi configuration loaded");
} else {
  Serial.println("Failed to load WiFi configuration");
}

// Start an OTA update
if (wifiHandler.startOTA()) {
  Serial.println("OTA update started");
} else {
  Serial.println("Failed to start OTA update");
}
```

## ConfigHandler

The `ConfigHandler` class manages the configuration of the Modular Macropad. It handles loading, saving, and updating configuration files.

### Key Features

- Manages configuration files
- Handles configuration updates
- Provides default configurations
- Validates configuration data
- Manages configuration backups

### Class Structure

```cpp
class ConfigHandler {
public:
  // Constructor and initialization
  ConfigHandler();
  void begin();
  
  // Configuration management
  bool loadConfig(const String& name, JsonDocument& doc);
  bool saveConfig(const String& name, const JsonDocument& doc);
  bool deleteConfig(const String& name);
  
  // Default configurations
  bool restoreDefault(const String& name);
  bool backupConfig(const String& name);
  bool restoreBackup(const String& name);
  
  // Configuration validation
  bool validateConfig(const String& name, const JsonDocument& doc);
  
  // Configuration listing
  std::vector<String> listConfigs();
  
private:
  // Internal state
  std::map<String, JsonDocument> _configs;
  std::map<String, JsonDocument> _backups;
  
  // Helper methods
  bool parseConfigFromJson(const String& name, const JsonDocument& doc);
  void saveConfigToJson(const String& name, const JsonDocument& doc);
  bool loadDefaultConfig(const String& name, JsonDocument& doc);
};
```

### Key Methods

#### Configuration Management

- `loadConfig(const String& name, JsonDocument& doc)`: Loads a configuration from a file
- `saveConfig(const String& name, const JsonDocument& doc)`: Saves a configuration to a file
- `deleteConfig(const String& name)`: Deletes a configuration file

#### Default Configurations

- `restoreDefault(const String& name)`: Restores a configuration to its default values
- `backupConfig(const String& name)`: Creates a backup of a configuration
- `restoreBackup(const String& name)`: Restores a configuration from a backup

#### Configuration Validation

- `validateConfig(const String& name, const JsonDocument& doc)`: Validates a configuration

#### Configuration Listing

- `listConfigs()`: Returns a list of all available configurations

### Implementation Details

#### Configuration Storage

Configurations are stored as JSON files in the LittleFS filesystem:

1. Each configuration has a unique name
2. Configuration files are stored in the `/config` directory
3. File names are in the format `{name}.json`

#### Default Configurations

Default configurations are stored in the `/config/defaults` directory:

1. Default configurations are used to restore settings to a known good state
2. Default configurations are loaded when a configuration is missing or invalid
3. Default configurations can be restored manually

#### Configuration Validation

Configurations are validated against a schema:

1. Each configuration type has a specific schema
2. Schemas are defined in JSON format
3. Validation is performed when loading or saving configurations

#### Configuration Backups

Configuration backups are stored in the `/config/backups` directory:

1. Backups are created automatically when a configuration is modified
2. Backups can be restored manually
3. Backups are named with a timestamp

### Example Usage

```cpp
// Initialize the config handler
ConfigHandler configHandler;
configHandler.begin();

// Load a configuration
JsonDocument doc;
if (configHandler.loadConfig("components", doc)) {
  // Configuration loaded successfully
}

// Save a configuration
doc["components"][0]["id"] = "button-1";
doc["components"][0]["type"] = "button";
doc["components"][0]["size"]["rows"] = 1;
doc["components"][0]["size"]["columns"] = 1;
doc["components"][0]["start_location"]["row"] = 0;
doc["components"][0]["start_location"]["column"] = 0;
configHandler.saveConfig("components", doc);

// Restore a default configuration
configHandler.restoreDefault("components");

// Create a backup
configHandler.backupConfig("components");

// Restore a backup
configHandler.restoreBackup("components");

// List all configurations
std::vector<String> configs = configHandler.listConfigs();
for (const auto& config : configs) {
  Serial.println(config);
}
```

## Conclusion

This document provides a comprehensive overview of the handlers used in the Modular Macropad firmware. Each handler is responsible for a specific aspect of the device's functionality, and they work together to provide a complete solution for the Modular Macropad.

The handlers are designed to be modular and extensible, allowing for easy addition of new features and functionality. They follow a consistent pattern of initialization, configuration, and operation, making them easy to understand and use.

For more information on specific handlers or their implementation, please refer to the source code and comments in the respective files. 