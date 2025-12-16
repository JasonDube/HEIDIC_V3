# FPS Camera Cube Rendering Issue - Debug Report

## Problem Summary
The FPS camera test project renders a cube that appears as a "flat z-fighting jumbled mess" instead of a proper 3D cube. The cube vertices and indices are correct (identical to the working spinning cube example), but the rendering is broken.

## What Works
- **Spinning Cube Example**: Renders correctly with proper 3D depth and faces
- **FPS Camera Controls**: Mouse look and WASD movement work correctly
- **Pipeline Creation**: Both FPS and cube pipelines are created successfully
- **Buffer Creation**: Vertex and index buffers are created with correct data

## What Doesn't Work
- **FPS Cube Rendering**: Cube appears as flat faces with z-fighting, all faces appear to be at the same depth
- **Both Pipelines**: Issue persists whether using `g_fpsPipeline` or `g_cubePipeline`
- **All Cube Sizes**: Issue occurs with 2x2x2 cube, 5x5x5 cube, and flat floor cube (20x1x20)

## Attempted Fixes (All Failed)
1. ✅ Used exact same vertices/indices as spinning cube
2. ✅ Used exact same matrices (model/view/projection) as spinning cube
3. ✅ Used exact same uniform buffer update code
4. ✅ Used same pipeline (g_cubePipeline) instead of g_fpsPipeline
5. ✅ Used same buffers (g_cubeVertexBuffer) instead of FPS-specific buffers
6. ✅ Fixed Mat4 conversion (tried both direct assignment and .data access)
7. ✅ Verified depth buffer is enabled and cleared
8. ✅ Verified depth stencil state matches spinning cube
9. ✅ Disabled face culling (no change)
10. ✅ Tried identity matrix, scaled matrices, rotated matrices (all fail)

## Key Files

### Main Rendering Code
- **File**: `vulkan/eden_vulkan_helpers.cpp`
- **FPS Render Function**: `heidic_render_fps()` starting at line ~2313
- **Spinning Cube Render Function**: `heidic_render_frame_cube()` starting at line ~1884
- **FPS Pipeline Creation**: `heidic_init_renderer_fps()` starting at line ~2077
- **Cube Pipeline Creation**: `heidic_init_renderer_cube()` starting at line ~1675

### Vertex/Index Data
- **Cube Vertices**: `cubeVertices` at line ~1652 (8 vertices, -1.0 to 1.0 range)
- **Cube Indices**: `cubeIndices` at line ~1665 (36 indices, 12 triangles)
- **FPS Floor Cube Vertices**: `floorCubeVertices` at line ~2056 (identical to cubeVertices)
- **FPS Floor Cube Indices**: `floorCubeIndices` at line ~2067 (identical to cubeIndices)

## Current Code State

### FPS Render Function (heidic_render_fps)
```cpp
// Line ~2313
extern "C" void heidic_render_fps(GLFWwindow* window, float camera_pos_x, float camera_pos_y, float camera_pos_z, float camera_yaw, float camera_pitch) {
    if (g_device == VK_NULL_HANDLE || g_swapchain == VK_NULL_HANDLE) {
        return;
    }
    
    // Use FPS pipeline if available, otherwise use cube pipeline
    VkPipeline pipelineToUse = (g_fpsPipeline != VK_NULL_HANDLE) ? g_fpsPipeline : g_cubePipeline;
    if (pipelineToUse == VK_NULL_HANDLE) {
        std::cerr << "[FPS] ERROR: No pipeline available!" << std::endl;
        return;
    }
    
    // ... standard Vulkan frame setup (wait, acquire image, reset command buffer) ...
    
    // Begin render pass
    VkRenderPassBeginInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = g_renderPass;
    renderPassInfo.framebuffer = g_framebuffers[imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = g_swapchainExtent;
    
    std::array<VkClearValue, 2> clearValues = {};
    clearValues[0].color = {{0.1f, 0.1f, 0.15f, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};
    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();
    
    vkCmdBeginRenderPass(g_commandBuffers[imageIndex], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    
    // Bind pipeline
    vkCmdBindPipeline(g_commandBuffers[imageIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineToUse);
    
    // Update uniform buffer
    UniformBufferObject ubo = {};
    
    // Model matrix - identity (no transform)
    ubo.model = glm::mat4(1.0f);
    
    // View matrix - calculate from camera position, yaw, and pitch
    float yaw_rad = camera_yaw * 3.14159f / 180.0f;
    float pitch_rad = camera_pitch * 3.14159f / 180.0f;
    
    glm::vec3 forward(
        sinf(yaw_rad) * cosf(pitch_rad),
        -sinf(pitch_rad),
        -cosf(yaw_rad) * cosf(pitch_rad)
    );
    forward = glm::normalize(forward);
    
    glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
    glm::vec3 up = glm::normalize(glm::cross(right, forward));
    
    Vec3 eye = {camera_pos_x, camera_pos_y, camera_pos_z};
    Vec3 center = {
        camera_pos_x + forward.x,
        camera_pos_y + forward.y,
        camera_pos_z + forward.z
    };
    Vec3 up_vec = {up.x, up.y, up.z};
    
    ubo.view = mat4_lookat(eye, center, up_vec);
    
    // Projection matrix
    float fov = 70.0f * 3.14159f / 180.0f;
    float aspect = (float)g_swapchainExtent.width / (float)g_swapchainExtent.height;
    float nearPlane = 0.1f;
    float farPlane = 100.0f;
    ubo.proj = mat4_perspective(fov, aspect, nearPlane, farPlane);
    ubo.proj[1][1] *= -1.0f;  // Vulkan clip space
    
    // Update uniform buffer
    void* data;
    vkMapMemory(g_device, g_uniformBuffersMemory[imageIndex], 0, sizeof(ubo), 0, &data);
    memcpy(data, &ubo, sizeof(ubo));
    vkUnmapMemory(g_device, g_uniformBuffersMemory[imageIndex]);
    
    // Bind descriptor set
    vkCmdBindDescriptorSets(g_commandBuffers[imageIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, g_pipelineLayout, 0, 1, &g_descriptorSets[imageIndex], 0, nullptr);
    
    // Bind vertex buffer
    VkBuffer vertexBuffers[] = {g_fpsCubeVertexBuffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(g_commandBuffers[imageIndex], 0, 1, vertexBuffers, offsets);
    
    // Bind index buffer
    vkCmdBindIndexBuffer(g_commandBuffers[imageIndex], g_fpsCubeIndexBuffer, 0, VK_INDEX_TYPE_UINT16);
    
    // Draw cube
    vkCmdDrawIndexed(g_commandBuffers[imageIndex], g_fpsCubeIndexCount, 1, 0, 0, 0);
    
    // ... render Neuroshell UI, end render pass, submit, present ...
}
```

