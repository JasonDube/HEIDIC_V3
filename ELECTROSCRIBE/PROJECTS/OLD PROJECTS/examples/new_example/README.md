# EDEN ENGINE - New Example

This example provides a starting point for working on new features with a larger window (1300x700).

## Building

1. Compile the EDEN ENGINE code:
   ```bash
   cargo run -- compile examples/new_example/new_example.hd
   ```

2. Compile the C++ code with Vulkan helpers:
   
   **Windows (using build.bat):**
   ```bash
   examples\new_example\build.bat
   ```
   
   **Manual build (Windows):**
   ```bash
   g++ -std=c++17 -O3 ^
       -I.. ^
       -I"C:\glfw-3.4\include" ^
       -I"%VULKAN_SDK%\Include" ^
       examples/new_example/new_example.cpp ^
       vulkan/eden_vulkan_helpers.cpp ^
       -o new_example.exe ^
       -L"%VULKAN_SDK%\Lib" ^
       -L"C:\glfw-3.4\build\src" ^
       -lvulkan-1 ^
       -lglfw3 ^
       -lgdi32
   ```
   
   **Note:** Make sure `VULKAN_SDK` environment variable is set to your Vulkan SDK path.

## Requirements

- Vulkan SDK installed
- GLFW3 library
- GLM math library
- Shader files (`vert_3d.spv` and `frag.spv`) must be in the working directory or a parent directory

## Features

- Window size: 1300x700
- Uses GLM for math operations
- Full Vulkan rendering pipeline

