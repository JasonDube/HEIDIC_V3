# Eden Vulkan Helpers Refactoring Status

## ⚠️ IMPORTANT NOTE (December 2024)

The extracted modules exist but are **NOT currently included** in `eden_vulkan_helpers.cpp` to avoid duplicate definitions. The original code remains in place for backward compatibility.

**For new projects**, include the extracted modules directly:
```cpp
#include "vulkan/utils/raycast.h"
#include "vulkan/platform/file_dialog.h"
#include "vulkan/formats/hdm_format.h"
#include "vulkan/renderers/mesh_renderer.h"
```

**For existing projects** (like SLAG LEGION, ESE), continue using the monolithic `eden_vulkan_helpers.h/.cpp`.

---

## Completed Extractions

### 1. Raycast Utilities ✅
**Location:** `vulkan/utils/raycast.h` & `raycast.cpp`

**Functions:**
- `screenToNDC()` - Convert screen to NDC coordinates
- `unproject()` - Create world ray from screen point
- `rayAABB()` - Ray-AABB intersection
- `rayTriangle()` - Ray-triangle intersection
- `rayPlane()` - Ray-plane intersection
- `pointLineDistanceSq()` - Point-to-line distance
- `projectToScreen()` - Project world point to screen
- `createCubeAABB()` - Create AABB for a cube

**Usage:**
```cpp
#include "utils/raycast.h"

glm::vec3 rayOrigin, rayDir;
unproject(ndc, invProj, invView, cameraPos, rayOrigin, rayDir);

AABB box = createCubeAABB(x, y, z, sx, sy, sz);
float tMin, tMax;
if (rayAABB(rayOrigin, rayDir, box, tMin, tMax)) {
    // Hit!
}
```

---

### 2. File Dialogs ✅
**Location:** `vulkan/platform/file_dialog.h` & `file_dialog.cpp`

**C Functions:**
- `nfd_open_file_dialog()` - Generic file open dialog
- `nfd_save_file_dialog()` - Generic file save dialog
- `nfd_open_obj_dialog()` - OBJ file dialog
- `nfd_open_texture_dialog()` - Texture file dialog
- `nfd_open_hdm_dialog()` - HDM file dialog
- `nfd_save_hdm_dialog()` - HDM save dialog
- `nfd_browse_folder_dialog()` - Folder browser

**C++ Wrappers:**
```cpp
#include "platform/file_dialog.h"

std::string file = eden::dialog::openOBJ();
std::string tex = eden::dialog::openTexture();
std::string hdm = eden::dialog::saveHDM("model.hdma");
```

---

### 3. HDM Format ✅
**Location:** `vulkan/formats/hdm_format.h` & `hdm_format.cpp`

**Features:**
- Binary format (.hdm) - compact, fast loading
- ASCII format (.hdma) - human-readable JSON with base64 geometry
- Properties-only JSON - lightweight metadata
- Base64 encoding/decoding
- JSON parsing utilities

**Usage:**
```cpp
#include "formats/hdm_format.h"

HDMProperties props;
HDMGeometry geom;
HDMTexture tex;

// Initialize defaults
eden::hdm::initDefaultProperties(props);

// Load (auto-detects format)
eden::hdm::load("model.hdma", props, geom, tex);

// Save ASCII
eden::hdm::saveAscii("output.hdma", props, geom, tex);

// Save binary
eden::hdm::saveBinary("output.hdm", props, geom, tex);
```

**C API:**
```c
hdm_load_file("model.hdma");
HDMItemPropertiesC item = hdm_get_item_properties();
HDMPhysicsPropertiesC phys = hdm_get_physics_properties();
```

---

### 4. ImGui Integration ✅
**Location:** `vulkan/eden_imgui.h` & `eden_imgui.cpp`

**Functions:**
- `heidic_init_imgui()` - Initialize ImGui
- `heidic_imgui_new_frame()` - Begin frame
- `heidic_imgui_render()` - Render to command buffer
- `heidic_imgui_render_demo_overlay()` - Demo FPS overlay
- `heidic_cleanup_imgui()` - Cleanup
- `heidic_imgui_want_capture_mouse()` - Check mouse capture
- `heidic_imgui_want_capture_keyboard()` - Check keyboard capture

