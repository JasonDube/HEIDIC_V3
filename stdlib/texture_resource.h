// EDEN ENGINE - TextureResource Class
// Unified texture loading: automatically handles DDS (compressed) and PNG (uncompressed)
// Creates Vulkan resources (image, view, sampler) ready for use in shaders

#ifndef EDEN_TEXTURE_RESOURCE_H
#define EDEN_TEXTURE_RESOURCE_H

#include "vulkan.h"
#include "dds_loader.h"
#include "png_loader.h"
#include <vector>
#include <string>
#include <algorithm>
#include <cstring>
#include <fstream>

// Forward declarations (these will be provided by Vulkan helpers or passed as parameters)
// For now, we'll assume they're globally accessible or we'll add parameters later
extern VkDevice g_device;
extern VkPhysicalDevice g_physicalDevice;
extern VkCommandPool g_commandPool;
extern VkQueue g_graphicsQueue;

/**
 * TextureResource - Unified texture loading and Vulkan resource management
 * 
 * Automatically detects format (DDS vs PNG) and creates appropriate Vulkan resources.
 * Handles both compressed (DDS) and uncompressed (PNG) textures seamlessly.
 */
class TextureResource {
private:
    VkImage m_image = VK_NULL_HANDLE;
    VkImageView m_imageView = VK_NULL_HANDLE;
    VkSampler m_sampler = VK_NULL_HANDLE;
    VkDeviceMemory m_imageMemory = VK_NULL_HANDLE;
    
    VkFormat m_format = VK_FORMAT_UNDEFINED;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    uint32_t m_mipmapCount = 1;
    
    bool m_loaded = false;
    
    // Helper function to find memory type (same as in vulkan helpers)
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(g_physicalDevice, &memProperties);
        
        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
            if ((typeFilter & (1 << i)) && 
                (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }
        
        return UINT32_MAX;
    }
    
    // Helper to begin single-time command buffer
    VkCommandBuffer beginSingleTimeCommands() {
        VkCommandBufferAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = g_commandPool;
        allocInfo.commandBufferCount = 1;
        
        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(g_device, &allocInfo, &commandBuffer);
        
        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        
        vkBeginCommandBuffer(commandBuffer, &beginInfo);
        return commandBuffer;
    }
    
    // Helper to end single-time command buffer
    void endSingleTimeCommands(VkCommandBuffer commandBuffer) {
        vkEndCommandBuffer(commandBuffer);
        
        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;
        
        vkQueueSubmit(g_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(g_graphicsQueue);
        
        vkFreeCommandBuffers(g_device, g_commandPool, 1, &commandBuffer);
    }
    
    // Create buffer helper (for staging)
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, 
                     VkMemoryPropertyFlags properties, VkBuffer& buffer, 
                     VkDeviceMemory& bufferMemory) {
        VkBufferCreateInfo bufferInfo = {};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        
        if (vkCreateBuffer(g_device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create buffer");
        }
        
        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(g_device, buffer, &memRequirements);
        
        VkMemoryAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);
        
        if (allocInfo.memoryTypeIndex == UINT32_MAX) {
            vkDestroyBuffer(g_device, buffer, nullptr);
            throw std::runtime_error("Failed to find suitable memory type for buffer");
        }
        
        if (vkAllocateMemory(g_device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
            vkDestroyBuffer(g_device, buffer, nullptr);
            throw std::runtime_error("Failed to allocate buffer memory");
        }
        
        vkBindBufferMemory(g_device, buffer, bufferMemory, 0);
    }
    
    // Detect file format from extension or magic number
    bool isDDS(const std::string& filepath) {
        // Check extension first (fast)
        std::string lowerPath = filepath;
        std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(), ::tolower);
        
        if (lowerPath.length() >= 4 && lowerPath.substr(lowerPath.length() - 4) == ".dds") {
            return true;
        }
        
        // TODO: Could also check magic number by reading first 4 bytes
        // For now, extension check is sufficient
        return false;
    }
    
