# Configuration Files Documentation

This document provides a comprehensive overview of the configuration files used in the Modular Macropad firmware, including their structure, purpose, and how they are loaded and processed.

## Overview

The Modular Macropad firmware uses JSON configuration files stored in the device's filesystem (LittleFS) to define the hardware components, key actions, LED configurations, and other system settings. These files are located in the `/config` directory.

## Configuration File Types

### 1. Components Configuration (`components.json`)

Defines the physical layout and properties of hardware components in the macropad.

**Location**: `/config/components.json`

**Structure**:
```json
{
  "components": [
    {
      "id": "button-1",
      "type": "button",
      "size": {
        "rows": 1,
        "columns": 1
      },
      "start_location": {
        "row": 0,
        "column": 0
      }
    },
    {
      "id": "encoder-1",
      "type": "encoder",
      "size": {
        "rows": 1,
        "columns": 1
      },
      "start_location": {
        "row": 0,
        "column": 1
      },
      "with_button": true
    }
  ]
}
```

**Fields**:
- `id`: Unique identifier for the component
- `type`: Component type (`button`, `encoder`, etc.)
- `size`: Physical size of the component
  - `rows`: Number of rows the component occupies
  - `columns`: Number of columns the component occupies
- `start_location`: Position of the component in the grid
  - `row`: Starting row (0-based)
  - `column`: Starting column (0-based)
- `with_button`: (Optional) For encoders, indicates if the encoder has a built-in button

**Loading**: The `ConfigManager::loadComponents()` function reads and parses this file, returning a vector of `Component` structures.

### 2. Actions Configuration (`actions.json`)

Defines the actions performed when keys are pressed or encoders are turned.

**Location**: `/config/actions.json`

**Structure**:
```json
{
  "actions": {
    "layer-config": {
      "button-1": {
        "type": "hid",
        "buttonPress": ["0x00", "0x00", "0x1A", "0x00", "0x00", "0x00", "0x00", "0x00"]
      },
      "encoder-1": {
        "type": "encoder",
        "clockwise": ["0x00", "0x00", "0x52", "0x00", "0x00", "0x00", "0x00", "0x00"],
        "counterclockwise": ["0x00", "0x00", "0x4F", "0x00", "0x00", "0x00", "0x00", "0x00"]
      }
    }
  }
}
```

**Fields**:
- `actions.layer-config`: Contains configurations for each component
  - Component ID (e.g., `button-1`): Configuration for that component
    - `type`: Action type (`hid`, `multimedia`, `macro`, `layer`)
    - `buttonPress`: HID report for button press (for buttons)
    - `clockwise`: HID report for clockwise rotation (for encoders)
    - `counterclockwise`: HID report for counterclockwise rotation (for encoders)
    - `macroId`: ID of the macro to execute (for macro actions)
    - `targetLayer`: Name of the layer to switch to (for layer actions)

**Loading**: The `ConfigManager::loadActions()` function reads and parses this file, returning a map of component IDs to `ActionConfig` structures.

### 3. LED Configuration (`leds.json`)

Defines the LED behavior for each component.

**Location**: `/config/leds.json`

**Structure**:
```json
{
  "leds": [
    {
      "id": "button-1",
      "type": "rgb",
      "default_color": "#FF0000",
      "pressed_color": "#00FF00",
      "animation": "breathing",
      "animation_speed": 100
    },
    {
      "id": "encoder-1",
      "type": "rgb",
      "default_color": "#0000FF",
      "pressed_color": "#FFFF00",
      "animation": "none"
    }
  ]
}
```

**Fields**:
- `leds`: Array of LED configurations
  - `id`: Component ID this LED belongs to
  - `type`: LED type (`rgb`, `monochrome`)
  - `default_color`: Default color (hex format)
  - `pressed_color`: Color when the associated button is pressed
  - `animation`: Animation type (`none`, `breathing`, `rainbow`, etc.)
  - `animation_speed`: Animation speed in milliseconds

**Loading**: The `LEDHandler::loadLEDConfig()` function reads and parses this file, configuring the LED behavior for each component.

### 4. Reports Configuration (`reports.json`)

Defines HID report codes for various keys and actions.

**Location**: `/config/reports.json`

