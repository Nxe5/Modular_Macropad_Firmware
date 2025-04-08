# Blender Macros Documentation

This document provides detailed information about each Blender macro and how to use them effectively.

## Modeling Macros

### 1. Subdivision Loop (01_subdivision_loop.json)
**Description**: Adds a subdivision surface modifier and sets up optimal settings
- Adds a Subdivision Surface modifier
- Sets viewport subdivisions to 2
- Sets render subdivisions to 3
- Enables optimal display settings

### 2. Edge Loop Select (02_edge_loop_select.json)
**Description**: Selects edge loops for quick modeling operations
- Uses Alt+Click functionality
- Selects connected edge loops
- Useful for quick edge selection and manipulation

### 3. Bevel Extrude (03_bevel_extrude.json)
**Description**: Creates a beveled extrusion with optimal settings
- Extrudes selected faces
- Adds a bevel modifier
- Sets bevel width to 0.1
- Enables smooth shading

### 4. Mirror Join (04_mirror_join.json)
**Description**: Sets up mirror modifier and joins geometry
- Adds mirror modifier
- Sets mirror axis to X
- Enables clipping
- Joins mirrored geometry

## Animation Macros

### 5. Keyframe All Properties (05_keyframe_all_properties.json)
**Description**: Creates keyframes for all animated properties
- Inserts keyframes for location, rotation, and scale
- Sets keyframe interpolation to Bezier
- Useful for quick animation setup

### 6. Add Drivers (06_add_drivers.json)
**Description**: Sets up common driver relationships
- Adds drivers for common properties
- Sets up expression drivers
- Useful for automated animations

### 7. Bake Animation (07_bake_animation.json)
**Description**: Bakes animation data to keyframes
- Bakes all animated properties
- Removes drivers after baking
- Useful for finalizing animations

### 8. Pose Library (08_pose_library.json)
**Description**: Manages pose library entries
- Adds current pose to library
- Sets up pose library UI
- Useful for character animation

### 9. Add Shape Key (09_add_shape_key.json)
**Description**: Creates and manages shape keys
- Adds basis shape key
- Creates new shape key
- Sets up shape key UI

## Mode and View Macros

### 10. Toggle Edit Mode (10_toggle_edit_mode.json)
**Description**: Switches between object and edit mode
- Toggles between modes
- Preserves selection
- Useful for quick mode switching

### 11. Add Modifier (11_add_modifier.json)
**Description**: Adds common modifiers with optimal settings
- Adds modifier menu
- Sets up modifier UI
- Useful for quick modifier addition

### 12. Toggle Wireframe (12_toggle_wireframe.json)
**Description**: Switches between solid and wireframe view
- Toggles wireframe display
- Maintains view settings
- Useful for modeling visualization

### 13. Add Keyframe (13_add_keyframe.json)
**Description**: Inserts keyframe at current frame
- Adds keyframe for selected properties
- Uses default interpolation
- Useful for quick keyframing

### 14. Toggle Playback (14_toggle_playback.json)
**Description**: Starts or stops animation playback
- Toggles animation playback
- Maintains current frame
- Useful for animation preview

### 15. Jump to Next Keyframe (15_jump_to_next_keyframe.json)
**Description**: Moves to next keyframe in timeline
- Jumps to next keyframe
- Maintains current view
- Useful for timeline navigation

### 16. Jump to Previous Keyframe (16_jump_to_previous_keyframe.json)
**Description**: Moves to previous keyframe in timeline
- Jumps to previous keyframe
- Maintains current view
- Useful for timeline navigation

### 17. Toggle Fullscreen (17_toggle_fullscreen.json)
**Description**: Switches between fullscreen and windowed mode
- Toggles fullscreen display
- Maintains workspace layout
- Useful for workspace management

### 18. Toggle Quad View (18_toggle_quad_view.json)
**Description**: Switches between single and quad view layouts
- Toggles view layout
- Maintains view settings
- Useful for multi-view editing

### 19. Toggle Outliner (19_toggle_outliner.json)
**Description**: Shows or hides the outliner panel
- Toggles outliner visibility
- Maintains panel settings
- Useful for interface management

### 20. Toggle Properties (20_toggle_properties.json)
**Description**: Shows or hides the properties panel
- Toggles properties visibility
- Maintains panel settings
- Useful for interface management

## Usage Tips

1. All macros are designed for Mac OS and use the Command key (0x08) for shortcuts
2. Delays between commands are set to 50ms for reliable execution
3. Macros can be combined for complex operations
4. Some macros may need adjustment based on Blender version
5. Test macros in a new file before using in important projects

## Customization

You can modify these macros by:
1. Adjusting delay times (ms parameter)
2. Changing key combinations
3. Adding or removing commands
4. Modifying modifier settings
5. Adding error handling

## Troubleshooting

If a macro doesn't work as expected:
1. Check Blender version compatibility
2. Verify keyboard layout settings
3. Ensure no conflicting shortcuts
4. Test with different delay values
5. Check for modifier conflicts 