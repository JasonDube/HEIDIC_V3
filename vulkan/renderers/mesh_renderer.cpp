// EDEN ENGINE - Mesh Renderer Implementation
// Extracted from eden_vulkan_helpers.cpp for modularity

#include "mesh_renderer.h"
#include "../../stdlib/mesh_resource.h"
#include "../../stdlib/texture_resource.h"

#include <fstream>
#include <iostream>
#include <vector>
#include <array>
#include <chrono>
#include <cstring>

#ifdef _WIN32
#include <sys/stat.h>
#define stat _stat
#endif

namespace eden {

// ============================================================================
// Helper Functions
// ============================================================================

static std::vector<char> readShaderFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open shader file: " + filename);
    }
    
    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();
    
    return buffer;
}

// ============================================================================
// MeshRenderer Implementation
// ============================================================================

MeshRenderer::MeshRenderer() {
    m_ubo.model = glm::mat4(1.0f);
    m_ubo.view = glm::mat4(1.0f);
    m_ubo.proj = glm::mat4(1.0f);
}

MeshRenderer::~MeshRenderer() {
    cleanup();
}

bool MeshRenderer::init(VkDevice device, VkPhysicalDevice physicalDevice,
                        VkRenderPass renderPass, VkCommandPool commandPool,
                        VkQueue graphicsQueue, const MeshRendererConfig& config) {
    if (m_initialized) {
        std::cerr << "[MeshRenderer] Already initialized" << std::endl;
        return false;
    }
    
    m_device = device;
    m_physicalDevice = physicalDevice;
    m_renderPass = renderPass;
    m_commandPool = commandPool;
    m_graphicsQueue = graphicsQueue;
    m_config = config;
    
    std::cout << "[MeshRenderer] Initializing..." << std::endl;
    
    // Create shader modules
    if (!createShaderModules()) {
        std::cerr << "[MeshRenderer] Failed to create shader modules" << std::endl;
        return false;
    }
    
    // Create descriptor set layout
    if (!createDescriptorSetLayout()) {
        std::cerr << "[MeshRenderer] Failed to create descriptor set layout" << std::endl;
        cleanup();
        return false;
    }
    
    // Create pipeline layout
    if (!createPipelineLayout()) {
        std::cerr << "[MeshRenderer] Failed to create pipeline layout" << std::endl;
        cleanup();
        return false;
    }
    
    // Create pipelines (filled and wireframe)
    if (!createPipeline(false)) {
        std::cerr << "[MeshRenderer] Failed to create filled pipeline" << std::endl;
        cleanup();
        return false;
    }
    
    if (m_config.enableWireframe && !createPipeline(true)) {
        std::cerr << "[MeshRenderer] Failed to create wireframe pipeline" << std::endl;
        // Not fatal - wireframe is optional
    }
    
    // Create uniform buffer
    if (!createUniformBuffer()) {
        std::cerr << "[MeshRenderer] Failed to create uniform buffer" << std::endl;
        cleanup();
        return false;
    }
    
    // Create descriptor pool and set
    if (!createDescriptorPool()) {
        std::cerr << "[MeshRenderer] Failed to create descriptor pool" << std::endl;
        cleanup();
        return false;
    }
    
    if (!allocateDescriptorSet()) {
        std::cerr << "[MeshRenderer] Failed to allocate descriptor set" << std::endl;
        cleanup();
        return false;
    }
    
    // Create dummy texture
    if (!createDummyTexture()) {
        std::cerr << "[MeshRenderer] Failed to create dummy texture" << std::endl;
        cleanup();
        return false;
    }
    
    // Update descriptor set with dummy texture
    updateDescriptorSet();
    
    m_initialized = true;
    std::cout << "[MeshRenderer] Initialized successfully!" << std::endl;
    return true;
}

