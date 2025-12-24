// ESE (Echo Synapse Editor) - Selection System
// Handles vertex, edge, face, and quad selection with picking

#ifndef ESE_SELECTION_H
#define ESE_SELECTION_H

#include "ese_types.h"
#include <vector>
#include <memory>

struct MeshVertex;
class MeshResource;

namespace ese {

class Selection {
public:
    Selection();
    ~Selection() = default;
    
    // Initialize selection system
    void init();
    
    // Set active mode
    void setMode(SelectionMode mode);
    SelectionMode getMode() const { return m_state.mode; }
    
    // Clear all selections
    void clearAll();
    void clearMode(SelectionMode mode);
    
    // Get state
    const SelectionState& getState() const { return m_state; }
    SelectionState& getState() { return m_state; }
    
    // Pick operations (returns true if something was selected)
    bool pickVertex(const std::vector<MeshVertex>& vertices, 
                    double mouseX, double mouseY,
                    const glm::mat4& view, const glm::mat4& proj,
                    int screenWidth, int screenHeight,
                    bool multiSelect);
    
    bool pickEdge(const std::vector<QuadEdge>& edges,
                  const std::vector<MeshVertex>& vertices,
                  double mouseX, double mouseY,
                  const glm::mat4& view, const glm::mat4& proj,
                  int screenWidth, int screenHeight,
                  bool multiSelect);
    
    bool pickFace(const std::vector<MeshVertex>& vertices,
                  const std::vector<uint32_t>& indices,
                  const glm::vec3& rayOrigin, const glm::vec3& rayDir);
    
    bool pickQuad(const std::vector<std::array<uint32_t, 4>>& quads,
                  const std::vector<MeshVertex>& vertices,
                  const glm::vec3& rayOrigin, const glm::vec3& rayDir,
                  bool multiSelect);
    
    // Get selected center point (for gizmo positioning)
    glm::vec3 getSelectionCenter(const std::vector<MeshVertex>& vertices) const;
    
    // Get all vertices affected by current selection
    std::set<uint32_t> getAffectedVertices(
        const std::vector<MeshVertex>& vertices,
        const std::vector<uint32_t>& indices,
        const std::vector<std::array<uint32_t, 4>>& quads,
        const std::vector<QuadEdge>& edges) const;
    
private:
    SelectionState m_state;
    
    // Project world point to screen
    glm::vec2 projectToScreen(const glm::vec3& worldPos,
                              const glm::mat4& view, const glm::mat4& proj,
                              int screenWidth, int screenHeight,
                              bool* inFront) const;
    
    // Ray-triangle intersection
    bool rayTriangle(const glm::vec3& origin, const glm::vec3& dir,
                     const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2,
                     float& t) const;
};

} // namespace ese

#endif // ESE_SELECTION_H

