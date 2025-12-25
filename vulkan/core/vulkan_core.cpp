// ============================================================================
// VULKAN CORE - Implementation
// ============================================================================
// All Vulkan boilerplate in ONE place. ~600 lines instead of 800+ per project.
// ============================================================================

#include "vulkan_core.h"

#include <iostream>
#include <fstream>
#include <set>
#include <algorithm>
#include <cstring>
#include <array>

// stb_image for texture loading
#define STB_IMAGE_IMPLEMENTATION
#include "../../stdlib/stb_image.h"

// ImGui includes (must be before namespace)
#ifdef VKCORE_ENABLE_IMGUI
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#endif

namespace vkcore {

// ============================================================================
// Cube Vertex Data (Position + Color)
// ============================================================================

static const float CUBE_VERTICES[] = {
    // Position          // Color
    // Front face (red)
    -0.5f, -0.5f,  0.5f,  1.0f, 0.3f, 0.3f,
     0.5f, -0.5f,  0.5f,  1.0f, 0.3f, 0.3f,
     0.5f,  0.5f,  0.5f,  1.0f, 0.3f, 0.3f,
    -0.5f,  0.5f,  0.5f,  1.0f, 0.3f, 0.3f,
    // Back face (green)
    -0.5f, -0.5f, -0.5f,  0.3f, 1.0f, 0.3f,
     0.5f, -0.5f, -0.5f,  0.3f, 1.0f, 0.3f,
     0.5f,  0.5f, -0.5f,  0.3f, 1.0f, 0.3f,
    -0.5f,  0.5f, -0.5f,  0.3f, 1.0f, 0.3f,
    // Top face (blue)
    -0.5f,  0.5f, -0.5f,  0.3f, 0.3f, 1.0f,
     0.5f,  0.5f, -0.5f,  0.3f, 0.3f, 1.0f,
     0.5f,  0.5f,  0.5f,  0.3f, 0.3f, 1.0f,
    -0.5f,  0.5f,  0.5f,  0.3f, 0.3f, 1.0f,
    // Bottom face (yellow)
    -0.5f, -0.5f, -0.5f,  1.0f, 1.0f, 0.3f,
     0.5f, -0.5f, -0.5f,  1.0f, 1.0f, 0.3f,
     0.5f, -0.5f,  0.5f,  1.0f, 1.0f, 0.3f,
    -0.5f, -0.5f,  0.5f,  1.0f, 1.0f, 0.3f,
    // Right face (magenta)
     0.5f, -0.5f, -0.5f,  1.0f, 0.3f, 1.0f,
     0.5f,  0.5f, -0.5f,  1.0f, 0.3f, 1.0f,
     0.5f,  0.5f,  0.5f,  1.0f, 0.3f, 1.0f,
     0.5f, -0.5f,  0.5f,  1.0f, 0.3f, 1.0f,
    // Left face (cyan)
    -0.5f, -0.5f, -0.5f,  0.3f, 1.0f, 1.0f,
    -0.5f,  0.5f, -0.5f,  0.3f, 1.0f, 1.0f,
    -0.5f,  0.5f,  0.5f,  0.3f, 1.0f, 1.0f,
    -0.5f, -0.5f,  0.5f,  0.3f, 1.0f, 1.0f,
};

static const uint32_t CUBE_INDICES[] = {
    0, 1, 2, 2, 3, 0,       // Front
    4, 6, 5, 6, 4, 7,       // Back
    8, 9, 10, 10, 11, 8,    // Top
    12, 14, 13, 14, 12, 15, // Bottom
    16, 17, 18, 18, 19, 16, // Right
    20, 22, 21, 22, 20, 23  // Left
};

// ============================================================================
// Constructor / Destructor
// ============================================================================

VulkanCore::VulkanCore() {
    m_viewMatrix = glm::lookAt(glm::vec3(0, 2, 5), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
    m_projMatrix = glm::perspective(glm::radians(60.0f), 16.0f/9.0f, 0.1f, 1000.0f);
    m_projMatrix[1][1] *= -1;  // Vulkan Y flip
}

VulkanCore::~VulkanCore() {
    shutdown();
}

// ============================================================================
// Initialization
// ============================================================================

bool VulkanCore::init(GLFWwindow* window, const CoreConfig& config) {
    if (m_initialized) return true;
    
    m_window = window;
    m_config = config;
    
    std::cout << "[VulkanCore] Initializing..." << std::endl;
    
    if (!createInstance()) { std::cerr << "[VulkanCore] FAILED: createInstance" << std::endl; return false; }
    if (!createSurface(window)) { std::cerr << "[VulkanCore] FAILED: createSurface" << std::endl; return false; }
    if (!selectPhysicalDevice()) { std::cerr << "[VulkanCore] FAILED: selectPhysicalDevice" << std::endl; return false; }
    if (!createLogicalDevice()) { std::cerr << "[VulkanCore] FAILED: createLogicalDevice" << std::endl; return false; }
    if (!createSwapchain()) { std::cerr << "[VulkanCore] FAILED: createSwapchain" << std::endl; return false; }
    if (!createDepthResources()) { std::cerr << "[VulkanCore] FAILED: createDepthResources" << std::endl; return false; }
    if (!createRenderPass()) { std::cerr << "[VulkanCore] FAILED: createRenderPass" << std::endl; return false; }
    if (!createDescriptorSetLayout()) { std::cerr << "[VulkanCore] FAILED: createDescriptorSetLayout" << std::endl; return false; }
    if (!createDescriptorPool()) { std::cerr << "[VulkanCore] FAILED: createDescriptorPool" << std::endl; return false; }
    if (!createFramebuffers()) { std::cerr << "[VulkanCore] FAILED: createFramebuffers" << std::endl; return false; }
    if (!createCommandPool()) { std::cerr << "[VulkanCore] FAILED: createCommandPool" << std::endl; return false; }
    if (!createCommandBuffers()) { std::cerr << "[VulkanCore] FAILED: createCommandBuffers" << std::endl; return false; }
    if (!createSyncObjects()) { std::cerr << "[VulkanCore] FAILED: createSyncObjects" << std::endl; return false; }
    if (!createDefaultResources()) { std::cerr << "[VulkanCore] FAILED: createDefaultResources" << std::endl; return false; }
    
    m_initialized = true;
    std::cout << "[VulkanCore] Ready! (" << m_swapchainExtent.width << "x" << m_swapchainExtent.height << ")" << std::endl;
    return true;
}

void VulkanCore::shutdown() {
    if (!m_initialized) return;
    
    vkDeviceWaitIdle(m_device);
    
    // Shutdown ImGui if initialized
    shutdownImGui();
    
    // Destroy resources
    for (auto& mesh : m_meshes) {
        if (mesh.valid) {
            destroyBuffer(mesh.vertexBuffer);
            destroyBuffer(mesh.indexBuffer);
        }
    }
    m_meshes.clear();
    
    for (auto& buf : m_buffers) {
        if (buf.valid) {
            vkDestroyBuffer(m_device, buf.buffer, nullptr);
            vkFreeMemory(m_device, buf.memory, nullptr);
        }
    }
    m_buffers.clear();
    
    for (auto& pipe : m_pipelines) {
        if (pipe.valid) {
            vkDestroyPipeline(m_device, pipe.pipeline, nullptr);
            vkDestroyPipelineLayout(m_device, pipe.layout, nullptr);
        }
    }
    m_pipelines.clear();
    
    // Destroy Vulkan objects
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(m_device, m_imageAvailableSemaphores[i], nullptr);
        vkDestroySemaphore(m_device, m_renderFinishedSemaphores[i], nullptr);
        vkDestroyFence(m_device, m_inFlightFences[i], nullptr);
    }
    
    vkDestroyCommandPool(m_device, m_commandPool, nullptr);
    
    cleanupSwapchain();
    
    vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr);
    vkDestroyRenderPass(m_device, m_renderPass, nullptr);
    vkDestroyDevice(m_device, nullptr);
    vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
    vkDestroyInstance(m_instance, nullptr);
    
