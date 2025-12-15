# ESE - Echo Synapse Editor

OBJ Model Viewer with Texture Support and ImGui Menu

## Purpose

ESE is a tool for viewing OBJ models and verifying that textures are applied correctly. It provides an ImGui-based menu interface for loading and inspecting 3D models.

## Features

- **ImGui Menu Bar** for file operations
- Load OBJ model files via File > Open Model
- Load texture files (PNG, DDS) via File > Open Texture
- Model info panel showing current loaded files
- View 3D models with textures applied

## Usage

1. Launch ESE
2. Use **File > Open Model** to load an OBJ file
3. Use **File > Open Texture** to load a texture file
4. The model will be displayed once both are loaded

## Future Features

- Lights and raycasting
- Direct vertex manipulation  
- Game-related control points
- Slider controls for model editing (e.g., scale the nose)
- Wireframe and normals visualization
- Camera orbit/pan/zoom controls

## Controls

- **File Menu** - Open models and textures
- **ESC** - Exit

## Building

This project uses the ELECTROSCRIBE build system. Compile with:

```
python ELECTROSCRIBE/main.py
```

Then open the ESE project and press Run.

## Dependencies

- GLFW (window management)
- Vulkan (rendering)
- ImGui (menu bar and UI)
- NFD (Native File Dialog) for file selection

## Project Configuration

The `.project_config` file enables ImGui support:

```
enable_ui_windows=true
```
