# FPS Colored Cubes Debug Report

## Problem Description

The FPS camera test project should display:
1. A large floor plane (50x1x50 units) scaled from a 2x2x2 cube
2. Multiple colored 1x1x1 reference cubes positioned on top of the floor

**Current Issues:**
- Floor cube appears as 1x1x1 instead of the expected large floor plane
- No colored reference cubes are visible in the scene

## Code Changes Made

### 1. Floor Cube Model Matrix (vulkan/eden_vulkan_helpers.cpp, lines ~2508-2514)

**Location:** `heidic_render_fps()` function

**Current Code:**
```cpp
// Model matrix - scale the floor cube to be a large floor
// Floor cube vertices are -1.0 to 1.0 (2x2x2 cube), so scale to make it a large floor
// Scale X and Z much more than Y to keep thickness but make it wide
glm::mat4 floorModel = glm::mat4(1.0f);
floorModel = glm::scale(floorModel, glm::vec3(25.0f, 0.5f, 25.0f));  // 50x1x50 units (wide floor, thin height)
floorModel = glm::translate(floorModel, glm::vec3(0.0f, -0.5f, 0.0f));  // Move down so top is at y=0
ubo.model = floorModel;
```

**Expected Behavior:**
- Floor cube vertices are defined as -1.0 to 1.0 (2x2x2 cube) - see `floorCubeVertices` at line ~2140
- Scaling by 25x0.5x25 should produce a 50x1x50 unit floor
- Translation moves it down so top face is at y=0

**Issue:** The floor appears as 1x1x1, suggesting the scaling is not being applied correctly, or the model matrix is being overwritten.

### 2. Colored Cube Creation (vulkan/eden_vulkan_helpers.cpp, lines ~2230-2275)

**Location:** `heidic_init_renderer_fps()` function

**Current Code:**
```cpp
// Create colored reference cubes (1x1x1 cubes with different colors)
struct ColoredCubeData {
    float x, y, z;
    float r, g, b;
};

std::vector<ColoredCubeData> referenceCubes = {
    {0.0f, 0.5f, 0.0f, 1.0f, 0.0f, 0.0f},   // Red at origin
    {5.0f, 0.5f, 0.0f, 0.0f, 1.0f, 0.0f},   // Green at +X
    {-5.0f, 0.5f, 0.0f, 0.0f, 0.0f, 1.0f},  // Blue at -X
    {0.0f, 0.5f, 5.0f, 1.0f, 1.0f, 0.0f},   // Yellow at +Z
    {0.0f, 0.5f, -5.0f, 1.0f, 0.0f, 1.0f},  // Magenta at -Z
    {5.0f, 0.5f, 5.0f, 0.0f, 1.0f, 1.0f},   // Cyan at +X+Z
    {-5.0f, 0.5f, -5.0f, 1.0f, 0.5f, 0.0f}, // Orange at -X-Z
    {10.0f, 0.5f, 0.0f, 0.5f, 0.5f, 1.0f},  // Light blue at +10X
    {-10.0f, 0.5f, 0.0f, 1.0f, 0.5f, 0.5f}, // Pink at -10X
};

g_numColoredCubes = static_cast<int>(referenceCubes.size());

// Create vertex buffers for each colored cube
for (int i = 0; i < g_numColoredCubes && i < 10; i++) {
    // Create cube vertices with the specified color (1x1x1 cube from -0.5 to 0.5)
    std::vector<Vertex> coloredCubeVertices = {
        {{-0.5f, -0.5f,  0.5f}, {referenceCubes[i].r, referenceCubes[i].g, referenceCubes[i].b}},
        {{ 0.5f, -0.5f,  0.5f}, {referenceCubes[i].r, referenceCubes[i].g, referenceCubes[i].b}},
        {{ 0.5f,  0.5f,  0.5f}, {referenceCubes[i].r, referenceCubes[i].g, referenceCubes[i].b}},
        {{-0.5f,  0.5f,  0.5f}, {referenceCubes[i].r, referenceCubes[i].g, referenceCubes[i].b}},
        {{-0.5f, -0.5f, -0.5f}, {referenceCubes[i].r, referenceCubes[i].g, referenceCubes[i].b}},
        {{ 0.5f, -0.5f, -0.5f}, {referenceCubes[i].r, referenceCubes[i].g, referenceCubes[i].b}},
        {{ 0.5f,  0.5f, -0.5f}, {referenceCubes[i].r, referenceCubes[i].g, referenceCubes[i].b}},
        {{-0.5f,  0.5f, -0.5f}, {referenceCubes[i].r, referenceCubes[i].g, referenceCubes[i].b}},
    };
    
    VkDeviceSize vertexBufferSize = sizeof(coloredCubeVertices[0]) * coloredCubeVertices.size();
    createBuffer(vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 g_coloredCubeVertexBuffers[i], g_coloredCubeVertexBufferMemory[i]);
    
    void* data;
    vkMapMemory(g_device, g_coloredCubeVertexBufferMemory[i], 0, vertexBufferSize, 0, &data);
    memcpy(data, coloredCubeVertices.data(), (size_t)vertexBufferSize);
    vkUnmapMemory(g_device, g_coloredCubeVertexBufferMemory[i]);
}

std::cout << "[FPS] Created " << g_numColoredCubes << " colored reference cubes" << std::endl;
```

