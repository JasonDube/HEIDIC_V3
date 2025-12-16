# EDEN ENGINE (Heidic V2) Development Log

## Project Goal
Rebuild the HEIDIC language compiler ("Heidic V2") from scratch, focusing on a clean architecture and integrating a custom game engine ("EDEN ENGINE") with Vulkan, GLFW, GLM, and ImGui support.

## Development Journey

### 1. Compiler Setup
- Reused core Lexer, Parser, AST, and Type Checker from original HEIDIC.
- Cleaned up the codebase to remove legacy dependencies.
- Configured Codegen to emit C++ code compatible with modern C++17.

### 2. Graphics Integration (Vulkan & GLFW)
- Created a `stdlib/` directory for standard library headers.
- Implemented `eden_vulkan_helpers` to abstract verbose Vulkan initialization.
- Integrated `GLFW` for window management.

### 3. The "Spinning Triangle" Challenge
We encountered several significant hurdles getting the basic 3D example to run:

#### Phase 1: Build System & Linking
- **Challenge**: The C++ compiler (`g++`) failed to link GLFW and Vulkan correctly on Windows.
- **Solution**: Identified correct library paths (`C:\glfw-3.4\build\src` for MinGW `libglfw3.a`) and Vulkan SDK paths. Created a robust build command.

#### Phase 2: The "Black Screen" of Doom
After successful compilation, the window appeared but remained black. We diagnosed multiple rendering pipeline issues:

1.  **Coordinate System Mismatch**:
    *   *Issue*: GLM defaults to OpenGL coordinates (Y-up, Z range -1 to 1). Vulkan uses Y-down and Z range 0 to 1. This caused the geometry to be projected upside-down and, more critically, **clipped** because the negative Z values from standard GLM projection were outside Vulkan's [0, 1] depth range.
    *   *Fix*:
        1.  Inverted the Y-axis in the projection matrix: `proj[1][1] *= -1`.
        2.  Updated `stdlib/math.h` to use `glm::perspectiveRH_ZO` (Zero-to-One) instead of the default `glm::perspective`.

2.  **Shader Pipeline Mismatch**:
    *   *Issue*: The initial fragment shader expected a texture sampler (Binding 1), but our simple renderer only set up a Uniform Buffer (Binding 0). This caused undefined behavior/rendering failure.
    *   *Fix*: Created a simplified `frag_3d.glsl` that relies solely on interpolated vertex colors, removing the texture dependency.

3.  **Data Alignment (The Final Boss)**:
    *   *Issue*: Our C++ wrapper struct `Mat4` contained both the `glm::mat4` data AND a cached float array `m[16]`, making it 128 bytes. The shader expected a standard 64-byte `mat4`. Sending this struct to the GPU meant the shader read garbage values for View and Projection matrices, putting the triangle at infinity.
    *   *Fix*: Redefined the `UniformBufferObject` struct in C++ to use `glm::mat4` directly, ensuring the data layout matched the shader exactly (192 bytes total).

## Current Status
- **Compiler**: Working.
- **Renderer**: Spinning triangle renders correctly with color interpolation.
- **Math**: GLM integration verified with Vulkan coordinate systems.

