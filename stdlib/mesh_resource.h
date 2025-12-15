// EDEN ENGINE - MeshResource Class
// Loads OBJ files and creates Vulkan vertex/index buffers ready for rendering
// Supports textured OBJs with UV coordinates

#ifndef EDEN_MESH_RESOURCE_H
#define EDEN_MESH_RESOURCE_H

#include "vulkan.h"
#include "obj_loader.h"
#include <vector>
#include <string>
#include <cstring>

// Forward declarations for Vulkan globals (same as TextureResource)
extern VkDevice g_device;
extern VkPhysicalDevice g_physicalDevice;
extern VkCommandPool g_commandPool;
extern VkQueue g_graphicsQueue;

// Mesh vertex structure (for OBJ meshes with position, normal, UV)
struct MeshVertex {
    float pos[3];      // Position (x, y, z)
    float normal[3];   // Normal (nx, ny, nz)
    float uv[2];       // Texture coordinates (u, v) - for textured OBJs
};

/**
 * MeshResource - Loads OBJ files and creates Vulkan buffers
 * 
 * Supports:
 * - Basic geometry (vertices, normals)
 * - Textured OBJs (UV coordinates)
 * - Automatic GPU buffer creation
 */
class MeshResource {
private:
    VkBuffer m_vertexBuffer = VK_NULL_HANDLE;
    VkBuffer m_indexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_vertexBufferMemory = VK_NULL_HANDLE;
    VkDeviceMemory m_indexBufferMemory = VK_NULL_HANDLE;
    
    uint32_t m_indexCount = 0;
    bool m_loaded = false;
    bool m_hasNormals = false;
    bool m_hasTexcoords = false;
    
    // Helper function to find memory type (same as TextureResource)
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
    
    // Create buffer helper
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
    
