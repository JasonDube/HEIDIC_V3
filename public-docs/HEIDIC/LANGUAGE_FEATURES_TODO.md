# HEIDIC Language Features - Consolidated TODO List

> **Status:** ~95% complete. Remaining features will make HEIDIC legendary.

## ✅ Completed Features

- ✅ Query syntax + codegen (Flecs/Bevy-level, cleaner)
- ✅ Compile-time shader embedding (glslc → SPIR-V arrays)
- ✅ FrameArena with `frame.alloc_array<T>` (zero-allocation rendering)
- ✅ Component SOA + Mesh SOA (CUDA/OptiX interop ready)
- ✅ System dependencies + topological sort (with cycle detection)
- ✅ Type aliases, default values
- ✅ Full Vulkan/GLFW/ImGui integration
- ✅ **Hot-Reloading (CONTINUUM)** - Systems, Shaders, Components (100% complete)
- ✅ **Resource Handles** - Texture (DDS, PNG), Mesh (OBJ), Audio (WAV, OGG) with hot-reload

---

## 🔴 CRITICAL - Blocks Usability (Sprint 1)

### 1. Query Iteration Syntax ⚠️ **CRITICAL**
**Status:** 🔴 Not Started  
**Priority:** CRITICAL (Blocks ECS usability)  
**Effort:** ~2-3 days  
**Impact:** Without this, ECS is unusable. Claude: "This is critical for usability."

**Implementation:**
```heidic
fn update(q: query<Position, Velocity>): void {
    for entity in q {
        entity.Position.x += entity.Velocity.x * dt;
    }
}
```

**Tasks:**
- [ ] Add `For` statement to AST
- [ ] Add `for` token to lexer
- [ ] Parse `for entity in q` pattern in parser
- [ ] Type checking (ensure collection is query type)
- [ ] Code generation (generate iteration loop, handle AoS/SOA)

---

### 2. SOA Access Pattern Clarity ⚠️ **HIGH**
**Status:** ✅ **COMPLETE**  
**Priority:** HIGH (User confusion)  
**Effort:** ~1 week (actual: Already implemented as part of query iteration)  
**Impact:** SOA is great for storage, but access pattern needs to be crystal clear.

**Implementation:**
```heidic
component_soa Velocity {
    x: [f32],
    y: [f32],
    z: [f32]
}

fn update(q: query<Position, Velocity>): void {
    for entity in q {
        // entity.Velocity.x should work transparently
        // Compiler generates: velocities.x[entity_index]
        entity.Position.x += entity.Velocity.x * dt;
    }
}
```

**Implemented:**
- [x] Entity access in type checker (detect SOA vs AoS)
- [x] Code generation for entity access (track iteration index)
- [x] Generate correct access pattern (AoS: `positions[i].x`, SOA: `velocities.x[i]`)
- [x] SOA component validation (all fields must be arrays)
- [x] Transparent syntax (same for AoS and SOA)

---

### 3. Better Error Messages ⚠️ **HIGH**
**Status:** ✅ **COMPLETE**  
**Priority:** HIGH (Developer experience)  
**Effort:** ~1 week (actual: ~6 hours total)  
**Impact:** Developer experience matters. Good error messages = faster iteration.

**Implemented:**
```
❌ Error at test.hd:42:8:
  41 |     let x: f32 = 10.0;
  42 |     let y: f32 = "hello";
      |                 ^^^^^^
  43 |     print(y);

Type mismatch: cannot assign 'string' to 'f32'
💡 Suggestion: Use a float value: let y: f32 = 10.0;
```

**Tasks:**
- [x] Add source location tracking (line/column in AST nodes)
- [x] Enhanced error reporting (file path, line, column, context)
- [x] Add suggestions for common errors
- [x] Error collection in type checker
- [x] Integrate ErrorReporter into parser with suggestions
- [x] Error recovery with poison types (continue after errors)
- [x] "Did you mean?" suggestions for typos (fuzzy matching)
- [x] Secondary locations for context (polish)

---

## 🔴 HIGH Priority - Core Features

### 4. Component Auto-Registration + Reflection ⚠️ **BLOCKS TOOLING**
**Status:** 🔴 Not Started  
**Priority:** HIGH (Blocks editor tools, serialization, networking)  
**Effort:** ~2-3 days  
**Impact:** Unlocks editor tools, serialization, hot-reload, networking.