**C++ Helper:**
```cpp
#include "eden_imgui.h"

eden::ImGuiContext::init(window);

// In render loop:
eden::ImGuiContext::beginFrame();
// ... ImGui calls ...
eden::ImGuiContext::endFrame(commandBuffer);

eden::ImGuiContext::cleanup();
```

---

## Remaining Extractions (TODO)

### 5. ESE Editor ✅
**Location:** `ELECTROSCRIBE/PROJECTS/ESE/src/`

The ESE editor has been extracted to its own module with these files:
- `ese_types.h` - Core data types and state structures
- `ese_editor.h` - Main editor class
- `ese_camera.h/.cpp` - Orbit camera system
- `ese_selection.h/.cpp` - Vertex/edge/face/quad picking
- `ese_gizmo.h/.cpp` - Move/scale/rotate gizmos
- `ese_operations.h/.cpp` - Extrusion, edge loops, etc.
- `ese_undo.h/.cpp` - Undo/redo system
- `README.md` - Module documentation

**Usage:**
```cpp
#include "ese_editor.h"

ese::Editor editor;
editor.init(window);
// In render loop:
editor.render(window);
// Cleanup:
editor.cleanup();
```

### 6. Mesh Renderer ✅
**Location:** `vulkan/renderers/mesh_renderer.h/.cpp`

OBJ mesh rendering with textures and hot-reload support.

**Features:**
- `eden::MeshRenderer` - Self-contained renderer class
- `eden::MeshUBO` - Uniform buffer structure
- `eden::MeshRendererConfig` - Configuration options
- Pipeline creation (filled and wireframe)
- Texture hot-reload
- Dummy texture fallback

**Usage:**
```cpp
eden::MeshRenderer renderer;
renderer.init(device, physicalDevice, renderPass, commandPool, graphicsQueue);
renderer.loadMesh("model.obj");
renderer.loadTexture("texture.png");
renderer.setMatrices(model, view, proj);
renderer.render(commandBuffer);
```

### 7. Cleanup Main File ✅
Updated `eden_vulkan_helpers.cpp` to:
- Include the new modules for shared types
- Added documentation header pointing to new modules
- Maintains full backward compatibility

**Note:** The original code in `eden_vulkan_helpers.cpp` remains functional
for existing projects. New development should use the extracted modules.

---

## Final Directory Structure

```
vulkan/
├── eden_vulkan_helpers.cpp    # Core Vulkan + legacy code (backward compat)
├── eden_vulkan_helpers.h      # Core header with module documentation
├── eden_imgui.cpp             # ImGui integration
├── eden_imgui.h
├── REFACTORING_STATUS.md      # This file
├── utils/
│   ├── raycast.cpp            # Ray intersection utilities
│   └── raycast.h
├── platform/
│   ├── file_dialog.cpp        # Native file dialogs (Windows)
│   └── file_dialog.h
├── formats/
│   ├── hdm_format.cpp         # HDM file format (binary + ASCII)
│   └── hdm_format.h
└── renderers/
    ├── mesh_renderer.cpp      # OBJ mesh renderer class
    └── mesh_renderer.h

ELECTROSCRIBE/PROJECTS/ESE/src/
├── ese_types.h                # Core data types
├── ese_editor.h               # Main editor class
├── ese_camera.cpp/.h          # Orbit camera
├── ese_selection.cpp/.h       # Picking system
├── ese_gizmo.cpp/.h           # Transform gizmos
├── ese_operations.cpp/.h      # Mesh operations
├── ese_undo.cpp/.h            # Undo/redo system
└── README.md                  # Module documentation
```

---

## Migration Guide

### For Existing Code (No Changes Needed)

Existing code using `eden_vulkan_helpers.h` continues to work unchanged.
The original C API is fully preserved:

```c
// Still works!
heidic_init_renderer(window);
heidic_init_renderer_obj_mesh(window, "model.obj", "texture.png");
heidic_render_obj_mesh(window);
nfd_open_file_dialog("*.obj", NULL);
```

