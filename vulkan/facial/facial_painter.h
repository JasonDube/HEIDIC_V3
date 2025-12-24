// ============================================================================
// FACIAL PAINTER - Direct 3D Painting for DMap Creation
// ============================================================================
// Allows painting displacement directly on 3D models, then bakes to DMap textures.
// Much more intuitive than manual UV1/DMap creation in Blender!
// ============================================================================

#ifndef FACIAL_PAINTER_H
#define FACIAL_PAINTER_H

#include "facial_types.h"
#include "../core/vulkan_core.h"
#include "../utils/raycast.h"

#include <glm/glm.hpp>
#include <vector>
#include <string>

namespace facial {

// ============================================================================
// Paint Brush Settings
// ============================================================================

struct PaintBrush {
    float size = 0.1f;           // Brush radius in world units
    float strength = 0.5f;        // Displacement strength (0.0 to 1.0)
    glm::vec3 direction = glm::vec3(0, 0, 1);  // Displacement direction (normalized)
    bool enabled = false;
};

// ============================================================================
// Painted Vertex Data
// ============================================================================

struct PaintedVertex {
    glm::vec3 basePosition;       // Original position
    glm::vec3 displacement;       // Painted displacement
    glm::vec2 uv0;                // Original UV0
    glm::vec2 uv1;                // Auto-generated UV1
    bool painted = false;         // Has this vertex been painted?
};

// ============================================================================
// FacialPainter Class
// ============================================================================

class FacialPainter {
public:
    FacialPainter();
    ~FacialPainter();
    
    // Non-copyable
    FacialPainter(const FacialPainter&) = delete;
    FacialPainter& operator=(const FacialPainter&) = delete;
    
    // ========================================================================
    // Setup
    // ========================================================================
    
    // Initialize with mesh data (from loaded model)
    // Takes vertices and indices, creates editable copy
    bool init(const std::vector<glm::vec3>& positions,
              const std::vector<glm::vec3>& normals,
              const std::vector<glm::vec2>& uv0,
              const std::vector<uint32_t>& indices);
    
    // ========================================================================
    // Painting
    // ========================================================================
    
    // Paint displacement at a 3D position (from raycast hit)
    // Returns true if any vertices were painted
    bool paintAt(const glm::vec3& worldPos, const glm::vec3& normal, const PaintBrush& brush);
    
    // Paint on a specific vertex (for precise control)
    void paintVertex(size_t vertexIndex, const glm::vec3& displacement);
    
    // Clear all painted displacement
    void clearPainting();
    
    // Undo last paint stroke
    void undo();
    
    // ========================================================================
    // UV1 Generation
    // ========================================================================
    
    enum class UV1Method {
        REGION_ATLAS,      // Face regions (jaw, lips, brows, etc.)
        SPHERICAL_PROJECTION,  // Simple sphere projection
        CUBE_PROJECTION,   // Cube map style
        COPY_UV0           // Copy UV0 (can refine later)
    };
    
    // Auto-generate UV1 coordinates
    // Method: Simple projection or region-based atlas
    void generateUV1(UV1Method method = UV1Method::REGION_ATLAS);
    
    // ========================================================================
    // DMap Baking
    // ========================================================================
    
    // Bake painted displacement to DMap texture
    // Returns true on success, saves texture to 'outputPath'
    bool bakeDMap(const std::string& outputPath, int resolution = 1024);
    
    // Get the baked DMap texture data (for loading into FacialSystem)
    bool getBakedDMap(std::vector<uint8_t>& pixels, uint32_t& width, uint32_t& height);
    
    // ========================================================================
    // Mesh Updates
    // ========================================================================
    
    // Get current mesh with displacement applied (for preview)
    // Returns vertices ready for upload to GPU
    void getDisplacedMesh(std::vector<glm::vec3>& positions,
                         std::vector<glm::vec3>& normals,
                         std::vector<glm::vec2>& uv0,
                         std::vector<glm::vec2>& uv1) const;
    
    // Update GPU mesh with current displacement (real-time preview)
    bool updateGPUMesh(vkcore::VulkanCore* core, vkcore::MeshHandle meshHandle);
    
    // ========================================================================
    // Getters
    // ========================================================================
    
    bool isInitialized() const { return m_initialized; }
    size_t getVertexCount() const { return m_paintedVertices.size(); }
    size_t getPaintedCount() const;
    
    // Get brush position for visualization
    glm::vec3 getBrushPosition() const { return m_brushPosition; }
    bool isBrushActive() const { return m_brushActive; }
    
private:
    // Find vertices within brush radius
    void findVerticesInBrush(const glm::vec3& center, float radius,
                             std::vector<size_t>& outIndices);
    
    // Calculate displacement falloff (smooth brush)
    float calculateFalloff(float distance, float radius);
    
    // Recalculate normals after displacement
    void recalculateNormals();
    
    // ========================================================================
    // State
    // ========================================================================
    
    bool m_initialized = false;
    std::vector<PaintedVertex> m_paintedVertices;
    std::vector<uint32_t> m_indices;
    
    // Brush state
    glm::vec3 m_brushPosition = glm::vec3(0);
    bool m_brushActive = false;
    
    // Undo system
    struct PaintStroke {
        std::vector<size_t> vertexIndices;
        std::vector<glm::vec3> oldDisplacements;
    };
    std::vector<PaintStroke> m_undoStack;
    static constexpr size_t MAX_UNDO_STEPS = 50;
};

} // namespace facial

#endif // FACIAL_PAINTER_H

