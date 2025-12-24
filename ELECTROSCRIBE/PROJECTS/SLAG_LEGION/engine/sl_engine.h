// SLAG LEGION Engine - Modular Game Engine
// Uses only extracted/modular code, no legacy eden_vulkan_helpers.cpp

#ifndef SL_ENGINE_H
#define SL_ENGINE_H

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <string>
#include <vector>
#include <memory>

// Use extracted modules
#include "../../../vulkan/utils/raycast.h"

namespace sl {

// ============================================================================
// Configuration
// ============================================================================

struct EngineConfig {
    int windowWidth = 1920;
    int windowHeight = 1080;
    bool fullscreen = true;
    bool enableValidation = true;
    std::string windowTitle = "SLAG LEGION";
};

// ============================================================================
// World Object (Cube)
// ============================================================================

struct WorldObject {
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 size = glm::vec3(1.0f);
    glm::vec3 color = glm::vec3(1.0f);
    glm::vec3 originalColor = glm::vec3(1.0f);
    float yawDegrees = 0.0f;
    
    // Item properties
    int itemTypeId = 0;
    std::string itemName;
    float tradeValue = 0.0f;
    float condition = 1.0f;
    float weight = 1.0f;
    int category = 0;
    bool isSalvaged = false;
    
    // Attachment
    bool isAttachedToVehicle = false;
    glm::vec3 localOffset = glm::vec3(0.0f);
    
    // Rendering & interaction
    bool isVisible = true;
    bool isStatic = false;      // Static objects don't have gravity
    bool isSelectable = true;   // Can be selected/picked up
    bool isSmallBlock = false;  // Small blocks can snap to big blocks
    int snappedToBigBlock = -1; // Index of big block this is snapped to (-1 = not snapped)
};

// ============================================================================
// Camera
// ============================================================================

struct Camera {
    glm::vec3 position = glm::vec3(0.0f, 2.0f, 5.0f);
    float yaw = 0.0f;      // Horizontal rotation (degrees)
    float pitch = -20.0f;  // Vertical rotation (degrees)
    float fov = 60.0f;
    float nearPlane = 0.1f;
    float farPlane = 50000.0f;
    
    glm::mat4 getViewMatrix() const;
    glm::mat4 getProjectionMatrix(float aspectRatio) const;
    glm::vec3 getForward() const;
    glm::vec3 getRight() const;
};

// ============================================================================
// UI Element
// ============================================================================

struct UIElement {
    enum Type { IMAGE, PANEL, TEXT };
    Type type;
    float x, y, width, height;
    glm::vec4 color = glm::vec4(1.0f);
    float depth = 0.0f;
    bool visible = true;
    std::string texturePath;
    std::string text;
    int fontId = 0;
};

// ============================================================================
// Main Engine Class
// ============================================================================

class Engine {
public:
    Engine();
    ~Engine();
    
    // Initialization
    bool init(const EngineConfig& config = EngineConfig());
    void shutdown();
    
    // Main loop
    bool shouldClose() const;
    void pollEvents();
    void render();
    
    // Window
    GLFWwindow* getWindow() const { return m_window; }
    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }
    
    // Camera
    Camera& getCamera() { return m_camera; }
    const Camera& getCamera() const { return m_camera; }
    
    // Input
    bool isKeyPressed(int key) const;
    glm::vec2 getMousePosition() const;
    glm::vec2 getMouseDelta();
    void hideCursor();
    void showCursor();
    
    // World Objects
    int createObject(const glm::vec3& position, const glm::vec3& size, const glm::vec3& color);
    WorldObject* getObject(int index);
    int getObjectCount() const { return static_cast<int>(m_objects.size()); }
    void setObjectPosition(int index, const glm::vec3& pos);
    void setObjectRotation(int index, float yawDegrees);
    void setObjectColor(int index, const glm::vec3& color);
    void restoreObjectColor(int index);
    
    // Raycasting
    int raycastFromScreen(float screenX, float screenY, glm::vec3& hitPoint);
    int raycastFromCenter(glm::vec3& hitPoint);
    float raycastDownward(const glm::vec3& position);
    
    // Vehicle System
    void attachToVehicle(int objectIndex, int vehicleIndex, const glm::vec3& localOffset);
    void detachFromVehicle(int objectIndex);
    bool isAttachedToVehicle(int objectIndex);
    void updateAttachedObjects(int vehicleIndex);
    
    // UI
    int createUIImage(float x, float y, float w, float h, const std::string& texture);
    int createUIPanel(float x, float y, float w, float h);
    int createUIText(float x, float y, const std::string& text, int fontId, float charW, float charH);
    void setUIVisible(int id, bool visible);
    void setUIColor(int id, const glm::vec4& color);
    void setUIDepth(int id, float depth);
    void setUIText(int id, const std::string& text);
    int loadFont(const std::string& path);
    
