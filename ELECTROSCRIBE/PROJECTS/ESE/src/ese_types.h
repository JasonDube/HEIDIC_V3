// ESE (Echo Synapse Editor) - Core Types
// 3D Model Editor for HEIDIC Engine

#ifndef ESE_TYPES_H
#define ESE_TYPES_H

#include <vector>
#include <array>
#include <set>
#include <string>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// Forward declarations
struct MeshVertex;
class MeshResource;

namespace ese {

// ============================================================================
// Selection Mode Enum
// ============================================================================

enum class SelectionMode {
    None = 0,
    Vertex = 1,
    Edge = 2,
    Face = 3,
    Quad = 4
};

// ============================================================================
// Gizmo Types
// ============================================================================

enum class GizmoAxis {
    None = -1,
    X = 0,
    Y = 1,
    Z = 2
};

enum class GizmoMode {
    None,
    Move,
    Scale,
    Rotate
};

// ============================================================================
// Quad Edge Structure
// ============================================================================

struct QuadEdge {
    uint32_t v0, v1;                    // Vertex indices
    std::vector<int> quadIndices;       // Which quads contain this edge
    int edgeInQuad;                     // Edge index within the first quad (0-3)
    
    QuadEdge() : v0(0), v1(0), edgeInQuad(0) {}
    QuadEdge(uint32_t a, uint32_t b) : v0(a), v1(b), edgeInQuad(0) {}
};

// ============================================================================
// Undo State
// ============================================================================

struct MeshState {
    std::vector<MeshVertex> vertices;
    std::vector<uint32_t> indices;
    bool valid = false;
};

// ============================================================================
// Connection Point (for mesh attachments)
// ============================================================================

struct ConnectionPoint {
    char name[64] = "Connection Point";
    int vertex_index = -1;
    float position[3] = {0.0f, 0.0f, 0.0f};
    int connection_type = 0;            // 0=child, 1=parent
    char connected_mesh[128] = "";
    char connected_mesh_class[64] = "";
};

// ============================================================================
// Camera State
// ============================================================================

struct OrbitCamera {
    float yaw = 45.0f;                  // Horizontal rotation (degrees)
    float pitch = 30.0f;                // Vertical rotation (degrees)
    float distance = 5.0f;              // Distance from target (zoom)
    float panX = 0.0f;                  // Pan offset X
    float panY = 0.0f;                  // Pan offset Y
    float targetX = 0.0f;               // Target point X
    float targetY = 0.0f;               // Target point Y
    float targetZ = 0.0f;               // Target point Z
    
    // Calculate camera eye position
    glm::vec3 getEyePosition() const {
        float yawRad = glm::radians(yaw);
        float pitchRad = glm::radians(glm::clamp(pitch, -89.0f, 89.0f));
        
        float camX = distance * cosf(pitchRad) * sinf(yawRad);
        float camY = distance * sinf(pitchRad);
        float camZ = distance * cosf(pitchRad) * cosf(yawRad);
        
        return glm::vec3(camX + targetX + panX,
                         camY + targetY + panY,
                         camZ + targetZ);
    }
    
    // Calculate look-at center
    glm::vec3 getCenter() const {
        return glm::vec3(targetX + panX, targetY + panY, targetZ);
    }
    
    // Get view matrix
    glm::mat4 getViewMatrix() const {
        return glm::lookAt(getEyePosition(), getCenter(), glm::vec3(0.0f, 1.0f, 0.0f));
    }
    
    // Reset to defaults
    void reset() {
        yaw = 45.0f;
        pitch = 30.0f;
        distance = 5.0f;
        panX = panY = targetX = targetY = targetZ = 0.0f;
    }
};

// ============================================================================
// Selection State
// ============================================================================

struct SelectionState {
    SelectionMode mode = SelectionMode::None;
    
    // Single selection
    int selectedVertex = -1;
    int selectedEdge = -1;
    int selectedFace = -1;
    int selectedQuad = -1;
    int selectedConnectionPoint = -1;
    
    // Multi-selection
    std::set<int> selectedVertices;
    std::set<int> selectedEdges;
    std::set<int> selectedQuads;
    
    // Clear all selections
    void clear() {
        selectedVertex = -1;
        selectedEdge = -1;
        selectedFace = -1;
        selectedQuad = -1;
        selectedConnectionPoint = -1;
        selectedVertices.clear();
        selectedEdges.clear();
        selectedQuads.clear();
    }
    
