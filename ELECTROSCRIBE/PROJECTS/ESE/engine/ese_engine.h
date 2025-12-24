// ESE Engine - Modular Vulkan Engine for Echo Synapse Editor
// Uses only refactored modules (no eden_vulkan_helpers.cpp)

#ifndef ESE_ENGINE_H
#define ESE_ENGINE_H

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vector>
#include <string>
#include <memory>
#include <array>

// Forward declarations
class MeshResource;
class TextureResource;

namespace ese {

// ============================================================================
// Vertex Structure
// ============================================================================

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoord;
    
    static VkVertexInputBindingDescription getBindingDescription();
    static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions();
};

// ============================================================================
// Uniform Buffer Object
// ============================================================================

struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};

// ============================================================================
// Push Constants (for per-object data)
// ============================================================================

struct PushConstants {
    glm::vec4 color;
    int useTexture;  // 1 = use texture, 0 = use color
    int padding[3];
};

// ============================================================================
// ESE Engine Class
// ============================================================================

class Engine {
public:
    Engine();
    ~Engine();
    
    // Non-copyable
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;
    
    // ========================================================================
    // Lifecycle
    // ========================================================================
    
    bool init(GLFWwindow* window);
    void cleanup();
    bool isInitialized() const { return m_initialized; }
    
    // ========================================================================
    // Rendering
    // ========================================================================
    
    void beginFrame();
    void endFrame();
    void render(GLFWwindow* window);
    
    // Record mesh drawing commands
    void drawMesh(VkCommandBuffer cmd);
    void drawWireframe(VkCommandBuffer cmd);
    void drawGrid(VkCommandBuffer cmd);
    void drawGizmo(VkCommandBuffer cmd, const glm::vec3& position);
    
    // ========================================================================
    // Mesh Operations
    // ========================================================================
    
    bool loadMesh(const std::string& objPath);
    bool loadTexture(const std::string& texturePath);
    bool createCube();
    
    // Get/Set mesh data
    MeshResource* getMesh() { return m_mesh.get(); }
    void updateMeshBuffers();  // Call after modifying mesh vertices
    
    // Get mesh info (without exposing incomplete type)
    int getVertexCount() const;
    int getTriangleCount() const;
    
    // ========================================================================
    // Camera / Matrices
    // ========================================================================
    
    void setModelMatrix(const glm::mat4& model);
    void setViewMatrix(const glm::mat4& view);
    void setProjectionMatrix(const glm::mat4& proj);
    void updateMatrices();
    
    const glm::mat4& getViewMatrix() const { return m_ubo.view; }
    const glm::mat4& getProjectionMatrix() const { return m_ubo.proj; }
    
    // ========================================================================
    // State
    // ========================================================================
    
    void setWireframeMode(bool enabled) { m_wireframeMode = enabled; }
    bool isWireframeMode() const { return m_wireframeMode; }
    
    // ========================================================================
    // Vulkan Getters (for modules that need direct access)
    // ========================================================================
    
    VkDevice getDevice() const { return m_device; }
    VkPhysicalDevice getPhysicalDevice() const { return m_physicalDevice; }
    VkRenderPass getRenderPass() const { return m_renderPass; }
    VkCommandPool getCommandPool() const { return m_commandPool; }
    VkQueue getGraphicsQueue() const { return m_graphicsQueue; }
    VkCommandBuffer getCurrentCommandBuffer() const { return m_commandBuffers[m_currentFrame]; }
    VkExtent2D getSwapchainExtent() const { return m_swapchainExtent; }
    
    // Window size
    int getWidth() const { return m_swapchainExtent.width; }
    int getHeight() const { return m_swapchainExtent.height; }

private:
    // ========================================================================
    // Vulkan Core
    // ========================================================================
    
    bool createInstance();
    bool createSurface(GLFWwindow* window);
    bool pickPhysicalDevice();
    bool createLogicalDevice();
    bool createSwapchain(GLFWwindow* window);
    bool createRenderPass();
    bool createFramebuffers();
    bool createCommandPool();
    bool createCommandBuffers();
    bool createSyncObjects();
    bool createDescriptorSetLayout();
    bool createPipeline();
    bool createWireframePipeline();
    bool createUniformBuffer();
    bool createDescriptorPool();
    bool createDescriptorSet();
    bool createDummyTexture();
    bool createVertexBuffer();
    bool createIndexBuffer();
    
