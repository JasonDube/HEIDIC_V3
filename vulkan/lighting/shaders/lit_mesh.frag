#version 450

// ============================================================================
// LIT MESH FRAGMENT SHADER - Blinn-Phong Lighting
// ============================================================================
// Part of the EDEN Engine modular lighting system.
// Supports directional light + up to 4 point lights.
// Per-object data comes from vertex shader (via push constants).
// ============================================================================

// Inputs from vertex shader
layout(location = 0) in vec3 fragWorldPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragTexCoord;
layout(location = 3) in vec4 fragObjectColor;

// Output color
layout(location = 0) out vec4 outColor;

// Maximum point lights (must match C++ MAX_POINT_LIGHTS)
#define MAX_POINT_LIGHTS 4

// Uniform buffer for per-frame data (constant during frame)
layout(binding = 0) uniform LightingUBO {
    mat4 view;
    mat4 projection;
    vec4 lightDir;       // xyz = direction, w = intensity
    vec4 lightColor;     // rgb = color
    vec4 ambientColor;   // rgb = ambient
    vec4 cameraPos;      // xyz = camera position
    vec4 material;       // x = shininess, y = specularStrength, z = numPointLights, w = debugMode (1=normals)
    
    // Point lights
    vec4 pointLightPosInt[MAX_POINT_LIGHTS];    // xyz = position, w = intensity
    vec4 pointLightColorRange[MAX_POINT_LIGHTS]; // xyz = color, w = range
} ubo;

// Texture sampler
layout(binding = 1) uniform sampler2D texSampler;

// Calculate point light contribution
vec3 calcPointLight(int index, vec3 N, vec3 V, vec3 baseColor, float shininess, float specularStrength) {
    vec3 lightPos = ubo.pointLightPosInt[index].xyz;
    float intensity = ubo.pointLightPosInt[index].w;
    vec3 lightCol = ubo.pointLightColorRange[index].xyz;
    float range = ubo.pointLightColorRange[index].w;
    
    // Skip disabled lights (intensity <= 0)
    if (intensity <= 0.0) return vec3(0.0);
    
    // Direction from fragment to light
    vec3 lightVec = lightPos - fragWorldPos;
    float distance = length(lightVec);
    vec3 L = normalize(lightVec);
    
    // Attenuation (smooth falloff)
    float attenuation = 1.0 - smoothstep(0.0, range, distance);
    attenuation *= attenuation;  // Quadratic falloff feels more natural
    
    if (attenuation <= 0.0) return vec3(0.0);
    
    // Diffuse
    float diff = max(dot(N, L), 0.0);
    vec3 diffuse = diff * lightCol * baseColor * intensity * attenuation;
    
    // Specular (Blinn-Phong)
    vec3 H = normalize(L + V);
    float spec = pow(max(dot(N, H), 0.0), shininess);
    vec3 specular = spec * specularStrength * lightCol * intensity * attenuation;
    
    return diffuse + specular;
}

