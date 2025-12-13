# HEIDIC

A statically-typed, compiled language designed for building high-performance game engines. HEIDIC compiles to native C++17 code for maximum performance.

## Overview

HEIDIC is a programming language designed for **building game engines and game logic**. It provides zero-cost abstractions, direct integration with Vulkan and GLFW, and built-in support for Entity Component Systems (ECS). It features an integrated development environment (Electroscribe) with hot-reload capabilities, making it ideal for rapid game development.

**Primary Focus:**
- **HEIDIC**: Optimized for building game engines (rendering systems, ECS frameworks, resource managers, physics engines)
- **Also**: Can be used for game logic (gameplay systems, AI, game state management)
- **Not**: A lightweight scripting language (like Lua or Python) - it's a full compiled language

HEIDIC is a complete programming language that can be used for both engine code and game logic, but it's particularly well-suited for engine development due to its performance characteristics and built-in engine features.

## Key Features

### Language Features

- **Static Typing**: Full type safety with type inference
- **Zero-Cost Abstractions**: Compiles to efficient C++ code
- **No Garbage Collection**: Manual memory management for predictable performance
- **ECS Support**: Built-in Entity Component System with SOA (Structure-of-Arrays) layouts
- **Hot Reload**: Runtime code reloading for rapid iteration
- **Direct API Access**: Native integration with Vulkan, GLFW, and ImGui

### Type System

#### Primitive Types
- `i32` - 32-bit signed integer
- `i64` - 64-bit signed integer
- `f32` - 32-bit floating point
- `f64` - 64-bit floating point
- `bool` - Boolean (`true` or `false`)
- `string` - String type
- `void` - No return value

#### Composite Types
- **Arrays**: `[T]` - Dynamic array of type T
- **Structs**: User-defined data structures
- **Components**: Special structs for ECS with default value support
- **Math Types**: `Vec2`, `Vec3`, `Vec4`, `Mat4`, `Quat`
- **Type Aliases**: Create shorter names for existing types

#### ECS Components

```heidic
component Position {
    x: f32,
    y: f32,
    z: f32
}

component Transform {
    position: Vec3,
    rotation: Quat,
    scale: Vec3 = Vec3(1, 1, 1)  // default value
}
```

### Control Flow

- `if/else` statements
- `while` loops
- `loop` (infinite loops)
- `match` expressions (pattern matching)
- `defer` statements (execute on scope exit)

### Standard Library

The language includes built-in support for:
- **Math**: Vector and matrix operations (`Vec2`, `Vec3`, `Vec4`, `Mat4`, `Quat`)
- **Resources**: Texture, mesh, and audio resource management
- **ECS**: Entity Component System with queries and systems
- **Rendering**: Vulkan renderer integration
- **Windowing**: GLFW window management
- **UI**: ImGui integration for in-game UI

## Electroscribe IDE

HEIDIC includes **Electroscribe**, a lightweight Pygame-based IDE with:

- **Syntax Highlighting**: Full HEIDIC syntax support
- **Project Management**: Create and manage HEIDIC projects
- **Integrated Build Pipeline**: Compile, build, and run with one click
- **Hot-Reload Support**: Automatic reloading of `@hot` systems
- **C++ View**: Toggle between HEIDIC source and generated C++
- **Output Panels**: Separate panels for compiler output and program terminal
- **File Watching**: Automatic recompilation on file changes

### Running Electroscribe

```bash
cd ELECTROSCRIBE
python main.py
```

## Building

### Prerequisites

- **Rust** (for the HEIDIC compiler)
- **Cargo** (Rust package manager)
- **g++** (C++17 compiler)
- **Vulkan SDK** (for Vulkan projects)
- **GLFW** (for windowing)
- **Python 3.7+** (for Electroscribe IDE)
- **Pygame** (for Electroscribe GUI)

### Build the Compiler

```bash
cargo build --release
```

This creates the HEIDIC compiler executable.

### Compile a HEIDIC File

```bash
cargo run -- compile examples/hello.hd
```

This generates a `hello.cpp` file in the same directory.

### Compile the Generated C++

```bash
g++ -std=c++17 -O3 examples/hello.cpp -o hello
./hello
```

Or use Electroscribe IDE for an integrated build experience.

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
├── stdlib/                # Standard library headers
│   ├── math.h             # Math types and functions
│   ├── mesh_resource.h    # Mesh loading
│   ├── texture_resource.h # Texture loading
│   ├── audio_resource.h   # Audio support
│   └── ...
├── vulkan/                # EDEN Engine runtime
│   ├── eden_vulkan_helpers.cpp  # Vulkan renderer
│   └── eden_vulkan_helpers.h
├── examples/              # Example projects
│   ├── hello.hd
│   ├── spinning_cube/
│   └── ...
└── DOCS/                  # Documentation
    └── HEIDIC/            # Language documentation
```

## Example

```heidic
fn main(): void {
    let x: f32 = 10.0;
    let y = 20.0;  // Type inference
    
    print("Hello, HEIDIC!");
    print(x + y);
}
```

## Documentation

- [Language Specification](DOCS/HEIDIC/LANGUAGE.md)
- [Language Reference](DOCS/HEIDIC/LANGUAGE_REFERENCE.md)
- [EDEN Engine vs HEIDIC](DOCS/EDEN_VS_HEIDIC.md)
- [Electroscribe IDE](ELECTROSCRIBE/README.md)

## Performance

HEIDIC is designed for high-performance game development:

1. **Zero-cost abstractions**: Compiles to efficient C++ code
2. **No garbage collection**: Manual memory management
3. **Arena allocators**: Fast memory allocation for game objects
4. **ECS support**: Built-in Entity Component System for efficient game object management
5. **SOA layouts**: Structure-of-Arrays for GPU computing and cache efficiency

## Compilation Pipeline

```
game.hd → [HEIDIC Compiler] → game.cpp → [C++ Compiler] → game.exe
```

This two-stage compilation ensures maximum performance while maintaining a clean, game-focused syntax.

## License

[Add your license here]

## Contributing

[Add contribution guidelines here]
