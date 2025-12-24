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
layout(location = 2) out vec2 fragTexCoord;

// Maximum sliders (must match C++ MAX_SLIDERS / 4)
#define MAX_SLIDER_VECS 8

// Facial UBO - slider weights and settings
layout(binding = 0) uniform FacialUBO {
    vec4 weights[MAX_SLIDER_VECS];  // 32 slider weights packed into 8 vec4s
    vec4 settings;                   // x = globalStrength, y = mirrorThreshold
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
    
    // Sample DMap using UV1 coordinates
    vec2 dmapUV = inTexCoord1;
    
    // Mirror X if on left side (UV1.x < mirrorThreshold)
    float mirrorThreshold = facial.settings.y;
    if (dmapUV.x < mirrorThreshold) {
        dmapUV.x = 1.0 - dmapUV.x;
    }
    
    // Sample displacement from DMap
    // RGB encodes XYZ displacement in range [0, 1], mapped to [-1, 1]
    vec4 dmapSample = texture(dmapTexture, dmapUV);
    vec3 displacement = dmapSample.rgb * 2.0 - 1.0;
    
    // Apply displacement scaled by first slider weight and global strength
    // In a full implementation, you'd sample multiple DMaps and blend them
    float sliderWeight = getSliderWeight(0);  // Use first slider for now
    float globalStrength = facial.settings.x;
    float maxDisplacement = 0.05;  // 5cm max displacement
    
    position += displacement * sliderWeight * globalStrength * maxDisplacement;
    
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

