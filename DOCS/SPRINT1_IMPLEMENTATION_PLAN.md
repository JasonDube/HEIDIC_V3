# Sprint 1 Implementation Plan - Critical Usability Fixes

## Overview

Sprint 1 focuses on making ECS queries actually usable. Currently, you can declare `query<Position, Velocity>` but can't iterate over it. This sprint fixes that.

## Goals

1. **Query Iteration Syntax** - Add `for entity in q` syntax
2. **SOA Access Pattern Clarity** - Make `entity.Velocity.x` work transparently
3. **Better Error Messages** - Add line numbers, context, suggestions

---

## Task 1: Query Iteration Syntax (`for entity in q`)

### Current State
- ✅ Query type exists: `query<Position, Velocity>`
- ✅ Can declare query parameters in functions
- ❌ Cannot iterate over queries
- ❌ No `for entity in q` syntax

### What We Need

**HEIDIC Code:**
```heidic
fn update_physics(q: query<Position, Velocity>): void {
    for entity in q {
        entity.Position.x += entity.Velocity.x * delta_time;
    }
}
```

**Generated C++ (target):**
```cpp
void update_physics(Query_Position_Velocity& q) {
    for (size_t i = 0; i < q.size(); ++i) {
        q.positions[i].x += q.velocities[i].x * delta_time;
    }
}
```

### Implementation Steps

1. **Add `For` statement to AST**
   - `For { iterator: String, collection: Expression, body: Vec<Statement> }`
   - Similar to `While` but for iteration

2. **Add `For` token to lexer**
   - `#[token("for")] For,`

3. **Parse `for entity in q` in parser**
   - Pattern: `for <identifier> in <expression> { ... }`
   - Validate that expression is a query type

4. **Type checking**
   - Ensure collection expression is a query type
   - Make iterator variable available in body scope
   - Type of iterator is an "entity" (has access to query components)

5. **Code generation**
   - Generate iteration loop over query
   - Generate entity access (entity.Position → query.positions[i])
   - Handle both AoS and SOA components

---

## Task 2: SOA Access Pattern Clarity

### Current State
- ✅ SOA components exist: `component_soa Velocity { x: [f32], y: [f32], z: [f32] }`
- ❌ Access pattern unclear: `entity.Velocity.x` - is this an array or single value?

### What We Need

**HEIDIC Code:**
```heidic
component_soa Velocity {
    x: [f32],
    y: [f32],
    z: [f32]
}

fn update(q: query<Position, Velocity>): void {
    for entity in q {
        // This should work - compiler hides SOA complexity
        entity.Position.x += entity.Velocity.x * delta_time;
    }
}
```

**Generated C++ (target):**
```cpp
void update(Query_Position_Velocity& q) {
    for (size_t i = 0; i < q.size(); ++i) {
        // SOA access: velocities.x[i] (not velocities[i].x)
        q.positions[i].x += q.velocities.x[i] * delta_time;
    }
}
```

### Implementation Steps

1. **Entity access in type checker**
   - When accessing `entity.Component.field`, check if component is SOA
   - If SOA: `entity.Velocity.x` → `velocities.x[i]` (array access)
   - If AoS: `entity.Position.x` → `positions[i].x` (struct member)

2. **Code generation for entity access**
   - Track current iteration index
   - Generate correct access pattern based on component layout (AoS vs SOA)

---

## Task 3: Better Error Messages

### Current State
```
Error: Type mismatch in assignment
```

### What We Need
```
Error at examples/test.hd:42:8:
    let x: f32 = "hello";
                 ^^^^^^^
Type mismatch: cannot assign 'string' to 'f32'
Suggestion: Use a float variable or convert: x = 10.0
```

### Implementation Steps

1. **Add source location tracking**
   - Store line/column in AST nodes
   - Pass location through parser, type checker, codegen

2. **Enhanced error reporting**
   - Include file path, line, column
   - Show surrounding code context
   - Add suggestions for common errors

3. **Multiple error collection**
   - Don't stop at first error
   - Collect all errors and report them together

---

## Implementation Order

1. **Start with Task 1** (Query Iteration) - This is the most critical
2. **Then Task 2** (SOA Access) - Works with Task 1
3. **Finally Task 3** (Error Messages) - Improves developer experience

---

## Testing

After each task, test with:

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

fn test_query(q: query<Position, Velocity>): void {
    for entity in q {
        entity.Position.x += entity.Velocity.x * 0.016;
    }
}
```

---

*Let's start implementing!*

