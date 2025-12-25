#version 450

// ============================================================================
// DMAP MESH VERTEX SHADER - Facial Animation with UV1 Displacement
// ============================================================================
// Samples a DMap texture using UV1 coordinates to displace vertices.
// RGB channels of DMap encode XYZ displacement deltas (-1 to 1 range).
// Slider weights control the strength of each displacement.
// ============================================================================

// Vertex inputs (POSITION_NORMAL_UV0_UV1 format)
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord0;  // UV0 - standard texture coordinates
layout(location = 3) in vec2 inTexCoord1;  // UV1 - DMap sampling coordinates

// Outputs to fragment shader
layout(location = 0) out vec3 fragWorldPos;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragTexCoord;  // UV0
layout(location = 3) out vec2 fragTexCoord1; // UV1 - for debug visualization

// Maximum sliders (must match C++ MAX_SLIDERS / 4)
#define MAX_SLIDER_VECS 8

// Facial UBO - slider weights and settings
layout(binding = 0) uniform FacialUBO {
    vec4 weights[MAX_SLIDER_VECS];  // 32 slider weights packed into 8 vec4s
    vec4 settings;                   // x = globalStrength, y = mirrorThreshold, z = hasDMap (1.0 if loaded)
} facial;

// DMap texture (sampled in vertex shader!)
layout(binding = 1) uniform sampler2D dmapTexture;

// Push constants for per-object data
layout(push_constant) uniform PushConstants {
    mat4 model;
    mat4 view;
    mat4 projection;
    vec4 objectColor;
} push;

// Helper to get slider weight by index
float getSliderWeight(int index) {
    int vecIndex = index / 4;
    int component = index % 4;
    return facial.weights[vecIndex][component];
}

void main() {
    // Start with base position
    vec3 position = inPosition;
    vec3 normal = inNormal;
    
    // Get slider weights
    float slider0 = getSliderWeight(0);  // Jaw Open - for now, primary test slider
    
    float globalStrength = facial.settings.x;
    float hasDMap = facial.settings.z;  // 1.0 if DMap is loaded, 0.0 otherwise
    float maxDisplacement = 0.03;  // 3cm max displacement - balanced: visible but prevents separation
    
    vec3 displacement = vec3(0.0);
    
    // DMap MODE: Sample displacement from texture using UV1 coordinates
    // DISABLED: Using CPU-side vertex welding instead
    // Shader-side displacement causes cracking at UV seams
    // CPU-side approach samples once per unique position and applies to all welded vertices
    if (false && hasDMap > 0.5 && globalStrength > 0.001 && slider0 > 0.001) {
        // Get UV1 coordinates for DMap sampling
        vec2 dmapUV = inTexCoord1;
        
        // Safety check: If UV1 is invalid (all zeros or NaN), skip displacement entirely
        // This prevents uniform displacement when UV1 data is missing or invalid
        if (dmapUV.x == 0.0 && dmapUV.y == 0.0) {
            // Invalid UV1 - no displacement
            displacement = vec3(0.0);
        } else if (isnan(dmapUV.x) || isnan(dmapUV.y) || isinf(dmapUV.x) || isinf(dmapUV.y)) {
            // Invalid UV1 - no displacement
            displacement = vec3(0.0);
        } else {
            // Valid UV1 - proceed with sampling
            // Clamp UV to valid range (but allow full 0-1 range, just avoid edge artifacts)
            dmapUV = clamp(dmapUV, vec2(0.0), vec2(1.0));
            
            // Sample displacement from DMap texture
            // RGB encodes XYZ displacement: 0.5 = no displacement, 0 = -1, 1 = +1
            vec4 dmapSample = texture(dmapTexture, dmapUV);
            
            // Convert from [0,1] to [-1,1] range
            // RGB encodes XYZ displacement: 0.5 = no displacement, 0 = -1, 1 = +1
            vec3 dmapDisplacement = dmapSample.rgb * 2.0 - 1.0;
            
            // Check if displacement is significant (at least one component > 0.08)
            // Neutral grey (128,128,128) = 0.502 → (0.004) after conversion, well below 0.08
            // Our test regions use 0.1-0.15, which convert to ~0.1-0.15, above 0.08
            float absX = abs(dmapDisplacement.x);
            float absY = abs(dmapDisplacement.y);
            float absZ = abs(dmapDisplacement.z);
            
            // Only apply if at least one component is significant
            // Threshold 0.08 allows our 0.1+ values while filtering neutral grey (~0.004)
            if (absX > 0.08 || absY > 0.08 || absZ > 0.08) {
                // Apply displacement along the sampled direction
                // Scale by slider weight and global strength
                displacement = dmapDisplacement * slider0 * globalStrength * maxDisplacement;
            } else {
                // Neutral grey or too small - no displacement
                displacement = vec3(0.0);
            }
        }
    } else {
        // No DMap loaded or slider inactive - no displacement
        displacement = vec3(0.0);
    }
    
    // FALLBACK: If no DMap loaded, use simple normal-based inflation (for testing)
    // DISABLED - we don't want uniform displacement
    // if (hasDMap < 0.5 && globalStrength > 0.001 && slider0 > 0.001) {
    //     // Simple inflation along normals (visible feedback without DMap)
    //     displacement = normal * slider0 * globalStrength * 0.05;
    // }
    
    // Apply displacement
    position += displacement;
    
    // Safety check: ensure position is valid (not NaN/Inf)
    if (isnan(position.x) || isnan(position.y) || isnan(position.z) ||
        isinf(position.x) || isinf(position.y) || isinf(position.z)) {
        position = inPosition;  // Fallback to original position
    }
    
    // Transform position
    vec4 worldPos = push.model * vec4(position, 1.0);
    fragWorldPos = worldPos.xyz;
    
    // Transform normal
    mat3 normalMatrix = transpose(inverse(mat3(push.model)));
    fragNormal = normalize(normalMatrix * normal);
    
    // Pass through UV0 for texture sampling
    fragTexCoord = inTexCoord0;
    
    // Pass through UV1 for debug visualization
    fragTexCoord1 = inTexCoord1;
    
    // Final clip-space position
    gl_Position = push.projection * push.view * worldPos;
    
    // Safety check: ensure clip position is valid
    if (isnan(gl_Position.x) || isnan(gl_Position.y) || isnan(gl_Position.z) || isnan(gl_Position.w)) {
        gl_Position = vec4(0.0, 0.0, 0.0, 1.0);  // Fallback to origin
    }
}

