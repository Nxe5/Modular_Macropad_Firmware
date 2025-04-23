# Macro System Documentation

This document provides a comprehensive overview of the macro system in the Modular Macropad firmware, including implementation details and how different macro commands are processed.

## Overview

The macro system allows users to create complex sequences of keyboard, mouse, and consumer control actions that can be triggered with a single button press. Macros are stored as JSON files in the `/macros` directory on the device's filesystem.

## Macro Structure

A macro is defined as a JSON object with the following structure:

```json
{
  "id": "unique_macro_id",
  "name": "Display Name",
  "description": "Description of what this macro does",
  "commands": [
    // Array of command objects
  ]
}
```

## Implementation Details

### Key Components

The macro system consists of several key components:

1. **MacroHandler** (`MacroHandler.h`, `MacroHandler.cpp`): Manages macro storage, loading, and execution.
2. **HIDHandler** (`HIDHandler.h`, `HIDHandler.cpp`): Handles the actual HID reports sent to the computer.
3. **KeyHandler** (`KeyHandler.h`, `KeyHandler.cpp`): Detects key presses and triggers macro execution.

### Macro Storage

Macros are stored as individual JSON files in the `/macros` directory on the device's filesystem. Each macro file is named after its ID with a `.json` extension.

The `MacroHandler::loadMacros()` function scans this directory and loads all macro files into memory. The `MacroHandler::saveMacro()` function saves a macro to disk.

### Macro Execution

When a key configured to trigger a macro is pressed, the `KeyHandler::executeAction()` function calls `MacroHandler::executeMacro()` with the macro ID.

The `MacroHandler::executeMacro()` function:
1. Looks up the macro by ID
2. Sets the current macro and command index
3. Marks the macro as executing

The `MacroHandler::update()` function is called in the main loop and:
1. Checks if a macro is currently executing
2. If a delay is active, waits for it to complete
3. Executes the current command
4. Moves to the next command

### Command Execution

The `MacroHandler::executeCommand()` function handles the execution of different command types:

```cpp
void MacroHandler::executeCommand(const MacroCommand& cmd) {
    switch (cmd.type) {
        case MACRO_CMD_KEY_PRESS: {
            if (hidHandler) {
                hidHandler->sendKeyboardReport(cmd.data.keyPress.report);
                delay(50); // Small delay to ensure keypress is registered
                hidHandler->sendEmptyKeyboardReport();
            }
            break;
        }
        // Other command types...
    }
}
```

## Command Types

### 1. Key Press

Simulates pressing and releasing a key on the keyboard.

```json
{
  "type": "key_press",
  "report": ["0x00", "0x00", "0x28", "0x00", "0x00", "0x00", "0x00", "0x00"]
}
```

**Implementation**: The `MACRO_CMD_KEY_PRESS` command sends the HID report to the computer, waits a short delay, and then sends an empty report to release the key.

### 2. Key Down

Simulates pressing a key without releasing it.

```json
{
  "type": "key_down",
  "report": ["0x00", "0x00", "0x28", "0x00", "0x00", "0x00", "0x00", "0x00"]
}
```

**Implementation**: The `MACRO_CMD_KEY_DOWN` command sends the HID report to the computer without releasing the key.

### 3. Key Up

Simulates releasing a previously pressed key.

```json
{
  "type": "key_up",
  "report": ["0x00", "0x00", "0x28", "0x00", "0x00", "0x00", "0x00", "0x00"]
}
```

**Implementation**: The `MACRO_CMD_KEY_UP` command sends an empty HID report to release all keys.

### 4. Consumer Press

Simulates pressing and releasing a consumer control key.

```json
{
  "type": "consumer_press",
  "report": ["0x00", "0x00", "0x00", "0x00"]
}
```

**Implementation**: The `MACRO_CMD_CONSUMER_PRESS` command sends the consumer control report to the computer, waits a short delay, and then sends an empty report to release the key.

### 5. Delay

Pauses execution for a specified amount of time.

