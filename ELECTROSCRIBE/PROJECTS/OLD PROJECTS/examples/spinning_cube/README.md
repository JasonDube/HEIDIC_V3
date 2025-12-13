# EDEN ENGINE - Spinning Cube Example

This example demonstrates a colorful 3D cube rendered with Vulkan using vertex and index buffers.

## Building

1. Compile the HEIDIC code:
   ```bash
   cargo run -- compile examples/spinning_cube/spinning_cube.hd
   ```

2. Compile the shaders (if you have glslc from Vulkan SDK):
   ```bash
   glslc examples/spinning_cube/vert_cube.glsl -o examples/spinning_cube/vert_cube.spv
   glslc examples/spinning_cube/frag_cube.glsl -o examples/spinning_cube/frag_cube.spv
   ```

3. Compile the C++ code with Vulkan helpers:
   ```bash
   g++ -std=c++17 -O3 examples/spinning_cube/spinning_cube.cpp vulkan/eden_vulkan_helpers.cpp -o examples/spinning_cube/spinning_cube.exe -I. -L. -lvulkan -lglfw
   ```

   Or on Windows with MinGW:
   ```bash
   g++ -std=c++17 -O3 examples/spinning_cube/spinning_cube.cpp vulkan/eden_vulkan_helpers.cpp -o examples/spinning_cube/spinning_cube.exe -IC:\VulkanSDK\1.3.xxx.x\Include -LC:\VulkanSDK\1.3.xxx.x\Lib -LC:\glfw-3.4\build\src -lvulkan-1 -lglfw3
   ```

## Requirements

- Vulkan SDK installed
- GLFW3 library
- Shader files (`vert_cube.spv` and `frag_cube.spv`) must be compiled and in the example directory

## Features

- Spinning colorful 3D cube
- Uses vertex and index buffers
- Full Vulkan rendering pipeline with depth testing
- Uses GLM for matrix math (rotation, view, projection)

## Differences from Triangle Example

- Uses vertex buffers instead of hardcoded shader vertices
- Uses indexed drawing (`vkCmdDrawIndexed`) instead of `vkCmdDraw`
- Has a separate pipeline with vertex input state configured
- Renders 8 vertices and 36 indices (12 triangles)

