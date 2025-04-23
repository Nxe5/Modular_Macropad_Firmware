# API Documentation

This document provides a comprehensive overview of the API endpoints available in the Modular Macropad firmware, including their purpose, request/response formats, and examples.

## Overview

The Modular Macropad firmware provides a RESTful API for configuring and controlling the device. The API is implemented using the ESPAsyncWebServer library and is accessible via HTTP requests when the device is connected to a network.

## Base URL

The base URL for all API endpoints is:

```
http://<device-ip-address>
```

Where `<device-ip-address>` is the IP address of the device on the network. If the device is in access point mode, the default IP address is `192.168.4.1`.

## Authentication

Currently, the API does not require authentication. This may be added in future versions for security.

## API Endpoints

### Configuration Endpoints

#### Get Components Configuration

Retrieves the components configuration.

**Endpoint**: `GET /api/config/components`

**Response**: JSON object containing the components configuration.

**Example Response**:
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

#### Get Actions Configuration

Retrieves the actions configuration.

**Endpoint**: `GET /api/config/actions`

**Response**: JSON object containing the actions configuration.

**Example Response**:
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

#### Update Actions Configuration

Updates the actions configuration.

**Endpoint**: `POST /api/config/actions`

**Request Body**: JSON object containing the actions to update.

**Example Request**:
```json
{
  "actions": {
    "layer-config": {
      "button-1": {
        "type": "hid",
        "buttonPress": ["0x00", "0x00", "0x1B", "0x00", "0x00", "0x00", "0x00", "0x00"]
      }
    }
  }
}
```

**Response**: JSON object indicating success or failure.

**Example Response**:
```json
{
  "message": "Actions config updated successfully"
}
```

#### Get Reports Configuration

Retrieves the HID reports configuration.

**Endpoint**: `GET /api/config/reports`

**Response**: JSON object containing the reports configuration.

**Example Response**:
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

#### Get Module Information

Retrieves the module information.

**Endpoint**: `GET /api/config/info`

**Response**: JSON object containing the module information.

**Example Response**:
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

#### Restore Configuration

Restores a configuration to its default values.

**Endpoint**: `POST /api/config/restore`

**Request Parameters**:
- `config`: Name of the configuration to restore (`info`, `components`, `leds`, `actions`, `reports`, `display`)

**Example Request**:
```
POST /api/config/restore?config=actions
```

**Response**: JSON object indicating success or failure.

**Example Response**:
```json
{
  "status": "success",
  "message": "Configuration restored successfully"
}
```

### Macro Endpoints

#### Get All Macros

Retrieves a list of all available macros.

**Endpoint**: `GET /api/macros`

**Response**: JSON object containing a list of macros.

**Example Response**:
```json
[
  {
    "id": "open_iterm_spotlight",
    "name": "Open iTerm via Spotlight",
    "description": "Opens Spotlight and launches iTerm"
  },
  {
    "id": "text_hello",
    "name": "Type Hello World",
    "description": "Types 'Hello, World!' with delays between characters"
  }
]
```

#### Get Macro by ID

Retrieves a specific macro by ID.

**Endpoint**: `GET /api/macros/{id}`

**Parameters**:
- `id`: ID of the macro to retrieve

**Example Request**:
```
GET /api/macros/open_iterm_spotlight
```

**Response**: JSON object containing the macro.

**Example Response**:
```json
{
  "id": "open_iterm_spotlight",
  "name": "Open iTerm via Spotlight",
  "description": "Opens Spotlight and launches iTerm",
  "commands": [
    {
      "type": "key_press",
      "report": ["0x08", "0x00", "0x2C", "0x00", "0x00", "0x00", "0x00", "0x00"]
    },
    {
      "type": "delay",
      "ms": 300
    },
    {
      "type": "type_text",
      "text": "iterm"
    },
    {
      "type": "delay",
      "ms": 200
    },
    {
      "type": "key_press",
      "report": ["0x00", "0x00", "0x28", "0x00", "0x00", "0x00", "0x00", "0x00"]
    }
  ]
}
```

#### Create or Update Macro

Creates a new macro or updates an existing one.

**Endpoint**: `POST /api/macros`

**Request Body**: JSON object containing the macro.

**Example Request**:
```json
{
  "id": "new_macro",
  "name": "New Macro",
  "description": "A new macro",
  "commands": [
    {
      "type": "key_press",
      "report": ["0x00", "0x00", "0x1A", "0x00", "0x00", "0x00", "0x00", "0x00"]
    },
    {
      "type": "delay",
      "ms": 100
    },
    {
      "type": "key_press",
      "report": ["0x00", "0x00", "0x1B", "0x00", "0x00", "0x00", "0x00", "0x00"]
    }
  ]
}
```

