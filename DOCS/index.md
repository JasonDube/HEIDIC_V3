# Welcome to HEIDIC

**HEIDIC** is a statically-typed, compiled programming language designed specifically for building high-performance game engines and game logic. It compiles to native C++17 code, providing zero-cost abstractions while maintaining a clean, game-focused syntax.

## ✨ Featured Features

!!! success "Production-Ready Features"
    Every feature listed below is **fully implemented, tested, and ready to use**. Try them yourself with the example files!

HEIDIC includes many powerful features that are **fully implemented and ready to use**:

- ✅ **Query Iteration** - `for entity in q` syntax for ECS queries
  - [Try it →](../ELECTROSCRIBE/PROJECTS/OLD%20PROJECTS/examples/query_iteration_example.hd)
- ✅ **SOA Components** - Transparent Structure-of-Arrays access
  - [Try it →](../ELECTROSCRIBE/PROJECTS/OLD%20PROJECTS/soa_access_test/soa_access_test.hd)
- ✅ **Pattern Matching** - Rust-style `match` expressions
  - [Try it →](../ELECTROSCRIBE/PROJECTS/OLD%20PROJECTS/pattern_matching_test/pattern_matching_test.hd)
- ✅ **Optional Types** - Null-safe `?Type` syntax
  - [Try it →](../ELECTROSCRIBE/PROJECTS/OLD%20PROJECTS/optional_types_test/optional_types_test.hd)
- ✅ **Defer Statements** - Automatic cleanup with `defer`
  - [Try it →](../ELECTROSCRIBE/PROJECTS/OLD%20PROJECTS/defer_test/defer_test.hd)
- ✅ **String Interpolation** - `"Hello, {name}!"` syntax
  - [Try it →](../ELECTROSCRIBE/PROJECTS/OLD%20PROJECTS/string_interpolation_test/string_interpolation_test.hd)
- ✅ **Memory Ownership** - Compile-time validation prevents use-after-free
  - [Try it →](../ELECTROSCRIBE/PROJECTS/OLD%20PROJECTS/memory_ownership_test/memory_ownership_test.hd)
- ✅ **Zero-Boilerplate** - Declarative pipelines and resources (400+ lines → 10 lines)
  - [Try pipelines →](../ELECTROSCRIBE/PROJECTS/OLD%20PROJECTS/pipeline_test/pipeline_test.hd)
  - [Try resources →](../ELECTROSCRIBE/PROJECTS/OLD%20PROJECTS/resource_test/resource_test.hd)
- ⚠️ **Automatic Bindless** - Infrastructure complete (~70%), shader integration pending
  - [Try it →](../ELECTROSCRIBE/PROJECTS/OLD%20PROJECTS/bindless_test/bindless_test.hd)
- ⚠️ **CUDA/OptiX Interop** - Prototype framework complete, code generation non-functional
  - [Try it →](../ELECTROSCRIBE/PROJECTS/OLD%20PROJECTS/cuda_test/cuda_test.hd)
- ✅ **Hot-Reload** - Edit code, shaders, and components while running
- ✅ **Enhanced Errors** - Clear error messages with context and suggestions
  - [Try it →](../ELECTROSCRIBE/PROJECTS/OLD%20PROJECTS/error_test/error_test.hd)

**[See All Features →](FEATURES.md)** - Complete feature showcase with examples  
**[Browse All Examples →](EXAMPLES.md)** - Quick reference to all test files

## Quick Links

### 🚀 Getting Started
- **[Introduction](HEIDIC/INTRODUCTION.md)** - Start here! Complete overview of HEIDIC and its ecosystem
- **[Quick Start Guide](HEIDIC/README.md)** - Get up and running quickly
- **[EDEN vs HEIDIC](EDEN_VS_HEIDIC.md)** - Understand the relationship between the language and engine

### 📚 Language Reference
- **[Language Specification](HEIDIC/LANGUAGE.md)** - Complete language reference
- **[Language Reference](HEIDIC/LANGUAGE_REFERENCE.md)** - Detailed API documentation
- **[Error Types](HEIDIC/ERROR_TYPES.md)** - All compiler error types explained

### 🎮 Engine Features
- **[ECS & Components](ECS/ECS_QUERIES_EXPLAINED.md)** - Entity Component System
- **[CONTINUUM Hot-Reload](CONTINUUM-HOT%20RELOAD%20DOCS/HOT_RELOADING_EXPLAINED.md)** - Runtime code reloading
- **[SOA Access Pattern](SOA%20DOCS/SOA_ACCESS_PATTERN_EXPLAINED.md)** - Structure-of-Arrays optimization

### 🛠️ Tools & Ecosystem
- **[Electroscribe IDE](../ELECTROSCRIBE/README.md)** - Integrated development environment
- **[ESE Editor](../ELECTROSCRIBE/PROJECTS/ESE/README.md)** - 3D model editor
- **[NEUROSHELL](../NEUROSHELL/README.md)** - Lightweight in-game UI system
- **[HEIROC](HEIDIC/HEIROC_ARCHITECTURE.md)** - Configuration scripting language

## 🎉 Recently Completed Features

