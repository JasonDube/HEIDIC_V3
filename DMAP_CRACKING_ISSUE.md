# DMap Vertex Separation/Cracking Issue - Code Analysis

## Problem
When applying DMap (Deformer Map) displacement, vertices are separating at UV seams, causing visible cracks in the face. This is a classic issue with texture-based displacement where vertices along UV seams have different UV1 coordinates but share the same 3D position, causing them to sample different displacement values and split apart.

## System Overview

The system uses:
- **UV1 coordinates** to sample a DMap texture in the vertex shader
- **RGB channels** encode XYZ displacement (-1 to +1 range, stored as 0-255)
- **Vertex shader** applies displacement per-vertex based on UV1 sampling
- **No vertex welding** - each vertex is processed independently

## Key Code Files

### 1. Vertex Shader: `vulkan/facial/shaders/dmap_mesh.vert`

```glsl
#version 450

// Vertex inputs (POSITION_NORMAL_UV0_UV1 format)
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord0;  // UV0 - standard texture coordinates
layout(location = 3) in vec2 inTexCoord1;  // UV1 - DMap sampling coordinates

// Facial UBO - slider weights and settings
layout(binding = 0) uniform FacialUBO {
    vec4 weights[MAX_SLIDER_VECS];  // 32 slider weights packed into 8 vec4s
    vec4 settings;                   // x = globalStrength, y = mirrorThreshold, z = hasDMap (1.0 if loaded)
} facial;

// DMap texture (sampled in vertex shader!)
layout(binding = 1) uniform sampler2D dmapTexture;

void main() {
    vec3 position = inPosition;
    vec3 normal = inNormal;
    
    float slider0 = getSliderWeight(0);  // Jaw Open - for now, primary test slider
    float globalStrength = facial.settings.x;
    float hasDMap = facial.settings.z;  // 1.0 if DMap is loaded, 0.0 otherwise
    float maxDisplacement = 0.03;  // 3cm max displacement
    
    vec3 displacement = vec3(0.0);
    
    // DMap MODE: Sample displacement from texture using UV1 coordinates
    if (hasDMap > 0.5 && globalStrength > 0.001 && slider0 > 0.001) {
        vec2 dmapUV = inTexCoord1;
        
        // Safety check: If UV1 is invalid (all zeros or NaN), skip displacement entirely
        if (dmapUV.x == 0.0 && dmapUV.y == 0.0) {
            displacement = vec3(0.0);
        } else if (isnan(dmapUV.x) || isnan(dmapUV.y) || isinf(dmapUV.x) || isinf(dmapUV.y)) {
            displacement = vec3(0.0);
        } else {
            // Valid UV1 - proceed with sampling
            dmapUV = clamp(dmapUV, vec2(0.0), vec2(1.0));
            
            // Sample displacement from DMap texture
            vec4 dmapSample = texture(dmapTexture, dmapUV);
            
            // Convert from [0,1] to [-1,1] range
            vec3 dmapDisplacement = dmapSample.rgb * 2.0 - 1.0;
            
            // Check if displacement is significant (at least one component > 0.08)
            float absX = abs(dmapDisplacement.x);
            float absY = abs(dmapDisplacement.y);
            float absZ = abs(dmapDisplacement.z);
            
            if (absX > 0.08 || absY > 0.08 || absZ > 0.08) {
                // Apply displacement along the sampled direction
                displacement = dmapDisplacement * slider0 * globalStrength * maxDisplacement;
            } else {
                displacement = vec3(0.0);
            }
        }
    }
    
    // Apply displacement
    position += displacement;
    
    // Transform position
    vec4 worldPos = push.model * vec4(position, 1.0);
    fragWorldPos = worldPos.xyz;
    
    // Transform normal
    mat3 normalMatrix = transpose(inverse(mat3(push.model)));
    fragNormal = normalize(normalMatrix * normal);
    
    // Pass through UV0 for texture sampling
    fragTexCoord = inTexCoord0;
    
    // Final clip-space position
    gl_Position = push.projection * push.view * worldPos;
}
```

**Key Issue**: Each vertex samples the DMap independently using its UV1 coordinates. If two vertices share the same 3D position but have different UV1 coordinates (common at UV seams), they get different displacement values and split apart.

### 2. Mesh Loading: `vulkan/utils/glb_loader.cpp`

```cpp
// GLBVertex structure
struct GLBVertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoord;   // UV0 - standard texture coordinates
    glm::vec2 texCoord1;  // UV1 - DMap coordinates (for facial animation)
    glm::vec4 color;
};

// Loading code reads TEXCOORD_1 from GLB file
for (size_t ai = 0; ai < prim.attributes_count; ++ai) {
    cgltf_attribute& attr = prim.attributes[ai];
    if (attr.type == cgltf_attribute_type_texcoord) {
        if (attr.index == 0) uvAccessor = attr.data;
        else if (attr.index == 1) uv1Accessor = attr.data;  // UV1
    }
}

// Each vertex gets its UV1 coordinates
if (uv1Accessor) {
    cgltf_accessor_read_float(uv1Accessor, vi, &v.texCoord1.x, 2);
} else {
    v.texCoord1 = glm::vec2(0);
}
```

