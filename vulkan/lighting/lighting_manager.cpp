// ============================================================================
// LIGHTING MANAGER - Implementation
// ============================================================================

#include "lighting_manager.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <fstream>
#include <vector>
#include <set>

namespace lighting {

// ============================================================================
// Helper: Read shader file
// ============================================================================

static std::vector<char> readShaderFile(const std::string& path) {
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[Lighting] Failed to open shader: " << path << std::endl;
        return {};
    }
    
    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    return buffer;
}

// ============================================================================
// Constructor / Destructor
// ============================================================================

LightingManager::LightingManager() {
    // Defaults are set in member initializers and LightingUBO constructor
}

LightingManager::~LightingManager() {
    if (m_initialized) {
        shutdown();
    }
}

// ============================================================================
// Lifecycle
// ============================================================================

bool LightingManager::init(vkcore::VulkanCore* core) {
    if (m_initialized) {
        std::cerr << "[Lighting] Already initialized!" << std::endl;
        return false;
    }
    
    if (!core || !core->isInitialized()) {
        std::cerr << "[Lighting] Invalid or uninitialized VulkanCore!" << std::endl;
        return false;
    }
    
    m_core = core;
    
    std::cout << "[Lighting] Initializing..." << std::endl;
    
    // Create resources
    if (!createLightingResources()) {
        std::cerr << "[Lighting] Failed to create lighting resources!" << std::endl;
        return false;
    }
    
    if (!createLitPipeline()) {
        std::cerr << "[Lighting] Failed to create lit pipeline!" << std::endl;
        return false;
    }
    
    // Initialize UBO with default values
    updateGPUBuffer();
    
    m_initialized = true;
    std::cout << "[Lighting] Initialized successfully" << std::endl;
    
    return true;
}

void LightingManager::shutdown() {
    if (!m_initialized || !m_core) {
        return;
    }
    
    std::cout << "[Lighting] Shutting down..." << std::endl;
    
    VkDevice device = m_core->getDevice();
    vkDeviceWaitIdle(device);
    
    // Destroy pipeline
    if (m_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, m_pipeline, nullptr);
        m_pipeline = VK_NULL_HANDLE;
    }
    
    if (m_pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, m_pipelineLayout, nullptr);
        m_pipelineLayout = VK_NULL_HANDLE;
    }
    
    // Free all descriptor sets before destroying the pool
    if (m_descriptorPool != VK_NULL_HANDLE) {
        // Free all allocated descriptor sets
        std::vector<VkDescriptorSet> setsToFree;
        for (const auto& pair : m_textureDescriptorSets) {
            setsToFree.push_back(pair.second);
        }
        if (!setsToFree.empty()) {
            vkFreeDescriptorSets(device, m_descriptorPool, static_cast<uint32_t>(setsToFree.size()), setsToFree.data());
        }
        m_textureDescriptorSets.clear();
        m_currentDescriptorSet = VK_NULL_HANDLE;
        
        vkDestroyDescriptorPool(device, m_descriptorPool, nullptr);
        m_descriptorPool = VK_NULL_HANDLE;
    }
    
    if (m_descriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, m_descriptorSetLayout, nullptr);
        m_descriptorSetLayout = VK_NULL_HANDLE;
    }
    
    if (m_uboBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, m_uboBuffer, nullptr);
        m_uboBuffer = VK_NULL_HANDLE;
    }
    
    if (m_uboMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, m_uboMemory, nullptr);
        m_uboMemory = VK_NULL_HANDLE;
    }
    
    // Cleanup default texture
    if (m_defaultSampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, m_defaultSampler, nullptr);
        m_defaultSampler = VK_NULL_HANDLE;
    }
    
    if (m_defaultTexView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, m_defaultTexView, nullptr);
        m_defaultTexView = VK_NULL_HANDLE;
    }
    
    if (m_defaultTexImage != VK_NULL_HANDLE) {
        vkDestroyImage(device, m_defaultTexImage, nullptr);
        m_defaultTexImage = VK_NULL_HANDLE;
    }
    
    if (m_defaultTexMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, m_defaultTexMemory, nullptr);
        m_defaultTexMemory = VK_NULL_HANDLE;
    }
    
    m_core = nullptr;
    m_initialized = false;
    m_currentDescriptorSet = VK_NULL_HANDLE;
    m_litPipeline = vkcore::INVALID_PIPELINE;
    
    std::cout << "[Lighting] Shutdown complete" << std::endl;
}

// ============================================================================
// Light Control
// ============================================================================

