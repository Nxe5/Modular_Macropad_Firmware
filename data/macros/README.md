# Modular Macropad Macros

This directory contains macros for the Modular Macropad, organized by category. Each macro is designed to enhance productivity and workflow efficiency.

## Blender Macros

### 1. Subdivision Loop (01_subdivision_loop.json)
Quickly adds a Subdivision Surface modifier with optimal settings for smooth modeling. This macro sets up a level 2 viewport subdivision and level 3 render subdivision, which is ideal for creating smooth, high-quality models while maintaining good performance during editing. Perfect for organic modeling and creating smooth surfaces.

**Use Cases:**
- Organic modeling
- Character creation
- Smooth surface modeling
- High-quality renders

### 2. Edge Loop Select (02_edge_loop_select.json)
Instantly selects entire edge loops with a single click. This is essential for efficient modeling as it allows you to quickly select and modify continuous edges around your model. Particularly useful for creating clean topology, adding edge loops for detail, or preparing for operations like beveling or extrusion.

**Use Cases:**
- Topology editing
- Adding edge loops
- Preparing for beveling
- Clean modeling workflow

### 3. Bevel Extrude (03_bevel_extrude.json)
Creates a professional-looking beveled extrusion in one step. This macro combines extrusion and beveling operations with a 0.1 unit bevel amount, which is ideal for creating clean, chamfered edges. Perfect for hard-surface modeling, creating mechanical parts, or adding subtle detail to edges.

**Use Cases:**
- Hard-surface modeling
- Mechanical parts
- Edge detailing
- Professional finish

### 4. Mirror Join (04_mirror_join.json)
Automates the process of setting up mirroring and joining geometry. This macro adds a mirror modifier along the X-axis and joins the mirrored geometry with the original, creating a symmetrical model. Essential for creating symmetrical objects like characters, vehicles, or architectural elements.

**Use Cases:**
- Character modeling
- Vehicle modeling
- Architectural elements
- Symmetrical objects

### 5. Keyframe All Properties (05_keyframe_all.json)
Quickly creates keyframes for all animated properties of selected objects. This macro is a time-saver for animators, automatically setting keyframes for position, rotation, scale, and other animated properties. Essential for character animation, mechanical animations, or any complex motion.

**Use Cases:**
- Character animation
- Mechanical animation
- Complex motion
- Animation workflow

## Utility Macros

### 1. Mouse Jiggler (mouse_jiggler.json)
Prevents screen timeout by moving the mouse cursor in a small pattern. Useful for keeping systems active during long periods of inactivity.

**Features:**
- Small circular mouse movement
- Configurable delay between movements
- Background automation
- System maintenance

### 2. Open iTerm Spotlight (open_iterm_spotlight.json)
Quickly opens iTerm using Spotlight Search. Streamlines terminal access for developers and power users.

**Features:**
- Command + Space shortcut
- Instant terminal access
- Workflow automation
- Developer productivity

## Command Types Reference

The macros use these command types:
- `key_press`: Press and release a key
- `key_down`: Press and hold a key
- `key_up`: Release a held key
- `type_text`: Type a string of text
- `delay`: Wait for specified milliseconds
- `mouse_click`: Click mouse buttons
- `mouse_move`: Move the mouse cursor
- `consumer_press`: Media/consumer control keys

## Macro Structure

Each macro follows this JSON structure:
```json
{
    "id": "unique_macro_id",
    "name": "Human Readable Name",
    "description": "Detailed description of the macro's purpose and use cases",
    "commands": [
        {
            "type": "command_type",
            "parameters": "value"
        }
    ]
}
```

## Best Practices

1. **Naming Conventions:**
   - Use descriptive, unique IDs
   - Include category prefixes
   - Use clear, concise names

2. **Documentation:**
   - Provide detailed descriptions
   - List specific use cases
   - Include any prerequisites

3. **Timing:**
   - Add appropriate delays between actions
   - Consider system response time
   - Test timing on different systems

4. **Testing:**
   - Verify macro behavior
   - Test on target platforms
   - Check edge cases

5. **Maintenance:**
   - Keep macros updated
   - Document changes
   - Version control

## Contributing

When adding new macros:
1. Follow the established naming convention
2. Include comprehensive documentation
3. Test thoroughly
4. Update this README
5. Consider cross-platform compatibility 