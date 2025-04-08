# Example Macros

This directory contains example macros demonstrating various capabilities of the Modular Macropad. Each macro is designed to showcase different features and use cases.

## Basic Macros

### 1. text_hello.json
A simple macro that types "Hello, World!" with a typing effect. Demonstrates:
- Basic text typing
- Using delays between characters
- Simple command sequencing

### 2. open_spotlight_app.json
Opens Spotlight Search and launches a specified application. Demonstrates:
- Key combinations (Command + Space)
- Text typing
- Enter key press
- Practical workflow automation

### 3. copy_paste_with_format.json
Copies text and pastes it with specific formatting. Demonstrates:
- Multiple key combinations
- Command sequencing
- Common keyboard shortcuts

## Advanced Macros

### 4. mouse_jiggler.json
Moves the mouse cursor in a small pattern to prevent screen timeout. Demonstrates:
- Mouse movement commands
- Repeating actions
- Background automation

### 5. media_control.json
Controls media playback with various commands. Demonstrates:
- Consumer control commands (media keys)
- Multiple actions in sequence
- System-wide controls

### 6. window_manager.json
Manages window positions and workspaces. Demonstrates:
- Complex key combinations
- Multiple command sequences
- System window management

## Gaming Macros

### 7. gaming_combo.json
Executes a series of keystrokes for gaming combos. Demonstrates:
- Precise timing with delays
- Multiple key presses
- Gaming-specific use case

## Development Macros

### 8. git_commands.json
Common Git commands in one macro. Demonstrates:
- Text typing for commands
- Multiple steps in development workflow
- Command-line automation

## Command Types Reference

The macros demonstrate these command types:
- `key_press`: Press and release a key
- `key_down`: Press and hold a key
- `key_up`: Release a held key
- `type_text`: Type a string of text
- `delay`: Wait for specified milliseconds
- `consumer_press`: Media/consumer control keys
- `mouse_move`: Move the mouse cursor
- `mouse_click`: Click mouse buttons
- `mouse_scroll`: Scroll the mouse wheel
- `repeat_start`/`repeat_end`: Repeat a sequence of commands

## Tips for Creating Macros

1. Use descriptive IDs and names
2. Add appropriate delays between actions
3. Test macros thoroughly before regular use
4. Consider platform compatibility
5. Use comments to document complex sequences

## Example Macro Structure

```json
{
    "id": "example_macro",
    "name": "Example Macro",
    "description": "Demonstrates basic macro functionality",
    "commands": [
        {
            "type": "type_text",
            "text": "Hello"
        },
        {
            "type": "delay",
            "ms": 100
        },
        {
            "type": "key_press",
            "report": ["0x00", "0x00", "0x28", "0x00", "0x00", "0x00", "0x00", "0x00"]
        }
    ]
}
``` 