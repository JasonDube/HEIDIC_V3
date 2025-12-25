// ============================================================================
// FACIAL SYSTEM - Implementation
// ============================================================================

#include "facial_system.h"

#include <iostream>
#include <fstream>
#include <cstring>
#include <algorithm>
#include <array>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/glm.hpp>
#include <cmath>

// stb_image for DMap loading (already included elsewhere, just declare)
#include "../../stdlib/stb_image.h"

namespace facial {

// ============================================================================
// Helper: Find memory type
// ============================================================================

static uint32_t findMemoryType(VkPhysicalDevice physDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physDevice, &memProperties);
    
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && 
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    return UINT32_MAX;
}

// ============================================================================
// Helper: Read shader file
// ============================================================================

static std::vector<char> readShaderFile(const std::string& path) {
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[Facial] Failed to open shader: " << path << std::endl;
        return {};
    }
    
    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    return buffer;
}

// ============================================================================
// FacialAnimationClip Implementation
// ============================================================================

void FacialAnimationClip::getWeightsAtTime(float time, std::array<float, MAX_SLIDERS>& outWeights) const {
    if (keyframes.empty()) {
        outWeights.fill(0.0f);
        return;
    }
    
    if (keyframes.size() == 1) {
        outWeights = keyframes[0].weights;
        return;
    }
    
    // Handle looping
    float t = time;
    if (loop && duration > 0.0f) {
        t = fmod(time, duration);
    }
    
    // Find surrounding keyframes
    const FacialKeyframe* prev = &keyframes[0];
    const FacialKeyframe* next = &keyframes[0];
    
    for (size_t i = 0; i < keyframes.size() - 1; ++i) {
        if (keyframes[i].time <= t && keyframes[i + 1].time >= t) {
            prev = &keyframes[i];
            next = &keyframes[i + 1];
            break;
        }
        if (i == keyframes.size() - 2) {
            prev = &keyframes[i + 1];
            next = &keyframes[i + 1];
        }
    }
    
    // Interpolate
    float range = next->time - prev->time;
    float blend = (range > 0.0001f) ? (t - prev->time) / range : 0.0f;
    blend = std::clamp(blend, 0.0f, 1.0f);
    
    for (int i = 0; i < MAX_SLIDERS; ++i) {
        outWeights[i] = prev->weights[i] * (1.0f - blend) + next->weights[i] * blend;
    }
}

// ============================================================================
// Constructor / Destructor
// ============================================================================

FacialSystem::FacialSystem() {
    // Initialize sliders with default names
    for (int i = 0; i < MAX_SLIDERS; ++i) {
        m_sliders[i].name = "slider_" + std::to_string(i);
        m_sliders[i].weight = 0.0f;
        m_sliders[i].dmapIndex = -1;
    }
}

FacialSystem::~FacialSystem() {
    if (m_initialized) {
        shutdown();
    }
}

// ============================================================================
// Lifecycle
// ============================================================================

bool FacialSystem::init(vkcore::VulkanCore* core) {
    if (m_initialized) {
        std::cerr << "[Facial] Already initialized!" << std::endl;
        return false;
    }
    
    if (!core || !core->isInitialized()) {
        std::cerr << "[Facial] VulkanCore not ready!" << std::endl;
        return false;
    }
    
    m_core = core;
    
    std::cout << "[Facial] Initializing facial animation system..." << std::endl;
    
    if (!createFacialResources()) {
        std::cerr << "[Facial] Failed to create resources!" << std::endl;
        return false;
    }
    
    if (!createDMapPipeline()) {
        std::cerr << "[Facial] Failed to create DMap pipeline!" << std::endl;
        return false;
    }
    
    m_initialized = true;
    std::cout << "[Facial] Initialized successfully" << std::endl;
    
    return true;
}

