#version 450

// ============================================================================
// LIT MESH VERTEX SHADER
// ============================================================================
// Part of the EDEN Engine modular lighting system.
// Uses push constants for per-object data (model matrix, color).
// ============================================================================

// Vertex inputs (matches POSITION_NORMAL_UV format)
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

// Outputs to fragment shader
layout(location = 0) out vec3 fragWorldPos;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragTexCoord;
layout(location = 3) out vec4 fragObjectColor;

// Maximum point lights (must match fragment shader)
#define MAX_POINT_LIGHTS 4

// Push constants for per-object data (changes every draw call)
layout(push_constant) uniform PushConstants {
    mat4 model;
    vec4 objectColor;
} push;

// Uniform buffer for per-frame data (constant during frame)
layout(binding = 0) uniform LightingUBO {
    mat4 view;
    mat4 projection;
    vec4 lightDir;
    vec4 lightColor;
    vec4 ambientColor;
    vec4 cameraPos;
    vec4 material;
    vec4 pointLightPosInt[MAX_POINT_LIGHTS];
    vec4 pointLightColorRange[MAX_POINT_LIGHTS];
} ubo;

void main() {
    // World-space position
    vec4 worldPos = push.model * vec4(inPosition, 1.0);
    fragWorldPos = worldPos.xyz;
    
    // World-space normal (using normal matrix for non-uniform scaling)
    mat3 normalMatrix = transpose(inverse(mat3(push.model)));
    fragNormal = normalize(normalMatrix * inNormal);
    
    // Pass through texture coordinates
    fragTexCoord = inTexCoord;
    
    // Pass object color to fragment shader
    fragObjectColor = push.objectColor;
    
    // Final clip-space position
    gl_Position = ubo.projection * ubo.view * worldPos;
}
