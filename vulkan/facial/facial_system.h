// ============================================================================
// FACIAL SYSTEM - DMap-based Facial Animation Manager
// ============================================================================
// Manages DMap textures, slider weights, and shader binding for facial animation.
// Works with VulkanCore to provide GPU-accelerated vertex displacement.
// ============================================================================

#ifndef FACIAL_SYSTEM_H
#define FACIAL_SYSTEM_H

#include "facial_types.h"
#include "../core/vulkan_core.h"

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <unordered_map>

namespace facial {

// ============================================================================
// FacialSystem Class
// ============================================================================

class FacialSystem {
public:
    FacialSystem();
    ~FacialSystem();
    
    // Non-copyable
    FacialSystem(const FacialSystem&) = delete;
    FacialSystem& operator=(const FacialSystem&) = delete;
    
    // ========================================================================
    // Lifecycle
    // ========================================================================
    
    // Initialize with a VulkanCore instance
    bool init(vkcore::VulkanCore* core);
    void shutdown();
    bool isInitialized() const { return m_initialized; }
    
    // ========================================================================
    // DMap Management
    // ========================================================================
    
    // Load a DMap texture from file (RGB displacement map)
    // Returns the DMap index, or -1 on failure
    int loadDMap(const std::string& path, const std::string& name, int sliderIndex);
    
    // Load DMap from raw pixel data (RGB or RGBA)
    int loadDMapFromMemory(const uint8_t* pixels, uint32_t width, uint32_t height,
                          const std::string& name, int sliderIndex, int channels = 3);
    
    // Get DMap info
    const DMapTexture* getDMap(int index) const;
    int getDMapCount() const { return static_cast<int>(m_dmaps.size()); }
    
    // ========================================================================
    // Slider Control
    // ========================================================================
    
    // Set a slider weight (0.0 = neutral, 1.0 = full)
    void setSliderWeight(int index, float weight);
    float getSliderWeight(int index) const;
    
    // Set multiple slider weights at once
    void setSliderWeights(const float* weights, int count);
    
    // Reset all sliders to neutral
    void resetSliders();
    
    // Configure slider properties
    void setSliderRange(int index, float minWeight, float maxWeight);
    void setSliderEnabled(int index, bool enabled);
    
    // ========================================================================
    // Expression Presets
    // ========================================================================
    
    // Register a preset expression
    void registerPreset(const std::string& name, const ExpressionPreset& preset);
    
    // Apply a preset (instantly sets slider weights)
    void applyPreset(const std::string& name);
    
    // Blend toward a preset over time
    void blendToPreset(const std::string& name, float blendFactor);
    
    // ========================================================================
    // Animation
    // ========================================================================
    
    // Load an animation clip
    int loadAnimation(const FacialAnimationClip& clip);
    
    // Play an animation
    void playAnimation(int clipIndex, bool loop = false);
    void stopAnimation();
    
    // Update animation (call every frame with deltaTime)
    void updateAnimation(float deltaTime);
    
    // Check if animation is playing
    bool isAnimationPlaying() const { return m_animationPlaying; }
    
    // ========================================================================
    // Rendering
    // ========================================================================
    
    // Get the DMap pipeline (uses UV1 for displacement sampling)
    vkcore::PipelineHandle getDMapPipeline() const { return m_dmapPipeline; }
    
    // Bind the facial system for rendering
    // Call this before drawing meshes that use DMap displacement
    void bind();
    
    // Set view/projection matrices (call once per frame)
    void setViewMatrix(const glm::mat4& view);
    void setProjectionMatrix(const glm::mat4& proj);
    
    // Set global displacement strength multiplier
    void setGlobalStrength(float strength);
    float getGlobalStrength() const { return m_globalStrength; }
    
    // Draw a mesh with DMap displacement
    // Uses the currently bound DMap textures and slider weights
    void drawMesh(vkcore::MeshHandle mesh, const glm::mat4& model,
                  vkcore::TextureHandle baseTexture = vkcore::INVALID_TEXTURE,
                  const glm::vec4& color = glm::vec4(1.0f));
    
private:
    // Create shader pipeline
    bool createDMapPipeline();
    
    // Create UBO for slider weights
    bool createFacialResources();
    
    // Upload UBO data to GPU
    void updateGPUBuffer();
    
    // ========================================================================
    // State
    // ========================================================================
    
    vkcore::VulkanCore* m_core = nullptr;
    bool m_initialized = false;
    
    // DMap textures
    std::vector<DMapTexture> m_dmaps;
    std::vector<vkcore::TextureHandle> m_dmapHandles;  // GPU texture handles
    
    // Sliders
    std::array<FacialSlider, MAX_SLIDERS> m_sliders;
    float m_globalStrength = 1.0f;
    
    // Presets
    std::unordered_map<std::string, ExpressionPreset> m_presets;
    
    // Animation
    std::vector<FacialAnimationClip> m_animationClips;
    int m_currentClipIndex = -1;
    float m_animationTime = 0.0f;
    bool m_animationPlaying = false;
    bool m_animationLoop = false;
    
    // Matrices
    glm::mat4 m_viewMatrix = glm::mat4(1.0f);
    glm::mat4 m_projMatrix = glm::mat4(1.0f);
    
    // Vulkan resources
    VkPipeline m_pipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;
    VkBuffer m_uboBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_uboMemory = VK_NULL_HANDLE;
    
    // Pipeline handle (for compatibility)
    vkcore::PipelineHandle m_dmapPipeline = vkcore::INVALID_PIPELINE;
    
    // UBO data
    FacialUBO m_uboData;
};

} // namespace facial

// ============================================================================
// C API for HEIDIC
// ============================================================================

#ifdef __cplusplus
extern "C" {
#endif

// Lifecycle
int facial_init(void* vulkanCore);
void facial_shutdown();
int facial_is_initialized();

// DMap loading
int facial_load_dmap(const char* path, const char* name, int sliderIndex);

// Slider control
void facial_set_slider(int index, float weight);
float facial_get_slider(int index);
void facial_reset_sliders();

// Presets
void facial_apply_preset(const char* name);

// Animation
void facial_play_animation(int clipIndex, int loop);
void facial_stop_animation();
void facial_update(float deltaTime);

// Rendering
void facial_bind();
void facial_set_view_matrix(const float* mat4);
void facial_set_projection_matrix(const float* mat4);
void facial_set_global_strength(float strength);
void facial_draw_mesh(unsigned int meshHandle, 
                      float px, float py, float pz,
                      float rx, float ry, float rz,
                      float sx, float sy, float sz,
                      unsigned int baseTexture,
                      float r, float g, float b, float a);

#ifdef __cplusplus
}
#endif

#endif // FACIAL_SYSTEM_H