void FacialSystem::shutdown() {
    if (!m_initialized || !m_core) {
        return;
    }
    
    std::cout << "[Facial] Shutting down..." << std::endl;
    
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
    
    if (m_descriptorPool != VK_NULL_HANDLE) {
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
    
    // Destroy neutral DMap texture
    if (m_neutralDMapTexture != vkcore::INVALID_TEXTURE) {
        m_core->destroyTexture(m_neutralDMapTexture);
        m_neutralDMapTexture = vkcore::INVALID_TEXTURE;
    }
    
    // Destroy DMap textures
    for (auto handle : m_dmapHandles) {
        if (handle != vkcore::INVALID_TEXTURE) {
            m_core->destroyTexture(handle);
        }
    }
    m_dmapHandles.clear();
    m_dmaps.clear();
    
    m_core = nullptr;
    m_initialized = false;
    
    std::cout << "[Facial] Shutdown complete" << std::endl;
}

// ============================================================================
// DMap Management
// ============================================================================

int FacialSystem::loadDMap(const std::string& path, const std::string& name, int sliderIndex) {
    if (!m_initialized) return -1;
    
    // Load image with stb_image
    int w, h, channels;
    stbi_uc* pixels = stbi_load(path.c_str(), &w, &h, &channels, STBI_rgb_alpha);
    if (!pixels) {
        std::cerr << "[Facial] Failed to load DMap: " << path << std::endl;
        return -1;
    }
    
    int result = loadDMapFromMemory(pixels, w, h, name, sliderIndex, 4);
    stbi_image_free(pixels);
    
    if (result >= 0) {
        std::cout << "[Facial] Loaded DMap '" << name << "' (" << w << "x" << h << ") for slider " << sliderIndex << std::endl;
    }
    
    return result;
}

// ============================================================================
// DMap Padding - Extend displacement values across UV seams
// ============================================================================
// This dilates non-neutral values into neutral grey areas to prevent
// vertex separation at UV seams. Similar to what Sims 4 does.
// ============================================================================

void FacialSystem::padDMapSeams(uint8_t* pixels, uint32_t width, uint32_t height, int channels, 
                                int paddingRadius, float maxBlendFactor, int passes) {
    if (!pixels || width == 0 || height == 0 || channels < 3) return;
    
    std::cout << "[DMap] Padding seams: radius=" << paddingRadius 
              << ", blend=" << maxBlendFactor << ", passes=" << passes << "..." << std::flush;
    
    // Neutral grey threshold (128 ± tolerance)
    const uint8_t neutralThreshold = 5;  // Consider 123-133 as "neutral"
    const uint8_t neutralValue = 128;
    
    int totalPaddedPixels = 0;
    
    // Multiple passes for better coverage
    for (int pass = 0; pass < passes; ++pass) {
        // Create a copy for reading (we'll write to original)
        size_t pixelCount = width * height;
        std::vector<uint8_t> source(pixelCount * channels);
        memcpy(source.data(), pixels, source.size());
        
        int paddedPixels = 0;
        
        // For each pixel
        for (uint32_t y = 0; y < height; ++y) {
            for (uint32_t x = 0; x < width; ++x) {
                    size_t idx = (y * width + x) * channels;
                
                // Check if this pixel is neutral (or very close to neutral)
                uint8_t r = source[idx + 0];
                uint8_t g = source[idx + 1];
                uint8_t b = source[idx + 2];
                
                bool isNeutral = (std::abs(static_cast<int>(r) - static_cast<int>(neutralValue)) < neutralThreshold &&
                                 std::abs(static_cast<int>(g) - static_cast<int>(neutralValue)) < neutralThreshold &&
                                 std::abs(static_cast<int>(b) - static_cast<int>(neutralValue)) < neutralThreshold);
                
                // If neutral, try to find nearby non-neutral pixels and blend them
                if (isNeutral) {
                    float totalR = 0.0f, totalG = 0.0f, totalB = 0.0f;
                    float totalWeight = 0.0f;
                    
                    // Sample nearby pixels in a circular pattern
                    for (int dy = -paddingRadius; dy <= paddingRadius; ++dy) {
                        for (int dx = -paddingRadius; dx <= paddingRadius; ++dx) {
                            // Skip center pixel (we're trying to fill it)
                            if (dx == 0 && dy == 0) continue;
                            
                            // Circular distance check
                            float dist = std::sqrt(static_cast<float>(dx * dx + dy * dy));
                            if (dist > paddingRadius) continue;
                            
                            int sx = static_cast<int>(x) + dx;
                            int sy = static_cast<int>(y) + dy;
                            
                            // Bounds check
                            if (sx < 0 || sx >= static_cast<int>(width) ||
                                sy < 0 || sy >= static_cast<int>(height)) {
                                continue;
                            }
                            
                            size_t srcIdx = (sy * width + sx) * channels;
                            uint8_t sr = source[srcIdx + 0];
                            uint8_t sg = source[srcIdx + 1];
                            uint8_t sb = source[srcIdx + 2];
                            
                            // Check if this neighbor is non-neutral
                            bool neighborIsNeutral = (std::abs(static_cast<int>(sr) - static_cast<int>(neutralValue)) < neutralThreshold &&
                                                      std::abs(static_cast<int>(sg) - static_cast<int>(neutralValue)) < neutralThreshold &&
                                                      std::abs(static_cast<int>(sb) - static_cast<int>(neutralValue)) < neutralThreshold);
                            
                            if (!neighborIsNeutral) {
                                // Weight by inverse distance (closer = stronger influence)
                                float weight = 1.0f / (1.0f + dist);
                                totalR += sr * weight;
                                totalG += sg * weight;
                                totalB += sb * weight;
                                totalWeight += weight;
                            }
                        }
                    }
                    
                    // If we found non-neutral neighbors, blend them in
                    if (totalWeight > 0.001f) {
                        // Blend: mix original neutral with nearby values
                        // Use a falloff so padding is subtle near seams
                        // Increase blend factor for later passes (more aggressive)
                        float passMultiplier = 1.0f + (pass * 0.2f);  // Each pass gets slightly more aggressive
                        float blendFactor = std::min(totalWeight / (paddingRadius * 2.0f) * passMultiplier, maxBlendFactor);
                        
                        pixels[idx + 0] = static_cast<uint8_t>(neutralValue * (1.0f - blendFactor) + (totalR / totalWeight) * blendFactor);
                        pixels[idx + 1] = static_cast<uint8_t>(neutralValue * (1.0f - blendFactor) + (totalG / totalWeight) * blendFactor);
                        pixels[idx + 2] = static_cast<uint8_t>(neutralValue * (1.0f - blendFactor) + (totalB / totalWeight) * blendFactor);
                        
                        paddedPixels++;
                    }
                }
            }
        }
        
        totalPaddedPixels += paddedPixels;
    }
    
    std::cout << " done (" << totalPaddedPixels << " pixels padded over " << passes << " passes)" << std::endl;
}

int FacialSystem::loadDMapFromMemory(const uint8_t* pixels, uint32_t width, uint32_t height,
                                     const std::string& name, int sliderIndex, int channels, 
                                     bool padSeams, int paddingRadius, float maxBlendFactor) {
    std::cout << "[Facial] loadDMapFromMemory called: " << name << ", size=" << width << "x" << height 
              << ", channels=" << channels << ", slider=" << sliderIndex << ", padSeams=" << padSeams 
              << ", paddingRadius=" << paddingRadius << ", maxBlendFactor=" << maxBlendFactor << std::endl;
    
    if (!m_initialized || !m_core) {
        std::cerr << "[Facial] Not initialized or core is null!" << std::endl;
        return -1;
    }
    if (sliderIndex < 0 || sliderIndex >= MAX_SLIDERS) {
        std::cerr << "[Facial] Invalid slider index: " << sliderIndex << std::endl;
        return -1;
    }
    
    // Copy pixel data (we may modify it for padding)
    size_t pixelDataSize = width * height * channels;
    std::vector<uint8_t> pixelCopy(pixelDataSize);
    memcpy(pixelCopy.data(), pixels, pixelDataSize);
    
    // Pad seams if requested (extends displacement values across UV seams to prevent cracking)
    if (padSeams) {
        padDMapSeams(pixelCopy.data(), width, height, channels, paddingRadius, maxBlendFactor, 2);  // 2 passes
    }
    
    std::cout << "[Facial] Calling createTextureLinear (for DMap - no SRGB conversion)..." << std::endl;
    // Create texture in VulkanCore with LINEAR format (no SRGB conversion)
    // CRITICAL: DMap textures must use linear format, not SRGB!
    // SRGB would convert neutral grey (128,128,128) to a different value when sampled
    vkcore::TextureHandle texHandle = m_core->createTextureLinear(pixelCopy.data(), width, height, channels);
    std::cout << "[Facial] createTextureLinear returned handle: " << texHandle << std::endl;
    if (texHandle == vkcore::INVALID_TEXTURE) {
        std::cerr << "[Facial] Failed to create DMap texture" << std::endl;
        return -1;
    }
    
    // Store DMap info
    DMapTexture dmap;
    dmap.name = name;
    dmap.width = width;
    dmap.height = height;
    dmap.sliderIndex = sliderIndex;
    dmap.valid = true;
    // Store pixel data for reference (after padding, if applied)
    dmap.pixels.resize(width * height * channels);
    memcpy(dmap.pixels.data(), pixelCopy.data(), dmap.pixels.size());
    
    int index = static_cast<int>(m_dmaps.size());
    m_dmaps.push_back(std::move(dmap));
    m_dmapHandles.push_back(texHandle);
    
    // Link slider to this DMap
    m_sliders[sliderIndex].name = name;
    m_sliders[sliderIndex].dmapIndex = index;
    
    // Update descriptor set with DMap texture (binding 1)
    // Use the first loaded DMap as the active one (can be extended to support multiple)
    if (index == 0) {
        VkDevice device = m_core->getDevice();
        
        // Get texture info from VulkanCore
        VkImageView view;
        VkSampler sampler;
        if (m_core->getTextureInfo(texHandle, view, sampler)) {
            VkDescriptorImageInfo imageInfo{};
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo.imageView = view;
            imageInfo.sampler = sampler;
            
            VkWriteDescriptorSet write{};
            write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet = m_descriptorSet;
            write.dstBinding = 1;  // DMap texture binding
            write.dstArrayElement = 0;
            write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            write.descriptorCount = 1;
            write.pImageInfo = &imageInfo;
            
            // CRITICAL: Wait for GPU to be idle before updating descriptor sets
            // This ensures the texture is fully created before we bind it
            vkDeviceWaitIdle(device);
            
            vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
            std::cout << "[Facial] DMap texture bound to descriptor set (binding 1)" << std::endl;
            std::cout << "[Facial] Texture handle: " << texHandle << ", view: " << (void*)view << ", sampler: " << (void*)sampler << std::endl;
        } else {
            std::cerr << "[Facial] Failed to get texture info for DMap!" << std::endl;
        }
    } else {
        std::cout << "[Facial] DMap loaded but not binding (index=" << index << ", only index 0 is bound)" << std::endl;
    }
    
    // CRITICAL: Update the GPU buffer to set hasDMap flag!
    updateGPUBuffer();
    std::cout << "[Facial] Updated GPU buffer, hasDMap = " << m_uboData.settings.z << std::endl;
    std::cout << "[Facial] Total DMaps loaded: " << m_dmaps.size() << std::endl;
    
    return index;
}

const DMapTexture* FacialSystem::getDMap(int index) const {
    if (index >= 0 && index < static_cast<int>(m_dmaps.size())) {
        return &m_dmaps[index];
    }
    return nullptr;
}

// ============================================================================
// Slider Control
// ============================================================================

void FacialSystem::setSliderWeight(int index, float weight) {
    if (index >= 0 && index < MAX_SLIDERS) {
        float clamped = std::clamp(weight, m_sliders[index].minWeight, m_sliders[index].maxWeight);
        m_sliders[index].weight = clamped;
        m_uboData.setWeight(index, clamped);
    }
}

float FacialSystem::getSliderWeight(int index) const {
    if (index >= 0 && index < MAX_SLIDERS) {
        return m_sliders[index].weight;
    }
    return 0.0f;
}

void FacialSystem::setSliderWeights(const float* weights, int count) {
    int limit = std::min(count, MAX_SLIDERS);
    for (int i = 0; i < limit; ++i) {
        setSliderWeight(i, weights[i]);
    }
}

void FacialSystem::resetSliders() {
    for (int i = 0; i < MAX_SLIDERS; ++i) {
        m_sliders[i].weight = 0.0f;
        m_uboData.setWeight(i, 0.0f);
    }
}

void FacialSystem::setSliderRange(int index, float minWeight, float maxWeight) {
    if (index >= 0 && index < MAX_SLIDERS) {
        m_sliders[index].minWeight = minWeight;
        m_sliders[index].maxWeight = maxWeight;
    }
}

void FacialSystem::setSliderEnabled(int index, bool enabled) {
    if (index >= 0 && index < MAX_SLIDERS) {
        m_sliders[index].enabled = enabled;
    }
}

// ============================================================================
// Expression Presets
// ============================================================================

void FacialSystem::registerPreset(const std::string& name, const ExpressionPreset& preset) {
    m_presets[name] = preset;
}

void FacialSystem::applyPreset(const std::string& name) {
    auto it = m_presets.find(name);
    if (it != m_presets.end()) {
        for (int i = 0; i < MAX_SLIDERS; ++i) {
            setSliderWeight(i, it->second.weights[i]);
        }
    }
}

void FacialSystem::blendToPreset(const std::string& name, float blendFactor) {
    auto it = m_presets.find(name);
    if (it != m_presets.end()) {
        float t = std::clamp(blendFactor, 0.0f, 1.0f);
        for (int i = 0; i < MAX_SLIDERS; ++i) {
            float current = m_sliders[i].weight;
            float target = it->second.weights[i];
            setSliderWeight(i, current * (1.0f - t) + target * t);
        }
    }
}

// ============================================================================
// Animation
// ============================================================================

int FacialSystem::loadAnimation(const FacialAnimationClip& clip) {
    int index = static_cast<int>(m_animationClips.size());
    m_animationClips.push_back(clip);
    return index;
}

void FacialSystem::playAnimation(int clipIndex, bool loop) {
    if (clipIndex >= 0 && clipIndex < static_cast<int>(m_animationClips.size())) {
        m_currentClipIndex = clipIndex;
        m_animationTime = 0.0f;
        m_animationPlaying = true;
        m_animationLoop = loop;
    }
}

void FacialSystem::stopAnimation() {
    m_animationPlaying = false;
    m_currentClipIndex = -1;
}

void FacialSystem::updateAnimation(float deltaTime) {
    if (!m_animationPlaying || m_currentClipIndex < 0) return;
    
    const FacialAnimationClip& clip = m_animationClips[m_currentClipIndex];
    
    m_animationTime += deltaTime;
    
    // Check if animation ended
    if (!m_animationLoop && m_animationTime >= clip.duration) {
        m_animationTime = clip.duration;
        m_animationPlaying = false;
    }
    
    // Get interpolated weights
    std::array<float, MAX_SLIDERS> weights;
    clip.getWeightsAtTime(m_animationTime, weights);
    
    // Apply to sliders
    for (int i = 0; i < MAX_SLIDERS; ++i) {
        setSliderWeight(i, weights[i]);
    }
}

// ============================================================================
// Rendering
// ============================================================================

void FacialSystem::setViewMatrix(const glm::mat4& view) {
    m_viewMatrix = view;
}

void FacialSystem::setProjectionMatrix(const glm::mat4& proj) {
    m_projMatrix = proj;
}

void FacialSystem::setGlobalStrength(float strength) {
    m_globalStrength = strength;
    m_uboData.settings.x = strength;
}

void FacialSystem::setBaseTexture(vkcore::TextureHandle texture) {
    if (!m_initialized || !m_core || m_descriptorSet == VK_NULL_HANDLE) return;
    
    // Use provided texture or default if invalid
    vkcore::TextureHandle texToUse = (texture != vkcore::INVALID_TEXTURE) ? texture : m_core->getDefaultTexture();
    
    VkImageView view;
    VkSampler sampler;
    if (m_core->getTextureInfo(texToUse, view, sampler)) {
        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = view;
        imageInfo.sampler = sampler;
        
        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = m_descriptorSet;
        write.dstBinding = 2;  // Base texture binding
        write.dstArrayElement = 0;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.descriptorCount = 1;
        write.pImageInfo = &imageInfo;
        
        vkUpdateDescriptorSets(m_core->getDevice(), 1, &write, 0, nullptr);
    }
}

void FacialSystem::bind() {
    if (!m_initialized || !m_core || m_pipeline == VK_NULL_HANDLE) {
        return;
    }
    
    // Update UBO with current slider weights
    updateGPUBuffer();
    
    VkCommandBuffer cmd = m_core->getCurrentCommandBuffer();
    
    // Bind pipeline
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
    
    // Bind descriptor set
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_pipelineLayout, 0, 1, &m_descriptorSet, 0, nullptr);
}