    m_initialized = false;
    std::cout << "[VulkanCore] Shutdown complete" << std::endl;
}

// ============================================================================
// Instance Creation
// ============================================================================

bool VulkanCore::createInstance() {
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = m_config.appName.c_str();
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "VulkanCore";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;
    
    uint32_t glfwExtCount = 0;
    const char** glfwExts = glfwGetRequiredInstanceExtensions(&glfwExtCount);
    
    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = glfwExtCount;
    createInfo.ppEnabledExtensionNames = glfwExts;
    
    if (vkCreateInstance(&createInfo, nullptr, &m_instance) != VK_SUCCESS) {
        return false;
    }
    return true;
}

bool VulkanCore::createSurface(GLFWwindow* window) {
    return glfwCreateWindowSurface(m_instance, window, nullptr, &m_surface) == VK_SUCCESS;
}

// ============================================================================
// Physical Device Selection
// ============================================================================

bool VulkanCore::selectPhysicalDevice() {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);
    if (deviceCount == 0) return false;
    
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());
    
    for (const auto& device : devices) {
        // Find queue families
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());
        
        bool hasGraphics = false, hasPresent = false;
        for (uint32_t i = 0; i < queueFamilyCount; i++) {
            if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                m_graphicsFamily = i;
                hasGraphics = true;
            }
            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface, &presentSupport);
            if (presentSupport) {
                m_presentFamily = i;
                hasPresent = true;
            }
        }
        
        if (hasGraphics && hasPresent) {
            m_physicalDevice = device;
            VkPhysicalDeviceProperties props;
            vkGetPhysicalDeviceProperties(device, &props);
            std::cout << "[VulkanCore] GPU: " << props.deviceName << std::endl;
            return true;
        }
    }
    return false;
}

// ============================================================================
// Logical Device
// ============================================================================

bool VulkanCore::createLogicalDevice() {
    std::set<uint32_t> uniqueFamilies = {m_graphicsFamily, m_presentFamily};
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    float queuePriority = 1.0f;
    
    for (uint32_t family : uniqueFamilies) {
        VkDeviceQueueCreateInfo queueInfo{};
        queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfo.queueFamilyIndex = family;
        queueInfo.queueCount = 1;
        queueInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueInfo);
    }
    
    const char* deviceExtensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    
    VkPhysicalDeviceFeatures deviceFeatures{};
    deviceFeatures.fillModeNonSolid = VK_TRUE;  // For wireframe
    
    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.enabledExtensionCount = 1;
    createInfo.ppEnabledExtensionNames = deviceExtensions;
    createInfo.pEnabledFeatures = &deviceFeatures;
    
    if (vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device) != VK_SUCCESS) {
        return false;
    }
    
    vkGetDeviceQueue(m_device, m_graphicsFamily, 0, &m_graphicsQueue);
    vkGetDeviceQueue(m_device, m_presentFamily, 0, &m_presentQueue);
    return true;
}

// ============================================================================
// Swapchain
// ============================================================================

bool VulkanCore::createSwapchain() {
    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physicalDevice, m_surface, &caps);
    
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &formatCount, formats.data());
    
    // Choose format
    m_swapchainFormat = formats[0].format;
    VkColorSpaceKHR colorSpace = formats[0].colorSpace;
    for (const auto& f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            m_swapchainFormat = f.format;
            colorSpace = f.colorSpace;
            break;
        }
    }
    
    // Choose extent
    if (caps.currentExtent.width != UINT32_MAX) {
        m_swapchainExtent = caps.currentExtent;
    } else {
        int w, h;
        glfwGetFramebufferSize(m_window, &w, &h);
        m_swapchainExtent.width = std::clamp((uint32_t)w, caps.minImageExtent.width, caps.maxImageExtent.width);
        m_swapchainExtent.height = std::clamp((uint32_t)h, caps.minImageExtent.height, caps.maxImageExtent.height);
    }
    
    uint32_t imageCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount) {
        imageCount = caps.maxImageCount;
    }
    
    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = m_surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = m_swapchainFormat;
    createInfo.imageColorSpace = colorSpace;
    createInfo.imageExtent = m_swapchainExtent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    
    uint32_t queueFamilyIndices[] = {m_graphicsFamily, m_presentFamily};
    if (m_graphicsFamily != m_presentFamily) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }
    
    createInfo.preTransform = caps.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = m_config.vsync ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_IMMEDIATE_KHR;
    createInfo.clipped = VK_TRUE;
    
    if (vkCreateSwapchainKHR(m_device, &createInfo, nullptr, &m_swapchain) != VK_SUCCESS) {
        return false;
    }
    
    vkGetSwapchainImagesKHR(m_device, m_swapchain, &imageCount, nullptr);
    m_swapchainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(m_device, m_swapchain, &imageCount, m_swapchainImages.data());
    
    // Create image views
    m_swapchainImageViews.resize(imageCount);
    for (size_t i = 0; i < imageCount; i++) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = m_swapchainImages[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = m_swapchainFormat;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;
        vkCreateImageView(m_device, &viewInfo, nullptr, &m_swapchainImageViews[i]);
    }
    
    return true;
}

// ============================================================================
// Depth Resources
// ============================================================================

bool VulkanCore::createDepthResources() {
    VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;
    
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = m_swapchainExtent.width;
    imageInfo.extent.height = m_swapchainExtent.height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = depthFormat;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    
    vkCreateImage(m_device, &imageInfo, nullptr, &m_depthImage);
    
    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(m_device, m_depthImage, &memReqs);
    
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    
    vkAllocateMemory(m_device, &allocInfo, nullptr, &m_depthImageMemory);
    vkBindImageMemory(m_device, m_depthImage, m_depthImageMemory, 0);
    
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_depthImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = depthFormat;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    
    vkCreateImageView(m_device, &viewInfo, nullptr, &m_depthImageView);
    return true;
}

// ============================================================================
// Render Pass
// ============================================================================

bool VulkanCore::createRenderPass() {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = m_swapchainFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    
    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = VK_FORMAT_D32_SFLOAT;
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    
    VkAttachmentReference colorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference depthRef{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
    
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;
    subpass.pDepthStencilAttachment = &depthRef;
    
    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    
    std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};
    
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;
    
    return vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &m_renderPass) == VK_SUCCESS;
}

// ============================================================================
// Descriptor Set Layout
// ============================================================================