### For New C++ Code (Recommended)

Use the extracted modules directly for cleaner, more maintainable code:

```cpp
// Include specific modules
#include "vulkan/utils/raycast.h"
#include "vulkan/platform/file_dialog.h"
#include "vulkan/formats/hdm_format.h"
#include "vulkan/renderers/mesh_renderer.h"
#include "vulkan/eden_imgui.h"

// Raycast example
glm::vec3 rayOrigin, rayDir;
unproject(ndc, invProj, invView, cameraPos, rayOrigin, rayDir);
AABB box = createCubeAABB(x, y, z, sx, sy, sz);
if (rayAABB(rayOrigin, rayDir, box, tMin, tMax)) { /* hit! */ }

// File dialog example
std::string file = eden::dialog::openOBJ();
std::string tex = eden::dialog::openTexture();

// HDM format example
HDMProperties props;
HDMGeometry geom;
HDMTexture tex;
eden::hdm::initDefaultProperties(props);
eden::hdm::load("model.hdma", props, geom, tex);
eden::hdm::saveAscii("output.hdma", props, geom, tex);

// Mesh renderer example
eden::MeshRenderer renderer;
renderer.init(device, physicalDevice, renderPass, commandPool, queue);
renderer.loadMesh("model.obj");
renderer.loadTexture("texture.png");
renderer.setMatrices(model, view, proj);
renderer.render(commandBuffer);
```

### For ESE Development

The ESE editor is now in its own module:

```cpp
#include "ELECTROSCRIBE/PROJECTS/ESE/src/ese_editor.h"
#include "ELECTROSCRIBE/PROJECTS/ESE/src/ese_camera.h"
#include "ELECTROSCRIBE/PROJECTS/ESE/src/ese_selection.h"
#include "ELECTROSCRIBE/PROJECTS/ESE/src/ese_operations.h"

// Use individual components
ese::Camera camera;
ese::Selection selection;
ese::Gizmo gizmo;
ese::UndoManager undo;

// Or use the integrated editor class
ese::Editor editor;
editor.init(window);
editor.render(window);
```

### Build System Updates

Add new source files to your build:

```
# Core modules
vulkan/utils/raycast.cpp
vulkan/platform/file_dialog.cpp
vulkan/formats/hdm_format.cpp
vulkan/eden_imgui.cpp
vulkan/renderers/mesh_renderer.cpp

# ESE modules (if using ESE)
ELECTROSCRIBE/PROJECTS/ESE/src/ese_camera.cpp
ELECTROSCRIBE/PROJECTS/ESE/src/ese_selection.cpp
ELECTROSCRIBE/PROJECTS/ESE/src/ese_gizmo.cpp
ELECTROSCRIBE/PROJECTS/ESE/src/ese_operations.cpp
ELECTROSCRIBE/PROJECTS/ESE/src/ese_undo.cpp
```

### Link Dependencies

- **Windows:** `comdlg32.lib`, `shell32.lib` (for file dialogs)
- **ImGui:** `imgui.lib` or sources (for UI)
- **GLM:** Header-only (for math)

---

## Benefits of Refactoring

1. **Maintainability** - Smaller, focused files (~200-850 lines each)
2. **Testability** - Can unit test individual modules
3. **Reusability** - Raycast, HDM format, mesh renderer can be used in other projects
4. **Compilation** - Faster incremental builds (only recompile changed modules)
5. **Readability** - Clear separation of concerns
6. **Flexibility** - Use only the modules you need
7. **Backward Compatible** - Existing code continues to work

---

## Summary

| Module | Lines | Purpose | Backward Compat |
|--------|-------|---------|-----------------|
| raycast | ~180 | Ray intersection | ✅ |
| file_dialog | ~250 | Native OS dialogs | ✅ |
| hdm_format | ~650 | HDM file format | ✅ |
| eden_imgui | ~220 | ImGui integration | ✅ |
| mesh_renderer | ~850 | OBJ rendering | ✅ |
| ESE modules | ~1200 | 3D mesh editor | ✅ |

**Total extracted:** ~3350 lines into clean, reusable modules.

---

*Refactoring completed: December 2024*

