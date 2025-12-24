// ============================================================================
// LIGHTING TYPES - Data structures for the Lighting Module
// ============================================================================
// Part of the EDEN Engine modular lighting system.
// These types define the GPU-side data layout for lighting calculations.
// ============================================================================

#ifndef LIGHTING_TYPES_H
#define LIGHTING_TYPES_H

#include <glm/glm.hpp>

namespace lighting {

// ============================================================================
// Light Definitions (CPU-side, user-friendly)
// ============================================================================

struct DirectionalLight {
    glm::vec3 direction = glm::vec3(0.5f, -1.0f, 0.5f);  // Default: top-right-front
    glm::vec3 color = glm::vec3(1.0f, 0.98f, 0.95f);     // Warm white
    float intensity = 1.0f;
    
    DirectionalLight() = default;
    DirectionalLight(glm::vec3 dir, glm::vec3 col, float i = 1.0f)
        : direction(glm::normalize(dir)), color(col), intensity(i) {}
};

struct AmbientLight {
    glm::vec3 color = glm::vec3(0.0f, 0.0f, 0.0f);  // No ambient - pitch black shadows
    
    AmbientLight() = default;
    AmbientLight(glm::vec3 col) : color(col) {}
};

struct PointLight {
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 color = glm::vec3(1.0f);
    float intensity = 1.0f;
    float range = 10.0f;  // Light falloff distance
    bool enabled = false;
    
    PointLight() = default;
    PointLight(glm::vec3 pos, glm::vec3 col, float i = 1.0f, float r = 10.0f)
        : position(pos), color(col), intensity(i), range(r), enabled(true) {}
};

// Maximum number of point lights supported
constexpr int MAX_POINT_LIGHTS = 4;

// ============================================================================
// Push Constants (per-object data, sent via vkCmdPushConstants)
// ============================================================================
// This changes per draw call - model matrix and object color.
// Max push constant size is typically 128-256 bytes.
// Our struct is 80 bytes (mat4 = 64, vec4 = 16).
// ============================================================================

struct PushConstants {
    glm::mat4 model;          // 64 bytes - model transform matrix
    glm::vec4 objectColor;    // 16 bytes - object tint color (rgba)
    
    PushConstants() : model(1.0f), objectColor(1.0f) {}
};

static_assert(sizeof(PushConstants) == 80, "PushConstants size mismatch");

// ============================================================================
// Lighting UBO (GPU-side, shader-compatible, per-frame data)
// ============================================================================
// This struct must match the layout in the lit shaders exactly.
// Contains data that is constant for the entire frame (view, projection, lights).
// Per-object data (model, color) is now in push constants.
// ============================================================================

struct LightingUBO {
    // View/Projection matrices (64 bytes each = 128 bytes)
    glm::mat4 view;
    glm::mat4 projection;
    
    // Directional light (16 bytes each = 32 bytes)
    glm::vec4 lightDir;       // xyz = normalized direction, w = intensity
    glm::vec4 lightColor;     // rgb = light color, a = unused
    
    // Ambient (16 bytes)
    glm::vec4 ambientColor;   // rgb = ambient color, a = unused
    
    // Camera (16 bytes)
    glm::vec4 cameraPos;      // xyz = world-space camera position, w = unused
    
    // Material properties (16 bytes)
    glm::vec4 material;       // x = shininess, y = specularStrength, z = numPointLights, w = debugMode (1=normals)
    
    // Point lights (32 bytes each = 128 bytes for 4 lights)
    glm::vec4 pointLightPosInt[MAX_POINT_LIGHTS];    // xyz = position, w = intensity
    glm::vec4 pointLightColorRange[MAX_POINT_LIGHTS]; // xyz = color, w = range
    
    // Initialize with sensible defaults
    LightingUBO() {
        view = glm::mat4(1.0f);
        projection = glm::mat4(1.0f);
        lightDir = glm::vec4(glm::normalize(glm::vec3(0.5f, -1.0f, 0.5f)), 1.0f);
        lightColor = glm::vec4(1.0f, 0.98f, 0.95f, 1.0f);
        ambientColor = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        cameraPos = glm::vec4(0.0f, 0.0f, 5.0f, 1.0f);
        material = glm::vec4(32.0f, 0.5f, 0.0f, 0.0f);  // shininess=32, specular=0.5, numLights=0
        
        // Initialize point lights to disabled (intensity = 0)
        for (int i = 0; i < MAX_POINT_LIGHTS; ++i) {
            pointLightPosInt[i] = glm::vec4(0.0f);
            pointLightColorRange[i] = glm::vec4(1.0f, 1.0f, 1.0f, 10.0f);
        }
    }
};

// Verify UBO size at compile time
// 128 (matrices) + 32 (dir light) + 16 (ambient) + 16 (camera) + 16 (material) + 128 (point lights) = 336 bytes
static_assert(sizeof(LightingUBO) == 336, "LightingUBO size mismatch - check alignment");

} // namespace lighting

#endif // LIGHTING_TYPES_H