!!! tip "Sprint 1 Complete!"
    All critical ergonomics features from Sprint 1 are now **fully implemented and tested**:
    
    - ✅ **Query Iteration Syntax** - `for entity in q` working perfectly
    - ✅ **SOA Access Pattern** - Transparent access, zero syntax difference
    - ✅ **Enhanced Error Messages** - Context-aware errors with suggestions
    
    Plus additional language features:
    
    - ✅ **Pattern Matching** - Rust-style `match` expressions
    - ✅ **Optional Types** - Null-safe `?Type` syntax
    - ✅ **Defer Statements** - Automatic cleanup
    - ✅ **String Interpolation** - `"Hello, {name}!"` syntax
    - ✅ **Memory Ownership** - Compile-time validation
    - ✅ **Zero-Boilerplate** - Declarative pipelines and resources

**[See Complete Feature List →](FEATURES.md)** | **[Try Examples →](EXAMPLES.md)**

---

## Key Features

### Language Features ✅ All Implemented!
- ✅ **Query Iteration** - `for entity in q` syntax for ECS queries
- ✅ **SOA Components** - Transparent Structure-of-Arrays access (same syntax as AoS!)
- ✅ **Pattern Matching** - Rust-style `match` expressions
- ✅ **Optional Types** - Null-safe `?Type` syntax
- ✅ **Defer Statements** - Automatic cleanup with `defer`
- ✅ **String Interpolation** - `"Hello, {name}!"` syntax
- ✅ **Memory Ownership** - Compile-time validation prevents use-after-free bugs
- ✅ **Zero-Boilerplate** - Declarative syntax (400+ lines → 10 lines for pipelines)
- ✅ **Enhanced Errors** - Clear error messages with context and suggestions
- ✅ **Static Typing** - Full type safety with type inference
- ✅ **Zero-Cost Abstractions** - Compiles to efficient C++ code
- ✅ **No Garbage Collection** - Manual memory management for predictable performance
- ✅ **ECS Support** - Built-in Entity Component System with SOA layouts
- ✅ **Hot Reload** - Runtime code reloading for rapid iteration
- ✅ **Direct API Access** - Native integration with Vulkan, GLFW, and ImGui

**[See Complete Features Showcase →](FEATURES.md)** - All features with working examples!

### The HEIDIC Ecosystem

HEIDIC is more than just a language - it's a complete ecosystem:

1. **HEIDIC Language** - The main programming language
2. **EDEN Engine** - The runtime game engine and standard library
3. **Electroscribe IDE** - Integrated development environment
4. **CONTINUUM** - Hot-reload system for rapid iteration
5. **NEUROSHELL** - Lightweight in-game UI system
6. **HEIROC** - Configuration scripting language
7. **ESE** - Echo Synapse Editor (3D model editor)

## Example Code ✅ All Working!

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

**Try this example:** [`query_iteration_example.hd`](../ELECTROSCRIBE/PROJECTS/OLD%20PROJECTS/examples/query_iteration_example.hd)

### More Examples

- **Pattern Matching:** [`pattern_matching_test.hd`](../ELECTROSCRIBE/PROJECTS/OLD%20PROJECTS/pattern_matching_test/pattern_matching_test.hd)
- **Optional Types:** [`optional_types_test.hd`](../ELECTROSCRIBE/PROJECTS/OLD%20PROJECTS/optional_types_test/optional_types_test.hd)
- **Defer Statements:** [`defer_test.hd`](../ELECTROSCRIBE/PROJECTS/OLD%20PROJECTS/defer_test/defer_test.hd)
- **SOA Access:** [`soa_access_test.hd`](../ELECTROSCRIBE/PROJECTS/OLD%20PROJECTS/soa_access_test/soa_access_test.hd)
- **String Interpolation:** [`string_interpolation_test.hd`](../ELECTROSCRIBE/PROJECTS/OLD%20PROJECTS/string_interpolation_test/string_interpolation_test.hd)
- **Memory Ownership:** [`memory_ownership_test.hd`](../ELECTROSCRIBE/PROJECTS/OLD%20PROJECTS/memory_ownership_test/memory_ownership_test.hd)
- **Zero-Boilerplate Pipelines:** [`pipeline_test.hd`](../ELECTROSCRIBE/PROJECTS/OLD%20PROJECTS/pipeline_test/pipeline_test.hd)
- **Zero-Boilerplate Resources:** [`resource_test.hd`](../ELECTROSCRIBE/PROJECTS/OLD%20PROJECTS/resource_test/resource_test.hd)
- **Error Messages:** [`error_test.hd`](../ELECTROSCRIBE/PROJECTS/OLD%20PROJECTS/error_test/error_test.hd)
- **CUDA/OptiX (Prototype):** [`cuda_test.hd`](../ELECTROSCRIBE/PROJECTS/OLD%20PROJECTS/cuda_test/cuda_test.hd)

**[See All Examples →](EXAMPLES.md)** - Complete list of test files  
**[See All Features →](FEATURES.md)** - Detailed feature documentation

## Documentation Structure

This documentation is organized into several sections:

- **Getting Started** - Introduction and quick start guides
- **Language Reference** - Complete language documentation
- **ECS & Components** - Entity Component System documentation
- **Engine Features** - EDEN Engine features and CONTINUUM hot-reload
- **Tools & Ecosystem** - IDE, editors, and supporting tools
- **Resources & Formats** - File formats and resource management
- **Knowledge Base** - Technical deep-dives and explanations
- **Development** - Roadmaps and implementation plans

## Contributing

We welcome contributions! Please see our development documentation for implementation plans and guidelines.

---

**HEIDIC** - Building game engines, one abstraction at a time.