void FacialSystem::drawMesh(vkcore::MeshHandle mesh, const glm::mat4& model,
                            const glm::mat4& view, const glm::mat4& projection,
                            vkcore::TextureHandle baseTexture, const glm::vec4& color) {
    if (!m_initialized || !m_core) return;
    
    // Get mesh buffers
    VkBuffer vertexBuffer, indexBuffer;
    uint32_t indexCount;
    if (!m_core->getMeshBuffers(mesh, vertexBuffer, indexBuffer, indexCount)) {
        return;
    }
    
    // NOTE: Base texture should be bound before rendering (not during command recording)
    // The default texture is already bound during initialization
    
    VkCommandBuffer cmd = m_core->getCurrentCommandBuffer();
    
    // Push constants: model, view, projection, color
    struct PushConstants {
        glm::mat4 model;
        glm::mat4 view;
        glm::mat4 projection;
        glm::vec4 color;
    } pushConstants;
    
    pushConstants.model = model;
    pushConstants.view = view;
    pushConstants.projection = projection;
    pushConstants.color = color;
    
    vkCmdPushConstants(cmd, m_pipelineLayout, 
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(PushConstants), &pushConstants);
    
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer, offsets);
    vkCmdBindIndexBuffer(cmd, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmd, indexCount, 1, 0, 0, 0);
}