void LightingManager::setDirectionalLight(const glm::vec3& direction, const glm::vec3& color, float intensity) {
    m_directionalLight.direction = glm::normalize(direction);
    m_directionalLight.color = color;
    m_directionalLight.intensity = intensity;
}

void LightingManager::setDirectionalLight(const DirectionalLight& light) {
    m_directionalLight = light;
    m_directionalLight.direction = glm::normalize(m_directionalLight.direction);
}

void LightingManager::setAmbientLight(const glm::vec3& color) {
    m_ambientLight.color = color;
}

void LightingManager::setAmbientLight(const AmbientLight& light) {
    m_ambientLight = light;
}

void LightingManager::setCameraPosition(const glm::vec3& position) {
    m_cameraPos = position;
}

void LightingManager::setShininess(float shininess) {
    m_shininess = glm::clamp(shininess, 1.0f, 256.0f);
}

void LightingManager::setSpecularStrength(float strength) {
    m_specularStrength = glm::clamp(strength, 0.0f, 1.0f);
}

void LightingManager::setDebugMode(int mode) {
    m_debugMode = mode;
}

// ============================================================================
// Point Lights
// ============================================================================

bool LightingManager::setPointLight(int index, const glm::vec3& position, const glm::vec3& color,
                                     float intensity, float range) {
    if (index < 0 || index >= MAX_POINT_LIGHTS) return false;
    
    m_pointLights[index].position = position;
    m_pointLights[index].color = color;
    m_pointLights[index].intensity = intensity;
    m_pointLights[index].range = range;
    m_pointLights[index].enabled = true;
    return true;
}

bool LightingManager::setPointLight(int index, const PointLight& light) {
    if (index < 0 || index >= MAX_POINT_LIGHTS) return false;
    m_pointLights[index] = light;
    return true;
}

void LightingManager::enablePointLight(int index, bool enabled) {
    if (index >= 0 && index < MAX_POINT_LIGHTS) {
        m_pointLights[index].enabled = enabled;
    }
}

static PointLight s_defaultPointLight;  // Default for invalid index

const PointLight& LightingManager::getPointLight(int index) const {
    if (index >= 0 && index < MAX_POINT_LIGHTS) {
        return m_pointLights[index];
    }
    return s_defaultPointLight;
}

void LightingManager::clearPointLights() {
    for (int i = 0; i < MAX_POINT_LIGHTS; ++i) {
        m_pointLights[i] = PointLight();
    }
}

int LightingManager::getActivePointLightCount() const {
    int count = 0;
    for (int i = 0; i < MAX_POINT_LIGHTS; ++i) {
        if (m_pointLights[i].enabled && m_pointLights[i].intensity > 0.0f) {
            count++;
        }
    }
    return count;
}

// ============================================================================
// Texture Support
// ============================================================================

// Helper: Get or create a descriptor set for a given texture
// Each texture gets its own descriptor set, preventing texture sharing
VkDescriptorSet LightingManager::getOrCreateDescriptorSet(vkcore::TextureHandle texture) {
    if (!m_initialized || !m_core) return VK_NULL_HANDLE;
    
    // Use special handle for default texture
    vkcore::TextureHandle lookupHandle = (texture == vkcore::INVALID_TEXTURE) ? m_defaultTextureHandle : texture;
    
    // Check if we already have a descriptor set for this texture
    auto it = m_textureDescriptorSets.find(lookupHandle);
    if (it != m_textureDescriptorSets.end()) {
        return it->second;
    }
    
    // Need to create a new descriptor set for this texture
    VkDevice device = m_core->getDevice();
    
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &m_descriptorSetLayout;
    
    VkDescriptorSet newSet = VK_NULL_HANDLE;
    if (vkAllocateDescriptorSets(device, &allocInfo, &newSet) != VK_SUCCESS) {
        std::cerr << "[Lighting] Failed to allocate descriptor set for texture!" << std::endl;
        return VK_NULL_HANDLE;
    }
    
    // Get texture info from VulkanCore, or use default
    VkImageView view = m_defaultTexView;
    VkSampler sampler = m_defaultSampler;
    bool usingLoadedTex = false;
    
    if (texture != vkcore::INVALID_TEXTURE) {
        if (m_core->getTextureInfo(texture, view, sampler)) {
            usingLoadedTex = true;
        } else {
            // Texture not valid, use default
            std::cerr << "[Lighting] getTextureInfo failed for texture " << texture << ", using default" << std::endl;
            view = m_defaultTexView;
            sampler = m_defaultSampler;
        }
    }
    
    std::cout << "[Lighting] Creating descriptor set for texture " << texture 
              << " (using " << (usingLoadedTex ? "loaded" : "default") << " texture)"
              << ", view: " << (view != VK_NULL_HANDLE ? "valid" : "NULL")
              << ", sampler: " << (sampler != VK_NULL_HANDLE ? "valid" : "NULL") << std::endl;
    
    // Update the new descriptor set with UBO and texture
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = m_uboBuffer;
    bufferInfo.offset = 0;
    bufferInfo.range = sizeof(LightingUBO);
    
    // Debug: Verify UBO buffer is valid
    if (m_uboBuffer == VK_NULL_HANDLE) {
        std::cerr << "[Lighting] ERROR: UBO buffer is NULL when creating descriptor set!" << std::endl;
        return VK_NULL_HANDLE;
    }
    
    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = view;
    imageInfo.sampler = sampler;
    
    std::vector<VkWriteDescriptorSet> writes(2);
    
    // UBO binding
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = newSet;
    writes[0].dstBinding = 0;
    writes[0].dstArrayElement = 0;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[0].descriptorCount = 1;
    writes[0].pBufferInfo = &bufferInfo;
    
    // Texture binding
    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = newSet;
    writes[1].dstBinding = 1;
    writes[1].dstArrayElement = 0;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[1].descriptorCount = 1;
    writes[1].pImageInfo = &imageInfo;
    
    vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    
    // Store the new descriptor set
    m_textureDescriptorSets[lookupHandle] = newSet;
    
    return newSet;
}

