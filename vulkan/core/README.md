# VulkanCore - Shared Rendering Foundation

## Overview

**VulkanCore** is the ONE place for Vulkan boilerplate. All projects use this instead of copy-pasting 800+ lines of init code.

## Files

| File | Lines | Purpose |
|------|-------|---------|
| `vulkan_core.h` | ~320 | Header with `VulkanCore` class + C API declarations |
| `vulkan_core.cpp` | ~950 | Implementation of all Vulkan boilerplate |
| **Total** | **~1270** | vs 800+ lines duplicated per project before |

## What's Inside

### VulkanCore Class (C++)

```cpp
namespace vkcore {

class VulkanCore {
public:
    // Lifecycle
    bool init(GLFWwindow* window, const CoreConfig& config);
    void shutdown();
    
    // Frame management
    bool beginFrame();
    void endFrame();
    
    // Pipeline (configurable shaders + vertex formats)
    PipelineHandle createPipeline(const PipelineConfig& config);
    void bindPipeline(PipelineHandle handle);
    
    // Buffers
    BufferHandle createVertexBuffer(const void* data, size_t size);
    BufferHandle createIndexBuffer(const uint32_t* data, size_t count);
    BufferHandle createUniformBuffer(size_t size);
    
    // Meshes (high-level)
    MeshHandle createCube(float size, const glm::vec3& color);
    MeshHandle createMesh(const MeshData& data);
    
    // Drawing
    void drawMesh(MeshHandle mesh, const glm::mat4& transform, const glm::vec4& color);
    
    // Camera
    void setCamera(const glm::vec3& position, const glm::vec3& target, const glm::vec3& up);
    void setPerspective(float fovDegrees, float nearPlane, float farPlane);
    
    // Accessors (for extensions like ImGui)
    VkDevice getDevice() const;
    VkRenderPass getRenderPass() const;
    VkQueue getGraphicsQueue() const;
    // ... more
};

}
```

### C API (for HEIDIC)

```c
// Lifecycle
int vkcore_init(void* glfwWindow, const char* appName, int width, int height);
void vkcore_shutdown();

// Frame
int vkcore_begin_frame();
void vkcore_end_frame();

// Pipeline
unsigned int vkcore_create_pipeline(const char* vertShader, const char* fragShader, int vertexFormat);
void vkcore_bind_pipeline(unsigned int handle);

// Mesh
unsigned int vkcore_create_cube(float size, float r, float g, float b);
void vkcore_draw_mesh(unsigned int mesh, 
                      float px, float py, float pz,    // position
                      float rx, float ry, float rz,    // rotation (degrees)
                      float sx, float sy, float sz,    // scale
                      float r, float g, float b, float a); // color

// Camera
void vkcore_set_camera(float eyeX, float eyeY, float eyeZ,
                       float targetX, float targetY, float targetZ);
void vkcore_set_perspective(float fovDegrees, float nearPlane, float farPlane);
```

## Vertex Formats

| Format | Components | Use Case |
|--------|-----------|----------|
| `POSITION_COLOR` | vec3 pos, vec3 color | Cubes, debug, simple |
| `POSITION_NORMAL_UV` | vec3 pos, vec3 normal, vec2 uv | Textured meshes |
| `POSITION_NORMAL_UV_COLOR` | vec3 pos, vec3 normal, vec2 uv, vec3 color | Full featured |
| `CUSTOM` | User-defined | Advanced use |

## Usage from HEIDIC

