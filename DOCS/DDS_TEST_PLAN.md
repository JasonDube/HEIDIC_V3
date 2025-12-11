# DDS Loader Test Plan

## Quick Test Approach

To test the DDS loader **right now**, we can render a DDS texture on a simple **fullscreen 2D quad** - no 3D geometry needed!

## Test Setup

### Option 1: Fullscreen Quad (Simplest)
- Create a simple 2D quad that fills the screen
- Render the DDS texture on it
- Perfect for texture testing!

### Option 2: Modify Existing Renderer
- Use the existing `heidic_render_frame()` function
- Replace triangle/cube with a fullscreen quad
- Display the DDS texture

## Implementation Steps

1. **Add DDS test function to vulkan helpers:**
   ```cpp
   // In eden_vulkan_helpers.h
   int heidic_test_dds_texture(GLFWwindow* window, const char* ddsPath);
   void heidic_render_test_texture(GLFWwindow* window);
   ```

2. **Load DDS texture:**
   - Call `load_dds()` from our loader
   - Create Vulkan image/imageView/sampler
   - Upload compressed data to GPU

3. **Create fullscreen quad:**
   - Simple 2D quad vertices (4 vertices)
   - Simple shader that displays texture
   - Render to screen

4. **Test:**
   - Place a DDS file in project directory
   - Call test function
   - Verify texture displays correctly!

## Fullscreen Quad Geometry

```cpp
// Simple quad covering full screen
struct QuadVertex {
    float pos[2];   // X, Y (-1 to 1)
    float uv[2];    // Texture coordinates (0 to 1)
};

// Quad vertices (NDC coordinates, covers entire screen)
QuadVertex quadVertices[] = {
    {{-1.0f, -1.0f}, {0.0f, 0.0f}},  // Bottom-left
    {{ 1.0f, -1.0f}, {1.0f, 0.0f}},  // Bottom-right
    {{ 1.0f,  1.0f}, {1.0f, 1.0f}},  // Top-right
    {{-1.0f,  1.0f}, {0.0f, 1.0f}}   // Top-left
};

uint16_t quadIndices[] = {
    0, 1, 2,
    2, 3, 0
};
```

## Simple Shader (for testing)

**Vertex shader:**
```glsl
#version 450
layout(location = 0) in vec2 inPos;
layout(location = 1) in vec2 inUV;

layout(location = 0) out vec2 fragUV;

void main() {
    gl_Position = vec4(inPos, 0.0, 1.0);  // Already in NDC
    fragUV = inUV;
}
```

**Fragment shader:**
```glsl
#version 450
layout(location = 0) in vec2 fragUV;
layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform sampler2D texSampler;

void main() {
    outColor = texture(texSampler, fragUV);
}
```

## Quick Test Function

```cpp
// Simple test: Load DDS and render on fullscreen quad
extern "C" int heidic_test_dds_texture(GLFWwindow* window, const char* ddsPath) {
    // 1. Initialize renderer (reuse existing)
    if (heidic_init_renderer(window) == 0) return 0;
    
    // 2. Load DDS texture
    DDSData dds = load_dds(ddsPath);
    if (dds.format == VK_FORMAT_UNDEFINED) {
        std::cerr << "[TEST] Failed to load DDS: " << ddsPath << std::endl;
        return 0;
    }
    
    std::cout << "[TEST] Loaded DDS: " << ddsPath << std::endl;
    std::cout << "[TEST] Format: " << dds.format << ", Size: " 
              << dds.width << "x" << dds.height << std::endl;
    
    // 3. Create Vulkan resources (image, imageView, sampler)
    // ... (use test_load_dds_texture from dds_test_helper.h)
    
    // 4. Create fullscreen quad pipeline
    // ... (simple 2D quad shader)
    
    // 5. Render loop (reuse existing render loop structure)
    // ... (draw quad with texture)
    
    return 1;
}
```

## Testing Workflow

1. **Get a DDS file:**
   - Convert a PNG to DDS using texconv or similar tool
   - OR download a test DDS texture
   - Place in project directory

2. **Add test code:**
   - Add test function to `eden_vulkan_helpers.cpp`
   - Compile and run

3. **Verify:**
   - Texture displays correctly
   - Format is detected properly
   - Loading is fast (should be <2ms for DDS)

## Benefits

- **Visual verification** - See the texture immediately
- **Fast iteration** - Can test with different DDS files
- **No 3D geometry needed** - Just a simple 2D quad
- **Reusable** - Fullscreen quad is useful for UI/overlays later

## Next Steps After Testing

Once DDS loading works visually:
1. ✅ Move to Phase 1.3 (TextureResource class)
2. ✅ Integrate into proper resource system
3. ✅ Add HEIDIC `resource` syntax

