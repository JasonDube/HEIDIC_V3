// EDEN ENGINE - HDM File Format Implementation
// Extracted from eden_vulkan_helpers.cpp for modularity

#include "hdm_format.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <cmath>
#include <algorithm>
#include <cctype>
#include <cstring>

namespace eden {
namespace hdm {

// ============================================================================
// Base64 Encoding/Decoding
// ============================================================================

static const char base64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static inline bool is_base64_char(unsigned char c) {
    return (isalnum(c) || (c == '+') || (c == '/'));
}

std::string base64Encode(const unsigned char* data, size_t len) {
    std::string ret;
    int i = 0;
    int j = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];
    
    while (len--) {
        char_array_3[i++] = *(data++);
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;
            
            for (i = 0; i < 4; i++) ret += base64_chars[char_array_4[i]];
            i = 0;
        }
    }
    
    if (i) {
        for (j = i; j < 3; j++) char_array_3[j] = '\0';
        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        for (j = 0; j < i + 1; j++) ret += base64_chars[char_array_4[j]];
        while (i++ < 3) ret += '=';
    }
    return ret;
}

std::vector<unsigned char> base64Decode(const std::string& encoded) {
    int in_len = encoded.size();
    int i = 0;
    int j = 0;
    int in = 0;
    unsigned char char_array_4[4], char_array_3[3];
    std::vector<unsigned char> ret;
    
    while (in_len-- && (encoded[in] != '=') && is_base64_char(encoded[in])) {
        char_array_4[i++] = encoded[in]; in++;
        if (i == 4) {
            for (i = 0; i < 4; i++) {
                const char* pos = strchr(base64_chars, char_array_4[i]);
                if (pos) char_array_4[i] = pos - base64_chars;
                else char_array_4[i] = 0;
            }
            char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];
            
            for (i = 0; i < 3; i++) ret.push_back(char_array_3[i]);
            i = 0;
        }
    }
    
    if (i) {
        for (j = 0; j < 4; j++) {
            const char* pos = strchr(base64_chars, char_array_4[j]);
            char_array_4[j] = pos ? (pos - base64_chars) : 0;
        }
        char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
        char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
        char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];
        for (j = 0; j < i - 1; j++) ret.push_back(char_array_3[j]);
    }
    return ret;
}

// ============================================================================
// JSON Parsing Utilities
// ============================================================================

std::string jsonFindString(const std::string& json, const std::string& key) {
    std::string searchKey = "\"" + key + "\":";
    size_t pos = json.find(searchKey);
    if (pos == std::string::npos) return "";
    
    pos = json.find("\"", pos + searchKey.length());
    if (pos == std::string::npos) return "";
    pos++;
    
    size_t endPos = json.find("\"", pos);
    if (endPos == std::string::npos) return "";
    
    return json.substr(pos, endPos - pos);
}

int jsonFindInt(const std::string& json, const std::string& key) {
    std::string searchKey = "\"" + key + "\":";
    size_t pos = json.find(searchKey);
    if (pos == std::string::npos) return 0;
    
    pos += searchKey.length();
    while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
    
    std::string numStr;
    while (pos < json.length() && (isdigit(json[pos]) || json[pos] == '-')) {
        numStr += json[pos++];
    }
    return numStr.empty() ? 0 : std::stoi(numStr);
}

float jsonFindFloat(const std::string& json, const std::string& key) {
    std::string searchKey = "\"" + key + "\":";
    size_t pos = json.find(searchKey);
    if (pos == std::string::npos) return 0.0f;
    
    pos += searchKey.length();
    while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
    
    std::string numStr;
    while (pos < json.length() && (isdigit(json[pos]) || json[pos] == '-' || 
           json[pos] == '.' || json[pos] == 'e' || json[pos] == 'E' || json[pos] == '+')) {
        numStr += json[pos++];
    }
    return numStr.empty() ? 0.0f : std::stof(numStr);
}

bool jsonFindBool(const std::string& json, const std::string& key) {
    std::string searchKey = "\"" + key + "\":";
    size_t pos = json.find(searchKey);
    if (pos == std::string::npos) return false;
    
    pos += searchKey.length();
    while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
    
    return json.substr(pos, 4) == "true";
}