void LightingManager::bindTexture(vkcore::TextureHandle texture) {
    if (!m_initialized || !m_core) return;
    
    // Get or create descriptor set for this texture
    m_currentDescriptorSet = getOrCreateDescriptorSet(texture);
    
    // Debug: Print first time a texture is bound
    static std::set<vkcore::TextureHandle> debuggedTextures;
    if (debuggedTextures.find(texture) == debuggedTextures.end()) {
        debuggedTextures.insert(texture);
        std::cout << "[Lighting] Bound texture " << texture 
                  << ", descriptorSet: " << (m_currentDescriptorSet != VK_NULL_HANDLE ? "valid" : "NULL!") << std::endl;
    }
}

// ============================================================================
// Matrices
// ============================================================================

void LightingManager::setViewMatrix(const glm::mat4& view) {
    m_viewMatrix = view;
}

void LightingManager::setProjectionMatrix(const glm::mat4& proj) {
    m_projMatrix = proj;
}

// ============================================================================
// Rendering
// ============================================================================

void LightingManager::bind() {
    if (!m_initialized || !m_core || m_pipeline == VK_NULL_HANDLE) {
        static bool warned = false;
        if (!warned) { warned = true; std::cerr << "[Lighting] bind() failed: not initialized or no pipeline" << std::endl; }
        return;
    }
    
    // Debug: Print once per session
    static bool debugPrinted = false;
    if (!debugPrinted) {
        debugPrinted = true;
        std::cout << "[Lighting] bind() called - pipeline: " << (m_pipeline != VK_NULL_HANDLE ? "valid" : "NULL")
                  << ", descriptorSet: " << (m_currentDescriptorSet != VK_NULL_HANDLE ? "valid" : "NULL") << std::endl;
    }
    
    // Update UBO with per-frame data (view, projection, lights)
    // Per-object data (model, color) is handled via push constants in drawLitMesh
    m_uboData.view = m_viewMatrix;
    m_uboData.projection = m_projMatrix;
    m_uboData.lightDir = glm::vec4(m_directionalLight.direction, m_directionalLight.intensity);
    m_uboData.lightColor = glm::vec4(m_directionalLight.color, 1.0f);
    m_uboData.ambientColor = glm::vec4(m_ambientLight.color, 1.0f);
    m_uboData.cameraPos = glm::vec4(m_cameraPos, 1.0f);
    
    // Count active point lights and pack data
    int numPointLights = 0;
    for (int i = 0; i < MAX_POINT_LIGHTS; ++i) {
        if (m_pointLights[i].enabled && m_pointLights[i].intensity > 0.0f) {
            m_uboData.pointLightPosInt[numPointLights] = glm::vec4(
                m_pointLights[i].position, m_pointLights[i].intensity);
            m_uboData.pointLightColorRange[numPointLights] = glm::vec4(
                m_pointLights[i].color, m_pointLights[i].range);
            numPointLights++;
        }
    }
    // Zero out remaining slots
    for (int i = numPointLights; i < MAX_POINT_LIGHTS; ++i) {
        m_uboData.pointLightPosInt[i] = glm::vec4(0.0f);
        m_uboData.pointLightColorRange[i] = glm::vec4(0.0f);
    }
    
    m_uboData.material = glm::vec4(m_shininess, m_specularStrength, float(numPointLights), float(m_debugMode));
    
    // Debug: Print UBO data once
    static bool uboDebugPrinted = false;
    if (!uboDebugPrinted) {
        uboDebugPrinted = true;
        std::cout << "[Lighting] UBO data:" << std::endl;
        std::cout << "  Light dir: (" << m_uboData.lightDir.x << ", " << m_uboData.lightDir.y << ", " << m_uboData.lightDir.z << "), intensity: " << m_uboData.lightDir.w << std::endl;
        std::cout << "  Light color: (" << m_uboData.lightColor.x << ", " << m_uboData.lightColor.y << ", " << m_uboData.lightColor.z << ")" << std::endl;
        std::cout << "  Ambient: (" << m_uboData.ambientColor.x << ", " << m_uboData.ambientColor.y << ", " << m_uboData.ambientColor.z << ")" << std::endl;
        std::cout << "  Camera pos: (" << m_uboData.cameraPos.x << ", " << m_uboData.cameraPos.y << ", " << m_uboData.cameraPos.z << ")" << std::endl;
        std::cout << "  Material: shininess=" << m_uboData.material.x << ", specular=" << m_uboData.material.y << ", numLights=" << m_uboData.material.z << std::endl;
        glm::vec3 projScale = glm::vec3(m_uboData.projection[0][0], m_uboData.projection[1][1], m_uboData.projection[2][2]);
        std::cout << "  Projection scale: (" << projScale.x << ", " << projScale.y << ", " << projScale.z << ")" << std::endl;
    }
    
    // Upload per-frame data to GPU
    updateGPUBuffer();
    
    VkCommandBuffer cmd = m_core->getCurrentCommandBuffer();
    
    // Bind our pipeline
    // Note: Descriptor set binding happens in drawLitMesh() after bindTexture() is called
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
}