**Expected Behavior:**
- Creates 9 colored cube vertex buffers
- Each cube is 1x1x1 (vertices from -0.5 to 0.5)
- Stores buffers in `g_coloredCubeVertexBuffers[]` array
- Sets `g_numColoredCubes = 9`

**Issue:** Cubes are created but not visible, suggesting rendering code has issues.

### 3. Colored Cube Rendering (vulkan/eden_vulkan_helpers.cpp, lines ~2606-2656)

**Location:** `heidic_render_fps()` function, after floor cube is drawn

**Current Code:**
```cpp
// Draw multiple colored 1x1x1 cubes on top of the floor for spatial reference
struct ColoredCubePos {
    float x, y, z;
};

std::vector<ColoredCubePos> cubePositions = {
    {0.0f, 0.5f, 0.0f},   // Red at origin
    {5.0f, 0.5f, 0.0f},   // Green at +X
    {-5.0f, 0.5f, 0.0f},  // Blue at -X
    {0.0f, 0.5f, 5.0f},   // Yellow at +Z
    {0.0f, 0.5f, -5.0f},  // Magenta at -Z
    {5.0f, 0.5f, 5.0f},   // Cyan at +X+Z
    {-5.0f, 0.5f, -5.0f}, // Orange at -X-Z
    {10.0f, 0.5f, 0.0f},  // Light blue at +10X
    {-10.0f, 0.5f, 0.0f}, // Pink at -10X
};

for (int i = 0; i < g_numColoredCubes && i < static_cast<int>(cubePositions.size()); i++) {
    if (g_coloredCubeVertexBuffers[i] == VK_NULL_HANDLE) {
        continue;
    }
    
    // Model matrix: 1x1x1 cube at the specified position
    glm::mat4 cubeModel = glm::mat4(1.0f);
    cubeModel = glm::translate(cubeModel, glm::vec3(cubePositions[i].x, cubePositions[i].y, cubePositions[i].z));
    // Cube vertices are -0.5 to 0.5, so no scaling needed for 1x1x1 cube
    ubo.model = cubeModel;
    
    // Update uniform buffer with new model matrix
    void* cubeData;
    vkMapMemory(g_device, g_uniformBuffersMemory[imageIndex], 0, sizeof(ubo), 0, &cubeData);
    memcpy(cubeData, &ubo, sizeof(ubo));
    vkUnmapMemory(g_device, g_uniformBuffersMemory[imageIndex]);
    
    // Rebind descriptor set (to ensure GPU sees updated uniform buffer)
    vkCmdBindDescriptorSets(g_commandBuffers[imageIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, g_pipelineLayout, 0, 1, &g_descriptorSets[imageIndex], 0, nullptr);
    
    // Bind the colored cube's vertex buffer
    VkBuffer vertexBuffers[] = {g_coloredCubeVertexBuffers[i]};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(g_commandBuffers[imageIndex], 0, 1, vertexBuffers, offsets);
    
    // Rebind index buffer (same indices work for all cubes - 36 indices for a cube)
    vkCmdBindIndexBuffer(g_commandBuffers[imageIndex], g_fpsCubeIndexBuffer, 0, VK_INDEX_TYPE_UINT16);
    
    // Draw the cube (36 indices for a cube: 6 faces * 2 triangles * 3 vertices)
    vkCmdDrawIndexed(g_commandBuffers[imageIndex], 36, 1, 0, 0, 0);
}
```

