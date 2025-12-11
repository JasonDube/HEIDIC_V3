// EDEN ENGINE - DDS Texture Loader
// Fast path for GPU-ready compressed textures (BC7, BC5, R8, etc.)
// Loads DDS files and extracts compressed data ready for direct GPU upload

#ifndef EDEN_DDS_LOADER_H
#define EDEN_DDS_LOADER_H

#include "vulkan.h"
#include <vector>
#include <string>
#include <fstream>
#include <cstring>

// DDS constants
#define DDS_MAGIC 0x20534444  // "DDS " in little-endian

// DDS header flags
#define DDSD_CAPS 0x1
#define DDSD_HEIGHT 0x2
#define DDSD_WIDTH 0x4
#define DDSD_PITCH 0x8
#define DDSD_PIXELFORMAT 0x1000
#define DDSD_MIPMAPCOUNT 0x20000
#define DDSD_LINEARSIZE 0x80000
#define DDSD_DEPTH 0x800000

// DDS pixel format flags
#define DDPF_ALPHAPIXELS 0x1
#define DDPF_ALPHA 0x2
#define DDPF_FOURCC 0x4
#define DDPF_RGB 0x40
#define DDPF_YUV 0x200
#define DDPF_LUMINANCE 0x20000

// DDS FourCC codes (compressed formats)
#define FOURCC_DXT1 0x31545844  // "DXT1"
#define FOURCC_DXT3 0x33545844  // "DXT3"
#define FOURCC_DXT5 0x35545844  // "DXT5"
#define FOURCC_BC4U 0x55344342  // "BC4U"
#define FOURCC_BC4S 0x53344342  // "BC4S"
#define FOURCC_BC5U 0x32495441  // "ATI2" / BC5U
#define FOURCC_BC5S 0x53354342  // "BC5S"
#define FOURCC_BC6H 0x68423620  // "BC6H"
#define FOURCC_BC7  0x20374243  // "BC7 "
#define FOURCC_DX10 0x30315844  // "DX10" (indicates extended header)

// DDS pixel format structure
struct DDSPixelFormat {
    uint32_t size;              // Must be 32
    uint32_t flags;
    uint32_t fourCC;
    uint32_t rgbBitCount;
    uint32_t rBitMask;
    uint32_t gBitMask;
    uint32_t bBitMask;
    uint32_t aBitMask;
};

// DDS header structure (without magic)
struct DDSHeader {
    uint32_t size;              // Must be 124
    uint32_t flags;
    uint32_t height;
    uint32_t width;
    uint32_t pitchOrLinearSize;
    uint32_t depth;
    uint32_t mipMapCount;
    uint32_t reserved1[11];
    DDSPixelFormat ddspf;
    uint32_t caps;
    uint32_t caps2;
    uint32_t caps3;
    uint32_t caps4;
    uint32_t reserved2;
};

// DDS DX10 extended header (for newer formats like BC7)
struct DDSHeaderDX10 {
    uint32_t dxgiFormat;        // DXGI_FORMAT enum
    uint32_t resourceDimension;
    uint32_t miscFlag;
    uint32_t arraySize;
    uint32_t miscFlags2;
};

// Loaded DDS data structure
struct DDSData {
    VkFormat format;                    // Vulkan format (BC7, BC5, etc.)
    uint32_t width;
    uint32_t height;
    uint32_t mipmapCount;
    uint32_t arraySize;                 // Usually 1 for 2D textures
    std::vector<uint8_t> compressedData; // Compressed texture data (ready for GPU)
    bool hasAlpha;
};

