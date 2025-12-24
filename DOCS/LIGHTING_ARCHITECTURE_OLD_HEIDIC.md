# EDEN ENGINE Lighting Architecture

## Design Philosophy

**Goal**: Create a lighting system that can seamlessly switch between:
- **Rasterized Lighting** (immediate, works everywhere)
- **Vulkan Ray Tracing** (VK_KHR_ray_tracing_pipeline)
- **OptiX/CUDA** (NVIDIA-specific, high performance)

**Principle**: The Heidic API remains unchanged regardless of backend. Game code doesn't need to know which rendering path is active.

## Architecture Overview

```
┌─────────────────────────────────────────┐
│         Heidic Game Code                 │
│  (heidic_set_light, heidic_draw_mesh)   │
└──────────────┬──────────────────────────┘
               │
┌──────────────▼──────────────────────────┐
│      Lighting API (Abstraction)         │
│  - heidic_set_light()                   │
│  - heidic_set_lighting_mode()           │
│  - heidic_enable_shadows()              │
└──────────────┬──────────────────────────┘
               │
       ┌───────┴───────┬──────────────┐
       │               │              │
┌──────▼──────┐ ┌─────▼──────┐ ┌─────▼──────┐
│  Rasterized │ │  Vulkan RT │ │   OptiX    │
│  Backend    │ │  Backend   │ │  Backend   │
│  (Default)  │ │  (Future)  │ │  (Future)  │
└─────────────┘ └────────────┘ └────────────┘
```

## Phase 1: Rasterized Lighting (Current/Immediate)

**Implementation**: Start with traditional Phong/Blinn-Phong lighting using vertex/fragment shaders.

**Features**:
- Directional lights (sun)
- Point lights
- Spot lights
- Ambient lighting
- Diffuse + Specular components
- Shadow maps (optional, Phase 1.5)

**Shader Structure**:
```glsl
// Uniform Buffer (Binding 0)
layout(binding = 0) uniform UBO {
    mat4 view;
    mat4 proj;
    vec3 lightDir;      // Directional light
    vec3 lightColor;    // Light color/intensity
    vec3 ambientColor;  // Ambient light
    vec3 viewPos;       // Camera position for specular
} ubo;
```

**Heidic API**:
```heidic
// Set a directional light
extern fn heidic_set_directional_light(dir_x: f32, dir_y: f32, dir_z: f32, r: f32, g: f32, b: f32): void;

// Set ambient light
extern fn heidic_set_ambient_light(r: f32, g: f32, b: f32): void;

// Set point light (future)
extern fn heidic_add_point_light(x: f32, y: f32, z: f32, r: f32, g: f32, b: f32, range: f32): i32;
```

**Benefits**:
- ✅ Works on all GPUs
- ✅ Fast performance
- ✅ Simple to implement
- ✅ Good foundation for future backends

## Phase 2: Backend Abstraction Layer

**Design**: Create an interface that all backends implement.

**C++ Interface** (internal):
```cpp
class LightingBackend {
public:
    virtual ~LightingBackend() = default;
    virtual void setDirectionalLight(glm::vec3 dir, glm::vec3 color) = 0;
    virtual void setAmbientLight(glm::vec3 color) = 0;
    virtual void updateUBO() = 0;  // Update uniform buffer
    virtual void renderShadows() = 0;  // Optional
};

class RasterizedLighting : public LightingBackend { ... };
class VulkanRTLighting : public LightingBackend { ... };
class OptiXLighting : public LightingBackend { ... };
```

**Backend Selection**:
```cpp
// In heidic_init_renderer()
LightingBackend* g_lightingBackend = nullptr;

void heidic_set_lighting_backend(const char* backend) {
    if (strcmp(backend, "rasterized") == 0) {
        g_lightingBackend = new RasterizedLighting();
    } else if (strcmp(backend, "vulkan_rt") == 0) {
        if (checkVulkanRTExtension()) {
            g_lightingBackend = new VulkanRTLighting();
        }
    } else if (strcmp(backend, "optix") == 0) {
        if (checkOptiXAvailable()) {
            g_lightingBackend = new OptiXLighting();
        }
    }
}
```

## Phase 3: Vulkan Ray Tracing Support

**When to Add**: After rasterized lighting is stable and working.

**Implementation**:
- Use `VK_KHR_ray_tracing_pipeline` extension
- Create acceleration structures (BLAS/TLAS)
- Ray generation, closest hit, miss shaders
- Hybrid rendering: rasterize primary, ray trace shadows/reflections

