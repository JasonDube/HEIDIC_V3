# Building EDEN ENGINE Examples

## Quick Start

Use the generic build script for any example:

```bash
examples\build_example.bat <example_name> [--imgui]
```

### Examples

**Build without ImGui:**
```bash
examples\build_example.bat spinning_cube
```

**Build with ImGui support:**
```bash
examples\build_example.bat spinning_cube --imgui
```

## What the Build Script Does

1. **Compiles HEIDIC code** - Runs `cargo run -- compile` to generate C++
2. **Compiles shaders** - Automatically finds and compiles `vert_*.glsl` and `frag_*.glsl` files
3. **Compiles C++ code** - Links with Vulkan, GLFW, and optionally ImGui

## Requirements

- **Vulkan SDK** - Must be installed and `VULKAN_SDK` environment variable set
- **GLFW** - Expected at `C:\glfw-3.4` (or set `GLFW_PATH`)
- **ImGui** (optional) - Automatically found at `vulkan_reference/official_samples/third_party/imgui/`

## Example Structure

Each example should have:
```
examples/
  <example_name>/
    <example_name>.hd          # HEIDIC source
    <example_name>.cpp          # Generated C++ (from .hd)
    vert_*.glsl                 # Vertex shaders (optional)
    frag_*.glsl                 # Fragment shaders (optional)
    *.spv                       # Compiled shaders (generated)
    build.bat                   # Example-specific build script (optional)
```

## Using ImGui

To use ImGui in your example:

1. **Build with ImGui:**
   ```bash
   examples\build_example.bat spinning_cube --imgui
   ```

2. **In your HEIDIC code:**
   ```c
   extern fn heidic_init_imgui(window: GLFWwindow): i32;
   extern fn heidic_imgui_new_frame(): void;
   extern fn heidic_imgui_render_demo_overlay(fps: f32, x: f32, y: f32, z: f32): void;
   
   // After initializing renderer:
   heidic_init_imgui(window);
   
   // In render loop:
   heidic_imgui_new_frame();
   heidic_imgui_render_demo_overlay(fps, camera_x, camera_y, camera_z);
   ```

## Custom Build Scripts

You can create example-specific build scripts that call the generic one:

```batch
@echo off
call ..\build_example.bat spinning_cube --imgui
```

Or customize further by copying `build_example.bat` and modifying it.