void LightingManager::drawLitMesh(vkcore::MeshHandle mesh, 
                                   const glm::mat4& model,
                                   const glm::vec4& color) {
    if (!m_initialized || !m_core) {
        static bool warnedOnce = false;
        if (!warnedOnce) { warnedOnce = true; std::cerr << "[Lighting] drawLitMesh: not initialized!" << std::endl; }
        return;
    }
    
    // Get mesh buffers from VulkanCore
    VkBuffer vertexBuffer, indexBuffer;
    uint32_t indexCount;
    if (!m_core->getMeshBuffers(mesh, vertexBuffer, indexBuffer, indexCount)) {
        static bool warnedOnce = false;
        if (!warnedOnce) { warnedOnce = true; std::cerr << "[Lighting] drawLitMesh: getMeshBuffers failed for mesh " << mesh << std::endl; }
        return;
    }
    
    VkCommandBuffer cmd = m_core->getCurrentCommandBuffer();
    
    // Bind the descriptor set for the current texture (set by bindTexture())
    // Each texture has its own descriptor set, preventing texture sharing
    if (m_currentDescriptorSet != VK_NULL_HANDLE) {
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                m_pipelineLayout, 0, 1, &m_currentDescriptorSet, 0, nullptr);
    } else {
        static bool warnedOnce = false;
        if (!warnedOnce) { warnedOnce = true; std::cerr << "[Lighting] drawLitMesh: No descriptor set bound! Call bindTexture first." << std::endl; }
        return;  // Can't render without a descriptor set
    }
    
    // Use push constants for per-object data (model matrix and color)
    // This allows multiple objects to be drawn without UBO synchronization issues
    PushConstants pushData;
    pushData.model = model;
    pushData.objectColor = color;
    
    vkCmdPushConstants(cmd, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 
                       0, sizeof(PushConstants), &pushData);
    
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer, offsets);
    vkCmdBindIndexBuffer(cmd, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
    
    // Debug: Print first draw call
    static bool drawDebugPrinted = false;
    if (!drawDebugPrinted) {
        drawDebugPrinted = true;
        std::cout << "[Lighting] drawLitMesh: Drawing " << indexCount << " indices, mesh=" << mesh 
                  << ", vertexBuffer=" << (vertexBuffer != VK_NULL_HANDLE ? "valid" : "NULL")
                  << ", indexBuffer=" << (indexBuffer != VK_NULL_HANDLE ? "valid" : "NULL") << std::endl;
    }
    
    vkCmdDrawIndexed(cmd, indexCount, 1, 0, 0, 0);
}

// ============================================================================
// Private: Create Resources
// ============================================================================

