// ESE (Echo Synapse Editor) - Transform Gizmo
// Visual gizmo for move, scale, and rotate operations

#ifndef ESE_GIZMO_H
#define ESE_GIZMO_H

#include "ese_types.h"
#include <glm/glm.hpp>

namespace ese {

class Gizmo {
public:
    Gizmo();
    ~Gizmo() = default;
    
    // Initialize gizmo
    void init();
    
    // Set gizmo mode
    void setMode(GizmoMode mode);
    GizmoMode getMode() const { return m_state.mode; }
    
    // Set gizmo position
    void setPosition(const glm::vec3& pos);
    glm::vec3 getPosition() const { return m_state.position; }
    
    // Hit test against gizmo axes (returns axis that was hit, or None)
    GizmoAxis hitTest(double mouseX, double mouseY,
                      const glm::mat4& view, const glm::mat4& proj,
                      int screenWidth, int screenHeight) const;
    
    // Begin drag operation
    void beginDrag(GizmoAxis axis, double mouseX, double mouseY);
    
    // Continue drag operation (returns delta for the axis)
    float continueDrag(double mouseX, double mouseY);
    
    // End drag operation
    void endDrag();
    
    // Check if currently dragging
    bool isDragging() const { return m_state.isDragging; }
    GizmoAxis getDragAxis() const { return m_state.dragAxis; }
    
    // Get state for rendering
    const GizmoState& getState() const { return m_state; }
    
    // Get axis color
    static glm::vec3 getAxisColor(GizmoAxis axis, bool selected = false);
    
    // Get axis direction
    static glm::vec3 getAxisDirection(GizmoAxis axis);
    
private:
    GizmoState m_state;
    
    // Cached screen positions for hit testing
    mutable glm::vec2 m_axisEndPoints[3];
    mutable glm::vec2 m_centerPoint;
    
    // Project point to screen
    glm::vec2 projectToScreen(const glm::vec3& worldPos,
                              const glm::mat4& view, const glm::mat4& proj,
                              int screenWidth, int screenHeight) const;
};

// ============================================================================
// Gizmo Renderer (for drawing the gizmo)
// ============================================================================

class GizmoRenderer {
public:
    // Draw move gizmo
    static void drawMoveGizmo(const glm::vec3& position, 
                              GizmoAxis selectedAxis,
                              const glm::mat4& view, const glm::mat4& proj,
                              int screenWidth, int screenHeight);
    
    // Draw scale gizmo
    static void drawScaleGizmo(const glm::vec3& position,
                               GizmoAxis selectedAxis,
                               const glm::mat4& view, const glm::mat4& proj,
                               int screenWidth, int screenHeight);
    
    // Draw rotate gizmo
    static void drawRotateGizmo(const glm::vec3& position,
                                GizmoAxis selectedAxis,
                                const glm::mat4& view, const glm::mat4& proj,
                                int screenWidth, int screenHeight);
    
private:
    // Helper to draw axis arrow
    static void drawAxis(const glm::vec2& start, const glm::vec2& end,
                         const glm::vec3& color, bool selected);
};

} // namespace ese

#endif // ESE_GIZMO_H

