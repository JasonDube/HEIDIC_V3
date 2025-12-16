# HEIDIC_v2 Language Tweak To-Do List

This document tracks language feature improvements and enhancements for HEIDIC_v2.

## In Progress 🚧

*(No features currently in progress)*

## The Final 5 Tweaks (From GROK's Review) 🚀

These are the features that will make HEIDIC legendary:

- [x] **1. Hot-Reloading by Default** - `@hot` attribute on systems/components/shaders
  - Status: 🟡 **75% Complete**
  - Priority: CRITICAL (The Killer Feature)
  - Effort: ~3-5 days remaining (entity storage integration)
  - Impact: Zero-downtime iteration - the feature that made Jai famous
  - **✅ System Hot-Reload**: Fully implemented and working
  - **✅ Shader Hot-Reload**: Fully implemented and working  
  - **🟡 Component Hot-Reload**: Foundation complete (parsing, metadata, migration templates), needs entity storage integration

- [ ] **2. Built-in Resource Handles** - `resource Image = "textures/brick.png"`
  - Status: 🔴 Not Started
  - Priority: HIGH
  - Effort: ~3-5 days
  - Impact: Eliminates 90% of asset loading boilerplate

- [ ] **3. Zero-Boilerplate Pipeline Creation** - `pipeline pbr { shader vertex "...", layout { ... } }`
  - Status: 🔴 Not Started
  - Priority: HIGH
  - Effort: ~1 week
  - Impact: 400 lines of Vulkan boilerplate → 10 lines of HEIDIC

- [ ] **4. Automatic Bindless Integration** - Any `resource Image` auto-registers in bindless heap
  - Status: 🔴 Not Started
  - Priority: MEDIUM
  - Effort: ~3-5 days
  - Impact: Zero manual descriptor updates ever again

- [ ] **5. CUDA/OptiX Interop** - `@[cuda]` and `@[launch(kernel = ...)]` attributes
  - Status: 🔴 Not Started
  - Priority: MEDIUM (but HIGH value for ray tracing)
  - Effort: ~1-2 weeks
  - Impact: Only indie engine with seamless CPU → GPU → Ray-tracing data flow

## Critical Ergonomics (From Claude's Review) ⚠️

**Note:** Gemini emphasizes that compiler and tooling polish are **essential for professional workflows and adoption in complex projects**. These are not optional - they're critical for adoption.

- [x] **Query Iteration Syntax** - `for entity in q` syntax for ECS queries
  - Status: ✅ **COMPLETE** - Implemented and tested!
  - Priority: CRITICAL (Blocks ECS usability)
  - Effort: ~2-3 days
  - Impact: Without this, ECS feels incomplete. Claude: "This is critical for usability."
  - Implementation: ✅ Added `for entity in q` syntax to parser, generate iteration code, handle AoS/SOA access patterns
  - Test Files: `ELECTROSCRIBE/PROJECTS/OLD PROJECTS/examples/query_iteration_example.hd`, `query_test/query_test.hd`

- [x] **SOA Access Pattern Clarity** - Make `entity.Velocity.x` work transparently
  - Status: ✅ **COMPLETE** - Transparent SOA access implemented!
  - Priority: HIGH
  - Effort: ~1 week
  - Impact: SOA is great for storage, but access pattern needs to be crystal clear
  - Implementation: ✅ Hide SOA complexity - compiler generates `velocities.x[entity_index]` behind the scenes
  - Test Files: `ELECTROSCRIBE/PROJECTS/OLD PROJECTS/soa_access_test/soa_access_test.hd`
  - Documentation: `DOCS/HEIDIC/SOA_ACCESS_PATTERN_IMPLEMENTATION.md`

- [x] **Better Error Messages** - Line numbers, context, suggestions
  - Status: ✅ **MOSTLY COMPLETE** - Enhanced error reporting implemented!
  - Priority: HIGH
  - Effort: ~1 week
  - Impact: Developer experience matters. Good error messages = faster iteration
  - Implementation: ✅ Added line/column tracking, context-aware errors, suggestions, multiple errors
  - Test Files: `ELECTROSCRIBE/PROJECTS/OLD PROJECTS/error_test/error_test.hd`
  - Documentation: `DOCS/HEIDIC/BETTER_ERROR_MESSAGES_IMPLEMENTATION.md`
  - Remaining: Some parser/lexer errors still use `bail!` instead of ErrorReporter

- [ ] **Memory Ownership Semantics** - RAII + compiler checks
  - Status: 🔴 Not Started
  - Priority: MEDIUM
  - Effort: ~1 week (RAII) or ~2-3 weeks (ownership)
  - Impact: Prevents use-after-free bugs
  - Implementation: Start with RAII (automatic cleanup), add compiler checks to prevent returning frame-scoped allocations

## Pending 📋

