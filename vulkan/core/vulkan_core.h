// ============================================================================
// VULKAN CORE - Shared Rendering Foundation
// ============================================================================
// The ONE place for Vulkan boilerplate. All projects use this.
// No more copy-pasting 800 lines of init code!
//
// Usage:
//   VulkanCore core;
//   core.init(window, config);
//   while (running) {
//       core.beginFrame();
//       core.drawMesh(meshId, transform);
//       core.endFrame();
//   }
//   core.shutdown();
// ============================================================================

#ifndef VULKAN_CORE_H
#define VULKAN_CORE_H

// Support both USE_IMGUI (build system) and VKCORE_ENABLE_IMGUI (explicit)
#if defined(USE_IMGUI) && !defined(VKCORE_ENABLE_IMGUI)
#define VKCORE_ENABLE_IMGUI
#endif

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <string>
#include <vector>
#include <functional>
#include <memory>

namespace vkcore {

// ============================================================================
// Configuration
// ============================================================================

struct CoreConfig {
    std::string appName = "VulkanCore App";
    int width = 1280;
    int height = 720;
    bool enableValidation = false;
    bool vsync = true;
    float clearColor[4] = {0.1f, 0.1f, 0.12f, 1.0f};
};

// ============================================================================
// Vertex Formats (configurable per-pipeline)
// ============================================================================

enum class VertexFormat {
    POSITION_COLOR,           // vec3 pos, vec3 color (cubes, debug)
    POSITION_NORMAL_UV,       // vec3 pos, vec3 normal, vec2 uv (meshes)
    POSITION_NORMAL_UV_COLOR, // vec3 pos, vec3 normal, vec2 uv, vec3 color
    POSITION_NORMAL_UV0_UV1,  // vec3 pos, vec3 normal, vec2 uv0, vec2 uv1 (facial DMap)
    CUSTOM                    // User provides attribute descriptions
};

struct VertexAttribute {
    uint32_t location;
    VkFormat format;
    uint32_t offset;
};

struct VertexLayout {
    uint32_t stride;
    std::vector<VertexAttribute> attributes;
};

// ============================================================================
// Pipeline Configuration
// ============================================================================

struct PipelineConfig {
    std::string vertexShaderPath;
    std::string fragmentShaderPath;
    VertexFormat vertexFormat = VertexFormat::POSITION_COLOR;
    VertexLayout customLayout;  // Only used if vertexFormat == CUSTOM
    VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL;
    VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT;
    bool depthTest = true;
    bool depthWrite = true;
    bool alphaBlend = false;
};

// ============================================================================
// Resource Handles
// ============================================================================

using PipelineHandle = uint32_t;
using BufferHandle = uint32_t;
using TextureHandle = uint32_t;
using MeshHandle = uint32_t;

constexpr PipelineHandle INVALID_PIPELINE = UINT32_MAX;
constexpr BufferHandle INVALID_BUFFER = UINT32_MAX;
constexpr TextureHandle INVALID_TEXTURE = UINT32_MAX;
constexpr MeshHandle INVALID_MESH = UINT32_MAX;

// ============================================================================
// Uniform Buffer Object (standard MVP)
// ============================================================================

struct StandardUBO {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 projection;
    glm::vec4 color;  // Optional object color
};

// ============================================================================
// Mesh Resource
// ============================================================================

struct MeshData {
    std::vector<float> vertices;  // Interleaved vertex data
    std::vector<uint32_t> indices;
    VertexFormat format;
    uint32_t vertexCount;
    uint32_t indexCount;
};

// ============================================================================
// VULKAN CORE CLASS
// ============================================================================

class VulkanCore {
public:
    VulkanCore();
    ~VulkanCore();
    
    // Non-copyable
    VulkanCore(const VulkanCore&) = delete;
    VulkanCore& operator=(const VulkanCore&) = delete;
    
    // ========================================================================
    // Lifecycle
    // ========================================================================
    
    bool init(GLFWwindow* window, const CoreConfig& config = CoreConfig());
    void shutdown();
    bool isInitialized() const { return m_initialized; }
    
    // ========================================================================
    // Frame Management
    // ========================================================================
    
    bool beginFrame();  // Returns false if should skip frame (resize, etc)
    void endFrame();
    
    // ========================================================================
    // Pipeline Management
    // ========================================================================
    
    PipelineHandle createPipeline(const PipelineConfig& config);
    void bindPipeline(PipelineHandle handle);
    void destroyPipeline(PipelineHandle handle);
    
    // ========================================================================
    // Buffer Management
    // ========================================================================
    
    BufferHandle createVertexBuffer(const void* data, size_t size);
    BufferHandle createIndexBuffer(const uint32_t* data, size_t count);
    BufferHandle createUniformBuffer(size_t size);
    void updateUniformBuffer(BufferHandle handle, const void* data, size_t size);
    void destroyBuffer(BufferHandle handle);
    
    // ========================================================================
    // Texture Management
    // ========================================================================
    