void main() {
    // Sample texture
    vec4 texColor = texture(texSampler, fragTexCoord);
    vec3 baseColor = texColor.rgb * fragObjectColor.rgb;
    
    // Normalize inputs - handle zero-length normals
    vec3 rawNormal = fragNormal;
    float normalLen = length(rawNormal);
    vec3 N = normalLen > 0.001 ? normalize(rawNormal) : vec3(0.0, 1.0, 0.0);
    
    vec3 V = normalize(ubo.cameraPos.xyz - fragWorldPos);
    
    // Material properties
    float shininess = ubo.material.x;
    float specularStrength = ubo.material.y;
    int numPointLights = int(ubo.material.z);
    int debugMode = int(ubo.material.w);
    
    // ========================================================================
    // Debug Mode: Show Normals as RGB
    // ========================================================================
    if (debugMode == 1) {
        // Map normal from [-1,1] to [0,1] for RGB display
        vec3 normalColor = N * 0.5 + 0.5;
        outColor = vec4(normalColor, 1.0);
        return;
    }
    
    // Debug Mode 2: Texture only (no lighting)
    if (debugMode == 2) {
        outColor = vec4(baseColor, 1.0);  // Force alpha to 1.0
        return;
    }
    
    // Debug Mode 3: Force opaque white (test if pipeline works at all)
    if (debugMode == 3) {
        outColor = vec4(1.0, 1.0, 1.0, 1.0);
        return;
    }
    
    // Debug Mode 4: Show UV coordinates as colors (R=U, G=V, B=0)
    if (debugMode == 4) {
        outColor = vec4(fragTexCoord.x, fragTexCoord.y, 0.0, 1.0);
        return;
    }
    
    // Debug Mode 5: Raw texture (ignore object color multiplier)
    if (debugMode == 5) {
        outColor = vec4(texColor.rgb, 1.0);
        return;
    }
    
    // Debug Mode 6: Show texture alpha channel (white=opaque, black=transparent)
    if (debugMode == 6) {
        outColor = vec4(texColor.aaa, 1.0);
        return;
    }
    
    // Debug Mode 7: Highlight "problem" pixels - show bright magenta where texture is very dark
    if (debugMode == 7) {
        float brightness = dot(texColor.rgb, vec3(0.299, 0.587, 0.114));  // Luminance
        if (brightness < 0.05) {
            outColor = vec4(1.0, 0.0, 1.0, 1.0);  // Magenta for dark pixels
        } else {
            outColor = vec4(texColor.rgb, 1.0);
        }
        return;
    }
    
    // Debug Mode 8: Check UV validity - show red for UVs outside 0-1 range
    if (debugMode == 8) {
        bool uvValid = (fragTexCoord.x >= 0.0 && fragTexCoord.x <= 1.0 &&
                        fragTexCoord.y >= 0.0 && fragTexCoord.y <= 1.0);
        if (!uvValid) {
            outColor = vec4(1.0, 0.0, 0.0, 1.0);  // Red for invalid UVs
        } else {
            outColor = vec4(0.0, 1.0, 0.0, 1.0);  // Green for valid UVs
        }
        return;
    }
    
    // Debug Mode 9: Sample texture at fixed center UV (0.5, 0.5) - tests if texture regions differ
    if (debugMode == 9) {
        vec4 centerTex = texture(texSampler, vec2(0.5, 0.5));
        outColor = vec4(centerTex.rgb, 1.0);
        return;
    }
    
    // Debug Mode 10: Show which UV quadrant (helps identify texture atlas issues)
    if (debugMode == 10) {
        float u = fragTexCoord.x;
        float v = fragTexCoord.y;
        vec3 quadColor;
        if (u < 0.5 && v < 0.5) quadColor = vec3(1.0, 0.0, 0.0);      // Bottom-left: Red
        else if (u >= 0.5 && v < 0.5) quadColor = vec3(0.0, 1.0, 0.0); // Bottom-right: Green
        else if (u < 0.5 && v >= 0.5) quadColor = vec3(0.0, 0.0, 1.0); // Top-left: Blue
        else quadColor = vec3(1.0, 1.0, 0.0);                          // Top-right: Yellow
        outColor = vec4(quadColor, 1.0);
        return;
    }
    
    // ========================================================================
    // Ambient
    // ========================================================================
    vec3 ambient = ubo.ambientColor.rgb * baseColor;
    
    // ========================================================================
    // Directional Light
    // ========================================================================
    vec3 dirResult = vec3(0.0);
    float dirIntensity = ubo.lightDir.w;
    if (dirIntensity > 0.0) {
        vec3 L = normalize(-ubo.lightDir.xyz);
        vec3 H = normalize(L + V);
        
        // Diffuse
        float diff = max(dot(N, L), 0.0);
        vec3 diffuse = diff * ubo.lightColor.rgb * baseColor * dirIntensity;
        
        // Specular
        float spec = pow(max(dot(N, H), 0.0), shininess);
        vec3 specular = spec * specularStrength * ubo.lightColor.rgb * dirIntensity;
        
        dirResult = diffuse + specular;
    }
    
    // ========================================================================
    // Point Lights
    // ========================================================================
    vec3 pointResult = vec3(0.0);
    for (int i = 0; i < MAX_POINT_LIGHTS; ++i) {
        if (i >= numPointLights) break;
        pointResult += calcPointLight(i, N, V, baseColor, shininess, specularStrength);
    }
    
    // ========================================================================
    // Final color
    // ========================================================================
    vec3 result = ambient + dirResult + pointResult;
    
    // Apply texture alpha with lighting
    outColor = vec4(result, texColor.a * fragObjectColor.a);
}
