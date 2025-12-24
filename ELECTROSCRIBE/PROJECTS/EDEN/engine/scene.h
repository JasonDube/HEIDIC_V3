// ============================================================================
// SCENE - Scene management for EDEN Level Editor
// ============================================================================
// Manages a collection of entities (models and lights).
// Handles save/load to JSON format.
// ============================================================================

#ifndef EDEN_SCENE_H
#define EDEN_SCENE_H

#include "entity.h"
#include "../../../../vulkan/core/vulkan_core.h"
#include "../../../../vulkan/utils/glb_loader.h"

#include <vector>
#include <string>
#include <functional>

namespace eden {

// ============================================================================
// Scene Class
// ============================================================================

class Scene {
public:
    Scene();
    ~Scene();
    
    // ========================================================================
    // Entity Management
    // ========================================================================
    
    // Add a model to the scene (loads from file)
    // Returns entity ID, or 0 on failure
    uint32_t addModel(vkcore::VulkanCore* core, const std::string& modelPath, 
                      const glm::vec3& position = glm::vec3(0.0f));
    
    // Add a point light to the scene
    uint32_t addPointLight(const glm::vec3& position, 
                           const glm::vec3& color = glm::vec3(1.0f),
                           float intensity = 1.0f, float range = 10.0f);
    
    // Add a directional light to the scene
    uint32_t addDirectionalLight(const glm::vec3& direction = glm::vec3(0.5f, -1.0f, 0.5f),
                                  const glm::vec3& color = glm::vec3(1.0f),
                                  float intensity = 1.0f);
    
    // Remove an entity by ID
    bool removeEntity(uint32_t id);
    
    // Get entity by ID (returns nullptr if not found)
    Entity* getEntity(uint32_t id);
    const Entity* getEntity(uint32_t id) const;
    
    // Get all entities
    std::vector<Entity>& getEntities() { return m_entities; }
    const std::vector<Entity>& getEntities() const { return m_entities; }
    
    // ========================================================================
    // Selection
    // ========================================================================
    
    void selectEntity(uint32_t id);
    void clearSelection();
    uint32_t getSelectedId() const { return m_selectedId; }
    Entity* getSelectedEntity();
    const Entity* getSelectedEntity() const;
    bool hasSelection() const { return m_selectedId != 0; }
    
    // ========================================================================
    // Scene State
    // ========================================================================
    
    void clear(vkcore::VulkanCore* core);  // Clear all entities
    bool isEmpty() const { return m_entities.empty(); }
    size_t getEntityCount() const { return m_entities.size(); }
    
    // File path for current scene
    const std::string& getFilePath() const { return m_filePath; }
    void setFilePath(const std::string& path) { m_filePath = path; }
    bool hasFilePath() const { return !m_filePath.empty(); }
    
    // Dirty flag (scene has unsaved changes)
    bool isDirty() const { return m_dirty; }
    void setDirty(bool dirty) { m_dirty = dirty; }
    
    // ========================================================================
    // Save/Load (implemented in scene.cpp)
    // ========================================================================
    
    bool saveToJSON(const std::string& path);
    bool loadFromJSON(vkcore::VulkanCore* core, const std::string& path);
    
private:
    std::vector<Entity> m_entities;
    uint32_t m_selectedId = 0;
    std::string m_filePath;
    bool m_dirty = false;
    
    // Helper to find entity index by ID
    int findEntityIndex(uint32_t id) const;
};

} // namespace eden

#endif // EDEN_SCENE_H