- [x] **Query Syntax Enhancement** - `query<Transform, Velocity>` for ECS
  - Status: ✅ Completed
  - Implementation: Added `Query(Vec<Type>)` to AST, parser support for `query<T1, T2, ...>` syntax, type checker validation (ensures query contains Component/ComponentSOA types), and codegen that generates query structs with iteration helpers
  - Syntax: `fn update(q: query<Position, Velocity>): void { }`
  - Generated as: `Query_Position_Velocity` struct with `for_each_query_position_velocity()` helper function
  - Note: Full `for entity in q` iteration syntax can be added later as an enhancement

- [x] **Compile-Time Shader Embedding** - `shader vertex "shaders/triangle.vert"`
  - Status: ✅ Completed
  - Implementation: Added `shader` keyword, shader stage keywords (vertex, fragment, compute, etc.), `ShaderDef` to AST, parser support, type checking, and codegen that compiles shaders with glslc and embeds SPIR-V bytecode as const arrays
  - Syntax: `shader vertex "path/to/shader.glsl" { }`
  - Generated as: Embedded SPIR-V bytecode arrays with helper functions `load_<name>_shader()` to load them
  - Requirements: glslc must be in PATH

- [x] **Frame-Scoped Memory (FrameArena)** - `frame.alloc_array<Vec3>(count)`
  - Status: ✅ Completed
  - Implementation: Added `FrameArena` type to AST and lexer, method call parsing for `frame.alloc_array<T>(count)` syntax, type checker validation, and codegen that generates a C++ FrameArena class with stack allocator
  - Syntax: `fn render_frame(frame: FrameArena): void { let positions = frame.alloc_array<Vec3>(count); }`
  - Generated as: C++ `FrameArena` class with `alloc_array<T>(count)` template method that returns `std::vector<T>`
  - Features: Automatic memory management - all allocations freed when FrameArena goes out of scope

- [ ] **Component Auto-Registration + Reflection** - Automatic component registry with metadata
  - Status: 🔴 Not Started
  - Priority: HIGH (The One Unchecked Box)
  - Effort: ~2-3 days
  - Impact: Unlocks editor tools, serialization, hot-reload, networking
  - Requirements:
    - Generate `ComponentRegistry::register_all()` at startup
    - Generate component metadata (ID, name, size, alignment, default constructor)
    - Generate reflection data (field names, types, offsets)
    - Generate serialization/deserialization helpers

- [x] **SOA (Structure of Arrays) for Mesh/Vertex/Index Data** - All mesh, vertex, and index data is SOA by default
  - Status: ✅ Completed
  - Implementation: Added `mesh_soa` keyword, `MeshSOADef` to AST, parser support, type checking (validates all fields are arrays), and codegen that generates SOA C++ structures
  - Syntax: `mesh_soa Mesh { positions: [Vec3], uvs: [Vec2], colors: [Vec3], indices: [i32] }`
  - Generated as: `struct Mesh { std::vector<Vec3> positions; std::vector<Vec2> uvs; ... }`

- [x] **SOA (Structure of Arrays) Mode for Components** - `component_soa Velocity { x: [f32], y: [f32], z: [f32] }`
  - Status: ✅ Completed
  - Implementation: Added `component_soa` keyword, `ComponentSOADef` to AST, parser support, type checking, and codegen
  - Benefits: SOA layout is cache-friendly for ECS iteration, better for vectorization, and aligns with CUDA/OptiX preferences
  - Syntax: `component_soa Velocity { x: [f32], y: [f32], z: [f32] }`

## Completed ✅

- [x] **System Dependency Declaration** - `@system(render, after = Physics, before = RenderSubmit)`
  - Status: ✅ Completed
  - Implementation: Added `@` token to lexer, `SystemAttribute` to AST with `after`/`before` dependencies, parser support for `@system(...)` attributes on functions, type checker validation (checks for undefined systems and circular dependencies), and codegen that generates a `run_systems()` scheduler function using topological sort
  - Syntax: `@system(render, after = Physics, before = RenderSubmit) fn update_transforms(q: query<Position, Velocity>): void { }`
  - Generated as: `run_systems()` function that calls systems in dependency order using Kahn's algorithm for topological sorting
  - Features: Validates all referenced systems exist, detects circular dependencies, generates ordered system execution

- [x] **SOA (Structure of Arrays) Support** - `mesh_soa` and `component_soa` keywords
  - Status: ✅ Completed
  - Implementation: Added SOA support for both mesh data (CUDA/OptiX optimized) and ECS components (cache-friendly iteration)
  - Syntax: `mesh_soa Mesh { positions: [Vec3], uvs: [Vec2] }` and `component_soa Velocity { x: [f32], y: [f32] }`
  - Generated as C++ structs with separate `std::vector` fields for each attribute (SOA layout)

- [x] **Vulkan Type Aliases** - `type ImageView = VkImageView;` syntax
  - Status: ✅ Completed
  - Implementation: Added `TypeAlias` to AST, parser, type checker, and codegen
  - Generated as C++ `using` statements

- [x] **Default Values in Components** - `scale: Vec3 = Vec3(1,1,1)` syntax
  - Status: ✅ Completed
  - Implementation: Extended `Field` to include optional default value, parser handles `= expression`, codegen generates constructors with default parameters

