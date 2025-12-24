# EDEN Engine Architecture

## Overview

```
┌─────────────────────────────────────────────────────────────────────┐
│                        HEIDIC SCRIPTS (.hd)                         │
│                   Game logic, UI flow, app behavior                 │
└────────────────────────────────┬────────────────────────────────────┘
                                 │ extern fn calls (C ABI)
                                 ▼
┌─────────────────────────────────────────────────────────────────────┐
│                         PROJECT API LAYER                           │
│              ease_api.cpp, sl_api.cpp, etc.                        │
│              Bridges HEIDIC → C++ Engine                            │
└────────────────────────────────┬────────────────────────────────────┘
                                 │ C++ method calls
                                 ▼
┌─────────────────────────────────────────────────────────────────────┐
│                       PROJECT ENGINE LAYER                          │
│            ease_engine.cpp, sl_engine.cpp, etc.                    │
│            App-specific logic (camera, input, game state)           │
└────────────────────────────────┬────────────────────────────────────┘
                                 │ uses
                                 ▼
┌─────────────────────────────────────────────────────────────────────┐
│                          VULKAN CORE                                │
│                    vulkan/core/vulkan_core.cpp                      │
│     THE shared foundation - pipelines, meshes, buffers, ImGui       │
└────────────────────────────────┬────────────────────────────────────┘
                                 │ Vulkan API
                                 ▼
┌─────────────────────────────────────────────────────────────────────┐
│                         GPU / VULKAN DRIVER                         │
└─────────────────────────────────────────────────────────────────────┘
```

## Core Modules

### 1. VulkanCore (`vulkan/core/`)
**The ONE place for Vulkan boilerplate.**

| File | Lines | Purpose |
|------|-------|---------|
| `vulkan_core.h` | ~430 | API definition, config structs, handles |
| `vulkan_core.cpp` | ~1500 | Implementation of all Vulkan operations |

**What it provides:**
- Window/swapchain management
- Pipeline creation (configurable vertex formats)
- Mesh upload/draw
- Camera/view matrices
- **ImGui integration** (optional, via `initImGui()`)

**What it does NOT do:**
- Game logic
- Input handling (beyond what GLFW provides)
- File loading (OBJ, textures, etc.)

### 2. Utility Modules (`vulkan/utils/`)

| File | Purpose |
|------|---------|
| `raycast.h/cpp` | AABB intersection, screen-to-world |
| `hdm_format.h/cpp` | HDM file format (future) |

### 3. Platform Modules (`vulkan/platform/`)

| File | Purpose |
|------|---------|
| `file_dialog.h/cpp` | Native file dialogs (NFD wrapper) |

## Project Structure

Each project in `ELECTROSCRIBE/PROJECTS/` follows this pattern:

```
ProjectName/
├── project_name.hd      # HEIDIC entry point
├── .project_config      # Build configuration
├── engine/
│   ├── name_engine.h    # C++ engine header
│   ├── name_engine.cpp  # C++ engine implementation
│   └── name_api.cpp     # C API bridge for HEIDIC
└── shaders/
    ├── *.vert           # GLSL vertex shaders
    ├── *.frag           # GLSL fragment shaders
    └── *.spv            # Compiled SPIR-V (auto-generated)
```

## .project_config Options

```ini
# Use VulkanCore (true) or legacy monolith (false)
use_modular_engine=true

# Source files to compile (relative to project root)
engine_sources=engine/ease_engine.cpp,engine/ease_api.cpp,../../../vulkan/core/vulkan_core.cpp

# Enable ImGui UI toolkit
enable_imgui=true

# Enable NEUROSHELL 2D overlay system
enable_neuroshell=false
```

## Shader Requirements

Shaders must match VulkanCore's `StandardUBO` structure:

```glsl
// VulkanCore StandardUBO (set 0, binding 0)
layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 model;       // Per-object transform
    mat4 view;        // Camera view matrix
    mat4 proj;        // Projection matrix
    vec4 color;       // Object color multiplier
} ubo;
```

## Adding ImGui to a Project

1. Set `enable_imgui=true` in `.project_config`
2. In engine init: `m_core.initImGui(m_window);`
3. In render loop:
   ```cpp
   m_core.beginFrame();
   m_core.beginImGuiFrame();
   
   // Your ImGui calls here
   ImGui::Begin("Window");
   ImGui::End();
   
   // Draw 3D content
   m_core.bindPipeline(m_pipeline);
   m_core.drawMesh(...);
   
   // Render ImGui on top
   m_core.renderImGui();
   m_core.endFrame();
   ```

## Design Principles

1. **Single Source of Truth**: VulkanCore is THE place for Vulkan code
2. **Clean Separation**: Each layer has one job
3. **No Duplication**: Utility code goes in shared modules
4. **Explicit Dependencies**: `.project_config` declares what's used
5. **Documentation**: Every module explains its purpose

## Current Status

| Project | Status | Backend |
|---------|--------|---------|
| EaSE | ✅ Working | VulkanCore |
| VKCORE_TEST | ✅ Working | VulkanCore |
| SLAG_LEGION | ⏸️ Shelved | Legacy (needs rewrite) |
| ESE | ⏸️ Shelved | Legacy (needs rewrite) |

