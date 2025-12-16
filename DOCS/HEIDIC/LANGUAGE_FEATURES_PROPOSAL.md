# HEIDIC_v2 Language Features Proposal

## Overview
This document outlines proposed language features to enhance HEIDIC_v2's ECS capabilities, Vulkan integration, and performance characteristics.

## Feature Analysis & Implementation Plan

### Phase 1: Quick Wins (Low Effort, High Value)

#### 1. Vulkan Type Aliases ✅ **EASY**
**Syntax:**
```heidic
type ImageView = VkImageView;
type DescriptorSet = VkDescriptorSet;
type CommandBuffer = VkCommandBuffer;
```

**Implementation:**
- Add `TypeAlias` to AST
- Codegen: Emit `using` or `typedef` in C++
- **Effort:** ~2-4 hours
- **Value:** High readability improvement

#### 2. Default Values in Components ✅ **MODERATE**
**Syntax:**
```heidic
component Transform {
    position: Vec3,
    rotation: Quat,
    scale: Vec3 = Vec3(1, 1, 1)  // default values
}
```

**Implementation:**
- Extend `Field` to include optional default value expression
- Codegen: Generate constructor with default parameters
- **Effort:** ~4-8 hours
- **Value:** High ergonomics improvement

### Phase 2: Core ECS Enhancements (Medium Effort, High Value)

#### 3. System Dependency Declaration ✅ **HIGH VALUE**
**Syntax:**
```heidic
@system(render, after = Physics, before = RenderSubmit)
fn update_transforms(q: query<Transform, Velocity>) {
    for entity in q {
        entity.Transform.position += entity.Velocity * dt;
    }
}
```

**Implementation:**
- Add `@system` attribute parsing
- Build dependency graph from `after`/`before` declarations
- Generate system scheduler code
- **Effort:** ~2-3 days
- **Value:** Critical for ECS usability

**Alternative (Simpler Start):**
```heidic
system update_transforms(after: Physics, before: RenderSubmit) {
    // ...
}
```

#### 4. Query Syntax Enhancement ✅ **HIGH VALUE**
**Current:** (Need to check current syntax)
**Proposed:**
```heidic
fn update_transforms(q: query<Transform, Velocity>) {
    for entity in q {
        // entity.Transform, entity.Velocity available
    }
}
```

**Implementation:**
- Add `query<T...>` type to type system
- Codegen: Generate ECS query iteration code
- **Effort:** ~1-2 days
- **Value:** Essential for ECS ergonomics

### Phase 3: Performance Features (High Effort, High Value)

#### 5. Frame-Scoped Memory (FrameArena) ✅ **HIGH VALUE**
**Syntax:**
```heidic
fn render_frame(frame: FrameArena) {
    let positions = frame.alloc_array<Vec3>(entity_count);
    // Automatically freed at frame end
}
```

**Implementation:**
- Add `FrameArena` type to standard library
- Implement stack allocator in C++ runtime
- Lifetime tracking (compile-time or runtime)
- **Effort:** ~3-5 days
- **Value:** Critical for zero-allocation rendering

**Alternative (Simpler):**
```heidic
fn render_frame() {
    frame_arena: FrameArena;
    let positions = frame_arena.alloc_array<Vec3>(entity_count);
    // Explicit scope-based cleanup
}
```

#### 6. Compile-Time Shader Embedding ✅ **HIGH VALUE**
**Syntax:**
```heidic
shader vertex "shaders/triangle.vert" {
    // Gets compiled to SPIR-V and embedded at compile-time
}

// Usage:
let shader_module = load_embedded_shader(vertex_shader);
```

**Implementation:**
- Add `shader` declaration to AST
- Integrate `glslc` into build pipeline
- Embed SPIR-V as `const uint8_t[]` in generated C++
- Handle compilation errors gracefully
- **Effort:** ~2-3 days
- **Value:** Eliminates runtime file I/O, type-safe shader loading

**Build Integration:**
```bash
# In build.ps1 or CMakeLists.txt
heidic compile game.hd --embed-shaders
# Automatically runs: glslc shaders/*.vert -o shaders/*.vert.spv
# Embeds SPIR-V in generated C++
```

### Phase 4: Advanced Features (High Effort, Medium-High Value)

#### 7. Component Auto-Registration ✅ **MODERATE VALUE**
**Syntax:**
```heidic
component Transform {
    position: Vec3,
    rotation: Quat,
    scale: Vec3
}
// Automatically registered in component registry
```

**Implementation:**
- Generate component metadata at compile-time
- Create component registry initialization code
- **Effort:** ~2-3 days
- **Value:** Convenience, but not critical

**Hot-Reload (Future):**
- Runtime component swapping
- Requires dynamic loading infrastructure
- **Effort:** ~1-2 weeks
- **Value:** Nice-to-have for rapid iteration

#### 8. SOA (Structure of Arrays) Mode ⚠️ **COMPLEX**
**Syntax:**
```heidic
component_soa Velocity {
    x: f32,
    y: f32,
    z: f32
}
// Compiler stores as: float x[], float y[], float z[]
```

**Implementation:**
- Transform component access patterns
- Generate SOA iteration code
- Handle mixed AoS/SOA queries
- **Effort:** ~1-2 weeks
- **Value:** Performance optimization, but complex

**Recommendation:** 
- Start with AoS (current approach)
- Add SOA as opt-in optimization later
- Profile first to see if needed

## Recommended Implementation Order

### Sprint 1 (1-2 weeks)
1. ✅ Vulkan type aliases
2. ✅ Default values in components
3. ✅ Query syntax enhancement

### Sprint 2 (2-3 weeks)
4. ✅ System dependency declaration
5. ✅ Compile-time shader embedding

### Sprint 3 (2-3 weeks)
6. ✅ Frame-scoped memory (FrameArena)
7. ✅ Component auto-registration

### Sprint 4 (Future)
8. ⚠️ SOA mode (if profiling shows need)
9. ⚠️ Hot-reload (if rapid iteration needed)

## Design Considerations

### Type System Impact
- All features must integrate with existing type checker
- Query types need special handling
- FrameArena needs lifetime analysis

### Codegen Complexity
- System scheduling requires graph algorithms
- SOA transformation is non-trivial
- Shader embedding needs build integration

### Runtime Requirements
- FrameArena needs C++ allocator
- Component registry needs runtime metadata
- System scheduler needs execution graph

### Backward Compatibility
- All features should be opt-in
- Existing code should continue to work
- Gradual migration path

## Questions to Resolve

1. **Query Syntax:** How do we handle optional components? `query<Transform, ?Velocity>`?
2. **System Scheduling:** Multi-threaded or single-threaded?
3. **FrameArena Lifetime:** Compile-time or runtime tracking?
4. **SOA Access:** How do we access `entity.Velocity.x` in SOA mode?
5. **Shader Errors:** How do we report glslc compilation errors?

## Next Steps

1. **Prototype:** Start with Vulkan aliases and default values (quick wins)
2. **Design:** Detailed design doc for system dependencies
3. **Research:** Study Bevy's system scheduler implementation
4. **Benchmark:** Profile current ECS to see if SOA is needed