// Map DXGI format to Vulkan format (for DX10 DDS files)
static VkFormat dxgiFormatToVulkan(uint32_t dxgiFormat) {
    // DXGI_FORMAT enum values (from dxgiformat.h)
    // Common formats we support:
    switch (dxgiFormat) {
        // BC compressed formats
        case 71:  return VK_FORMAT_BC1_RGB_UNORM_BLOCK;      // DXGI_FORMAT_BC1_UNORM
        case 72:  return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;     // DXGI_FORMAT_BC1_UNORM_SRGB
        case 74:  return VK_FORMAT_BC2_UNORM_BLOCK;          // DXGI_FORMAT_BC2_UNORM
        case 75:  return VK_FORMAT_BC2_UNORM_BLOCK;          // DXGI_FORMAT_BC2_UNORM_SRGB (use same format)
        case 77:  return VK_FORMAT_BC3_UNORM_BLOCK;          // DXGI_FORMAT_BC3_UNORM
        case 78:  return VK_FORMAT_BC3_UNORM_BLOCK;          // DXGI_FORMAT_BC3_UNORM_SRGB (use same format)
        case 80:  return VK_FORMAT_BC4_UNORM_BLOCK;          // DXGI_FORMAT_BC4_UNORM
        case 81:  return VK_FORMAT_BC4_SNORM_BLOCK;          // DXGI_FORMAT_BC4_SNORM
        case 83:  return VK_FORMAT_BC5_UNORM_BLOCK;          // DXGI_FORMAT_BC5_UNORM (RG/BC5U)
        case 84:  return VK_FORMAT_BC5_SNORM_BLOCK;          // DXGI_FORMAT_BC5_SNORM
        case 95:  return VK_FORMAT_BC6H_UFLOAT_BLOCK;        // DXGI_FORMAT_BC6H_UF16
        case 96:  return VK_FORMAT_BC6H_SFLOAT_BLOCK;        // DXGI_FORMAT_BC6H_SF16
        case 97:  return VK_FORMAT_BC7_UNORM_BLOCK;          // DXGI_FORMAT_BC7_UNORM
        case 98:  return VK_FORMAT_BC7_SRGB_BLOCK;           // DXGI_FORMAT_BC7_UNORM_SRGB
        
        // Uncompressed formats (less common, but handle them)
        case 28:  return VK_FORMAT_R8G8B8A8_UNORM;           // DXGI_FORMAT_R8G8B8A8_UNORM
        case 29:  return VK_FORMAT_R8G8B8A8_SRGB;            // DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
        
        default:
            return VK_FORMAT_UNDEFINED;
    }
}

// Map DDS FourCC to Vulkan format (for legacy DDS files)
static VkFormat fourCCToVulkan(uint32_t fourCC, bool hasAlpha = false) {
    switch (fourCC) {
        case FOURCC_DXT1:
            return hasAlpha ? VK_FORMAT_BC1_RGBA_UNORM_BLOCK : VK_FORMAT_BC1_RGB_UNORM_BLOCK;
        case FOURCC_DXT3:
            return VK_FORMAT_BC2_UNORM_BLOCK;
        case FOURCC_DXT5:
            return VK_FORMAT_BC3_UNORM_BLOCK;
        case FOURCC_BC4U:
            return VK_FORMAT_BC4_UNORM_BLOCK;
        case FOURCC_BC4S:
            return VK_FORMAT_BC4_SNORM_BLOCK;
        case FOURCC_BC5U:  // "ATI2" / BC5U
            return VK_FORMAT_BC5_UNORM_BLOCK;
        case FOURCC_BC5S:
            return VK_FORMAT_BC5_SNORM_BLOCK;
        case FOURCC_BC6H:
            return VK_FORMAT_BC6H_UFLOAT_BLOCK;  // Assuming unsigned by default
        case FOURCC_BC7:
            return VK_FORMAT_BC7_UNORM_BLOCK;
        default:
            return VK_FORMAT_UNDEFINED;
    }
}

// Calculate compressed block size (in bytes) for a given format
static uint32_t getBlockSize(VkFormat format) {
    switch (format) {
        case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
        case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
        case VK_FORMAT_BC4_UNORM_BLOCK:
        case VK_FORMAT_BC4_SNORM_BLOCK:
            return 8;  // 64 bits per 4x4 block
        
        case VK_FORMAT_BC2_UNORM_BLOCK:
        case VK_FORMAT_BC3_UNORM_BLOCK:
        case VK_FORMAT_BC5_UNORM_BLOCK:
        case VK_FORMAT_BC5_SNORM_BLOCK:
        case VK_FORMAT_BC6H_UFLOAT_BLOCK:
        case VK_FORMAT_BC6H_SFLOAT_BLOCK:
        case VK_FORMAT_BC7_UNORM_BLOCK:
        case VK_FORMAT_BC7_SRGB_BLOCK:
            return 16; // 128 bits per 4x4 block
        
        default:
            return 0;  // Uncompressed or unsupported
    }
}

