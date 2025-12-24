// EDEN ENGINE - Raycast Utilities
// Extracted from eden_vulkan_helpers.cpp for modularity

#ifndef EDEN_RAYCAST_H
#define EDEN_RAYCAST_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// AABB structure for ray-AABB intersection
struct AABB {
    glm::vec3 min;
    glm::vec3 max;
    
    AABB() : min(0.0f), max(0.0f) {}
    AABB(glm::vec3 min, glm::vec3 max) : min(min), max(max) {}
};

// Convert mouse screen position to normalized device coordinates (NDC)
// NDC: x,y in [-1, 1]
// Vulkan NDC: Y=1 is top, Y=-1 is bottom (Y points down)
// GLFW screen: Y=0 is top, Y=height is bottom
glm::vec2 screenToNDC(float screenX, float screenY, int width, int height);

// Unproject: Convert NDC coordinates to world-space ray
// Returns ray origin and direction via output parameters
// Requires current camera position for ray origin
void unproject(glm::vec2 ndc, glm::mat4 invProj, glm::mat4 invView, 
               const glm::vec3& cameraPos, glm::vec3& rayOrigin, glm::vec3& rayDir);

// Ray-AABB intersection using Möller-Trumbore slab method
// Returns true if ray hits AABB, and t (distance along ray) if hit
// tMin is the entry point, tMax is the exit point
bool rayAABB(const glm::vec3& rayOrigin, const glm::vec3& rayDir, 
             const AABB& box, float& tMin, float& tMax);

// Create AABB for a cube at position (x,y,z) with dimensions (sx, sy, sz)
AABB createCubeAABB(float x, float y, float z, float sx, float sy, float sz);

// Ray-Triangle intersection (Möller-Trumbore algorithm)
// Returns true if ray hits triangle, t is distance along ray to hit point
bool rayTriangle(const glm::vec3& rayOrigin, const glm::vec3& rayDir,
                 const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2,
                 float& t, float& u, float& v);

// Ray-Plane intersection
// Returns true if ray hits plane, t is distance along ray to hit point
bool rayPlane(const glm::vec3& rayOrigin, const glm::vec3& rayDir,
              const glm::vec3& planePoint, const glm::vec3& planeNormal, float& t);

// Point-Line distance (for edge picking)
// Returns squared distance from point to line segment
float pointLineDistanceSq(const glm::vec3& point, 
                          const glm::vec3& lineStart, const glm::vec3& lineEnd);

// Point-to-screen projection
// Projects a world point to screen coordinates
glm::vec2 projectToScreen(const glm::vec3& worldPos, 
                          const glm::mat4& view, const glm::mat4& proj,
                          int screenWidth, int screenHeight, bool* inFront = nullptr);

#endif // EDEN_RAYCAST_H