void jsonFindFloatArray(const std::string& json, const std::string& key, float* out, int count) {
    std::string searchKey = "\"" + key + "\":";
    size_t pos = json.find(searchKey);
    if (pos == std::string::npos) return;
    
    pos = json.find("[", pos);
    if (pos == std::string::npos) return;
    pos++;
    
    for (int i = 0; i < count; i++) {
        while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == ',')) pos++;
        
        std::string numStr;
        while (pos < json.length() && (isdigit(json[pos]) || json[pos] == '-' || 
               json[pos] == '.' || json[pos] == 'e' || json[pos] == 'E' || json[pos] == '+')) {
            numStr += json[pos++];
        }
        if (!numStr.empty()) out[i] = std::stof(numStr);
    }
}

// ============================================================================
// Properties Initialization
// ============================================================================

void initDefaultProperties(HDMProperties& props) {
    memset(&props, 0, sizeof(props));
    strncpy(props.hdm_version, "2.0", sizeof(props.hdm_version) - 1);
    strncpy(props.item.item_name, "Unnamed", sizeof(props.item.item_name) - 1);
    props.item.condition = 1.0f;
    props.item.weight = 1.0f;
    props.item.mesh_class = 0;
    props.model.scale[0] = props.model.scale[1] = props.model.scale[2] = 1.0f;
    props.physics.collision_type = 1;
    props.physics.collision_bounds[0] = props.physics.collision_bounds[1] = props.physics.collision_bounds[2] = 1.0f;
    props.physics.is_static = true;
    props.physics.mass = 1.0f;
    props.num_control_points = 0;
}

// ============================================================================
// Binary Format
// ============================================================================

bool saveBinary(const char* filepath, const HDMProperties& props,
                const HDMGeometry& geom, const HDMTexture& tex) {
    std::ofstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[HDM] Failed to open file for writing: " << filepath << std::endl;
        return false;
    }
    
    // Sanitize properties
    HDMProperties sanitizedProps = props;
    if (sanitizedProps.num_control_points < 0 || sanitizedProps.num_control_points > 8) {
        sanitizedProps.num_control_points = 0;
    }
    
    // Calculate sizes
    size_t propsSize = sizeof(HDMProperties);
    size_t geomHeaderSize = sizeof(uint32_t) * 2;
    size_t geomVerticesSize = geom.vertices.size() * sizeof(HDMVertex);
    size_t geomIndicesSize = geom.indices.size() * sizeof(uint32_t);
    size_t geomTotalSize = geomHeaderSize + geomVerticesSize + geomIndicesSize;
    size_t texHeaderSize = sizeof(uint32_t) * 3;
    size_t texDataSize = tex.data.size();
    size_t texTotalSize = texHeaderSize + texDataSize;
    
    // Build header
    HDMHeader header;
    header.version = 2;
    header.props_offset = sizeof(HDMHeader);
    header.props_size = propsSize;
    header.geometry_offset = header.props_offset + propsSize;
    header.geometry_size = geomTotalSize;
    header.texture_offset = header.geometry_offset + geomTotalSize;
    header.texture_size = texTotalSize;
    
    // Write header
    file.write(reinterpret_cast<const char*>(&header), sizeof(header));
    
    // Write properties
    file.write(reinterpret_cast<const char*>(&sanitizedProps), sizeof(sanitizedProps));
    
    // Write geometry header
    uint32_t vertexCount = geom.vertices.size();
    uint32_t indexCount = geom.indices.size();
    file.write(reinterpret_cast<const char*>(&vertexCount), sizeof(vertexCount));
    file.write(reinterpret_cast<const char*>(&indexCount), sizeof(indexCount));
    
    // Write vertices
    if (!geom.vertices.empty()) {
        file.write(reinterpret_cast<const char*>(geom.vertices.data()), geomVerticesSize);
    }
    
    // Write indices
    if (!geom.indices.empty()) {
        file.write(reinterpret_cast<const char*>(geom.indices.data()), geomIndicesSize);
    }
    
    // Write texture header
    file.write(reinterpret_cast<const char*>(&tex.width), sizeof(tex.width));
    file.write(reinterpret_cast<const char*>(&tex.height), sizeof(tex.height));
    file.write(reinterpret_cast<const char*>(&tex.format), sizeof(tex.format));
    
    // Write texture data
    if (!tex.data.empty()) {
        file.write(reinterpret_cast<const char*>(tex.data.data()), texDataSize);
    }
    
    file.close();
    std::cout << "[HDM] Saved binary HDM: " << filepath << std::endl;
    return true;
}