// Calculate size of compressed mipmap level
static uint32_t calculateMipmapSize(uint32_t width, uint32_t height, VkFormat format) {
    uint32_t blockSize = getBlockSize(format);
    if (blockSize == 0) {
        // Uncompressed - assume RGBA8
        return width * height * 4;
    }
    
    // Compressed: 4x4 blocks
    uint32_t blocksWide = (width + 3) / 4;
    uint32_t blocksHigh = (height + 3) / 4;
    return blocksWide * blocksHigh * blockSize;
}

// Load DDS file and extract compressed data
DDSData load_dds(const std::string& path) {
    DDSData result = {};
    result.format = VK_FORMAT_UNDEFINED;
    result.mipmapCount = 1;
    result.arraySize = 1;
    result.hasAlpha = false;
    
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        // Return empty result - caller should check format != VK_FORMAT_UNDEFINED
        // Note: File not found or cannot open - check path/permissions
        return result;
    }
    
    // Read magic number
    uint32_t magic = 0;
    file.read(reinterpret_cast<char*>(&magic), sizeof(uint32_t));
    if (magic != DDS_MAGIC) {
        file.close();
        return result;  // Invalid DDS file
    }
    
    // Read header
    DDSHeader header = {};
    file.read(reinterpret_cast<char*>(&header), sizeof(DDSHeader));
    
    // Validate header
    if (header.size != 124) {
        file.close();
        return result;  // Invalid header size
    }
    
    result.width = header.width;
    result.height = header.height;
    result.mipmapCount = (header.flags & DDSD_MIPMAPCOUNT) ? header.mipMapCount : 1;
    result.hasAlpha = (header.ddspf.flags & DDPF_ALPHAPIXELS) != 0;
    
    // Determine format
    bool hasDX10Header = (header.ddspf.fourCC == FOURCC_DX10);
    
    if (hasDX10Header) {
        // Read DX10 extended header
        DDSHeaderDX10 dx10Header = {};
        file.read(reinterpret_cast<char*>(&dx10Header), sizeof(DDSHeaderDX10));
        
        result.format = dxgiFormatToVulkan(dx10Header.dxgiFormat);
        result.arraySize = dx10Header.arraySize > 0 ? dx10Header.arraySize : 1;
    } else {
        // Legacy DDS - use FourCC
        if (header.ddspf.flags & DDPF_FOURCC) {
            result.format = fourCCToVulkan(header.ddspf.fourCC, result.hasAlpha);
        } else if (header.ddspf.flags & DDPF_RGB) {
            // Uncompressed RGB - convert to appropriate Vulkan format
            if (header.ddspf.rgbBitCount == 32 && 
                header.ddspf.rBitMask == 0x00FF0000 &&
                header.ddspf.gBitMask == 0x0000FF00 &&
                header.ddspf.bBitMask == 0x000000FF) {
                result.format = result.hasAlpha ? VK_FORMAT_R8G8B8A8_UNORM : VK_FORMAT_B8G8R8A8_UNORM;
            } else {
                // Unsupported uncompressed format
                file.close();
                return result;
            }
        } else {
            // Unsupported format
            file.close();
            return result;
        }
    }
    
    // Check if format is supported
    if (result.format == VK_FORMAT_UNDEFINED) {
        file.close();
        return result;
    }
    
    // Calculate total data size (all mipmaps)
    uint32_t totalSize = 0;
    uint32_t currentWidth = result.width;
    uint32_t currentHeight = result.height;
    
    for (uint32_t i = 0; i < result.mipmapCount; i++) {
        totalSize += calculateMipmapSize(currentWidth, currentHeight, result.format);
        currentWidth = currentWidth > 1 ? currentWidth / 2 : 1;
        currentHeight = currentHeight > 1 ? currentHeight / 2 : 1;
    }
    
    // Multiply by array size (for texture arrays)
    totalSize *= result.arraySize;
    
    // Read compressed data
    result.compressedData.resize(totalSize);
    file.read(reinterpret_cast<char*>(result.compressedData.data()), totalSize);
    
    file.close();
    
    // Verify we read the expected amount
    if (file.gcount() != static_cast<std::streamsize>(totalSize)) {
        // File was truncated or format mismatch
        result.format = VK_FORMAT_UNDEFINED;
        result.compressedData.clear();
        return result;
    }
    
    return result;
}

#endif // EDEN_DDS_LOADER_H