**Key Issue**: The loader doesn't track which vertices share the same 3D position. Each vertex is stored independently with its own UV1 coordinates.

### 3. GPU Upload: `ELECTROSCRIBE/PROJECTS/EaSE/engine/ease_engine.cpp`

```cpp
void Viewer::uploadMeshToGPU() {
    // Convert to VulkanCore format
    vkcore::MeshData data;
    
    if (m_useFacialAnimation && m_facial.isInitialized()) {
        data.format = vkcore::VertexFormat::POSITION_NORMAL_UV0_UV1;
        
        for (const auto& v : m_mesh.vertices) {
            // Position
            data.vertices.push_back(v.position.x);
            data.vertices.push_back(v.position.y);
            data.vertices.push_back(v.position.z);
            // Normal
            data.vertices.push_back(v.normal.x);
            data.vertices.push_back(v.normal.y);
            data.vertices.push_back(v.normal.z);
            // UV0 (base texture)
            data.vertices.push_back(v.texCoord.x);
            data.vertices.push_back(v.texCoord.y);
            // UV1 (DMap sampling)
            float uv1x = m_mesh.hasUV1 ? v.texCoord1.x : 0.5f;
            float uv1y = m_mesh.hasUV1 ? v.texCoord1.y : 0.5f;
            
            uv1x = glm::clamp(uv1x, 0.0f, 1.0f);
            uv1y = glm::clamp(uv1y, 0.0f, 1.0f);
            
            data.vertices.push_back(uv1x);
            data.vertices.push_back(uv1y);
        }
    }
    
    // Upload to GPU
    m_mesh.renderHandle = m_core.createMesh(data);
}
```

**Key Issue**: Vertices are uploaded as-is, with no vertex welding or averaging. Each vertex maintains its own UV1 coordinates.

### 4. Vertex Format: `vulkan/core/vulkan_core.cpp`

```cpp
case VertexFormat::POSITION_NORMAL_UV0_UV1:
    attrs.resize(4);
    attrs[0] = {0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0};                    // position
    attrs[1] = {1, 0, VK_FORMAT_R32G32B32_SFLOAT, sizeof(float) * 3};    // normal
    attrs[2] = {2, 0, VK_FORMAT_R32G32_SFLOAT, sizeof(float) * 6};       // uv0 (textures)
    attrs[3] = {3, 0, VK_FORMAT_R32G32_SFLOAT, sizeof(float) * 8};       // uv1 (DMap)
    break;
```

## The Problem Explained

1. **UV Seams**: At UV seams, the same 3D vertex position appears in multiple UV islands with different UV1 coordinates.
2. **Independent Sampling**: Each vertex samples the DMap independently using its UV1 coordinates.
3. **Different Displacements**: Vertices that should be welded together get different displacement values.
4. **Cracking**: The vertices separate, creating visible cracks along UV seams.

## Potential Solutions

### Option 1: Vertex Welding (Post-Displacement Averaging)
After sampling the DMap in the vertex shader, average the positions of vertices that share the same 3D location. This requires:
- A vertex position hash map to identify shared vertices
- A compute shader or CPU pass to average displacements
- Storing vertex welding information

### Option 2: DMap Padding/Bleeding
Extend displacement values across UV seams in the DMap texture itself. This helps but doesn't fully solve the problem if seams are sharp.

### Option 3: Higher Resolution DMaps
Use higher resolution textures to reduce interpolation artifacts at seam boundaries.

### Option 4: Topology-Aware Sampling
Store which vertices need to be welded together and handle them specially. Requires:
- Building a vertex adjacency/welding map
- Special handling in shader or CPU

### Option 5: Vertex-Based Blend Shapes
Instead of texture-based DMaps, store actual vertex position deltas. More memory but no seam issues.

## Current Test DMap Creation

The test DMap is created with regions that have smooth blending at boundaries:

```cpp
void Viewer::createRegionTestDMap() {
    // Creates 1024x1024 DMap with regions
    // Each region has displacement in different directions
    // Smooth blending at region boundaries (5% blend size)
    
    struct Region {
        const char* name;
        float uMin, vTop, uMax, vBottom;
        float dispX, dispY, dispZ;
    };
    
    std::vector<Region> regions = {
        {"Body/Non-Essential", 0.000f, 1.000f, 0.125f, 0.878f, 0.0f, 0.0f, 0.0f},
        {"Brow", 0.193f, 0.994f, 0.816f, 0.783f, 0.0f, 0.1f, 0.0f},
        {"Mouth", 0.250f, 0.750f, 0.776f, 0.562f, 0.0f, -0.1f, 0.0f},
        // ... etc
    };
    
    // Smooth blending at boundaries
    const float blendSize = 0.05f;  // 5% of texture size
    // Uses smoothstep for blending
}
```