**Structure**:
```json
{
  "reports": {
    "keyboard": {
      "a": ["0x00", "0x00", "0x04", "0x00", "0x00", "0x00", "0x00", "0x00"],
      "b": ["0x00", "0x00", "0x05", "0x00", "0x00", "0x00", "0x00", "0x00"],
      "enter": ["0x00", "0x00", "0x28", "0x00", "0x00", "0x00", "0x00", "0x00"]
    },
    "consumer": {
      "play_pause": ["0x00", "0x00", "0xCD", "0x00"],
      "volume_up": ["0x00", "0x00", "0xE9", "0x00"],
      "volume_down": ["0x00", "0x00", "0xEA", "0x00"]
    }
  }
}
```

**Fields**:
- `reports.keyboard`: Keyboard HID reports
  - Key name: HID report for that key
- `reports.consumer`: Consumer control HID reports
  - Control name: HID report for that control

**Loading**: The `HIDHandler` uses this file to look up HID reports for keys and consumer controls.

### 5. Module Information (`info.json`)

Contains general information about the module.

**Location**: `/config/info.json`

**Structure**:
```json
{
  "name": "Modular Macropad",
  "version": "1.0.0",
  "author": "Your Name",
  "description": "A modular macropad with customizable keys and encoders",
  "module-size": "4x4",
  "gridSize": {
    "rows": 4,
    "columns": 4
  },
  "defaults": {
    "layer": "default",
    "brightness": 50
  },
  "settings": {
    "debounce_time": 50,
    "polling_rate": 1000
  },
  "supportedComponentTypes": ["button", "encoder", "slider", "display"]
}
```

**Fields**:
- `name`: Module name
- `version`: Firmware version
- `author`: Author name
- `description`: Module description
- `module-size`: Physical size of the module
- `gridSize`: Grid dimensions
  - `rows`: Number of rows
  - `columns`: Number of columns
- `defaults`: Default settings
  - `layer`: Default layer name
  - `brightness`: Default LED brightness
- `settings`: System settings
  - `debounce_time`: Key debounce time in milliseconds
  - `polling_rate`: Key polling rate in milliseconds
- `supportedComponentTypes`: List of supported component types

**Loading**: The `ModuleSetup::loadModuleInfo()` function reads and parses this file, initializing the module information.

### 6. WiFi Configuration (`wifi.json`)

Stores WiFi connection settings.

**Location**: `/config/wifi.json`

**Structure**:
```json
{
  "ssid": "YourWiFiName",
  "password": "YourWiFiPassword",
  "ap_mode": false
}
```

**Fields**:
- `ssid`: WiFi network name
- `password`: WiFi password
- `ap_mode`: Whether to operate in access point mode

**Loading**: The `WiFiManager::loadWiFiConfig()` function reads and parses this file, configuring the WiFi connection.

## Default Configurations

Default configurations are stored in the `/config/defaults/` directory. These files serve as templates and can be used to restore the system to a known good state.

**Location**: `/config/defaults/`

**Files**:
- `components.json`: Default component configuration
- `actions.json`: Default actions configuration
- `leds.json`: Default LED configuration
- `reports.json`: Default reports configuration
- `info.json`: Default module information
- `wifi.json`: Default WiFi configuration

## Configuration Management

The `ConfigManager` class provides methods for loading and parsing configuration files:

- `loadComponents()`: Loads and parses the components configuration
- `loadActions()`: Loads and parses the actions configuration
- `readFile()`: Reads a file from the filesystem

The `WiFiManager` class provides methods for managing WiFi configuration:

- `loadWiFiConfig()`: Loads WiFi configuration
- `saveWiFiConfig()`: Saves WiFi configuration
- `resetToDefaults()`: Resets all configurations to defaults

## API Endpoints

The firmware provides several API endpoints for managing configurations:

- `GET /api/config/components`: Get components configuration
- `GET /api/config/actions`: Get actions configuration
- `POST /api/config/actions`: Update actions configuration
- `GET /api/config/reports`: Get reports configuration
- `GET /api/config/info`: Get module information
- `POST /api/config/restore`: Restore a configuration to defaults

## Example: Creating a Custom Configuration

To create a custom configuration for your macropad:

1. Define the physical layout in `components.json`
2. Configure key actions in `actions.json`
3. Set LED behavior in `leds.json`
4. Update module information in `info.json`

You can use the web interface or API endpoints to update these configurations, or manually edit the JSON files and upload them to the device. 