bool VulkanCore::createDescriptorSetLayout() {
    std::array<VkDescriptorSetLayoutBinding, 2> bindings{};
    
    // Binding 0: UBO
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    
    // Binding 1: Texture sampler
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();
    
    return vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, &m_descriptorSetLayout) == VK_SUCCESS;
}

// ============================================================================
// Descriptor Pool
// ============================================================================

bool VulkanCore::createDescriptorPool() {
    std::array<VkDescriptorPoolSize, 2> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = 100;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = 100;
    
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = 200;
    
    return vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_descriptorPool) == VK_SUCCESS;
}

// ============================================================================
// Framebuffers
// ============================================================================

bool VulkanCore::createFramebuffers() {
    m_framebuffers.resize(m_swapchainImageViews.size());
    
    for (size_t i = 0; i < m_swapchainImageViews.size(); i++) {
        std::array<VkImageView, 2> attachments = {m_swapchainImageViews[i], m_depthImageView};
        
        VkFramebufferCreateInfo fbInfo{};
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass = m_renderPass;
        fbInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        fbInfo.pAttachments = attachments.data();
        fbInfo.width = m_swapchainExtent.width;
        fbInfo.height = m_swapchainExtent.height;
        fbInfo.layers = 1;
        
        if (vkCreateFramebuffer(m_device, &fbInfo, nullptr, &m_framebuffers[i]) != VK_SUCCESS) {
            return false;
        }
    }
    return true;
}

// ============================================================================
// Command Pool & Buffers
// ============================================================================

bool VulkanCore::createCommandPool() {
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = m_graphicsFamily;
    
    return vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_commandPool) == VK_SUCCESS;
}

bool VulkanCore::createCommandBuffers() {
    m_commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = m_commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(m_commandBuffers.size());
    
    return vkAllocateCommandBuffers(m_device, &allocInfo, m_commandBuffers.data()) == VK_SUCCESS;
}

// ============================================================================
// Sync Objects
// ============================================================================

bool VulkanCore::createSyncObjects() {
    m_imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    m_renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    m_inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
    
    VkSemaphoreCreateInfo semInfo{};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(m_device, &semInfo, nullptr, &m_imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(m_device, &semInfo, nullptr, &m_renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(m_device, &fenceInfo, nullptr, &m_inFlightFences[i]) != VK_SUCCESS) {
            return false;
        }
    }
    return true;
}

// ============================================================================
// Default Resources (UBO, Default Pipeline)
// ============================================================================

bool VulkanCore::createDefaultResources() {
    // Create object UBO
    m_objectUBO = createUniformBuffer(sizeof(StandardUBO));
    if (m_objectUBO == INVALID_BUFFER) return false;
    
    // Create default 1x1 white texture
    uint8_t whitePixel[4] = {255, 255, 255, 255};
    m_defaultTexture = createTexture(whitePixel, 1, 1, 4);
    if (m_defaultTexture == INVALID_TEXTURE) {
        std::cerr << "[VulkanCore] Failed to create default texture" << std::endl;
        return false;
    }
    m_currentTexture = m_defaultTexture;
    
    // Allocate descriptor set
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &m_descriptorSetLayout;
    
    if (vkAllocateDescriptorSets(m_device, &allocInfo, &m_objectDescriptorSet) != VK_SUCCESS) {
        return false;
    }
    
    // Update descriptor set with UBO and default texture
    VkDescriptorBufferInfo bufInfo{};
    bufInfo.buffer = m_buffers[m_objectUBO].buffer;
    bufInfo.offset = 0;
    bufInfo.range = sizeof(StandardUBO);
    
    VkDescriptorImageInfo imageInfo{};
    imageInfo.sampler = m_textures[m_defaultTexture].sampler;
    imageInfo.imageView = m_textures[m_defaultTexture].view;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    
    std::array<VkWriteDescriptorSet, 2> writes{};
    
    // UBO write
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = m_objectDescriptorSet;
    writes[0].dstBinding = 0;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[0].descriptorCount = 1;
    writes[0].pBufferInfo = &bufInfo;
    
    // Texture write
    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = m_objectDescriptorSet;
    writes[1].dstBinding = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[1].descriptorCount = 1;
    writes[1].pImageInfo = &imageInfo;
    
    vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    
    return true;
}

// ============================================================================
// Swapchain Cleanup/Recreation
// ============================================================================

void VulkanCore::cleanupSwapchain() {
    vkDestroyImageView(m_device, m_depthImageView, nullptr);
    vkDestroyImage(m_device, m_depthImage, nullptr);
    vkFreeMemory(m_device, m_depthImageMemory, nullptr);
    
    for (auto fb : m_framebuffers) vkDestroyFramebuffer(m_device, fb, nullptr);
    for (auto iv : m_swapchainImageViews) vkDestroyImageView(m_device, iv, nullptr);
    vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
}

void VulkanCore::recreateSwapchain() {
    int w = 0, h = 0;
    glfwGetFramebufferSize(m_window, &w, &h);
    while (w == 0 || h == 0) {
        glfwGetFramebufferSize(m_window, &w, &h);
        glfwWaitEvents();
    }
    vkDeviceWaitIdle(m_device);
    cleanupSwapchain();
    createSwapchain();
    createDepthResources();
    createFramebuffers();
}

// ============================================================================
// Frame Management
// ============================================================================

bool VulkanCore::beginFrame() {
    m_frameStarted = false;
    
    vkWaitForFences(m_device, 1, &m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX);
    
    VkResult result = vkAcquireNextImageKHR(m_device, m_swapchain, UINT64_MAX,
                                             m_imageAvailableSemaphores[m_currentFrame],
                                             VK_NULL_HANDLE, &m_imageIndex);
    
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_framebufferResized) {
        m_framebufferResized = false;
        recreateSwapchain();
        return false;
    }
    
    vkResetFences(m_device, 1, &m_inFlightFences[m_currentFrame]);
    
    VkCommandBuffer cmd = m_commandBuffers[m_currentFrame];
    vkResetCommandBuffer(cmd, 0);
    
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(cmd, &beginInfo);
    
    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = {{m_config.clearColor[0], m_config.clearColor[1], m_config.clearColor[2], m_config.clearColor[3]}};
    clearValues[1].depthStencil = {1.0f, 0};
    
    VkRenderPassBeginInfo rpInfo{};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpInfo.renderPass = m_renderPass;
    rpInfo.framebuffer = m_framebuffers[m_imageIndex];
    rpInfo.renderArea.extent = m_swapchainExtent;
    rpInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    rpInfo.pClearValues = clearValues.data();
    
    vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);
    
    // Set viewport and scissor
    VkViewport viewport{0, 0, (float)m_swapchainExtent.width, (float)m_swapchainExtent.height, 0, 1};
    VkRect2D scissor{{0, 0}, m_swapchainExtent};
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &scissor);
    
    m_frameStarted = true;
    return true;
}