### Working Spinning Cube Render Function (heidic_render_frame_cube)
```cpp
// Line ~1884
extern "C" void heidic_render_frame_cube(GLFWwindow* window) {
    // ... same Vulkan frame setup ...
    
    // Bind pipeline
    vkCmdBindPipeline(g_commandBuffers[imageIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, g_cubePipeline);
    
    // Update uniform buffer
    UniformBufferObject ubo = {};
    
    // Model matrix - rotate the cube
    Vec3 axis = {1.0f, 1.0f, 0.0f};
    ubo.model = mat4_rotate(axis, g_cubeRotationAngle);
    
    // View matrix - look at cube from above and to the side
    Vec3 eye = {3.0f, 3.0f, 3.0f};
    Vec3 center = {0.0f, 0.0f, 0.0f};
    Vec3 up = {0.0f, 1.0f, 0.0f};
    ubo.view = mat4_lookat(eye, center, up);
    
    // Projection matrix
    float fov = 45.0f * 3.14159f / 180.0f;
    float aspect = (float)g_swapchainExtent.width / (float)g_swapchainExtent.height;
    float nearPlane = 0.1f;
    float farPlane = 100.0f;
    ubo.proj = mat4_perspective(fov, aspect, nearPlane, farPlane);
    ubo.proj[1][1] *= -1.0f;
    
    // Update uniform buffer (same code)
    void* data;
    vkMapMemory(g_device, g_uniformBuffersMemory[imageIndex], 0, sizeof(ubo), 0, &data);
    memcpy(data, &ubo, sizeof(ubo));
    vkUnmapMemory(g_device, g_uniformBuffersMemory[imageIndex]);
    
    // Bind descriptor set (same)
    vkCmdBindDescriptorSets(g_commandBuffers[imageIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, g_pipelineLayout, 0, 1, &g_descriptorSets[imageIndex], 0, nullptr);
    
    // Bind vertex buffer
    VkBuffer vertexBuffers[] = {g_cubeVertexBuffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(g_commandBuffers[imageIndex], 0, 1, vertexBuffers, offsets);
    
    // Bind index buffer
    vkCmdBindIndexBuffer(g_commandBuffers[imageIndex], g_cubeIndexBuffer, 0, VK_INDEX_TYPE_UINT16);
    
    // Draw cube
    vkCmdDrawIndexed(g_commandBuffers[imageIndex], g_cubeIndexCount, 1, 0, 0, 0);
    
    // ... end render pass, submit, present ...
}
```

## Key Differences Between Working and Broken Code

### Differences Found:
1. **View Matrix**: FPS uses dynamic camera calculation, spinning cube uses fixed view
   - **Tried**: Using exact same fixed view as spinning cube → Still broken
   
2. **FOV**: FPS uses 70°, spinning cube uses 45°
   - **Tried**: Using exact same 45° FOV → Still broken