// Helper: find memory type
static uint32_t findMemoryType(VkPhysicalDevice physDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(physDevice, &memProps);
    
    for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && 
            (memProps.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    return UINT32_MAX;
}

bool LightingManager::createLightingResources() {
    VkDevice device = m_core->getDevice();
    VkPhysicalDevice physDevice = m_core->getPhysicalDevice();
    
    // ========================================================================
    // Create UBO Buffer
    // ========================================================================
    
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = sizeof(LightingUBO);
    bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    if (vkCreateBuffer(device, &bufferInfo, nullptr, &m_uboBuffer) != VK_SUCCESS) {
        std::cerr << "[Lighting] Failed to create UBO buffer!" << std::endl;
        return false;
    }
    
    VkMemoryRequirements memReq;
    vkGetBufferMemoryRequirements(device, m_uboBuffer, &memReq);
    
    uint32_t memTypeIndex = findMemoryType(physDevice, memReq.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    
    if (memTypeIndex == UINT32_MAX) {
        std::cerr << "[Lighting] Failed to find suitable memory type!" << std::endl;
        return false;
    }
    
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = memTypeIndex;
    
    if (vkAllocateMemory(device, &allocInfo, nullptr, &m_uboMemory) != VK_SUCCESS) {
        std::cerr << "[Lighting] Failed to allocate UBO memory!" << std::endl;
        return false;
    }
    
    vkBindBufferMemory(device, m_uboBuffer, m_uboMemory, 0);
    
    // ========================================================================
    // Create Default White Texture (1x1 pixel)
    // ========================================================================
    
    // Create image
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = 1;
    imageInfo.extent.height = 1;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    
    if (vkCreateImage(device, &imageInfo, nullptr, &m_defaultTexImage) != VK_SUCCESS) {
        std::cerr << "[Lighting] Failed to create default texture image!" << std::endl;
        return false;
    }
    
    VkMemoryRequirements imgMemReq;
    vkGetImageMemoryRequirements(device, m_defaultTexImage, &imgMemReq);
    
    uint32_t imgMemType = findMemoryType(physDevice, imgMemReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (imgMemType == UINT32_MAX) {
        std::cerr << "[Lighting] Failed to find image memory type!" << std::endl;
        return false;
    }
    
    VkMemoryAllocateInfo imgAllocInfo{};
    imgAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    imgAllocInfo.allocationSize = imgMemReq.size;
    imgAllocInfo.memoryTypeIndex = imgMemType;
    
    if (vkAllocateMemory(device, &imgAllocInfo, nullptr, &m_defaultTexMemory) != VK_SUCCESS) {
        std::cerr << "[Lighting] Failed to allocate texture memory!" << std::endl;
        return false;
    }
    
    vkBindImageMemory(device, m_defaultTexImage, m_defaultTexMemory, 0);
    
    // Upload white pixel data to the texture
    {
        // Create staging buffer with white pixel
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingMemory;
        const uint32_t whitePixel = 0xFFFFFFFF;  // RGBA white
        
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = 4;  // 4 bytes for RGBA
        bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        
        if (vkCreateBuffer(device, &bufferInfo, nullptr, &stagingBuffer) != VK_SUCCESS) {
            std::cerr << "[Lighting] Failed to create staging buffer!" << std::endl;
            return false;
        }
        
        VkMemoryRequirements stagingMemReq;
        vkGetBufferMemoryRequirements(device, stagingBuffer, &stagingMemReq);
        
        uint32_t stagingMemType = findMemoryType(physDevice, stagingMemReq.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        
        VkMemoryAllocateInfo stagingAllocInfo{};
        stagingAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        stagingAllocInfo.allocationSize = stagingMemReq.size;
        stagingAllocInfo.memoryTypeIndex = stagingMemType;
        
        vkAllocateMemory(device, &stagingAllocInfo, nullptr, &stagingMemory);
        vkBindBufferMemory(device, stagingBuffer, stagingMemory, 0);
        
        // Copy white pixel to staging buffer
        void* data;
        vkMapMemory(device, stagingMemory, 0, 4, 0, &data);
        memcpy(data, &whitePixel, 4);
        vkUnmapMemory(device, stagingMemory);
        
        // Use single-time command buffer
        VkCommandBufferAllocateInfo cmdAllocInfo{};
        cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmdAllocInfo.commandPool = m_core->getCommandPool();
        cmdAllocInfo.commandBufferCount = 1;
        
        VkCommandBuffer cmd;
        vkAllocateCommandBuffers(device, &cmdAllocInfo, &cmd);
        
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmd, &beginInfo);
        
        // Transition to transfer destination
        VkImageMemoryBarrier barrier1{};
        barrier1.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier1.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier1.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier1.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier1.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier1.image = m_defaultTexImage;
        barrier1.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier1.subresourceRange.baseMipLevel = 0;
        barrier1.subresourceRange.levelCount = 1;
        barrier1.subresourceRange.baseArrayLayer = 0;
        barrier1.subresourceRange.layerCount = 1;
        barrier1.srcAccessMask = 0;
        barrier1.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            0, 0, nullptr, 0, nullptr, 1, &barrier1);
        
        // Copy staging buffer to image
        VkBufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = {0, 0, 0};
        region.imageExtent = {1, 1, 1};
        
        vkCmdCopyBufferToImage(cmd, stagingBuffer, m_defaultTexImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        
        // Transition to shader read
        VkImageMemoryBarrier barrier2{};
        barrier2.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier2.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier2.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier2.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier2.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier2.image = m_defaultTexImage;
        barrier2.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier2.subresourceRange.baseMipLevel = 0;
        barrier2.subresourceRange.levelCount = 1;
        barrier2.subresourceRange.baseArrayLayer = 0;
        barrier2.subresourceRange.layerCount = 1;
        barrier2.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier2.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0, 0, nullptr, 0, nullptr, 1, &barrier2);
        
        vkEndCommandBuffer(cmd);
        
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmd;
        vkQueueSubmit(m_core->getGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(m_core->getGraphicsQueue());
        
        // Cleanup staging resources
        vkFreeCommandBuffers(device, m_core->getCommandPool(), 1, &cmd);
        vkDestroyBuffer(device, stagingBuffer, nullptr);
        vkFreeMemory(device, stagingMemory, nullptr);
    }
    
    // Create image view
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_defaultTexImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    
    if (vkCreateImageView(device, &viewInfo, nullptr, &m_defaultTexView) != VK_SUCCESS) {
        std::cerr << "[Lighting] Failed to create texture view!" << std::endl;
        return false;
    }
    
    // Create sampler
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_WHITE;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    
    if (vkCreateSampler(device, &samplerInfo, nullptr, &m_defaultSampler) != VK_SUCCESS) {
        std::cerr << "[Lighting] Failed to create sampler!" << std::endl;
        return false;
    }
    
    // ========================================================================
    // Create Descriptor Set Layout
    // ========================================================================
    
    // UBO binding (binding 0)
    VkDescriptorSetLayoutBinding uboBinding{};
    uboBinding.binding = 0;
    uboBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboBinding.descriptorCount = 1;
    uboBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    
    // Texture sampler binding (binding 1) - for future texture support
    VkDescriptorSetLayoutBinding samplerBinding{};
    samplerBinding.binding = 1;
    samplerBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerBinding.descriptorCount = 1;
    samplerBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    
    std::vector<VkDescriptorSetLayoutBinding> bindings = {uboBinding, samplerBinding};
    
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();
    
    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &m_descriptorSetLayout) != VK_SUCCESS) {
        std::cerr << "[Lighting] Failed to create descriptor set layout!" << std::endl;
        return false;
    }
    
    // ========================================================================
    // Create Descriptor Pool
    // ========================================================================
    
    // Create descriptor pool with capacity for many descriptor sets (one per texture)
    // This allows each texture to have its own descriptor set, preventing texture sharing
    const uint32_t MAX_DESCRIPTOR_SETS = 64;  // Support up to 64 unique textures
    
    std::vector<VkDescriptorPoolSize> poolSizes = {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MAX_DESCRIPTOR_SETS},  // One UBO per set
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_DESCRIPTOR_SETS}  // One texture per set
    };
    
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = MAX_DESCRIPTOR_SETS;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;  // Allow freeing individual sets
    
    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS) {
        std::cerr << "[Lighting] Failed to create descriptor pool!" << std::endl;
        return false;
    }
    
    // Initialize default texture handle (special value for default white texture)
    // This is used as a key in the descriptor set map when INVALID_TEXTURE is passed
    m_defaultTextureHandle = vkcore::INVALID_TEXTURE - 1;  // Use a value that won't conflict with real textures
    
    // Create descriptor set for default texture (will be created on first use)
    // We don't allocate it here - it will be created when bindTexture(INVALID_TEXTURE) is called
    
    // ========================================================================
    // Create Pipeline Layout (with push constants for per-object data)
    // ========================================================================
    
    // Push constant range for model matrix and object color
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(PushConstants);  // 80 bytes (mat4 + vec4)
    
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &m_descriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
    
    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS) {
        std::cerr << "[Lighting] Failed to create pipeline layout!" << std::endl;
        return false;
    }
    
    return true;
}