**Benefits**:
- ✅ Works with existing Vulkan setup
- ✅ Hardware-accelerated on RTX GPUs
- ✅ Can mix with rasterization

## Phase 4: OptiX/CUDA Integration

**When to Add**: For maximum performance on NVIDIA hardware or specific use cases.

**Implementation**:
- Separate CUDA context alongside Vulkan
- OptiX for ray tracing
- CUDA kernels for compute (particle systems, physics, etc.)
- Interop between Vulkan and CUDA buffers

**Architecture**:
```
Vulkan (Rasterization) ──┐
                         ├──> Shared Memory/Textures
CUDA/OptiX (Ray Tracing) ┘
```

**Use Cases**:
- High-quality global illumination
- Complex light bounces
- Scientific visualization
- Professional rendering features

## Implementation Plan

### Step 1: Basic Rasterized Lighting (Week 1)
1. Add normal vectors to `Vertex` struct
2. Update vertex shader to pass normals
3. Implement Phong lighting in fragment shader
4. Add `heidic_set_directional_light()` API
5. Test with existing cube/mesh examples

### Step 2: Lighting Abstraction (Week 2)
1. Create `LightingBackend` interface
2. Refactor rasterized lighting into `RasterizedLighting` class
3. Add backend selection mechanism
4. Ensure API remains unchanged

### Step 3: Normal Maps (Week 3)
1. Add normal map texture support
2. Sample normal map in fragment shader
3. Transform tangent-space normals to world space
4. Use in lighting calculations

### Step 4: Shadow Maps (Week 4, Optional)
1. Render depth from light's perspective
2. Sample shadow map in fragment shader
3. Add `heidic_enable_shadows()` API

### Step 5: Vulkan RT Foundation (Future)
1. Check for `VK_KHR_ray_tracing_pipeline` support
2. Create acceleration structure builders
3. Implement basic ray generation shader
4. Hybrid rendering: rasterize + ray trace shadows

### Step 6: OptiX Foundation (Future)
1. Add CUDA/OptiX initialization
2. Create OptiX context
3. Implement basic OptiX lighting backend
4. Buffer interop between Vulkan and CUDA

## API Design (Heidic)

```heidic
// Lighting Control
extern fn heidic_set_directional_light(dir_x: f32, dir_y: f32, dir_z: f32, r: f32, g: f32, b: f32): void;
extern fn heidic_set_ambient_light(r: f32, g: f32, b: f32): void;
extern fn heidic_set_lighting_mode(mode: string): void;  // "rasterized", "vulkan_rt", "optix"

// Point Lights (future)
extern fn heidic_add_point_light(x: f32, y: f32, z: f32, r: f32, g: f32, b: f32, range: f32): i32;
extern fn heidic_remove_point_light(light_id: i32): void;

// Shadows (future)
extern fn heidic_enable_shadows(enabled: i32): void;
extern fn heidic_set_shadow_resolution(width: i32, height: i32): void;
```

## File Structure

```
heidic_v2/
├── vulkan/
│   ├── eden_vulkan_helpers.h/cpp      (Current rendering)
│   ├── lighting/
│   │   ├── lighting_backend.h         (Interface)
│   │   ├── rasterized_lighting.h/cpp  (Phase 1)
│   │   ├── vulkan_rt_lighting.h/cpp   (Phase 3)
│   │   └── optix_lighting.h/cpp       (Phase 4)
│   └── shaders/
│       ├── vert_lit.glsl               (Vertex shader with normals)
│       ├── frag_lit.glsl               (Phong lighting)
│       ├── frag_lit_normalmap.glsl    (With normal maps)
│       └── rt_raygen.glsl              (Future: RT shaders)
├── cuda/                               (Future: OptiX/CUDA)
│   ├── optix_context.h/cpp
│   └── kernels/
└── docs/
    └── LIGHTING_ARCHITECTURE.md       (This file)
```

## Benefits of This Approach

1. **No Breaking Changes**: Game code works regardless of backend
2. **Progressive Enhancement**: Start simple, add features incrementally
3. **Performance Options**: Choose best backend for target hardware
4. **Future-Proof**: Easy to add new backends (Metal RT, DXR, etc.)
5. **Testing**: Can A/B test different backends for quality/performance

## Next Steps

1. **Start with Phase 1**: Implement basic rasterized lighting
2. **Design the abstraction**: Create `LightingBackend` interface early
3. **Keep it simple**: Don't over-engineer, but keep extensibility in mind
4. **Document as we go**: Update this doc as we implement

---

**Status**: Design Phase  
**Last Updated**: December 2024  
**Next Milestone**: Implement Phase 1 (Rasterized Lighting)