**Implementation:**
```heidic
component Transform {
    position: Vec3,
    rotation: Quat,
    scale: Vec3 = Vec3(1, 1, 1)
}
// Automatically registered in ComponentRegistry
```

**Tasks:**
- [ ] Generate `ComponentRegistry` initialization code
- [ ] Generate component metadata (id, name, size, alignment, default)
- [ ] Generate reflection data (field names, types, offsets)
- [ ] Generate serialization/deserialization helpers

---

### 5. Zero-Boilerplate Pipeline Creation ⭐ **400 LINES → 10 LINES**
**Status:** ✅ **COMPLETE**  
**Priority:** HIGH  
**Effort:** ~1 week  
**Impact:** Removes 400 lines of Vulkan boilerplate per pipeline.

**Implementation:**
```heidic
pipeline pbr {
    shader vertex   "pbr.vert"
    shader fragment "pbr.frag"
    layout {
        binding 0: uniform SceneData
        binding 1: storage Materials[]
        binding 2: sampler2D albedo_maps[]
    }
}
```

**Tasks:**
- [ ] Add `pipeline` keyword to lexer/parser
- [ ] Parse pipeline declaration (shader references, layout)
- [ ] Generate full `VkGraphicsPipeline` creation code
- [ ] Generate `VkDescriptorSetLayout` from layout declaration
- [ ] Generate helper functions: `create_pbr_pipeline()`, `bind_pbr_pipeline()`
- [ ] Generate reflection data (for bindless/shader introspection)

---

## 🟡 MEDIUM Priority - Advanced Features

### 6. Automatic Bindless Integration ⭐ **ZERO DESCRIPTOR UPDATES**
**Status:** ✅ **COMPLETE** (Core infrastructure)  
**Priority:** MEDIUM  
**Effort:** ~3-5 days (actual: ~4 hours for core)  
**Impact:** Eliminates descriptor set management entirely.

**Implementation:**
```heidic
resource Image albedo = "textures/brick.png";
resource Image normal = "textures/brick_norm.png";
// Automatically registered in global bindless heap
// Shaders just use: bindless_texture(albedo_index)
```

**Tasks:**
- [ ] Generate global bindless descriptor set
- [ ] Auto-register all `resource Image` declarations
- [ ] Generate index constants: `ALBEDO_TEXTURE_INDEX = 0`, etc.
- [ ] Generate shader code that uses bindless texture access

---

### 7. CUDA/OptiX Interop ⭐ **THE SECRET WEAPON**
**Status:** ✅ **COMPLETE** (Core Infrastructure)  
**Priority:** MEDIUM (but HIGH value for ray tracing)  
**Effort:** ~1-2 weeks (actual: ~2 hours for core infrastructure)  
**Impact:** Only indie engine with seamless CPU → GPU → Ray-tracing data flow.

**Implementation:**
```heidic
@[cuda]
component_soa Position { x: [f32], y: [f32], z: [f32] }

@[launch(kernel = raytrace)]
fn raytrace_scene(q: query<Position, Velocity>): void {
    // HEIDIC code that compiles to CUDA kernel
}
```

**Tasks:**
- [ ] Add `@[cuda]` attribute parsing
- [ ] Add `@[launch(kernel = name)]` attribute parsing
- [ ] Generate `.cu` files from `@[launch]` functions
- [ ] Generate CUDA kernel launch code
- [ ] Generate memory transfer code (CPU ↔ GPU)
- [ ] Generate OptiX integration code

---

### 8. Memory Ownership Semantics ⚠️ **PREVENTS BUGS**
**Status:** ✅ **COMPLETE** (Compile-time validation)  
**Priority:** MEDIUM  
**Effort:** ~1 week (actual: ~2 hours for compile-time checks)  
**Impact:** Prevents use-after-free bugs by catching frame-scoped memory returns at compile time.

**Current Issue:**
```heidic
let positions = frame.alloc_array<Vec3>(100);
return positions; // BUG: positions is frame-scoped!
```

**Tasks:**
- [ ] RAII-style automatic cleanup (destructors)
- [ ] Compiler checks to prevent returning frame-scoped allocations
- [ ] Add ownership semantics later if needed

---

