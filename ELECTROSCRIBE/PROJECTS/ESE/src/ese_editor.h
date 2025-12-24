// ESE (Echo Synapse Editor) - Main Editor Class
// Combines all ESE subsystems into a cohesive editor

#ifndef ESE_EDITOR_H
#define ESE_EDITOR_H

#include "ese_types.h"
#include "ese_camera.h"
#include "ese_selection.h"
#include "ese_gizmo.h"
#include "ese_operations.h"
#include "ese_undo.h"
#include "../../vulkan/formats/hdm_format.h"

#include <vulkan/vulkan.h>
#include <memory>
#include <string>

// Forward declarations
struct GLFWwindow;
class MeshResource;

namespace ese {

class Editor {
public:
    Editor();
    ~Editor();
    
    // ========================================================================
    // Lifecycle
    // ========================================================================
    
    // Initialize the editor
    bool init(GLFWwindow* window);
    
    // Cleanup resources
    void cleanup();
    
    // Check if initialized
    bool isInitialized() const { return m_state.initialized; }
    
    // ========================================================================
    // Rendering
    // ========================================================================
    
    // Main render function (called each frame)
    void render(GLFWwindow* window);
    
    // Render mesh to command buffer
    void renderMesh(VkCommandBuffer commandBuffer);
    
    // Render UI (ImGui)
    void renderUI();
    
    // Render gizmo overlay
    void renderGizmo();
    
    // Render selection visualization
    void renderSelection();
    
    // ========================================================================
    // Input
    // ========================================================================
    
    // Process input
    void processInput(GLFWwindow* window);
    
    // Handle key press
    void onKeyPress(int key, int mods);
    
    // Handle mouse button
    void onMouseButton(int button, int action, int mods);
    
    // Handle mouse scroll
    void onScroll(float delta);
    
    // ========================================================================
    // File Operations
    // ========================================================================
    
    // Load OBJ file
    bool loadOBJ(const std::string& path);
    
    // Load texture
    bool loadTexture(const std::string& path);
    
    // Load HDM file (properties + optional geometry)
    bool loadHDM(const std::string& path);
    
    // Save HDM file
    bool saveHDM(const std::string& path);
    
    // Create new cube primitive
    bool createCube();
    
    // ========================================================================
    // Selection
    // ========================================================================
    
    // Set selection mode
    void setSelectionMode(SelectionMode mode);
    SelectionMode getSelectionMode() const { return m_selection.getMode(); }
    
    // Clear selection
    void clearSelection();
    
    // ========================================================================
    // Operations
    // ========================================================================
    
    // Extrude current selection
    void extrude();
    
    // Insert edge loop at selected edge
    void insertEdgeLoop();
    
    // Undo last operation
    void undo();
    
    // Redo last undone operation
    void redo();
    
    // ========================================================================
    // Camera
    // ========================================================================
    
    Camera& getCamera() { return m_camera; }
    const Camera& getCamera() const { return m_camera; }
    
    // Reset camera to default view
    void resetCamera();
    
    // Frame model in view
    void frameModel();
    
    // ========================================================================
    // Properties
    // ========================================================================
    
    // Get HDM properties
    HDMProperties& getProperties() { return m_properties; }
    const HDMProperties& getProperties() const { return m_properties; }
    
    // Mark properties as modified
    void markPropertiesModified() { m_state.propertiesModified = true; }
    
    // Check if properties are modified
    bool isPropertiesModified() const { return m_state.propertiesModified; }
    
    // ========================================================================
    // State
    // ========================================================================
    
    // Get editor state
    const EditorState& getState() const { return m_state; }
    
    // Toggle wireframe mode
    void toggleWireframe() { m_state.wireframeMode = !m_state.wireframeMode; }
    
    // Toggle normal visualization
    void toggleNormals() { m_state.showNormals = !m_state.showNormals; }
    
private:
    // Editor state
    EditorState m_state;
    
    // Subsystems
    Camera m_camera;
    Selection m_selection;
    Gizmo m_gizmo;
    UndoManager m_undo;
    
    // Model data
    std::unique_ptr<MeshResource> m_mesh;
    HDMProperties m_properties;
    
    // Vulkan resources (references to global state)
    GLFWwindow* m_window = nullptr;
    
    // Internal helpers
    void updateMatrices();
    void rebuildQuads();
    void processGizmoInput(double mouseX, double mouseY, bool leftDown);
    void processSelectionInput(double mouseX, double mouseY, bool leftClick, bool ctrlHeld);
};

// ============================================================================
// C API (for HEIDIC integration)
// ============================================================================

} // namespace ese

#ifdef __cplusplus
extern "C" {
#endif

// Initialize ESE editor
int heidic_init_ese(GLFWwindow* window);

// Render ESE editor (call each frame)
void heidic_render_ese(GLFWwindow* window);

// Cleanup ESE editor
void heidic_cleanup_ese();

#ifdef __cplusplus
}
#endif

#endif // ESE_EDITOR_H

