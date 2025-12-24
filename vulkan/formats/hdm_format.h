// EDEN ENGINE - HDM File Format
// Extracted from eden_vulkan_helpers.cpp for modularity
// HDM (HEIDIC Dynamic Model) format for 3D models with metadata

#ifndef EDEN_HDM_FORMAT_H
#define EDEN_HDM_FORMAT_H

#include <vector>
#include <string>
#include <cstdint>
#include <cstring>

// ============================================================================
// HDM Data Structures
// ============================================================================

// HDM File Header (binary format)
struct HDMHeader {
    char magic[4] = {'H', 'D', 'M', '\0'};  // File identifier
    uint32_t version = 2;                    // Format version
    uint32_t props_offset;                   // Offset to properties section
    uint32_t props_size;                     // Size of properties section
    uint32_t geometry_offset;                // Offset to geometry section
    uint32_t geometry_size;                  // Size of geometry section
    uint32_t texture_offset;                 // Offset to texture section
    uint32_t texture_size;                   // Size of texture section
};

// Model reference properties
struct HDMModelProperties {
    char obj_path[256] = {0};               // Original OBJ file path
    char texture_path[256] = {0};           // Texture file path
    float scale[3] = {1.0f, 1.0f, 1.0f};   // Model scale
    float origin_offset[3] = {0.0f, 0.0f, 0.0f};  // Origin offset
};

// Item properties (for game items)
struct HDMItemProperties {
    int item_type_id = 0;                   // Item type identifier
    char item_name[64] = {0};               // Display name
    int trade_value = 0;                    // Base trade value
    float condition = 1.0f;                 // Item condition (0-1)
    float weight = 1.0f;                    // Item weight
    int category = 0;                       // Item category
    bool is_salvaged = false;               // Salvaged flag
    int mesh_class = 0;                     // Mesh class (0=head, 1=body, 2=attachment, 3=prop)
};

// Physics properties
struct HDMPhysicsProperties {
    int collision_type = 1;                 // 0=sphere, 1=box, 2=convex
    float collision_bounds[3] = {1.0f, 1.0f, 1.0f};  // Collision bounds
    bool is_static = true;                  // Static object flag
    float mass = 1.0f;                      // Object mass (for dynamic objects)
};

// Control point (attachment point)
struct HDMControlPoint {
    char name[32] = {0};                    // Point name
    float position[3] = {0.0f, 0.0f, 0.0f}; // Position relative to origin
};

// Complete HDM Properties
struct HDMProperties {
    char hdm_version[16] = "2.0";           // HDM version string
    HDMModelProperties model;                // Model reference
    HDMItemProperties item;                  // Item properties
    HDMPhysicsProperties physics;            // Physics properties
    int num_control_points = 0;              // Number of control points
    HDMControlPoint control_points[8];       // Up to 8 control points
};

// HDM Vertex format
struct HDMVertex {
    float position[3];                       // Position
    float normal[3];                         // Normal
    float texcoord[2];                       // Texture coordinate
};

// HDM Geometry data
struct HDMGeometry {
    std::vector<HDMVertex> vertices;
    std::vector<uint32_t> indices;
};

// HDM Texture data (embedded)
struct HDMTexture {
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t format = 0;                    // 0 = RGBA8
    std::vector<unsigned char> data;
};

// ============================================================================
// C-Style API Structures (for HEIDIC interop)
// ============================================================================

struct HDMItemPropertiesC {
    int item_type_id;
    char item_name[64];
    int trade_value;
    float condition;
    float weight;
    int category;
    int is_salvaged;
};

struct HDMPhysicsPropertiesC {
    int collision_type;
    float collision_bounds_x;
    float collision_bounds_y;
    float collision_bounds_z;
    int is_static;
    float mass;
};

struct HDMModelPropertiesC {
    char obj_path[256];
    char texture_path[256];
    float scale_x;
    float scale_y;
    float scale_z;
};

// ============================================================================
// HDM Format Functions
// ============================================================================

namespace eden {
namespace hdm {

// Initialize properties with default values
void initDefaultProperties(HDMProperties& props);

// Save HDM to binary format (.hdm)
bool saveBinary(const char* filepath, const HDMProperties& props,
                const HDMGeometry& geom, const HDMTexture& tex);

// Load HDM from binary format (.hdm)
bool loadBinary(const char* filepath, HDMProperties& props,
                HDMGeometry& geom, HDMTexture& tex);

// Save HDM to ASCII/JSON format (.hdma)
bool saveAscii(const char* filepath, const HDMProperties& props,
               const HDMGeometry& geom, const HDMTexture& tex);

// Load HDM from ASCII/JSON format (.hdma)
bool loadAscii(const char* filepath, HDMProperties& props,
               HDMGeometry& geom, HDMTexture& tex);

// Save only properties to JSON format (no geometry/texture)
bool savePropertiesJson(const char* filepath, const HDMProperties& props);

// Load only properties from JSON format (no geometry/texture)
bool loadPropertiesJson(const char* filepath, HDMProperties& props);

// Determine file format from extension and load accordingly
bool load(const char* filepath, HDMProperties& props,
          HDMGeometry& geom, HDMTexture& tex);

// Determine file format from extension and save accordingly
bool save(const char* filepath, const HDMProperties& props,
          const HDMGeometry& geom, const HDMTexture& tex);

// Base64 utilities
std::string base64Encode(const unsigned char* data, size_t len);
std::vector<unsigned char> base64Decode(const std::string& encoded);

// JSON parsing utilities
std::string jsonFindString(const std::string& json, const std::string& key);
int jsonFindInt(const std::string& json, const std::string& key);
float jsonFindFloat(const std::string& json, const std::string& key);
bool jsonFindBool(const std::string& json, const std::string& key);
void jsonFindFloatArray(const std::string& json, const std::string& key, float* out, int count);

} // namespace hdm
} // namespace eden

// ============================================================================
// C-Style API (for HEIDIC interop)
// ============================================================================

#ifdef __cplusplus
extern "C" {
#endif

// Load HDM file (any format)
int hdm_load_file(const char* filepath);

// Get loaded properties
HDMItemPropertiesC hdm_get_item_properties();
HDMPhysicsPropertiesC hdm_get_physics_properties();
HDMModelPropertiesC hdm_get_model_properties();

// Save HDM file
int hdm_save_file(const char* filepath);

#ifdef __cplusplus
}
#endif

#endif // EDEN_HDM_FORMAT_H