    // Check if anything is selected
    bool hasSelection() const {
        switch (mode) {
            case SelectionMode::Vertex:
                return selectedVertex >= 0 || !selectedVertices.empty();
            case SelectionMode::Edge:
                return selectedEdge >= 0 || !selectedEdges.empty();
            case SelectionMode::Face:
                return selectedFace >= 0;
            case SelectionMode::Quad:
                return selectedQuad >= 0 || !selectedQuads.empty();
            default:
                return false;
        }
    }
};

// ============================================================================
// Gizmo State
// ============================================================================

struct GizmoState {
    GizmoMode mode = GizmoMode::None;
    GizmoAxis dragAxis = GizmoAxis::None;
    bool isDragging = false;
    
    float dragStartValue = 0.0f;
    double dragStartMouseX = 0.0;
    double dragStartMouseY = 0.0;
    
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 scaleCenter = glm::vec3(0.0f);
    
    void reset() {
        mode = GizmoMode::None;
        dragAxis = GizmoAxis::None;
        isDragging = false;
        position = glm::vec3(0.0f);
        scaleCenter = glm::vec3(0.0f);
    }
};

// ============================================================================
// Extrude State
// ============================================================================

struct ExtrudeState {
    bool enabled = false;               // W key enables extrude
    bool executed = false;              // Has extrusion been performed this drag?
    std::vector<uint32_t> vertices;     // Indices of extruded vertices to move
    
    void reset() {
        enabled = false;
        executed = false;
        vertices.clear();
    }
};

// ============================================================================
// Mouse State
// ============================================================================

struct MouseState {
    bool leftDown = false;
    bool rightDown = false;
    double lastX = 0.0;
    double lastY = 0.0;
    float scrollDelta = 0.0f;
    
    void update(double x, double y, bool left, bool right) {
        lastX = x;
        lastY = y;
        leftDown = left;
        rightDown = right;
    }
};

// ============================================================================
// Editor State (aggregates all state)
// ============================================================================

struct EditorState {
    bool initialized = false;
    bool modelLoaded = false;
    bool wireframeMode = false;
    bool showNormals = false;
    bool showQuads = false;
    bool showConnectionPointWindow = false;
    bool propertiesModified = false;
    
    std::string currentObjPath;
    std::string currentTexturePath;
    
    OrbitCamera camera;
    SelectionState selection;
    GizmoState gizmo;
    ExtrudeState extrude;
    MouseState mouse;
    
    // Undo stack
    std::vector<MeshState> undoStack;
    static const size_t MAX_UNDO_LEVELS = 20;
    
    // Reconstructed quads for quad mode
    std::vector<std::array<uint32_t, 4>> reconstructedQuads;
    std::vector<QuadEdge> quadEdges;
    size_t lastQuadIndexCount = 0;
    size_t lastQuadEdgeIndexCount = 0;
    
    // Connection points
    std::vector<ConnectionPoint> connectionPoints;
    
    // View/projection matrices (updated each frame)
    glm::mat4 viewMatrix = glm::mat4(1.0f);
    glm::mat4 projMatrix = glm::mat4(1.0f);
    
    void reset() {
        initialized = false;
        modelLoaded = false;
        wireframeMode = false;
        showNormals = false;
        showQuads = false;
        showConnectionPointWindow = false;
        propertiesModified = false;
        currentObjPath.clear();
        currentTexturePath.clear();
        camera.reset();
        selection.clear();
        gizmo.reset();
        extrude.reset();
        undoStack.clear();
        reconstructedQuads.clear();
        quadEdges.clear();
        lastQuadIndexCount = 0;
        lastQuadEdgeIndexCount = 0;
        connectionPoints.clear();
    }
};

// ============================================================================
// Category/Class Names
// ============================================================================

static const char* CATEGORY_NAMES[] = {
    "Generic",
    "Consumable", 
    "Part",
    "Resource",
    "Scrap",
    "Furniture",
    "Weapon",
    "Tool"
};

static const char* MESH_CLASS_NAMES[] = {
    "head",
    "torso",
    "arm",
    "leg"
};

static const char* CONNECTION_TYPE_NAMES[] = {
    "child",
    "parent"
};

} // namespace ese

#endif // ESE_TYPES_H