void VulkanCore::endFrame() {
    if (!m_frameStarted) return;
    
    VkCommandBuffer cmd = m_commandBuffers[m_currentFrame];
    
    vkCmdEndRenderPass(cmd);
    vkEndCommandBuffer(cmd);
    
    VkSemaphore waitSems[] = {m_imageAvailableSemaphores[m_currentFrame]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    VkSemaphore signalSems[] = {m_renderFinishedSemaphores[m_currentFrame]};
    
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSems;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSems;
    
    vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, m_inFlightFences[m_currentFrame]);
    
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSems;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &m_swapchain;
    presentInfo.pImageIndices = &m_imageIndex;
    
    VkResult result = vkQueuePresentKHR(m_presentQueue, &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        m_framebufferResized = true;
    }
    
    m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    m_frameStarted = false;
}

// ============================================================================
// Pipeline Creation
// ============================================================================

PipelineHandle VulkanCore::createPipeline(const PipelineConfig& config) {
    auto vertCode = readShaderFile(config.vertexShaderPath);
    auto fragCode = readShaderFile(config.fragmentShaderPath);
    
    if (vertCode.empty() || fragCode.empty()) {
        std::cerr << "[VulkanCore] Failed to load shaders: " << config.vertexShaderPath << ", " << config.fragmentShaderPath << std::endl;
        return INVALID_PIPELINE;
    }
    
    VkShaderModule vertModule = createShaderModule(vertCode);
    VkShaderModule fragModule = createShaderModule(fragCode);
    
    VkPipelineShaderStageCreateInfo stages[2] = {};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vertModule;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fragModule;
    stages[1].pName = "main";
    
    auto bindingDesc = getBindingDescription(config.vertexFormat);
    auto attrDescs = getAttributeDescriptions(config.vertexFormat);
    
    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &bindingDesc;
    vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(attrDescs.size());
    vertexInput.pVertexAttributeDescriptions = attrDescs.data();
    
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = config.topology;
    
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;
    
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.polygonMode = config.polygonMode;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = config.cullMode;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = config.depthTest ? VK_TRUE : VK_FALSE;
    depthStencil.depthWriteEnable = config.depthWrite ? VK_TRUE : VK_FALSE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | 
                                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    if (config.alphaBlend) {
        colorBlendAttachment.blendEnable = VK_TRUE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    }
    
    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    
    VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates = dynamicStates;
    
    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &m_descriptorSetLayout;
    
    VkPipelineLayout pipelineLayout;
    if (vkCreatePipelineLayout(m_device, &layoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
        vkDestroyShaderModule(m_device, vertModule, nullptr);
        vkDestroyShaderModule(m_device, fragModule, nullptr);
        return INVALID_PIPELINE;
    }
    
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
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = m_renderPass;
    pipelineInfo.subpass = 0;
    
    VkPipeline pipeline;
    VkResult result = vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);
    
    vkDestroyShaderModule(m_device, vertModule, nullptr);
    vkDestroyShaderModule(m_device, fragModule, nullptr);
    
    if (result != VK_SUCCESS) {
        vkDestroyPipelineLayout(m_device, pipelineLayout, nullptr);
        return INVALID_PIPELINE;
    }
    
    // Store
    PipelineHandle handle = static_cast<PipelineHandle>(m_pipelines.size());
    m_pipelines.push_back({pipeline, pipelineLayout, true});
    
    std::cout << "[VulkanCore] Pipeline created: " << config.vertexShaderPath << std::endl;
    return handle;
}

void VulkanCore::bindPipeline(PipelineHandle handle) {
    if (handle >= m_pipelines.size() || !m_pipelines[handle].valid) return;
    m_currentPipeline = handle;
    vkCmdBindPipeline(m_commandBuffers[m_currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelines[handle].pipeline);
}

void VulkanCore::destroyPipeline(PipelineHandle handle) {
    if (handle >= m_pipelines.size() || !m_pipelines[handle].valid) return;
    vkDestroyPipeline(m_device, m_pipelines[handle].pipeline, nullptr);
    vkDestroyPipelineLayout(m_device, m_pipelines[handle].layout, nullptr);
    m_pipelines[handle].valid = false;
}

// ============================================================================
// Buffer Management
// ============================================================================

BufferHandle VulkanCore::createVertexBuffer(const void* data, size_t size) {
    VkBuffer buffer;
    VkDeviceMemory memory;
    
    if (!createBufferInternal(size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                              buffer, memory)) {
        return INVALID_BUFFER;
    }
    
    void* mapped;
    vkMapMemory(m_device, memory, 0, size, 0, &mapped);
    memcpy(mapped, data, size);
    vkUnmapMemory(m_device, memory);
    
    BufferHandle handle = static_cast<BufferHandle>(m_buffers.size());
    m_buffers.push_back({buffer, memory, size, true});
    return handle;
}

BufferHandle VulkanCore::createIndexBuffer(const uint32_t* data, size_t count) {
    size_t size = count * sizeof(uint32_t);
    VkBuffer buffer;
    VkDeviceMemory memory;
    
    if (!createBufferInternal(size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                              buffer, memory)) {
        return INVALID_BUFFER;
    }
    
    void* mapped;
    vkMapMemory(m_device, memory, 0, size, 0, &mapped);
    memcpy(mapped, data, size);
    vkUnmapMemory(m_device, memory);
    
    BufferHandle handle = static_cast<BufferHandle>(m_buffers.size());
    m_buffers.push_back({buffer, memory, size, true});
    return handle;
}

BufferHandle VulkanCore::createUniformBuffer(size_t size) {
    VkBuffer buffer;
    VkDeviceMemory memory;
    
    if (!createBufferInternal(size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                              buffer, memory)) {
        return INVALID_BUFFER;
    }
    
    BufferHandle handle = static_cast<BufferHandle>(m_buffers.size());
    m_buffers.push_back({buffer, memory, size, true});
    return handle;
}

void VulkanCore::updateUniformBuffer(BufferHandle handle, const void* data, size_t size) {
    if (handle >= m_buffers.size() || !m_buffers[handle].valid) return;
    
    void* mapped;
    vkMapMemory(m_device, m_buffers[handle].memory, 0, size, 0, &mapped);
    memcpy(mapped, data, size);
    vkUnmapMemory(m_device, m_buffers[handle].memory);
}

void VulkanCore::destroyBuffer(BufferHandle handle) {
    if (handle >= m_buffers.size() || !m_buffers[handle].valid) return;
    vkDestroyBuffer(m_device, m_buffers[handle].buffer, nullptr);
    vkFreeMemory(m_device, m_buffers[handle].memory, nullptr);
    m_buffers[handle].valid = false;
}

// ============================================================================
// Texture Management
// ============================================================================

TextureHandle VulkanCore::loadTexture(const std::string& path) {
    // Load image using stb_image
    int width, height, channels;
    unsigned char* pixels = stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);
    if (!pixels) {
        std::cerr << "[VulkanCore] Failed to load texture: " << path << std::endl;
        return INVALID_TEXTURE;
    }
    
    TextureHandle handle = createTexture(pixels, width, height, 4);
    stbi_image_free(pixels);
    
    if (handle != INVALID_TEXTURE) {
        std::cout << "[VulkanCore] Loaded texture: " << path << " (" << width << "x" << height << ")" << std::endl;
    }
    return handle;
}

