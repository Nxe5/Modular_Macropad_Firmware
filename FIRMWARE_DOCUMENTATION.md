# Modular Macropad Firmware Documentation

## Table of Contents
1. [Overview](#overview)
2. [Architecture](#architecture)
3. [C++ vs JavaScript Concepts](#cpp-vs-javascript-concepts)
4. [Core Components](#core-components)
5. [Configuration System](#configuration-system)
6. [Hardware Interfaces](#hardware-interfaces)
7. [API Endpoints](#api-endpoints)
8. [Memory Management](#memory-management)
9. [Async Operations](#async-operations)

## Overview

This documentation explains the firmware code for the Modular Macropad, written in C++ for the ESP32-S3 microcontroller. The code is structured similarly to a Node.js application but with key differences due to hardware constraints and C++ paradigms.

### Key Differences from JavaScript
- **Memory Management**: Manual memory management vs JavaScript's garbage collection
- **Concurrency**: FreeRTOS tasks vs JavaScript's event loop
- **File System**: SPIFFS vs Node.js fs module
- **Type System**: Static typing vs JavaScript's dynamic typing

## Architecture

### Main Components
1. **ModuleSetup**: Handles hardware initialization (similar to a JavaScript constructor)
2. **ConfigManager**: Manages JSON configuration files (similar to a config service)
3. **KeyHandler**: Processes keyboard inputs (similar to an event listener)
4. **LEDHandler**: Controls LED outputs (similar to a display service)
5. **EncoderHandler**: Manages rotary encoders (similar to an input handler)
6. **HIDHandler**: Handles USB HID communication (similar to a USB service)
7. **DisplayHandler**: Controls the OLED display (similar to a display service)
8. **MacroHandler**: Processes macro commands (similar to a command processor)
9. **WiFiManager**: Manages WiFi connectivity (similar to a network service)

### File Structure
```
src/
├── main.cpp           # Application entry point (like index.js)
├── ModuleSetup.*      # Hardware initialization
├── ConfigManager.*    # Configuration management
├── KeyHandler.*       # Keyboard input handling
├── LEDHandler.*       # LED control
├── EncoderHandler.*   # Encoder input handling
├── HIDHandler.*       # USB HID communication
├── DisplayHandler.*   # Display control
├── MacroHandler.*     # Macro processing
└── WiFiManager.*      # WiFi management
```

## C++ vs JavaScript Concepts

### Classes and Objects
```cpp
// C++ Class Declaration
class KeyHandler {
private:
    uint8_t pin;  // Private member (like private fields in JS)
public:
    KeyHandler(uint8_t pin);  // Constructor
    void begin();            // Public method
};
```

```javascript
// JavaScript Equivalent
class KeyHandler {
    #pin;  // Private field
    constructor(pin) {
        this.#pin = pin;
    }
    begin() {
        // Implementation
    }
}
```

### Memory Management
```cpp
// C++ Memory Management
class ConfigManager {
private:
    StaticJsonDocument<2048> doc;  // Stack allocation
    char* buffer;                   // Heap allocation
public:
    ConfigManager() {
        buffer = new char[1024];    // Manual memory allocation
    }
    ~ConfigManager() {
        delete[] buffer;            // Manual memory deallocation
    }
};
```

```javascript
// JavaScript Memory Management
class ConfigManager {
    constructor() {
        this.buffer = new Array(1024);  // Automatic memory management
    }
}
```

### Async Operations
```cpp
// C++ FreeRTOS Task
void keyboardTask(void *pvParameters) {
    while (true) {
        // Process keyboard inputs
        vTaskDelay(pdMS_TO_TICKS(10));  // Non-blocking delay
    }
}
```

```javascript
// JavaScript Async Operation
async function keyboardTask() {
    while (true) {
        // Process keyboard inputs
        await new Promise(resolve => setTimeout(resolve, 10));
    }
}
```

## Core Components

### ModuleSetup
Handles hardware initialization and pin configuration.

```cpp
// C++ Implementation
void ModuleSetup::begin() {
    // Initialize hardware
    configurePinModes(rowPins, colPins, rows, cols);
    initializeKeyHandler();
    initializeEncoderHandler();
}
```

```javascript
// JavaScript Equivalent
class ModuleSetup {
    async begin() {
        await this.configurePinModes(rowPins, colPins, rows, cols);
        await this.initializeKeyHandler();
        await this.initializeEncoderHandler();
    }
}
```

### ConfigManager
Manages JSON configuration files using ArduinoJson library.

```cpp
// C++ Implementation
bool ConfigManager::loadConfig(const char* filename) {
    File file = SPIFFS.open(filename, "r");
    if (!file) return false;
    
    StaticJsonDocument<2048> doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    
    return !error;
}
```

```javascript
// JavaScript Equivalent
class ConfigManager {
    async loadConfig(filename) {
        try {
            const file = await fs.readFile(filename, 'utf8');
            return JSON.parse(file);
        } catch (error) {
            return false;
        }
    }
}
```

## Configuration System

The firmware uses JSON configuration files stored in SPIFFS (SPI Flash File System):

1. **actions.json**: Defines key bindings and macros
2. **components.json**: Defines hardware components
3. **info.json**: Contains system information
4. **reports.json**: Defines HID report codes

### Example Configuration
```json
{
  "actions": {
    "layer-config": {
      "button-1": {
        "type": "hid",
        "buttonPress": ["0x00", "0x00", "0x1A", "0x00", "0x00", "0x00", "0x00", "0x00"]
      }
    }
  }
}
```

## Hardware Interfaces

### GPIO Pins
```cpp
// C++ GPIO Configuration
void configurePinModes(uint8_t* rowPins, uint8_t* colPins, uint8_t rows, uint8_t cols) {
    for (uint8_t i = 0; i < rows; i++) {
        pinMode(rowPins[i], INPUT_PULLUP);
    }
    for (uint8_t i = 0; i < cols; i++) {
        pinMode(colPins[i], OUTPUT);
        digitalWrite(colPins[i], HIGH);
    }
}
```

```javascript
// JavaScript Equivalent (using Node.js GPIO)
class GPIO {
    async configurePinModes(rowPins, colPins, rows, cols) {
        for (let i = 0; i < rows; i++) {
            await gpio.setup(rowPins[i], gpio.DIR_IN, gpio.EDGE_BOTH);
        }
        for (let i = 0; i < cols; i++) {
            await gpio.setup(colPins[i], gpio.DIR_OUT);
            await gpio.write(colPins[i], 1);
        }
    }
}
```

## API Endpoints

The firmware provides a REST API for configuration:

1. **GET /api/config/actions**: Get current actions configuration
2. **POST /api/config/actions**: Update actions configuration
3. **GET /api/config/components**: Get components configuration
4. **GET /api/config/info**: Get system information
5. **GET /api/config/reports**: Get HID report definitions

### Example API Usage
```cpp
// C++ API Handler
void handleGetActionsConfig(AsyncWebServerRequest *request) {
    File file = SPIFFS.open("/data/config/actions.json", "r");
    String content = file.readString();
    file.close();
    request->send(200, "application/json", content);
}
```

```javascript
// JavaScript API Handler
async function handleGetActionsConfig(req, res) {
    try {
        const content = await fs.readFile('/data/config/actions.json', 'utf8');
        res.json(JSON.parse(content));
    } catch (error) {
        res.status(500).json({ error: 'Failed to read config' });
    }
}
```

## Memory Management

### Stack vs Heap
```cpp
// C++ Memory Allocation
class MemoryExample {
private:
    StaticJsonDocument<2048> stackDoc;  // Stack allocation
    char* heapBuffer;                    // Heap allocation
public:
    MemoryExample() {
        heapBuffer = new char[1024];     // Heap allocation
    }
    ~MemoryExample() {
        delete[] heapBuffer;             // Heap deallocation
    }
};
```

### Memory Constraints
- ESP32-S3 has limited RAM (512KB)
- Stack size is limited
- Heap fragmentation must be managed

## Async Operations

### FreeRTOS Tasks
```cpp
// C++ Task Creation
void createTasks() {
    xTaskCreate(
        keyboardTask,    // Task function
        "Keyboard",      // Task name
        4096,           // Stack size
        NULL,           // Parameters
        1,              // Priority
        NULL            // Task handle
    );
}
```

```javascript
// JavaScript Worker Thread
const { Worker } = require('worker_threads');

function createWorker() {
    const worker = new Worker('./keyboardTask.js');
    worker.on('message', (data) => {
        // Handle keyboard events
    });
}
```

### Event Handling
```cpp
// C++ Event Handling
void KeyHandler::handleKeyPress() {
    if (digitalRead(pin) == LOW) {
        // Handle key press
        notifyObservers(KEY_PRESSED);
    }
}
```

```javascript
// JavaScript Event Handling
class KeyHandler {
    handleKeyPress() {
        if (this.pin.read() === 0) {
            this.emit('keyPressed');
        }
    }
}
```

## Best Practices

1. **Memory Management**
   - Use stack allocation when possible
   - Minimize heap allocations
   - Clean up resources properly

2. **Error Handling**
   - Check return values
   - Use try-catch blocks
   - Log errors appropriately

3. **Task Management**
   - Keep tasks small and focused
   - Use appropriate stack sizes
   - Monitor task priorities

4. **Configuration**
   - Validate JSON before saving
   - Handle file system errors
   - Keep configurations small

## Debugging

### Serial Debugging
```cpp
// C++ Serial Debug
void debugPrint(const char* message) {
    Serial.println(message);
}
```

### LED Debugging
```cpp
// C++ LED Debug
void debugLED(uint8_t pattern) {
    digitalWrite(LED_PIN, pattern);
}
```

## Common Issues and Solutions

1. **Memory Issues**
   - Symptom: Crashes or unexpected behavior
   - Solution: Check stack size and heap allocations

2. **WiFi Connection**
   - Symptom: Failed to connect
   - Solution: Check credentials and network settings

3. **Configuration Loading**
   - Symptom: Invalid JSON
   - Solution: Validate JSON before saving

## Future Improvements

1. **Web Interface**
   - Add WebSocket support
   - Implement real-time updates
   - Add configuration validation

2. **Performance**
   - Optimize memory usage
   - Improve task scheduling
   - Add power management

3. **Features**
   - Add more macro types
   - Support multiple layers
   - Add LED effects 