3. **Buffers**: FPS uses `g_fpsCubeVertexBuffer`, spinning cube uses `g_cubeVertexBuffer`
   - **Tried**: Using exact same `g_cubeVertexBuffer` → Still broken

4. **Pipeline**: FPS uses `g_fpsPipeline`, spinning cube uses `g_cubePipeline`
   - **Tried**: Using exact same `g_cubePipeline` → Still broken

## Pipeline Configuration

Both pipelines are configured identically:
- Same vertex input (position + color, 2 attributes)
- Same input assembly (triangle list)
- Same viewport/scissor
- Same rasterization (back face culling, counter-clockwise front face)
- Same multisampling (1 sample)
- Same depth stencil (depth test enabled, VK_COMPARE_OP_LESS)
- Same color blending (no blending)
- Same pipeline layout (`g_pipelineLayout`)
- Same render pass (`g_renderPass`)

## Uniform Buffer Object Structure
```cpp
struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};
```

## Mat4 Helper Functions
```cpp
// From stdlib/math.h
inline Mat4 mat4_lookat(Vec3 eye, Vec3 center, Vec3 up) {
    return Mat4(glm::lookAt(glm::vec3(eye), glm::vec3(center), glm::vec3(up)));
}

inline Mat4 mat4_perspective(float fov_rad, float aspect, float near_plane, float far_plane) {
    return Mat4(glm::perspectiveRH_ZO(fov_rad, aspect, near_plane, far_plane));
}

struct Mat4 {
    glm::mat4 data;
    float m[16];
    
    Mat4(const glm::mat4& mat) : data(mat) { updateM(); }
    operator glm::mat4() const { return data; }
    // ...
};
```

## Observations

1. **Z-Fighting**: All faces appear at the same depth, suggesting depth buffer isn't working OR all vertices are being transformed to the same depth
2. **Flat Appearance**: Cube looks flat, suggesting perspective projection might not be working
3. **Both Pipelines Fail**: Even using the exact same pipeline that works for spinning cube fails, suggesting the issue is NOT in pipeline creation
4. **Same Buffers Fail**: Even using the exact same vertex/index buffers that work for spinning cube fails, suggesting the issue is NOT in buffer data

## Hypotheses

1. **Uniform Buffer Not Updating**: Maybe the uniform buffer isn't being updated correctly for the FPS renderer
2. **Descriptor Set Issue**: Maybe the descriptor set isn't bound correctly or points to wrong uniform buffer
3. **Matrix Conversion Issue**: Maybe the Mat4 to glm::mat4 conversion isn't working correctly in the FPS renderer context
4. **Shader Issue**: Maybe the shaders expect different uniform buffer layout or the shader modules are different
5. **Render Pass State**: Maybe something about the render pass state is different between the two renderers
6. **Command Buffer State**: Maybe the command buffer is in a bad state when FPS renderer runs

## Debugging Suggestions

1. **Add Debug Output**: Print the actual matrix values being sent to the uniform buffer
2. **Compare Shader Modules**: Verify that `g_fpsVertShaderModule` and `g_cubeVertShaderModule` are identical
3. **Check Descriptor Sets**: Verify that `g_descriptorSets[imageIndex]` points to the correct uniform buffer
4. **Verify Uniform Buffer Memory**: Check that the uniform buffer memory is actually being written to
5. **Compare Render Pass State**: Verify that both renderers use the exact same render pass configuration
6. **Check Command Buffer**: Verify that the command buffer is properly reset and in correct state
7. **Test with Fixed View**: Try using the exact same view matrix calculation code from spinning cube (copy-paste, no modifications)

## Files to Examine

1. `vulkan/eden_vulkan_helpers.cpp` - Main rendering code
2. `stdlib/math.h` - Mat4 helper functions
3. Shader files: `vert_3d.spv`, `frag_3d.spv`, `vert_cube.spv`, `frag_cube.spv` (if available)
4. `ELECTROSCRIBE/PROJECTS/fps_camera_test/fps_camera_test.hd` - HEIDIC source code

## Environment

- **OS**: Windows 10
- **Compiler**: MinGW-w64 (g++)
- **Graphics API**: Vulkan
- **Language**: C++17
- **Project**: HEIDIC compiler + FPS camera test

## Next Steps for Debugging

1. Add matrix value printing to verify uniform buffer contents
2. Verify shader modules are identical (compare byte-by-byte or use same shader files)
3. Check if uniform buffer descriptor set binding is correct
4. Verify render pass and framebuffer state
5. Compare command buffer recording between working and broken renderers line-by-line
6. Test with minimal changes: copy entire `heidic_render_frame_cube` function and only change the view matrix calculation

---

**Status**: Issue persists after extensive debugging attempts. All obvious differences between working and broken code have been eliminated, yet the problem remains. The issue appears to be subtle and may be related to Vulkan state management, uniform buffer updates, or shader compilation differences.

