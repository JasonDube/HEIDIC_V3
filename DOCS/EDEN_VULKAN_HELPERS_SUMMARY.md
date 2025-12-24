# EDEN Vulkan Helpers - Functional Summary

## What This File Does

`vulkan/eden_vulkan_helpers.cpp` is the **core rendering implementation** for the EDEN Engine. It provides the Vulkan backend that powers all graphics in HEIDIC applications.

---

## Core Systems

### 1. Vulkan Renderer Initialization

**Entry Point:** `heidic_init_renderer(GLFWwindow* window)`

Initializes the complete Vulkan rendering pipeline:

```
1. Create Vulkan Instance
2. Setup Debug Messenger (validation layers)
3. Create Window Surface
4. Select Physical Device (GPU)
5. Find Graphics Queue Family
6. Create Logical Device
7. Create Swapchain
8. Create Image Views
9. Create Render Pass
10. Create Descriptor Set Layout
11. Load Shaders (custom or default)
12. Create Graphics Pipeline
13. Create Depth Buffer
14. Create Framebuffers
15. Create Command Pool & Buffers
16. Create Uniform Buffers
17. Create Descriptor Pool & Sets
18. Create Synchronization Primitives
```

**Related Functions:**
- `heidic_glfw_vulkan_hints()` - Set GLFW window hints for Vulkan
- `heidic_create_fullscreen_window()` - Create fullscreen window
- `heidic_create_borderless_window()` - Create borderless window
- `heidic_cleanup_renderer()` - Destroy all Vulkan resources

---

### 2. Frame Rendering

**Entry Point:** `heidic_render_frame()`

Renders a single frame:

```
1. Wait for previous frame fence
2. Acquire next swapchain image
3. Reset fence
4. Update uniform buffer (MVP matrices)
5. Reset & begin command buffer
6. Begin render pass
7. Bind pipeline & descriptor sets
8. Draw geometry
9. End render pass
10. Submit command buffer
11. Present image
```

**Uniform Buffer Structure:**
```cpp
struct UniformBufferObject {
    glm::mat4 model;  // Object transform
    glm::mat4 view;   // Camera view matrix
    glm::mat4 proj;   // Projection matrix
};
```

---

### 3. Cube Renderer

**Purpose:** Renders a spinning 3D cube with colored vertices

**Functions:**
- `heidic_init_cube_renderer()` - Initialize cube pipeline with cube shaders
- `heidic_render_cube_frame()` - Render rotating cube
- `heidic_cleanup_cube_renderer()` - Cleanup resources

**Features:**
- Colored vertices (different color per face)
- Rotation animation
- Depth testing enabled

---

### 4. FPS Camera Renderer

**Purpose:** First-person camera system with WASD + mouse controls

**Functions:**
- `heidic_init_fps_renderer()` - Initialize FPS camera
- `heidic_render_fps_frame()` - Render scene with FPS camera
- `heidic_fps_update_camera()` - Update camera from input
- `heidic_fps_set_camera_position()` - Set camera position directly

**Camera State:**
```cpp
glm::vec3 position;    // Camera position in world
glm::vec3 front;       // Look direction
glm::vec3 up;          // Up vector
float yaw, pitch;      // Euler angles
float speed;           // Movement speed
float sensitivity;     // Mouse sensitivity
```

**Controls:**
- WASD - Move
- Mouse - Look
- Space/Ctrl - Up/Down

---

### 5. OBJ Mesh Renderer

**Purpose:** Load and render OBJ files with textures

**Functions:**
- `heidic_init_obj_mesh_renderer()` - Initialize mesh rendering pipeline
- `heidic_load_obj_mesh()` - Load OBJ file into GPU buffers
- `heidic_render_obj_mesh_frame()` - Render the loaded mesh
- `heidic_load_obj_mesh_texture()` - Load texture for mesh

**Features:**
- OBJ file parsing (via MeshResource class)
- Texture mapping (DDS, PNG support)
- Hot-reload detection for textures
- Per-vertex normals and UVs

**Vertex Format:**
```cpp
struct MeshVertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;
};
```

---

### 6. ESE - Echo Synapse Editor

**Purpose:** Full 3D model editor with ImGui UI

**Entry Points:**
- `ese_init(GLFWwindow* window)` - Initialize ESE
- `ese_render_frame()` - Render editor frame
- `ese_cleanup()` - Cleanup ESE resources

#### 6.1 Camera System

