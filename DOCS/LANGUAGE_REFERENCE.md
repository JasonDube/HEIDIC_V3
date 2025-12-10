# HEIDIC Language Reference

Complete reference documentation for the HEIDIC programming language.

## Table of Contents

1. [Overview](#overview)
2. [Types](#types)
3. [Syntax](#syntax)
4. [Built-in Functions](#built-in-functions)
5. [Standard Library](#standard-library)
6. [EDEN Engine API](#eden-engine-api)
7. [Examples](#examples)

---

## Overview

HEIDIC is a statically-typed, compiled language designed for building high-performance game engines. It compiles to native C++17 code for maximum performance.

**Key Features:**
- Zero-cost abstractions
- No garbage collection (manual memory management)
- ECS (Entity Component System) support
- SOA (Structure-of-Arrays) layout for GPU computing
- Direct integration with Vulkan, GLFW, and ImGui

---

## Types

### Primitive Types

| Type | Description | C++ Equivalent |
|------|-------------|----------------|
| `i32` | 32-bit signed integer | `int32_t` |
| `i64` | 64-bit signed integer | `int64_t` |
| `f32` | 32-bit floating point | `float` |
| `f64` | 64-bit floating point | `double` |
| `bool` | Boolean (`true` or `false`) | `bool` |
| `string` | String type | `std::string` |
| `void` | No return value | `void` |

### Composite Types

#### Arrays
```heidic
let numbers: [i32];           // Dynamic array of integers
let positions: [Vec3];        // Dynamic array of 3D vectors
```

#### Structs
```heidic
struct Point {
    x: f32,
    y: f32
}
```

#### Components (ECS)
```heidic
component Position {
    x: f32,
    y: f32,
    z: f32
}

// With default values
component Transform {
    position: Vec3,
    rotation: Quat,
    scale: Vec3 = Vec3(1, 1, 1)  // default value
}
```

#### SOA (Structure-of-Arrays)

**Mesh SOA** - Optimized for CUDA/OptiX ray tracing:
```heidic
mesh_soa Mesh {
    positions: [Vec3],
    uvs: [Vec2],
    colors: [Vec3],
    indices: [i32]
}
```

**Component SOA** - Optimized for ECS iteration:
```heidic
component_soa Velocity {
    x: [f32],
    y: [f32],
    z: [f32]
}
```

### Math Types

| Type | Description | C++ Equivalent |
|------|-------------|----------------|
| `Vec2` | 2D vector (x, y) | `Vec2` (GLM wrapper) |
| `Vec3` | 3D vector (x, y, z) | `Vec3` (GLM wrapper) |
| `Vec4` | 4D vector (x, y, z, w) | `Vec4` (GLM wrapper) |
| `Mat4` | 4x4 matrix | `Mat4` (GLM wrapper) |

### Vulkan Types

| Type | Description |
|------|-------------|
| `VkInstance` | Vulkan instance |
| `VkDevice` | Vulkan logical device |
| `VkPhysicalDevice` | Vulkan physical device |
| `VkQueue` | Vulkan queue |
| `VkCommandPool` | Command pool |
| `VkCommandBuffer` | Command buffer |
| `VkSwapchainKHR` | Swapchain |
| `VkSurfaceKHR` | Surface |
| `VkRenderPass` | Render pass |
| `VkPipeline` | Graphics pipeline |
| `VkFramebuffer` | Framebuffer |
| `VkBuffer` | Buffer |
| `VkImage` | Image |
| `VkImageView` | Image view |
| `VkSemaphore` | Semaphore |
| `VkFence` | Fence |
| `VkResult` | Result code |

### GLFW Types

| Type | Description |
|------|-------------|
| `GLFWwindow` | Window handle (pointer type) |
| `GLFWbool` | Boolean (int32_t) |

### Type Aliases

```heidic
type ImageView = VkImageView;
type DescriptorSet = VkDescriptorSet;
type CommandBuffer = VkCommandBuffer;
```

### Query Types (ECS)

Query types allow you to query entities that have specific components:

```heidic
component Position {
    x: f32,
    y: f32,
    z: f32
}

component Velocity {
    x: f32,
    y: f32,
    z: f32
}

fn update_transforms(q: query<Position, Velocity>): void {
    // Query for entities with both Position and Velocity components
}
```

**Syntax:** `query<Component1, Component2, ...>`

**Requirements:**
- All types in the query must be Component or ComponentSOA types
- At least one component type is required

**Generated Code:**
- Creates a `Query_Component1_Component2_...` struct
- Generates a `for_each_query_component1_component2_...()` helper function for iteration

### Shaders (Compile-Time Embedding)

Shaders are compiled to SPIR-V at compile-time and embedded in the generated C++ code:

```heidic
shader vertex "shaders/triangle.vert" {
    // Optional metadata/comments
}

shader fragment "shaders/triangle.frag" {
    // Gets compiled to SPIR-V and embedded
}
```

**Supported Shader Stages:**
- `vertex` - Vertex shader
- `fragment` - Fragment/pixel shader
- `compute` - Compute shader
- `geometry` - Geometry shader
- `tessellation_control` - Tessellation control shader
- `tessellation_evaluation` - Tessellation evaluation shader

**Usage:**
The compiler automatically:
1. Compiles the shader source file to SPIR-V using `glslc`
2. Embeds the SPIR-V bytecode as a `const uint8_t[]` array
3. Generates a helper function `load_<name>_shader()` to load the shader

**Example:**
```heidic
shader vertex "shaders/cube.vert" { }
shader fragment "shaders/cube.frag" { }

fn main(): void {
    // In C++ code, you can use:
    // auto vert_code = load_cube_vert_shader();
    // auto frag_code = load_cube_frag_shader();
}
```

**Requirements:**
- `glslc` (GLSL compiler from Vulkan SDK) must be in PATH
- Shader source files must exist at the specified path

---

### System Dependency Declaration

Systems can declare execution order dependencies using the `@system` attribute:

**Syntax:**
```heidic
@system(system_name, after = Dependency1, before = Dependency2)
fn system_function(q: query<Component1, Component2>): void {
    // System implementation
}
```

**Parameters:**
- `system_name`: Name of the system (used for dependency references)
- `after = SystemName`: Systems that must run before this one
- `before = SystemName`: Systems that must run after this one

**Example:**
```heidic
component Position { x: f32, y: f32, z: f32 }
component Velocity { x: f32, y: f32, z: f32 }

@system(physics)
fn physics_system(q: query<Position, Velocity>): void {
    // Physics updates
}

@system(render, after = Physics, before = RenderSubmit)
fn render_system(q: query<Position>): void {
    // Rendering logic
}

@system(rendersubmit)
fn render_submit(): void {
    // Submit render commands
}
```

**Generated Code:**
The compiler generates a `run_systems()` function that executes systems in dependency order using topological sorting. The compiler validates:
- All referenced systems exist
- No circular dependencies exist

**Execution Order:**
In the example above, systems run in this order:
1. `physics_system` (no dependencies)
2. `render_system` (after Physics)
3. `render_submit` (after render, which is after Physics)

---

### Frame-Scoped Memory (FrameArena)

FrameArena provides zero-allocation memory management for frame-scoped data:

**Syntax:**
```heidic
fn render_frame(frame: FrameArena): void {
    let entity_count: i32 = 100;
    let positions = frame.alloc_array<Vec3>(entity_count);
    let velocities = frame.alloc_array<Vec3>(entity_count);
    
    // Use positions and velocities...
    // All memory automatically freed when frame goes out of scope
}
```

**Usage:**
- `FrameArena` is a type that provides frame-scoped memory allocation
- `frame.alloc_array<T>(count)` allocates an array of `count` elements of type `T`
- Returns `std::vector<T>` in generated C++ code
- All allocations are automatically freed when the `FrameArena` goes out of scope

**Benefits:**
- Zero-allocation rendering (no heap allocations per frame)
- Automatic memory management (no manual free/delete)
- Cache-friendly (stack allocator with block-based allocation)

**Example:**
```heidic
fn main(): void {
    let frame: FrameArena;
    render_frame(frame);
    // frame is automatically cleaned up here
}
```

---

## Syntax

### Variables

```heidic
let x: f32 = 10.0;           // Explicit type
let y = 20.0;                // Type inference
let name: string = "Hello";  // String literal
```

### Functions

```heidic
fn add(a: f32, b: f32): f32 {
    return a + b;
}

fn greet(name: string): void {
    print("Hello, ");
    print(name);
    print("\n");
}
```

### Control Flow

#### If/Else
```heidic
if condition {
    // code
} else {
    // code
}
```

#### While Loop
```heidic
while condition {
    // code
}
```

#### Infinite Loop
```heidic
loop {
    // code
    if should_break {
        break;  // (not yet implemented)
    }
}
```

### Operators

#### Arithmetic
- `+` - Addition
- `-` - Subtraction
- `*` - Multiplication
- `/` - Division
- `%` - Modulo

#### Comparison
- `==` - Equal
- `!=` - Not equal
- `<` - Less than
- `<=` - Less than or equal
- `>` - Greater than
- `>=` - Greater than or equal

#### Logical
- `&&` - Logical AND
- `||` - Logical OR
- `!` - Logical NOT

#### Assignment
- `=` - Assignment

### Member Access

```heidic
let point: Point = Point { x: 1.0, y: 2.0 };
let x = point.x;             // Member access
let arr: [i32] = [1, 2, 3];
let first = arr[0];          // Array indexing
```

### Extern Functions

```heidic
extern fn glfwInit(): i32;
extern fn some_function(x: f32, y: f32): void from "library_name";
```

### Systems (ECS)

```heidic
system Physics {
    fn update(delta_time: f32): void {
        // System logic
    }
}
```

---

## Built-in Functions

### Print

```heidic
print(value);              // Print any value
print("Hello, World!\n");  // Print string
print(42);                 // Print number
```

**Signature:** `fn print(value: any): void`

### GLFW Functions

#### Window Management

```heidic
glfwInit(): i32
```
Initialize GLFW. Returns non-zero on success.

```heidic
glfwCreateWindow(width: i32, height: i32, title: string, monitor: GLFWwindow, share: GLFWwindow): GLFWwindow
```
Create a window. Returns window handle.

```heidic
glfwWindowShouldClose(window: GLFWwindow): i32
```
Check if window should close. Returns non-zero if should close.

```heidic
glfwPollEvents(): void
```
Poll for events.

```heidic
glfwGetKey(window: GLFWwindow, key: i32): i32
```
Get key state. Returns `GLFW_PRESS` or `GLFW_RELEASE`.

```heidic
glfwSetWindowShouldClose(window: GLFWwindow, value: i32): void
```
Set window should close flag.

```heidic
glfwDestroyWindow(window: GLFWwindow): void
```
Destroy a window.

```heidic
glfwTerminate(): void
```
Terminate GLFW.

```heidic
glfwWindowHint(hint: i32, value: i32): void
```
Set window hint.

#### GLFW Constants

```heidic
GLFW_KEY_SPACE = 32
GLFW_KEY_ESCAPE = 256
GLFW_KEY_ENTER = 257
GLFW_KEY_W = 87
GLFW_KEY_A = 65
GLFW_KEY_S = 83
GLFW_KEY_D = 68
GLFW_PRESS = 1
GLFW_RELEASE = 0
```

### ImGui Functions

```heidic
ImGui::Begin(name: string): bool
```
Begin an ImGui window. Returns true if window is open.

```heidic
ImGui::End(): void
```
End an ImGui window.

```heidic
ImGui::Text(text: string): void
```
Display text.

```heidic
ImGui::Button(label: string): bool
```
Display a button. Returns true if clicked.

```heidic
ImGui::NewFrame(): void
```
Start a new ImGui frame.

```heidic
ImGui::Render(): void
```
Render ImGui draw data.

**Note:** ImGui functions can also be called with `ImGui_` prefix (e.g., `ImGui_Begin`).

---

## Standard Library

### Math Types

The standard library provides math types that wrap GLM:

- `Vec2`, `Vec3`, `Vec4` - Vector types
- `Mat4` - 4x4 matrix type

These types are compatible with GLM operations in C++.

---

## EDEN Engine API

The EDEN Engine provides a high-level API for Vulkan rendering, windowing, and debugging.

### Window & Input

```heidic
extern fn heidic_glfw_init(): i32;
extern fn heidic_create_window(width: i32, height: i32, title: string): GLFWwindow;
extern fn heidic_destroy_window(window: GLFWwindow): void;
extern fn heidic_set_window_should_close(window: GLFWwindow, value: i32): void;
extern fn heidic_get_key(window: GLFWwindow, key: i32): i32;
extern fn heidic_glfw_terminate(): void;
extern fn heidic_glfw_vulkan_hints(): void;
```

### Renderer

```heidic
extern fn heidic_init_renderer(window: GLFWwindow): i32;
extern fn heidic_cleanup_renderer(): void;
extern fn heidic_window_should_close(window: GLFWwindow): i32;
extern fn heidic_poll_events(): void;
extern fn heidic_is_key_pressed(window: GLFWwindow, key: i32): i32;
```

### Frame Control

```heidic
extern fn heidic_begin_frame(): void;
extern fn heidic_end_frame(): void;
```

### Drawing

```heidic
extern fn heidic_draw_cube(x: f32, y: f32, z: f32, rx: f32, ry: f32, rz: f32, sx: f32, sy: f32, sz: f32): void;
```
Draw a cube at position (x, y, z) with rotation (rx, ry, rz) in degrees and scale (sx, sy, sz).

```heidic
extern fn heidic_draw_line(x1: f32, y1: f32, z1: f32, x2: f32, y2: f32, z2: f32, r: f32, g: f32, b: f32): void;
```
Draw a line from (x1, y1, z1) to (x2, y2, z2) with color (r, g, b).

```heidic
extern fn heidic_draw_model_origin(x: f32, y: f32, z: f32, rx: f32, ry: f32, rz: f32, length: f32): void;
```
Draw coordinate axes at position (x, y, z) with rotation (rx, ry, rz) and axis length.

```heidic
extern fn heidic_update_camera(px: f32, py: f32, pz: f32, rx: f32, ry: f32, rz: f32): void;
```
Update camera position (px, py, pz) and rotation (rx, ry, rz) in degrees.

### Mesh Loading

```heidic
extern fn heidic_load_ascii_model(filename: string): i32;
```
Load an ASCII model file. Returns mesh ID on success, -1 on failure.

```heidic
extern fn heidic_draw_mesh(mesh_id: i32, x: f32, y: f32, z: f32, rx: f32, ry: f32, rz: f32): void;
```
Draw a loaded mesh at position (x, y, z) with rotation (rx, ry, rz) in degrees.

### ImGui Wrappers

```heidic
extern fn heidic_imgui_init(window: GLFWwindow): void;
extern fn heidic_imgui_begin(name: string): void;
extern fn heidic_imgui_end(): void;
extern fn heidic_imgui_text(text: string): void;
extern fn heidic_imgui_text_float(label: string, value: f32): void;
extern fn heidic_imgui_drag_float(label: string, v: f32, speed: f32): f32;
```

### Vector Operations

```heidic
extern fn heidic_vec_copy(src: Vec3): Vec3;
extern fn heidic_attach_camera_translation(player_translation: Vec3): Vec3;
extern fn heidic_attach_camera_rotation(player_rotation: Vec3): Vec3;
```

**`heidic_vec_copy`**: Returns a copy of the source Vec3 value.
- `src`: Source Vec3 value to copy
- Returns: A copy of the Vec3

**`heidic_attach_camera_translation`**: Returns the player's translation for camera attachment.
- `player_translation`: Player's translation Vec3
- Returns: Player's translation (for assigning to camera)

**`heidic_attach_camera_rotation`**: Returns the player's rotation for camera attachment.
- `player_rotation`: Player's rotation Vec3
- Returns: Player's rotation (for assigning to camera)

**Example Usage:**
```heidic
struct Transform {
    translation: Vec3,
    rotation: Vec3
}

let camera: Transform = Transform(Vec3(0, 1000, 0), Vec3(-90, 0, 0));
let player: Transform = Transform(Vec3(0, 0, 0), Vec3(0, 45, 0));

// In game loop - make camera follow player
while true {
    // Copy player transform to camera
    camera.translation = heidic_attach_camera_translation(player.translation);
    camera.rotation = heidic_attach_camera_rotation(player.rotation);
    
    // Or use the generic copy function
    camera.translation = heidic_vec_copy(player.translation);
    camera.rotation = heidic_vec_copy(player.rotation);
    
    // Update camera with new transform
    heidic_update_camera(
        camera.translation.x, camera.translation.y, camera.translation.z,
        camera.rotation.x, camera.rotation.y, camera.rotation.z
    );
}
```

### Math Helpers

```heidic
extern fn heidic_convert_degrees_to_radians(degrees: f32): f32;
extern fn heidic_convert_radians_to_degrees(radians: f32): f32;
extern fn heidic_sin(radians: f32): f32;
extern fn heidic_cos(radians: f32): f32;
```

### Utility

```heidic
extern fn heidic_sleep_ms(ms: i32): void;
```
Sleep for specified milliseconds.

---

## Examples

### Hello World

```heidic
fn main(): void {
    print("Hello, HEIDIC!\n");
}
```

### Simple Math

```heidic
fn add(a: f32, b: f32): f32 {
    return a + b;
}

fn main(): void {
    let result: f32 = add(10.0, 20.0);
    print(result);
    print("\n");
}
```

### SOA Mesh

```heidic
mesh_soa Mesh {
    positions: [Vec3],
    uvs: [Vec2],
    colors: [Vec3],
    indices: [i32]
}

fn main(): void {
    // SOA layout: each field is a separate array
    // mesh.positions[0], mesh.uvs[0], mesh.colors[0] all refer to vertex 0
    print("SOA mesh defined\n");
}
```

### SOA Component

```heidic
component_soa Velocity {
    x: [f32],
    y: [f32],
    z: [f32]
}

fn main(): void {
    // SOA layout: velocities.x[i], velocities.y[i], velocities.z[i] for entity i
    // Cache-friendly for ECS iteration
    print("SOA component defined\n");
}
```

### EDEN Engine Example

```heidic
extern fn heidic_glfw_init(): i32;
extern fn heidic_create_window(width: i32, height: i32, title: string): GLFWwindow;
extern fn heidic_init_renderer(window: GLFWwindow): i32;
extern fn heidic_window_should_close(window: GLFWwindow): i32;
extern fn heidic_poll_events(): void;
extern fn heidic_begin_frame(): void;
extern fn heidic_end_frame(): void;
extern fn heidic_draw_cube(x: f32, y: f32, z: f32, rx: f32, ry: f32, rz: f32, sx: f32, sy: f32, sz: f32): void;
extern fn heidic_cleanup_renderer(): void;
extern fn heidic_destroy_window(window: GLFWwindow): void;
extern fn heidic_glfw_terminate(): void;

fn main(): void {
    if heidic_glfw_init() == 0 {
        return;
    }
    
    let window = heidic_create_window(800, 600, "HEIDIC Example");
    if heidic_init_renderer(window) != 0 {
        return;
    }
    
    while heidic_window_should_close(window) == 0 {
        heidic_poll_events();
        heidic_begin_frame();
        
        heidic_draw_cube(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0);
        
        heidic_end_frame();
    }
    
    heidic_cleanup_renderer();
    heidic_destroy_window(window);
    heidic_glfw_terminate();
}
```

---

## Compilation

HEIDIC compiles to C++ which is then compiled to native machine code:

```
game.hd → [HEIDIC Compiler] → game.cpp → [C++ Compiler] → game.exe
```

**Compile a file:**
```bash
heidic_v2 compile game.hd
```

**Compile and run:**
```bash
heidic_v2 run game.hd
```

The generated C++ code requires:
- C++17 compiler
- Vulkan SDK
- GLFW3
- GLM
- ImGui (optional)

---

## Notes

- **Coordinate System:** Right-handed, Y-up (1 unit = 1 cm)
- **Memory Management:** Manual (no garbage collection)
- **Performance:** Zero-cost abstractions, compiles to efficient C++ code
- **SOA Layout:** Preferred for CUDA/OptiX ray tracing and ECS iteration
- **Type Safety:** Statically typed with type checking

---

*Last updated: HEIDIC v2 with SOA support, compile-time shader embedding, and ECS query syntax*