    // Load DDS texture and create Vulkan resources
    void loadDDS(const std::string& filepath) {
        // Check if file exists first for better error message
        std::ifstream testFile(filepath, std::ios::binary);
        if (!testFile.is_open()) {
            throw std::runtime_error("DDS file not found or cannot open: " + filepath + 
                                    " (check path is relative to executable or use absolute path)");
        }
        testFile.close();
        
        DDSData ddsData = load_dds(filepath);
        
        if (ddsData.format == VK_FORMAT_UNDEFINED) {
            throw std::runtime_error("Failed to load DDS file: " + filepath + 
                                    " (file exists but is invalid or unsupported format)");
        }
        
        m_format = ddsData.format;
        m_width = ddsData.width;
        m_height = ddsData.height;
        m_mipmapCount = ddsData.mipmapCount;
        
        // Create Vulkan image
        VkImageCreateInfo imageInfo = {};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = m_width;
        imageInfo.extent.height = m_height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = m_mipmapCount;
        imageInfo.arrayLayers = 1;
        imageInfo.format = m_format;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        
        if (vkCreateImage(g_device, &imageInfo, nullptr, &m_image) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create DDS image");
        }
        
        // Allocate image memory
        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(g_device, m_image, &memRequirements);
        
        VkMemoryAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, 
                                                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        
        if (allocInfo.memoryTypeIndex == UINT32_MAX) {
            vkDestroyImage(g_device, m_image, nullptr);
            throw std::runtime_error("Failed to find suitable memory type for DDS image");
        }
        
        if (vkAllocateMemory(g_device, &allocInfo, nullptr, &m_imageMemory) != VK_SUCCESS) {
            vkDestroyImage(g_device, m_image, nullptr);
            throw std::runtime_error("Failed to allocate DDS image memory");
        }
        
        vkBindImageMemory(g_device, m_image, m_imageMemory, 0);
        
        // Create staging buffer for compressed data
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        createBuffer(ddsData.compressedData.size(), 
                    VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                    stagingBuffer, stagingBufferMemory);
        
        // Copy compressed data to staging buffer
        void* data;
        vkMapMemory(g_device, stagingBufferMemory, 0, ddsData.compressedData.size(), 0, &data);
        memcpy(data, ddsData.compressedData.data(), ddsData.compressedData.size());
        vkUnmapMemory(g_device, stagingBufferMemory);
        
        // Upload to GPU
        VkCommandBuffer commandBuffer = beginSingleTimeCommands();
        
        // Transition to transfer destination
        VkImageMemoryBarrier barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = m_image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = m_mipmapCount;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        
        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 
                            VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
        
        // Copy buffer to image (compressed blocks)
        VkBufferImageCopy region = {};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = {0, 0, 0};
        region.imageExtent = {m_width, m_height, 1};
        
        vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, m_image, 
                              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        
        // Transition to shader-readable
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        
        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, 
                            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
        
        endSingleTimeCommands(commandBuffer);
        
        // Cleanup staging buffer
        vkDestroyBuffer(g_device, stagingBuffer, nullptr);
        vkFreeMemory(g_device, stagingBufferMemory, nullptr);
    }
    
    // Load PNG texture and create Vulkan resources
    void loadPNG(const std::string& filepath) {
        PNGData pngData = load_png(filepath);
        
        m_format = pngData.format;
        m_width = pngData.width;
        m_height = pngData.height;
        m_mipmapCount = 1;  // PNG has no mipmaps
        
        // Create Vulkan image
        VkImageCreateInfo imageInfo = {};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = m_width;
        imageInfo.extent.height = m_height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = m_format;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        
        if (vkCreateImage(g_device, &imageInfo, nullptr, &m_image) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create PNG image");
        }
        
        // Allocate image memory
        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(g_device, m_image, &memRequirements);
        
        VkMemoryAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, 
                                                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        
        if (allocInfo.memoryTypeIndex == UINT32_MAX) {
            vkDestroyImage(g_device, m_image, nullptr);
            throw std::runtime_error("Failed to find suitable memory type for PNG image");
        }
        
        if (vkAllocateMemory(g_device, &allocInfo, nullptr, &m_imageMemory) != VK_SUCCESS) {
            vkDestroyImage(g_device, m_image, nullptr);
            throw std::runtime_error("Failed to allocate PNG image memory");
        }
        
        vkBindImageMemory(g_device, m_image, m_imageMemory, 0);
        
        // Create staging buffer for uncompressed RGBA8 data
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        createBuffer(pngData.pixelData.size(), 
                    VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                    stagingBuffer, stagingBufferMemory);
        
        // Copy pixel data to staging buffer
        void* data;
        vkMapMemory(g_device, stagingBufferMemory, 0, pngData.pixelData.size(), 0, &data);
        memcpy(data, pngData.pixelData.data(), pngData.pixelData.size());
        vkUnmapMemory(g_device, stagingBufferMemory);
        
        // Upload to GPU
        VkCommandBuffer commandBuffer = beginSingleTimeCommands();
        
        // Transition to transfer destination
        VkImageMemoryBarrier barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = m_image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        
        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 
                            VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
        
        // Copy buffer to image (uncompressed RGBA8)
        VkBufferImageCopy region = {};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = {0, 0, 0};
        region.imageExtent = {m_width, m_height, 1};
        
        vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, m_image, 
                              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        
        // Transition to shader-readable
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        
        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, 
                            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
        
        endSingleTimeCommands(commandBuffer);
        
        // Cleanup staging buffer
        vkDestroyBuffer(g_device, stagingBuffer, nullptr);
        vkFreeMemory(g_device, stagingBufferMemory, nullptr);
    }
    
