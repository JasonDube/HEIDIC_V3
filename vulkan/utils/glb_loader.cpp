// ============================================================================
// GLB/GLTF Loader - Implementation
// ============================================================================

#include "glb_loader.h"
#include <iostream>
#include <fstream>

// stb_image for decoding embedded textures (already defined in vulkan_core.cpp)
#include "../../stdlib/stb_image.h"

// Check if cgltf is available
#if __has_include("../../third_party/cgltf/cgltf.h")
#define CGLTF_IMPLEMENTATION
#include "../../third_party/cgltf/cgltf.h"
#define HAS_CGLTF 1
#else
#define HAS_CGLTF 0
#endif

namespace eden {

// ============================================================================
// GLBModel methods
// ============================================================================

size_t GLBModel::totalVertices() const {
    size_t total = 0;
    for (const auto& mesh : meshes) {
        total += mesh.vertices.size();
    }
    return total;
}

size_t GLBModel::totalTriangles() const {
    size_t total = 0;
    for (const auto& mesh : meshes) {
        total += mesh.indices.size() / 3;
    }
    return total;
}

void GLBModel::clear() {
    name.clear();
    meshes.clear();
    textures.clear();
}

// ============================================================================
// Check if GLB is supported
// ============================================================================

bool isGLBSupported() {
#if HAS_CGLTF
    return true;
#else
    return false;
#endif
}

// ============================================================================
// GLB Loading
// ============================================================================

#if HAS_CGLTF

bool loadGLB(const std::string& path, GLBModel& model) {
    model.clear();
    
    std::cout << "[GLB] Loading: " << path << std::endl;
    
    // Parse the file
    cgltf_options options = {};
    cgltf_data* data = nullptr;
    
    cgltf_result result = cgltf_parse_file(&options, path.c_str(), &data);
    if (result != cgltf_result_success) {
        std::cerr << "[GLB] Failed to parse file" << std::endl;
        return false;
    }
    
    // Load buffers (required for binary data)
    result = cgltf_load_buffers(&options, data, path.c_str());
    if (result != cgltf_result_success) {
        std::cerr << "[GLB] Failed to load buffers" << std::endl;
        cgltf_free(data);
        return false;
    }
    
    // Extract model name from filename
    size_t lastSlash = path.find_last_of("/\\");
    size_t lastDot = path.find_last_of('.');
    if (lastSlash == std::string::npos) lastSlash = 0; else lastSlash++;
    if (lastDot == std::string::npos || lastDot < lastSlash) lastDot = path.length();
    model.name = path.substr(lastSlash, lastDot - lastSlash);
    
    // Extract textures
    std::cout << "[GLB] Found " << data->images_count << " images, " 
              << data->textures_count << " textures" << std::endl;
    
    for (size_t ti = 0; ti < data->images_count; ++ti) {
        cgltf_image& img = data->images[ti];
        GLBTexture tex;
        tex.name = img.name ? img.name : ("texture_" + std::to_string(ti));
        
        const uint8_t* imageData = nullptr;
        size_t imageSize = 0;
        
        if (img.buffer_view) {
            // Embedded in buffer
            imageData = static_cast<const uint8_t*>(img.buffer_view->buffer->data) + img.buffer_view->offset;
            imageSize = img.buffer_view->size;
        } else if (img.uri) {
            // External file or data URI - push invalid texture to preserve indices
            std::cout << "[GLB] External image (not supported): " << img.uri << std::endl;
            tex.valid = false;
            model.textures.push_back(std::move(tex));
            continue;
        }
        
        if (imageData && imageSize > 0) {
            // Decode with stb_image
            int w, h, channels;
            stbi_uc* pixels = stbi_load_from_memory(imageData, static_cast<int>(imageSize), &w, &h, &channels, STBI_rgb_alpha);
            if (pixels) {
                tex.width = static_cast<uint32_t>(w);
                tex.height = static_cast<uint32_t>(h);
                tex.pixels.resize(w * h * 4);
                memcpy(tex.pixels.data(), pixels, tex.pixels.size());
                tex.valid = true;
                stbi_image_free(pixels);
                std::cout << "[GLB] Loaded texture: " << tex.name << " (" << w << "x" << h << ")" << std::endl;
            } else {
                std::cerr << "[GLB] Failed to decode texture: " << tex.name << std::endl;
            }
        }
        
        model.textures.push_back(std::move(tex));
    }
    
    // Process all meshes
    for (size_t mi = 0; mi < data->meshes_count; ++mi) {
        cgltf_mesh& gltfMesh = data->meshes[mi];
        
        for (size_t pi = 0; pi < gltfMesh.primitives_count; ++pi) {
            cgltf_primitive& prim = gltfMesh.primitives[pi];
            
            // Only handle triangles
            if (prim.type != cgltf_primitive_type_triangles) {
                continue;
            }
            
            GLBMesh mesh;
            mesh.name = gltfMesh.name ? gltfMesh.name : ("mesh_" + std::to_string(mi));
            mesh.textureIndex = -1;
            mesh.hasNormals = false;  // Will be set true if normal accessor found
            
            // Get texture from material
            if (prim.material) {
                cgltf_material* mat = prim.material;
                // Check PBR base color texture
                if (mat->has_pbr_metallic_roughness && 
                    mat->pbr_metallic_roughness.base_color_texture.texture) {
                    cgltf_texture* tex = mat->pbr_metallic_roughness.base_color_texture.texture;
                    if (tex->image) {
                        mesh.textureIndex = static_cast<int>(tex->image - data->images);
                    }
                }
            }
            
            // Find attributes
            cgltf_accessor* posAccessor = nullptr;
            cgltf_accessor* normAccessor = nullptr;
            cgltf_accessor* uvAccessor = nullptr;   // TEXCOORD_0
            cgltf_accessor* uv1Accessor = nullptr;  // TEXCOORD_1 (for DMap facial animation)
            cgltf_accessor* colorAccessor = nullptr;
            
            for (size_t ai = 0; ai < prim.attributes_count; ++ai) {
                cgltf_attribute& attr = prim.attributes[ai];
                if (attr.type == cgltf_attribute_type_position) posAccessor = attr.data;
                else if (attr.type == cgltf_attribute_type_normal) normAccessor = attr.data;
                else if (attr.type == cgltf_attribute_type_texcoord) {
                    // TEXCOORD_0 or TEXCOORD_1 based on index
                    if (attr.index == 0) uvAccessor = attr.data;
                    else if (attr.index == 1) uv1Accessor = attr.data;
                }
                else if (attr.type == cgltf_attribute_type_color) colorAccessor = attr.data;
            }
            
            if (!posAccessor) {
                std::cerr << "[GLB] Mesh has no position data, skipping" << std::endl;
                continue;
            }
            
            // Track if we have actual normal data
            mesh.hasNormals = (normAccessor != nullptr);
            if (!mesh.hasNormals) {
                std::cout << "[GLB] Mesh '" << mesh.name << "' has no normal data - will need generation" << std::endl;
            }
            
            // Track if we have UV1 (for DMap facial animation)
            mesh.hasUV1 = (uv1Accessor != nullptr);
            if (mesh.hasUV1) {
                std::cout << "[GLB] Mesh '" << mesh.name << "' has TEXCOORD_1 (UV1 for DMap)" << std::endl;
            }
            
            // Read vertices
            size_t vertexCount = posAccessor->count;
            mesh.vertices.resize(vertexCount);
            
            for (size_t vi = 0; vi < vertexCount; ++vi) {
                GLBVertex& v = mesh.vertices[vi];
                
                // Position (required)
                cgltf_accessor_read_float(posAccessor, vi, &v.position.x, 3);
                
                // Normal (optional)
                if (normAccessor) {
                    cgltf_accessor_read_float(normAccessor, vi, &v.normal.x, 3);
                } else {
                    v.normal = glm::vec3(0, 1, 0);
                }
                
                // UV0 (optional)
                if (uvAccessor) {
                    cgltf_accessor_read_float(uvAccessor, vi, &v.texCoord.x, 2);
                } else {
                    v.texCoord = glm::vec2(0);
                }
                
                // UV1 - DMap coordinates (optional)
                if (uv1Accessor) {
                    cgltf_accessor_read_float(uv1Accessor, vi, &v.texCoord1.x, 2);
                } else {
                    v.texCoord1 = glm::vec2(0);
                }
                
                // Vertex color (optional)
                if (colorAccessor) {
                    cgltf_accessor_read_float(colorAccessor, vi, &v.color.x, 4);
                } else {
                    v.color = glm::vec4(1.0f);  // Default white
                }
            }
            
            // Read indices
            if (prim.indices) {
                mesh.indices.resize(prim.indices->count);
                for (size_t ii = 0; ii < prim.indices->count; ++ii) {
                    mesh.indices[ii] = static_cast<uint32_t>(cgltf_accessor_read_index(prim.indices, ii));
                }
            } else {
                // No indices - generate sequential
                mesh.indices.resize(vertexCount);
                for (size_t ii = 0; ii < vertexCount; ++ii) {
                    mesh.indices[ii] = static_cast<uint32_t>(ii);
                }
            }
            
            model.meshes.push_back(std::move(mesh));
        }
    }
    
    cgltf_free(data);
    
    std::cout << "[GLB] Loaded: " << model.meshes.size() << " meshes, " 
              << model.totalVertices() << " verts, " 
              << model.totalTriangles() << " tris" << std::endl;
    
    return !model.meshes.empty();
}

#else

// Stub when cgltf is not available
bool loadGLB(const std::string& path, GLBModel& model) {
    (void)path;
    model.clear();
    std::cerr << "[GLB] GLB loading not available - cgltf library not found" << std::endl;
    std::cerr << "[GLB] Download cgltf.h from https://github.com/jkuhlmann/cgltf" << std::endl;
    std::cerr << "[GLB] Place in: third_party/cgltf/cgltf.h" << std::endl;
    return false;
}

#endif

} // namespace eden

