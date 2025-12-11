// EDEN ENGINE - OBJ File Loader
// Parses Wavefront OBJ format (ASCII) - supports vertices, normals, texture coordinates, and faces
// Handles textured OBJs with UV coordinates

#ifndef EDEN_OBJ_LOADER_H
#define EDEN_OBJ_LOADER_H

#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <cstdlib>

// Mesh data structure - stores parsed OBJ data
struct MeshData {
    std::vector<float> positions;   // Vec3 per vertex (x, y, z)
    std::vector<float> normals;     // Vec3 per vertex (nx, ny, nz) - optional
    std::vector<float> texcoords;   // Vec2 per vertex (u, v) - optional (for textured OBJs)
    std::vector<uint32_t> indices;  // Index buffer (triangles)
    
    // Metadata
    bool hasNormals = false;
    bool hasTexcoords = false;
    uint32_t vertexCount = 0;
    uint32_t indexCount = 0;
};

/**
 * Load OBJ file and parse into MeshData structure
 * 
 * Supports:
 * - `v` lines (vertices)
 * - `vn` lines (normals)
 * - `vt` lines (texture coordinates - for textured OBJs)
 * - `f` lines (faces) with formats:
 *   - `f 1 2 3` (position only)
 *   - `f 1/1 2/2 3/3` (position + UV)
 *   - `f 1/1/1 2/2/2 3/3/3` (position + UV + normal)
 * 
 * @param filepath Path to OBJ file
 * @return MeshData structure with parsed mesh data
 * @throws std::runtime_error if file cannot be opened or parsing fails
 */
