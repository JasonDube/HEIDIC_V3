// ============================================================================
// EaSE Engine - Model Viewer
// ============================================================================
// Uses VulkanCore for rendering. Supports OBJ and GLB formats.
// ============================================================================

#ifndef EASE_ENGINE_H
#define EASE_ENGINE_H

#include "../../../../vulkan/core/vulkan_core.h"
#include "../../../../vulkan/lighting/lighting_manager.h"
#include "../../../../vulkan/facial/facial_system.h"
#include "../../../../vulkan/facial/facial_painter.h"
#include "../../../../vulkan/platform/file_dialog.h"
#include "../../../../vulkan/utils/glb_loader.h"
#include "../../../../vulkan/utils/raycast.h"
#include <string>
#include <vector>

namespace ease {

// ============================================================================
// Vertex format for loaded meshes
// ============================================================================

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoord;
};

// ============================================================================
// Loaded mesh data
// ============================================================================

struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    vkcore::MeshHandle renderHandle = vkcore::INVALID_MESH;
    std::string name;
};

// ============================================================================
// Orbit camera for viewing
// ============================================================================

struct OrbitCamera {
    float yaw = 45.0f;
    float pitch = 30.0f;
    float distance = 5.0f;
    glm::vec3 target = glm::vec3(0.0f);
    float fov = 60.0f;
    float nearPlane = 0.1f;
    float farPlane = 1000.0f;
    float aspectRatio = 16.0f / 9.0f;
    
    glm::vec3 getPosition() const;
    glm::mat4 getViewMatrix() const;
    glm::mat4 getProjectionMatrix() const;
    
    void orbit(float dx, float dy);
    void zoom(float delta);
    void pan(float dx, float dy);
    void reset();
};

// ============================================================================
// EaSE Viewer Engine
// ============================================================================

class Viewer {
public:
    bool init(int width, int height);
    void run();
    void cleanup();
    
    // File operations
    bool loadOBJ(const std::string& path);
    bool loadGLB(const std::string& path);
    bool loadTexture(const std::string& path);
    void openFileDialog();
    void openTextureDialog();
    
    // Scroll callback (called by GLFW)
    void onScroll(float yoffset);
    
    // Mouse callback for painting
    void onMouseClick(double x, double y, int button);
    
private:
    void processInput();
    void render();
    void renderUI();
    void renderPaintUI();
    void uploadMeshToGPU();
    void initPainterFromMesh();  // Initialize painter with current mesh data
    
    // Vulkan
    vkcore::VulkanCore m_core;
    vkcore::PipelineHandle m_pipeline = vkcore::INVALID_PIPELINE;
    GLFWwindow* m_window = nullptr;
    
    // Lighting
    lighting::LightingManager m_lighting;
    bool m_useLighting = true;  // Toggle between lit and unlit
    
    // Facial Animation
    facial::FacialSystem m_facial;
    bool m_useFacialAnimation = false;  // Toggle facial animation
    bool m_showFacialUI = false;         // Show facial slider UI
    float m_facialSliders[8] = {0};      // First 8 sliders for UI
    
    // Facial Painting (DMap Creation)
    facial::FacialPainter m_painter;
    bool m_paintMode = false;            // Is paint mode active?
    facial::PaintBrush m_brush;          // Current brush settings (initialized in init)
    bool m_showPaintUI = false;          // Show paint tools UI
    
    // Data
    Mesh m_mesh;
    OrbitCamera m_camera;
    vkcore::TextureHandle m_texture = vkcore::INVALID_TEXTURE;
    std::string m_textureName;
    
    // Input state
    glm::vec2 m_lastMouse = glm::vec2(0);
    bool m_rightMouseDown = false;
    bool m_middleMouseDown = false;
    
    // UI state
    bool m_showUI = true;
    glm::vec3 m_modelColor = glm::vec3(1.0f, 1.0f, 1.0f);  // Pure white - show texture as-is
    bool m_wireframe = false;
    bool m_disableBackfaceCulling = false;  // Toggle for debugging
    vkcore::PipelineHandle m_pipelineNoCull = vkcore::INVALID_PIPELINE;  // Pipeline with culling disabled
    
    // Model transform (for fixing orientation issues)
    glm::vec3 m_modelRotation = glm::vec3(0.0f);     // Euler angles in degrees (X, Y, Z)
    glm::vec3 m_modelTranslation = glm::vec3(0.0f);  // Position offset (X, Y, Z)
    
    // Debug rendering
    bool m_debugSolidColor = false;  // Render with solid color only (no texture/lighting)
    int m_lightingDebugMode = 0;     // 0=normal, 1=normals, 2=texture only, 3=solid white
    glm::vec3 m_backgroundColor = glm::vec3(0.1f, 0.1f, 0.12f);  // Clear/background color
    
    // Normal visualization and manipulation
    bool m_flipNormals = false;           // Track if normals have been flipped
    bool m_pendingNormalFlip = false;     // Deferred flip operation
    int m_pendingRecalcNormals = 0;       // 0=none, 1=outward, 2=inward
    
    // Helper functions
    void flipMeshNormals();               // Flip all normals in current mesh
    void recalculateNormals(bool outward); // Recalculate all normals consistently
    
    // Light settings (for UI)
    glm::vec3 m_lightDir = glm::normalize(glm::vec3(0.5f, -1.0f, 0.5f));
    glm::vec3 m_lightColor = glm::vec3(1.0f, 0.98f, 0.95f);
    float m_lightIntensity = 1.0f;
    glm::vec3 m_ambientColor = glm::vec3(0.2f, 0.2f, 0.2f);  // Minimum ambient so models are visible
    float m_shininess = 32.0f;
    float m_specularStrength = 0.5f;
    
    // Point lights (up to 4)
    struct PointLightUI {
        bool enabled = false;
        glm::vec3 position = glm::vec3(0.0f, 2.0f, 2.0f);
        glm::vec3 color = glm::vec3(1.0f, 0.8f, 0.6f);  // Warm orange
        float intensity = 1.5f;
        float range = 8.0f;
    };
    PointLightUI m_pointLights[4] = {
        { true, glm::vec3(2.0f, 1.5f, 2.0f), glm::vec3(1.0f, 0.9f, 0.7f), 1.5f, 8.0f },  // Warm front-right
        { false, glm::vec3(-2.0f, 1.5f, -2.0f), glm::vec3(0.6f, 0.8f, 1.0f), 1.0f, 8.0f }, // Cool blue back
        { false, glm::vec3(0.0f, 3.0f, 0.0f), glm::vec3(1.0f, 1.0f, 1.0f), 1.0f, 10.0f },  // White top
        { false, glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.3f, 0.3f, 0.5f), 0.5f, 5.0f }   // Dim underlight
    };
};

} // namespace ease

#endif // EASE_ENGINE_H

