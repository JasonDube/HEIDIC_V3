// ESE (Echo Synapse Editor) - Orbit Camera
// Camera system for 3D model viewing

#ifndef ESE_CAMERA_H
#define ESE_CAMERA_H

#include "ese_types.h"
#include <glm/glm.hpp>

struct GLFWwindow;

namespace ese {

class Camera {
public:
    Camera();
    ~Camera() = default;
    
    // Initialize camera with default settings
    void init();
    
    // Update camera from mouse input
    // Returns true if camera changed
    bool update(GLFWwindow* window, bool imguiWantsMouse);
    
    // Process scroll input for zoom
    void processScroll(float delta);
    
    // Get camera state
    const OrbitCamera& getState() const { return m_state; }
    OrbitCamera& getState() { return m_state; }
    
    // Get matrices
    glm::mat4 getViewMatrix() const;
    glm::mat4 getProjectionMatrix(float aspectRatio, float fov = 45.0f, 
                                   float nearPlane = 0.1f, float farPlane = 100.0f) const;
    
    // Get camera position
    glm::vec3 getPosition() const;
    glm::vec3 getTarget() const;
    
    // Reset to default view
    void reset();
    
    // Frame the model (auto-fit camera distance)
    void frameModel(const glm::vec3& center, float radius);
    
    // Mouse drag operations
    void startRotate(double mouseX, double mouseY);
    void startPan(double mouseX, double mouseY);
    void continueRotate(double mouseX, double mouseY, float sensitivity = 0.5f);
    void continuePan(double mouseX, double mouseY, float sensitivity = 0.01f);
    void endDrag();
    
private:
    OrbitCamera m_state;
    MouseState m_mouse;
    
    bool m_isRotating = false;
    bool m_isPanning = false;
    double m_dragStartX = 0.0;
    double m_dragStartY = 0.0;
};

} // namespace ese

#endif // ESE_CAMERA_H

