#version 450

// ============================================================================
// DMAP MESH FRAGMENT SHADER - Basic Lit Rendering
// ============================================================================
// Simple Blinn-Phong lighting for displaced facial meshes.
// Uses UV0 for base color texture sampling.
// ============================================================================

// Inputs from vertex shader
layout(location = 0) in vec3 fragWorldPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragTexCoord;  // UV0
layout(location = 3) in vec2 fragTexCoord1; // UV1 - for debug visualization

// Output color
layout(location = 0) out vec4 outColor;

// Base color texture (sampled with UV0)
layout(binding = 2) uniform sampler2D baseTexture;

// Facial UBO - for debug mode
layout(binding = 0) uniform FacialUBO {
    vec4 weights[8];  // Slider weights (not used in fragment shader)
    vec4 settings;    // x = globalStrength, y = mirrorThreshold, z = hasDMap, w = debugMode
} facial;

// Push constants
layout(push_constant) uniform PushConstants {
    mat4 model;
    mat4 view;
    mat4 projection;
    vec4 objectColor;
} push;

// Simple directional light (hardcoded for now)
const vec3 LIGHT_DIR = normalize(vec3(0.5, -1.0, 0.5));
const vec3 LIGHT_COLOR = vec3(1.0, 0.98, 0.95);
const vec3 AMBIENT_COLOR = vec3(0.2, 0.2, 0.25);

void main() {
    // Debug Mode: Show UV1 coordinates as colors (R=U, G=V, B=0)
    // Use settings.w as debug mode flag: 1.0 = UV1 visualization
    if (facial.settings.w > 0.5) {
        // Check if UV1 is invalid (all zeros or NaN)
        if (fragTexCoord1.x == 0.0 && fragTexCoord1.y == 0.0) {
            // Invalid UV1 - show bright magenta to indicate problem
            outColor = vec4(1.0, 0.0, 1.0, 1.0);  // Magenta = invalid UV1
            return;
        }
        
        // Clamp UV1 to [0,1] range for display
        vec2 uv1Clamped = clamp(fragTexCoord1, vec2(0.0), vec2(1.0));
        
        // Enhance visibility: multiply by 2 and clamp to make colors more vibrant
        // This helps see subtle variations
        float u = clamp(uv1Clamped.x * 2.0, 0.0, 1.0);
        float v = clamp(uv1Clamped.y * 2.0, 0.0, 1.0);
        
        outColor = vec4(u, v, 0.0, 1.0);
        return;
    }
    
    // Normal rendering mode
    // Sample base texture
    vec4 texColor = texture(baseTexture, fragTexCoord);
    vec3 baseColor = texColor.rgb * push.objectColor.rgb;
    
    // Normalize inputs
    vec3 N = normalize(fragNormal);
    vec3 L = normalize(-LIGHT_DIR);
    
    // Ambient
    vec3 ambient = AMBIENT_COLOR * baseColor;
    
    // Diffuse (Lambert)
    float diff = max(dot(N, L), 0.0);
    vec3 diffuse = diff * LIGHT_COLOR * baseColor;
    
    // Simple specular (Blinn-Phong)
    vec3 viewDir = normalize(-fragWorldPos);  // Approximate
    vec3 halfDir = normalize(L + viewDir);
    float spec = pow(max(dot(N, halfDir), 0.0), 32.0);
    vec3 specular = spec * LIGHT_COLOR * 0.3;
    
    // Final color
    vec3 result = ambient + diffuse + specular;
    
    outColor = vec4(result, texColor.a * push.objectColor.a);
}

