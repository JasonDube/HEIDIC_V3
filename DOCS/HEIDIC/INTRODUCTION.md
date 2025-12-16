# HEIDIC: Introduction and Features

!!! success "Production-Ready Features"
    HEIDIC includes many **fully implemented** language features ready to use:
    
    - ✅ Query Iteration (`for entity in q`)
    - ✅ SOA Components (transparent access)
    - ✅ Pattern Matching (`match` expressions)
    - ✅ Optional Types (`?Type` syntax)
    - ✅ Defer Statements (automatic cleanup)
    - ✅ String Interpolation (`"Hello, {name}!"`)
    - ✅ Memory Ownership (compile-time validation)
    - ✅ Zero-Boilerplate (declarative pipelines & resources)
    - ✅ Enhanced Error Messages (context-aware)
    
    **[Try Examples →](../EXAMPLES.md)** | **[See Features →](../FEATURES.md)**

## What is HEIDIC?

**HEIDIC** is a statically-typed, compiled programming language designed specifically for building high-performance game engines and game logic. It compiles to native C++17 code, providing zero-cost abstractions while maintaining a clean, game-focused syntax.

### Core Philosophy

HEIDIC is built with one primary goal: **making game engine development faster, safer, and more maintainable** without sacrificing performance. It combines:

- **Game-focused syntax** - Language features designed for common game development patterns
- **Zero-cost abstractions** - Compiles to efficient C++ code with no runtime overhead
- **Built-in engine features** - ECS, hot-reload, and graphics APIs integrated at the language level
- **Type safety** - Static typing with type inference to catch errors at compile time
- **No garbage collection** - Manual memory management for predictable performance

### Primary Use Cases

- ✅ **Game Engine Development** - Rendering systems, ECS frameworks, resource managers, physics engines
- ✅ **Game Logic** - Gameplay systems, AI, game state management
- ❌ **Not a scripting language** - HEIDIC is a full compiled language, not a lightweight scripting solution