void MeshRenderer::cleanup() {
    if (m_device == VK_NULL_HANDLE) return;
    
    vkDeviceWaitIdle(m_device);
    
    // Cleanup mesh and texture
    m_mesh.reset();
    m_ownedTexture.reset();
    m_texture = nullptr;
    
    // Cleanup dummy texture
    if (m_dummySampler != VK_NULL_HANDLE) {
        vkDestroySampler(m_device, m_dummySampler, nullptr);
        m_dummySampler = VK_NULL_HANDLE;
    }
    if (m_dummyTextureView != VK_NULL_HANDLE) {
        vkDestroyImageView(m_device, m_dummyTextureView, nullptr);
        m_dummyTextureView = VK_NULL_HANDLE;
    }
    if (m_dummyTexture != VK_NULL_HANDLE) {
        vkDestroyImage(m_device, m_dummyTexture, nullptr);
        m_dummyTexture = VK_NULL_HANDLE;
    }
    if (m_dummyTextureMemory != VK_NULL_HANDLE) {
        vkFreeMemory(m_device, m_dummyTextureMemory, nullptr);
        m_dummyTextureMemory = VK_NULL_HANDLE;
    }
    
    // Cleanup uniform buffer
    if (m_uniformBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(m_device, m_uniformBuffer, nullptr);
        m_uniformBuffer = VK_NULL_HANDLE;
    }
    if (m_uniformBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(m_device, m_uniformBufferMemory, nullptr);
        m_uniformBufferMemory = VK_NULL_HANDLE;
    }
    
    // Cleanup descriptor resources
    if (m_descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
        m_descriptorPool = VK_NULL_HANDLE;
    }
    if (m_descriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr);
        m_descriptorSetLayout = VK_NULL_HANDLE;
    }
    
    // Cleanup pipeline resources
    if (m_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(m_device, m_pipeline, nullptr);
        m_pipeline = VK_NULL_HANDLE;
    }
    if (m_wireframePipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(m_device, m_wireframePipeline, nullptr);
        m_wireframePipeline = VK_NULL_HANDLE;
    }
    if (m_pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
        m_pipelineLayout = VK_NULL_HANDLE;
    }
    
    // Cleanup shader modules
    if (m_vertShaderModule != VK_NULL_HANDLE) {
        vkDestroyShaderModule(m_device, m_vertShaderModule, nullptr);
        m_vertShaderModule = VK_NULL_HANDLE;
    }
    if (m_fragShaderModule != VK_NULL_HANDLE) {
        vkDestroyShaderModule(m_device, m_fragShaderModule, nullptr);
        m_fragShaderModule = VK_NULL_HANDLE;
    }
    
    m_initialized = false;
    std::cout << "[MeshRenderer] Cleaned up" << std::endl;
}

bool MeshRenderer::loadMesh(const std::string& objPath) {
    try {
        std::cout << "[MeshRenderer] Loading mesh: " << objPath << std::endl;
        m_mesh = std::make_unique<MeshResource>(objPath);
        std::cout << "[MeshRenderer] Mesh loaded: " 
                  << m_mesh->getVertexCount() << " vertices, "
                  << m_mesh->getIndexCount() << " indices" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[MeshRenderer] Failed to load mesh: " << e.what() << std::endl;
        return false;
    }
}

bool MeshRenderer::loadMesh(std::unique_ptr<MeshResource> mesh) {
    if (!mesh) {
        std::cerr << "[MeshRenderer] Null mesh provided" << std::endl;
        return false;
    }
    m_mesh = std::move(mesh);
    return true;
}

bool MeshRenderer::loadTexture(const std::string& texturePath) {
    try {
        std::cout << "[MeshRenderer] Loading texture: " << texturePath << std::endl;
        m_ownedTexture = std::make_unique<TextureResource>(texturePath);
        m_texture = m_ownedTexture.get();
        m_texturePath = texturePath;
        
        // Get file modification time for hot-reload
        struct stat fileStat;
        if (stat(texturePath.c_str(), &fileStat) == 0) {
            m_textureLastModified = fileStat.st_mtime;
        }
        
        updateDescriptorSet();
        std::cout << "[MeshRenderer] Texture loaded successfully!" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[MeshRenderer] Failed to load texture: " << e.what() << std::endl;
        clearTexture();
        return false;
    }
}

bool MeshRenderer::setTexture(TextureResource* texture) {
    m_ownedTexture.reset();
    m_texture = texture;
    m_texturePath.clear();
    m_textureLastModified = 0;
    updateDescriptorSet();
    return true;
}

void MeshRenderer::clearTexture() {
    m_ownedTexture.reset();
    m_texture = nullptr;
    m_texturePath.clear();
    m_textureLastModified = 0;
    updateDescriptorSet();
}