**Response**: JSON object indicating success or failure.

**Example Response**:
```json
{
  "status": "success",
  "message": "Macro saved successfully"
}
```

#### Delete Macro

Deletes a macro by ID.

**Endpoint**: `DELETE /api/macros/{id}`

**Parameters**:
- `id`: ID of the macro to delete

**Example Request**:
```
DELETE /api/macros/new_macro
```

**Response**: JSON object indicating success or failure.

**Example Response**:
```json
{
  "status": "success",
  "message": "Macro deleted successfully"
}
```

### System Endpoints

#### Get System Status

Retrieves the current system status.

**Endpoint**: `GET /api/status`

**Response**: JSON object containing the system status.

**Example Response**:
```json
{
  "status": "ok",
  "version": "1.0.0",
  "uptime": 3600,
  "memory": {
    "free": 100000,
    "total": 327680
  },
  "wifi": {
    "connected": true,
    "ssid": "YourWiFiName",
    "rssi": -65
  }
}
```

#### Reboot System

Reboots the system.

**Endpoint**: `POST /api/reboot`

**Response**: JSON object indicating success or failure.

**Example Response**:
```json
{
  "status": "success",
  "message": "System rebooting"
}
```

#### Factory Reset

Resets the system to factory defaults.

**Endpoint**: `POST /api/factory-reset`

**Response**: JSON object indicating success or failure.

**Example Response**:
```json
{
  "status": "success",
  "message": "System reset to factory defaults"
}
```

## Error Responses

When an error occurs, the API returns a JSON object with an error message.

**Example Error Response**:
```json
{
  "error": "Invalid request format"
}
```

Common HTTP status codes:
- `200 OK`: Request successful
- `400 Bad Request`: Invalid request
- `404 Not Found`: Resource not found
- `500 Internal Server Error`: Server error

## CORS Support

The API supports Cross-Origin Resource Sharing (CORS) for web applications. The following headers are included in responses:

- `Access-Control-Allow-Origin: *`
- `Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS`
- `Access-Control-Allow-Headers: Content-Type`

## Example Usage

### Using cURL

Get the components configuration:
```bash
curl -X GET http://192.168.4.1/api/config/components
```

Update the actions configuration:
```bash
curl -X POST -H "Content-Type: application/json" -d '{"actions":{"layer-config":{"button-1":{"type":"hid","buttonPress":["0x00","0x00","0x1B","0x00","0x00","0x00","0x00","0x00"]}}}}' http://192.168.4.1/api/config/actions
```

### Using JavaScript

Get the components configuration:
```javascript
fetch('http://192.168.4.1/api/config/components')
  .then(response => response.json())
  .then(data => console.log(data))
  .catch(error => console.error('Error:', error));
```

Update the actions configuration:
```javascript
fetch('http://192.168.4.1/api/config/actions', {
  method: 'POST',
  headers: {
    'Content-Type': 'application/json',
  },
  body: JSON.stringify({
    actions: {
      'layer-config': {
        'button-1': {
          type: 'hid',
          buttonPress: ['0x00', '0x00', '0x1B', '0x00', '0x00', '0x00', '0x00', '0x00']
        }
      }
    }
  }),
})
  .then(response => response.json())
  .then(data => console.log(data))
  .catch(error => console.error('Error:', error));
```

## Implementation Details

The API is implemented in the `WiFiManager` class, which sets up the web server and defines the API endpoints. The API routes are defined in the `src/api/routes` directory.

### Key Files

- `src/WiFiManager.cpp`: Main implementation of the web server and API endpoints
- `src/api/routes/config.cpp`: Implementation of configuration-related endpoints

### Adding New Endpoints

To add a new API endpoint:

1. Define the endpoint handler function in the appropriate file
2. Register the endpoint in the `setupRoutes()` function in `WiFiManager.cpp`

Example:
```cpp
// Define the handler function
void handleNewEndpoint(AsyncWebServerRequest *request) {
  // Handle the request
  request->send(200, "application/json", "{\"message\":\"New endpoint\"}");
}

// Register the endpoint
void WiFiManager::setupRoutes() {
  // ... existing routes ...
  
  _server.on("/api/new-endpoint", HTTP_GET, handleNewEndpoint);
  
  // ... rest of existing code ...
}
``` 