bool LightingManager::createLitPipeline() {
    VkDevice device = m_core->getDevice();
    
    // Load shaders
    auto vertCode = readShaderFile("shaders/lit_mesh.vert.spv");
    auto fragCode = readShaderFile("shaders/lit_mesh.frag.spv");
    
    if (vertCode.empty() || fragCode.empty()) {
        std::cerr << "[Lighting] Failed to load shaders!" << std::endl;
        std::cerr << "[Lighting] Vertex shader size: " << vertCode.size() << ", Fragment shader size: " << fragCode.size() << std::endl;
        return false;
    }
    
    std::cout << "[Lighting] Loaded shaders: vert=" << vertCode.size() << " bytes, frag=" << fragCode.size() << " bytes" << std::endl;
    
    // Create shader modules
    VkShaderModuleCreateInfo vertModuleInfo{};
    vertModuleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    vertModuleInfo.codeSize = vertCode.size();
    vertModuleInfo.pCode = reinterpret_cast<const uint32_t*>(vertCode.data());
    
    VkShaderModule vertModule;
    if (vkCreateShaderModule(device, &vertModuleInfo, nullptr, &vertModule) != VK_SUCCESS) {
        std::cerr << "[Lighting] Failed to create vertex shader module!" << std::endl;
        return false;
    }
    
    VkShaderModuleCreateInfo fragModuleInfo{};
    fragModuleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    fragModuleInfo.codeSize = fragCode.size();
    fragModuleInfo.pCode = reinterpret_cast<const uint32_t*>(fragCode.data());
    
    VkShaderModule fragModule;
    if (vkCreateShaderModule(device, &fragModuleInfo, nullptr, &fragModule) != VK_SUCCESS) {
        vkDestroyShaderModule(device, vertModule, nullptr);
        std::cerr << "[Lighting] Failed to create fragment shader module!" << std::endl;
        return false;
    }
    
    // Shader stages
    VkPipelineShaderStageCreateInfo vertStage{};
    vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertStage.module = vertModule;
    vertStage.pName = "main";
    
    VkPipelineShaderStageCreateInfo fragStage{};
    fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStage.module = fragModule;
    fragStage.pName = "main";
    
    VkPipelineShaderStageCreateInfo stages[] = {vertStage, fragStage};
    
    // Vertex input: position (vec3) + normal (vec3) + uv (vec2)
    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = sizeof(float) * 8;  // pos(3) + normal(3) + uv(2)
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    
    std::vector<VkVertexInputAttributeDescription> attrs(3);
    attrs[0].binding = 0;
    attrs[0].location = 0;
    attrs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrs[0].offset = 0;  // position
    
    attrs[1].binding = 0;
    attrs[1].location = 1;
    attrs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrs[1].offset = sizeof(float) * 3;  // normal
    
    attrs[2].binding = 0;
    attrs[2].location = 2;
    attrs[2].format = VK_FORMAT_R32G32_SFLOAT;
    attrs[2].offset = sizeof(float) * 6;  // uv
    
    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &binding;
    vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(attrs.size());
    vertexInput.pVertexAttributeDescriptions = attrs.data();
    
    // Input assembly
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;
    
    // Viewport (dynamic)
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(m_core->getWidth());
    viewport.height = static_cast<float>(m_core->getHeight());
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    
    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = {m_core->getWidth(), m_core->getHeight()};
    
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;
    
    // Rasterizer
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;  // Disabled for compatibility with various model formats
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;
    
    // Multisampling
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    
    // Depth stencil
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;   // Re-enabled - proper depth testing
    depthStencil.depthWriteEnable = VK_TRUE;  // Re-enabled - write to depth buffer
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;
    
    // Color blending
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;
    
    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    
    // Dynamic state
    std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();
    
    // Create pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = stages;
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = m_pipelineLayout;
    pipelineInfo.renderPass = m_core->getRenderPass();
    pipelineInfo.subpass = 0;
    
    VkResult result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline);
    
    // Cleanup shader modules
    vkDestroyShaderModule(device, vertModule, nullptr);
    vkDestroyShaderModule(device, fragModule, nullptr);
    
    if (result != VK_SUCCESS) {
        std::cerr << "[Lighting] Failed to create pipeline! Error code: " << result << std::endl;
        return false;
    }
    
    std::cout << "[Lighting] Pipeline created successfully, handle: " << m_pipeline << std::endl;
    
    std::cout << "[Lighting] Lit pipeline created" << std::endl;
    return true;
}

