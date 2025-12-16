# Shaders Explained: A Beginner's Guide

## What Are Shaders?

**Shaders** are small programs that run on your graphics card (GPU) to control how 3D graphics are rendered. Think of them as specialized functions that tell the GPU:

- **Where** to draw things (vertex shader)
- **What color** each pixel should be (fragment shader)
- **How** to process lighting, textures, and effects

Shaders are written in special languages like **GLSL** (OpenGL Shading Language) or **HLSL** (High-Level Shading Language). In HEIDIC, we use GLSL, which compiles to **SPIR-V** (a binary format that Vulkan understands).

---

## A Brief History: The Shift from Fixed-Function to Programmable Graphics

If you're an old-school programmer, you might remember when shaders "came out" in the early 2000s. Here's what happened:

### The Fixed-Function Era (1990s - Early 2000s)

**GPUs existed, but they weren't programmable.** Cards like:
- 3dfx Voodoo
- NVIDIA RIVA TNT
- ATI Rage

...had **fixed-function pipelines**. You could:
- Configure lighting modes (Gouraud shading, Phong shading)
- Set texture mapping modes
- Enable/disable fog
- Adjust blending modes

But you **couldn't write custom code**. The GPU had hardcoded rendering algorithms, and you could only tweak parameters. The CPU would send commands like:

```
"Draw a triangle at these coordinates with Gouraud shading and this texture"
```

And the GPU would render it using its built-in, unchangeable algorithms.

### The Programmable Revolution (Early 2000s)

Everything changed when **programmable shaders** were introduced:

- **DirectX 8 (2000)**: Pixel shaders (fragment shaders) - you could now write custom code for pixels!
- **DirectX 9 (2002)**: Vertex shaders - you could now write custom code for vertices!
- **OpenGL 2.0 (2004)**: GLSL (OpenGL Shading Language) - a high-level language for writing shaders

This was **revolutionary**. Instead of being stuck with whatever rendering modes the GPU manufacturer gave you, you could now write your own rendering code that runs on the GPU.

### Why It Matters

Before shaders:
- Every game looked similar (limited by fixed GPU features)
- Special effects were impossible or very difficult
- You had to wait for GPU manufacturers to add new features

After shaders:
- Developers could create **any visual effect** they could imagine
- Games could have unique, custom looks
- Innovation happened in software, not just hardware

So yes - **shaders came on the scene when GPUs became programmable**, not just when GPUs started handling graphics. GPUs were already doing the heavy lifting, but shaders gave developers the power to **control** how that rendering happened.

Today, virtually all modern graphics programming uses programmable shaders. The fixed-function pipeline is essentially obsolete.

---

## The Graphics Pipeline

When you render a triangle (or any 3D object), it goes through several stages:

```
3D Model Data → Vertex Shader → Rasterization → Fragment Shader → Screen
```

1. **Vertex Shader** - Processes each vertex (corner point) of your 3D model
2. **Rasterization** - Converts the 3D shape into pixels (fragments)
3. **Fragment Shader** - Colors each pixel
4. **Screen** - You see the final image!

---

## Vertex Shader

### What It Does

The **vertex shader** runs once for each **vertex** (corner point) of your 3D model. It's responsible for:

1. **Positioning** - Where the vertex appears on screen
2. **Transformation** - Moving, rotating, scaling objects
3. **Passing Data** - Sending information to the fragment shader

### Example: Your `my_shader.vert`

```glsl
#version 450

// Input: Vertex position and color (from vertex buffer)
layout(location = 0) in vec3 inPosition;  // x, y, z coordinates
layout(location = 1) in vec3 inColor;     // r, g, b color

// Output: Color to pass to fragment shader
layout(location = 0) out vec3 fragColor;

// Uniform buffer: Transformation matrices (shared by all vertices)
layout(binding = 0) uniform UniformBufferObject {
    mat4 model;   // Object's position/rotation/scale
    mat4 view;    // Camera position
    mat4 proj;    // Camera projection (perspective)
} ubo;

void main() {
    // Transform vertex from 3D world space to 2D screen space
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);
    
    // Pass color to fragment shader
    fragColor = inColor;
}
```

### Key Concepts

- **`in`** - Input data (comes from vertex buffer or uniforms)
- **`out`** - Output data (sent to next stage, usually fragment shader)
- **`uniform`** - Shared data (same for all vertices, like camera position)
- **`gl_Position`** - Special output that tells GPU where to draw the vertex
- **Matrices** - `model`, `view`, `proj` transform 3D coordinates to screen coordinates

### What Happens

For a triangle with 3 vertices, the vertex shader runs **3 times**:
- Once for the bottom vertex
- Once for the top-right vertex  
- Once for the top-left vertex

Each time, it calculates where that vertex should appear on screen.

---

## Fragment Shader

### What It Does

The **fragment shader** (also called pixel shader) runs once for each **pixel** that your triangle covers. It's responsible for:

1. **Coloring** - What color each pixel should be
2. **Lighting** - How light affects the pixel
3. **Textures** - Applying images to surfaces
4. **Effects** - Fog, transparency, etc.

### Example: Your `my_shader.frag`

```glsl
#version 450

// Input: Color from vertex shader (interpolated across the triangle)
layout(location = 0) in vec3 fragColor;

// Output: Final pixel color
layout(location = 0) out vec4 outColor;

void main() {
    // Set pixel to bright red
    outColor = vec4(1.0, 0.0, 0.0, 1.0);
    //        vec4(red, green, blue, alpha)
    //        Values range from 0.0 (dark) to 1.0 (bright)
}
```

### Key Concepts

- **`in`** - Input from vertex shader (automatically interpolated between vertices)
- **`out`** - Final pixel color that appears on screen
- **`vec4`** - 4-component vector (red, green, blue, alpha/transparency)
- **Interpolation** - Colors blend smoothly across the triangle

### What Happens

If your triangle covers 1000 pixels on screen, the fragment shader runs **1000 times**:
- Once for each pixel
- Each pixel gets its color calculated
- The result is what you see on screen!

### Color Interpolation

If your triangle has:
- Bottom vertex: **Red** (1.0, 0.0, 0.0)
- Top-right vertex: **Green** (0.0, 1.0, 0.0)
- Top-left vertex: **Blue** (0.0, 0.0, 1.0)

The fragment shader will automatically **blend** these colors across the triangle, creating a smooth gradient!

---

## How They Work Together

### Example: Rendering a Colored Triangle

1. **Vertex Shader** (runs 3 times):
   ```
   Vertex 1: Position (0, -0.5, 0) → Screen position (400, 500)
   Vertex 2: Position (0.5, 0.5, 0) → Screen position (600, 200)
   Vertex 3: Position (-0.5, 0.5, 0) → Screen position (200, 200)
   ```

2. **Rasterization**:
   - GPU figures out which pixels are inside the triangle
   - Let's say it finds 1000 pixels

3. **Fragment Shader** (runs 1000 times):
   ```
   Pixel 1: Color = (1.0, 0.0, 0.0) → Red
   Pixel 2: Color = (0.9, 0.1, 0.0) → Red-orange
   Pixel 3: Color = (0.8, 0.2, 0.0) → Orange
   ... (colors interpolate across triangle)
   Pixel 1000: Color = (0.0, 0.0, 1.0) → Blue
   ```

4. **Result**: A beautiful gradient triangle appears on screen!

---

## Common Shader Operations

### Vertex Shader

- **Transform positions**: `gl_Position = matrix * position`
- **Calculate normals**: For lighting calculations
- **Pass data**: Colors, texture coordinates, etc.

### Fragment Shader

- **Simple color**: `outColor = vec4(1.0, 0.0, 0.0, 1.0)` (solid red)
- **Use input color**: `outColor = vec4(fragColor, 1.0)` (use vertex color)
- **Textures**: Sample from images
- **Lighting**: Calculate how light affects the surface
- **Effects**: Fog, glow, distortion, etc.

---

## Why Shaders Are Powerful

1. **Parallel Processing** - GPUs have thousands of cores, so shaders run incredibly fast
2. **Flexibility** - You can create any visual effect you can imagine
3. **Real-time** - Changes happen instantly (like hot-reload!)
4. **Efficiency** - Only process what's visible on screen

---

## In HEIDIC

When you use `@hot shader` in HEIDIC:

```heidic
@hot
shader vertex "shaders/my_shader.vert" {
}

@hot
shader fragment "shaders/my_shader.frag" {
}
```

HEIDIC will:
1. Compile your GLSL shaders to SPIR-V (`.spv` files)
2. Load them into the GPU
3. Watch for changes and hot-reload them automatically
4. Recreate the graphics pipeline when shaders change

This means you can edit shaders, save, and see changes **instantly** without restarting your game!

---

## Tips for Writing Shaders

1. **Start Simple** - Begin with solid colors, then add complexity
2. **Test Incrementally** - Change one thing at a time
3. **Use Comments** - Document what each part does
4. **Watch for Errors** - Shader compilation errors will show in the compiler output
5. **Experiment** - Try different colors, values, and formulas!

---

## Common Shader Patterns

### Solid Color
```glsl
outColor = vec4(1.0, 0.0, 0.0, 1.0);  // Bright red
```

### Use Vertex Color
```glsl
outColor = vec4(fragColor, 1.0);  // Use color from vertex shader
```

### Gradient Based on Position
```glsl
outColor = vec4(gl_FragCoord.x / 800.0, 0.0, 0.0, 1.0);  // Red gradient
```

### Animated Color
```glsl
float time = ...;  // Time value
outColor = vec4(sin(time), cos(time), 0.0, 1.0);  // Pulsing colors
```

---

## Next Steps

Now that you understand shaders, try:
- Changing colors in your fragment shader
- Experimenting with different vertex positions
- Adding time-based animations
- Creating gradients and patterns
- Learning about textures and lighting

Happy shader coding! 🎨