**Expected Behavior:**
- Loops through all colored cubes
- Updates uniform buffer with each cube's model matrix (translation only, no scaling)
- Binds each cube's vertex buffer
- Draws each cube using the shared index buffer

**Potential Issues:**
1. The uniform buffer update overwrites the view/projection matrices - but these should be the same for all cubes, so this should be fine
2. The index buffer `g_fpsCubeIndexBuffer` is shared - this should work if all cubes use the same index pattern
3. The loop might not execute if `g_numColoredCubes == 0`

### 4. Global Variables (vulkan/eden_vulkan_helpers.cpp, lines ~2133-2136)

**Location:** Top of file, static variables

**Current Code:**
```cpp
// Colored reference cubes (1x1x1 cubes for spatial reference)
static VkBuffer g_coloredCubeVertexBuffers[10] = {VK_NULL_HANDLE};
static VkDeviceMemory g_coloredCubeVertexBufferMemory[10] = {VK_NULL_HANDLE};
static int g_numColoredCubes = 0;
```

**Expected Behavior:**
- Stores vertex buffers and memory for up to 10 colored cubes
- `g_numColoredCubes` tracks how many were created

## Root Cause Analysis

### Issue 1: Floor Cube Appears 1x1x1

**Hypothesis:** The model matrix scaling is not being applied, or the uniform buffer is being overwritten before the floor cube is drawn.

**Evidence:**
- Floor cube vertices are defined as -1.0 to 1.0 (2x2x2 cube)
- Model matrix should scale by 25x0.5x25 to produce 50x1x50 floor
- User reports seeing 1x1x1 cube instead

**Possible Causes:**
1. The uniform buffer update for the floor cube happens, but then gets overwritten by colored cube rendering before the floor is drawn
2. The model matrix calculation is incorrect (order of operations)
3. The shader is not reading the model matrix correctly

**Code Flow:**
1. Floor model matrix is set (line ~2511-2514)
2. Uniform buffer is updated with floor model matrix (line ~2576-2580)
3. Floor cube is drawn (line ~2604)
4. Then colored cubes are rendered (line ~2628+)

The floor should be drawn before colored cubes, so the uniform buffer shouldn't be overwritten. However, the uniform buffer is updated again for each colored cube, which might cause issues if the floor draw hasn't completed.

### Issue 2: Colored Cubes Not Visible

**Hypothesis:** The colored cubes are either not being created, not being rendered, or are being rendered but not visible (wrong position, culled, etc.)

**Evidence:**
- User reports no colored cubes visible
- Code creates 9 colored cubes in init function
- Code attempts to render them in render function

