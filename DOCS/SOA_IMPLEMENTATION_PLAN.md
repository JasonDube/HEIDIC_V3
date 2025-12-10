# SOA Access Pattern Clarity - Implementation Plan

## Current Status

**What Exists:**
- ✅ Regular arrays: `[Type]` - fully implemented
- ✅ AoS components: `component Name { ... }` - fully implemented
- ❓ SOA components: `component_soa` - mentioned in docs but needs verification

**What's Missing:**
- ❌ Detection of SOA vs AoS components in type checker
- ❌ Different codegen for SOA access (`velocities.x[i]`) vs AoS access (`positions[i].x`)
- ❌ Tracking iteration index in for loops

---

## Implementation Steps

### Step 1: Add SOA Component Detection

**Goal:** Detect if a component is SOA or AoS

**Changes Needed:**
1. Add `is_soa: bool` flag to `ComponentDef` in AST
2. Add `component_soa` token to lexer
3. Parse `component_soa` keyword in parser
4. Validate SOA components (all fields must be arrays)

**Files to Modify:**
- `src/ast.rs` - Add `is_soa` flag
- `src/lexer.rs` - Add `ComponentSOA` token
- `src/parser.rs` - Parse `component_soa` keyword
- `src/type_checker.rs` - Validate SOA components

### Step 2: Track Component Types in Queries

**Goal:** Know which components in a query are SOA vs AoS

**Changes Needed:**
1. Store component metadata (name, is_soa) in type checker
2. Pass component metadata to codegen
3. Generate different access patterns based on is_soa flag

**Files to Modify:**
- `src/type_checker.rs` - Store component metadata
- `src/codegen.rs` - Use metadata for codegen

### Step 3: Implement SOA Access Pattern in Codegen

**Goal:** Generate correct C++ code for SOA vs AoS access

**Current Codegen (AoS only):**
```rust
// Generates: query.positions[i].x
format!("{}.{}[{}_index].{}", query_name, component_plural, entity_name, member)
```

**Target Codegen (AoS + SOA):**
```rust
if component_is_soa {
    // SOA: query.velocities.x[i]
    format!("{}.{}.{}[{}_index]", query_name, component_plural, member, entity_name)
} else {
    // AoS: query.positions[i].x
    format!("{}.{}[{}_index].{}", query_name, component_plural, entity_name, member)
}
```

**Files to Modify:**
- `src/codegen.rs` - Update `generate_expression_with_entity` function

### Step 4: Track Iteration Index

**Goal:** Know the current iteration index in for loops

**Changes Needed:**
1. Track iteration index variable name in codegen
2. Use index variable in entity access generation

**Files to Modify:**
- `src/codegen.rs` - Track iteration index in for loop codegen

---

## Testing

After implementation, test with:

```heidic
component Position {  // AoS
    x: f32,
    y: f32,
    z: f32
}

component_soa Velocity {  // SOA
    x: [f32],
    y: [f32],
    z: [f32]
}

fn update(q: query<Position, Velocity>): void {
    for entity in q {
        // AoS access: positions[i].x
        // SOA access: velocities.x[i]
        entity.Position.x += entity.Velocity.x * 0.016;
        entity.Position.y += entity.Velocity.y * 0.016;
        entity.Position.z += entity.Velocity.z * 0.016;
    }
}
```

**Expected Generated C++:**
```cpp
void update(Query_Position_Velocity& q) {
    for (size_t i = 0; i < q.size(); ++i) {
        // AoS: positions[i].x
        // SOA: velocities.x[i]
        q.positions[i].x += q.velocities.x[i] * 0.016;
        q.positions[i].y += q.velocities.y[i] * 0.016;
        q.positions[i].z += q.velocities.z[i] * 0.016;
    }
}
```

---

## Implementation Order

1. **Step 1** - Add SOA component detection (foundation)
2. **Step 2** - Track component types in queries (metadata)
3. **Step 3** - Implement SOA access pattern (codegen)
4. **Step 4** - Track iteration index (polish)

---

*Let's start implementing!*