    void cleanupSwapchain();
    void recreateSwapchain(GLFWwindow* window);
    
    // ========================================================================
    // Helper Functions
    // ========================================================================
    
    VkShaderModule createShaderModule(const std::vector<char>& code);
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                      VkMemoryPropertyFlags properties,
                      VkBuffer& buffer, VkDeviceMemory& bufferMemory);
    VkCommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(VkCommandBuffer commandBuffer);
    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
    
    // ========================================================================
    // Vulkan Objects
    // ========================================================================
    
    VkInstance m_instance = VK_NULL_HANDLE;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    VkQueue m_graphicsQueue = VK_NULL_HANDLE;
    VkQueue m_presentQueue = VK_NULL_HANDLE;
    
    VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
    std::vector<VkImage> m_swapchainImages;
    std::vector<VkImageView> m_swapchainImageViews;
    VkFormat m_swapchainImageFormat;
    VkExtent2D m_swapchainExtent;
    
    VkRenderPass m_renderPass = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> m_framebuffers;
    
    VkCommandPool m_commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> m_commandBuffers;
    
    // Depth buffer
    VkImage m_depthImage = VK_NULL_HANDLE;
    VkDeviceMemory m_depthImageMemory = VK_NULL_HANDLE;
    VkImageView m_depthImageView = VK_NULL_HANDLE;
    
    // Synchronization
    std::vector<VkSemaphore> m_imageAvailableSemaphores;
    std::vector<VkSemaphore> m_renderFinishedSemaphores;
    std::vector<VkFence> m_inFlightFences;
    uint32_t m_currentFrame = 0;
    uint32_t m_imageIndex = 0;
    static const int MAX_FRAMES_IN_FLIGHT = 2;
    
    // Pipeline
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
    VkPipeline m_wireframePipeline = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;
    
    // Uniform buffer
    VkBuffer m_uniformBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_uniformBufferMemory = VK_NULL_HANDLE;
    void* m_uniformBufferMapped = nullptr;
    UniformBufferObject m_ubo;
    
    // Vertex/Index buffers
    VkBuffer m_vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_vertexBufferMemory = VK_NULL_HANDLE;
    VkBuffer m_indexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_indexBufferMemory = VK_NULL_HANDLE;
    uint32_t m_indexCount = 0;
    
    // Dummy texture
    VkImage m_dummyTexture = VK_NULL_HANDLE;
    VkDeviceMemory m_dummyTextureMemory = VK_NULL_HANDLE;
    VkImageView m_dummyTextureView = VK_NULL_HANDLE;
    VkSampler m_dummySampler = VK_NULL_HANDLE;
    
    // Mesh and texture
    std::unique_ptr<MeshResource> m_mesh;
    std::unique_ptr<TextureResource> m_texture;
    
    // Queue family indices
    uint32_t m_graphicsFamily = 0;
    uint32_t m_presentFamily = 0;
    
    // State
    bool m_initialized = false;
    bool m_wireframeMode = false;
    bool m_framebufferResized = false;
    bool m_frameInProgress = false;
    
    // Shader paths
    std::string m_shaderPath = "shaders/";
};

} // namespace ese

// ============================================================================
// C API for HEIDIC
// ============================================================================

#ifdef __cplusplus
extern "C" {
#endif

// Lifecycle
int ese_init(GLFWwindow* window);
void ese_cleanup();
void ese_render(GLFWwindow* window);

// Mesh operations  
int ese_load_mesh(const char* path);
int ese_load_texture(const char* path);
int ese_create_cube();

// Camera
void ese_set_view_matrix(float* mat4);
void ese_set_projection_matrix(float* mat4);

// State
void ese_set_wireframe(int enabled);

#ifdef __cplusplus
}
#endif

#endif // ESE_ENGINE_H