### 9. Type Inference Improvements
**Status:** ✅ **COMPLETE**  
**Priority:** MEDIUM  
**Effort:** ~2-3 days (actual: ~2 hours)  
**Impact:** Less boilerplate. Rust-style inference makes the language feel more modern.

**Current:**
```heidic
let x: f32 = 10.0;  // Explicit type still works
let numbers = [1, 2, 3];  // Now infers [i32]
let result = helper_function();  // Infers return type
```

**Implemented:**
```heidic
let positions = [Vec3(0,0,0), Vec3(1,1,1)]; // Infers [Vec3] (when struct literals are fully implemented)
let numbers = [1, 2, 3];  // Infers [i32]
let floats = [1.0, 2.5];  // Infers [f32]
let result = helper_function();  // Infers return type from function
```

**Tasks:**
- [x] Extend type inference to array literals
- [x] Infer types from function return values (already worked)
- [x] Infer types from struct constructors (when struct literals are fully implemented)

---

### 10. String Handling Improvements
**Status:** ✅ **PARTIALLY COMPLETE** (String interpolation implemented)  
**Priority:** MEDIUM  
**Effort:** ~1 week (actual: ~2 hours for interpolation)  
**Impact:** Clear string operations and ownership model.

**Implemented:**
- [x] Add string interpolation: `let msg = "Hello, {name}";`
- [x] Type validation for interpolated variables
- [x] Clear error messages for undefined variables and invalid types

**Remaining Tasks:**
- [ ] Fix string variable conversion (type-aware codegen)
- [ ] Add string concatenation operator: `"hello" + "world"`
- [ ] Add string manipulation functions (split, join, format)
- [ ] Fix bool conversion output ("true"/"false" instead of "1"/"0")
- [ ] Document string operations (concatenation, formatting)
- [ ] Clarify ownership (strings are value types, copied on assignment)

---

### 11. Pattern Matching
**Status:** ✅ **COMPLETE**  
**Priority:** MEDIUM  
**Effort:** ~1 week (actual: ~2 hours)  
**Impact:** Makes error handling and state machines much cleaner.

**Implemented:**
```heidic
match result {
    VK_SUCCESS => { /* ... */ }
    VK_ERROR_OUT_OF_MEMORY => { /* ... */ }
    value => { /* ... */ }  // Variable binding
    _ => { /* ... */ }  // Wildcard
}
```

**Tasks:**
- [x] Add `match` keyword to lexer/parser
- [x] Parse match expression with patterns (literal, variable, wildcard, identifier)
- [x] Type checking for match expressions (pattern type compatibility)
- [x] Code generation for match (if-else chain)

---

### 12. Optional Types
**Status:** ✅ **COMPLETE**  
**Priority:** MEDIUM  
**Effort:** ~1 week (actual: ~2 hours)  
**Impact:** Eliminates null pointer bugs.

**Implemented:**
```heidic
let mesh: ?Mesh = load_mesh("model.obj");
if mesh {
    draw(mesh.unwrap());
}
```

**Tasks:**
- [x] Add `?Type` syntax to type system
- [x] Add `unwrap()` method
- [x] Add null safety checks
- [x] Code generation for optional types (std::optional)
- [x] Add `null` literal support
- [x] Add implicit wrapping (assigning non-optional to optional)

---

### 13. Standard Library Expansion
**Status:** 🔴 Not Started  
**Priority:** MEDIUM  
**Effort:** ~2-3 weeks  
**Impact:** Boosts general-purpose utility and reduces reliance on generated C++ libraries.

**Tasks:**
- [ ] Week 1: Collections (HashMap, HashSet, Vec operations)
- [ ] Week 2: String manipulation (split, join, format, interpolation)
- [ ] Week 3: File I/O and algorithms (sort, search, filter)

---

## 🟢 LOW Priority - Polish & Quality of Life

### 14. Defer Statements
**Status:** ✅ **COMPLETE**  
**Priority:** LOW  
**Effort:** ~2-3 days (actual: ~2 hours)  
**Impact:** Ensures cleanup code always runs.

**Implemented:**
```heidic
fn process_file(path: string) {
    let file = open_file(path);
    defer close_file(file); // Always runs at scope exit
    
    // ... use file ...
}
```

