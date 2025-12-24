// EDEN ENGINE - Raycast Utilities Implementation
// Extracted from eden_vulkan_helpers.cpp for modularity

#include "raycast.h"
#include <cmath>
#include <algorithm>

glm::vec2 screenToNDC(float screenX, float screenY, int width, int height) {
    float ndcX = (2.0f * screenX / width) - 1.0f;
    // Map screen Y=0 (top) to NDC Y=-1 (top), screen Y=height (bottom) to NDC Y=1 (bottom)
    float ndcY = (2.0f * screenY / height) - 1.0f;
    return glm::vec2(ndcX, ndcY);
}

void unproject(glm::vec2 ndc, glm::mat4 invProj, glm::mat4 invView,
               const glm::vec3& cameraPos, glm::vec3& rayOrigin, glm::vec3& rayDir) {
    // NDC clip space: Z = -1 (near), Z = +1 (far)
    // This is correct for both OpenGL and Vulkan NDC coordinates
    glm::vec4 clipNear = glm::vec4(ndc.x, ndc.y, -1.0f, 1.0f);  // Near plane Z = -1
    glm::vec4 clipFar = glm::vec4(ndc.x, ndc.y, 1.0f, 1.0f);    // Far plane Z = +1
    
    // Transform to eye space (view space)
    glm::vec4 eyeNear = invProj * clipNear;
    eyeNear /= eyeNear.w;  // Perspective divide
    
    glm::vec4 eyeFar = invProj * clipFar;
    eyeFar /= eyeFar.w;  // Perspective divide
    
    // Transform to world space
    glm::vec4 worldNear = invView * eyeNear;
    glm::vec4 worldFar = invView * eyeFar;
    
    glm::vec3 worldNearPoint = glm::vec3(worldNear);
    glm::vec3 worldFarPoint = glm::vec3(worldFar);
    
    // Ray origin is camera position
    rayOrigin = cameraPos;
    
    // Ray direction: from near point to far point (normalized)
    glm::vec3 dirVec = worldFarPoint - worldNearPoint;
    float dirLen = glm::length(dirVec);
    if (dirLen > 0.0001f) {
        rayDir = dirVec / dirLen;
    } else {
        // Fallback: if near and far are too close, use direction from camera to far point
        rayDir = glm::normalize(worldFarPoint - cameraPos);
    }
}

bool rayAABB(const glm::vec3& rayOrigin, const glm::vec3& rayDir, 
             const AABB& box, float& tMin, float& tMax) {
    // Ensure ray direction is normalized
    glm::vec3 dir = glm::normalize(rayDir);
    
    // Handle division by zero by using a very small epsilon
    const float epsilon = 1e-6f;
    glm::vec3 invDir;
    invDir.x = (fabsf(dir.x) < epsilon) ? (dir.x >= 0.0f ? 1e6f : -1e6f) : (1.0f / dir.x);
    invDir.y = (fabsf(dir.y) < epsilon) ? (dir.y >= 0.0f ? 1e6f : -1e6f) : (1.0f / dir.y);
    invDir.z = (fabsf(dir.z) < epsilon) ? (dir.z >= 0.0f ? 1e6f : -1e6f) : (1.0f / dir.z);
    
    glm::vec3 t0 = (box.min - rayOrigin) * invDir;
    glm::vec3 t1 = (box.max - rayOrigin) * invDir;
    
    glm::vec3 tMinVec = glm::min(t0, t1);
    glm::vec3 tMaxVec = glm::max(t0, t1);
    
    tMin = glm::max(glm::max(tMinVec.x, tMinVec.y), tMinVec.z);
    tMax = glm::min(glm::min(tMaxVec.x, tMaxVec.y), tMaxVec.z);
    
    // Ray hits if tMax >= tMin AND tMax >= 0
    return (tMax >= tMin) && (tMax >= 0.0f);
}

AABB createCubeAABB(float x, float y, float z, float sx, float sy, float sz) {
    float halfSx = sx * 0.5f;
    float halfSy = sy * 0.5f;
    float halfSz = sz * 0.5f;
    
    glm::vec3 min(x - halfSx, y - halfSy, z - halfSz);
    glm::vec3 max(x + halfSx, y + halfSy, z + halfSz);
    return AABB(min, max);
}

bool rayTriangle(const glm::vec3& rayOrigin, const glm::vec3& rayDir,
                 const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2,
                 float& t, float& u, float& v) {
    const float EPSILON = 1e-6f;
    
    glm::vec3 edge1 = v1 - v0;
    glm::vec3 edge2 = v2 - v0;
    glm::vec3 h = glm::cross(rayDir, edge2);
    float a = glm::dot(edge1, h);
    
    if (a > -EPSILON && a < EPSILON) {
        return false; // Ray parallel to triangle
    }
    
    float f = 1.0f / a;
    glm::vec3 s = rayOrigin - v0;
    u = f * glm::dot(s, h);
    
    if (u < 0.0f || u > 1.0f) {
        return false;
    }
    
    glm::vec3 q = glm::cross(s, edge1);
    v = f * glm::dot(rayDir, q);
    
    if (v < 0.0f || u + v > 1.0f) {
        return false;
    }
    
    t = f * glm::dot(edge2, q);
    
    return t > EPSILON;
}

bool rayPlane(const glm::vec3& rayOrigin, const glm::vec3& rayDir,
              const glm::vec3& planePoint, const glm::vec3& planeNormal, float& t) {
    float denom = glm::dot(planeNormal, rayDir);
    
    if (fabsf(denom) < 1e-6f) {
        return false; // Ray parallel to plane
    }
    
    t = glm::dot(planePoint - rayOrigin, planeNormal) / denom;
    return t >= 0.0f;
}

float pointLineDistanceSq(const glm::vec3& point, 
                          const glm::vec3& lineStart, const glm::vec3& lineEnd) {
    glm::vec3 line = lineEnd - lineStart;
    float lineLengthSq = glm::dot(line, line);
    
    if (lineLengthSq < 1e-6f) {
        // Line is a point
        return glm::dot(point - lineStart, point - lineStart);
    }
    
    // Project point onto line, clamped to segment
    float t = std::max(0.0f, std::min(1.0f, glm::dot(point - lineStart, line) / lineLengthSq));
    glm::vec3 projection = lineStart + t * line;
    
    glm::vec3 diff = point - projection;
    return glm::dot(diff, diff);
}

glm::vec2 projectToScreen(const glm::vec3& worldPos, 
                          const glm::mat4& view, const glm::mat4& proj,
                          int screenWidth, int screenHeight, bool* inFront) {
    glm::vec4 clipPos = proj * view * glm::vec4(worldPos, 1.0f);
    
    if (inFront) {
        *inFront = clipPos.w > 0.0f;
    }
    
    if (fabsf(clipPos.w) < 1e-6f) {
        return glm::vec2(-1000.0f, -1000.0f); // Off-screen
    }
    
    // Perspective divide to get NDC
    glm::vec3 ndc = glm::vec3(clipPos) / clipPos.w;
    
    // Convert NDC to screen coordinates
    float screenX = (ndc.x + 1.0f) * 0.5f * screenWidth;
    float screenY = (ndc.y + 1.0f) * 0.5f * screenHeight;
    
    return glm::vec2(screenX, screenY);
}