void MeshRenderer::render(VkCommandBuffer commandBuffer) {
    if (!m_initialized || !m_mesh) return;
    
    // Update uniform buffer
    updateUniformBuffer();
    
    // Bind pipeline
    VkPipeline pipeline = m_wireframeEnabled && m_wireframePipeline != VK_NULL_HANDLE 
                          ? m_wireframePipeline : m_pipeline;
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    
    // Bind descriptor set
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, 
                            m_pipelineLayout, 0, 1, &m_descriptorSet, 0, nullptr);
    
    // Bind vertex buffer
    VkBuffer vertexBuffers[] = {m_mesh->getVertexBuffer()};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
    
    // Bind index buffer
    vkCmdBindIndexBuffer(commandBuffer, m_mesh->getIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);
    
    // Draw
    vkCmdDrawIndexed(commandBuffer, m_mesh->getIndexCount(), 1, 0, 0, 0);
}

void MeshRenderer::setModelMatrix(const glm::mat4& model) {
    m_ubo.model = model;
}

void MeshRenderer::setViewMatrix(const glm::mat4& view) {
    m_ubo.view = view;
}

void MeshRenderer::setProjectionMatrix(const glm::mat4& proj) {
    m_ubo.proj = proj;
}

void MeshRenderer::setMatrices(const glm::mat4& model, const glm::mat4& view, const glm::mat4& proj) {
    m_ubo.model = model;
    m_ubo.view = view;
    m_ubo.proj = proj;
}

void MeshRenderer::setWireframe(bool enabled) {
    m_wireframeEnabled = enabled && m_wireframePipeline != VK_NULL_HANDLE;
}

void MeshRenderer::checkHotReload() {
    if (!m_config.enableHotReload || m_texturePath.empty() || !m_ownedTexture) {
        return;
    }
    
    struct stat fileStat;
    if (stat(m_texturePath.c_str(), &fileStat) == 0) {
        if (fileStat.st_mtime > m_textureLastModified) {
            std::cout << "[MeshRenderer] Texture changed, reloading: " << m_texturePath << std::endl;
            
            vkDeviceWaitIdle(m_device);
            
            try {
                m_ownedTexture = std::make_unique<TextureResource>(m_texturePath);
                m_texture = m_ownedTexture.get();
                m_textureLastModified = fileStat.st_mtime;
                updateDescriptorSet();
                std::cout << "[MeshRenderer] Texture reloaded!" << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "[MeshRenderer] Reload failed: " << e.what() << std::endl;
            }
        }
    }
}

// ============================================================================
// Internal Helpers
// ============================================================================

bool MeshRenderer::createShaderModules() {
    std::vector<char> vertCode, fragCode;
    
    // Try configured paths first
    try {
        vertCode = readShaderFile(m_config.vertexShaderPath);
        fragCode = readShaderFile(m_config.fragmentShaderPath);
    } catch (const std::exception&) {
        // Try alternate paths
        try {
            vertCode = readShaderFile("mesh.vert.spv");
            fragCode = readShaderFile("mesh.frag.spv");
        } catch (const std::exception& e) {
            std::cerr << "[MeshRenderer] Could not load shaders: " << e.what() << std::endl;
            return false;
        }
    }
    
    VkShaderModuleCreateInfo vertInfo = {};
    vertInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    vertInfo.codeSize = vertCode.size();
    vertInfo.pCode = reinterpret_cast<const uint32_t*>(vertCode.data());
    
    if (vkCreateShaderModule(m_device, &vertInfo, nullptr, &m_vertShaderModule) != VK_SUCCESS) {
        return false;
    }
    
    VkShaderModuleCreateInfo fragInfo = {};
    fragInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    fragInfo.codeSize = fragCode.size();
    fragInfo.pCode = reinterpret_cast<const uint32_t*>(fragCode.data());
    
    if (vkCreateShaderModule(m_device, &fragInfo, nullptr, &m_fragShaderModule) != VK_SUCCESS) {
        vkDestroyShaderModule(m_device, m_vertShaderModule, nullptr);
        m_vertShaderModule = VK_NULL_HANDLE;
        return false;
    }
    
    return true;
}

