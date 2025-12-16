# ESE - Echo Synapse Editor

**ESE** (Echo Synapse Editor) is a 3D model editor built with HEIDIC. It provides tools for viewing, editing, and manipulating 3D models with support for mesh manipulation, texture mapping, and connection points.

## Overview

ESE is a powerful 3D modeling tool that allows you to:
- View and inspect OBJ models
- Apply and preview textures
- Manipulate mesh geometry (vertices, edges, faces, quads)
- Create and edit connection points
- Use interactive gizmos for transformations

## Features

### Model Viewing
- **OBJ Model Loading** - Load and view 3D models
- **Texture Support** - Apply PNG and DDS textures
- **Multiple View Modes** - Vertex, edge, face, and quad selection modes

### Mesh Editing
- **Vertex Manipulation** - Select and move individual vertices
- **Edge Operations** - Select edges and insert edge loops
- **Face/Quad Editing** - Select and extrude faces and quads
- **Interactive Gizmos** - Move, scale, and rotate selected elements

### Advanced Features
- **Extrusion** - Extend faces and quads to create new geometry
- **Edge Loop Insertion** - Add edge loops perpendicular to selected edges
- **Connection Points** - Define and manage connection points for game logic
- **Undo/Redo** - Full undo/redo support (Ctrl+Z)

## Usage

### Launching ESE

ESE is a HEIDIC project. To run it:

1. Open Electroscribe IDE
2. Load the ESE project from `ELECTROSCRIBE/PROJECTS/ESE/`
3. Click the `>` (Run) button

### Controls

- **Mouse:**
  - Left-drag: Rotate camera
  - Right-drag: Pan camera
  - Wheel: Zoom

- **Keyboard:**
  - `1` - Vertex mode (select vertices)
  - `2` - Edge mode (select edges)
  - `3` - Face mode (select triangles)
  - `4` - Quad mode (select quads)
  - `W` - Extrude mode (in quad mode)
  - `Ctrl+Click` - Multi-select
  - `Ctrl+Z` - Undo
  - `ESC` - Exit

- **Gizmo:**
  - Click and drag X/Y/Z axes to move, scale, or rotate selected elements

### File Operations

- **File > Open Model** - Load an OBJ file
- **File > Open Texture** - Load a texture file (PNG, DDS)
- **Create > Cube** - Create a basic cube mesh for testing
- **View > Connection Points** - View existing connection points and their properties

## Project Structure

ESE project files are located in:
- `ELECTROSCRIBE/PROJECTS/ESE/ese.hd` - Main ESE source code
- `ELECTROSCRIBE/PROJECTS/ESE/` - Project assets and generated files

## Dependencies

- GLFW (window management)
- Vulkan (rendering)
- ImGui (menu bar and UI)
- NFD (Native File Dialog) for file selection

## Future Features

- Lights and raycasting
- Direct vertex manipulation improvements
- Game-related control points
- Slider controls for model editing (e.g., scale the nose)
- Wireframe and normals visualization
- Enhanced camera controls

## Source Code

ESE source code is located in:
- `ELECTROSCRIBE/PROJECTS/ESE/ese.hd` - Main application code
- `vulkan/eden_vulkan_helpers.cpp` - Rendering and mesh manipulation code

## Related Tools

- **[EDEN Engine](EDEN_ENGINE.md)** - Runtime game engine
- **[Electroscribe](ELECTROSCRIBE.md)** - IDE for HEIDIC development