bool loadBinary(const char* filepath, HDMProperties& props,
                HDMGeometry& geom, HDMTexture& tex) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[HDM] Failed to open file: " << filepath << std::endl;
        return false;
    }
    
    // Read header
    HDMHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(header));
    
    // Verify magic
    if (header.magic[0] != 'H' || header.magic[1] != 'D' || header.magic[2] != 'M') {
        std::cerr << "[HDM] Invalid HDM file (bad magic): " << filepath << std::endl;
        file.close();
        return false;
    }
    
    // Check version
    if (header.version < 2) {
        std::cerr << "[HDM] Old HDM format (v" << header.version << ")" << std::endl;
        file.close();
        return false;
    }
    
    // Read properties
    file.seekg(header.props_offset);
    file.read(reinterpret_cast<char*>(&props), sizeof(props));
    
    // Sanitize
    if (props.num_control_points < 0 || props.num_control_points > 8) {
        props.num_control_points = 0;
    }
    
    // Read geometry
    file.seekg(header.geometry_offset);
    uint32_t vertexCount, indexCount;
    file.read(reinterpret_cast<char*>(&vertexCount), sizeof(vertexCount));
    file.read(reinterpret_cast<char*>(&indexCount), sizeof(indexCount));
    
    geom.vertices.resize(vertexCount);
    geom.indices.resize(indexCount);
    
    if (vertexCount > 0) {
        file.read(reinterpret_cast<char*>(geom.vertices.data()), vertexCount * sizeof(HDMVertex));
    }
    if (indexCount > 0) {
        file.read(reinterpret_cast<char*>(geom.indices.data()), indexCount * sizeof(uint32_t));
    }
    
    // Read texture
    file.seekg(header.texture_offset);
    file.read(reinterpret_cast<char*>(&tex.width), sizeof(tex.width));
    file.read(reinterpret_cast<char*>(&tex.height), sizeof(tex.height));
    file.read(reinterpret_cast<char*>(&tex.format), sizeof(tex.format));
    
    size_t texDataSize = tex.width * tex.height * 4;
    if (texDataSize > 0) {
        tex.data.resize(texDataSize);
        file.read(reinterpret_cast<char*>(tex.data.data()), texDataSize);
    }
    
    file.close();
    std::cout << "[HDM] Loaded binary HDM: " << filepath << std::endl;
    return true;
}

// ============================================================================
// ASCII/JSON Format
// ============================================================================

