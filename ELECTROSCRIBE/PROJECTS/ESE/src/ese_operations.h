// ESE (Echo Synapse Editor) - Mesh Operations
// Extrusion, edge loops, and other mesh editing operations

#ifndef ESE_OPERATIONS_H
#define ESE_OPERATIONS_H

#include "ese_types.h"
#include <vector>
#include <set>
#include <memory>

struct MeshVertex;
class MeshResource;

namespace ese {

class MeshOperations {
public:
    // ========================================================================
    // Extrusion
    // ========================================================================
    
    // Extrude selected quad(s) - creates new geometry
    // Returns indices of newly created vertices that should be moved
    static std::vector<uint32_t> extrudeQuads(
        std::vector<MeshVertex>& vertices,
        std::vector<uint32_t>& indices,
        const std::set<int>& selectedQuads,
        const std::vector<std::array<uint32_t, 4>>& quads);
    
    // Extrude single face (triangle)
    static std::vector<uint32_t> extrudeFace(
        std::vector<MeshVertex>& vertices,
        std::vector<uint32_t>& indices,
        int faceIndex);
    
    // ========================================================================
    // Edge Loop
    // ========================================================================
    
    // Insert edge loop perpendicular to selected edge
    // Returns number of new vertices created
    static int insertEdgeLoop(
        std::vector<MeshVertex>& vertices,
        std::vector<uint32_t>& indices,
        int selectedEdgeIndex,
        const std::vector<QuadEdge>& edges,
        const std::vector<std::array<uint32_t, 4>>& quads);
    
    // ========================================================================
    // Vertex Operations
    // ========================================================================
    
    // Move vertices by delta
    static void moveVertices(
        std::vector<MeshVertex>& vertices,
        const std::set<uint32_t>& vertexIndices,
        const glm::vec3& delta);
    
    // Scale vertices around center
    static void scaleVertices(
        std::vector<MeshVertex>& vertices,
        const std::set<uint32_t>& vertexIndices,
        const glm::vec3& center,
        float scaleFactor,
        GizmoAxis axis);
    
    // Rotate vertices around center
    static void rotateVertices(
        std::vector<MeshVertex>& vertices,
        const std::set<uint32_t>& vertexIndices,
        const glm::vec3& center,
        float angleDegrees,
        GizmoAxis axis);
    
    // ========================================================================
    // Quad Reconstruction
    // ========================================================================
    
    // Reconstruct quads from triangulated mesh
    static std::vector<std::array<uint32_t, 4>> reconstructQuads(
        const std::vector<MeshVertex>& vertices,
        const std::vector<uint32_t>& indices);
    
    // Build quad edges (edges that form quad perimeters)
    static std::vector<QuadEdge> buildQuadEdges(
        const std::vector<MeshVertex>& vertices,
        const std::vector<uint32_t>& indices,
        const std::vector<std::array<uint32_t, 4>>& quads);
    
    // ========================================================================
    // Utilities
    // ========================================================================
    
    // Find shared edge between two quads
    static bool findSharedEdge(
        const std::array<uint32_t, 4>& quad1,
        const std::array<uint32_t, 4>& quad2,
        uint32_t& v1, uint32_t& v2);
    
    // Calculate face normal
    static glm::vec3 calculateFaceNormal(
        const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2);
    
    // Check if two vertices have same position (within epsilon)
    static bool samePosition(
        const MeshVertex& a, const MeshVertex& b,
        float epsilon = 0.0001f);
    
    // Find all vertices at same position (for connected mesh operations)
    static std::set<uint32_t> findCoincidentVertices(
        const std::vector<MeshVertex>& vertices,
        uint32_t sourceVertex,
        float epsilon = 0.0001f);
    
    // Recalculate normals for affected triangles
    static void recalculateNormals(
        std::vector<MeshVertex>& vertices,
        const std::vector<uint32_t>& indices,
        const std::set<uint32_t>& affectedVertices);
    
private:
    // Helper: Get position key string for edge matching
    static std::string positionKey(float x, float y, float z);
    
    // Helper: Add quad triangles to index buffer
    static void addQuadTriangles(
        std::vector<uint32_t>& indices,
        uint32_t v0, uint32_t v1, uint32_t v2, uint32_t v3,
        bool flipNormal = false);
};

} // namespace ese

#endif // ESE_OPERATIONS_H