## Compiler & Tooling Polish (From Gemini's Review) ⚠️ **ESSENTIAL FOR ADOPTION**

- [ ] **Development Tooling** - LSP, Formatter, Linter
  - Status: 🔴 Not Started
  - Priority: HIGH (Essential for professional workflows)
  - Effort: ~2-3 weeks
  - Impact: Essential for professional workflows and adoption in complex projects
  - Gemini: "Develop core development tools: a Language Server (LSP) for IDE features like auto-completion and 'go-to-definition,' a dedicated Formatter (`heidic fmt`), and a Linter (`heidic lint`)."
  - Implementation:
    - Language Server (LSP): syntax highlighting, auto-completion, go-to-definition, error squiggles
    - Formatter: consistent code style, auto-format on save, configurable rules
    - Linter: style checks, best practices, performance warnings, unused code detection

- [ ] **Standard Library Expansion** - Collections, String Manipulation, File I/O, Algorithms
  - Status: 🔴 Not Started
  - Priority: MEDIUM
  - Effort: ~2-3 weeks
  - Impact: Boosts general-purpose utility and reduces reliance on generated C++ libraries
  - Gemini: "Expand the standard library beyond core engine bindings to include collections (HashMap, HashSet), string manipulation (split, join, format), file I/O, and common algorithms (sort, search)."
  - Implementation Plan:
    - Week 1: Collections (HashMap, HashSet, Vec operations)
    - Week 2: String manipulation (split, join, format, interpolation)
    - Week 3: File I/O and algorithms (sort, search, filter)

## Additional Features (From Claude's Review)

- [ ] **Type Inference Improvements** - Rust-style inference for arrays, function returns
  - Status: 🔴 Not Started
  - Priority: MEDIUM
  - Effort: ~2-3 days
  - Impact: Less boilerplate, more modern feel
  - Note: Test file exists: `type_inference_test.hd`

- [x] **String Handling** - Documentation, interpolation, manipulation functions
  - Status: ✅ **PARTIALLY COMPLETE** - String interpolation implemented!
  - Priority: MEDIUM
  - Effort: ~1 week (actual: ~2 hours for interpolation)
  - Impact: Clear string operations and ownership model
  - Test Files: `ELECTROSCRIBE/PROJECTS/OLD PROJECTS/string_interpolation_test/string_interpolation_test.hd`
  - Documentation: `DOCS/HEIDIC/STRING_HANDLING_IMPLEMENTATION.md`
  - Remaining: String variable interpolation bug, manipulation functions pending

- [x] **Pattern Matching** - `match` expression for error handling
  - Status: ✅ **COMPLETE** - Pattern matching implemented!
  - Priority: MEDIUM
  - Effort: ~1 week (actual: ~2 hours)
  - Impact: Makes error handling and state machines much cleaner
  - Test Files: `ELECTROSCRIBE/PROJECTS/OLD PROJECTS/pattern_matching_test/pattern_matching_test.hd`
  - Documentation: `DOCS/HEIDIC/PATTERN_MATCHING_IMPLEMENTATION.md`

- [x] **Optional Types** - `?T` syntax for nullable types
  - Status: ✅ **COMPLETE** - Optional types implemented!
  - Priority: MEDIUM
  - Effort: ~1 week (actual: ~2 hours)
  - Impact: Eliminates null pointer bugs
  - Test Files: `ELECTROSCRIBE/PROJECTS/OLD PROJECTS/optional_types_test/optional_types_test.hd`
  - Documentation: `DOCS/HEIDIC/OPTIONAL_TYPES_IMPLEMENTATION.md`

- [x] **Defer Statements** - `defer` for automatic cleanup
  - Status: ✅ **COMPLETE** - Defer statements implemented!
  - Priority: LOW
  - Effort: ~2-3 days (actual: ~2 hours)
  - Impact: Ensures cleanup code always runs
  - Test Files: `ELECTROSCRIBE/PROJECTS/OLD PROJECTS/defer_test/defer_test.hd`
  - Documentation: `DOCS/HEIDIC/DEFER_STATEMENTS_IMPLEMENTATION.md`

## Potential Issues to Address

- [ ] **GLFWwindow as Pointer Type** - Wrap as opaque handle (leaky abstraction)
  - Status: 🔴 Not Started
  - Priority: LOW
  - Effort: ~1 day

- [ ] **print() Type Safety** - Make variadic but type-checked (currently bypasses type safety)
  - Status: 🔴 Not Started
  - Priority: LOW
  - Effort: ~1 day

- [ ] **ImGui Namespace Consistency** - Pick one syntax (ImGui:: vs ImGui_)
  - Status: 🔴 Not Started
  - Priority: LOW
  - Effort: ~1 day

## Notes

- Features are prioritized by effort vs. value
- **Critical ergonomics** (query iteration, error messages) are now top priority (Claude's feedback)
- Quick wins (Vulkan aliases, default values) are completed
- Complex features (SOA, hot-reload) are in the roadmap
- Claude's review confirms: "The foundation is excellent. The remaining work is about ergonomics and safety, not architecture."