bool saveAscii(const char* filepath, const HDMProperties& props,
               const HDMGeometry& geom, const HDMTexture& tex) {
    std::ofstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "[HDM] Failed to open file for writing: " << filepath << std::endl;
        return false;
    }
    
    file << "{\n";
    file << "  \"hdm_version\": \"2.0\",\n";
    file << "  \"format\": \"ascii\",\n\n";
    
    // Properties
    file << "  \"properties\": {\n";
    file << "    \"model\": {\n";
    file << "      \"obj_path\": \"" << props.model.obj_path << "\",\n";
    file << "      \"texture_path\": \"" << props.model.texture_path << "\",\n";
    file << "      \"scale\": [" << props.model.scale[0] << ", " << props.model.scale[1] << ", " << props.model.scale[2] << "],\n";
    file << "      \"origin_offset\": [" << props.model.origin_offset[0] << ", " << props.model.origin_offset[1] << ", " << props.model.origin_offset[2] << "]\n";
    file << "    },\n";
    
    file << "    \"item\": {\n";
    file << "      \"item_type_id\": " << props.item.item_type_id << ",\n";
    file << "      \"item_name\": \"" << props.item.item_name << "\",\n";
    file << "      \"trade_value\": " << props.item.trade_value << ",\n";
    file << "      \"condition\": " << props.item.condition << ",\n";
    file << "      \"weight\": " << props.item.weight << ",\n";
    file << "      \"category\": " << props.item.category << ",\n";
    file << "      \"is_salvaged\": " << (props.item.is_salvaged ? "true" : "false") << "\n";
    file << "    },\n";
    
    // Sanitize physics values
    int collisionType = props.physics.collision_type;
    if (collisionType < 0 || collisionType > 2) collisionType = 1;
    
    float bounds[3] = {
        props.physics.collision_bounds[0],
        props.physics.collision_bounds[1],
        props.physics.collision_bounds[2]
    };
    for (int i = 0; i < 3; i++) {
        if (std::isnan(bounds[i]) || std::isinf(bounds[i]) || bounds[i] < 0.001f || bounds[i] > 10000.0f) {
            bounds[i] = 1.0f;
        }
    }
    
    float mass = props.physics.mass;
    if (std::isnan(mass) || std::isinf(mass) || mass < 0.0f || mass > 100000.0f) {
        mass = 1.0f;
    }
    
    file << "    \"physics\": {\n";
    file << "      \"collision_type\": " << collisionType << ",\n";
    file << "      \"collision_bounds\": [" << bounds[0] << ", " << bounds[1] << ", " << bounds[2] << "],\n";
    file << "      \"is_static\": " << (props.physics.is_static ? "true" : "false") << ",\n";
    file << "      \"mass\": " << mass << "\n";
    file << "    },\n";
    
    // Control points
    file << "    \"control_points\": [\n";
    int numPoints = std::min(std::max(props.num_control_points, 0), 8);
    for (int i = 0; i < numPoints; i++) {
        file << "      {\"name\": \"" << props.control_points[i].name << "\", \"position\": [" 
             << props.control_points[i].position[0] << ", " 
             << props.control_points[i].position[1] << ", " 
             << props.control_points[i].position[2] << "]}";
        if (i < numPoints - 1) file << ",";
        file << "\n";
    }
    file << "    ]\n";
    file << "  },\n\n";
    
    // Geometry (base64 encoded)
    file << "  \"geometry\": {\n";
    file << "    \"vertex_count\": " << geom.vertices.size() << ",\n";
    file << "    \"index_count\": " << geom.indices.size() << ",\n";
    if (!geom.vertices.empty()) {
        std::string vertData = base64Encode(reinterpret_cast<const unsigned char*>(geom.vertices.data()), 
                                           geom.vertices.size() * sizeof(HDMVertex));
        file << "    \"vertices_base64\": \"" << vertData << "\",\n";
    }
    if (!geom.indices.empty()) {
        std::string idxData = base64Encode(reinterpret_cast<const unsigned char*>(geom.indices.data()),
                                          geom.indices.size() * sizeof(uint32_t));
        file << "    \"indices_base64\": \"" << idxData << "\"\n";
    }
    file << "  },\n\n";
    
    // Texture (base64 encoded)
    file << "  \"texture\": {\n";
    file << "    \"width\": " << tex.width << ",\n";
    file << "    \"height\": " << tex.height << ",\n";
    file << "    \"format\": " << tex.format << ",\n";
    if (!tex.data.empty()) {
        std::string texData = base64Encode(tex.data.data(), tex.data.size());
        file << "    \"data_base64\": \"" << texData << "\"\n";
    }
    file << "  }\n";
    file << "}\n";
    
    file.close();
    std::cout << "[HDM] Saved ASCII HDM: " << filepath << std::endl;
    return true;
}