    TextureHandle loadTexture(const std::string& path);
    TextureHandle createTexture(const uint8_t* pixels, uint32_t width, uint32_t height, uint32_t channels = 4);
    TextureHandle createTextureLinear(const uint8_t* pixels, uint32_t width, uint32_t height, uint32_t channels = 4);  // Creates texture with linear format (no SRGB) - for DMap textures
    void bindTexture(TextureHandle handle);  // Binds for next draw calls
    void destroyTexture(TextureHandle handle);
    TextureHandle getDefaultTexture() const { return m_defaultTexture; }
    
    // ========================================================================
    // Mesh Management (high-level)
    // ========================================================================
    
    MeshHandle createMesh(const MeshData& data);
    MeshHandle createCube(float size = 1.0f, const glm::vec3& color = glm::vec3(1.0f));
    void destroyMesh(MeshHandle handle);
    
    // Get mesh buffer info for external rendering (e.g., LightingManager)
    bool getMeshBuffers(MeshHandle mesh, VkBuffer& vertexBuffer, VkBuffer& indexBuffer, uint32_t& indexCount) const;
    
    // Get texture info for external rendering (e.g., LightingManager)
    bool getTextureInfo(TextureHandle tex, VkImageView& view, VkSampler& sampler) const;
    
    // ========================================================================
    // Drawing
    // ========================================================================
    
    void drawMesh(MeshHandle mesh, const glm::mat4& transform, const glm::vec4& color = glm::vec4(1.0f));
    void drawVertices(BufferHandle vertexBuffer, uint32_t vertexCount);
    void drawIndexed(BufferHandle vertexBuffer, BufferHandle indexBuffer, uint32_t indexCount);
    
    // ========================================================================
    // Camera / View
    // ========================================================================
    
    void setViewMatrix(const glm::mat4& view);
    void setProjectionMatrix(const glm::mat4& proj);
    void setCamera(const glm::vec3& position, const glm::vec3& target, const glm::vec3& up = glm::vec3(0, 1, 0));
    void setPerspective(float fovDegrees, float nearPlane, float farPlane);
    
    const glm::mat4& getViewMatrix() const { return m_viewMatrix; }
    const glm::mat4& getProjectionMatrix() const { return m_projMatrix; }
    
    // ========================================================================
    // Accessors (for advanced use / extension)
    // ========================================================================
    
    VkDevice getDevice() const { return m_device; }
    VkPhysicalDevice getPhysicalDevice() const { return m_physicalDevice; }
    VkRenderPass getRenderPass() const { return m_renderPass; }
    VkCommandBuffer getCurrentCommandBuffer() const { return m_commandBuffers[m_currentFrame]; }
    VkDescriptorSetLayout getDescriptorSetLayout() const { return m_descriptorSetLayout; }
    VkDescriptorPool getDescriptorPool() const { return m_descriptorPool; }
    
    uint32_t getWidth() const { return m_swapchainExtent.width; }
    
    // Background color
    void setClearColor(float r, float g, float b, float a = 1.0f) {
        m_config.clearColor[0] = r;
        m_config.clearColor[1] = g;
        m_config.clearColor[2] = b;
        m_config.clearColor[3] = a;
    }
    uint32_t getHeight() const { return m_swapchainExtent.height; }
    float getAspectRatio() const { return (float)m_swapchainExtent.width / (float)m_swapchainExtent.height; }
    
    // Queue access for extensions (ImGui, etc)
    VkQueue getGraphicsQueue() const { return m_graphicsQueue; }
    uint32_t getGraphicsFamily() const { return m_graphicsFamily; }
    VkCommandPool getCommandPool() const { return m_commandPool; }
    VkInstance getInstance() const { return m_instance; }
    uint32_t getImageCount() const { return static_cast<uint32_t>(m_swapchainImages.size()); }
    VkFormat getSwapchainFormat() const { return m_swapchainFormat; }
    
    // ========================================================================
    // ImGui Integration (optional)
    // ========================================================================
    
    // Call these if you want to use ImGui with VulkanCore
    bool initImGui(GLFWwindow* window);
    void shutdownImGui();
    void beginImGuiFrame();
    void renderImGui();  // Call between beginFrame() and endFrame()
    bool isImGuiInitialized() const { return m_imguiInitialized; }

private:
    // ========================================================================
    // Vulkan Initialization
    // ========================================================================
    
    bool createInstance();
    bool createSurface(GLFWwindow* window);
    bool selectPhysicalDevice();
    bool createLogicalDevice();
    bool createSwapchain();
    bool createDepthResources();
    bool createRenderPass();
    bool createDescriptorSetLayout();
    bool createDescriptorPool();
    bool createFramebuffers();
    bool createCommandPool();
    bool createCommandBuffers();
    bool createSyncObjects();
    bool createDefaultResources();
    
    void cleanupSwapchain();
    void recreateSwapchain();
    
    // ========================================================================
    // Helpers
    // ========================================================================
    
    VkShaderModule createShaderModule(const std::vector<char>& code);
    std::vector<char> readShaderFile(const std::string& path);
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    VkCommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(VkCommandBuffer cmd);
    bool createBufferInternal(VkDeviceSize size, VkBufferUsageFlags usage,
                              VkMemoryPropertyFlags properties,
                              VkBuffer& buffer, VkDeviceMemory& memory);
    VkVertexInputBindingDescription getBindingDescription(VertexFormat format);
    std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions(VertexFormat format);
    
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
    VkFormat m_swapchainFormat;
    VkExtent2D m_swapchainExtent;
    
