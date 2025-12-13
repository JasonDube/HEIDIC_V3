# EDEN ENGINE - Spinning Triangle Example

This example demonstrates a simple spinning triangle rendered with Vulkan.

## Building

1. Compile the EDEN ENGINE code:
   ```bash
   cd heidic_v2
   cargo run -- compile examples/spinning_triangle/spinning_triangle.hd
   ```

2. Compile the C++ code with Vulkan helpers:
   ```bash
   g++ -std=c++17 -O3 examples/spinning_triangle/spinning_triangle.cpp vulkan/eden_vulkan_helpers.cpp -o spinning_triangle -lvulkan -lglfw
   ```

## Requirements

- Vulkan SDK installed
- GLFW3 library
- Shader files (`vert_3d.spv` and `frag.spv`) must be in the working directory or a parent directory
  - The shader files should be compiled from `shaders/vert_3d.glsl` and `shaders/frag.glsl`
  - Or copy the `.spv` files from the parent project's `shaders/` directory

## Features

- Spinning triangle with RGB colors
- Uses GLM for matrix math (rotation, view, projection)
- Full Vulkan rendering pipeline