**Tasks:**
- [x] Add `defer` keyword to lexer/parser
- [x] Add `Defer` variant to AST
- [x] Generate cleanup code at scope exit (RAII pattern using helper class)
- [x] Support multiple defer statements (reverse order execution)
- [x] Support defer in any scope (functions, blocks, loops)

---

### 15. Built-in Profiler Overlay
**Status:** 🔴 Not Started  
**Priority:** LOW  
**Effort:** ~2-3 days  
**Impact:** One-line profiler integration.

**Implementation:**
```heidic
profiler.show();  // Shows FPS, frame time, system timings
```

**Tasks:**
- [ ] Integrate with ImGui
- [ ] Track system execution times
- [ ] Display frame graph
- [ ] Memory usage stats

---

### 16. Optional Components in Queries
**Status:** 🔴 Not Started  
**Priority:** LOW  
**Effort:** ~1-2 days  
**Impact:** More flexible ECS queries.

**Implementation:**
```heidic
fn update(q: query<Position, ?Velocity>): void {
    // ?Velocity means optional - entity may or may not have Velocity
    for entity in q {
        if entity.has(Velocity) {
            entity.Position += entity.Velocity * dt;
        }
    }
}
```

**Tasks:**
- [ ] Add `?Component` syntax in query parsing
- [ ] Generate optional component checks
- [ ] Add `entity.has(Component)` method

---

## 🔧 Tooling & Infrastructure

### 17. Development Tooling - LSP, Formatter, Linter ⚠️ **ESSENTIAL FOR ADOPTION**
**Status:** 🔴 Not Started  
**Priority:** HIGH (Essential for professional workflows)  
**Effort:** ~2-3 weeks  
**Impact:** Essential for professional workflows and adoption in complex projects.

**Language Server (LSP):**
- [ ] Syntax highlighting
- [ ] Auto-completion
- [ ] Go-to-definition
- [ ] Error squiggles
- [ ] Hover information
- [ ] Symbol search

**Formatter:**
- [ ] Consistent code style
- [ ] Auto-format on save
- [ ] Configurable rules

**Linter:**
- [ ] Style checks
- [ ] Best practices
- [ ] Performance warnings
- [ ] Unused code detection

---

## Implementation Priority (Recommended Order)

### Sprint 1 (Weeks 1-2) - Critical Usability Fixes
1. **Query Iteration Syntax** (`for entity in q`) - CRITICAL
2. **SOA Access Pattern Clarity** - HIGH
3. **Better Error Messages** - HIGH

**Why First:** These are blocking usability. Without query iteration, ECS is unusable.

---

### Sprint 2 (Weeks 3-4) - Core Features
4. **Component Auto-Registration + Reflection** - HIGH (blocks tooling)
5. **Zero-Boilerplate Pipeline Creation** - HIGH (productivity booster)

---

### Sprint 3 (Weeks 5-6) - Advanced Features
6. **Automatic Bindless Integration** - MEDIUM
7. **CUDA/OptiX Interop** - MEDIUM (high value)

---

### Sprint 4 (Weeks 7-9) - Tooling & Ergonomics
8. **Development Tooling** (LSP, formatter, linter) - HIGH
9. **Memory Ownership Semantics** - MEDIUM
10. **Type Inference Improvements** - MEDIUM
11. **String Handling Improvements** - MEDIUM

---

### Sprint 5 (Weeks 10-12) - Language Features
12. **Pattern Matching** - MEDIUM
13. **Optional Types** - MEDIUM
14. **Standard Library Expansion** - MEDIUM

---

### Sprint 6 (Future) - Polish
15. **Defer Statements** - ✅ COMPLETE
16. **Built-in Profiler** - LOW
17. **Optional Components in Queries** - LOW

---

## Notes

- **Hot-Reloading (CONTINUUM)** is ✅ **100% COMPLETE** - Systems, Shaders, Components all working
- **Resource Handles** are ✅ **COMPLETE** - Texture, Mesh, Audio with hot-reload support
- Focus on **Sprint 1** first - these are blocking usability
- **Component Auto-Registration** unlocks editor tools and serialization
- **Pipeline Declaration** removes massive Vulkan boilerplate
- **Tooling (LSP)** is essential for professional adoption

---

*Last updated: After audio resource implementation*  
*Next milestone: Query iteration syntax*