Even with smooth blending in the DMap, vertices still crack because the issue is at the vertex level, not the texture level.

## Answers to Uncle Claude's Questions

### 1. Is UV1 actually laid out differently than UV0?

**Answer: NO - UV1 is likely a copy of UV0!**

Looking at the `generateUV1()` function in `ease_engine.cpp` (lines 1615-1691):

```cpp
void Viewer::generateUV1() {
    // Method 1: Copy UV0 (best for full body meshes that already have good UV0)
    if (hasValidUV0) {
        // Copy UV0 to UV1 - this works great for full body meshes
        for (auto& v : m_mesh.vertices) {
            v.texCoord1 = v.texCoord;  // ← UV1 is just a copy of UV0!
        }
    }
}
```

**This means UV1 has the SAME seams as UV0!** If the user generated UV1 in EaSE, it's just copying UV0. If they exported from Blender, we need to check if they actually created a separate UV1 channel or if it's also a copy.

**This explains the cracking perfectly** - if UV1 has the same seams as UV0, then vertices at those seams will have different UV1 coordinates but share the same 3D position, causing them to sample different DMap values and split apart.

### 2. What created the GLB file? Was UV1 specifically authored for DMaps?

**Answer: Unknown - needs user verification.**

The code supports two scenarios:
- **GLB with TEXCOORD_1**: If the GLB file has a second UV channel (TEXCOORD_1), it's loaded as UV1
- **Auto-generated UV1**: If no UV1 exists, the user can click "Generate UV1" which either:
  - Copies UV0 (if UV0 is valid)
  - Uses planar projection (if UV0 is invalid)

**We need to ask the user:**
- Did you export UV1 from Blender specifically for DMaps?
- Or did you use the "Generate UV1" button in EaSE?
- If you generated it in EaSE, it's likely just a copy of UV0!

### 3. Can we add UV1 debug visualization?

**Answer: YES - Just implemented!**

I've added a UV1 coordinate visualization mode:
- **Shader changes**: Modified `dmap_mesh.vert` to pass UV1 to fragment shader
- **Fragment shader**: Added debug mode that shows UV1 as colors (R=U, G=V, B=0)
- **UI toggle**: Added checkbox "Show UV1 Coordinates (Debug)" in Facial Animation panel
- **How to use**: Enable facial animation, then check "Show UV1 Coordinates (Debug)"

**This will show:**
- Red = U coordinate (0-1)
- Green = V coordinate (0-1)  
- Blue = 0

**You'll be able to see:**
- Where UV seams are (sharp color changes)
- If UV1 is identical to UV0 (same seam locations)
- If UV1 has different seams (different color patterns)

## Questions for Analysis

1. **Does the GLB file have proper UV1 coordinates?** (Check if UV1 is actually different from UV0) - **LIKELY NO, it's probably a copy**
2. **Are UV seams causing the issue?** (Check if vertices at seams have different UV1 values) - **YES, and UV1 likely has same seams as UV0**
3. **Should we implement vertex welding?** (Most robust solution but requires significant changes)
4. **Can we pad the DMap better?** (Simpler but may not fully solve)
5. **Should we switch to vertex-based blend shapes?** (Different approach entirely)

## Solution Implemented: DMap Padding

Based on Uncle Claude's feedback, we've implemented **DMap padding/dilation** - a simpler approach than vertex welding that extends displacement values across UV seams.

### How It Works

1. **Before creating the texture**, we process the DMap pixel data
2. **For each neutral grey pixel** (128,128,128), we look at nearby pixels
3. **If nearby pixels have displacement values**, we blend them into the neutral area
4. **This creates a "bleed" effect** that extends displacement values across seams

### Implementation Details

- **Padding radius**: 2 pixels (configurable)
- **Blend factor**: Max 50% influence (subtle padding, doesn't overpower original values)
- **Distance-weighted**: Closer neighbors have stronger influence
- **Automatic**: Enabled by default for all DMaps

### Code Location

- `FacialSystem::padDMapSeams()` - The padding function
- `FacialSystem::loadDMapFromMemory()` - Calls padding before creating texture
- Can be disabled by passing `padSeams=false` (for testing)

### Testing

1. Create a test DMap with regions
2. The padding will automatically extend values across region boundaries
3. This should reduce/eliminate vertex separation at seams
4. If issues persist, can increase padding radius or adjust blend factor

## Next Steps

1. **Test the padding**: Create a test DMap and see if cracking is reduced
2. **Adjust parameters**: If needed, increase padding radius or blend factor
3. **Compare with/without**: Can disable padding to verify it's helping
4. **If still issues**: May need to implement vertex welding (more complex)