void FacialSystem::setDebugMode(bool enabled) {
    m_debugMode = enabled;
    m_uboData.settings.w = enabled ? 1.0f : 0.0f;
    updateGPUBuffer();
}

void FacialSystem::updateGPUBuffer() {
    if (!m_initialized || m_uboMemory == VK_NULL_HANDLE) return;
    
    // Update hasDMap flag (settings.z) - 1.0 if any valid DMap is loaded, 0.0 otherwise
    bool hasValidDMap = false;
    for (const auto& dmap : m_dmaps) {
        if (dmap.valid) {
            hasValidDMap = true;
            break;
        }
    }
    m_uboData.settings.z = hasValidDMap ? 1.0f : 0.0f;
    
    // Update debug mode (settings.w)
    m_uboData.settings.w = m_debugMode ? 1.0f : 0.0f;
    
    void* data;
    vkMapMemory(m_core->getDevice(), m_uboMemory, 0, sizeof(FacialUBO), 0, &data);
    memcpy(data, &m_uboData, sizeof(FacialUBO));
    vkUnmapMemory(m_core->getDevice(), m_uboMemory);
}

// ============================================================================
// Resource Creation
// ============================================================================

bool FacialSystem::createFacialResources() {
    VkDevice device = m_core->getDevice();
    VkPhysicalDevice physDevice = m_core->getPhysicalDevice();
    
    // Create UBO buffer
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = sizeof(FacialUBO);
    bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    if (vkCreateBuffer(device, &bufferInfo, nullptr, &m_uboBuffer) != VK_SUCCESS) {
        std::cerr << "[Facial] Failed to create UBO buffer!" << std::endl;
        return false;
    }
    
    VkMemoryRequirements memReq;
    vkGetBufferMemoryRequirements(device, m_uboBuffer, &memReq);
    
    uint32_t memTypeIndex = findMemoryType(physDevice, memReq.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    
    if (memTypeIndex == UINT32_MAX) {
        std::cerr << "[Facial] Failed to find suitable memory type!" << std::endl;
        return false;
    }
    
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = memTypeIndex;
    
    if (vkAllocateMemory(device, &allocInfo, nullptr, &m_uboMemory) != VK_SUCCESS) {
        std::cerr << "[Facial] Failed to allocate UBO memory!" << std::endl;
        return false;
    }
    
    vkBindBufferMemory(device, m_uboBuffer, m_uboMemory, 0);
    
    // Create descriptor set layout
    std::vector<VkDescriptorSetLayoutBinding> bindings(3);
    
    // Binding 0: Facial UBO (slider weights) - used in both vertex and fragment shaders
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;  // Both stages!
    bindings[0].pImmutableSamplers = nullptr;
    
    // Binding 1: DMap texture sampler (vertex shader)
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;  // Sampled in vertex shader!
    bindings[1].pImmutableSamplers = nullptr;
    
    // Binding 2: Base color texture sampler (fragment shader)
    bindings[2].binding = 2;
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[2].descriptorCount = 1;
    bindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[2].pImmutableSamplers = nullptr;
    
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();
    
    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &m_descriptorSetLayout) != VK_SUCCESS) {
        std::cerr << "[Facial] Failed to create descriptor set layout!" << std::endl;
        return false;
    }
    
    // Create descriptor pool
    std::vector<VkDescriptorPoolSize> poolSizes = {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_DMAP_TEXTURES + 1}  // +1 for base texture
    };
    
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = MAX_DMAP_TEXTURES;
    
    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS) {
        std::cerr << "[Facial] Failed to create descriptor pool!" << std::endl;
        return false;
    }
    
    // Allocate descriptor set
    VkDescriptorSetAllocateInfo descAllocInfo{};
    descAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descAllocInfo.descriptorPool = m_descriptorPool;
    descAllocInfo.descriptorSetCount = 1;
    descAllocInfo.pSetLayouts = &m_descriptorSetLayout;
    
    if (vkAllocateDescriptorSets(device, &descAllocInfo, &m_descriptorSet) != VK_SUCCESS) {
        std::cerr << "[Facial] Failed to allocate descriptor set!" << std::endl;
        return false;
    }
    
    // Update descriptor set with UBO
    VkDescriptorBufferInfo uboInfo{};
    uboInfo.buffer = m_uboBuffer;
    uboInfo.offset = 0;
    uboInfo.range = sizeof(FacialUBO);
    
    VkWriteDescriptorSet uboWrite{};
    uboWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    uboWrite.dstSet = m_descriptorSet;
    uboWrite.dstBinding = 0;
    uboWrite.dstArrayElement = 0;
    uboWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboWrite.descriptorCount = 1;
    uboWrite.pBufferInfo = &uboInfo;
    
    vkUpdateDescriptorSets(device, 1, &uboWrite, 0, nullptr);
    
    // Create neutral grey DMap texture (128, 128, 128 = no displacement)
    // IMPORTANT: The default white texture (255,255,255) encodes +1,+1,+1 displacement!
    // We need neutral grey (128,128,128) which encodes (0,0,0) displacement.
    // CRITICAL: Use LINEAR format (no SRGB) so values are sampled exactly as written
    uint8_t neutralGrey[4] = {128, 128, 128, 255};  // RGB = 128 = neutral, A = 255 = opaque
    m_neutralDMapTexture = m_core->createTextureLinear(neutralGrey, 1, 1, 4);
    if (m_neutralDMapTexture == vkcore::INVALID_TEXTURE) {
        std::cerr << "[Facial] Failed to create neutral DMap texture!" << std::endl;
        return false;
    }
    std::cout << "[Facial] Created neutral grey DMap texture (128,128,128) for default binding" << std::endl;
    
    // Get neutral DMap texture info
    VkImageView neutralView;
    VkSampler neutralSampler;
    if (!m_core->getTextureInfo(m_neutralDMapTexture, neutralView, neutralSampler)) {
        std::cerr << "[Facial] Failed to get neutral DMap texture info!" << std::endl;
        return false;
    }
    
    // Bind default textures
    VkImageView defaultView;
    VkSampler defaultSampler;
    if (m_core->getTextureInfo(m_core->getDefaultTexture(), defaultView, defaultSampler)) {
        // Binding 1: Neutral grey DMap (128,128,128 = no displacement)
        VkDescriptorImageInfo neutralImageInfo{};
        neutralImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        neutralImageInfo.imageView = neutralView;
        neutralImageInfo.sampler = neutralSampler;
        
        VkWriteDescriptorSet dmapWrite{};
        dmapWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        dmapWrite.dstSet = m_descriptorSet;
        dmapWrite.dstBinding = 1;
        dmapWrite.dstArrayElement = 0;
        dmapWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        dmapWrite.descriptorCount = 1;
        dmapWrite.pImageInfo = &neutralImageInfo;
        
        // Binding 2: Default base texture (white)
        VkDescriptorImageInfo defaultImageInfo{};
        defaultImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        defaultImageInfo.imageView = defaultView;
        defaultImageInfo.sampler = defaultSampler;
        
        VkWriteDescriptorSet baseWrite{};
        baseWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        baseWrite.dstSet = m_descriptorSet;
        baseWrite.dstBinding = 2;
        baseWrite.dstArrayElement = 0;
        baseWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        baseWrite.descriptorCount = 1;
        baseWrite.pImageInfo = &defaultImageInfo;
        
        std::array<VkWriteDescriptorSet, 2> writes = {dmapWrite, baseWrite};
        vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }
    
    return true;
}

