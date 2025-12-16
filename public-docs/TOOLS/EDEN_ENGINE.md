# EDEN Engine

**EDEN Engine** is the runtime game engine and standard library for HEIDIC. It provides the core systems needed to build games, including rendering, ECS, resource management, and more.

## Overview

EDEN Engine is built on top of Vulkan and provides a high-level API for game development. It's designed to work seamlessly with the HEIDIC language, offering zero-cost abstractions and direct access to low-level APIs when needed.

## Key Features

- **Vulkan Rendering** - High-performance graphics rendering
- **ECS System** - Entity Component System with SOA support
- **Resource Management** - Automatic loading and management of assets
- **Hot-Reload** - Runtime code reloading via CONTINUUM
- **Standard Library** - Built-in types, math functions, and utilities

## Architecture

EDEN Engine consists of:

- **`vulkan/`** - Vulkan renderer and graphics pipeline
- **`stdlib/`** - Standard library headers (math, resources, etc.)
- **ECS Runtime** - Entity Component System implementation
- **Resource System** - Asset loading and management

## Relationship to HEIDIC

EDEN Engine is the runtime that executes HEIDIC code. When you compile HEIDIC, it generates C++ code that uses EDEN Engine's APIs. This separation allows:

- HEIDIC to focus on game logic
- EDEN Engine to handle low-level systems
- Direct C++ interop when needed

See [EDEN vs HEIDIC](../EDEN_VS_HEIDIC.md) for more details.

## Getting Started

EDEN Engine is automatically available when you use HEIDIC. No separate installation is required.

### Example Usage

```heidic
// EDEN Engine provides the renderer and ECS
system render(query Position, Mesh) {
    for entity in query {
        // Render entity using EDEN Engine's renderer
        render_mesh(entity.Mesh, entity.Position);
    }
}
```

## Documentation

- [EDEN vs HEIDIC](../EDEN_VS_HEIDIC.md) - Understanding the relationship
- [ECS Queries Explained](../ECS/ECS_QUERIES_EXPLAINED.md) - Entity Component System
- [Resource System Plan](../RESOURCE_SYSTEM_PLAN.md) - Asset management
- [CONTINUUM Hot-Reload](../CONTINUUM-HOT%20RELOAD%20DOCS/HOT_RELOADING_EXPLAINED.md) - Runtime code reloading

## Source Code

EDEN Engine source code is located in:
- `vulkan/` - Vulkan renderer implementation
- `stdlib/` - Standard library headers

## Related Tools

- **[Electroscribe](ELECTROSCRIBE.md)** - IDE for HEIDIC development
- **[ESE](ESE.md)** - 3D model editor
- **[NEUROSHELL](../../NEUROSHELL/README.md)** - Lightweight UI system

