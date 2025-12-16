# HEIDIC Language Specification

## Overview

HEIDIC is a statically-typed, compiled language designed for building high-performance game engines. It compiles to native C++ code for maximum performance.

## Types

### Primitive Types

- `i32` - 32-bit signed integer
- `i64` - 64-bit signed integer
- `f32` - 32-bit floating point
- `f64` - 64-bit floating point
- `bool` - Boolean (`true` or `false`)
- `string` - String type
- `void` - No return value

### Composite Types

- Arrays: `[T]` - Dynamic array of type T
- Structs: User-defined data structures
- Components: Special structs for ECS (Entity Component System)

## Syntax

### Variables

```heidic
let x: f32 = 10.0;
let y = 20.0;  // Type inference
```

### Functions

```heidic
fn add(a: f32, b: f32): f32 {
    return a + b;
}
```

### Structs

```heidic
struct Point {
    x: f32,
    y: f32
}
```

### Components

Components are special structs used in Entity Component Systems:

```heidic
component Position {
    x: f32,
    y: f32,
    z: f32
}
```

Components support default values for fields:

```heidic
component Transform {
    position: Vec3,
    rotation: Quat,
    scale: Vec3 = Vec3(1, 1, 1)  // default value
}
```

### Type Aliases

Type aliases allow you to create shorter or more descriptive names for existing types:

```heidic
type ImageView = VkImageView;
type DescriptorSet = VkDescriptorSet;
type CommandBuffer = VkCommandBuffer;
```

### Control Flow

```heidic
// If statement
if condition {
    // code
} else {
    // code
}

// While loop
while condition {
    // code
}

// Infinite loop
loop {
    // code
}
```

### Operators

- Arithmetic: `+`, `-`, `*`, `/`, `%`
- Comparison: `==`, `!=`, `<`, `<=`, `>`, `>=`
- Logical: `&&`, `||`, `!`
- Assignment: `=`

## Performance Features

1. **Zero-cost abstractions**: Compiles to efficient C++ code
2. **No garbage collection**: Manual memory management
3. **Arena allocators**: Fast memory allocation for game objects
4. **ECS support**: Built-in Entity Component System for efficient game object management

## Standard Library

The language includes built-in types for game development:

- `Vec2` - 2D vector (x, y)
- `Vec3` - 3D vector (x, y, z)
- `Vec4` - 4D vector (x, y, z, w)

## Compilation

HEIDIC compiles to C++ which is then compiled to native machine code:

```
game.hd → [HEIDIC Compiler] → game.cpp → [C++ Compiler] → game.exe
```

This two-stage compilation ensures maximum performance while maintaining a clean, game-focused syntax.