bool FacialSystem::createDMapPipeline() {
    VkDevice device = m_core->getDevice();
    
    // Load shaders
    auto vertCode = readShaderFile("shaders/dmap_mesh.vert.spv");
    auto fragCode = readShaderFile("shaders/dmap_mesh.frag.spv");
    
    if (vertCode.empty() || fragCode.empty()) {
        std::cerr << "[Facial] Failed to load shaders!" << std::endl;
        return false;
    }
    
    // Create shader modules
    VkShaderModuleCreateInfo vertModuleInfo{};
    vertModuleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    vertModuleInfo.codeSize = vertCode.size();
    vertModuleInfo.pCode = reinterpret_cast<const uint32_t*>(vertCode.data());
    
    VkShaderModule vertModule;
    if (vkCreateShaderModule(device, &vertModuleInfo, nullptr, &vertModule) != VK_SUCCESS) {
        std::cerr << "[Facial] Failed to create vertex shader module!" << std::endl;
        return false;
    }
    
    VkShaderModuleCreateInfo fragModuleInfo{};
    fragModuleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    fragModuleInfo.codeSize = fragCode.size();
    fragModuleInfo.pCode = reinterpret_cast<const uint32_t*>(fragCode.data());
    
    VkShaderModule fragModule;
    if (vkCreateShaderModule(device, &fragModuleInfo, nullptr, &fragModule) != VK_SUCCESS) {
        vkDestroyShaderModule(device, vertModule, nullptr);
        std::cerr << "[Facial] Failed to create fragment shader module!" << std::endl;
        return false;
    }
    
    // Shader stages
    VkPipelineShaderStageCreateInfo shaderStages[2] = {};
    shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaderStages[0].module = vertModule;
    shaderStages[0].pName = "main";
    
    shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderStages[1].module = fragModule;
    shaderStages[1].pName = "main";
    
    // Vertex input - POSITION_NORMAL_UV0_UV1 format
    VkVertexInputBindingDescription bindingDesc{};
    bindingDesc.binding = 0;
    bindingDesc.stride = sizeof(float) * 10;  // vec3 + vec3 + vec2 + vec2
    bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    
    std::vector<VkVertexInputAttributeDescription> attrDescs(4);
    attrDescs[0] = {0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0};                    // position
    attrDescs[1] = {1, 0, VK_FORMAT_R32G32B32_SFLOAT, sizeof(float) * 3};    // normal
    attrDescs[2] = {2, 0, VK_FORMAT_R32G32_SFLOAT, sizeof(float) * 6};       // uv0
    attrDescs[3] = {3, 0, VK_FORMAT_R32G32_SFLOAT, sizeof(float) * 8};       // uv1
    
    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &bindingDesc;
    vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(attrDescs.size());
    vertexInput.pVertexAttributeDescriptions = attrDescs.data();
    
    // Input assembly
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    
    // Viewport state (dynamic)
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;
    
    // Rasterizer
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    
    // Multisampling
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    
    // Depth stencil
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    
    // Color blending
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;
    
    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
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
    
    // Push constant range: model (mat4), view (mat4), projection (mat4), color (vec4)
    // Total: 3 * 16 floats + 4 floats = 52 floats = 208 bytes
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(glm::mat4) * 3 + sizeof(glm::vec4);  // model + view + proj + color
    
    // Pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &m_descriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
    
    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS) {
        vkDestroyShaderModule(device, vertModule, nullptr);
        vkDestroyShaderModule(device, fragModule, nullptr);
        std::cerr << "[Facial] Failed to create pipeline layout!" << std::endl;
        return false;
    }
    
    // Create graphics pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo{};
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
    pipelineInfo.renderPass = m_core->getRenderPass();
    pipelineInfo.subpass = 0;
    
    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline) != VK_SUCCESS) {
        vkDestroyShaderModule(device, vertModule, nullptr);
        vkDestroyShaderModule(device, fragModule, nullptr);
        std::cerr << "[Facial] Failed to create graphics pipeline!" << std::endl;
        return false;
    }
    
    // Cleanup shader modules
    vkDestroyShaderModule(device, vertModule, nullptr);
    vkDestroyShaderModule(device, fragModule, nullptr);
    
    std::cout << "[Facial] Created DMap pipeline" << std::endl;
    return true;
}

} // namespace facial