    // Create image view and sampler
    void createViewAndSampler() {
        // Create image view
        VkImageViewCreateInfo viewInfo = {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = m_image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = m_format;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = m_mipmapCount;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;
        
        if (vkCreateImageView(g_device, &viewInfo, nullptr, &m_imageView) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create image view");
        }
        
        // Create sampler
        VkSamplerCreateInfo samplerInfo = {};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.anisotropyEnable = VK_FALSE;
        samplerInfo.maxAnisotropy = 1.0f;
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = static_cast<float>(m_mipmapCount);
        
        if (vkCreateSampler(g_device, &samplerInfo, nullptr, &m_sampler) != VK_SUCCESS) {
            vkDestroyImageView(g_device, m_imageView, nullptr);
            throw std::runtime_error("Failed to create sampler");
        }
    }

public:
    /**
     * Constructor - Loads texture from file and creates Vulkan resources
     * @param filepath Path to texture file (DDS or PNG)
     * @throws std::runtime_error if loading or resource creation fails
     */
    TextureResource(const std::string& filepath) {
        try {
            // Auto-detect format and load
            if (isDDS(filepath)) {
                loadDDS(filepath);
            } else {
                // Assume PNG (could add more format detection later)
                loadPNG(filepath);
            }
            
            // Create image view and sampler
            createViewAndSampler();
            
            m_loaded = true;
        } catch (const std::exception& e) {
            cleanup();
            throw;
        }
    }
    
    // Move constructor
    TextureResource(TextureResource&& other) noexcept 
        : m_image(other.m_image), m_imageView(other.m_imageView), 
          m_sampler(other.m_sampler), m_imageMemory(other.m_imageMemory),
          m_format(other.m_format), m_width(other.m_width), 
          m_height(other.m_height), m_mipmapCount(other.m_mipmapCount),
          m_loaded(other.m_loaded) {
        other.m_image = VK_NULL_HANDLE;
        other.m_imageView = VK_NULL_HANDLE;
        other.m_sampler = VK_NULL_HANDLE;
        other.m_imageMemory = VK_NULL_HANDLE;
        other.m_loaded = false;
    }
    
    // Delete copy constructor and assignment (textures are unique resources)
    TextureResource(const TextureResource&) = delete;
    TextureResource& operator=(const TextureResource&) = delete;
    
    // Destructor - RAII cleanup
    ~TextureResource() {
        cleanup();
    }
    
    // Cleanup helper
    void cleanup() {
        if (m_sampler != VK_NULL_HANDLE) {
            vkDestroySampler(g_device, m_sampler, nullptr);
            m_sampler = VK_NULL_HANDLE;
        }
        if (m_imageView != VK_NULL_HANDLE) {
            vkDestroyImageView(g_device, m_imageView, nullptr);
            m_imageView = VK_NULL_HANDLE;
        }
        if (m_image != VK_NULL_HANDLE) {
            vkDestroyImage(g_device, m_image, nullptr);
            m_image = VK_NULL_HANDLE;
        }
        if (m_imageMemory != VK_NULL_HANDLE) {
            vkFreeMemory(g_device, m_imageMemory, nullptr);
            m_imageMemory = VK_NULL_HANDLE;
        }
        m_loaded = false;
    }
    
    // Getters for Vulkan resources (ready to use in descriptors, etc.)
    VkImage getImage() const { return m_image; }
    VkImageView getImageView() const { return m_imageView; }
    VkSampler getSampler() const { return m_sampler; }
    VkFormat getFormat() const { return m_format; }
    uint32_t getWidth() const { return m_width; }
    uint32_t getHeight() const { return m_height; }
    uint32_t getMipmapCount() const { return m_mipmapCount; }
    bool isLoaded() const { return m_loaded; }
    
    // Get descriptor image info (ready for descriptor set updates)
    VkDescriptorImageInfo getDescriptorImageInfo() const {
        VkDescriptorImageInfo imageInfo = {};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = m_imageView;
        imageInfo.sampler = m_sampler;
        return imageInfo;
    }
};

#endif // EDEN_TEXTURE_RESOURCE_H

