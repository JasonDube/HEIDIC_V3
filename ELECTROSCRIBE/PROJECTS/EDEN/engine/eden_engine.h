// ============================================================================
// EDEN ENGINE - Level Editor Main Engine
// ============================================================================
// The main engine class for EDEN level editor.
// Handles window, rendering, UI, and scene management.
// ============================================================================

#ifndef EDEN_ENGINE_H
#define EDEN_ENGINE_H

#include "scene.h"
#include "entity.h"
#include "../../../../vulkan/core/vulkan_core.h"
#include "../../../../vulkan/platform/file_dialog.h"
#include "../../../../vulkan/lighting/lighting_manager.h"

#include <string>

namespace eden {

// ============================================================================
// FPS Camera for viewport navigation (fly-through controls)
// ============================================================================
// WASD + mouse look, Space/Shift for up/down
// Hold right mouse button to look around
// ============================================================================

struct FPSCamera {
    glm::vec3 position = glm::vec3(0.0f, 2.0f, 5.0f);
    float yaw = 0.0f;      // Horizontal rotation (degrees)
    float pitch = 0.0f;    // Vertical rotation (degrees)
    float fov = 60.0f;
    float nearPlane = 0.1f;
    float farPlane = 1000.0f;
    float moveSpeed = 5.0f;
    float lookSpeed = 0.15f;
    float aspectRatio = 1.777f;
    
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
    
    glm::vec3 getPosition() const {
        return position;
    }
    
    glm::mat4 getViewMatrix() const {
        return glm::lookAt(position, position + getForward(), glm::vec3(0, 1, 0));
    }
    
    glm::mat4 getProjectionMatrix() const {
        glm::mat4 proj = glm::perspective(glm::radians(fov), aspectRatio, nearPlane, farPlane);
        proj[1][1] *= -1;  // Flip Y for Vulkan clip space
        return proj;
    }
    
    void setAspectRatio(float ar) { aspectRatio = ar; }
    
    void look(float dx, float dy) {
        yaw += dx * lookSpeed;
        pitch -= dy * lookSpeed;
        pitch = glm::clamp(pitch, -89.0f, 89.0f);
    }
    
    void move(float forward, float right, float up, float deltaTime) {
        glm::vec3 movement(0.0f);
        
        if (forward != 0.0f) {
            movement += getForward() * forward;
        }
        if (right != 0.0f) {
            movement += getRight() * right;
        }
        if (up != 0.0f) {
            movement.y += up;
        }
        
        if (glm::length(movement) > 0.001f) {
            movement = glm::normalize(movement);
            position += movement * moveSpeed * deltaTime;
        }
    }
    
    void reset() {
        position = glm::vec3(0.0f, 2.0f, 5.0f);
        yaw = 0.0f;
        pitch = 0.0f;
    }
};

// ============================================================================
// Editor Class
// ============================================================================

class Editor {
public:
    Editor();
    ~Editor();
    
    bool init(int width, int height);
    void run();
    void cleanup();
    
private:
    // ========================================================================
    // Core Systems
    // ========================================================================
    
    void update(float deltaTime);
    void render();
    void handleInput();
    
    // ========================================================================
    // UI (ImGui)
    // ========================================================================
    
    void renderUI();
    void renderMenuBar();
    void renderSceneHierarchy();
    void renderProperties();
    void renderViewportInfo();
    
    // ========================================================================
    // File Operations
    // ========================================================================
    
    void newScene();
    void openScene();
    void saveScene();
    void saveSceneAs();
    void addModelDialog();
    void addPointLightToScene();
    void addDirectionalLightToScene();
    void deleteSelectedEntity();
    
    // ========================================================================
    // Rendering
    // ========================================================================
    
    void renderScene();
    void updateLighting();
    
    // Helper to create a cube with normals for lighting
    vkcore::MeshHandle createLitCube(float size);
    
    // ========================================================================
    // State
    // ========================================================================
    
    GLFWwindow* m_window = nullptr;
    vkcore::VulkanCore m_core;
    lighting::LightingManager m_lighting;
    FPSCamera m_camera;
    Scene m_scene;
    
    // Default cube (always visible for debugging)
    vkcore::MeshHandle m_debugCube = vkcore::INVALID_MESH;
    bool m_showDebugCube = true;
    
    // Input state for FPS camera
    glm::vec2 m_lastMousePos = glm::vec2(0);
    bool m_rightMouseDown = false;
    bool m_firstMouse = true;
    
    bool m_running = false;
    bool m_showUI = true;
    int m_windowWidth = 1400;
    int m_windowHeight = 900;
    
    // Lighting settings (for directional light fallback if no dir light in scene)
    glm::vec3 m_defaultLightDir = glm::normalize(glm::vec3(0.5f, -1.0f, 0.5f));
    glm::vec3 m_defaultLightColor = glm::vec3(1.0f, 0.98f, 0.95f);
    float m_defaultLightIntensity = 1.0f;
    glm::vec3 m_ambientColor = glm::vec3(0.3f, 0.3f, 0.35f);  // Brighter ambient for debugging
    float m_shininess = 32.0f;
    float m_specularStrength = 0.5f;
};

} // namespace eden

// ============================================================================
// C API for HEIDIC
// ============================================================================

extern "C" {
    int eden_init(int width, int height);
    void eden_run();
    void eden_cleanup();
}

#endif // EDEN_ENGINE_H