TextureHandle VulkanCore::createTexture(const uint8_t* pixels, uint32_t width, uint32_t height, uint32_t channels) {
    (void)channels; // Always convert to RGBA
    
    VkDeviceSize imageSize = width * height * 4;
    
    // Create staging buffer
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;
    
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = imageSize;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    if (vkCreateBuffer(m_device, &bufferInfo, nullptr, &stagingBuffer) != VK_SUCCESS) {
        return INVALID_TEXTURE;
    }
    
    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(m_device, stagingBuffer, &memReqs);
    
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits, 
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    
    if (vkAllocateMemory(m_device, &allocInfo, nullptr, &stagingMemory) != VK_SUCCESS) {
        vkDestroyBuffer(m_device, stagingBuffer, nullptr);
        return INVALID_TEXTURE;
    }
    
    vkBindBufferMemory(m_device, stagingBuffer, stagingMemory, 0);
    
    // Copy pixel data
    void* data;
    vkMapMemory(m_device, stagingMemory, 0, imageSize, 0, &data);
    memcpy(data, pixels, imageSize);
    vkUnmapMemory(m_device, stagingMemory);
    
    // Create image
    TextureResource tex;
    tex.width = width;
    tex.height = height;
    
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    
    if (vkCreateImage(m_device, &imageInfo, nullptr, &tex.image) != VK_SUCCESS) {
        vkDestroyBuffer(m_device, stagingBuffer, nullptr);
        vkFreeMemory(m_device, stagingMemory, nullptr);
        return INVALID_TEXTURE;
    }
    
    vkGetImageMemoryRequirements(m_device, tex.image, &memReqs);
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    
    if (vkAllocateMemory(m_device, &allocInfo, nullptr, &tex.memory) != VK_SUCCESS) {
        vkDestroyImage(m_device, tex.image, nullptr);
        vkDestroyBuffer(m_device, stagingBuffer, nullptr);
        vkFreeMemory(m_device, stagingMemory, nullptr);
        return INVALID_TEXTURE;
    }
    
    vkBindImageMemory(m_device, tex.image, tex.memory, 0);
    
    // Transition and copy
    VkCommandBuffer cmd = beginSingleTimeCommands();
    
    // Transition to transfer dst
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = tex.image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier);
    
    // Copy buffer to image
    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {width, height, 1};
    
    vkCmdCopyBufferToImage(cmd, stagingBuffer, tex.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    
    // Transition to shader read
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier);
    
    endSingleTimeCommands(cmd);
    
    // Cleanup staging
    vkDestroyBuffer(m_device, stagingBuffer, nullptr);
    vkFreeMemory(m_device, stagingMemory, nullptr);
    
    // Create image view
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = tex.image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    
    if (vkCreateImageView(m_device, &viewInfo, nullptr, &tex.view) != VK_SUCCESS) {
        vkDestroyImage(m_device, tex.image, nullptr);
        vkFreeMemory(m_device, tex.memory, nullptr);
        return INVALID_TEXTURE;
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
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    
    if (vkCreateSampler(m_device, &samplerInfo, nullptr, &tex.sampler) != VK_SUCCESS) {
        vkDestroyImageView(m_device, tex.view, nullptr);
        vkDestroyImage(m_device, tex.image, nullptr);
        vkFreeMemory(m_device, tex.memory, nullptr);
        return INVALID_TEXTURE;
    }
    
    tex.valid = true;
    
    // Find free slot or add new
    TextureHandle handle = INVALID_TEXTURE;
    for (size_t i = 0; i < m_textures.size(); ++i) {
        if (!m_textures[i].valid) {
            m_textures[i] = tex;
            handle = static_cast<TextureHandle>(i);
            break;
        }
    }
    if (handle == INVALID_TEXTURE) {
        handle = static_cast<TextureHandle>(m_textures.size());
        m_textures.push_back(tex);
    }
    
    return handle;
}

TextureHandle VulkanCore::createTextureLinear(const uint8_t* pixels, uint32_t width, uint32_t height, uint32_t channels) {
    (void)channels; // Always convert to RGBA
    
    VkDeviceSize imageSize = width * height * 4;
    
    // Create staging buffer
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;
    
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = imageSize;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    if (vkCreateBuffer(m_device, &bufferInfo, nullptr, &stagingBuffer) != VK_SUCCESS) {
        return INVALID_TEXTURE;
    }
    
    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(m_device, stagingBuffer, &memReqs);
    
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits, 
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    
    if (vkAllocateMemory(m_device, &allocInfo, nullptr, &stagingMemory) != VK_SUCCESS) {
        vkDestroyBuffer(m_device, stagingBuffer, nullptr);
        return INVALID_TEXTURE;
    }
    
    vkBindBufferMemory(m_device, stagingBuffer, stagingMemory, 0);
    
    // Copy pixel data
    void* data;
    vkMapMemory(m_device, stagingMemory, 0, imageSize, 0, &data);
    memcpy(data, pixels, imageSize);
    vkUnmapMemory(m_device, stagingMemory);
    
    // Create image
    TextureResource tex;
    tex.width = width;
    tex.height = height;
    
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;  // LINEAR format (no SRGB) - critical for DMap textures!
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    
    if (vkCreateImage(m_device, &imageInfo, nullptr, &tex.image) != VK_SUCCESS) {
        vkDestroyBuffer(m_device, stagingBuffer, nullptr);
        vkFreeMemory(m_device, stagingMemory, nullptr);
        return INVALID_TEXTURE;
    }
    
    vkGetImageMemoryRequirements(m_device, tex.image, &memReqs);
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    
    if (vkAllocateMemory(m_device, &allocInfo, nullptr, &tex.memory) != VK_SUCCESS) {
        vkDestroyImage(m_device, tex.image, nullptr);
        vkDestroyBuffer(m_device, stagingBuffer, nullptr);
        vkFreeMemory(m_device, stagingMemory, nullptr);
        return INVALID_TEXTURE;
    }
    
    vkBindImageMemory(m_device, tex.image, tex.memory, 0);
    
    // Transition and copy
    VkCommandBuffer cmd = beginSingleTimeCommands();
    
    // Transition to transfer dst
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = tex.image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier);
    
    // Copy buffer to image
    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {width, height, 1};
    
    vkCmdCopyBufferToImage(cmd, stagingBuffer, tex.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    
    // Transition to shader read
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier);
    
    endSingleTimeCommands(cmd);
    
    // Cleanup staging
    vkDestroyBuffer(m_device, stagingBuffer, nullptr);
    vkFreeMemory(m_device, stagingMemory, nullptr);
    
    // Create image view
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = tex.image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;  // LINEAR format to match image
    viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    
    if (vkCreateImageView(m_device, &viewInfo, nullptr, &tex.view) != VK_SUCCESS) {
        vkDestroyImage(m_device, tex.image, nullptr);
        vkFreeMemory(m_device, tex.memory, nullptr);
        return INVALID_TEXTURE;
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
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    
    if (vkCreateSampler(m_device, &samplerInfo, nullptr, &tex.sampler) != VK_SUCCESS) {
        vkDestroyImageView(m_device, tex.view, nullptr);
        vkDestroyImage(m_device, tex.image, nullptr);
        vkFreeMemory(m_device, tex.memory, nullptr);
        return INVALID_TEXTURE;
    }
    
    tex.valid = true;
    
    // Find free slot or add new
    TextureHandle handle = INVALID_TEXTURE;
    for (size_t i = 0; i < m_textures.size(); ++i) {
        if (!m_textures[i].valid) {
            m_textures[i] = tex;
            handle = static_cast<TextureHandle>(i);
            break;
        }
    }
    if (handle == INVALID_TEXTURE) {
        handle = static_cast<TextureHandle>(m_textures.size());
        m_textures.push_back(tex);
    }
    
    return handle;
}