> **Note:** If you want to use EDEN Engine with a simple scripting language, see **[HEIROC](#heiroc---configuration-scripting-language)** - a minimal scripting DSL that transpiles to HEIDIC, perfect for asset configuration and simple entity behaviors.

### EDEN Engine - The Runtime Game Engine

**EDEN Engine** is the actual game engine that HEIDIC compiles to use. It's the runtime C++ engine that provides:

- **Vulkan Rendering** - Low-level graphics API integration
- **ECS Runtime** - Entity Component System implementation
- **Resource Management** - Texture, mesh, and audio loading
- **CONTINUUM Hot-Reload** - Runtime code reloading system
- **Standard Library** - Math, utilities, and engine APIs

**The Relationship:**
```
HEIDIC Code (.hd) → [HEIDIC Compiler] → C++ Code → [C++ Compiler] → Executable
                                                          ↓
                                                   Uses EDEN Engine
                                                   (Runtime Library)
```

HEIDIC is the **language** you write in, while EDEN Engine is the **runtime engine** that executes your code. When you write HEIDIC code, it compiles to C++ that calls EDEN Engine APIs. For more details, see [EDEN vs HEIDIC](../EDEN_VS_HEIDIC.md).

---

## The HEIDIC Ecosystem

HEIDIC is more than just a language - it's a complete ecosystem for game development:

### Core Components

1. **HEIDIC Language** - The main programming language (this document)
2. **EDEN Engine** - The runtime game engine and standard library
3. **Electroscribe IDE** - Integrated development environment
4. **CONTINUUM** - Hot-reload system for rapid iteration

### Additional Tools & Libraries

#### NEUROSHELL - Lightweight In-Game UI System

**NEUROSHELL** is a fast, lightweight UI system designed for in-game interfaces. It's an alternative to ImGui for runtime game UI, optimized for performance and simplicity.

**Features:**
- **Lightweight** - ~1000 lines vs ImGui's 20k+
- **Fast compile** - No heavy dependencies, pure C++/HEIDIC
- **Fast runtime** - Batched rendering, optimized for games
- **Animated textures** - Sprite sheets and frame timing
- **Clean API** - Separate from engine helpers, minimal interface

**Usage:**
```heidic
// Enable NEUROSHELL in project config
enable_neuroshell=true

// Initialize
if (neuroshell_is_enabled()) {
    neuroshell_init(window);
}

// Create UI elements
let panel = neuroshell_create_panel(10, 10, 200, 100);
let button = neuroshell_create_button(20, 20, 100, 30, "ui/button.png");

// In render loop
neuroshell_update(delta_time);
neuroshell_render(commandBuffer);
```

**When to use:**
- In-game HUDs and UI overlays
- Menu systems
- Crosshairs and reticles
- Health bars and status displays
- When you need lightweight, fast UI without ImGui's overhead

#### HEIROC - Configuration Scripting Language

**HEIROC** (HEIDIC Engine Interface for Rapid Object Configuration) is a minimal scripting language that transpiles to HEIDIC. It's designed for simple configuration and asset scripting.

**Philosophy:**
- **HEIDIC** - Full programming language for engines and game logic
- **HEIROC** - Minimal configuration/scripting DSL that transpiles to HEIDIC
- **Assets** - Packed with HEIROC scripts (like HDM files with embedded scripts)
- **Level Editor** - Assigns HEIROC scripts to assets in the scene

**Features:**
- **Simple syntax** - No includes, assumes 3D context
- **Transpiles to HEIDIC** - Automatically generates full HEIDIC code
- **Asset scripting** - Attach scripts to 3D models and game objects
- **Hot-reload support** - Scripts can be reloaded at runtime

**Example:**
```heiroc
// HEIROC script for a door entity
PANEL* door_panel {
    x: 0,
    y: 0,
    width: 100,
    height: 200
}

main_loop() {
    while true {
        // Door logic here
        if player_nearby {
            open_door();
        }
    }
}
```

**When to use:**
- Asset configuration scripts
- Level-specific entity behaviors
- Simple game object logic
- When you need a lightweight scripting solution that compiles to HEIDIC

#### ESE - Echo Synapse Editor

**ESE** (Echo Synapse Editor) is a 3D model editor and viewer built with HEIDIC. It's a powerful tool for viewing, editing, and manipulating 3D models.

**Features:**
- **OBJ Model Viewer** - Load and view 3D models with textures
- **Mesh Editing** - Direct vertex, edge, face, and quad manipulation
- **3D Gizmos** - Move, scale, and rotate tools for mesh editing
- **Extrusion** - Extrude faces and quads to create new geometry
- **Edge Loop Insertion** - Add edge loops for subdivision
- **HDM File Support** - Native support for HEIDIC's HDM model format
- **ImGui Integration** - Full menu system and UI
- **Undo/Redo** - Full history support for mesh operations

**Editing Modes:**
- **Vertex Mode** - Select and manipulate individual vertices
- **Edge Mode** - Select and manipulate edges
- **Face Mode** - Select and manipulate triangular faces
- **Quad Mode** - Select and manipulate quads (reconstructed from triangles)

**Controls:**
- Mouse: Left-drag rotate, Right-drag pan, Wheel zoom
- 1-4: Switch between vertex/edge/face/quad modes
- W: Extrude mode (in quad mode)
- I: Insert edge loop (in edge mode)
- Ctrl+Z: Undo
- Gizmo: Click and drag X/Y/Z axes for transformations

**When to use:**
- Viewing and inspecting 3D models
- Editing mesh geometry
- Creating and modifying game assets
- Testing HDM file format
- Prototyping 3D models

**Building:**
ESE is a HEIDIC project that can be built and run through Electroscribe IDE.

---

## Key Features Overview

> **✨ New:** Many features are **fully implemented and tested!** See the [Features Showcase](../FEATURES.md) for complete examples and test files you can try right now.

### 1. Language Features

#### Static Typing with Type Inference
```heidic
let x: f32 = 10.0;        // Explicit type
let y = 20.0;             // Type inference (f32)
let name = "Player";      // Type inference (string)
```

#### Zero-Cost Abstractions
HEIDIC compiles directly to C++17, ensuring that high-level language features have no runtime overhead. What you write in HEIDIC is what you get in C++.

#### No Garbage Collection
Manual memory management provides predictable performance characteristics essential for real-time game engines.

#### Modern Control Flow
```heidic
// If/else
if condition {
    // code
} else {
    // code
}

// While loops
while condition {
    // code
}

// Infinite loops
loop {
    // code
}

// Pattern matching ✅ IMPLEMENTED
match value {
    VK_SUCCESS => { print("Success!\n"); }
    VK_ERROR_OUT_OF_MEMORY => { print("Out of memory\n"); }
    _ => { print("Other error\n"); }
}

// Defer statements ✅ IMPLEMENTED (execute on scope exit)
{
    let resource = acquire();
    defer release(resource);  // Always executes when scope exits
    // use resource
}
```

**Try it:** See [Features Showcase](../FEATURES.md) for working examples!

---

### 2. Entity Component System (ECS)

HEIDIC has **first-class ECS support** built into the language. No need for external libraries or complex setup.

#### Components

**Standard Components (AoS - Array of Structures):**
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

// Components support default values
component Transform {
    position: Vec3,
    rotation: Quat,
    scale: Vec3 = Vec3(1, 1, 1)  // default value
}
```

**SOA Components (Structure-of-Arrays) for Performance:**
```heidic
component_soa Velocity {
    x: [f32],    // Array of X velocities
    y: [f32],    // Array of Y velocities
    z: [f32]     // Array of Z velocities
}
```

SOA layout provides:
- **Cache-friendly iteration** - Accessing the same field across many entities
- **Vectorization** - SIMD instructions can process multiple values
- **GPU-friendly** - CUDA/OptiX prefer SOA for parallel processing

#### Systems and Queries ✅ IMPLEMENTED

```heidic
system physics(query Position, Velocity) {
    for entity in query {
        entity.Position.x += entity.Velocity.x * delta_time;
        entity.Position.y += entity.Velocity.y * delta_time;
        entity.Position.z += entity.Velocity.z * delta_time;
    }
}
```

The compiler automatically handles the difference between AoS and SOA components - you write the same code regardless!

**Try it:** 
- [`query_iteration_example.hd`](../../ELECTROSCRIBE/PROJECTS/OLD%20PROJECTS/examples/query_iteration_example.hd)
- [`mixed_aos_soa_query.hd`](../../ELECTROSCRIBE/PROJECTS/OLD%20PROJECTS/examples/mixed_aos_soa_query.hd)

#### Hot-Reloadable Systems

Mark systems with `@hot` to enable runtime code reloading:

```heidic
@hot
system update_health(query Health) {
    for entity in query {
        entity.Health.current = min(entity.Health.current + 1.0, entity.Health.max);
    }
}
```

Edit and save - the system reloads automatically without restarting the game!

---

### 3. CONTINUUM: Hot-Reload System

HEIDIC's **CONTINUUM** system provides comprehensive hot-reloading capabilities:

#### System Hot-Reload ✅
- Mark systems with `@hot`
- Edit code and save
- System reloads automatically via DLL swapping
- Game continues running with new code

#### Shader Hot-Reload ✅
- Edit shader files (GLSL/SPIR-V)
- Shaders recompile and pipelines rebuild automatically
- Visual changes appear instantly

#### Component Hot-Reload 🔄 (In Development)
- Change component structure while game runs
- Automatic data migration
- Entities preserve their data
- Type conversions handled automatically

**Example:**
```heidic
// Initial component
@hot
component Player {
    health: f32,
    position: Vec3,
}

// Change it while game runs:
@hot
component Player {
    health: f32,
    position: Vec3,
    mana: f32,  // New field added!
}

// All existing Player entities automatically get mana: 0.0
// Game never stopped!
```

---

### 4. Type System

#### Primitive Types

| Type | Description | C++ Equivalent |
|------|-------------|----------------|
| `i32` | 32-bit signed integer | `int32_t` |
| `i64` | 64-bit signed integer | `int64_t` |
| `f32` | 32-bit floating point | `float` |
| `f64` | 64-bit floating point | `double` |
| `bool` | Boolean (`true` or `false`) | `bool` |
| `string` | String type | `std::string` |
| `void` | No return value | `void` |

#### Composite Types

**Arrays:**
```heidic
let numbers: [i32];           // Dynamic array of integers
let positions: [Vec3];        // Dynamic array of 3D vectors
```

**Structs:**
```heidic
struct Point {
    x: f32,
    y: f32
}
```

**Type Aliases:**
```heidic
type ImageView = VkImageView;
type DescriptorSet = VkDescriptorSet;
type CommandBuffer = VkCommandBuffer;
```

#### Built-in Math Types

HEIDIC includes optimized math types for game development:

```heidic
let pos: Vec2 = Vec2(10.0, 20.0);
let position: Vec3 = Vec3(0.0, 1.0, 0.0);
let color: Vec4 = Vec4(1.0, 0.0, 0.0, 1.0);
let transform: Mat4 = Mat4::identity();
let rotation: Quat = Quat::identity();
```

---

### 5. Standard Library & Engine Integration

#### Direct API Access

HEIDIC provides native integration with industry-standard APIs:

- **Vulkan** - Low-level graphics API for maximum performance
- **GLFW** - Window and input management
- **ImGui** - Immediate-mode GUI for tools and debugging

#### Resource Management

Built-in support for game resources:

```heidic
// Texture loading
let texture = load_texture("assets/texture.png");

// Mesh loading
let mesh = load_mesh("assets/model.obj");

// Audio support
let sound = load_audio("assets/sound.wav");
```

#### Math Library

Comprehensive math operations:
- Vector operations (dot, cross, normalize, etc.)
- Matrix operations (multiply, inverse, transpose, etc.)
- Quaternion operations (rotation, slerp, etc.)

---

### 6. Electroscribe IDE

HEIDIC includes **Electroscribe**, a lightweight Pygame-based integrated development environment:

#### Features

- **Syntax Highlighting** - Full HEIDIC syntax support with color coding
- **Project Management** - Create and manage HEIDIC projects easily
- **Integrated Build Pipeline** - Compile, build, and run with one click
- **Hot-Reload Support** - Automatic reloading of `@hot` systems
- **C++ View** - Toggle between HEIDIC source and generated C++ code
- **Output Panels** - Separate panels for compiler output and program terminal
- **File Watching** - Automatic recompilation on file changes (optional)

#### Workflow

1. Create a new project with the `+` button
2. Write HEIDIC code with syntax highlighting
3. Click `>` to compile, build, and run
4. Edit `@hot` systems and save - they reload automatically!
5. Toggle C++ view to see generated code

---

### 7. Performance Features

HEIDIC is designed for high-performance game development:

#### Zero-Cost Abstractions
- Compiles to efficient C++ code
- No runtime overhead for language features
- Direct mapping to C++ constructs

#### Manual Memory Management
- No garbage collection pauses
- Predictable performance characteristics
- Arena allocators for fast game object allocation

#### ECS Optimization
- Built-in Entity Component System
- SOA (Structure-of-Arrays) layout support
- Cache-friendly data access patterns
- GPU/CUDA interop ready

#### Compilation Pipeline

```
game.hd → [HEIDIC Compiler] → game.cpp → [C++ Compiler] → game.exe
```

Two-stage compilation ensures maximum performance while maintaining clean syntax.

---

### 8. Error Handling

HEIDIC provides comprehensive compile-time error checking with helpful messages:

#### Error Types

The compiler detects and reports:
- Undefined variables
- Type mismatches
- Wrong argument counts/types
- Invalid control flow conditions
- Array/component access errors
- And many more...

#### Error Messages

All errors include:
- **Source location** (file, line, column)
- **Context** (surrounding code)
- **Clear description** of the problem
- **Helpful suggestions** on how to fix it

**Example:**
```
Error at test.hd:18:23:
 17 | fn test_undefined_variable(): void {
 18 |     let result: i32 = undefined_var + 5;
                           ^^^^^^^^^^^^^
 19 | }

Undefined variable: 'undefined_var'
💡 Suggestion: Did you mean to declare it first? Use: let undefined_var: Type = value;
```

---

### 9. Use Cases

#### Game Engine Development

HEIDIC excels at building:
- **Rendering Systems** - Vulkan-based graphics pipelines
- **ECS Frameworks** - Entity Component System implementations
- **Resource Managers** - Texture, mesh, and audio loading
- **Physics Engines** - Collision detection and response
- **Input Systems** - Keyboard, mouse, and gamepad handling

#### Game Logic

HEIDIC can also be used for:
- **Gameplay Systems** - Player movement, combat, interactions
- **AI Systems** - Behavior trees, state machines
- **Game State Management** - Menus, levels, save/load
- **UI Systems** - ImGui-based interfaces

---

### 10. Getting Started

#### Prerequisites

- **Rust** (for the HEIDIC compiler)
- **Cargo** (Rust package manager)
- **g++** (C++17 compiler)
- **Vulkan SDK** (for Vulkan projects)
- **GLFW** (for windowing)
- **Python 3.7+** (for Electroscribe IDE)
- **Pygame** (for Electroscribe GUI)

#### Build the Compiler

```bash
cargo build --release
```

#### Compile a HEIDIC File

```bash
cargo run -- compile examples/hello.hd
```

This generates `hello.cpp` in the same directory.

#### Compile the Generated C++

```bash
g++ -std=c++17 -O3 examples/hello.cpp -o hello
./hello
```

#### Use Electroscribe IDE

```bash
cd ELECTROSCRIBE
python main.py
```

Create a project, write code, and run with one click!

---

### 11. Example Code

#### Simple Hello World

```heidic
fn main(): void {
    print("Hello, HEIDIC!");
}
```

#### ECS Example ✅ FULLY IMPLEMENTED

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

@hot
system physics(query Position, Velocity) {
    for entity in query {
        entity.Position.x += entity.Velocity.x * delta_time;
        entity.Position.y += entity.Velocity.y * delta_time;
        entity.Position.z += entity.Velocity.z * delta_time;
    }
}

fn main(): void {
    // Create entity
    let entity = create_entity();
    
    // Add components
    add_component(entity, Position { x: 0.0, y: 0.0, z: 0.0 });
    add_component(entity, Velocity { x: 1.0, y: 0.0, z: 0.0 });
    
    // Game loop
    loop {
        physics();
        render();
    }
}
```

**Try this example:**
- `ELECTROSCRIBE/PROJECTS/OLD PROJECTS/examples/query_iteration_example.hd` - Complete working example
- `ELECTROSCRIBE/PROJECTS/OLD PROJECTS/query_test/query_test.hd` - Comprehensive test suite

Open these files in Electroscribe IDE to try them!

#### Pattern Matching Example ✅ IMPLEMENTED

```heidic
fn handle_result(result: VkResult): void {
    match result {
        VK_SUCCESS => {
            print("Success!\n");
        }
        VK_ERROR_OUT_OF_MEMORY => {
            print("Out of memory\n");
        }
        _ => {
            print("Other error\n");
        }
    }
}
```

**Try it:** `ELECTROSCRIBE/PROJECTS/OLD PROJECTS/pattern_matching_test/pattern_matching_test.hd`

#### Optional Types Example ✅ IMPLEMENTED

```heidic
fn load_mesh(path: string): ?Mesh {
    return load_mesh_file(path);  // Might return null
}

fn main(): void {
    let mesh: ?Mesh = load_mesh("model.obj");
    if mesh {
        draw(mesh.unwrap());  // Safe unwrap
    }
}
```

**Try it:** [`optional_types_test.hd`](../../ELECTROSCRIBE/PROJECTS/OLD%20PROJECTS/optional_types_test/optional_types_test.hd)

---

## 🎯 Try These Features Now!

All features shown here are **fully implemented and tested**. You can try them right now:

1. **Open Electroscribe IDE:**
   ```bash
   cd ELECTROSCRIBE
   python main.py
   ```

2. **Load a test project:**
   - Click `[O]` to open
   - Navigate to `ELECTROSCRIBE/PROJECTS/OLD PROJECTS/`
   - Open any `.hd` test file

3. **Run it:**
   - Click `>` to compile and run
   - See the results!

**See [Features Showcase](../FEATURES.md) for complete examples and test files!**

---

## Project Structure

```
HEIDIC/
├── src/                    # HEIDIC compiler (Rust)
│   ├── main.rs            # Compiler entry point
│   ├── lexer.rs           # Lexical analysis
│   ├── parser.rs          # Syntax parsing
│   ├── ast.rs             # Abstract syntax tree
│   ├── type_checker.rs    # Type checking
│   └── codegen.rs         # C++ code generation
├── ELECTROSCRIBE/         # Integrated Development Environment
│   ├── main.py            # IDE entry point
│   └── PROJECTS/          # HEIDIC projects
│       ├── ESE/           # Echo Synapse Editor (3D model editor)
│       └── ...
├── stdlib/                # Standard library headers
│   ├── math.h             # Math types and functions
│   ├── mesh_resource.h    # Mesh loading
│   ├── texture_resource.h # Texture loading
│   └── ...
├── vulkan/                # EDEN Engine runtime
│   ├── eden_vulkan_helpers.cpp  # Vulkan renderer
│   └── eden_vulkan_helpers.h
├── NEUROSHELL/            # Lightweight in-game UI system
│   ├── include/           # NEUROSHELL headers
│   ├── src/               # NEUROSHELL implementation
│   └── shaders/           # UI shaders
├── heiroc_transpiler/     # HEIROC → HEIDIC transpiler
│   ├── src/               # Transpiler source (Rust)
│   └── ...
├── examples/              # Example projects
│   ├── hello.hd
│   ├── spinning_cube/
│   └── ...
└── DOCS/                  # Documentation
    └── HEIDIC/            # Language documentation
```

---

## Documentation

### Core Language Documentation
- [Language Specification](LANGUAGE.md) - Complete language reference
- [Language Reference](LANGUAGE_REFERENCE.md) - Detailed API documentation
- [Error Types](ERROR_TYPES.md) - All compiler error types explained
- [SOA Access Pattern](SOA_ACCESS_PATTERN_EXPLAINED.md) - Understanding SOA components

### Engine & Tools Documentation
- [EDEN vs HEIDIC](../EDEN_VS_HEIDIC.md) - Understanding the relationship
- [Electroscribe IDE](../../ELECTROSCRIBE/README.md) - IDE documentation
- [Hot Reload Explained](../CONTINUUM-HOT%20RELOAD%20DOCS/HOT_RELOADING_EXPLAINED.md) - CONTINUUM system details
- [Component Hot-Load](../CONTINUUM-HOT%20RELOAD%20DOCS/component_hotload_explained.md) - Component hot-reloading

### Ecosystem Documentation
- [NEUROSHELL README](../../NEUROSHELL/README.md) - Lightweight in-game UI system
- [HEIROC Architecture](HEIROC_ARCHITECTURE.md) - HEIROC scripting language
- [ESE README](../../ELECTROSCRIBE/PROJECTS/ESE/README.md) - Echo Synapse Editor (3D model editor)

---

## Why HEIDIC?

### Compared to C++

- ✅ **Game-focused syntax** - Built for game development patterns
- ✅ **Built-in ECS** - No need for external libraries
- ✅ **Hot-reload** - Iterate without restarting
- ✅ **Type safety** - Catch errors at compile time
- ✅ **Zero-cost** - Same performance as C++

### Compared to Scripting Languages

- ✅ **Full compiled language** - Maximum performance
- ✅ **No runtime overhead** - Direct C++ compilation
- ✅ **Type safety** - Static typing prevents runtime errors
- ✅ **Engine integration** - Built for game engines, not scripting

---

## License

[Add your license here]

## Contributing

[Add contribution guidelines here]

---

**HEIDIC** - Building game engines, one abstraction at a time.

