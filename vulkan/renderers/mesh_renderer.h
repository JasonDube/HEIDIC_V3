// EDEN ENGINE - Mesh Renderer
// Extracted from eden_vulkan_helpers.cpp for modularity
// Renders OBJ meshes with textures and hot-reload support

#ifndef EDEN_MESH_RENDERER_H
#define EDEN_MESH_RENDERER_H

#include <vulkan/vulkan.h>
#include <memory>
#include <string>
#include <ctime>
#include <glm/glm.hpp>

// Forward declarations
struct GLFWwindow;
class MeshResource;
class TextureResource;

namespace eden {

// ============================================================================
// Uniform Buffer Object
// ============================================================================

struct MeshUBO {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};

// ============================================================================
// Mesh Renderer Configuration
// ============================================================================

struct MeshRendererConfig {
    std::string vertexShaderPath = "shaders/mesh.vert.spv";
    std::string fragmentShaderPath = "shaders/mesh.frag.spv";
    bool enableWireframe = false;
    bool enableHotReload = true;
    float hotReloadCheckInterval = 0.1f;  // seconds
};

// ============================================================================
// Mesh Renderer Class
// ============================================================================

class MeshRenderer {
public:
    MeshRenderer();
    ~MeshRenderer();
    
    // Non-copyable
    MeshRenderer(const MeshRenderer&) = delete;
    MeshRenderer& operator=(const MeshRenderer&) = delete;
    
    // ========================================================================
    // Lifecycle
    // ========================================================================
    
    // Initialize renderer with Vulkan context
    // Requires: Vulkan device, render pass, command pool already initialized
    bool init(VkDevice device, VkPhysicalDevice physicalDevice,
              VkRenderPass renderPass, VkCommandPool commandPool,
              VkQueue graphicsQueue, const MeshRendererConfig& config = {});
    
    // Cleanup all Vulkan resources
    void cleanup();
    
    // Check if initialized
    bool isInitialized() const { return m_initialized; }
    
    // ========================================================================
    // Mesh Loading
    // ========================================================================
    
    // Load mesh from OBJ file
    bool loadMesh(const std::string& objPath);
    
    // Load mesh from MeshResource
    bool loadMesh(std::unique_ptr<MeshResource> mesh);
    
    // Create mesh from vertex/index data
    bool createMesh(const std::vector<float>& vertices,
                    const std::vector<uint32_t>& indices);
    
    // Get mesh resource (for direct manipulation)
    MeshResource* getMesh() { return m_mesh.get(); }
    const MeshResource* getMesh() const { return m_mesh.get(); }
    
    // ========================================================================
    // Texture Loading
    // ========================================================================
    
    // Load texture from file
    bool loadTexture(const std::string& texturePath);
    
    // Load texture from TextureResource
    bool setTexture(TextureResource* texture);
    
    // Clear texture (use white default)
    void clearTexture();
    
    // ========================================================================
    // Rendering
    // ========================================================================
    
    // Render mesh to command buffer
    // Uses current transform state
    void render(VkCommandBuffer commandBuffer);
    
    // Full render frame (acquire image, render, present)
    void renderFrame(GLFWwindow* window, VkSwapchainKHR swapchain,
                     const std::vector<VkFramebuffer>& framebuffers,
                     VkExtent2D extent);
    
    // ========================================================================
    // Transform
    // ========================================================================
    
    // Set model matrix
    void setModelMatrix(const glm::mat4& model);
    
    // Set view matrix
    void setViewMatrix(const glm::mat4& view);
    
    // Set projection matrix
    void setProjectionMatrix(const glm::mat4& proj);
    
    // Set all matrices at once
    void setMatrices(const glm::mat4& model, const glm::mat4& view, const glm::mat4& proj);
    
    // Get current matrices
    const glm::mat4& getModelMatrix() const { return m_ubo.model; }
    const glm::mat4& getViewMatrix() const { return m_ubo.view; }
    const glm::mat4& getProjectionMatrix() const { return m_ubo.proj; }
    