**Possible Causes:**
1. `g_numColoredCubes` is 0 (cubes weren't created)
2. Vertex buffers are NULL (creation failed)
3. Cubes are being rendered but are:
   - Behind the camera
   - Outside the view frustum
   - Being culled (back face culling)
   - Too small to see
   - Being drawn but with wrong matrices (view/proj not updated)

**Code Flow:**
1. Colored cubes are created in `heidic_init_renderer_fps()` (line ~2230-2275)
2. `g_numColoredCubes` is set to 9
3. In render function, loop checks `g_numColoredCubes` (line ~2628)
4. For each cube, updates uniform buffer with cube's model matrix
5. But: The view and projection matrices in `ubo` are set BEFORE the floor cube is drawn
6. When colored cubes are rendered, the view/proj matrices should still be in `ubo`, but we're only updating `ubo.model`

**Critical Issue:** When updating the uniform buffer for colored cubes, we're doing:
```cpp
ubo.model = cubeModel;  // Only updates model matrix
memcpy(cubeData, &ubo, sizeof(ubo));  // Copies entire ubo (including view/proj)
```

This should work IF `ubo.view` and `ubo.proj` are still set from earlier. But if they were overwritten or not set, the cubes won't render correctly.

## Recommended Fixes

### Fix 1: Preserve View/Projection Matrices for Colored Cubes

**Problem:** When rendering colored cubes, we update `ubo.model` but need to ensure `ubo.view` and `ubo.proj` are still set.

**Solution:** Store view and projection matrices before the loop, or recalculate them for each cube (they should be the same).

**Code Change:**
```cpp
// Before colored cube loop, ensure view/proj are set
// (They should already be set from floor cube rendering)
// But to be safe, recalculate or store them

// Then in loop:
ubo.model = cubeModel;
// ubo.view and ubo.proj should already be set from floor cube
```

### Fix 2: Verify Floor Model Matrix Order

**Problem:** Matrix multiplication order might be wrong.

**Current:** `scale` then `translate`
**Should be:** `translate` then `scale`? Or `scale` then `translate`?

For a floor, we want to:
1. Scale the cube to be large and flat
2. Translate it down so top is at y=0

Current order: `scale(25, 0.5, 25)` then `translate(0, -0.5, 0)`
- This scales first, then translates
- Translation is in world space, not scaled space
- So translate(0, -0.5, 0) moves by 0.5 units in world space

If we want the top at y=0, and the cube is scaled to 0.5 in Y (so height is 1 unit), and vertices go from -1 to 1 (so unscaled height is 2), then:
- Scaled height = 2 * 0.5 = 1 unit
- Bottom of scaled cube = -1 * 0.5 = -0.5
- Top of scaled cube = 1 * 0.5 = 0.5
- To move top to y=0, we need to translate by -0.5

So the current translation seems correct. But maybe the issue is that we need to translate FIRST, then scale?

Actually, in matrix multiplication, `M = T * S` means "scale first, then translate" (right-to-left reading).
But in code, `model = scale(model, ...)` then `model = translate(model, ...)` means we're doing `T * S`, which is "scale first, then translate in world space".

For a floor, we probably want to translate in local space first, then scale. So we should do `S * T`, which means `translate` first, then `scale`.

**Fix:**
```cpp
glm::mat4 floorModel = glm::mat4(1.0f);
floorModel = glm::translate(floorModel, glm::vec3(0.0f, -0.5f, 0.0f));  // Translate first
floorModel = glm::scale(floorModel, glm::vec3(25.0f, 0.5f, 25.0f));  // Then scale
```

### Fix 3: Add Debug Output (One-Time, Not Per-Frame)

**Problem:** Need to verify cubes are created and being rendered.

**Solution:** Add one-time debug output in init and first render.

**Code Change:**
```cpp
// In init, after creating cubes:
if (g_numColoredCubes > 0) {
    std::cout << "[FPS] Successfully created " << g_numColoredCubes << " colored cubes" << std::endl;
} else {
    std::cerr << "[FPS] ERROR: Failed to create colored cubes!" << std::endl;
}

// In render, first frame only:
static bool firstRender = true;
if (firstRender) {
    std::cout << "[FPS] First render: g_numColoredCubes=" << g_numColoredCubes << std::endl;
    firstRender = false;
}
```

## Files Modified

1. `vulkan/eden_vulkan_helpers.cpp`
   - Lines ~2133-2136: Global variables for colored cubes
   - Lines ~2230-2275: Colored cube creation in `heidic_init_renderer_fps()`
   - Lines ~2508-2514: Floor cube model matrix in `heidic_render_fps()`
   - Lines ~2606-2656: Colored cube rendering in `heidic_render_fps()`
   - Lines ~2670-2685: Cleanup code in `heidic_cleanup_renderer_fps()`

## Test Case

**Expected Result:**
- Large gray floor plane (50x1x50 units) visible
- 9 colored 1x1x1 cubes visible on top of floor at:
  - Red at (0, 0.5, 0)
  - Green at (5, 0.5, 0)
  - Blue at (-5, 0.5, 0)
  - Yellow at (0, 0.5, 5)
  - Magenta at (0, 0.5, -5)
  - Cyan at (5, 0.5, 5)
  - Orange at (-5, 0.5, -5)
  - Light blue at (10, 0.5, 0)
  - Pink at (-10, 0.5, 0)

**Actual Result:**
- Floor appears as 1x1x1 cube
- No colored cubes visible

## Next Steps

1. Verify `g_numColoredCubes` is 9 after init
2. Verify colored cube vertex buffers are not NULL
3. Fix floor model matrix order (translate then scale)
4. Ensure view/projection matrices are preserved when rendering colored cubes
5. Add one-time debug output to verify rendering loop executes

