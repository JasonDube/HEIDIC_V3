# HEIDIC Architecture - What Depends on What

## The Two Parts of HEIDIC

### Part 1: LANGUAGE FEATURES (Transpiler-Only)
These are **100% independent** - they just convert to C++, no runtime needed:

| Feature | What it does | Depends on monolith? |
|---------|--------------|---------------------|
| `let x: f32 = 10.0` | Type system | NO |
| `struct Point { x: f32, y: f32 }` | Structs | NO |
| `component Position { x: f32 }` | ECS components | NO |
| `mesh_soa Mesh { positions: [Vec3] }` | SOA types | NO |
| `query<Position, Velocity>` | ECS queries | NO |
| `@system(physics)` | System scheduling | NO |
| `shader vertex "file.vert" {}` | Shader embedding | NO |
| `FrameArena` | Frame-scoped memory | NO |
| `if/while/loop` | Control flow | NO |
| Type inference | Auto types | NO |

**These features are PURE transpiler magic.** They convert HEIDIC syntax → C++ code.
No runtime library needed.

### Part 2: RUNTIME FUNCTIONS (Need Backend)
These `extern fn heidic_*` functions need SOMEONE to implement them:

```heidic
extern fn heidic_init_renderer(window: GLFWwindow): i32;
extern fn heidic_begin_frame(): void;
extern fn heidic_end_frame(): void;
extern fn heidic_draw_cube(x: f32, y: f32, z: f32, ...): void;
extern fn heidic_draw_mesh(mesh_id: i32, ...): void;
extern fn heidic_update_camera(...): void;
extern fn heidic_load_ascii_model(filename: string): i32;
```

**These NEED a backend.** Currently we have:

| Backend | File(s) | Used by |
|---------|---------|---------|
| Monolith | `eden_vulkan_helpers.cpp` | ESE, older projects |
| Standalone | `sl_engine.cpp` + `sl_api.cpp` | SLAG_LEGION |

---

## Visual Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                    YOUR .HD FILE                                 │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │ LANGUAGE FEATURES (transpiler handles these)            │    │
│  │ - structs, components, mesh_soa                         │    │
│  │ - query<>, @system(), FrameArena                        │    │
│  │ - type inference, control flow                          │    │
│  └─────────────────────────────────────────────────────────┘    │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │ RUNTIME CALLS (need backend implementation)             │    │
│  │ - heidic_init_renderer()                                │    │
│  │ - heidic_draw_cube()                                    │    │
│  │ - heidic_begin_frame() / heidic_end_frame()            │    │
│  └─────────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
                    ┌─────────────────┐
                    │ HEIDIC COMPILER │
                    │ (Rust transpiler)│
                    └────────┬────────┘
                              │
                              ▼
                    ┌─────────────────┐
                    │   .CPP FILE     │
                    │ (generated C++) │
                    └────────┬────────┘
                              │
              ┌───────────────┴───────────────┐
              ▼                               ▼
    ┌─────────────────────┐       ┌─────────────────────┐
    │ eden_vulkan_helpers │       │ sl_engine.cpp       │
    │ (monolith backend)  │       │ (standalone backend)│
    │                     │       │                     │
    │ Implements:         │       │ Implements:         │
    │ - heidic_init_*     │       │ - heidic_init_*     │
    │ - heidic_draw_*     │       │ - heidic_draw_*     │
    │ - heidic_update_*   │       │ - heidic_update_*   │
    │ + ImGui integration │       │ (no ImGui)          │
    │ + Gizmos            │       │                     │
    │ + Selection         │       │                     │
    └─────────────────────┘       └─────────────────────┘
              │                               │
              ▼                               ▼
           ESE.exe                     SLAG_LEGION.exe
```

---

## What We Actually Extracted

### Reusable Modules (independent of any backend)
```
vulkan/utils/raycast.h       - Ray math (AABB, triangle, plane)
vulkan/platform/file_dialog.h - Windows file dialogs  
vulkan/formats/hdm_format.h   - HDM file format
```

### SLAG_LEGION's New Backend
```
SLAG_LEGION/engine/sl_engine.cpp - Vulkan setup, rendering
SLAG_LEGION/engine/sl_api.cpp    - heidic_* function implementations
```
This is **completely independent** of the monolith.

### ESE's Extracted Logic (not yet wired in)
```
ESE/src/ese_camera.cpp     - Orbit camera
ESE/src/ese_selection.cpp  - Selection/picking
ESE/src/ese_gizmo.cpp      - Transform gizmos
ESE/src/ese_operations.cpp - Mesh operations
ESE/src/ese_undo.cpp       - Undo/redo
```
These exist but ESE still uses the monolith because ImGui integration is complex.

---

## Summary

| Question | Answer |
|----------|--------|
| Are HEIDIC language features dependent on the monolith? | **NO** - pure transpiler |
| Are HEIDIC runtime functions dependent on the monolith? | **Depends on project** |
| Does ESE need the monolith? | **YES** - ImGui integration |
| Does SLAG_LEGION need the monolith? | **NO** - uses sl_engine.cpp |
| What did the refactor accomplish? | Created standalone backend + utility modules |

---

## The Path Forward

**Option A: Keep ESE on monolith** (current)
- ESE works with all features
- SLAG_LEGION uses clean standalone engine
- New simple projects can use standalone engine

**Option B: Fully extract ESE** (significant work)
- Would need to rewrite ImGui Vulkan integration
- Would need to wire up all ese_*.cpp modules
- Essentially rebuilding the editor

**Recommendation:** Option A is practical. The monolith isn't "bad" - it's battle-tested code.
The refactor gave us a clean path for NEW projects (like SLAG_LEGION).

