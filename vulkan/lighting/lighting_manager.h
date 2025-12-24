// ============================================================================
// LIGHTING MANAGER - Modular Lighting System for EDEN Engine
// ============================================================================
// A clean, reusable lighting module that works with VulkanCore.
// Provides Blinn-Phong directional lighting with ambient.
//
// Usage (C++):
//   lighting::LightingManager lights;
//   lights.init(&vulkanCore);
//   lights.setDirectionalLight({1, -1, 1}, {1, 1, 1});
//   // In render loop:
//   lights.bind(commandBuffer);
//   lights.updateUBO(model, view, proj, color);
//   // draw mesh...
//
// Usage (HEIDIC):
//   lighting_init(vkcore);
//   lighting_set_directional(1.0, -1.0, 1.0, 1.0, 1.0, 1.0, 1.0);
//   lighting_bind();
//   lighting_update_matrices(...);
// ============================================================================

#ifndef LIGHTING_MANAGER_H
#define LIGHTING_MANAGER_H

#include "lighting_types.h"
#include "../core/vulkan_core.h"

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <unordered_map>

namespace lighting {

class LightingManager {
public:
    LightingManager();
    ~LightingManager();
    
    // Non-copyable
    LightingManager(const LightingManager&) = delete;
    LightingManager& operator=(const LightingManager&) = delete;
    
    // ========================================================================
    // Lifecycle
    // ========================================================================
    
    // Initialize with a VulkanCore instance
    // Creates lit pipeline and UBO resources
    bool init(vkcore::VulkanCore* core);
    void shutdown();
    bool isInitialized() const { return m_initialized; }
    
    // ========================================================================
    // Light Control
    // ========================================================================
    
    // Set directional light (like the sun)
    void setDirectionalLight(const glm::vec3& direction, const glm::vec3& color, float intensity = 1.0f);
    void setDirectionalLight(const DirectionalLight& light);
    
    // Set ambient light (fills shadows)
    void setAmbientLight(const glm::vec3& color);
    void setAmbientLight(const AmbientLight& light);
    
    // Set camera position (needed for specular highlights)
    void setCameraPosition(const glm::vec3& position);
    
    // Set material properties
    void setShininess(float shininess);
    
    // Debug rendering modes
    // 0 = normal lighting, 1 = show normals as RGB colors
    void setDebugMode(int mode);
    void setSpecularStrength(float strength);
    
    // ========================================================================
    // Point Lights
    // ========================================================================
    
    // Set a point light (index 0-3)
    // Returns false if index out of range
    bool setPointLight(int index, const glm::vec3& position, const glm::vec3& color, 
                       float intensity = 1.0f, float range = 10.0f);
    bool setPointLight(int index, const PointLight& light);
    
    // Enable/disable a point light
    void enablePointLight(int index, bool enabled);
    
    // Get point light (returns default if index invalid)
    const PointLight& getPointLight(int index) const;
    
    // Clear all point lights
    void clearPointLights();
    
    // Get number of active point lights
    int getActivePointLightCount() const;
    
    // ========================================================================
    // Getters
    // ========================================================================
    
    const DirectionalLight& getDirectionalLight() const { return m_directionalLight; }
    const AmbientLight& getAmbientLight() const { return m_ambientLight; }
    const glm::vec3& getCameraPosition() const { return m_cameraPos; }
    float getShininess() const { return m_shininess; }
    float getSpecularStrength() const { return m_specularStrength; }
    
    // Get the lit pipeline (use this instead of your own pipeline for lit meshes)
    vkcore::PipelineHandle getLitPipeline() const { return m_litPipeline; }
    
    // ========================================================================
    // Texture Support
    // ========================================================================
    
    // Bind a texture for lit rendering (call before drawLitMesh)
    // Pass INVALID_TEXTURE to use the default white texture
    void bindTexture(vkcore::TextureHandle texture);
    
    // ========================================================================
    // Rendering
    // ========================================================================
    
    // Bind the lit pipeline and descriptor set for rendering
    // Call this before drawing lit meshes
    void bind();
    
    // Update the UBO with current transforms and draw a mesh
    // This handles the full UBO update including lighting data
    void drawLitMesh(vkcore::MeshHandle mesh, 
                     const glm::mat4& model,
                     const glm::vec4& color = glm::vec4(1.0f));
    