```json
{
  "type": "delay",
  "ms": 300
}
```

**Implementation**: The `MACRO_CMD_DELAY` command sets the `delayUntil` variable to the current time plus the specified delay. The `update()` function will not execute the next command until this time has passed.

### 6. Type Text

Types a string of text.

```json
{
  "type": "type_text",
  "text": "Hello, World!"
}
```

**Implementation**: The `MACRO_CMD_TYPE_TEXT` command converts each character in the text to the appropriate HID key code and sends a series of key press and release commands.

### 7. Mouse Move

Moves the mouse cursor by a relative amount.

```json
{
  "type": "mouse_move",
  "x": 10,
  "y": -5
}
```

**Implementation**: The `MACRO_CMD_MOUSE_MOVE` command uses the `USBHIDMouse` class to move the mouse cursor. The speed parameter determines how the movement is applied:
- For slower speeds (1-5), the movement is broken into multiple small steps
- For faster speeds (6-10), the movement is multiplied

### 8. Mouse Click

Simulates clicking a mouse button.

```json
{
  "type": "mouse_button_press",
  "button": 1
}
```

**Implementation**: The `MACRO_CMD_MOUSE_CLICK` command uses the `USBHIDMouse` class to click the specified mouse button.

### 9. Mouse Wheel

Simulates scrolling the mouse wheel.

```json
{
  "type": "mouse_wheel",
  "amount": 1
}
```

**Implementation**: The `MACRO_CMD_MOUSE_SCROLL` command uses the `USBHIDMouse` class to scroll the mouse wheel by the specified amount.

### 10. Execute Macro

Executes another macro as a sub-macro.

```json
{
  "type": "execute_macro",
  "macroId": "another_macro_id"
}
```

**Implementation**: The `MACRO_CMD_EXECUTE_MACRO` command is currently a no-op in the simplified version of the firmware, as nested macro execution is not supported.

## Multiple Key Presses and Releases

The firmware maintains an internal state of which keys are currently pressed using the `HIDHandler::pressedKeys` set. When a key is pressed, it's added to this set, and when a key is released, it's removed.

When multiple keys are pressed simultaneously, the firmware:
1. Tracks each pressed key in the `pressedKeys` set
2. Maintains the modifier state in the `activeModifiers` variable
3. Sends a single HID report that includes all currently pressed keys

When a key release command is executed:
1. The firmware identifies which specific key needs to be released based on the report parameter
2. It updates its internal state by removing that key from the `pressedKeys` set
3. It sends a new HID report to the computer that includes all currently pressed keys (excluding the one that was just released)

The `HIDHandler::updateKeyboardReportFromState()` function is responsible for creating a new HID report based on the current state of pressed keys:

```cpp
bool HIDHandler::updateKeyboardReportFromState() {
    // Create a fresh report
    uint8_t report[HID_KEYBOARD_REPORT_SIZE] = {0};
    
    // Set modifier byte
    report[0] = activeModifiers;
    
    // Add regular keys (up to 6)
    int keyIndex = 2; // First key goes in byte 2 (after modifiers and reserved byte)
    
    for (uint8_t key : pressedKeys) {
        // Skip modifiers as they're handled separately
        if (!HIDHandler::isModifier(key)) {
            // Make sure we don't exceed the report size
            if (keyIndex < HID_KEYBOARD_REPORT_SIZE) {
                report[keyIndex++] = key;
            } else {
                // No more room in the report - this is N-key rollover limitation
                USBSerial.println("Warning: Too many keys pressed, some ignored");
                break;
            }
        }
    }
    
    // Send the updated report
    return sendKeyboardReport(report, HID_KEYBOARD_REPORT_SIZE);
}
```

## API Endpoints

The firmware provides several API endpoints for managing macros:

1. **GET /api/macros**: Returns a list of all available macros with their IDs, names, and descriptions.
2. **GET /api/macros/{id}**: Returns the full content of a specific macro.
3. **POST /api/macros**: Creates or updates a macro.
4. **DELETE /api/macros/{id}**: Deletes a macro.

