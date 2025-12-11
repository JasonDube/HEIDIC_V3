# SOA Access Pattern Explained - Understanding Arrays in HEIDIC

## Overview

HEIDIC has **two types of arrays**:

1. **Regular Arrays** - `[Type]` - Standard dynamic arrays (like `std::vector` in C++)
2. **SOA Components** - `component_soa` - Structure-of-Arrays layout for performance

This document explains the difference and how to access them correctly.

---

## 1. Regular Arrays (Standard)

### Syntax
```heidic
let numbers: [i32];           // Dynamic array of integers
let positions: [Vec3];        // Dynamic array of 3D vectors
let names: [string];          // Dynamic array of strings
```

### How They Work
- Standard array type (like `std::vector<T>` in C++)
- Access with index: `array[0]`, `array[1]`, etc.
- Stored as: `std::vector<int>`, `std::vector<Vec3>`, etc.

### Example
```heidic
let numbers: [i32];
numbers[0] = 10;
numbers[1] = 20;
let first = numbers[0];  // Access element at index 0
```

### Generated C++
```cpp
std::vector<int32_t> numbers;
numbers[0] = 10;
numbers[1] = 20;
int32_t first = numbers[0];
```

---

## 2. SOA Components (Structure-of-Arrays)

### What is SOA?

**SOA (Structure-of-Arrays)** is a memory layout optimization where instead of storing data as:
```
Array of Structures (AoS):
  [Entity1{pos, vel}, Entity2{pos, vel}, Entity3{pos, vel}]
```

We store it as:
```
Structure of Arrays (SOA):
  positions: [pos1, pos2, pos3, ...]
  velocities: [vel1, vel2, vel3, ...]
```

### Why SOA?

**Performance Benefits:**
1. **Cache-Friendly**: When iterating, you access the same field across many entities
2. **Vectorization**: SIMD instructions can process multiple values at once
3. **GPU-Friendly**: CUDA/OptiX prefer SOA layout for parallel processing

### Syntax
```heidic
// SOA Component - all fields must be arrays
component_soa Velocity {
    x: [f32],    // Array of X velocities
    y: [f32],    // Array of Y velocities
    z: [f32]     // Array of Z velocities
}
```

### Generated C++ Structure
```cpp
struct Velocity {
    std::vector<float> x;  // Array of X values
    std::vector<float> y;  // Array of Y values
    std::vector<float> z;  // Array of Z values
};
```

---

## 3. The Problem: Access Pattern Confusion

### Current Issue

When you have a SOA component in a query, accessing it is confusing:

```heidic
component_soa Velocity {
    x: [f32],
    y: [f32],
    z: [f32]
}

component Position {  // Regular AoS component
    x: f32,
    y: f32,
    z: f32
}

fn update(q: query<Position, Velocity>): void {
    for entity in q {
        // ❌ CONFUSING: Is entity.Velocity.x an array or a single value?
        // entity.Velocity.x is an array [f32], but we want a single value!
        entity.Position.x += entity.Velocity.x * delta_time;  // How does this work?
    }
}
```

### The Solution: Compiler Hides SOA Complexity

The compiler should automatically handle the SOA access pattern:

**HEIDIC Code (what you write):**
```heidic
fn update(q: query<Position, Velocity>): void {
    for entity in q {
        // You write this - simple and clear
        entity.Position.x += entity.Velocity.x * delta_time;
    }
}
```

**Generated C++ (what compiler generates):**
```cpp
void update(Query_Position_Velocity& q) {
    for (size_t i = 0; i < q.size(); ++i) {
        // AoS access: positions[i].x (struct member)
        // SOA access: velocities.x[i] (array access)
        q.positions[i].x += q.velocities.x[i] * delta_time;
    }
}
```

### Key Insight

When iterating over a query:
- **AoS Component** (`Position`): `entity.Position.x` → `positions[i].x` (struct member access)
- **SOA Component** (`Velocity`): `entity.Velocity.x` → `velocities.x[i]` (array access)

The compiler automatically:
1. Tracks the current iteration index `i`
2. Determines if component is AoS or SOA
3. Generates the correct access pattern

---

## 4. Comparison: Old Engine Arrays vs HEIDIC

### Old Engine (2001-style)

**Array of Structures (AoS):**
```c
typedef struct {
    float x, y, z;
} Position;

Position positions[1000];  // Array of Position structs

// Access:
positions[i].x = 10.0;  // Access struct member
```

**Manual SOA (if you wanted it):**
```c
// You had to manually create separate arrays
float position_x[1000];
float position_y[1000];
float position_z[1000];

// Access:
position_x[i] = 10.0;  // Access array element
```

### HEIDIC (Modern)

**Regular Arrays:**
```heidic
let positions: [Position];  // Array of Position structs
positions[0].x = 10.0;      // Access struct member
```

**SOA Components (Automatic):**
```heidic
component_soa Velocity {
    x: [f32],
    y: [f32],
    z: [f32]
}

// In query iteration, compiler handles access automatically:
for entity in q {
    entity.Velocity.x = 10.0;  // Compiler generates: velocities.x[i] = 10.0
}
```

---

## 5. Mixed Queries (AoS + SOA)

You can mix AoS and SOA components in the same query:

```heidic
component Position {      // AoS - Array of Structures
    x: f32,
    y: f32,
    z: f32
}

component_soa Velocity {  // SOA - Structure of Arrays
    x: [f32],
    y: [f32],
    z: [f32]
}

fn update(q: query<Position, Velocity>): void {
    for entity in q {
        // Position is AoS: positions[i].x
        // Velocity is SOA: velocities.x[i]
        entity.Position.x += entity.Velocity.x * delta_time;
        entity.Position.y += entity.Velocity.y * delta_time;
        entity.Position.z += entity.Velocity.z * delta_time;
    }
}
```

**Generated C++:**
```cpp
void update(Query_Position_Velocity& q) {
    for (size_t i = 0; i < q.size(); ++i) {
        // AoS access
        q.positions[i].x += q.velocities.x[i] * delta_time;
        q.positions[i].y += q.velocities.y[i] * delta_time;
        q.positions[i].z += q.velocities.z[i] * delta_time;
    }
}
```

---

## 6. Summary: Array Types in HEIDIC

| Type | Syntax | Use Case | Access Pattern |
|------|--------|----------|----------------|
| **Regular Array** | `[Type]` | General-purpose arrays | `array[index]` |
| **AoS Component** | `component Name { ... }` | Standard components | `entity.Component.field` → `components[i].field` |
| **SOA Component** | `component_soa Name { ... }` | Performance-critical components | `entity.Component.field` → `components.field[i]` |

### When to Use Each

**Regular Arrays (`[Type]`):**
- General-purpose data storage
- Lists, collections, buffers
- When you need standard array operations

**AoS Components (`component`):**
- Standard ECS components
- When fields are accessed together
- Simpler to understand and use

**SOA Components (`component_soa`):**
- Performance-critical components (Velocity, Position for physics)
- When iterating over many entities
- When you need GPU/CUDA interop
- When you need vectorization

---

## 7. Implementation Goal

**Current State:**
- ✅ SOA components exist (`component_soa`)
- ✅ Regular arrays exist (`[Type]`)
- ❌ Access pattern unclear in queries (`entity.Velocity.x` - is it an array or value?)

**Target State:**
- ✅ Compiler automatically handles SOA access in queries
- ✅ `entity.Velocity.x` works transparently (compiler generates `velocities.x[i]`)
- ✅ No confusion - same syntax for AoS and SOA

---

*This is what Sprint 1 Task 2 implements: making SOA access pattern clear and automatic.*