bool MeshRenderer::createDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding uboBinding = {};
    uboBinding.binding = 0;
    uboBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboBinding.descriptorCount = 1;
    uboBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    
    VkDescriptorSetLayoutBinding samplerBinding = {};
    samplerBinding.binding = 1;
    samplerBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerBinding.descriptorCount = 1;
    samplerBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    
    VkDescriptorSetLayoutBinding bindings[] = {uboBinding, samplerBinding};
    
    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 2;
    layoutInfo.pBindings = bindings;
    
    return vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, &m_descriptorSetLayout) == VK_SUCCESS;
}

bool MeshRenderer::createPipelineLayout() {
    VkPipelineLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &m_descriptorSetLayout;
    
    return vkCreatePipelineLayout(m_device, &layoutInfo, nullptr, &m_pipelineLayout) == VK_SUCCESS;
}

bool MeshRenderer::createPipeline(bool wireframe) {
    // Shader stages
    VkPipelineShaderStageCreateInfo vertStage = {};
    vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertStage.module = m_vertShaderModule;
    vertStage.pName = "main";
    
    VkPipelineShaderStageCreateInfo fragStage = {};
    fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStage.module = m_fragShaderModule;
    fragStage.pName = "main";
    
    VkPipelineShaderStageCreateInfo shaderStages[] = {vertStage, fragStage};
    
    // Vertex input (matches MeshVertex)
    VkVertexInputBindingDescription bindingDesc = {};
    bindingDesc.binding = 0;
    bindingDesc.stride = sizeof(float) * 8;  // pos(3) + normal(3) + uv(2)
    bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    
    VkVertexInputAttributeDescription attrDescs[3] = {};
    attrDescs[0].binding = 0;
    attrDescs[0].location = 0;
    attrDescs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrDescs[0].offset = 0;
    
    attrDescs[1].binding = 0;
    attrDescs[1].location = 1;
    attrDescs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrDescs[1].offset = sizeof(float) * 3;
    
    attrDescs[2].binding = 0;
    attrDescs[2].location = 2;
    attrDescs[2].format = VK_FORMAT_R32G32_SFLOAT;
    attrDescs[2].offset = sizeof(float) * 6;
    
    VkPipelineVertexInputStateCreateInfo vertexInput = {};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &bindingDesc;
    vertexInput.vertexAttributeDescriptionCount = 3;
    vertexInput.pVertexAttributeDescriptions = attrDescs;
    
    // Input assembly
    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;
    
    // Viewport and scissor (dynamic)
    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;
    
    // Rasterization
    VkPipelineRasterizationStateCreateInfo rasterizer = {};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = wireframe ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;
    
    // Multisampling
    VkPipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    
    // Depth stencil
    VkPipelineDepthStencilStateCreateInfo depthStencil = {};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;
    
    // Color blending
    VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;
    
    VkPipelineColorBlendStateCreateInfo colorBlending = {};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    
    // Dynamic state
    VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState = {};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates = dynamicStates;
    
    // Create pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = m_pipelineLayout;
    pipelineInfo.renderPass = m_renderPass;
    pipelineInfo.subpass = 0;
    
    VkPipeline* target = wireframe ? &m_wireframePipeline : &m_pipeline;
    return vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, target) == VK_SUCCESS;
}

bool MeshRenderer::createUniformBuffer() {
    VkDeviceSize bufferSize = sizeof(MeshUBO);
    createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 m_uniformBuffer, m_uniformBufferMemory);
    return m_uniformBuffer != VK_NULL_HANDLE;
}

bool MeshRenderer::createDescriptorPool() {
    VkDescriptorPoolSize poolSizes[2] = {};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = 1;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = 1;
    
    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 2;
    poolInfo.pPoolSizes = poolSizes;
    poolInfo.maxSets = 1;
    
    return vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_descriptorPool) == VK_SUCCESS;
}

bool MeshRenderer::allocateDescriptorSet() {
    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &m_descriptorSetLayout;
    
    return vkAllocateDescriptorSets(m_device, &allocInfo, &m_descriptorSet) == VK_SUCCESS;
}

