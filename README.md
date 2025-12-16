# HEIDIC

**HEIDIC** is a statically-typed, compiled programming language designed specifically for building high-performance game engines and game logic. It compiles to native C++17 code, providing zero-cost abstractions while maintaining a clean, game-focused syntax.

## What is HEIDIC?

HEIDIC is more than just a language—it's a complete ecosystem for game development:

- **HEIDIC Language** - The main programming language (compiles to C++17)
- **EDEN Engine** - The runtime game engine and standard library (Vulkan, ECS, resources)
- **Electroscribe IDE** - Integrated development environment with hot-reload
- **CONTINUUM** - Hot-reload system for rapid iteration
- **NEUROSHELL** - Lightweight in-game UI system
- **HEIROC** - Configuration scripting language (transpiles to HEIDIC)
- **ESE** - Echo Synapse Editor (3D model editor)

### Core Philosophy

HEIDIC is built with one primary goal: **making game engine development faster, safer, and more maintainable** without sacrificing performance. It combines:

- **Game-focused syntax** - Language features designed for common game development patterns
- **Zero-cost abstractions** - Compiles to efficient C++ code with no runtime overhead
- **Built-in engine features** - ECS, hot-reload, and graphics APIs integrated at the language level
- **Type safety** - Static typing with type inference to catch errors at compile time
- **No garbage collection** - Manual memory management for predictable performance

## ✨ Key Features

### Language Features (All Production-Ready!)

- ✅ **Query Iteration** - `for entity in q` syntax for ECS queries
- ✅ **SOA Components** - Transparent Structure-of-Arrays access (same syntax as AoS!)
- ✅ **Pattern Matching** - Rust-style `match` expressions
- ✅ **Optional Types** - Null-safe `?Type` syntax
- ✅ **Defer Statements** - Automatic cleanup with `defer`
- ✅ **String Interpolation** - `"Hello, {name}!"` syntax
- ✅ **Memory Ownership** - Compile-time validation prevents use-after-free bugs
- ✅ **Zero-Boilerplate** - Declarative pipelines and resources (400+ lines → 10 lines)
- ✅ **Enhanced Errors** - Clear error messages with context and suggestions
- ✅ **Hot-Reload** - Edit code, shaders, and components while running

### Engine Features

- ✅ **ECS System** - Built-in Entity Component System with SOA layouts
- ✅ **Vulkan Integration** - Direct access to Vulkan rendering APIs
- ✅ **Resource Management** - One-line texture, mesh, and audio loading
- ✅ **CONTINUUM Hot-Reload** - Runtime code reloading (systems, shaders, components)
- ✅ **Frame Arena** - Fast frame-scoped memory allocation

### Prototype Features (Framework Complete)

- ⚠️ **Automatic Bindless** - Infrastructure complete (~70%), shader integration pending
- ⚠️ **CUDA/OptiX Interop** - Prototype framework complete, code generation non-functional

## Quick Example

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
    print("Hello, HEIDIC!");
}
```

## The HEIDIC Ecosystem

### HEIDIC Language
The main programming language that compiles to C++17. Write game engine code with clean syntax, zero-cost abstractions, and built-in ECS support.

### EDEN Engine
The runtime game engine that provides:
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

### Electroscribe IDE
Integrated development environment with:
- Syntax highlighting for HEIDIC
- Project management
- Integrated build pipeline
- Hot-reload support
- C++ view (see generated code)

### CONTINUUM
Hot-reload system that allows you to:
- Edit systems without restarting (`@hot` systems)
- Edit shaders, pipelines rebuild automatically
- Change component structure, data migrates automatically

### NEUROSHELL
Lightweight in-game UI system (~1000 lines) optimized for performance, perfect for HUDs, menus, and in-game interfaces.

### HEIROC
Configuration scripting language that transpiles to HEIDIC. Perfect for asset configuration and simple entity behaviors.

### ESE (Echo Synapse Editor)
3D model editor built with HEIDIC, featuring mesh manipulation, gizmos, and HDM file format support.

## Getting Started

### Prerequisites

- **Rust** (for the HEIDIC compiler)
- **C++17 Compiler** (g++, clang++, or MSVC)
- **Vulkan SDK** (for Vulkan projects)
- **GLFW** (for windowing)
- **Python 3.7+** (for Electroscribe IDE)
- **Pygame** (for Electroscribe GUI)

### Build the Compiler

```bash
cargo build --release
```

### Using Electroscribe IDE

```bash
cd ELECTROSCRIBE
python main.py
```

Then:
1. Click `[N]` to create a new project
2. Write your HEIDIC code
3. Click `>` to compile and run

### Command Line Usage

```bash
# Compile a HEIDIC file
cargo run -- compile examples/hello.hd

# This generates hello.cpp, then compile it:
g++ -std=c++17 -O3 examples/hello.cpp -o hello
./hello
```

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
├── NEUROSHELL/            # Lightweight UI system
├── HEIROC/                # HEIROC transpiler
└── examples/              # Example projects
```

## Documentation

For complete documentation, see the [HEIDIC Documentation](DOCS/index.md) (when available locally).

Key documentation topics:
- [Language Specification](DOCS/HEIDIC/LANGUAGE.md)
- [Language Reference](DOCS/HEIDIC/LANGUAGE_REFERENCE.md)
- [ECS & Components](DOCS/ECS/ECS_QUERIES_EXPLAINED.md)
- [CONTINUUM Hot-Reload](DOCS/CONTINUUM-HOT%20RELOAD%20DOCS/HOT_RELOADING_EXPLAINED.md)
- [Features Showcase](DOCS/FEATURES.md)
- [Examples](DOCS/EXAMPLES.md)

## Performance

HEIDIC is designed for high-performance game development:

1. **Zero-cost abstractions** - Compiles to efficient C++ code
2. **No garbage collection** - Manual memory management
3. **Arena allocators** - Fast frame-scoped memory allocation
4. **ECS support** - Built-in Entity Component System for efficient game object management
5. **SOA layouts** - Structure-of-Arrays for GPU computing and cache efficiency

## Compilation Pipeline

```
game.hd → [HEIDIC Compiler] → game.cpp → [C++ Compiler] → game.exe
                                                              ↓
                                                       Uses EDEN Engine
                                                       (Runtime Library)
```

This two-stage compilation ensures maximum performance while maintaining a clean, game-focused syntax.

## Example Projects

Try these examples in Electroscribe IDE:

- **Query Iteration**: `ELECTROSCRIBE/PROJECTS/OLD PROJECTS/examples/query_iteration_example.hd`
- **Pattern Matching**: `ELECTROSCRIBE/PROJECTS/OLD PROJECTS/pattern_matching_test/pattern_matching_test.hd`
- **Optional Types**: `ELECTROSCRIBE/PROJECTS/OLD PROJECTS/optional_types_test/optional_types_test.hd`
- **Zero-Boilerplate Pipelines**: `ELECTROSCRIBE/PROJECTS/OLD PROJECTS/pipeline_test/pipeline_test.hd`

See [Examples](DOCS/EXAMPLES.md) for a complete list.

## License

[Add your license here]

## Contributing

[Add contribution guidelines here]