    // Update matrices (call once per frame before drawing)
    void setViewMatrix(const glm::mat4& view);
    void setProjectionMatrix(const glm::mat4& proj);
    
private:
    // Create the lit shader pipeline
    bool createLitPipeline();
    
    // Create UBO and descriptor set for lighting
    bool createLightingResources();
    
    // Update the GPU UBO
    void updateGPUBuffer();
    
    // Get or create a descriptor set for a given texture
    // Each texture gets its own descriptor set to prevent sharing
    VkDescriptorSet getOrCreateDescriptorSet(vkcore::TextureHandle texture);
    
    // ========================================================================
    // State
    // ========================================================================
    
    vkcore::VulkanCore* m_core = nullptr;
    bool m_initialized = false;
    
    // Lighting state
    DirectionalLight m_directionalLight;
    AmbientLight m_ambientLight;
    PointLight m_pointLights[MAX_POINT_LIGHTS];
    glm::vec3 m_cameraPos = glm::vec3(0.0f, 0.0f, 5.0f);
    float m_shininess = 32.0f;
    float m_specularStrength = 0.5f;
    int m_debugMode = 0;  // 0 = normal, 1 = show normals as RGB
    
    // Matrices
    glm::mat4 m_viewMatrix = glm::mat4(1.0f);
    glm::mat4 m_projMatrix = glm::mat4(1.0f);
    
    // Vulkan resources (managed independently for modularity)
    VkPipeline m_pipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    VkBuffer m_uboBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_uboMemory = VK_NULL_HANDLE;
    
    // Default white texture (for when no texture is bound)
    VkImage m_defaultTexImage = VK_NULL_HANDLE;
    VkDeviceMemory m_defaultTexMemory = VK_NULL_HANDLE;
    VkImageView m_defaultTexView = VK_NULL_HANDLE;
    VkSampler m_defaultSampler = VK_NULL_HANDLE;
    
    // Per-texture descriptor sets (one set per unique texture)
    // This prevents texture sharing by giving each texture its own descriptor set
    std::unordered_map<vkcore::TextureHandle, VkDescriptorSet> m_textureDescriptorSets;
    vkcore::TextureHandle m_defaultTextureHandle = vkcore::INVALID_TEXTURE; // Special handle for default texture
    
    // Currently bound descriptor set (for drawing)
    VkDescriptorSet m_currentDescriptorSet = VK_NULL_HANDLE;
    
    // Legacy handle (for compatibility)
    vkcore::PipelineHandle m_litPipeline = vkcore::INVALID_PIPELINE;
    
    // The actual UBO data
    LightingUBO m_uboData;
};

} // namespace lighting

// ============================================================================
// C API for HEIDIC
// ============================================================================

#ifdef __cplusplus
extern "C" {
#endif

// Lifecycle
int lighting_init(void* vulkanCore);
void lighting_shutdown();
int lighting_is_initialized();

// Light control
void lighting_set_directional(float dx, float dy, float dz, 
                               float r, float g, float b, float intensity);
void lighting_set_ambient(float r, float g, float b);
void lighting_set_camera_pos(float x, float y, float z);
void lighting_set_shininess(float shininess);
void lighting_set_specular_strength(float strength);

// Matrices
void lighting_set_view_matrix(const float* mat4);
void lighting_set_projection_matrix(const float* mat4);

// Texture
void lighting_bind_texture(unsigned int textureHandle);

// Point lights
int lighting_set_point_light(int index, float px, float py, float pz,
                              float r, float g, float b, 
                              float intensity, float range);
void lighting_enable_point_light(int index, int enabled);
void lighting_clear_point_lights();
int lighting_get_active_point_light_count();

// Rendering
void lighting_bind();
void lighting_draw_mesh(unsigned int meshHandle,
                        float px, float py, float pz,
                        float rx, float ry, float rz,
                        float sx, float sy, float sz,
                        float r, float g, float b, float a);

// Get pipeline handle (for manual binding)
unsigned int lighting_get_pipeline();

#ifdef __cplusplus
}
#endif

#endif // LIGHTING_MANAGER_H

