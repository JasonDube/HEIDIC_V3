# FPS Camera Test

A test project demonstrating FPS camera movement with mouse look and WASD controls.

## Features

- **FPS Camera Movement**: First-person camera with smooth mouse look
- **WASD Controls**:
  - `W` - Move forward in the direction you're facing
  - `S` - Move backward
  - `A` - Strafe left
  - `D` - Strafe right
- **Mouse Look**: Mouse movement controls camera rotation (yaw and pitch)
- **Programmatic Floor Cube**: A floor cube is created programmatically for testing

## Controls

- **W** - Move forward
- **S** - Move backward
- **A** - Strafe left
- **D** - Strafe right
- **Mouse** - Look around (camera rotation)
- **ESC** - Exit

## Implementation Notes

The camera uses:
- **Yaw**: Horizontal rotation (left/right)
- **Pitch**: Vertical rotation (up/down, clamped to prevent flipping)
- **Forward/Right Vectors**: Calculated from yaw and pitch for movement

The renderer needs to be implemented in C++ to:
1. Create a programmatic floor cube mesh
2. Set up camera view matrix from position, yaw, and pitch
3. Render the floor cube with proper lighting

## Building

This project requires the HEIDIC compiler and C++ renderer implementation.