void VulkanCore::bindTexture(TextureHandle handle) {
    TextureHandle texToUse = m_defaultTexture;
    if (handle < m_textures.size() && m_textures[handle].valid) {
        texToUse = handle;
    }
    
    if (texToUse == m_currentTexture) return;  // Already bound
    m_currentTexture = texToUse;
    
    // Update descriptor set with new texture
    VkDescriptorImageInfo imageInfo{};
    imageInfo.sampler = m_textures[texToUse].sampler;
    imageInfo.imageView = m_textures[texToUse].view;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    
    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = m_objectDescriptorSet;
    write.dstBinding = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.descriptorCount = 1;
    write.pImageInfo = &imageInfo;
    
    vkUpdateDescriptorSets(m_device, 1, &write, 0, nullptr);
}

void VulkanCore::destroyTexture(TextureHandle handle) {
    if (handle >= m_textures.size() || !m_textures[handle].valid) return;
    
    TextureResource& tex = m_textures[handle];
    if (tex.sampler) vkDestroySampler(m_device, tex.sampler, nullptr);
    if (tex.view) vkDestroyImageView(m_device, tex.view, nullptr);
    if (tex.image) vkDestroyImage(m_device, tex.image, nullptr);
    if (tex.memory) vkFreeMemory(m_device, tex.memory, nullptr);
    tex.valid = false;
}

// ============================================================================
// Mesh Management
// ============================================================================

MeshHandle VulkanCore::createMesh(const MeshData& data) {
    BufferHandle vb = createVertexBuffer(data.vertices.data(), data.vertices.size() * sizeof(float));
    BufferHandle ib = createIndexBuffer(data.indices.data(), data.indices.size());
    
    if (vb == INVALID_BUFFER || ib == INVALID_BUFFER) {
        if (vb != INVALID_BUFFER) destroyBuffer(vb);
        if (ib != INVALID_BUFFER) destroyBuffer(ib);
        return INVALID_MESH;
    }
    
    MeshHandle handle = static_cast<MeshHandle>(m_meshes.size());
    m_meshes.push_back({vb, ib, static_cast<uint32_t>(data.indices.size()), true});
    return handle;
}

MeshHandle VulkanCore::createCube(float size, const glm::vec3& color) {
    // Scale and colorize cube vertices
    std::vector<float> vertices;
    for (int i = 0; i < 24; i++) {
        vertices.push_back(CUBE_VERTICES[i * 6 + 0] * size);  // x
        vertices.push_back(CUBE_VERTICES[i * 6 + 1] * size);  // y
        vertices.push_back(CUBE_VERTICES[i * 6 + 2] * size);  // z
        vertices.push_back(CUBE_VERTICES[i * 6 + 3] * color.r); // r
        vertices.push_back(CUBE_VERTICES[i * 6 + 4] * color.g); // g
        vertices.push_back(CUBE_VERTICES[i * 6 + 5] * color.b); // b
    }
    
    MeshData data;
    data.vertices = vertices;
    data.indices = std::vector<uint32_t>(CUBE_INDICES, CUBE_INDICES + 36);
    data.format = VertexFormat::POSITION_COLOR;
    data.vertexCount = 24;
    data.indexCount = 36;
    
    return createMesh(data);
}

void VulkanCore::destroyMesh(MeshHandle handle) {
    if (handle >= m_meshes.size() || !m_meshes[handle].valid) return;
    destroyBuffer(m_meshes[handle].vertexBuffer);
    destroyBuffer(m_meshes[handle].indexBuffer);
    m_meshes[handle].valid = false;
}

bool VulkanCore::getMeshBuffers(MeshHandle mesh, VkBuffer& vertexBuffer, VkBuffer& indexBuffer, uint32_t& indexCount) const {
    if (mesh >= m_meshes.size() || !m_meshes[mesh].valid) return false;
    
    const auto& meshRes = m_meshes[mesh];
    if (meshRes.vertexBuffer >= m_buffers.size() || !m_buffers[meshRes.vertexBuffer].valid) return false;
    if (meshRes.indexBuffer >= m_buffers.size() || !m_buffers[meshRes.indexBuffer].valid) return false;
    
    vertexBuffer = m_buffers[meshRes.vertexBuffer].buffer;
    indexBuffer = m_buffers[meshRes.indexBuffer].buffer;
    indexCount = meshRes.indexCount;
    return true;
}

bool VulkanCore::getTextureInfo(TextureHandle tex, VkImageView& view, VkSampler& sampler) const {
    if (tex >= m_textures.size() || !m_textures[tex].valid) return false;
    
    view = m_textures[tex].view;
    sampler = m_textures[tex].sampler;
    return true;
}

// ============================================================================
// Drawing
// ============================================================================

void VulkanCore::drawMesh(MeshHandle mesh, const glm::mat4& transform, const glm::vec4& color) {
    if (!m_frameStarted) return;
    if (mesh >= m_meshes.size() || !m_meshes[mesh].valid) return;
    if (m_currentPipeline == INVALID_PIPELINE) return;
    
    // Update UBO
    StandardUBO ubo;
    ubo.model = transform;
    ubo.view = m_viewMatrix;
    ubo.projection = m_projMatrix;
    ubo.color = color;
    updateUniformBuffer(m_objectUBO, &ubo, sizeof(ubo));
    
    VkCommandBuffer cmd = m_commandBuffers[m_currentFrame];
    
    // Bind descriptor set
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_pipelines[m_currentPipeline].layout,
                            0, 1, &m_objectDescriptorSet, 0, nullptr);
    
    // Bind vertex buffer
    VkBuffer vertexBuffers[] = {m_buffers[m_meshes[mesh].vertexBuffer].buffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);
    
    // Bind index buffer
    vkCmdBindIndexBuffer(cmd, m_buffers[m_meshes[mesh].indexBuffer].buffer, 0, VK_INDEX_TYPE_UINT32);
    
    // Draw
    vkCmdDrawIndexed(cmd, m_meshes[mesh].indexCount, 1, 0, 0, 0);
}

void VulkanCore::drawVertices(BufferHandle vertexBuffer, uint32_t vertexCount) {
    if (!m_frameStarted || vertexBuffer >= m_buffers.size()) return;
    
    VkCommandBuffer cmd = m_commandBuffers[m_currentFrame];
    VkBuffer buffers[] = {m_buffers[vertexBuffer].buffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, buffers, offsets);
    vkCmdDraw(cmd, vertexCount, 1, 0, 0);
}

void VulkanCore::drawIndexed(BufferHandle vertexBuffer, BufferHandle indexBuffer, uint32_t indexCount) {
    if (!m_frameStarted) return;
    if (vertexBuffer >= m_buffers.size() || indexBuffer >= m_buffers.size()) return;
    
    VkCommandBuffer cmd = m_commandBuffers[m_currentFrame];
    VkBuffer buffers[] = {m_buffers[vertexBuffer].buffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, buffers, offsets);
    vkCmdBindIndexBuffer(cmd, m_buffers[indexBuffer].buffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmd, indexCount, 1, 0, 0, 0);
}