**Orbit Camera:**
- Yaw (horizontal rotation)
- Pitch (vertical rotation)
- Distance (zoom level)
- Target point (orbit center)

**Controls:**
- Left-drag: Rotate camera
- Right-drag: Pan camera
- Scroll wheel: Zoom

#### 6.2 Selection Modes

| Key | Mode | Selectable |
|-----|------|------------|
| 1 | Vertex | Individual vertices |
| 2 | Edge | Edges between vertices |
| 3 | Face | Triangle faces |
| 4 | Quad | Reconstructed quads |

**Selection Features:**
- Click to select single element
- Ctrl+click for multi-select
- Visual highlighting of selected elements

#### 6.3 Transform Gizmos

**Move Gizmo:**
- X axis (red)
- Y axis (green)
- Z axis (blue)
- Drag to move along axis

**Scale Gizmo (press S):**
- Same axis visualization
- Drag to scale along axis
- Scales from selection center

**Rotate Gizmo (press R):**
- Same axis visualization
- Drag to rotate around axis
- Rotates around selection center

#### 6.4 Mesh Operations

**Extrusion (W key in quad mode):**
1. Select one or more quads
2. Press W to enter extrude mode
3. Drag gizmo to extrude
4. Creates new geometry, extends mesh

**Edge Loop Insertion (L key in edge mode):**
1. Select an edge
2. Press L to insert edge loop
3. New vertices created at edge midpoints
4. Perpendicular loop cut through mesh

#### 6.5 Undo System

- Ctrl+Z to undo
- Saves complete mesh state
- 50 undo levels (configurable)

#### 6.6 ImGui Interface

**Menu Bar:**
- File: New, Open, Save, Export
- Edit: Undo, Select All, Deselect
- View: Connection Points, Grid
- Create: Cube, Plane, Sphere

**Properties Panel:**
- Object name
- Transform (position, rotation, scale)
- Material properties
- Selection info

---

### 7. HDM File Format

**Purpose:** Custom file format for 3D models with game metadata

#### Binary Format (.hdm)

```
Header (magic, version, flags)
├── Properties Section
│   ├── Item Properties (name, weight, value, category)
│   ├── Physics Properties (collision type, mass, bounds)
│   └── Model Properties (scale, texture path)
├── Geometry Section
│   ├── Vertex count
│   ├── Index count
│   ├── Vertex data (position, normal, UV)
│   └── Index data
└── Texture Section (optional embedded texture)
```

#### ASCII Format (.hdma)

JSON-like format with base64-encoded geometry:
```json
{
  "hdm_version": "1.0",
  "item": {
    "name": "Crate",
    "weight": 10.0,
    "value": 50
  },
  "physics": {
    "collision_type": 1,
    "mass": 10.0
  },
  "geometry": {
    "vertex_count": 24,
    "vertices_base64": "AAAAA..."
  }
}
```

**Functions:**
- `hdm_save_binary()` / `hdm_load_binary()`
- `hdm_save_ascii()` / `hdm_load_ascii()`
- `hdm_save_json()` / `hdm_load_json()` (properties only)

---

### 8. Raycast System

**Purpose:** Ray-object intersection for mouse picking

**Functions:**
- `screenToNDC()` - Convert screen coords to normalized device coords
- `unproject()` - Create world-space ray from screen point
- `rayAABB()` - Test ray against axis-aligned bounding box

**Usage in ESE:**
```cpp
// On mouse click
glm::vec3 rayOrigin, rayDir;
unproject(mouseNDC, invProj, invView, rayOrigin, rayDir);

// Test against each object
for (auto& obj : objects) {
    float tMin, tMax;
    if (rayAABB(rayOrigin, rayDir, obj.bounds, tMin, tMax)) {
        // Hit!
    }
}
```

---

### 9. ImGui Integration

**Purpose:** Immediate-mode GUI for editor interfaces

**Functions:**
- `heidic_init_imgui()` - Initialize ImGui for Vulkan/GLFW
- `heidic_imgui_new_frame()` - Begin new ImGui frame
- `heidic_imgui_render()` - Render ImGui draw data

**Features:**
- Automatic font atlas texture creation
- Vulkan descriptor pool for ImGui textures
- Integration with GLFW input handling

---

### 10. Texture Renderers

**DDS Quad Renderer:**
- Tests DDS texture loading
- Renders textured quad to screen
- Supports BC1-BC7 compression formats

**PNG Quad Renderer:**
- Tests PNG/uncompressed texture loading
- Same quad rendering approach
- RGBA8 format support

