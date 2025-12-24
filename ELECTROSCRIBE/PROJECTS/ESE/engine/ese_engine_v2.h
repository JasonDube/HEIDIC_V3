// ============================================================================
// ESE Engine v2 - Built on VulkanCore
// ============================================================================
// Echo Synapse Editor using VulkanCore + ImGui
// 
// Before: ~1400 lines of custom Vulkan code
// After:  ~200 lines (just editor logic)
// ============================================================================

#ifndef ESE_ENGINE_V2_H
#define ESE_ENGINE_V2_H

#define VKCORE_ENABLE_IMGUI  // Enable ImGui support in VulkanCore
#include "../../../../vulkan/core/vulkan_core.h"
#include "../../../../vulkan/utils/raycast.h"

#include <string>
#include <vector>
#include <memory>

namespace ese {

// ============================================================================
// Orbit Camera (for editor)
// ============================================================================

struct OrbitCamera {
    float yaw = 45.0f;
    float pitch = 30.0f;
    float distance = 5.0f;
    glm::vec3 target = glm::vec3(0.0f);
    glm::vec3 pan = glm::vec3(0.0f);
    
    glm::vec3 getPosition() const {
        float yawRad = glm::radians(yaw);
        float pitchRad = glm::radians(glm::clamp(pitch, -89.0f, 89.0f));
        return target + pan + glm::vec3(
            distance * cosf(pitchRad) * sinf(yawRad),
            distance * sinf(pitchRad),
            distance * cosf(pitchRad) * cosf(yawRad)
        );
    }
    
    glm::mat4 getViewMatrix() const {
        return glm::lookAt(getPosition(), target + pan, glm::vec3(0, 1, 0));
    }
    
    void orbit(float dx, float dy, float sensitivity = 0.5f) {
        yaw += dx * sensitivity;
        pitch += dy * sensitivity;
        pitch = glm::clamp(pitch, -89.0f, 89.0f);
    }
    
    void zoom(float delta) {
        distance *= (1.0f - delta * 0.1f);
        distance = glm::clamp(distance, 0.5f, 100.0f);
    }
    
    void doPan(float dx, float dy) {
        float yawRad = glm::radians(yaw);
        float speed = distance * 0.002f;
        pan.x -= dx * cosf(yawRad) * speed;
        pan.y += dy * speed;
        pan.z -= dx * (-sinf(yawRad)) * speed;
    }
    
    void reset() {
        yaw = 45.0f; pitch = 30.0f; distance = 5.0f;
        target = glm::vec3(0.0f); pan = glm::vec3(0.0f);
    }
};

// ============================================================================
// Selection Modes
// ============================================================================

enum class SelectionMode { None, Vertex, Edge, Face, Quad };
enum class GizmoMode { None, Move, Scale, Rotate };
enum class GizmoAxis { None = -1, X = 0, Y = 1, Z = 2 };

// ============================================================================
// Mesh Data
// ============================================================================

struct MeshVertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoord;
};

struct EditableMesh {
    std::vector<MeshVertex> vertices;
    std::vector<uint32_t> indices;
    vkcore::MeshHandle renderMesh = vkcore::INVALID_MESH;
    std::string name = "Untitled";
    bool modified = false;
    
    void clear() {
        vertices.clear();
        indices.clear();
        modified = false;
    }
};

// ============================================================================
// ESE Editor Engine
// ============================================================================

class EditorEngine {
public:
    EditorEngine();
    ~EditorEngine();
    
    // ========================================================================
    // Lifecycle
    // ========================================================================
    
    bool init(int width = 1280, int height = 720);
    bool initWithWindow(GLFWwindow* existingWindow);  // Use existing window
    void shutdown();
    bool shouldClose() const;
    void pollEvents();
    
    // ========================================================================
    // Rendering
    // ========================================================================
    
    void beginFrame();
    void render();
    void renderUI();
    void endFrame();
    
    // ========================================================================
    // File Operations
    // ========================================================================
    
    bool loadOBJ(const std::string& path);
    bool saveHDM(const std::string& path);
    bool loadHDM(const std::string& path);
    void createCube();
    void newMesh();
    
    // ========================================================================
    // Camera
    // ========================================================================
    
    OrbitCamera& getCamera() { return m_camera; }
    void processInput();
    
    // ========================================================================
    // Selection
    // ========================================================================
    
    void setSelectionMode(SelectionMode mode);
    SelectionMode getSelectionMode() const { return m_selectionMode; }
    void clearSelection();
    int pickAt(float screenX, float screenY);
    
    // ========================================================================
    // Gizmo
    // ========================================================================
    
    void setGizmoMode(GizmoMode mode);
    GizmoMode getGizmoMode() const { return m_gizmoMode; }
    
    // ========================================================================
    // Mesh Info
    // ========================================================================
    
    int getVertexCount() const { return static_cast<int>(m_mesh.vertices.size()); }
    int getTriangleCount() const { return static_cast<int>(m_mesh.indices.size() / 3); }
    const std::string& getMeshName() const { return m_mesh.name; }
    bool isModified() const { return m_mesh.modified; }
    
    // ========================================================================
    // View Options
    // ========================================================================
    
    void setWireframeMode(bool enabled) { m_wireframeMode = enabled; }
    bool isWireframeMode() const { return m_wireframeMode; }
    void toggleWireframe() { m_wireframeMode = !m_wireframeMode; }
    
    // ========================================================================
    // Accessors
    // ========================================================================
    
    GLFWwindow* getWindow() const { return m_window; }
    vkcore::VulkanCore& getCore() { return m_core; }

private:
    bool initCore(int width, int height);  // Common init after window setup
    void createDefaultCube();
    void updateRenderMesh();
    void drawGizmo();
    void drawUI();
    
    // VulkanCore
    vkcore::VulkanCore m_core;
    vkcore::PipelineHandle m_solidPipeline = vkcore::INVALID_PIPELINE;
    vkcore::PipelineHandle m_wirePipeline = vkcore::INVALID_PIPELINE;
    
    // Window
    GLFWwindow* m_window = nullptr;
    bool m_ownsWindow = false;  // true if we created the window, false if external
    
    // Camera
    OrbitCamera m_camera;
    
    // Mesh data
    EditableMesh m_mesh;
    
    // Selection
    SelectionMode m_selectionMode = SelectionMode::Face;
    GizmoMode m_gizmoMode = GizmoMode::Move;
    GizmoAxis m_activeAxis = GizmoAxis::None;
    int m_selectedVertex = -1;
    int m_selectedEdge = -1;
    int m_selectedFace = -1;
    
    // Input state
    glm::vec2 m_lastMousePos = glm::vec2(0);
    bool m_rightMouseDown = false;
    bool m_middleMouseDown = false;
    
    // View options
    bool m_wireframeMode = false;
    bool m_showGrid = true;
    bool m_showNormals = false;
};

} // namespace ese

#endif // ESE_ENGINE_V2_H