bool loadAscii(const char* filepath, HDMProperties& props,
               HDMGeometry& geom, HDMTexture& tex) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "[HDM] Failed to open file: " << filepath << std::endl;
        return false;
    }
    
    std::string json;
    std::string line;
    while (std::getline(file, line)) {
        json += line + "\n";
    }
    file.close();
    
    initDefaultProperties(props);
    
    // Parse model properties
    std::string objPath = jsonFindString(json, "obj_path");
    std::string texPath = jsonFindString(json, "texture_path");
    strncpy(props.model.obj_path, objPath.c_str(), sizeof(props.model.obj_path) - 1);
    strncpy(props.model.texture_path, texPath.c_str(), sizeof(props.model.texture_path) - 1);
    jsonFindFloatArray(json, "scale", props.model.scale, 3);
    jsonFindFloatArray(json, "origin_offset", props.model.origin_offset, 3);
    
    // Parse item properties
    props.item.item_type_id = jsonFindInt(json, "item_type_id");
    std::string itemName = jsonFindString(json, "item_name");
    strncpy(props.item.item_name, itemName.c_str(), sizeof(props.item.item_name) - 1);
    props.item.trade_value = jsonFindInt(json, "trade_value");
    props.item.condition = jsonFindFloat(json, "condition");
    props.item.weight = jsonFindFloat(json, "weight");
    props.item.category = jsonFindInt(json, "category");
    props.item.is_salvaged = jsonFindBool(json, "is_salvaged");
    
    // Parse physics properties
    props.physics.collision_type = jsonFindInt(json, "collision_type");
    jsonFindFloatArray(json, "collision_bounds", props.physics.collision_bounds, 3);
    props.physics.is_static = jsonFindBool(json, "is_static");
    props.physics.mass = jsonFindFloat(json, "mass");
    
    // Parse geometry
    int vertexCount = jsonFindInt(json, "vertex_count");
    int indexCount = jsonFindInt(json, "index_count");
    
    if (vertexCount > 0) {
        std::string vertB64 = jsonFindString(json, "vertices_base64");
        if (!vertB64.empty()) {
            std::vector<unsigned char> vertData = base64Decode(vertB64);
            size_t expectedSize = vertexCount * sizeof(HDMVertex);
            if (vertData.size() >= expectedSize) {
                geom.vertices.resize(vertexCount);
                memcpy(geom.vertices.data(), vertData.data(), expectedSize);
            }
        }
    }
    
    if (indexCount > 0) {
        std::string idxB64 = jsonFindString(json, "indices_base64");
        if (!idxB64.empty()) {
            std::vector<unsigned char> idxData = base64Decode(idxB64);
            size_t expectedSize = indexCount * sizeof(uint32_t);
            if (idxData.size() >= expectedSize) {
                geom.indices.resize(indexCount);
                memcpy(geom.indices.data(), idxData.data(), expectedSize);
            }
        }
    }
    
    // Parse texture
    tex.width = jsonFindInt(json, "width");
    tex.height = jsonFindInt(json, "height");
    tex.format = jsonFindInt(json, "format");
    
    if (tex.width > 0 && tex.height > 0) {
        std::string texB64 = jsonFindString(json, "data_base64");
        if (!texB64.empty()) {
            tex.data = base64Decode(texB64);
        }
    }
    
    std::cout << "[HDM] Loaded ASCII HDM: " << filepath << std::endl;
    return true;
}

// ============================================================================
// Properties-Only JSON (lighter format)
// ============================================================================

bool savePropertiesJson(const char* filepath, const HDMProperties& props) {
    std::ofstream file(filepath);
    if (!file.is_open()) return false;
    
    file << "{\n";
    file << "  \"hdm_version\": \"" << props.hdm_version << "\",\n\n";
    file << "  \"model\": {\n";
    file << "    \"obj_path\": \"" << props.model.obj_path << "\",\n";
    file << "    \"texture_path\": \"" << props.model.texture_path << "\",\n";
    file << "    \"scale\": [" << props.model.scale[0] << ", " << props.model.scale[1] << ", " << props.model.scale[2] << "]\n";
    file << "  },\n\n";
    file << "  \"item_properties\": {\n";
    file << "    \"item_type_id\": " << props.item.item_type_id << ",\n";
    file << "    \"item_name\": \"" << props.item.item_name << "\",\n";
    file << "    \"trade_value\": " << props.item.trade_value << ",\n";
    file << "    \"condition\": " << props.item.condition << ",\n";
    file << "    \"weight\": " << props.item.weight << ",\n";
    file << "    \"category\": " << props.item.category << ",\n";
    file << "    \"is_salvaged\": " << (props.item.is_salvaged ? "true" : "false") << "\n";
    file << "  },\n\n";
    file << "  \"physics\": {\n";
    file << "    \"collision_type\": " << props.physics.collision_type << ",\n";
    file << "    \"collision_bounds\": [" << props.physics.collision_bounds[0] << ", " << props.physics.collision_bounds[1] << ", " << props.physics.collision_bounds[2] << "],\n";
    file << "    \"is_static\": " << (props.physics.is_static ? "true" : "false") << ",\n";
    file << "    \"mass\": " << props.physics.mass << "\n";
    file << "  }\n";
    file << "}\n";
    
    file.close();
    return true;
}