void LightingManager::updateGPUBuffer() {
    if (!m_initialized || m_uboMemory == VK_NULL_HANDLE) return;
    
    void* data;
    vkMapMemory(m_core->getDevice(), m_uboMemory, 0, sizeof(LightingUBO), 0, &data);
    memcpy(data, &m_uboData, sizeof(LightingUBO));
    vkUnmapMemory(m_core->getDevice(), m_uboMemory);
}

} // namespace lighting

// ============================================================================
// C API Implementation
// ============================================================================

static lighting::LightingManager* g_lightingManager = nullptr;

extern "C" {

int lighting_init(void* vulkanCore) {
    if (g_lightingManager) {
        return 0;  // Already initialized
    }
    
    vkcore::VulkanCore* core = static_cast<vkcore::VulkanCore*>(vulkanCore);
    g_lightingManager = new lighting::LightingManager();
    
    if (!g_lightingManager->init(core)) {
        delete g_lightingManager;
        g_lightingManager = nullptr;
        return 0;
    }
    
    return 1;
}

void lighting_shutdown() {
    if (g_lightingManager) {
        g_lightingManager->shutdown();
        delete g_lightingManager;
        g_lightingManager = nullptr;
    }
}

int lighting_is_initialized() {
    return (g_lightingManager && g_lightingManager->isInitialized()) ? 1 : 0;
}

void lighting_set_directional(float dx, float dy, float dz, 
                               float r, float g, float b, float intensity) {
    if (g_lightingManager) {
        g_lightingManager->setDirectionalLight(
            glm::vec3(dx, dy, dz),
            glm::vec3(r, g, b),
            intensity
        );
    }
}

void lighting_set_ambient(float r, float g, float b) {
    if (g_lightingManager) {
        g_lightingManager->setAmbientLight(glm::vec3(r, g, b));
    }
}

void lighting_set_camera_pos(float x, float y, float z) {
    if (g_lightingManager) {
        g_lightingManager->setCameraPosition(glm::vec3(x, y, z));
    }
}

void lighting_set_shininess(float shininess) {
    if (g_lightingManager) {
        g_lightingManager->setShininess(shininess);
    }
}

void lighting_set_specular_strength(float strength) {
    if (g_lightingManager) {
        g_lightingManager->setSpecularStrength(strength);
    }
}

int lighting_set_point_light(int index, float px, float py, float pz,
                              float r, float g, float b, 
                              float intensity, float range) {
    if (g_lightingManager) {
        return g_lightingManager->setPointLight(index, 
            glm::vec3(px, py, pz), glm::vec3(r, g, b), intensity, range) ? 1 : 0;
    }
    return 0;
}

void lighting_enable_point_light(int index, int enabled) {
    if (g_lightingManager) {
        g_lightingManager->enablePointLight(index, enabled != 0);
    }
}

void lighting_clear_point_lights() {
    if (g_lightingManager) {
        g_lightingManager->clearPointLights();
    }
}

int lighting_get_active_point_light_count() {
    if (g_lightingManager) {
        return g_lightingManager->getActivePointLightCount();
    }
    return 0;
}

void lighting_set_view_matrix(const float* mat4) {
    if (g_lightingManager && mat4) {
        g_lightingManager->setViewMatrix(glm::make_mat4(mat4));
    }
}

void lighting_set_projection_matrix(const float* mat4) {
    if (g_lightingManager && mat4) {
        g_lightingManager->setProjectionMatrix(glm::make_mat4(mat4));
    }
}

void lighting_bind_texture(unsigned int textureHandle) {
    if (g_lightingManager) {
        g_lightingManager->bindTexture(static_cast<vkcore::TextureHandle>(textureHandle));
    }
}

void lighting_bind() {
    if (g_lightingManager) {
        g_lightingManager->bind();
    }
}

void lighting_draw_mesh(unsigned int meshHandle,
                        float px, float py, float pz,
                        float rx, float ry, float rz,
                        float sx, float sy, float sz,
                        float r, float g, float b, float a) {
    if (g_lightingManager) {
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(px, py, pz));
        model = glm::rotate(model, glm::radians(rx), glm::vec3(1, 0, 0));
        model = glm::rotate(model, glm::radians(ry), glm::vec3(0, 1, 0));
        model = glm::rotate(model, glm::radians(rz), glm::vec3(0, 0, 1));
        model = glm::scale(model, glm::vec3(sx, sy, sz));
        
        g_lightingManager->drawLitMesh(
            static_cast<vkcore::MeshHandle>(meshHandle),
            model,
            glm::vec4(r, g, b, a)
        );
    }
}

unsigned int lighting_get_pipeline() {
    if (g_lightingManager) {
        return g_lightingManager->getLitPipeline();
    }
    return vkcore::INVALID_PIPELINE;
}

} // extern "C"