```heidic
// Declare the extern functions
extern fn vkcore_init(window: GLFWwindow, name: string, w: i32, h: i32): i32;
extern fn vkcore_shutdown(): void;
extern fn vkcore_begin_frame(): i32;
extern fn vkcore_end_frame(): void;
extern fn vkcore_create_pipeline(vert: string, frag: string, format: i32): i32;
extern fn vkcore_bind_pipeline(handle: i32): void;
extern fn vkcore_create_cube(size: f32, r: f32, g: f32, b: f32): i32;
extern fn vkcore_draw_mesh(mesh: i32, px: f32, py: f32, pz: f32, 
                           rx: f32, ry: f32, rz: f32,
                           sx: f32, sy: f32, sz: f32,
                           r: f32, g: f32, b: f32, a: f32): void;
extern fn vkcore_set_camera(ex: f32, ey: f32, ez: f32, tx: f32, ty: f32, tz: f32): void;
extern fn vkcore_set_perspective(fov: f32, near: f32, far: f32): void;

fn main(): void {
    // Init
    glfwInit();
    let window = glfwCreateWindow(1280, 720, "My Game", null, null);
    vkcore_init(window, "My Game", 1280, 720);
    
    // Create pipeline and mesh
    let pipeline = vkcore_create_pipeline("shaders/cube.vert.spv", "shaders/cube.frag.spv", 0);
    let cube = vkcore_create_cube(1.0, 1.0, 0.5, 0.5);
    
    // Set camera
    vkcore_set_camera(0.0, 5.0, 10.0, 0.0, 0.0, 0.0);
    vkcore_set_perspective(60.0, 0.1, 1000.0);
    
    // Main loop
    while glfwWindowShouldClose(window) == 0 {
        glfwPollEvents();
        
        if vkcore_begin_frame() != 0 {
            vkcore_bind_pipeline(pipeline);
            vkcore_draw_mesh(cube, 0.0, 0.0, 0.0, 0.0, 45.0, 0.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0);
            vkcore_end_frame();
        }
    }
    
    vkcore_shutdown();
    glfwTerminate();
}
```

## Usage from C++

```cpp
#include "vulkan/core/vulkan_core.h"

int main() {
    glfwInit();
    GLFWwindow* window = glfwCreateWindow(1280, 720, "My App", nullptr, nullptr);
    
    vkcore::VulkanCore core;
    vkcore::CoreConfig config;
    config.appName = "My App";
    core.init(window, config);
    
    // Create pipeline
    vkcore::PipelineConfig pipeConfig;
    pipeConfig.vertexShaderPath = "shaders/cube.vert.spv";
    pipeConfig.fragmentShaderPath = "shaders/cube.frag.spv";
    auto pipeline = core.createPipeline(pipeConfig);
    
    // Create mesh
    auto cube = core.createCube(1.0f, glm::vec3(1.0f, 0.5f, 0.5f));
    
    // Set camera
    core.setCamera(glm::vec3(0, 5, 10), glm::vec3(0, 0, 0));
    core.setPerspective(60.0f, 0.1f, 1000.0f);
    
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        
        if (core.beginFrame()) {
            core.bindPipeline(pipeline);
            
            glm::mat4 transform = glm::rotate(glm::mat4(1.0f), time, glm::vec3(0, 1, 0));
            core.drawMesh(cube, transform);
            
            core.endFrame();
        }
    }
    
    core.shutdown();
    glfwTerminate();
}
```

## Extensibility

VulkanCore exposes Vulkan handles for advanced use cases:

```cpp
// Get handles for ImGui integration
VkDevice device = core.getDevice();
VkRenderPass renderPass = core.getRenderPass();
VkQueue graphicsQueue = core.getGraphicsQueue();
uint32_t graphicsFamily = core.getGraphicsFamily();
VkCommandPool commandPool = core.getCommandPool();

// Use in ImGui init
ImGui_ImplVulkan_InitInfo initInfo = {};
initInfo.Instance = ...;  // You'd need to expose this too
initInfo.Device = device;
initInfo.Queue = graphicsQueue;
initInfo.QueueFamily = graphicsFamily;
// etc.
```

## Benefits

1. **One source of truth** - All Vulkan boilerplate in one place
2. **No duplication** - SLAG_LEGION, ESE, new projects all use this
3. **Configurable** - Different shaders, vertex formats, settings
4. **Clean API** - Both C++ and C interfaces
5. **HEIDIC compatible** - All C API functions can be called from HEIDIC
6. **Extensible** - Vulkan handles exposed for ImGui, custom rendering