bool loadPropertiesJson(const char* filepath, HDMProperties& props) {
    std::ifstream file(filepath);
    if (!file.is_open()) return false;
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string json = buffer.str();
    file.close();
    
    initDefaultProperties(props);
    
    std::string ver = jsonFindString(json, "hdm_version");
    if (!ver.empty()) strncpy(props.hdm_version, ver.c_str(), sizeof(props.hdm_version) - 1);
    
    std::string obj = jsonFindString(json, "obj_path");
    if (!obj.empty()) strncpy(props.model.obj_path, obj.c_str(), sizeof(props.model.obj_path) - 1);
    
    std::string tex = jsonFindString(json, "texture_path");
    if (!tex.empty()) strncpy(props.model.texture_path, tex.c_str(), sizeof(props.model.texture_path) - 1);
    
    props.item.item_type_id = jsonFindInt(json, "item_type_id");
    std::string name = jsonFindString(json, "item_name");
    if (!name.empty()) strncpy(props.item.item_name, name.c_str(), sizeof(props.item.item_name) - 1);
    props.item.trade_value = jsonFindInt(json, "trade_value");
    props.item.condition = jsonFindFloat(json, "condition");
    props.item.weight = jsonFindFloat(json, "weight");
    props.item.category = jsonFindInt(json, "category");
    props.item.is_salvaged = jsonFindBool(json, "is_salvaged");
    
    props.physics.collision_type = jsonFindInt(json, "collision_type");
    props.physics.is_static = jsonFindBool(json, "is_static");
    props.physics.mass = jsonFindFloat(json, "mass");
    
    return true;
}

// ============================================================================
// Auto-Format Detection
// ============================================================================

bool load(const char* filepath, HDMProperties& props,
          HDMGeometry& geom, HDMTexture& tex) {
    std::string path(filepath);
    
    // Check extension
    if (path.length() >= 5 && path.substr(path.length() - 5) == ".hdma") {
        return loadAscii(filepath, props, geom, tex);
    } else if (path.length() >= 4 && path.substr(path.length() - 4) == ".hdm") {
        return loadBinary(filepath, props, geom, tex);
    }
    
    // Try binary first, then ASCII
    if (loadBinary(filepath, props, geom, tex)) return true;
    return loadAscii(filepath, props, geom, tex);
}

bool save(const char* filepath, const HDMProperties& props,
          const HDMGeometry& geom, const HDMTexture& tex) {
    std::string path(filepath);
    
    // Check extension
    if (path.length() >= 5 && path.substr(path.length() - 5) == ".hdma") {
        return saveAscii(filepath, props, geom, tex);
    } else {
        return saveBinary(filepath, props, geom, tex);
    }
}

} // namespace hdm
} // namespace eden

// ============================================================================
// C-Style API (global state for loaded properties)
// ============================================================================

static HDMProperties g_loadedHdmProperties;

extern "C" int hdm_load_file(const char* filepath) {
    eden::hdm::initDefaultProperties(g_loadedHdmProperties);
    return eden::hdm::loadPropertiesJson(filepath, g_loadedHdmProperties) ? 1 : 0;
}

extern "C" HDMItemPropertiesC hdm_get_item_properties() {
    HDMItemPropertiesC result;
    result.item_type_id = g_loadedHdmProperties.item.item_type_id;
    strncpy(result.item_name, g_loadedHdmProperties.item.item_name, sizeof(result.item_name) - 1);
    result.trade_value = g_loadedHdmProperties.item.trade_value;
    result.condition = g_loadedHdmProperties.item.condition;
    result.weight = g_loadedHdmProperties.item.weight;
    result.category = g_loadedHdmProperties.item.category;
    result.is_salvaged = g_loadedHdmProperties.item.is_salvaged ? 1 : 0;
    return result;
}

extern "C" HDMPhysicsPropertiesC hdm_get_physics_properties() {
    HDMPhysicsPropertiesC result;
    result.collision_type = g_loadedHdmProperties.physics.collision_type;
    result.collision_bounds_x = g_loadedHdmProperties.physics.collision_bounds[0];
    result.collision_bounds_y = g_loadedHdmProperties.physics.collision_bounds[1];
    result.collision_bounds_z = g_loadedHdmProperties.physics.collision_bounds[2];
    result.is_static = g_loadedHdmProperties.physics.is_static ? 1 : 0;
    result.mass = g_loadedHdmProperties.physics.mass;
    return result;
}

extern "C" HDMModelPropertiesC hdm_get_model_properties() {
    HDMModelPropertiesC result;
    strncpy(result.obj_path, g_loadedHdmProperties.model.obj_path, sizeof(result.obj_path) - 1);
    strncpy(result.texture_path, g_loadedHdmProperties.model.texture_path, sizeof(result.texture_path) - 1);
    result.scale_x = g_loadedHdmProperties.model.scale[0];
    result.scale_y = g_loadedHdmProperties.model.scale[1];
    result.scale_z = g_loadedHdmProperties.model.scale[2];
    return result;
}

extern "C" int hdm_save_file(const char* filepath) {
    return eden::hdm::savePropertiesJson(filepath, g_loadedHdmProperties) ? 1 : 0;
}