    // ========================================================================
    // State
    // ========================================================================
    
    // Enable/disable wireframe mode
    void setWireframe(bool enabled);
    bool isWireframe() const { return m_wireframeEnabled; }
    
    // Check for texture hot-reload (call each frame)
    void checkHotReload();
    
    // Get Vulkan pipeline (for external use)
    VkPipeline getPipeline() const { return m_wireframeEnabled ? m_wireframePipeline : m_pipeline; }
    VkPipelineLayout getPipelineLayout() const { return m_pipelineLayout; }
    VkDescriptorSet getDescriptorSet() const { return m_descriptorSet; }
    
private:
    // Vulkan context (not owned)
    VkDevice m_device = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkRenderPass m_renderPass = VK_NULL_HANDLE;
    VkCommandPool m_commandPool = VK_NULL_HANDLE;
    VkQueue m_graphicsQueue = VK_NULL_HANDLE;
    
    // Pipeline resources
    VkPipeline m_pipeline = VK_NULL_HANDLE;
    VkPipeline m_wireframePipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;
    VkShaderModule m_vertShaderModule = VK_NULL_HANDLE;
    VkShaderModule m_fragShaderModule = VK_NULL_HANDLE;
    
    // Uniform buffer
    VkBuffer m_uniformBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_uniformBufferMemory = VK_NULL_HANDLE;
    MeshUBO m_ubo;
    
    // Dummy texture (when no texture is loaded)
    VkImage m_dummyTexture = VK_NULL_HANDLE;
    VkImageView m_dummyTextureView = VK_NULL_HANDLE;
    VkSampler m_dummySampler = VK_NULL_HANDLE;
    VkDeviceMemory m_dummyTextureMemory = VK_NULL_HANDLE;
    
    // Mesh and texture
    std::unique_ptr<MeshResource> m_mesh;
    std::unique_ptr<TextureResource> m_ownedTexture;
    TextureResource* m_texture = nullptr;
    
    // Hot-reload state
    std::string m_texturePath;
    std::time_t m_textureLastModified = 0;
    float m_hotReloadTimer = 0.0f;
    
    // State
    bool m_initialized = false;
    bool m_wireframeEnabled = false;
    MeshRendererConfig m_config;
    
    // Internal helpers
    bool createShaderModules();
    bool createDescriptorSetLayout();
    bool createPipelineLayout();
    bool createPipeline(bool wireframe);
    bool createUniformBuffer();
    bool createDescriptorPool();
    bool allocateDescriptorSet();
    bool createDummyTexture();
    void updateDescriptorSet();
    void updateUniformBuffer();
    
    // Vulkan helpers
    VkCommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(VkCommandBuffer commandBuffer);
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, 
                      VkMemoryPropertyFlags properties,
                      VkBuffer& buffer, VkDeviceMemory& bufferMemory);
    void createImage(uint32_t width, uint32_t height, VkFormat format,
                     VkImageTiling tiling, VkImageUsageFlags usage,
                     VkMemoryPropertyFlags properties,
                     VkImage& image, VkDeviceMemory& imageMemory);
};

} // namespace eden

// ============================================================================
// C API (for HEIDIC integration)
// ============================================================================

#ifdef __cplusplus
extern "C" {
#endif

// Initialize OBJ mesh renderer (path-based)
int heidic_init_renderer_obj_mesh(GLFWwindow* window, const char* objPath, const char* texturePath);

// Initialize OBJ mesh renderer with HEIDIC resources
int heidic_init_renderer_obj_mesh_resource(GLFWwindow* window, void* meshResource, void* textureResource);

// Render OBJ mesh
void heidic_render_obj_mesh(GLFWwindow* window);

// Cleanup OBJ mesh renderer
void heidic_cleanup_renderer_obj_mesh();

#ifdef __cplusplus
}
#endif

#endif // EDEN_MESH_RENDERER_H