inline MeshData load_obj(const std::string& filepath) {
    MeshData result;
    
    std::ifstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open OBJ file: " + filepath);
    }
    
    // Temporary storage for OBJ data (1-indexed, as OBJ format uses)
    std::vector<float> tempPositions;   // Vec3
    std::vector<float> tempNormals;     // Vec3
    std::vector<float> tempTexcoords;   // Vec2
    
    std::string line;
    uint32_t lineNumber = 0;
    
    while (std::getline(file, line)) {
        lineNumber++;
        
        // Trim whitespace
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);
        
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') {
            continue;
        }
        
        // Parse line type
        std::istringstream iss(line);
        std::string type;
        iss >> type;
        
        if (type == "v") {
            // Vertex position: v x y z [w]
            float x, y, z, w = 1.0f;
            if (iss >> x >> y >> z) {
                tempPositions.push_back(x);
                tempPositions.push_back(y);
                tempPositions.push_back(z);
                // Optional w component (homogeneous coordinate) - we ignore it
            }
        } else if (type == "vn") {
            // Vertex normal: vn nx ny nz
            float nx, ny, nz;
            if (iss >> nx >> ny >> nz) {
                tempNormals.push_back(nx);
                tempNormals.push_back(ny);
                tempNormals.push_back(nz);
                result.hasNormals = true;
            }
        } else if (type == "vt") {
            // Texture coordinate: vt u v [w]
            // OBJ format uses bottom-left origin (0,0 at bottom-left)
            // Vulkan/OpenGL use top-left origin (0,0 at top-left)
            // So we need to flip V coordinate: v_flipped = 1.0 - v
            float u, v, w = 0.0f;
            if (iss >> u >> v) {
                tempTexcoords.push_back(u);
                tempTexcoords.push_back(1.0f - v); // Flip V for Vulkan coordinate system
                result.hasTexcoords = true;
                // Optional w component - we ignore it
            }
        } else if (type == "f") {
            // Face: f v1[/vt1][/vn1] v2[/vt2][/vn2] v3[/vt3][/vn3] [v4...]
            // OBJ uses 1-based indexing, we need to convert to 0-based
            
            std::vector<std::string> faceTokens;
            std::string token;
            while (iss >> token) {
                faceTokens.push_back(token);
            }
            
            if (faceTokens.size() < 3) {
                continue; // Invalid face (need at least 3 vertices)
            }
            
            // Parse face indices (handle different formats)
            auto parseFaceIndex = [](const std::string& token, int32_t& posIdx, int32_t& texIdx, int32_t& normIdx) -> bool {
                posIdx = -1;
                texIdx = -1;
                normIdx = -1;
                
                try {
                    // Count slashes to determine format
                    size_t slash1 = token.find('/');
                    if (slash1 == std::string::npos) {
                        // Format: f 1 2 3 (position only)
                        posIdx = std::stoi(token) - 1; // Convert to 0-based
                    } else {
                        // Has at least one slash
                        posIdx = std::stoi(token.substr(0, slash1)) - 1;
                        
                        size_t slash2 = token.find('/', slash1 + 1);
                        if (slash2 == std::string::npos) {
                            // Format: f 1/1 2/2 3/3 (position + UV)
                            if (slash1 + 1 < token.length()) {
                                std::string texStr = token.substr(slash1 + 1);
                                if (!texStr.empty()) {
                                    texIdx = std::stoi(texStr) - 1;
                                }
                            }
                        } else {
                            // Format: f 1/1/1 2/2/2 3/3/3 (position + UV + normal)
                            // OR: f 1//3 (position + normal, no UV)
                            std::string texStr = token.substr(slash1 + 1, slash2 - slash1 - 1);
                            if (!texStr.empty()) {
                                texIdx = std::stoi(texStr) - 1;
                            }
                            // If texStr is empty, texIdx stays -1 (no UV)
                            if (slash2 + 1 < token.length()) {
                                std::string normStr = token.substr(slash2 + 1);
                                if (!normStr.empty()) {
                                    normIdx = std::stoi(normStr) - 1;
                                }
                            }
                        }
                    }
                    return true;
                } catch (...) {
                    // Failed to parse - invalid format
                    return false;
                }
            };
            
            // Triangulate face (if quad or higher, split into triangles)
            // For now, we'll just use the first 3 vertices (simple triangulation)
            // TODO: Proper fan triangulation for n-gons
            
            // Parse first 3 vertices
            int32_t v1_pos, v1_tex, v1_norm;
            int32_t v2_pos, v2_tex, v2_norm;
            int32_t v3_pos, v3_tex, v3_norm;
            
            if (!parseFaceIndex(faceTokens[0], v1_pos, v1_tex, v1_norm) ||
                !parseFaceIndex(faceTokens[1], v2_pos, v2_tex, v2_norm) ||
                !parseFaceIndex(faceTokens[2], v3_pos, v3_tex, v3_norm)) {
                continue; // Failed to parse face indices
            }
            
            // Validate indices
            uint32_t maxPos = tempPositions.size() / 3;
            if (v1_pos < 0 || v1_pos >= (int32_t)maxPos ||
                v2_pos < 0 || v2_pos >= (int32_t)maxPos ||
                v3_pos < 0 || v3_pos >= (int32_t)maxPos) {
                continue; // Invalid indices
            }
            
            // Create vertices (interleaved: pos, normal, texcoord)
            // We'll create a unique vertex for each face vertex (no index sharing for now)
            // This is simpler but uses more memory - can optimize later with vertex deduplication
            
            uint32_t baseIndex = result.positions.size() / 3;
            
            // Vertex 1
            result.positions.push_back(tempPositions[v1_pos * 3 + 0]);
            result.positions.push_back(tempPositions[v1_pos * 3 + 1]);
            result.positions.push_back(tempPositions[v1_pos * 3 + 2]);
            
            if (result.hasNormals && v1_norm >= 0 && v1_norm < (int32_t)(tempNormals.size() / 3)) {
                result.normals.push_back(tempNormals[v1_norm * 3 + 0]);
                result.normals.push_back(tempNormals[v1_norm * 3 + 1]);
                result.normals.push_back(tempNormals[v1_norm * 3 + 2]);
            } else {
                result.normals.push_back(0.0f);
                result.normals.push_back(0.0f);
                result.normals.push_back(1.0f); // Default normal (pointing up)
            }
            
            // Check if this face vertex has a valid UV index
            if (v1_tex >= 0 && v1_tex < (int32_t)(tempTexcoords.size() / 2)) {
                result.texcoords.push_back(tempTexcoords[v1_tex * 2 + 0]);
                result.texcoords.push_back(tempTexcoords[v1_tex * 2 + 1]);
                result.hasTexcoords = true; // Mark that we're using UVs
            } else {
                // No UV specified for this vertex, use default
                result.texcoords.push_back(0.0f);
                result.texcoords.push_back(0.0f);
            }
            
            // Vertex 2
            result.positions.push_back(tempPositions[v2_pos * 3 + 0]);
            result.positions.push_back(tempPositions[v2_pos * 3 + 1]);
            result.positions.push_back(tempPositions[v2_pos * 3 + 2]);
            
            if (result.hasNormals && v2_norm >= 0 && v2_norm < (int32_t)(tempNormals.size() / 3)) {
                result.normals.push_back(tempNormals[v2_norm * 3 + 0]);
                result.normals.push_back(tempNormals[v2_norm * 3 + 1]);
                result.normals.push_back(tempNormals[v2_norm * 3 + 2]);
            } else {
                result.normals.push_back(0.0f);
                result.normals.push_back(0.0f);
                result.normals.push_back(1.0f);
            }
            
            // Check if this face vertex has a valid UV index
            if (v2_tex >= 0 && v2_tex < (int32_t)(tempTexcoords.size() / 2)) {
                result.texcoords.push_back(tempTexcoords[v2_tex * 2 + 0]);
                result.texcoords.push_back(tempTexcoords[v2_tex * 2 + 1]);
                result.hasTexcoords = true; // Mark that we're using UVs
            } else {
                // No UV specified for this vertex, use default
                result.texcoords.push_back(1.0f);
                result.texcoords.push_back(0.0f);
            }
            
            // Vertex 3
            result.positions.push_back(tempPositions[v3_pos * 3 + 0]);
            result.positions.push_back(tempPositions[v3_pos * 3 + 1]);
            result.positions.push_back(tempPositions[v3_pos * 3 + 2]);
            
            if (result.hasNormals && v3_norm >= 0 && v3_norm < (int32_t)(tempNormals.size() / 3)) {
                result.normals.push_back(tempNormals[v3_norm * 3 + 0]);
                result.normals.push_back(tempNormals[v3_norm * 3 + 1]);
                result.normals.push_back(tempNormals[v3_norm * 3 + 2]);
            } else {
                result.normals.push_back(0.0f);
                result.normals.push_back(0.0f);
                result.normals.push_back(1.0f);
            }
            
            // Check if this face vertex has a valid UV index
            if (v3_tex >= 0 && v3_tex < (int32_t)(tempTexcoords.size() / 2)) {
                result.texcoords.push_back(tempTexcoords[v3_tex * 2 + 0]);
                result.texcoords.push_back(tempTexcoords[v3_tex * 2 + 1]);
                result.hasTexcoords = true; // Mark that we're using UVs
            } else {
                // No UV specified for this vertex, use default
                result.texcoords.push_back(0.5f);
                result.texcoords.push_back(1.0f);
            }
            
            // Add indices (simple sequential indexing for now)
            result.indices.push_back(baseIndex);
            result.indices.push_back(baseIndex + 1);
            result.indices.push_back(baseIndex + 2);
            
            // Handle quads (triangulate into 2 triangles)
            if (faceTokens.size() >= 4) {
                int32_t v4_pos, v4_tex, v4_norm;
                if (!parseFaceIndex(faceTokens[3], v4_pos, v4_tex, v4_norm) ||
                    v4_pos < 0 || v4_pos >= (int32_t)maxPos) {
                    continue; // Failed to parse 4th vertex or invalid index
                }
                
                {
                    // Add vertex 4
                    result.positions.push_back(tempPositions[v4_pos * 3 + 0]);
                    result.positions.push_back(tempPositions[v4_pos * 3 + 1]);
                    result.positions.push_back(tempPositions[v4_pos * 3 + 2]);
                    
                    if (result.hasNormals && v4_norm >= 0 && v4_norm < (int32_t)(tempNormals.size() / 3)) {
                        result.normals.push_back(tempNormals[v4_norm * 3 + 0]);
                        result.normals.push_back(tempNormals[v4_norm * 3 + 1]);
                        result.normals.push_back(tempNormals[v4_norm * 3 + 2]);
                    } else {
                        result.normals.push_back(0.0f);
                        result.normals.push_back(0.0f);
                        result.normals.push_back(1.0f);
                    }
                    
                    // Check if this face vertex has a valid UV index
                    if (v4_tex >= 0 && v4_tex < (int32_t)(tempTexcoords.size() / 2)) {
                        result.texcoords.push_back(tempTexcoords[v4_tex * 2 + 0]);
                        result.texcoords.push_back(tempTexcoords[v4_tex * 2 + 1]);
                        result.hasTexcoords = true; // Mark that we're using UVs
                    } else {
                        // No UV specified for this vertex, use default
                        result.texcoords.push_back(1.0f);
                        result.texcoords.push_back(1.0f);
                    }
                    
                    // Second triangle: v1, v3, v4
                    result.indices.push_back(baseIndex);
                    result.indices.push_back(baseIndex + 2);
                    result.indices.push_back(baseIndex + 3);
                }
            }
        }
        // Ignore other OBJ commands (mtllib, usemtl, o, g, s, etc.) for now
    }
    
    file.close();
    
    // Set metadata
    result.vertexCount = result.positions.size() / 3;
    result.indexCount = result.indices.size();
    
    if (result.vertexCount == 0) {
        throw std::runtime_error("OBJ file contains no vertices: " + filepath);
    }
    
    // Ensure normals array matches vertex count
    if (result.normals.size() != result.positions.size()) {
        result.normals.resize(result.positions.size(), 0.0f);
        result.hasNormals = false;
    }
    
    // Ensure texcoords array matches vertex count (don't destroy UV data!)
    // Only resize if we're missing UVs, but preserve what we have
    uint32_t expectedTexcoordCount = (result.positions.size() / 3) * 2;
    if (result.texcoords.size() < expectedTexcoordCount) {
        // Pad with zeros only if we're short
        result.texcoords.resize(expectedTexcoordCount, 0.0f);
    } else if (result.texcoords.size() > expectedTexcoordCount) {
        // Shouldn't happen, but trim if somehow we have too many
        result.texcoords.resize(expectedTexcoordCount);
    }
    // If sizes match, we keep the parsed UV coordinates as-is
    
    return result;
}

#endif // EDEN_OBJ_LOADER_H

