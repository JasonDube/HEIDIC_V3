# SLAG LEGION

**A scavenging and piloting game built with HEIDIC**

> ⚡ **New!** SLAG LEGION now uses a clean, modular engine (`sl::Engine`) instead of the legacy monolithic code.

![Status](https://img.shields.io/badge/Status-Alpha-orange)
![Engine](https://img.shields.io/badge/Engine-EDEN-blue)
![Language](https://img.shields.io/badge/Language-HEIDIC-purple)

---

## 🎮 Overview

In a post-industrial world, you pilot vehicles through slag heaps and ruins, scavenging valuable materials and trading them for survival. SLAG LEGION combines first-person exploration with vehicle piloting mechanics in a gritty, industrial setting.

## ✨ Features

### Movement & Controls
- **First-Person Movement**: Full WASD movement with mouse look
- **Jumping & Physics**: Realistic gravity and ground collision
- **Variable Speed**: 9 speed levels for precise control

### Vehicle Piloting
- **Enter/Exit Vehicles**: Approach the helm and press E
- **Full 3D Movement**: Fly in any direction
- **Rotation Controls**: Turn your vehicle left and right
- **Cargo System**: Items placed on vehicles stay attached

### Scavenging System
- **Item Pickup**: Grab items with Left Ctrl
- **Targeting System**: Cycle through items with T key
- **Item Properties**: Each item has type, value, condition, and weight
- **Vehicle Storage**: Drop items on your vehicle to transport them

### UI System (Neuroshell)
- **Crosshair**: Center-screen aiming reticle
- **Target Panel**: Shows current target info
- **Distance Display**: Real-time distance to targets

## 🎯 Controls

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
| E | Enter/Exit vehicle |

## 📁 Project Structure

```
SLAG_LEGION/
├── slag_legion.hd          # Main game source (HEIDIC)
├── slag_legion.cpp         # Generated C++ (auto-generated)
├── engine/                 # Modular SL Engine
│   ├── sl_engine.h         # Engine header
│   ├── sl_engine.cpp       # Engine implementation
│   ├── sl_api.h            # C API header
│   └── sl_api.cpp          # C API + HEIDIC compatibility
├── CMakeLists.txt          # CMake build configuration
├── build_modular.bat       # Modular build script
├── crosshair.png           # UI crosshair texture
├── items.json              # Item definitions
├── fonts/                  # Font textures
│   └── dbyte_2x.png        # Default font
├── audio/                  # Sound effects & music
├── textures/               # Game textures
└── models/                 # 3D models
```

## 🏗️ Architecture

SLAG LEGION uses a clean, modular architecture:

```
slag_legion.hd (HEIDIC game code)
        │
        ▼
slag_legion.cpp (generated C++)
        │
        ▼
sl_api.cpp (C API wrapper)
        │
        ▼
sl::Engine (modular Vulkan engine)
        │
        ├── Uses: vulkan/utils/raycast.h/.cpp
        ├── Uses: glm for math
        └── Uses: GLFW for windowing
```

**Why modular?**
- Clean separation of concerns
- Easier to maintain and extend
- No legacy code dependencies
- Can be reused for other games

## 🔧 Building

### Requirements
- HEIDIC Compiler
- EDEN Engine Runtime
- Vulkan SDK 1.3+
- C++17 compatible compiler

### Build Steps

```bash
# From Electroscribe IDE
1. Open slag_legion.hd
2. Press F5 to compile and run

# Or from command line
heidic compile slag_legion.hd
```

## 🎨 Game Systems

### Entity System
The game uses a simple entity system with cubes representing world objects:
- **Index 0-13**: Scavengeable items
- **Index 14**: Vehicle (cannot be picked up)
- **Index 15**: Helm/Control panel
- **Index 16**: Ground
- **Index 17**: Buildings/Structures

### Physics
- Gravity applies to all non-static objects
- Ground collision prevents falling through floor
- Vehicle collision prevents walking through

### Attachment System
Items can be attached to vehicles:
- Items maintain relative position during movement
- Items rotate with vehicle
- Detach items by picking them up

## 🚀 Roadmap

### v0.1 (Current)
- [x] Basic FPS movement
- [x] Vehicle piloting
- [x] Item pickup system
- [x] Targeting system
- [x] Basic UI

### v0.2 (Planned)
- [ ] Inventory system
- [ ] Trading mechanics
- [ ] Multiple vehicles
- [ ] Save/Load system

### v0.3 (Future)
- [ ] Procedural world generation
- [ ] NPC traders
- [ ] Combat system
- [ ] Crafting

## 📝 License

Part of the HEIDIC Project - See root LICENSE file.

## 🔗 Related Projects

- **HEIDIC** - The programming language
- **EDEN Engine** - The game engine runtime
- **Electroscribe** - The IDE
- **ESE** - 3D model editor