    // Timing
    float getDeltaTime() const { return m_deltaTime; }
    void sleep(int milliseconds);
    
    // Debug
    void drawLine(const glm::vec3& start, const glm::vec3& end, const glm::vec3& color);
    
    // UI rendering handled by NEUROSHELL (not in 3D engine)
    
    // Small block snapping
    int findBigBlockUnder(const glm::vec3& point);
    void snapSmallBlockToBigBlock(int smallIdx, int bigIdx);
    void dropSmallBlock(int smallIdx, const glm::vec3& dropPos);

private:
    // Vulkan initialization
    bool initVulkan();
    bool createInstance();
    bool selectPhysicalDevice();
    bool createLogicalDevice();
    bool createSwapchain();
    bool createDepthResources();
    bool createRenderPass();
    bool createDescriptorSetLayout();
    bool createPipeline();
    bool createFallbackPipeline();
    bool createFramebuffers();
    bool createCommandPool();
    bool createVertexBuffer();
    bool createIndexBuffer();
    bool createUniformBuffer();
    bool createDescriptorPool();
    bool createDescriptorSets();
    bool createCommandBuffers();
    bool createSyncObjects();
    
    void cleanupSwapchain();
    void recreateSwapchain();
    void initWorldObjects();
    
    void recordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex);
    void updateUniformBuffer(const WorldObject& obj);
    
    // Helper functions
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    bool createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                      VkBuffer& buffer, VkDeviceMemory& bufferMemory);
    std::vector<char> readShaderFile(const std::string& filename);
    VkShaderModule createShaderModule(const std::vector<char>& code);
    
    // Window
    GLFWwindow* m_window = nullptr;
    int m_width = 1920;
    int m_height = 1080;
    
    // Vulkan handles
    VkInstance m_instance = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    VkQueue m_graphicsQueue = VK_NULL_HANDLE;
    VkQueue m_presentQueue = VK_NULL_HANDLE;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
    VkRenderPass m_renderPass = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
    VkCommandPool m_commandPool = VK_NULL_HANDLE;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;
    
    std::vector<VkImage> m_swapchainImages;
    std::vector<VkImageView> m_swapchainImageViews;
    std::vector<VkFramebuffer> m_framebuffers;
    std::vector<VkCommandBuffer> m_commandBuffers;
    
    VkSemaphore m_imageAvailableSemaphore = VK_NULL_HANDLE;
    VkSemaphore m_renderFinishedSemaphore = VK_NULL_HANDLE;
    VkFence m_inFlightFence = VK_NULL_HANDLE;
    
    uint32_t m_graphicsFamily = 0;
    uint32_t m_presentFamily = 0;
    VkFormat m_swapchainFormat = VK_FORMAT_B8G8R8A8_SRGB;
    VkExtent2D m_swapchainExtent = {0, 0};
    
    // Depth buffer
    VkImage m_depthImage = VK_NULL_HANDLE;
    VkDeviceMemory m_depthImageMemory = VK_NULL_HANDLE;
    VkImageView m_depthImageView = VK_NULL_HANDLE;
    VkFormat m_depthFormat = VK_FORMAT_D32_SFLOAT;
    
    // Cube rendering
    VkBuffer m_cubeVertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_cubeVertexMemory = VK_NULL_HANDLE;
    VkBuffer m_cubeIndexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_cubeIndexMemory = VK_NULL_HANDLE;
    VkBuffer m_uniformBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_uniformMemory = VK_NULL_HANDLE;
    
    // Game state
    Camera m_camera;
    std::vector<WorldObject> m_objects;
    std::vector<UIElement> m_uiElements;
    std::vector<std::pair<glm::vec3, glm::vec3>> m_debugLines;
    std::vector<glm::vec3> m_debugLineColors;
    
    // Input state
    glm::vec2 m_lastMousePos = glm::vec2(0.0f);
    bool m_firstMouse = true;
    
    // Timing
    float m_deltaTime = 0.016f;
    double m_lastFrameTime = 0.0;
    
    // Config
    bool m_validationEnabled = true;
    // Crosshair is handled by NEUROSHELL (2D UI layer), not the 3D engine
};

// ============================================================================
// Utility Functions
// ============================================================================

// Math helpers
inline float degToRad(float degrees) { return degrees * 0.0174532925f; }
inline float radToDeg(float radians) { return radians * 57.2957795f; }

} // namespace sl

#endif // SL_ENGINE_H

