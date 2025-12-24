#version 450

// ============================================================================
// UNLIT MODEL SHADER - Pure texture output (no lighting)
// ============================================================================
// Use this when lighting is disabled. Shows texture * vertex color directly.
// For lit rendering, use lit_mesh.frag instead.
// ============================================================================

// Input from vertex shader
layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragWorldPos;
layout(location = 3) in vec4 fragColor;

// Texture sampler (binding 1)
layout(set = 0, binding = 1) uniform sampler2D texSampler;

// Output
layout(location = 0) out vec4 outColor;

void main() {
    // Sample texture
    vec4 texColor = texture(texSampler, fragTexCoord);
    
    // Pure unlit: texture * vertex color (no lighting calculation)
    outColor = vec4(texColor.rgb * fragColor.rgb, texColor.a * fragColor.a);
}
