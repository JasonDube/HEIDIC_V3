# Refactor Status - What We Actually Have

## The Original Problem
`eden_vulkan_helpers.cpp` was a ~3000+ line monolithic file containing EVERYTHING:
- Vulkan init, swapchain, pipelines
- ImGui integration
- Mesh rendering
- Camera, gizmos, selection
- File dialogs, HDM format
- Raycasting, and more...

## What We Extracted (Reusable Modules)

### 1. Utility Modules (in `vulkan/`)
```
vulkan/utils/raycast.h/.cpp      - Ray-AABB, ray-triangle, ray-plane intersections
vulkan/platform/file_dialog.h/.cpp - Native Windows file dialogs (NFD wrapper)
vulkan/formats/hdm_format.h/.cpp  - HDM file format read/write
```

### 2. ESE Editor Logic (in `ELECTROSCRIBE/PROJECTS/ESE/src/`)
```
ese_types.h       - Data structures (SelectionMode, GizmoState, etc.)
ese_camera.h/.cpp - Orbit camera math
ese_selection.h/.cpp - Selection/picking logic
ese_gizmo.h/.cpp  - Transform gizmo rendering & interaction
ese_operations.h/.cpp - Mesh operations (extrude, edge loop)
ese_undo.h/.cpp   - Undo/redo stack
```

### 3. SLAG_LEGION Engine (in `ELECTROSCRIBE/PROJECTS/SLAG_LEGION/engine/`)
```
sl_engine.h/.cpp  - NEW standalone Vulkan engine for the game
sl_api.h/.cpp     - C API that HEIDIC calls
```

## Current Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                        HEIDIC                               │
│              (Transpiles .hd → .cpp)                        │
│         Calls extern "C" functions from either:             │
└─────────────────┬───────────────────────┬───────────────────┘
                  │                       │
                  ▼                       ▼
┌─────────────────────────────┐ ┌─────────────────────────────┐
│           ESE               │ │       SLAG_LEGION           │
│                             │ │                             │
│  uses: eden_vulkan_helpers  │ │  uses: sl_engine.cpp        │
│        + ImGui              │ │        (standalone)         │
│                             │ │                             │
│  WHY: Full editor UI needs  │ │  WHY: Simple game loop,     │
│  deep ImGui integration     │ │  no ImGui needed            │
│  that's baked into the      │ │                             │
│  original file              │ │  CAN USE: raycast.h,        │
│                             │ │  file_dialog.h (optional)   │
└─────────────────────────────┘ └─────────────────────────────┘
```

## What This Means

### ESE (Echo Synapse Editor)
- **Still uses `eden_vulkan_helpers.cpp`** - the ImGui windows, gizmo rendering, 
  and editor UI are tightly coupled to that file
- The extracted modules (`ese_camera.cpp`, etc.) exist but aren't wired in yet
- To fully decouple ESE would require rewriting ImGui integration from scratch

### SLAG_LEGION (Game)
- **Uses NEW `sl_engine.cpp`** - completely independent of legacy code
- Simple Vulkan renderer (cubes, basic meshes)
- No ImGui dependency
- Can use extracted modules like `raycast.h` for picking

### Extracted Modules
- `raycast.h` - Used by SLAG_LEGION for object picking
- `file_dialog.h` - Can be used by any project needing file dialogs
- `hdm_format.h` - Can be used by any project loading HDM files

## The Honest Summary

| Component | Status | Uses Legacy Code? |
|-----------|--------|-------------------|
| ESE | Working | YES - `eden_vulkan_helpers.cpp` |
| SLAG_LEGION | Working | NO - `sl_engine.cpp` |
| raycast module | Extracted | Independent |
| file_dialog module | Extracted | Independent |
| hdm_format module | Extracted | Independent |
| ese_camera/gizmo/etc | Extracted | NOT WIRED IN to ESE yet |

## Why ESE Still Needs Legacy Code

The `eden_vulkan_helpers.cpp` file has:
1. **ImGui Vulkan backend** - texture uploads, render pipeline
2. **ImGui draw integration** - rendering UI over 3D scene
3. **Editor-specific rendering** - gizmo overlays, selection highlights
4. **Complex state management** - undo/redo tied to ImGui widgets

Extracting all this would essentially mean rewriting ImGui integration.
For now, ESE works with the original file, and that's fine.

## HEIDIC's Role

HEIDIC is just a **language transpiler**:
```
your_game.hd  →  HEIDIC transpiler  →  your_game.cpp
```

The `.cpp` calls `extern "C"` functions like:
- `heidic_init_vulkan()`
- `heidic_render_frame()`
- `heidic_load_mesh()`

These functions are implemented in EITHER:
- `eden_vulkan_helpers.cpp` (for ESE)
- `sl_engine.cpp` + `sl_api.cpp` (for SLAG_LEGION)

HEIDIC doesn't care which backend - it just needs the C functions to exist.