## Example Macros

### Open iTerm via Spotlight

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

This macro:
1. Presses Command+Space to open Spotlight
2. Waits 300ms
3. Types "iterm"
4. Waits 200ms
5. Presses Enter to launch iTerm

### Type Hello World

```json
{
  "id": "text_hello",
  "name": "Type Hello World",
  "description": "Types 'Hello, World!' with delays between characters",
  "commands": [
    {
      "type": "type_text",
      "text": "H"
    },
    {
      "type": "delay",
      "ms": 100
    },
    {
      "type": "type_text",
      "text": "e"
    },
    {
      "type": "delay",
      "ms": 100
    },
    {
      "type": "type_text",
      "text": "l"
    },
    {
      "type": "delay",
      "ms": 100
    },
    {
      "type": "type_text",
      "text": "l"
    },
    {
      "type": "delay",
      "ms": 100
    },
    {
      "type": "type_text",
      "text": "o"
    },
    {
      "type": "delay",
      "ms": 100
    },
    {
      "type": "type_text",
      "text": ", "
    },
    {
      "type": "delay",
      "ms": 100
    },
    {
      "type": "type_text",
      "text": "W"
    },
    {
      "type": "delay",
      "ms": 100
    },
    {
      "type": "type_text",
      "text": "o"
    },
    {
      "type": "delay",
      "ms": 100
    },
    {
      "type": "type_text",
      "text": "r"
    },
    {
      "type": "delay",
      "ms": 100
    },
    {
      "type": "type_text",
      "text": "l"
    },
    {
      "type": "delay",
      "ms": 100
    },
    {
      "type": "type_text",
      "text": "d"
    },
    {
      "type": "delay",
      "ms": 100
    },
    {
      "type": "type_text",
      "text": "!"
    }
  ]
}
```

This macro types "Hello, World!" with a 100ms delay between each character, creating a typing effect.

## Common Modifier Keys

When creating key press commands, you can use the following modifier values in the first byte of the report:

| Modifier    | Hex Value | Description                 |
| ----------- | --------- | --------------------------- |
| None        | 0x00      | No modifier keys            |
| Ctrl        | 0x01      | Left Control                |
| Shift       | 0x02      | Left Shift                  |
| Alt         | 0x04      | Left Alt                    |
| GUI         | 0x08      | Left GUI (Windows/Command)  |
| Right Ctrl  | 0x10      | Right Control               |
| Right Shift | 0x20      | Right Shift                 |
| Right Alt   | 0x40      | Right Alt                   |
| Right GUI   | 0x80      | Right GUI (Windows/Command) |

## Common Key Codes

Here are some common key codes for reference:

| Key       | Hex Value | Description                  |
| --------- | --------- | ---------------------------- |
| A-Z       | 0x04-0x1D | Letters A through Z          |
| 1-9       | 0x1E-0x26 | Numbers 1 through 9          |
| 0         | 0x27      | Number 0                     |
| Enter     | 0x28      | Enter/Return                 |
| Escape    | 0x29      | Escape                       |
| Backspace | 0x2A      | Backspace                    |
| Tab       | 0x2B      | Tab                          |
| Space     | 0x2C      | Space                        |
| -         | 0x2D      | Minus/Hyphen                 |
| =         | 0x2E      | Equals                       |
| [         | 0x2F      | Left Square Bracket          |
| ]         | 0x30      | Right Square Bracket         |
| \         | 0x31      | Backslash                    |
| ;         | 0x33      | Semicolon                    |
| '         | 0x34      | Quote                        |
| `         | 0x35      | Grave Accent                 |
| ,         | 0x36      | Comma                        |
| .         | 0x37      | Period                       |
| /         | 0x38      | Forward Slash                |
| Caps Lock | 0x39      | Caps Lock                    |
| F1-F12    | 0x3A-0x45 | Function Keys F1 through F12 | 