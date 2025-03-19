# Modular MacroPad Firmware

An ESP32-based firmware for modular mechanical keyboard macropads with RGB LEDs. This firmware provides customizable key mappings, LED effects, and a web-based configuration interface.

## Features

- Support for modular macropad designs
- Customizable key mappings for multiple layers
- RGB LED support with various effects and per-key configuration
- Encoder support for rotary input
- Web-based configuration interface
- WiFi connectivity in both AP and Station modes

## Hardware Requirements

- ESP32-S3 microcontroller
- SK6812 or WS2812B RGB LEDs
- Mechanical switches
- Rotary encoders (optional)

## Installation

1. Clone this repository
2. Open the project in PlatformIO or Arduino IDE
3. Adjust configuration for your specific hardware in `src/ModuleSetup.h`
4. Build and upload the firmware
5. Upload web interface files to SPIFFS (see below)

### Uploading Web Interface Files

In PlatformIO:
```
pio run --target uploadfs
```

In Arduino IDE:
- Use the "ESP32 Sketch Data Upload" tool

## Web Interface

The firmware includes a web-based configuration interface accessible via WiFi:

1. Power on your MacroPad
2. Connect to the "MacroPad" WiFi network (default password: macropad123)
3. Open a web browser and navigate to http://192.168.4.1
4. Use the interface to configure your MacroPad

The web interface allows you to:
- Configure LED colors and effects
- Map keys to actions
- Set up WiFi connectivity
- Update firmware

## Configuration

The firmware uses JSON configuration files stored in the ESP32's SPIFFS file system:

- `/config/LEDs.json` - LED configuration
- `/config/keys.json` - Key mapping configuration
- `/config/wifi.json` - WiFi settings

These files can be edited directly through the web interface.

## API Documentation

The firmware provides a REST API and WebSocket interface for configuration. Details can be found in the [web interface README](data/web/README.md).

## Power Management

The firmware includes power management features to optimize LED brightness based on USB power availability, ensuring stable operation even with many LEDs.

## Dependencies

- ESP32 Arduino Core
- Adafruit NeoPixel
- ArduinoJson
- ESPAsyncWebServer
- AsyncTCP
- Various other libraries (see platformio.ini)

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request. 