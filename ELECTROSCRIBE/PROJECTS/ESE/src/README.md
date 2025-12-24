# ESE (Echo Synapse Editor) - Source Code

This directory contains the refactored ESE editor source code, extracted from
`vulkan/eden_vulkan_helpers.cpp` for better modularity and maintainability.

## Module Overview

### Core Files

| File | Purpose |
|------|---------|
| `ese_types.h` | Core data types, enums, and state structures |
| `ese_editor.h` | Main editor class that combines all subsystems |

### Subsystems

| Module | Header | Source | Purpose |
|--------|--------|--------|---------|
| **Camera** | `ese_camera.h` | `ese_camera.cpp` | Orbit camera with pan/rotate/zoom |
| **Selection** | `ese_selection.h` | `ese_selection.cpp` | Vertex/edge/face/quad picking |
| **Gizmo** | `ese_gizmo.h` | `ese_gizmo.cpp` | Move/scale/rotate transform gizmos |
| **Operations** | `ese_operations.h` | `ese_operations.cpp` | Extrusion, edge loops, etc. |
| **Undo** | `ese_undo.h` | `ese_undo.cpp` | Undo/redo system |

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│                      ESE Editor                          │
│  ┌─────────┐ ┌───────────┐ ┌───────┐ ┌────────────────┐│
│  │ Camera  │ │ Selection │ │ Gizmo │ │ MeshOperations ││
│  └─────────┘ └───────────┘ └───────┘ └────────────────┘│
│  ┌───────────────────────────────────────────────────┐ │
│  │                   UndoManager                      │ │
│  └───────────────────────────────────────────────────┘ │
│  ┌───────────────────────────────────────────────────┐ │
│  │                   EditorState                      │ │
│  └───────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────┐
│                  MeshResource (stdlib)                   │
└─────────────────────────────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────┐
│                 Vulkan Renderer (eden)                   │
└─────────────────────────────────────────────────────────┘
```

## Data Flow

1. **Input** → `Editor::processInput()` → `Camera`, `Selection`, `Gizmo`
2. **Selection** → `MeshOperations` → `MeshResource` → `UndoManager`
3. **Render** → `Camera` matrices → `MeshResource` → Vulkan command buffer
4. **UI** → ImGui → `Editor` properties → HDM save/load

## Selection Modes

| Key | Mode | Description |
|-----|------|-------------|
| `1` | Vertex | Select individual vertices |
| `2` | Edge | Select quad edges (not diagonals) |
| `3` | Face | Select triangles |
| `4` | Quad | Select reconstructed quads |

## Operations

| Key | Operation | Requirements |
|-----|-----------|--------------|
| `W` | Extrude mode | Quad mode, has selection |
| `I` | Insert edge loop | Edge mode, edge selected |
| `Ctrl+Z` | Undo | Any mode |
| `Ctrl+Y` | Redo | After undo |

## Dependencies

- **glm** - Vector/matrix math
- **GLFW** - Window/input
- **Vulkan** - Rendering
- **ImGui** - UI (optional, USE_IMGUI)
- **MeshResource** - stdlib mesh class
- **HDM Format** - vulkan/formats/hdm_format.h

## Integration

### From HEIDIC code:
```cpp
// ese.hd or main.cpp
heidic_init_ese(window);
// In render loop:
heidic_render_ese(window);
// Cleanup:
heidic_cleanup_ese();
```

### From C++ code:
```cpp
#include "ese_editor.h"

ese::Editor editor;
editor.init(window);
// In render loop:
editor.render(window);
// Cleanup:
editor.cleanup();
```

## Building

Add these source files to your build:
- `ese_camera.cpp`
- `ese_selection.cpp`
- `ese_gizmo.cpp`
- `ese_operations.cpp`
- `ese_undo.cpp`
- `ese_editor.cpp` (when implemented)

Include paths:
- `ELECTROSCRIBE/PROJECTS/ESE/src/`
- `stdlib/` (for MeshResource)
- `vulkan/formats/` (for HDM)
- `third_party/imgui/` (if USE_IMGUI)