**TextureResource Renderer:**
- Uses `TextureResource` class
- Automatic format detection
- Resource system integration

---

### 11. Ball Renderer

**Purpose:** Physics demo with bouncing balls

**Features:**
- Multiple balls with different colors
- Simple physics (gravity, bouncing)
- Sphere rendering with lighting

---

### 12. Native File Dialogs

**Purpose:** OS-native file open/save dialogs

**Functions:**
- `nfd_open_file_dialog()` - Generic file dialog
- `nfd_open_obj_dialog()` - Filtered for OBJ files
- `nfd_open_texture_dialog()` - Filtered for image files

**Platform Support:**
- Windows: Uses `GetOpenFileNameA`
- Linux/Mac: Stub (TODO)

---

## Global State Summary

### Vulkan Handles
```cpp
VkInstance g_instance;
VkPhysicalDevice g_physicalDevice;
VkDevice g_device;
VkSwapchainKHR g_swapchain;
VkRenderPass g_renderPass;
VkPipeline g_pipeline;
VkPipelineLayout g_pipelineLayout;
VkCommandPool g_commandPool;
VkQueue g_graphicsQueue;
```

### Synchronization
```cpp
VkSemaphore g_imageAvailableSemaphore;
VkSemaphore g_renderFinishedSemaphore;
VkFence g_inFlightFence;
```

### ESE State
```cpp
// Camera
float g_eseCameraYaw, g_eseCameraPitch, g_eseCameraDistance;

// Selection
int g_eseSelectedVertex, g_eseSelectedEdge, g_eseSelectedFace, g_eseSelectedQuad;
std::vector<int> g_eseMultiSelectedVertices, g_eseMultiSelectedEdges;

// Modes
bool g_eseVertexMode, g_eseEdgeMode, g_eseFaceMode, g_eseQuadMode;
bool g_eseExtrudeMode, g_eseGizmoScaling, g_eseGizmoRotating;
```

---

## Dependencies

### External Libraries
- **GLFW** - Window and input management
- **Vulkan SDK** - Graphics API
- **GLM** - Math library (vectors, matrices)
- **ImGui** - Immediate-mode GUI (optional, `USE_IMGUI`)
- **stb_image** - PNG loading

### Internal Dependencies
- `eden_vulkan_helpers.h` - Header declarations
- `stdlib/dds_loader.h` - DDS texture loading
- `stdlib/png_loader.h` - PNG texture loading
- `stdlib/texture_resource.h` - Texture resource management
- `stdlib/mesh_resource.h` - Mesh resource management
- `stdlib/resource.h` - Base resource class

---

## Exported C Functions

These functions are callable from HEIDIC code:

```cpp
// Initialization
extern "C" void heidic_glfw_vulkan_hints();
extern "C" GLFWwindow* heidic_create_fullscreen_window(const char* title);
extern "C" GLFWwindow* heidic_create_borderless_window(int width, int height, const char* title);
extern "C" int heidic_init_renderer(GLFWwindow* window);
extern "C" void heidic_cleanup_renderer();

// Frame rendering
extern "C" void heidic_render_frame();
extern "C" void heidic_set_rotation_speed(float speed);

// Cube renderer
extern "C" int heidic_init_cube_renderer(GLFWwindow* window);
extern "C" void heidic_render_cube_frame();

// FPS renderer
extern "C" int heidic_init_fps_renderer(GLFWwindow* window);
extern "C" void heidic_render_fps_frame();
extern "C" void heidic_fps_set_camera_position(float x, float y, float z);

// Mesh renderer
extern "C" int heidic_init_obj_mesh_renderer(GLFWwindow* window);
extern "C" int heidic_load_obj_mesh(const char* filepath);
extern "C" void heidic_render_obj_mesh_frame();

// ESE
extern "C" int ese_init(GLFWwindow* window);
extern "C" void ese_render_frame();
extern "C" void ese_cleanup();

// HDM
extern "C" int hdm_load_file(const char* filepath);
extern "C" HDMItemPropertiesC hdm_get_item_properties();
extern "C" HDMPhysicsPropertiesC hdm_get_physics_properties();
extern "C" HDMModelPropertiesC hdm_get_model_properties();

// File dialogs
extern "C" const char* nfd_open_file_dialog(const char* filterList, const char* defaultPath);
extern "C" const char* nfd_open_obj_dialog();
extern "C" const char* nfd_open_texture_dialog();
```

---

*Document generated: December 2024*

