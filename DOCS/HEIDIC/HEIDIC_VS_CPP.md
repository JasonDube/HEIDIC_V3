# HEIDIC vs C++: Why Use HEIDIC?

## Overview

HEIDIC is a programming language designed for **building game engines**, not scripting game logic. It compiles to C++ and provides game engine-specific abstractions and features that make engine development faster, safer, and more maintainable.

**Important Distinction:**
- **HEIDIC**: A language for building game engines (like Unity's C#, Unreal's C++)
- **Not**: A scripting language for game logic (like Lua, Python, or visual scripting)

HEIDIC is used to write the engine itself - rendering systems, ECS frameworks, resource managers, physics engines, etc.

## Key Advantages

### 1. Game-Focused Syntax

**C++:**
```cpp
struct Position {
    float x, y, z;
};

void update_positions(std::vector<Position>& positions, float deltaTime) {
    for (auto& pos : positions) {
        pos.x += 1.0f * deltaTime;
    }
}
```

**HEIDIC:**
```heidic
component Position {
    x: f32,
    y: f32,
    z: f32
}

system update_positions(query Position) {
    for pos in query {
        pos.x += 1.0 * deltaTime;
    }
}
```

HEIDIC's syntax is designed specifically for game development patterns, making common operations more intuitive.

### 2. Built-in ECS Support

**C++:** You need to implement or integrate an ECS library (EnTT, Flecs, etc.), which adds complexity and dependencies.

**HEIDIC:** ECS is a first-class language feature:
```heidic
component Position { x: f32, y: f32, z: f32 }
component Velocity { x: f32, y: f32, z: f32 }

system physics(query Position, Velocity) {
    for (pos, vel) in query {
        pos.x += vel.x * deltaTime;
        pos.y += vel.y * deltaTime;
        pos.z += vel.z * deltaTime;
    }
}
```

### 3. Zero-Cost Abstractions with Safety

HEIDIC compiles to efficient C++ code, so you get:
- **Performance**: Same as hand-written C++
- **Safety**: Type checking and compile-time guarantees
- **Productivity**: Higher-level abstractions without runtime overhead

### 4. Hot Reload Built-In

**C++:** Hot reload requires complex DLL loading, symbol management, and careful state preservation.

**HEIDIC:** Hot reload is a language feature:
```heidic
@hot
system player_movement(query Position, Velocity) {
    // Edit this code, save, and it reloads automatically!
}
```

### 5. Cleaner Resource Management

**C++:**
```cpp
std::unique_ptr<TextureResource> texture;
texture = std::make_unique<TextureResource>("texture.png");
if (!texture->isLoaded()) {
    // error handling
}
```

**HEIDIC:**
```heidic
let texture = load_texture("texture.png");
// Automatic resource management with clear semantics
```

### 6. Math Types as Primitives

**C++:** You need to include and use external math libraries (GLM, etc.)

**HEIDIC:** Math types are built-in:
```heidic
let position: Vec3 = Vec3(1.0, 2.0, 3.0);
let matrix: Mat4 = Mat4::identity();
let result = matrix * position;  // Clean, intuitive syntax
```

### 7. SOA Layout Support

**C++:** Structure-of-Arrays requires manual memory management and careful indexing.

**HEIDIC:** SOA is a language feature for cache-efficient data layouts:
```heidic
component_soa Velocity {
    x: [f32],
    y: [f32],
    z: [f32]
}
// Automatically optimized for cache efficiency
```

### 8. Integrated Development Experience

**C++:** You need separate tools for:
- Code editing
- Building
- Debugging
- Hot reload setup

**HEIDIC:** Electroscribe IDE provides:
- Syntax highlighting
- One-click build and run
- Integrated hot reload
- C++ view for debugging

### 9. Type Safety Without Verbosity

**C++:**
```cpp
template<typename T>
void process_entities(std::vector<T>& entities) {
    // Template complexity
}
```

**HEIDIC:**
```heidic
fn process_entities<T>(entities: [T]): void {
    // Type inference and safety without template complexity
}
```

### 10. Game Engine Integration

**C++:** You write engine code and game code in the same language, making it hard to separate concerns.

**HEIDIC:** Clear separation:
- **HEIDIC**: Your game logic (`.hd` files)
- **EDEN Engine**: Runtime engine (C++ backend)
- Clean API boundary between game and engine

## When to Use HEIDIC

### ✅ Use HEIDIC When:

- **Building a game engine** (rendering systems, ECS frameworks, resource managers)
- You want ECS without library dependencies
- You need hot reload for rapid engine iteration
- You want game engine-focused abstractions
- You need performance but want productivity
- You're building with Vulkan/GLFW
- You're writing engine code, not game logic scripts

### ❌ Use C++ When:

- Building non-game applications
- You need full C++ ecosystem compatibility
- You require specific C++ libraries
- You're working on low-level systems programming
- You need to integrate with existing C++ codebases extensively

### ❌ HEIDIC is NOT for:

- Scripting game logic (use Lua, Python, or a visual scripting system)
- Writing gameplay code that runs on top of an engine
- Modding or user-generated content scripting

## Performance Comparison

HEIDIC compiles to C++, so performance is identical:

```
HEIDIC Code → [Compiler] → C++ Code → [g++] → Binary
```

The generated C++ is optimized and efficient - you're not sacrificing performance for productivity.

## Migration Path

HEIDIC code can call C++ functions directly:
```heidic
extern fn cpp_function(x: f32): f32;

fn main(): void {
    let result = cpp_function(10.0);
}
```

This means you can:
- Gradually migrate C++ code to HEIDIC
- Use existing C++ libraries
- Keep performance-critical C++ code where needed

## Conclusion

HEIDIC is not a replacement for C++ in all scenarios, but for **building game engines**, it provides:

1. **Faster Development**: Game engine-focused syntax and built-in ECS
2. **Better Safety**: Type checking and compile-time guarantees
3. **Same Performance**: Compiles to efficient C++ code
4. **Modern Workflow**: Hot reload and integrated tooling
5. **Clear Separation**: Engine code vs game logic

If you're **building a game engine**, HEIDIC gives you the power of C++ with the productivity of a higher-level language. It's designed for writing the engine itself - the rendering pipeline, ECS framework, resource systems, and other core engine components - not for scripting gameplay logic on top of an existing engine.