// ============================================================================
// Camera
// ============================================================================

void VulkanCore::setViewMatrix(const glm::mat4& view) {
    m_viewMatrix = view;
}

void VulkanCore::setProjectionMatrix(const glm::mat4& proj) {
    m_projMatrix = proj;
}

void VulkanCore::setCamera(const glm::vec3& position, const glm::vec3& target, const glm::vec3& up) {
    m_viewMatrix = glm::lookAt(position, target, up);
}

void VulkanCore::setPerspective(float fovDegrees, float nearPlane, float farPlane) {
    m_projMatrix = glm::perspective(glm::radians(fovDegrees), getAspectRatio(), nearPlane, farPlane);
    m_projMatrix[1][1] *= -1;  // Vulkan Y flip
}

// ============================================================================
// Helpers
// ============================================================================

std::vector<char> VulkanCore::readShaderFile(const std::string& path) {
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open()) return {};
    
    size_t size = (size_t)file.tellg();
    std::vector<char> buffer(size);
    file.seekg(0);
    file.read(buffer.data(), size);
    return buffer;
}

VkShaderModule VulkanCore::createShaderModule(const std::vector<char>& code) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());
    
    VkShaderModule module;
    vkCreateShaderModule(m_device, &createInfo, nullptr, &module);
    return module;
}

uint32_t VulkanCore::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memProps);
    
    for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProps.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    return 0;
}

VkCommandBuffer VulkanCore::beginSingleTimeCommands() {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = m_commandPool;
    allocInfo.commandBufferCount = 1;
    
    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(m_device, &allocInfo, &cmd);
    
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    
    vkBeginCommandBuffer(cmd, &beginInfo);
    return cmd;
}

void VulkanCore::endSingleTimeCommands(VkCommandBuffer cmd) {
    vkEndCommandBuffer(cmd);
    
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;
    
    vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_graphicsQueue);
    
    vkFreeCommandBuffers(m_device, m_commandPool, 1, &cmd);
}

bool VulkanCore::createBufferInternal(VkDeviceSize size, VkBufferUsageFlags usage,
                                       VkMemoryPropertyFlags properties,
                                       VkBuffer& buffer, VkDeviceMemory& memory) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    if (vkCreateBuffer(m_device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        return false;
    }
    
    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(m_device, buffer, &memReqs);
    
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits, properties);
    
    if (vkAllocateMemory(m_device, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
        vkDestroyBuffer(m_device, buffer, nullptr);
        return false;
    }
    
    vkBindBufferMemory(m_device, buffer, memory, 0);
    return true;
}

VkVertexInputBindingDescription VulkanCore::getBindingDescription(VertexFormat format) {
    VkVertexInputBindingDescription desc{};
    desc.binding = 0;
    desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    
    switch (format) {
        case VertexFormat::POSITION_COLOR:
            desc.stride = sizeof(float) * 6;  // vec3 pos + vec3 color
            break;
        case VertexFormat::POSITION_NORMAL_UV:
            desc.stride = sizeof(float) * 8;  // vec3 pos + vec3 normal + vec2 uv
            break;
        case VertexFormat::POSITION_NORMAL_UV_COLOR:
            desc.stride = sizeof(float) * 11; // vec3 pos + vec3 normal + vec2 uv + vec3 color
            break;
        case VertexFormat::POSITION_NORMAL_UV0_UV1:
            desc.stride = sizeof(float) * 10; // vec3 pos + vec3 normal + vec2 uv0 + vec2 uv1
            break;
        default:
            desc.stride = sizeof(float) * 6;
    }
    return desc;
}

std::vector<VkVertexInputAttributeDescription> VulkanCore::getAttributeDescriptions(VertexFormat format) {
    std::vector<VkVertexInputAttributeDescription> attrs;
    
    switch (format) {
        case VertexFormat::POSITION_COLOR:
            attrs.resize(2);
            attrs[0] = {0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0};                    // position
            attrs[1] = {1, 0, VK_FORMAT_R32G32B32_SFLOAT, sizeof(float) * 3};    // color
            break;
        case VertexFormat::POSITION_NORMAL_UV:
            attrs.resize(3);
            attrs[0] = {0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0};                    // position
            attrs[1] = {1, 0, VK_FORMAT_R32G32B32_SFLOAT, sizeof(float) * 3};    // normal
            attrs[2] = {2, 0, VK_FORMAT_R32G32_SFLOAT, sizeof(float) * 6};       // uv
            break;
        case VertexFormat::POSITION_NORMAL_UV_COLOR:
            attrs.resize(4);
            attrs[0] = {0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0};                    // position
            attrs[1] = {1, 0, VK_FORMAT_R32G32B32_SFLOAT, sizeof(float) * 3};    // normal
            attrs[2] = {2, 0, VK_FORMAT_R32G32_SFLOAT, sizeof(float) * 6};       // uv
            attrs[3] = {3, 0, VK_FORMAT_R32G32B32_SFLOAT, sizeof(float) * 8};    // color
            break;
        case VertexFormat::POSITION_NORMAL_UV0_UV1:
            attrs.resize(4);
            attrs[0] = {0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0};                    // position
            attrs[1] = {1, 0, VK_FORMAT_R32G32B32_SFLOAT, sizeof(float) * 3};    // normal
            attrs[2] = {2, 0, VK_FORMAT_R32G32_SFLOAT, sizeof(float) * 6};       // uv0 (textures)
            attrs[3] = {3, 0, VK_FORMAT_R32G32_SFLOAT, sizeof(float) * 8};       // uv1 (DMap)
            break;
        default:
            attrs.resize(2);
            attrs[0] = {0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0};
            attrs[1] = {1, 0, VK_FORMAT_R32G32B32_SFLOAT, sizeof(float) * 3};
    }
    return attrs;
}

// ============================================================================
// ImGui Integration (Optional)
// ============================================================================

bool VulkanCore::initImGui(GLFWwindow* window) {
#ifdef VKCORE_ENABLE_IMGUI
    if (m_imguiInitialized) return true;
    
    // Create descriptor pool for ImGui
    VkDescriptorPoolSize poolSizes[] = {
        {VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}
    };
    
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = 1000;
    poolInfo.poolSizeCount = sizeof(poolSizes) / sizeof(poolSizes[0]);
    poolInfo.pPoolSizes = poolSizes;
    
    if (vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_imguiDescriptorPool) != VK_SUCCESS) {
        std::cerr << "[VulkanCore] Failed to create ImGui descriptor pool" << std::endl;
        return false;
    }
    
    // Initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    
    ImGui::StyleColorsDark();
    
    // Init platform/renderer backends
    ImGui_ImplGlfw_InitForVulkan(window, true);
    
    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.ApiVersion = VK_API_VERSION_1_0;
    initInfo.Instance = m_instance;
    initInfo.PhysicalDevice = m_physicalDevice;
    initInfo.Device = m_device;
    initInfo.QueueFamily = m_graphicsFamily;
    initInfo.Queue = m_graphicsQueue;
    initInfo.PipelineCache = VK_NULL_HANDLE;
    initInfo.DescriptorPool = m_imguiDescriptorPool;
    initInfo.MinImageCount = 2;
    initInfo.ImageCount = static_cast<uint32_t>(m_swapchainImages.size());
    initInfo.PipelineInfoMain.RenderPass = m_renderPass;
    initInfo.PipelineInfoMain.Subpass = 0;
    
    ImGui_ImplVulkan_Init(&initInfo);
    // Fonts are uploaded automatically in newer ImGui versions
    
    m_imguiInitialized = true;
    std::cout << "[VulkanCore] ImGui initialized" << std::endl;
    return true;
