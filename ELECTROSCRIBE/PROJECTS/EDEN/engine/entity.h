// ============================================================================
// ENTITY - Core entity type for EDEN Level Editor
// ============================================================================
// Entities can be models (meshes with textures) or lights (point/directional).
// Each entity has a unique ID, name, and transform properties.
// ============================================================================

#ifndef EDEN_ENTITY_H
#define EDEN_ENTITY_H

#include "../../../../vulkan/core/vulkan_core.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <string>

namespace eden {

// ============================================================================
// Entity Types
// ============================================================================

enum class EntityType {
    MODEL,              // 3D mesh with texture
    POINT_LIGHT,        // Point light source
    DIRECTIONAL_LIGHT   // Directional light (sun)
};

// ============================================================================
// Entity Structure
// ============================================================================

struct Entity {
    // Identity
    uint32_t id = 0;
    std::string name;
    EntityType type = EntityType::MODEL;
    
    // Transform (applies to all entity types)
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 rotation = glm::vec3(0.0f);  // Euler angles in degrees
    glm::vec3 scale = glm::vec3(1.0f);
    
    // Model data (only used if type == MODEL)
    vkcore::MeshHandle mesh = vkcore::INVALID_MESH;
    vkcore::TextureHandle texture = vkcore::INVALID_TEXTURE;
    std::string modelPath;
    glm::vec4 color = glm::vec4(1.0f);  // Tint color
    
    // Light data (only used if type == POINT_LIGHT or DIRECTIONAL_LIGHT)
    glm::vec3 lightColor = glm::vec3(1.0f);
    float intensity = 1.0f;
    float range = 10.0f;  // Only for point lights
    
    // ========================================================================
    // Helper Methods
    // ========================================================================
    
    // Get the model matrix for this entity's transform
    glm::mat4 getModelMatrix() const {
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, position);
        model = glm::rotate(model, glm::radians(rotation.x), glm::vec3(1, 0, 0));
        model = glm::rotate(model, glm::radians(rotation.y), glm::vec3(0, 1, 0));
        model = glm::rotate(model, glm::radians(rotation.z), glm::vec3(0, 0, 1));
        model = glm::scale(model, scale);
        return model;
    }
    
    // Get light direction (for directional lights, uses rotation)
    glm::vec3 getLightDirection() const {
        // Default direction is down (-Y)
        glm::vec3 dir = glm::vec3(0.0f, -1.0f, 0.0f);
        // Apply rotation
        glm::mat4 rot = glm::mat4(1.0f);
        rot = glm::rotate(rot, glm::radians(rotation.x), glm::vec3(1, 0, 0));
        rot = glm::rotate(rot, glm::radians(rotation.y), glm::vec3(0, 1, 0));
        rot = glm::rotate(rot, glm::radians(rotation.z), glm::vec3(0, 0, 1));
        return glm::normalize(glm::vec3(rot * glm::vec4(dir, 0.0f)));
    }
    
    // Check if this is a light entity
    bool isLight() const {
        return type == EntityType::POINT_LIGHT || type == EntityType::DIRECTIONAL_LIGHT;
    }
    
    // Check if this is a model entity
    bool isModel() const {
        return type == EntityType::MODEL;
    }
    
    // Get type as string (for display)
    const char* getTypeName() const {
        switch (type) {
            case EntityType::MODEL: return "Model";
            case EntityType::POINT_LIGHT: return "Point Light";
            case EntityType::DIRECTIONAL_LIGHT: return "Directional Light";
            default: return "Unknown";
        }
    }
};

// ============================================================================
// Entity ID Generator
// ============================================================================

inline uint32_t generateEntityId() {
    static uint32_t nextId = 1;
    return nextId++;
}

} // namespace eden

#endif // EDEN_ENTITY_H

