# SLAG LEGION

**A scavenging and piloting game built with HEIDIC and EDEN Engine**

![Status](https://img.shields.io/badge/Status-Alpha_v0.1-orange)
![Engine](https://img.shields.io/badge/Engine-EDEN-blue)
![Language](https://img.shields.io/badge/Language-HEIDIC-purple)

---

## Overview

In a post-industrial world, you pilot vehicles through slag heaps and ruins, scavenging valuable materials and trading them for survival. SLAG LEGION combines first-person exploration with vehicle piloting mechanics in a gritty, industrial setting.

SLAG LEGION serves as both a full-featured game and a demonstration of HEIDIC's game development capabilities.

## Features

### Movement & Exploration
- **First-Person Movement**: Full WASD movement with mouse look
- **Jumping & Physics**: Realistic gravity and ground collision
- **Variable Speed**: 9 speed levels for precise control

### Vehicle Piloting
- **Enter/Exit Vehicles**: Approach the helm and press E to enter pilot mode
- **Full 3D Movement**: Fly in any direction
- **Rotation Controls**: Turn your vehicle left and right
- **Cargo System**: Items placed on vehicles stay attached during flight

### Scavenging System
- **Item Pickup**: Grab items with Left Ctrl
- **Targeting System**: Cycle through nearby items with T key
- **Item Properties**: Each item has type, value, condition, and weight
- **Vehicle Storage**: Drop items on your vehicle to transport them

### UI System (Neuroshell)
- **Crosshair**: Center-screen aiming reticle
- **Target Panel**: Shows current target information
- **Distance Display**: Real-time distance to targets

## Controls

### On Foot

| Key | Action |
|-----|--------|
| W/A/S/D | Move forward/left/backward/right |
| SPACE | Jump |
| Mouse | Look around |
| 1-9 | Set movement speed |
| LEFT CTRL | Pick up / Drop item |
| T | Cycle through targets |
| ESC | Exit game |

### In Vehicle (Pilot Mode)

| Key | Action |
|-----|--------|
| W/S | Move forward/backward |
| A/D | Turn left/right |
| SPACE | Ascend |
| LEFT SHIFT | Descend |
| E | Enter/Exit vehicle (when near helm) |

## Technical Implementation

SLAG LEGION showcases several HEIDIC features:

### Game Systems
```heidic
// Example: Player movement with physics
fn update_player(window: GLFWwindow, state: GameState, delta_time: f32): void {
    // Calculate movement vectors from camera yaw
    let yaw_rad: f32 = heidic_convert_degrees_to_radians(state.camera_yaw);
    
    let forward: Vec3 = Vec3(
        heidic_sin(yaw_rad),
        0.0,
        -heidic_cos(yaw_rad)
    );
    
    // WASD movement
    if glfwGetKey(window, 87) == 1 {  // W key
        velocity.x = forward.x * state.player_speed * delta_time;
        velocity.z = forward.z * state.player_speed * delta_time;
    }
    
    // Apply gravity
    state.player_velocity_y = state.player_velocity_y - PLAYER_GRAVITY * delta_time;
}
```

### Vehicle Piloting
```heidic
// Vehicle movement in pilot mode
if state.pilot_mode == 1 {
    let vehicle_yaw_rad: f32 = state.vehicle_yaw * 0.0174532925;
    
    // W - Forward in vehicle's facing direction
    if glfwGetKey(window, 87) == 1 {
        let forward_x: f32 = heidic_sin(vehicle_yaw_rad);
        let forward_z: f32 = heidic_cos(vehicle_yaw_rad);
        vehicle_move_delta.x = forward_x * state.vehicle_pilot_speed * delta_time;
        vehicle_move_delta.z = forward_z * state.vehicle_pilot_speed * delta_time;
    }
    
    // Update attached cargo
    heidic_update_attached_cubes(new_x, new_y, new_z, state.vehicle_yaw, size.y);
}
```

### Raycasting for Item Selection
```heidic
// Find item under crosshair
let cube_index: i32 = 0;
while cube_index < WORLD_CUBE_COUNT {
    let cube_pos: Vec3 = heidic_get_cube_position(cube_index);
    let cube_size: Vec3 = heidic_get_cube_size_xyz(cube_index);
    
    let hit: i32 = heidic_raycast_cube_hit_center(window, 
        cube_pos.x, cube_pos.y, cube_pos.z, 
        cube_size.x, cube_size.y, cube_size.z);
    
    if hit == 1 {
        // Calculate distance and track closest hit
        // ...
    }
    cube_index = cube_index + 1;
}
```

## Project Structure

```
ELECTROSCRIBE/PROJECTS/SLAG_LEGION/
├── slag_legion.hd      # Main game source
├── build.bat           # Build script
├── README.md           # Project documentation
├── crosshair.png       # UI crosshair texture
├── items.json          # Item definitions
├── fonts/              # Font textures
│   └── dbyte_2x.png    # Default pixel font
├── audio/              # Sound effects & music
├── textures/           # Game textures
├── models/             # 3D models (future)
└── src/                # Additional source modules
```

## Building & Running

### From Electroscribe IDE
1. Open `ELECTROSCRIBE/PROJECTS/SLAG_LEGION/slag_legion.hd`
2. Press **F5** to compile and run

### From Command Line
```bash
cd ELECTROSCRIBE/PROJECTS/SLAG_LEGION
heidic_v2 compile slag_legion.hd
slag_legion.exe
```

## Roadmap

### v0.1 (Current - Alpha)
- [x] First-person movement with physics
- [x] Vehicle piloting system
- [x] Item pickup/drop system
- [x] Targeting system
- [x] Basic HUD (crosshair, target panel)

### v0.2 (Planned)
- [ ] Inventory management UI
- [ ] Trading with NPCs
- [ ] Multiple vehicle types
- [ ] Save/Load game state

### v0.3 (Future)
- [ ] Procedural world generation
- [ ] Combat system
- [ ] Crafting mechanics
- [ ] Multiplayer support

## Related Tools

- **[EDEN Engine](EDEN_ENGINE.md)** - Runtime game engine powering SLAG LEGION
- **[Electroscribe](ELECTROSCRIBE.md)** - IDE used for development
- **[ESE](ESE.md)** - 3D model editor for game assets
- **[HEIROC](../HEIDIC/HEIROC_ARCHITECTURE.md)** - Scripting language for game logic

## Source Code

The full source code is available at:
[SLAG_LEGION on GitHub](https://github.com/JasonDube/HEIDIC/tree/main/ELECTROSCRIBE/PROJECTS/SLAG_LEGION)

## Contributing

SLAG LEGION is part of the HEIDIC project. Contributions are welcome! See the main repository for contribution guidelines.