// ============================================================================
// C API Implementation
// ============================================================================

static facial::FacialSystem* g_facialSystem = nullptr;

extern "C" {

int facial_init(void* vulkanCore) {
    if (g_facialSystem) return 0;
    g_facialSystem = new facial::FacialSystem();
    return g_facialSystem->init(static_cast<vkcore::VulkanCore*>(vulkanCore)) ? 1 : 0;
}

void facial_shutdown() {
    if (g_facialSystem) {
        g_facialSystem->shutdown();
        delete g_facialSystem;
        g_facialSystem = nullptr;
    }
}

int facial_is_initialized() {
    return (g_facialSystem && g_facialSystem->isInitialized()) ? 1 : 0;
}

int facial_load_dmap(const char* path, const char* name, int sliderIndex) {
    if (!g_facialSystem) return -1;
    return g_facialSystem->loadDMap(path, name, sliderIndex);
}

void facial_set_slider(int index, float weight) {
    if (g_facialSystem) g_facialSystem->setSliderWeight(index, weight);
}

float facial_get_slider(int index) {
    return g_facialSystem ? g_facialSystem->getSliderWeight(index) : 0.0f;
}

void facial_reset_sliders() {
    if (g_facialSystem) g_facialSystem->resetSliders();
}

void facial_apply_preset(const char* name) {
    if (g_facialSystem) g_facialSystem->applyPreset(name);
}

void facial_play_animation(int clipIndex, int loop) {
    if (g_facialSystem) g_facialSystem->playAnimation(clipIndex, loop != 0);
}

void facial_stop_animation() {
    if (g_facialSystem) g_facialSystem->stopAnimation();
}

void facial_update(float deltaTime) {
    if (g_facialSystem) g_facialSystem->updateAnimation(deltaTime);
}

void facial_bind() {
    if (g_facialSystem) g_facialSystem->bind();
}

void facial_set_view_matrix(const float* mat4) {
    if (g_facialSystem && mat4) {
        glm::mat4 view;
        memcpy(&view, mat4, sizeof(glm::mat4));
        g_facialSystem->setViewMatrix(view);
    }
}

void facial_set_projection_matrix(const float* mat4) {
    if (g_facialSystem && mat4) {
        glm::mat4 proj;
        memcpy(&proj, mat4, sizeof(glm::mat4));
        g_facialSystem->setProjectionMatrix(proj);
    }
}

void facial_set_global_strength(float strength) {
    if (g_facialSystem) g_facialSystem->setGlobalStrength(strength);
}

void facial_draw_mesh(unsigned int meshHandle,
                      float px, float py, float pz,
                      float rx, float ry, float rz,
                      float sx, float sy, float sz,
                      unsigned int baseTexture,
                      float r, float g, float b, float a) {
    if (!g_facialSystem) return;
    
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(px, py, pz));
    model = glm::rotate(model, glm::radians(rx), glm::vec3(1, 0, 0));
    model = glm::rotate(model, glm::radians(ry), glm::vec3(0, 1, 0));
    model = glm::rotate(model, glm::radians(rz), glm::vec3(0, 0, 1));
    model = glm::scale(model, glm::vec3(sx, sy, sz));
    
    // Use identity matrices for view/projection (C API doesn't have camera access)
    // In practice, the C++ API should be used which passes proper camera matrices
    glm::mat4 view = glm::mat4(1.0f);
    glm::mat4 projection = glm::mat4(1.0f);
    
    g_facialSystem->drawMesh(static_cast<vkcore::MeshHandle>(meshHandle), model,
                             view, projection,
                             static_cast<vkcore::TextureHandle>(baseTexture),
                             glm::vec4(r, g, b, a));
}

} // extern "C"

