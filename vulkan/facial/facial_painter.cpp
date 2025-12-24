// ============================================================================
// FACIAL PAINTER - Implementation
// ============================================================================

#include "facial_painter.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <fstream>

// For DMap texture saving - use simple file I/O for now
// TODO: Add stb_image_write.h or use VulkanCore texture saving

namespace facial {

// ============================================================================
// Constructor / Destructor
// ============================================================================

FacialPainter::FacialPainter() {
}

FacialPainter::~FacialPainter() {
}

// ============================================================================
// Initialization
// ============================================================================

bool FacialPainter::init(const std::vector<glm::vec3>& positions,
                         const std::vector<glm::vec3>& normals,
                         const std::vector<glm::vec2>& uv0,
                         const std::vector<uint32_t>& indices) {
    if (positions.size() != normals.size() || positions.size() != uv0.size()) {
        std::cerr << "[FacialPainter] Mismatched vertex data sizes!" << std::endl;
        return false;
    }
    
    m_paintedVertices.clear();
    m_paintedVertices.resize(positions.size());
    
    for (size_t i = 0; i < positions.size(); ++i) {
        m_paintedVertices[i].basePosition = positions[i];
        m_paintedVertices[i].displacement = glm::vec3(0);
        m_paintedVertices[i].uv0 = uv0[i];
        m_paintedVertices[i].uv1 = glm::vec2(0);  // Will be generated
        m_paintedVertices[i].painted = false;
    }
    
    m_indices = indices;
    
    // Auto-generate UV1
    generateUV1(UV1Method::REGION_ATLAS);
    
    m_initialized = true;
    std::cout << "[FacialPainter] Initialized with " << positions.size() << " vertices" << std::endl;
    
    return true;
}

// ============================================================================
// Painting
// ============================================================================

bool FacialPainter::paintAt(const glm::vec3& worldPos, const glm::vec3& normal, const PaintBrush& brush) {
    if (!m_initialized || !brush.enabled) return false;
    
    m_brushPosition = worldPos;
    m_brushActive = true;
    
    // Find vertices within brush radius
    std::vector<size_t> vertexIndices;
    findVerticesInBrush(worldPos, brush.size, vertexIndices);
    
    if (vertexIndices.empty()) return false;
    
    // Store for undo
    PaintStroke stroke;
    stroke.vertexIndices = vertexIndices;
    for (size_t idx : vertexIndices) {
        stroke.oldDisplacements.push_back(m_paintedVertices[idx].displacement);
    }
    m_undoStack.push_back(stroke);
    if (m_undoStack.size() > MAX_UNDO_STEPS) {
        m_undoStack.erase(m_undoStack.begin());
    }
    
    // Apply displacement
    glm::vec3 displacement = brush.direction * brush.strength * 0.01f;  // Scale to reasonable units
    
    for (size_t idx : vertexIndices) {
        PaintedVertex& v = m_paintedVertices[idx];
        
        // Calculate distance from brush center
        float dist = glm::length(v.basePosition + v.displacement - worldPos);
        float falloff = calculateFalloff(dist, brush.size);
        
        // Apply displacement with falloff
        v.displacement += displacement * falloff;
        v.painted = true;
    }
    
    // Recalculate normals
    recalculateNormals();
    
    return true;
}

void FacialPainter::paintVertex(size_t vertexIndex, const glm::vec3& displacement) {
    if (vertexIndex >= m_paintedVertices.size()) return;
    
    m_paintedVertices[vertexIndex].displacement += displacement;
    m_paintedVertices[vertexIndex].painted = true;
    
    recalculateNormals();
}

void FacialPainter::clearPainting() {
    for (auto& v : m_paintedVertices) {
        v.displacement = glm::vec3(0);
        v.painted = false;
    }
    m_undoStack.clear();
    recalculateNormals();
}

void FacialPainter::undo() {
    if (m_undoStack.empty()) return;
    
    PaintStroke& stroke = m_undoStack.back();
    for (size_t i = 0; i < stroke.vertexIndices.size(); ++i) {
        size_t idx = stroke.vertexIndices[i];
        m_paintedVertices[idx].displacement = stroke.oldDisplacements[i];
    }
    m_undoStack.pop_back();
    
    recalculateNormals();
}

// ============================================================================
// UV1 Generation
// ============================================================================

void FacialPainter::generateUV1(UV1Method method) {
    if (m_paintedVertices.empty()) return;
    
    switch (method) {
        case UV1Method::COPY_UV0:
            // Simple: copy UV0
            for (auto& v : m_paintedVertices) {
                v.uv1 = v.uv0;
            }
            break;
            
        case UV1Method::REGION_ATLAS: {
            // Region-based atlas: analyze vertex positions to assign regions
            // This is a simplified version - in production, you'd use more sophisticated region detection
            
            for (auto& v : m_paintedVertices) {
                glm::vec3 pos = v.basePosition;
                
                // Simple region assignment based on Y position and X position
                // Jaw: low Y, center X
                // Lips: medium-low Y, center X
                // Cheeks: medium Y, sides
                // Brows: high Y, center X
                
                float u, v_coord;
                
                if (pos.y < -0.3f) {
                    // Jaw region (0.7-1.0, 0.0-0.3)
                    u = 0.85f + (pos.x + 0.5f) * 0.1f;  // Map X to 0.7-1.0
                    v_coord = 0.15f + (-pos.y + 0.3f) * 0.15f;  // Map Y to 0.0-0.3
                } else if (pos.y < 0.0f) {
                    // Lip region (0.6-0.9, 0.3-0.5)
                    u = 0.75f + (pos.x + 0.5f) * 0.1f;
                    v_coord = 0.4f + (-pos.y) * 0.1f;
                } else if (pos.y < 0.5f) {
                    // Cheek/Nose region (0.0-0.6, 0.4-0.7)
                    u = 0.3f + (pos.x + 0.5f) * 0.3f;
                    v_coord = 0.55f + (pos.y) * 0.15f;
                } else {
                    // Brow region (0.4-0.7, 0.7-1.0)
                    u = 0.55f + (pos.x + 0.5f) * 0.15f;
                    v_coord = 0.85f + (pos.y - 0.5f) * 0.15f;
                }
                
                v.uv1 = glm::vec2(glm::clamp(u, 0.0f, 1.0f), glm::clamp(v_coord, 0.0f, 1.0f));
            }
            break;
        }
        
        case UV1Method::SPHERICAL_PROJECTION: {
            // Simple spherical projection
            for (auto& v : m_paintedVertices) {
                glm::vec3 pos = glm::normalize(v.basePosition);
                float u = 0.5f + atan2(pos.x, pos.z) / (2.0f * 3.14159f);
                float v_coord = 0.5f - asin(pos.y) / 3.14159f;
                v.uv1 = glm::vec2(u, v_coord);
            }
            break;
        }
        
        case UV1Method::CUBE_PROJECTION: {
            // Cube map style projection (simplified)
            for (auto& v : m_paintedVertices) {
                glm::vec3 pos = glm::normalize(v.basePosition);
                float absX = fabs(pos.x);
                float absY = fabs(pos.y);
                float absZ = fabs(pos.z);
                
                float u, v_coord;
                if (absX >= absY && absX >= absZ) {
                    // X-dominant face
                    u = pos.x > 0 ? 0.75f : 0.25f;
                    v_coord = 0.5f + pos.y * 0.25f;
                } else if (absY >= absZ) {
                    // Y-dominant face
                    u = 0.5f + pos.x * 0.25f;
                    v_coord = pos.y > 0 ? 0.75f : 0.25f;
                } else {
                    // Z-dominant face
                    u = pos.z > 0 ? 0.5f : 1.0f;
                    v_coord = 0.5f + pos.y * 0.25f;
                }
                v.uv1 = glm::vec2(glm::clamp(u, 0.0f, 1.0f), glm::clamp(v_coord, 0.0f, 1.0f));
            }
            break;
        }
    }
    
    std::cout << "[FacialPainter] Generated UV1 coordinates using method " << static_cast<int>(method) << std::endl;
}

// ============================================================================
// DMap Baking
// ============================================================================

bool FacialPainter::bakeDMap(const std::string& outputPath, int resolution) {
    if (!m_initialized || m_paintedVertices.empty()) return false;
    
    // Create texture buffer (RGB)
    std::vector<uint8_t> pixels(resolution * resolution * 3, 128);  // Initialize to neutral gray
    
    // Maximum displacement for normalization (adjust based on your model scale)
    float maxDisplacement = 0.1f;  // 10cm max
    
    // Sample each vertex and write to texture
    int samples = 0;
    for (const auto& v : m_paintedVertices) {
        if (!v.painted && glm::length(v.displacement) < 0.0001f) continue;
        
        // Convert UV1 to pixel coordinates
        int u = static_cast<int>(v.uv1.x * (resolution - 1));
        int v_coord = static_cast<int>((1.0f - v.uv1.y) * (resolution - 1));  // Flip V
        u = std::clamp(u, 0, resolution - 1);
        v_coord = std::clamp(v_coord, 0, resolution - 1);
        
        // Normalize displacement to -1..1 range
        glm::vec3 normalized = v.displacement / maxDisplacement;
        normalized = glm::clamp(normalized, -1.0f, 1.0f);
        
        // Convert to 0..255
        int r = static_cast<int>((normalized.x + 1.0f) * 127.5f);
        int g = static_cast<int>((normalized.y + 1.0f) * 127.5f);
        int b = static_cast<int>((normalized.z + 1.0f) * 127.5f);
        
        // Write to texture (average if multiple vertices map to same pixel)
        int idx = (v_coord * resolution + u) * 3;
        if (pixels[idx] == 128 && pixels[idx+1] == 128 && pixels[idx+2] == 128) {
            // First sample at this pixel
            pixels[idx] = static_cast<uint8_t>(r);
            pixels[idx+1] = static_cast<uint8_t>(g);
            pixels[idx+2] = static_cast<uint8_t>(b);
        } else {
            // Average with existing
            pixels[idx] = (pixels[idx] + r) / 2;
            pixels[idx+1] = (pixels[idx+1] + g) / 2;
            pixels[idx+2] = (pixels[idx+2] + b) / 2;
        }
        samples++;
    }
    
    // Save as PPM (Portable Pixmap) - simple format, can be converted to PNG later
    // Or save as raw RGB data
    std::string ppmPath = outputPath;
    if (ppmPath.size() > 4 && ppmPath.substr(ppmPath.size() - 4) == ".png") {
        ppmPath = ppmPath.substr(0, ppmPath.size() - 4) + ".ppm";
    }
    
    std::ofstream file(ppmPath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[FacialPainter] Failed to open file for writing: " << ppmPath << std::endl;
        return false;
    }
    
    // Write PPM header
    file << "P6\n" << resolution << " " << resolution << "\n255\n";
    
    // Write pixel data
    file.write(reinterpret_cast<const char*>(pixels.data()), pixels.size());
    file.close();
    
    std::cout << "[FacialPainter] Baked DMap to " << ppmPath 
              << " (" << resolution << "x" << resolution << ", " << samples << " samples)" << std::endl;
    std::cout << "[FacialPainter] Note: Saved as PPM format. Convert to PNG with: convert " << ppmPath << " " << outputPath << std::endl;
    return true;
}

bool FacialPainter::getBakedDMap(std::vector<uint8_t>& pixels, uint32_t& width, uint32_t& height) {
    // Similar to bakeDMap but returns data instead of saving
    int resolution = 1024;
    width = resolution;
    height = resolution;
    pixels.resize(resolution * resolution * 3, 128);
    
    float maxDisplacement = 0.1f;
    
    for (const auto& v : m_paintedVertices) {
        if (!v.painted && glm::length(v.displacement) < 0.0001f) continue;
        
        int u = static_cast<int>(v.uv1.x * (resolution - 1));
        int v_coord = static_cast<int>((1.0f - v.uv1.y) * (resolution - 1));
        u = std::clamp(u, 0, resolution - 1);
        v_coord = std::clamp(v_coord, 0, resolution - 1);
        
        glm::vec3 normalized = glm::clamp(v.displacement / maxDisplacement, -1.0f, 1.0f);
        int r = static_cast<int>((normalized.x + 1.0f) * 127.5f);
        int g = static_cast<int>((normalized.y + 1.0f) * 127.5f);
        int b = static_cast<int>((normalized.z + 1.0f) * 127.5f);
        
        int idx = (v_coord * resolution + u) * 3;
        pixels[idx] = static_cast<uint8_t>(r);
        pixels[idx+1] = static_cast<uint8_t>(g);
        pixels[idx+2] = static_cast<uint8_t>(b);
    }
    
    return true;
}

// ============================================================================
// Mesh Updates
// ============================================================================

void FacialPainter::getDisplacedMesh(std::vector<glm::vec3>& positions,
                                     std::vector<glm::vec3>& normals,
                                     std::vector<glm::vec2>& uv0,
                                     std::vector<glm::vec2>& uv1) const {
    if (!m_initialized) return;
    
    positions.clear();
    normals.clear();
    uv0.clear();
    uv1.clear();
    
    for (const auto& v : m_paintedVertices) {
        positions.push_back(v.basePosition + v.displacement);
        normals.push_back(glm::normalize(v.basePosition + v.displacement - v.basePosition));  // Simplified
        uv0.push_back(v.uv0);
        uv1.push_back(v.uv1);
    }
}

bool FacialPainter::updateGPUMesh(vkcore::VulkanCore* core, vkcore::MeshHandle meshHandle) {
    if (!m_initialized || !core) return false;
    
    // Get displaced mesh data
    std::vector<glm::vec3> positions, normals;
    std::vector<glm::vec2> uv0, uv1;
    getDisplacedMesh(positions, normals, uv0, uv1);
    
    // Create interleaved vertex data (POSITION_NORMAL_UV0_UV1 format)
    std::vector<float> vertexData;
    for (size_t i = 0; i < positions.size(); ++i) {
        // Position
        vertexData.push_back(positions[i].x);
        vertexData.push_back(positions[i].y);
        vertexData.push_back(positions[i].z);
        // Normal
        vertexData.push_back(normals[i].x);
        vertexData.push_back(normals[i].y);
        vertexData.push_back(normals[i].z);
        // UV0
        vertexData.push_back(uv0[i].x);
        vertexData.push_back(uv0[i].y);
        // UV1
        vertexData.push_back(uv1[i].x);
        vertexData.push_back(uv1[i].y);
    }
    
    // TODO: Update mesh buffer in VulkanCore
    // This requires adding an updateMesh() method to VulkanCore
    // For now, we'll need to recreate the mesh
    
    return true;
}

// ============================================================================
// Helpers
// ============================================================================

void FacialPainter::findVerticesInBrush(const glm::vec3& center, float radius,
                                        std::vector<size_t>& outIndices) {
    outIndices.clear();
    float radiusSq = radius * radius;
    
    for (size_t i = 0; i < m_paintedVertices.size(); ++i) {
        const auto& v = m_paintedVertices[i];
        glm::vec3 pos = v.basePosition + v.displacement;
        glm::vec3 diff = pos - center;
        float distSq = glm::dot(diff, diff);  // Squared distance
        
        if (distSq <= radiusSq) {
            outIndices.push_back(i);
        }
    }
}

float FacialPainter::calculateFalloff(float distance, float radius) {
    if (distance >= radius) return 0.0f;
    
    // Smooth falloff (smoothstep)
    float t = 1.0f - (distance / radius);
    return t * t * (3.0f - 2.0f * t);  // Smoothstep function
}

void FacialPainter::recalculateNormals() {
    // Simple normal recalculation from triangles
    // In production, you'd want more sophisticated normal calculation
    
    // Reset normals
    for (auto& v : m_paintedVertices) {
        // Normals will be recalculated from face normals
    }
    
    // Calculate face normals and accumulate
    std::vector<glm::vec3> normals(m_paintedVertices.size(), glm::vec3(0));
    
    for (size_t i = 0; i < m_indices.size(); i += 3) {
        uint32_t i0 = m_indices[i];
        uint32_t i1 = m_indices[i + 1];
        uint32_t i2 = m_indices[i + 2];
        
        glm::vec3 p0 = m_paintedVertices[i0].basePosition + m_paintedVertices[i0].displacement;
        glm::vec3 p1 = m_paintedVertices[i1].basePosition + m_paintedVertices[i1].displacement;
        glm::vec3 p2 = m_paintedVertices[i2].basePosition + m_paintedVertices[i2].displacement;
        
        glm::vec3 faceNormal = glm::normalize(glm::cross(p1 - p0, p2 - p0));
        
        normals[i0] += faceNormal;
        normals[i1] += faceNormal;
        normals[i2] += faceNormal;
    }
    
    // Normalize
    for (size_t i = 0; i < normals.size(); ++i) {
        if (glm::length(normals[i]) > 0.0001f) {
            normals[i] = glm::normalize(normals[i]);
        } else {
            normals[i] = glm::vec3(0, 1, 0);  // Default up
        }
    }
}

size_t FacialPainter::getPaintedCount() const {
    size_t count = 0;
    for (const auto& v : m_paintedVertices) {
        if (v.painted || glm::length(v.displacement) > 0.0001f) {
            count++;
        }
    }
    return count;
}

} // namespace facial

