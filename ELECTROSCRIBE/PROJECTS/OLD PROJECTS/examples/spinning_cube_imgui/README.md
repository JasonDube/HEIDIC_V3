# EDEN ENGINE - Spinning Cube with ImGui

This example demonstrates a colorful 3D cube rendered with Vulkan, with an ImGui overlay showing FPS and camera information.

## Building

**Build with ImGui support:**
```bash
examples\spinning_cube_imgui\build.bat
```

Or use the generic build script:
```bash
examples\build_example.bat spinning_cube_imgui --imgui
```

## Features

- Spinning colorful 3D cube
- ImGui overlay showing:
  - FPS counter
  - Camera position
  - Application frame time
- Full Vulkan rendering pipeline
- Uses vertex and index buffers

## Requirements

- Vulkan SDK installed
- GLFW3 library
- ImGui source files (automatically found in `vulkan_reference/official_samples/third_party/imgui/`)
- Shader files (`vert_cube.spv` and `frag_cube.spv`) - compiled automatically

## Differences from spinning_cube

- Includes ImGui initialization
- Renders ImGui overlay each frame
- Shows FPS and camera information
- Must be built with `--imgui` flag

## Usage

1. Build the example (with `--imgui` flag)
2. Run `spinning_cube_imgui.exe`
3. You'll see the spinning cube with an ImGui window showing FPS and camera info

## Notes

- The ImGui overlay is automatically rendered after the 3D scene
- FPS is calculated in the main loop
- Camera position shown is a placeholder (hardcoded in renderer)
- To add more ImGui UI, modify the HEIDIC code to call ImGui functions

