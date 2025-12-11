// EDEN ENGINE - PNG Texture Loader
// Source asset loader (PNG → uncompressed RGBA for GPU upload)
// For production assets, convert PNG → DDS at build time for better performance

#ifndef EDEN_PNG_LOADER_H
#define EDEN_PNG_LOADER_H

#include "vulkan.h"
#include <vector>
#include <string>
#include <fstream>
#include <cstring>

// Use stb_image for PNG loading (single-header library)
// Only define implementation once (prevent multiple includes from redefining)
#ifndef STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_LINEAR  // We don't need HDR support for PNG
#include "../stdlib/stb_image.h"
#endif

// PNG data structure (similar to DDS but uncompressed)
struct PNGData {
    VkFormat format;              // Always VK_FORMAT_R8G8B8A8_SRGB for PNG
    uint32_t width, height;
    std::vector<uint8_t> pixelData;  // RGBA8 uncompressed data
};

/**
 * Load PNG file and return image data ready for Vulkan upload
 * 
 * @param filepath Path to PNG file
 * @return PNGData structure with pixel data, or throws on error
 */
inline PNGData load_png(const std::string& filepath) {
    PNGData result;
    
    // stb_image loads PNG and converts to RGBA8
    int width, height, channels;
    unsigned char* pixels = stbi_load(filepath.c_str(), &width, &height, &channels, STBI_rgb_alpha);
    
    if (!pixels) {
        throw std::runtime_error("Failed to load PNG: " + filepath + " - " + stbi_failure_reason());
    }
    
    // PNG is always loaded as RGBA8
    result.format = VK_FORMAT_R8G8B8A8_SRGB;
    result.width = static_cast<uint32_t>(width);
    result.height = static_cast<uint32_t>(height);
    
    // Copy pixel data (STBI_rgb_alpha always gives 4 channels)
    size_t dataSize = width * height * 4;
    result.pixelData.resize(dataSize);
    memcpy(result.pixelData.data(), pixels, dataSize);
    
    // Free stb_image's buffer
    stbi_image_free(pixels);
    
    return result;
}

/**
 * Load PNG from memory buffer
 * Useful for embedded assets or network loading
 */
inline PNGData load_png_from_memory(const uint8_t* data, size_t size) {
    PNGData result;
    
    int width, height, channels;
    unsigned char* pixels = stbi_load_from_memory(data, static_cast<int>(size), 
                                                   &width, &height, &channels, STBI_rgb_alpha);
    
    if (!pixels) {
        throw std::runtime_error("Failed to load PNG from memory - " + std::string(stbi_failure_reason()));
    }
    
    result.format = VK_FORMAT_R8G8B8A8_SRGB;
    result.width = static_cast<uint32_t>(width);
    result.height = static_cast<uint32_t>(height);
    
    size_t dataSize = width * height * 4;
    result.pixelData.resize(dataSize);
    memcpy(result.pixelData.data(), pixels, dataSize);
    
    stbi_image_free(pixels);
    
    return result;
}

#endif // EDEN_PNG_LOADER_H