bool MeshRenderer::createDummyTexture() {
    // Create 1x1 white texture
    uint32_t whitePixel = 0xFFFFFFFF;
    
    createImage(1, 1, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                m_dummyTexture, m_dummyTextureMemory);
    
    // Upload data
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;
    createBuffer(4, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingBuffer, stagingMemory);
    
    void* data;
    vkMapMemory(m_device, stagingMemory, 0, 4, 0, &data);
    memcpy(data, &whitePixel, 4);
    vkUnmapMemory(m_device, stagingMemory);
    
    VkCommandBuffer cmd = beginSingleTimeCommands();
    
    // Transition to transfer dst
    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = m_dummyTexture;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);
    
    VkBufferImageCopy region = {};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1;
    region.imageExtent = {1, 1, 1};
    
    vkCmdCopyBufferToImage(cmd, stagingBuffer, m_dummyTexture, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    
    // Transition to shader read
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);
    
    endSingleTimeCommands(cmd);
    
    vkDestroyBuffer(m_device, stagingBuffer, nullptr);
    vkFreeMemory(m_device, stagingMemory, nullptr);
    
    // Create image view
    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_dummyTexture;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.layerCount = 1;
    
    if (vkCreateImageView(m_device, &viewInfo, nullptr, &m_dummyTextureView) != VK_SUCCESS) {
        return false;
    }
    
    // Create sampler
    VkSamplerCreateInfo samplerInfo = {};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    
    return vkCreateSampler(m_device, &samplerInfo, nullptr, &m_dummySampler) == VK_SUCCESS;
}

void MeshRenderer::updateDescriptorSet() {
    if (m_descriptorSet == VK_NULL_HANDLE) return;
    
    // Buffer info
    VkDescriptorBufferInfo bufferInfo = {};
    bufferInfo.buffer = m_uniformBuffer;
    bufferInfo.offset = 0;
    bufferInfo.range = sizeof(MeshUBO);
    
    // Image info
    VkDescriptorImageInfo imageInfo = {};
    if (m_texture) {
        imageInfo = m_texture->getDescriptorImageInfo();
    } else {
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = m_dummyTextureView;
        imageInfo.sampler = m_dummySampler;
    }
    
    VkWriteDescriptorSet writes[2] = {};
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = m_descriptorSet;
    writes[0].dstBinding = 0;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[0].descriptorCount = 1;
    writes[0].pBufferInfo = &bufferInfo;
    
    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = m_descriptorSet;
    writes[1].dstBinding = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[1].descriptorCount = 1;
    writes[1].pImageInfo = &imageInfo;
    
    vkUpdateDescriptorSets(m_device, 2, writes, 0, nullptr);
}

void MeshRenderer::updateUniformBuffer() {
    void* data;
    vkMapMemory(m_device, m_uniformBufferMemory, 0, sizeof(MeshUBO), 0, &data);
    memcpy(data, &m_ubo, sizeof(MeshUBO));
    vkUnmapMemory(m_device, m_uniformBufferMemory);
}

// ============================================================================
// Vulkan Helpers
// ============================================================================

VkCommandBuffer MeshRenderer::beginSingleTimeCommands() {
    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = m_commandPool;
    allocInfo.commandBufferCount = 1;
    
    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(m_device, &allocInfo, &commandBuffer);
    
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    
    vkBeginCommandBuffer(commandBuffer, &beginInfo);
    return commandBuffer;
}

void MeshRenderer::endSingleTimeCommands(VkCommandBuffer commandBuffer) {
    vkEndCommandBuffer(commandBuffer);
    
    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    
    vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_graphicsQueue);
    
    vkFreeCommandBuffers(m_device, m_commandPool, 1, &commandBuffer);
}

uint32_t MeshRenderer::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memProperties);
    
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && 
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    
    throw std::runtime_error("Failed to find suitable memory type");
}

void MeshRenderer::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                                VkMemoryPropertyFlags properties,
                                VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    if (vkCreateBuffer(m_device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create buffer");
    }
    
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(m_device, buffer, &memRequirements);
    
    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);
    
    if (vkAllocateMemory(m_device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate buffer memory");
    }
    
    vkBindBufferMemory(m_device, buffer, bufferMemory, 0);
}

void MeshRenderer::createImage(uint32_t width, uint32_t height, VkFormat format,
                               VkImageTiling tiling, VkImageUsageFlags usage,
                               VkMemoryPropertyFlags properties,
                               VkImage& image, VkDeviceMemory& imageMemory) {
    VkImageCreateInfo imageInfo = {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    if (vkCreateImage(m_device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create image");
    }
    
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(m_device, image, &memRequirements);
    
    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);
    
    if (vkAllocateMemory(m_device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate image memory");
    }
    
    vkBindImageMemory(m_device, image, imageMemory, 0);
}

} // namespace eden

