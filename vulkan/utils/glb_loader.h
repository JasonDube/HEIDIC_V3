// ============================================================================
// GLB/GLTF Loader - Clean module for loading GLTF 2.0 models
// ============================================================================
// Uses cgltf (header-only library) for parsing
// Requires: third_party/cgltf/cgltf.h from https://github.com/jkuhlmann/cgltf
// ============================================================================

#ifndef GLB_LOADER_H
#define GLB_LOADER_H

#include <string>
#include <vector>
#include <glm/glm.hpp>

namespace eden {

// ============================================================================
// Loaded texture data from GLB/GLTF
// ============================================================================

struct GLBTexture {
    std::string name;
    std::vector<uint8_t> pixels;  // RGBA pixels
    uint32_t width = 0;
    uint32_t height = 0;
    bool valid = false;
};

// ============================================================================
// Loaded mesh data from GLB/GLTF
// ============================================================================

struct GLBVertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoord;   // UV0 - standard texture coordinates
    glm::vec2 texCoord1;  // UV1 - DMap coordinates (for facial animation)
    glm::vec4 color;      // Vertex color (if present, else white)
};

struct GLBMesh {
    std::string name;
    std::vector<GLBVertex> vertices;
    std::vector<uint32_t> indices;
    int textureIndex = -1;  // Index into GLBModel::textures, or -1 if none
    bool hasNormals = false;  // True if GLB file contained normal data
    bool hasUV1 = false;      // True if GLB file contained TEXCOORD_1 (UV1 for DMap)
};

struct GLBModel {
    std::string name;
    std::vector<GLBMesh> meshes;
    std::vector<GLBTexture> textures;  // Embedded textures
    
    // Stats
    size_t totalVertices() const;
    size_t totalTriangles() const;
    
    void clear();
};

// ============================================================================
// Loader functions
// ============================================================================

// Load a GLB or GLTF file
// Returns true on success, fills 'model' with mesh data
bool loadGLB(const std::string& path, GLBModel& model);

// Check if cgltf library is available
bool isGLBSupported();

} // namespace eden

#endif // GLB_LOADER_H

