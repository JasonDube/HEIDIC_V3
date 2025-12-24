// ============================================================================
// SLAG LEGION Engine v2 - Built on VulkanCore
// ============================================================================
// This is the NEW engine that uses VulkanCore for all Vulkan boilerplate.
// Game-specific logic only - no Vulkan init code!
// 
// Before: ~1500 lines (800 Vulkan boilerplate + 700 game logic)
// After:  ~400 lines (just game logic)
// ============================================================================

#ifndef SL_ENGINE_V2_H
#define SL_ENGINE_V2_H

#include "../../../../vulkan/core/vulkan_core.h"
#include "../../../../vulkan/utils/raycast.h"

#include <string>
#include <vector>

namespace sl {

// ============================================================================
// Game Object
// ============================================================================

struct GameObject {
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 size = glm::vec3(1.0f);
    glm::vec3 color = glm::vec3(1.0f);
    glm::vec3 originalColor = glm::vec3(1.0f);
    float yawDegrees = 0.0f;
    
    // Item properties
    std::string name;
    int typeId = 0;
    float tradeValue = 0.0f;
    float condition = 1.0f;
    float weight = 1.0f;
    int category = 0;
    
    // State
    bool isVisible = true;
    bool isStatic = false;
    bool isSelectable = true;
    bool isSmallBlock = false;
    bool isAttachedToVehicle = false;
    int snappedToBigBlock = -1;
    int attachedToVehicle = -1;
    glm::vec3 localOffset = glm::vec3(0.0f);
    
    // Rendering
    vkcore::MeshHandle mesh = vkcore::INVALID_MESH;
    
    // Helper to get transform matrix
    glm::mat4 getTransform() const {
        glm::mat4 t = glm::mat4(1.0f);
        t = glm::translate(t, position);
        t = glm::rotate(t, glm::radians(yawDegrees), glm::vec3(0, 1, 0));
        t = glm::scale(t, size);
        return t;
    }
};

// ============================================================================
// FPS Camera
// ============================================================================

struct FPSCamera {
    glm::vec3 position = glm::vec3(0.0f, 2.0f, 5.0f);
    float yaw = 0.0f;      // Horizontal (degrees)
    float pitch = 0.0f;    // Vertical (degrees)
    float fov = 60.0f;
    float nearPlane = 0.1f;
    float farPlane = 50000.0f;
    float moveSpeed = 10.0f;
    float lookSpeed = 0.1f;
    
    glm::vec3 getForward() const {
        float yawRad = glm::radians(yaw);
        float pitchRad = glm::radians(pitch);
        return glm::vec3(
            sin(yawRad) * cos(pitchRad),
            sin(pitchRad),
            -cos(yawRad) * cos(pitchRad)
        );
    }
    
    glm::vec3 getRight() const {
        return glm::normalize(glm::cross(getForward(), glm::vec3(0, 1, 0)));
    }
    
    glm::mat4 getViewMatrix() const {
        return glm::lookAt(position, position + getForward(), glm::vec3(0, 1, 0));
    }
    
    glm::mat4 getProjectionMatrix(float aspect) const {
        glm::mat4 proj = glm::perspective(glm::radians(fov), aspect, nearPlane, farPlane);
        proj[1][1] *= -1;  // Vulkan Y flip
        return proj;
    }
};

// ============================================================================
// SLAG LEGION Game Engine
// ============================================================================

class GameEngine {
public:
    GameEngine();
    ~GameEngine();
    
    // ========================================================================
    // Lifecycle
    // ========================================================================
    
    bool init(int width = 1920, int height = 1080, bool fullscreen = true);
    void shutdown();
    bool shouldClose() const;
    void pollEvents();
    
    // ========================================================================
    // Rendering
    // ========================================================================
    
    void beginFrame();
    void render();
    void endFrame();
    
    // ========================================================================
    // Input
    // ========================================================================
    
    bool isKeyPressed(int key) const;
    glm::vec2 getMousePosition() const;
    glm::vec2 getMouseDelta();
    void hideCursor();
    void showCursor();
    
    // ========================================================================
    // Camera
    // ========================================================================
    
    FPSCamera& getCamera() { return m_camera; }
    void updateCamera(float deltaTime);
    
    // ========================================================================
    // Game Objects
    // ========================================================================
    
    int createObject(const glm::vec3& pos, const glm::vec3& size, const glm::vec3& color);
    int createSmallBlock(const glm::vec3& pos, const glm::vec3& color);
    GameObject* getObject(int index);
    int getObjectCount() const { return static_cast<int>(m_objects.size()); }
    void setObjectPosition(int index, const glm::vec3& pos);
    void setObjectRotation(int index, float yaw);
    void setObjectColor(int index, const glm::vec3& color);
    void restoreObjectColor(int index);
    
    // ========================================================================
    // Raycasting
    // ========================================================================
    
    int raycastFromScreen(float screenX, float screenY, glm::vec3& hitPoint);
    int raycastFromCenter(glm::vec3& hitPoint);
    float raycastDownward(const glm::vec3& position);
    
    // ========================================================================
    // Vehicle System
    // ========================================================================
    
    void attachToVehicle(int objectIndex, int vehicleIndex, const glm::vec3& localOffset);
    void detachFromVehicle(int objectIndex);
    bool isAttachedToVehicle(int objectIndex) const;
    void updateAttachedObjects(int vehicleIndex);
    
    // ========================================================================
    // Block Snapping
    // ========================================================================
    
    int findBigBlockUnder(const glm::vec3& point);
    void snapSmallBlockToBigBlock(int smallIdx, int bigIdx);
    void dropSmallBlock(int smallIdx, const glm::vec3& dropPos);
    
    // ========================================================================
    // Accessors
    // ========================================================================
    
    GLFWwindow* getWindow() const { return m_window; }
    vkcore::VulkanCore& getCore() { return m_core; }
    float getDeltaTime() const { return m_deltaTime; }
    int getWidth() const { return m_core.getWidth(); }
    int getHeight() const { return m_core.getHeight(); }

private:
    void initWorld();
    void processInput(float deltaTime);
    
    // VulkanCore - the foundation
    vkcore::VulkanCore m_core;
    vkcore::PipelineHandle m_pipeline = vkcore::INVALID_PIPELINE;
    
    // Window
    GLFWwindow* m_window = nullptr;
    
    // Game state
    FPSCamera m_camera;
    std::vector<GameObject> m_objects;
    
    // Shared mesh for cubes (all cubes use same geometry)
    vkcore::MeshHandle m_cubeMesh = vkcore::INVALID_MESH;
    
    // Input state
    glm::vec2 m_lastMousePos = glm::vec2(0);
    glm::vec2 m_mouseDelta = glm::vec2(0);
    bool m_firstMouse = true;
    bool m_cursorHidden = false;
    
    // Timing
    float m_deltaTime = 0.016f;
    double m_lastFrameTime = 0.0;
};

// ============================================================================
// Utility
// ============================================================================

inline float degToRad(float d) { return d * 0.0174532925f; }
inline float radToDeg(float r) { return r * 57.2957795f; }

} // namespace sl

#endif // SL_ENGINE_V2_H