#else
    std::cerr << "[VulkanCore] ImGui support not compiled in (define VKCORE_ENABLE_IMGUI)" << std::endl;
    return false;
#endif
}

void VulkanCore::shutdownImGui() {
#ifdef VKCORE_ENABLE_IMGUI
    if (!m_imguiInitialized) return;
    
    vkDeviceWaitIdle(m_device);
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    
    if (m_imguiDescriptorPool) {
        vkDestroyDescriptorPool(m_device, m_imguiDescriptorPool, nullptr);
        m_imguiDescriptorPool = VK_NULL_HANDLE;
    }
    
    m_imguiInitialized = false;
#endif
}

void VulkanCore::beginImGuiFrame() {
#ifdef VKCORE_ENABLE_IMGUI
    if (!m_imguiInitialized) return;
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
#endif
}

void VulkanCore::renderImGui() {
#ifdef VKCORE_ENABLE_IMGUI
    if (!m_imguiInitialized || !m_frameStarted) return;
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), m_commandBuffers[m_currentFrame]);
#endif
}

} // namespace vkcore

// ============================================================================
// C API Implementation (for HEIDIC)
// ============================================================================

static vkcore::VulkanCore* g_core = nullptr;

extern "C" int vkcore_init(void* glfwWindow, const char* appName, int width, int height) {
    if (g_core) return 1;  // Already initialized
    
    g_core = new vkcore::VulkanCore();
    vkcore::CoreConfig config;
    config.appName = appName ? appName : "VulkanCore App";
    config.width = width;
    config.height = height;
    
    if (!g_core->init(static_cast<GLFWwindow*>(glfwWindow), config)) {
        delete g_core;
        g_core = nullptr;
        return 0;
    }
    return 1;
}

extern "C" void vkcore_shutdown() {
    if (g_core) {
        delete g_core;
        g_core = nullptr;
    }
}

extern "C" int vkcore_is_initialized() {
    return g_core && g_core->isInitialized() ? 1 : 0;
}

extern "C" int vkcore_begin_frame() {
    return g_core ? (g_core->beginFrame() ? 1 : 0) : 0;
}

extern "C" void vkcore_end_frame() {
    if (g_core) g_core->endFrame();
}

extern "C" unsigned int vkcore_create_pipeline(const char* vertShader, const char* fragShader, int vertexFormat) {
    if (!g_core) return vkcore::INVALID_PIPELINE;
    
    vkcore::PipelineConfig config;
    config.vertexShaderPath = vertShader;
    config.fragmentShaderPath = fragShader;
    config.vertexFormat = static_cast<vkcore::VertexFormat>(vertexFormat);
    
    return g_core->createPipeline(config);
}

extern "C" void vkcore_bind_pipeline(unsigned int handle) {
    if (g_core) g_core->bindPipeline(handle);
}

extern "C" void vkcore_destroy_pipeline(unsigned int handle) {
    if (g_core) g_core->destroyPipeline(handle);
}

extern "C" unsigned int vkcore_create_cube(float size, float r, float g, float b) {
    if (!g_core) return vkcore::INVALID_MESH;
    return g_core->createCube(size, glm::vec3(r, g, b));
}

extern "C" unsigned int vkcore_create_mesh(const float* vertices, int vertexCount,
                                            const unsigned int* indices, int indexCount, int vertexFormat) {
    if (!g_core) return vkcore::INVALID_MESH;
    
    vkcore::MeshData data;
    data.vertices = std::vector<float>(vertices, vertices + vertexCount * 6);  // Assuming 6 floats per vertex
    data.indices = std::vector<uint32_t>(indices, indices + indexCount);
    data.format = static_cast<vkcore::VertexFormat>(vertexFormat);
    data.vertexCount = vertexCount;
    data.indexCount = indexCount;
    
    return g_core->createMesh(data);
}

extern "C" void vkcore_destroy_mesh(unsigned int handle) {
    if (g_core) g_core->destroyMesh(handle);
}

extern "C" void vkcore_draw_mesh(unsigned int meshHandle,
                                  float px, float py, float pz,
                                  float rx, float ry, float rz,
                                  float sx, float sy, float sz,
                                  float r, float g, float b, float a) {
    if (!g_core) return;
    
    glm::mat4 transform = glm::mat4(1.0f);
    transform = glm::translate(transform, glm::vec3(px, py, pz));
    transform = glm::rotate(transform, glm::radians(rx), glm::vec3(1, 0, 0));
    transform = glm::rotate(transform, glm::radians(ry), glm::vec3(0, 1, 0));
    transform = glm::rotate(transform, glm::radians(rz), glm::vec3(0, 0, 1));
    transform = glm::scale(transform, glm::vec3(sx, sy, sz));
    
    g_core->drawMesh(meshHandle, transform, glm::vec4(r, g, b, a));
}

extern "C" void vkcore_set_camera(float eyeX, float eyeY, float eyeZ,
                                   float targetX, float targetY, float targetZ) {
    if (g_core) {
        g_core->setCamera(glm::vec3(eyeX, eyeY, eyeZ), glm::vec3(targetX, targetY, targetZ));
    }
}

extern "C" void vkcore_set_perspective(float fovDegrees, float nearPlane, float farPlane) {
    if (g_core) g_core->setPerspective(fovDegrees, nearPlane, farPlane);
}

extern "C" void vkcore_set_view_matrix(const float* mat4) {
    if (g_core && mat4) {
        g_core->setViewMatrix(glm::make_mat4(mat4));
    }
}

extern "C" void vkcore_set_projection_matrix(const float* mat4) {
    if (g_core && mat4) {
        g_core->setProjectionMatrix(glm::make_mat4(mat4));
    }
}

extern "C" int vkcore_get_width() {
    return g_core ? g_core->getWidth() : 0;
}

extern "C" int vkcore_get_height() {
    return g_core ? g_core->getHeight() : 0;
}

extern "C" float vkcore_get_aspect_ratio() {
    return g_core ? g_core->getAspectRatio() : 1.0f;
}

// ImGui C API
extern "C" int vkcore_init_imgui(void* glfwWindow) {
    return g_core ? (g_core->initImGui(static_cast<GLFWwindow*>(glfwWindow)) ? 1 : 0) : 0;
}

extern "C" void vkcore_shutdown_imgui() {
    if (g_core) g_core->shutdownImGui();
}

extern "C" void vkcore_begin_imgui_frame() {
    if (g_core) g_core->beginImGuiFrame();
}

extern "C" void vkcore_render_imgui() {
    if (g_core) g_core->renderImGui();
}

extern "C" int vkcore_is_imgui_initialized() {
    return g_core ? (g_core->isImGuiInitialized() ? 1 : 0) : 0;
}

