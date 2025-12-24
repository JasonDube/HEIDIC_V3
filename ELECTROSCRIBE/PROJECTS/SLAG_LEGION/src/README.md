# SLAG LEGION Source Modules

This directory contains modular source files for the SLAG LEGION game.

## Planned Modules

As the game grows, we'll split functionality into separate files:

- `player.hd` - Player movement, physics, and state
- `vehicle.hd` - Vehicle piloting and physics
- `pickup.hd` - Item pickup and drop system
- `targeting.hd` - Targeting and scanning system
- `inventory.hd` - Inventory management
- `trading.hd` - Trading mechanics
- `world.hd` - World generation and management
- `ui.hd` - User interface components

## Current Status

Currently, all game logic is in the main `slag_legion.hd` file. As HEIDIC gains a proper module/import system, we'll refactor into these separate files for better organization.

## Architecture

```
slag_legion.hd (main)
    │
    ├── Player System
    │   ├── Movement (WASD)
    │   ├── Physics (gravity, jump)
    │   └── Camera (mouse look)
    │
    ├── Vehicle System
    │   ├── Piloting controls
    │   ├── Movement physics
    │   └── Attachment system
    │
    ├── Pickup System
    │   ├── Raycast selection
    │   ├── Pickup/drop
    │   └── Visual feedback
    │
    ├── Targeting System
    │   ├── Target cycling
    │   └── Distance tracking
    │
    └── UI System (Neuroshell)
        ├── Crosshair
        ├── Target panel
        └── Distance display
```