    // Load OBJ and create Vulkan buffers
    void loadOBJ(const std::string& filepath) {
        // Parse OBJ file
        MeshData meshData = load_obj(filepath);
        
        m_hasNormals = meshData.hasNormals;
        m_hasTexcoords = meshData.hasTexcoords;
        m_indexCount = meshData.indexCount;
        
        // Convert MeshData to interleaved vertex format
        m_vertices.clear();
        m_vertices.reserve(meshData.vertexCount);
        
        for (size_t i = 0; i < meshData.vertexCount; i++) {
            MeshVertex vertex = {};
            
            // Position
            vertex.pos[0] = meshData.positions[i * 3 + 0];
            vertex.pos[1] = meshData.positions[i * 3 + 1];
            vertex.pos[2] = meshData.positions[i * 3 + 2];
            
            // Normal
            if (m_hasNormals && i * 3 + 2 < meshData.normals.size()) {
                vertex.normal[0] = meshData.normals[i * 3 + 0];
                vertex.normal[1] = meshData.normals[i * 3 + 1];
                vertex.normal[2] = meshData.normals[i * 3 + 2];
            } else {
                vertex.normal[0] = 0.0f;
                vertex.normal[1] = 0.0f;
                vertex.normal[2] = 1.0f; // Default normal
            }
            
            // UV coordinates (for textured OBJs)
            if (m_hasTexcoords && i * 2 + 1 < meshData.texcoords.size()) {
                vertex.uv[0] = meshData.texcoords[i * 2 + 0];
                vertex.uv[1] = meshData.texcoords[i * 2 + 1];
            } else {
                vertex.uv[0] = 0.0f;
                vertex.uv[1] = 0.0f; // Default UV
            }
            
            m_vertices.push_back(vertex);
        }
        
        // Store indices
        m_indices = meshData.indices;
        
        // Create vertex buffer
        VkDeviceSize vertexBufferSize = sizeof(MeshVertex) * m_vertices.size();
        createBuffer(vertexBufferSize, 
                    VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                    m_vertexBuffer, m_vertexBufferMemory);
        
        // Create staging buffer for vertex data
        VkBuffer stagingVertexBuffer;
        VkDeviceMemory stagingVertexBufferMemory;
        createBuffer(vertexBufferSize,
                    VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                    stagingVertexBuffer, stagingVertexBufferMemory);
        
        // Copy vertex data to staging buffer
        void* data;
        vkMapMemory(g_device, stagingVertexBufferMemory, 0, vertexBufferSize, 0, &data);
        memcpy(data, m_vertices.data(), (size_t)vertexBufferSize);
        vkUnmapMemory(g_device, stagingVertexBufferMemory);
        
        // Copy staging buffer to vertex buffer (using command buffer)
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
        
        VkBufferCopy copyRegion = {};
        copyRegion.size = vertexBufferSize;
        vkCmdCopyBuffer(commandBuffer, stagingVertexBuffer, m_vertexBuffer, 1, &copyRegion);
        
        vkEndCommandBuffer(commandBuffer);
        
        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;
        
        vkQueueSubmit(g_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(g_graphicsQueue);
        
        vkFreeCommandBuffers(g_device, g_commandPool, 1, &commandBuffer);
        vkDestroyBuffer(g_device, stagingVertexBuffer, nullptr);
        vkFreeMemory(g_device, stagingVertexBufferMemory, nullptr);
        
        // Create index buffer
        VkDeviceSize indexBufferSize = sizeof(uint32_t) * m_indices.size();
        createBuffer(indexBufferSize,
                    VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                    m_indexBuffer, m_indexBufferMemory);
        
        // Create staging buffer for index data
        VkBuffer stagingIndexBuffer;
        VkDeviceMemory stagingIndexBufferMemory;
        createBuffer(indexBufferSize,
                    VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                    stagingIndexBuffer, stagingIndexBufferMemory);
        
        // Copy index data to staging buffer
        vkMapMemory(g_device, stagingIndexBufferMemory, 0, indexBufferSize, 0, &data);
        memcpy(data, m_indices.data(), (size_t)indexBufferSize);
        vkUnmapMemory(g_device, stagingIndexBufferMemory);
        
        // Copy staging buffer to index buffer
        vkAllocateCommandBuffers(g_device, &allocInfo, &commandBuffer);
        vkBeginCommandBuffer(commandBuffer, &beginInfo);
        
        copyRegion.size = indexBufferSize;
        vkCmdCopyBuffer(commandBuffer, stagingIndexBuffer, m_indexBuffer, 1, &copyRegion);
        
        vkEndCommandBuffer(commandBuffer);
        
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;
        
        vkQueueSubmit(g_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(g_graphicsQueue);
        
        vkFreeCommandBuffers(g_device, g_commandPool, 1, &commandBuffer);
        vkDestroyBuffer(g_device, stagingIndexBuffer, nullptr);
        vkFreeMemory(g_device, stagingIndexBufferMemory, nullptr);
    }

public:
    // Stored vertex/index data for HDM export
    std::vector<MeshVertex> m_vertices;
    std::vector<uint32_t> m_indices;
    
public:
    /**
     * Default constructor - creates empty mesh (use createFromData)
     */
    MeshResource() = default;
    
    /**
     * Constructor - Loads OBJ file and creates Vulkan buffers
     * @param filepath Path to OBJ file
     * @throws std::runtime_error if loading or buffer creation fails
     */
    MeshResource(const std::string& filepath) {
        try {
            loadOBJ(filepath);
            m_loaded = true;
        } catch (const std::exception& e) {
            cleanup();
            throw;
        }
    }
    
    /**
     * Create mesh from raw vertex/index data (for HDM loading)
     * @param device Vulkan device
     * @param physicalDevice Vulkan physical device  
     * @param vertices Vertex data (MeshVertex format)
     * @param indices Index data
     * @return true if successful
     */
    bool createFromData(VkDevice device, VkPhysicalDevice physicalDevice,
                        const std::vector<MeshVertex>& vertices, 
                        const std::vector<uint32_t>& indices) {
        // Copy vertices directly (already in MeshVertex format)
        m_vertices = vertices;
        
        m_indices = indices;
        m_indexCount = indices.size();
        m_hasNormals = true;
        m_hasTexcoords = true;
        
        // Create vertex buffer
        VkDeviceSize vertexBufferSize = sizeof(MeshVertex) * m_vertices.size();
        if (vertexBufferSize == 0) return false;
        
        try {
            createBuffer(vertexBufferSize, 
                        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                        m_vertexBuffer, m_vertexBufferMemory);
            
            // Create staging buffer for vertex data
            VkBuffer stagingVertexBuffer;
            VkDeviceMemory stagingVertexBufferMemory;
            createBuffer(vertexBufferSize,
                        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                        stagingVertexBuffer, stagingVertexBufferMemory);
            
            // Copy vertex data to staging buffer
            void* data;
            vkMapMemory(g_device, stagingVertexBufferMemory, 0, vertexBufferSize, 0, &data);
            memcpy(data, m_vertices.data(), (size_t)vertexBufferSize);
            vkUnmapMemory(g_device, stagingVertexBufferMemory);
            
            // Copy staging buffer to vertex buffer
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
            
            VkBufferCopy copyRegion = {};
            copyRegion.size = vertexBufferSize;
            vkCmdCopyBuffer(commandBuffer, stagingVertexBuffer, m_vertexBuffer, 1, &copyRegion);
            
            vkEndCommandBuffer(commandBuffer);
            
            VkSubmitInfo submitInfo = {};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &commandBuffer;
            
            vkQueueSubmit(g_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
            vkQueueWaitIdle(g_graphicsQueue);
            
            vkFreeCommandBuffers(g_device, g_commandPool, 1, &commandBuffer);
            vkDestroyBuffer(g_device, stagingVertexBuffer, nullptr);
            vkFreeMemory(g_device, stagingVertexBufferMemory, nullptr);
            
            // Create index buffer
            VkDeviceSize indexBufferSize = sizeof(uint32_t) * m_indices.size();
            createBuffer(indexBufferSize,
                        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                        m_indexBuffer, m_indexBufferMemory);
            
            // Create staging buffer for index data
            VkBuffer stagingIndexBuffer;
            VkDeviceMemory stagingIndexBufferMemory;
            createBuffer(indexBufferSize,
                        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                        stagingIndexBuffer, stagingIndexBufferMemory);
            
            // Copy index data to staging buffer
            vkMapMemory(g_device, stagingIndexBufferMemory, 0, indexBufferSize, 0, &data);
            memcpy(data, m_indices.data(), (size_t)indexBufferSize);
            vkUnmapMemory(g_device, stagingIndexBufferMemory);
            
            // Copy staging buffer to index buffer
            vkAllocateCommandBuffers(g_device, &allocInfo, &commandBuffer);
            vkBeginCommandBuffer(commandBuffer, &beginInfo);
            
            copyRegion.size = indexBufferSize;
            vkCmdCopyBuffer(commandBuffer, stagingIndexBuffer, m_indexBuffer, 1, &copyRegion);
            
            vkEndCommandBuffer(commandBuffer);
            
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &commandBuffer;
            
            vkQueueSubmit(g_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
            vkQueueWaitIdle(g_graphicsQueue);
            
            vkFreeCommandBuffers(g_device, g_commandPool, 1, &commandBuffer);
            vkDestroyBuffer(g_device, stagingIndexBuffer, nullptr);
            vkFreeMemory(g_device, stagingIndexBufferMemory, nullptr);
            
            m_loaded = true;
            return true;
        } catch (const std::exception& e) {
            cleanup();
            return false;
        }
    }
    
    // Move constructor
    MeshResource(MeshResource&& other) noexcept 
        : m_vertexBuffer(other.m_vertexBuffer), m_indexBuffer(other.m_indexBuffer),
          m_vertexBufferMemory(other.m_vertexBufferMemory), m_indexBufferMemory(other.m_indexBufferMemory),
          m_indexCount(other.m_indexCount), m_loaded(other.m_loaded),
          m_hasNormals(other.m_hasNormals), m_hasTexcoords(other.m_hasTexcoords) {
        other.m_vertexBuffer = VK_NULL_HANDLE;
        other.m_indexBuffer = VK_NULL_HANDLE;
        other.m_vertexBufferMemory = VK_NULL_HANDLE;
        other.m_indexBufferMemory = VK_NULL_HANDLE;
        other.m_loaded = false;
    }
    
    // Delete copy constructor and assignment (meshes are unique resources)
    MeshResource(const MeshResource&) = delete;
    MeshResource& operator=(const MeshResource&) = delete;
    
    // Destructor - RAII cleanup
    ~MeshResource() {
        cleanup();
    }
    
    // Cleanup helper
    void cleanup() {
        if (m_indexBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(g_device, m_indexBuffer, nullptr);
            m_indexBuffer = VK_NULL_HANDLE;
        }
        if (m_indexBufferMemory != VK_NULL_HANDLE) {
            vkFreeMemory(g_device, m_indexBufferMemory, nullptr);
            m_indexBufferMemory = VK_NULL_HANDLE;
        }
        if (m_vertexBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(g_device, m_vertexBuffer, nullptr);
            m_vertexBuffer = VK_NULL_HANDLE;
        }
        if (m_vertexBufferMemory != VK_NULL_HANDLE) {
            vkFreeMemory(g_device, m_vertexBufferMemory, nullptr);
            m_vertexBufferMemory = VK_NULL_HANDLE;
        }
        m_loaded = false;
    }
    
    // Getters for Vulkan resources
    VkBuffer getVertexBuffer() const { return m_vertexBuffer; }
    VkBuffer getIndexBuffer() const { return m_indexBuffer; }
    uint32_t getIndexCount() const { return m_indexCount; }
    bool hasNormals() const { return m_hasNormals; }
    bool hasTexcoords() const { return m_hasTexcoords; }
    bool isLoaded() const { return m_loaded; }
    
    // Getters for raw data (for HDM export)
    const std::vector<MeshVertex>& getVertices() const { return m_vertices; }
    std::vector<MeshVertex>& getVerticesMutable() { return m_vertices; }
    const std::vector<uint32_t>& getIndices() const { return m_indices; }
    std::vector<uint32_t>& getIndicesMutable() { return m_indices; }
    
    /**
     * Rebuild GPU buffers after modifying vertex/index data
     * Call this after changing vertices via getVerticesMutable() or indices via getIndicesMutable()
     */
    void rebuildBuffers() {
        if (m_vertices.empty()) return;
        
        // CRITICAL: Wait for GPU to finish using the old buffers before destroying them
        vkDeviceWaitIdle(g_device);
        
        // Destroy old buffers
        if (m_vertexBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(g_device, m_vertexBuffer, nullptr);
            m_vertexBuffer = VK_NULL_HANDLE;
        }
        if (m_vertexBufferMemory != VK_NULL_HANDLE) {
            vkFreeMemory(g_device, m_vertexBufferMemory, nullptr);
            m_vertexBufferMemory = VK_NULL_HANDLE;
        }
        if (m_indexBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(g_device, m_indexBuffer, nullptr);
            m_indexBuffer = VK_NULL_HANDLE;
        }
        if (m_indexBufferMemory != VK_NULL_HANDLE) {
            vkFreeMemory(g_device, m_indexBufferMemory, nullptr);
            m_indexBufferMemory = VK_NULL_HANDLE;
        }
        
        try {
            // Create new vertex buffer
            VkDeviceSize vertexBufferSize = sizeof(MeshVertex) * m_vertices.size();
            createBuffer(vertexBufferSize, 
                        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                        m_vertexBuffer, m_vertexBufferMemory);
            
            // Staging buffer for vertices
            VkBuffer stagingBuffer;
            VkDeviceMemory stagingBufferMemory;
            createBuffer(vertexBufferSize,
                        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                        stagingBuffer, stagingBufferMemory);
            
            void* data;
            vkMapMemory(g_device, stagingBufferMemory, 0, vertexBufferSize, 0, &data);
            memcpy(data, m_vertices.data(), (size_t)vertexBufferSize);
            vkUnmapMemory(g_device, stagingBufferMemory);
            
            // Copy to GPU
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
            
            VkBufferCopy copyRegion = {};
            copyRegion.size = vertexBufferSize;
            vkCmdCopyBuffer(commandBuffer, stagingBuffer, m_vertexBuffer, 1, &copyRegion);
            
            vkEndCommandBuffer(commandBuffer);
            
            VkSubmitInfo submitInfo = {};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &commandBuffer;
            
            vkQueueSubmit(g_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
            vkQueueWaitIdle(g_graphicsQueue);
            
            vkFreeCommandBuffers(g_device, g_commandPool, 1, &commandBuffer);
            vkDestroyBuffer(g_device, stagingBuffer, nullptr);
            vkFreeMemory(g_device, stagingBufferMemory, nullptr);
            
            // Create new index buffer
            if (!m_indices.empty()) {
                m_indexCount = m_indices.size();
                VkDeviceSize indexBufferSize = sizeof(uint32_t) * m_indices.size();
                
                createBuffer(indexBufferSize,
                            VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                            m_indexBuffer, m_indexBufferMemory);
                
                VkBuffer stagingIndexBuffer;
                VkDeviceMemory stagingIndexBufferMemory;
                createBuffer(indexBufferSize,
                            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                            stagingIndexBuffer, stagingIndexBufferMemory);
                
                vkMapMemory(g_device, stagingIndexBufferMemory, 0, indexBufferSize, 0, &data);
                memcpy(data, m_indices.data(), (size_t)indexBufferSize);
                vkUnmapMemory(g_device, stagingIndexBufferMemory);
                
                vkAllocateCommandBuffers(g_device, &allocInfo, &commandBuffer);
                vkBeginCommandBuffer(commandBuffer, &beginInfo);
                
                copyRegion.size = indexBufferSize;
                vkCmdCopyBuffer(commandBuffer, stagingIndexBuffer, m_indexBuffer, 1, &copyRegion);
                
                vkEndCommandBuffer(commandBuffer);
                
                submitInfo.pCommandBuffers = &commandBuffer;
                vkQueueSubmit(g_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
                vkQueueWaitIdle(g_graphicsQueue);
                
                vkFreeCommandBuffers(g_device, g_commandPool, 1, &commandBuffer);
                vkDestroyBuffer(g_device, stagingIndexBuffer, nullptr);
                vkFreeMemory(g_device, stagingIndexBufferMemory, nullptr);
            }
            
        } catch (const std::exception& e) {
            std::cerr << "[MeshResource] Failed to rebuild buffers: " << e.what() << std::endl;
        }
    }
    
    // Get vertex input binding description (for pipeline creation)
    VkVertexInputBindingDescription getVertexInputBinding() const {
        VkVertexInputBindingDescription binding = {};
        binding.binding = 0;
        binding.stride = sizeof(MeshVertex);
        binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return binding;
    }
    
    // Get vertex input attribute descriptions (for pipeline creation)
    std::vector<VkVertexInputAttributeDescription> getVertexInputAttributes() const {
        std::vector<VkVertexInputAttributeDescription> attributes(3);
        
        // Position
        attributes[0].binding = 0;
        attributes[0].location = 0;
        attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributes[0].offset = offsetof(MeshVertex, pos);
        
        // Normal
        attributes[1].binding = 0;
        attributes[1].location = 1;
        attributes[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributes[1].offset = offsetof(MeshVertex, normal);
        
        // UV coordinates (for textured OBJs)
        attributes[2].binding = 0;
        attributes[2].location = 2;
        attributes[2].format = VK_FORMAT_R32G32_SFLOAT;
        attributes[2].offset = offsetof(MeshVertex, uv);
        
        return attributes;
    }
};

#endif // EDEN_MESH_RESOURCE_H