    VkRenderPass m_renderPass = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> m_framebuffers;
    
    VkImage m_depthImage = VK_NULL_HANDLE;
    VkDeviceMemory m_depthImageMemory = VK_NULL_HANDLE;
    VkImageView m_depthImageView = VK_NULL_HANDLE;
    
    VkCommandPool m_commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> m_commandBuffers;
    
    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    
    // Sync objects
    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;
    std::vector<VkSemaphore> m_imageAvailableSemaphores;
    std::vector<VkSemaphore> m_renderFinishedSemaphores;
    std::vector<VkFence> m_inFlightFences;
    uint32_t m_currentFrame = 0;
    uint32_t m_imageIndex = 0;
    
    uint32_t m_graphicsFamily = 0;
    uint32_t m_presentFamily = 0;
    
    // ========================================================================
    // Resource Storage
    // ========================================================================
    
    struct PipelineResource {
        VkPipeline pipeline = VK_NULL_HANDLE;
        VkPipelineLayout layout = VK_NULL_HANDLE;
        bool valid = false;
    };
    
    struct BufferResource {
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkDeviceSize size = 0;
        bool valid = false;
    };
    
    struct MeshResource {
        BufferHandle vertexBuffer = INVALID_BUFFER;
        BufferHandle indexBuffer = INVALID_BUFFER;
        uint32_t indexCount = 0;
        bool valid = false;
    };
    
    struct TextureResource {
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
        VkSampler sampler = VK_NULL_HANDLE;
        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
        uint32_t width = 0;
        uint32_t height = 0;
        bool valid = false;
    };
    
    std::vector<PipelineResource> m_pipelines;
    std::vector<BufferResource> m_buffers;
    std::vector<MeshResource> m_meshes;
    std::vector<TextureResource> m_textures;
    TextureHandle m_defaultTexture = INVALID_TEXTURE;
    TextureHandle m_currentTexture = INVALID_TEXTURE;
    
    // Current bound pipeline
    PipelineHandle m_currentPipeline = INVALID_PIPELINE;
    
    // Default pipeline for simple rendering
    PipelineHandle m_defaultPipeline = INVALID_PIPELINE;
    
    // Per-object uniform buffer and descriptor set
    BufferHandle m_objectUBO = INVALID_BUFFER;
    VkDescriptorSet m_objectDescriptorSet = VK_NULL_HANDLE;
    
    // ========================================================================
    // State
    // ========================================================================
    
    GLFWwindow* m_window = nullptr;
    CoreConfig m_config;
    bool m_initialized = false;
    bool m_frameStarted = false;
    bool m_framebufferResized = false;
    
    glm::mat4 m_viewMatrix = glm::mat4(1.0f);
    glm::mat4 m_projMatrix = glm::mat4(1.0f);
    
    // ImGui state
    bool m_imguiInitialized = false;
    VkDescriptorPool m_imguiDescriptorPool = VK_NULL_HANDLE;
};

} // namespace vkcore

// ============================================================================
// C API for HEIDIC
// ============================================================================

#ifdef __cplusplus
extern "C" {
#endif

// Lifecycle
int vkcore_init(void* glfwWindow, const char* appName, int width, int height);
void vkcore_shutdown();
int vkcore_is_initialized();

// Frame
int vkcore_begin_frame();
void vkcore_end_frame();

// Pipeline
unsigned int vkcore_create_pipeline(const char* vertShader, const char* fragShader, int vertexFormat);
void vkcore_bind_pipeline(unsigned int handle);
void vkcore_destroy_pipeline(unsigned int handle);

// Mesh
unsigned int vkcore_create_cube(float size, float r, float g, float b);
unsigned int vkcore_create_mesh(const float* vertices, int vertexCount, 
                                 const unsigned int* indices, int indexCount, int vertexFormat);
void vkcore_destroy_mesh(unsigned int handle);

// Drawing
void vkcore_draw_mesh(unsigned int meshHandle, 
                      float px, float py, float pz,
                      float rx, float ry, float rz,
                      float sx, float sy, float sz,
                      float r, float g, float b, float a);

// Camera
void vkcore_set_camera(float eyeX, float eyeY, float eyeZ,
                       float targetX, float targetY, float targetZ);
void vkcore_set_perspective(float fovDegrees, float nearPlane, float farPlane);
void vkcore_set_view_matrix(const float* mat4);
void vkcore_set_projection_matrix(const float* mat4);

// Info
int vkcore_get_width();
int vkcore_get_height();
float vkcore_get_aspect_ratio();

// ImGui (optional - compile with VKCORE_ENABLE_IMGUI)
int vkcore_init_imgui(void* glfwWindow);
void vkcore_shutdown_imgui();
void vkcore_begin_imgui_frame();
void vkcore_render_imgui();
int vkcore_is_imgui_initialized();

#ifdef __cplusplus
}
#endif

#endif // VULKAN_CORE_H

