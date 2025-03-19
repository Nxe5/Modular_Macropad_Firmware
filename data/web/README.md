# MacroPad Web Interface

This folder contains the web interface for configuring the MacroPad firmware. The interface is served directly from the ESP32 using its built-in WiFi capabilities.

## Features

- **Dashboard**: View device status and perform system actions
- **LED Configuration**: Configure LED colors, brightness, and behavior
- **Button Mapping**: Map buttons to LEDs and actions
- **WiFi Settings**: Configure WiFi connection settings

## Technical Details

The web interface uses:
- HTML5
- CSS3
- Vanilla JavaScript
- WebSockets for real-time communication

## Usage

1. Power on your MacroPad
2. Connect to the "MacroPad" WiFi network (default password: macropad123)
3. Open a web browser and navigate to http://192.168.4.1
4. Use the interface to configure your MacroPad

## API Endpoints

The ESP32 provides the following REST API endpoints:

- `GET /api/status` - Get current device status
- `GET /api/config/led` - Get LED configuration
- `POST /api/config/led` - Update LED configuration
- `GET /api/config/wifi` - Get WiFi configuration
- `POST /api/config/wifi` - Update WiFi configuration
- `POST /api/reset` - Reset to defaults

## WebSocket API

The WebSocket endpoint is at `/ws` and supports the following commands:

- `update_led` - Update an LED's color
- `save_config` - Save current LED configuration

## File Structure

- `index.html` - Main entry point for the web interface
- `style.css` - Additional styling for LED controls
- `README.md` - This file

## Customization

To customize the web interface, edit the files in this directory and upload them to the ESP32 using the data upload feature in PlatformIO or Arduino IDE.

## Troubleshooting

If you cannot connect to the web interface:
1. Ensure your MacroPad is powered on
2. Check that you're connected to the "MacroPad" WiFi network
3. Try accessing http://192.168.4.1 in your browser
4. If still not working, reset the device and try again 