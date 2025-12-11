// EDEN ENGINE Vulkan Helper Functions Implementation
// Simplified implementation for spinning triangle

#include "eden_vulkan_helpers.h"
#include <iostream>
#include <vector>
#include <cstring>
#include <thread>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <array>
#include <fstream>
#include <string>
#include <memory>
#include <cstddef>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif
#include "../stdlib/dds_loader.h"
#include "../stdlib/png_loader.h"
#include "../stdlib/texture_resource.h"
#include "../stdlib/mesh_resource.h"

// ImGui includes (if available)
#ifdef USE_IMGUI
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
#endif

// Vertex structure
struct Vertex {
    float pos[3];
    float color[3];
};

// Uniform buffer object
struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};

// Global Vulkan state (accessible to TextureResource and other resource classes)
VkInstance g_instance = VK_NULL_HANDLE;
VkPhysicalDevice g_physicalDevice = VK_NULL_HANDLE;
VkSurfaceKHR g_surface = VK_NULL_HANDLE;
VkDevice g_device = VK_NULL_HANDLE;
VkSwapchainKHR g_swapchain = VK_NULL_HANDLE;
VkRenderPass g_renderPass = VK_NULL_HANDLE;
VkPipeline g_pipeline = VK_NULL_HANDLE;
VkPipeline g_cubePipeline = VK_NULL_HANDLE;
VkPipelineLayout g_pipelineLayout = VK_NULL_HANDLE;
// Vertex buffers for custom shaders (when they need vertex input)
VkBuffer g_triangleVertexBuffer = VK_NULL_HANDLE;
VkDeviceMemory g_triangleVertexBufferMemory = VK_NULL_HANDLE;
bool g_usingCustomShaders = false; // Track if we're using custom shaders that need vertex buffers
std::vector<VkFramebuffer> g_framebuffers;
VkCommandPool g_commandPool = VK_NULL_HANDLE;
std::vector<VkCommandBuffer> g_commandBuffers;
VkQueue g_graphicsQueue = VK_NULL_HANDLE;
static uint32_t g_graphicsQueueFamilyIndex = 0;
static uint32_t g_currentFrame = 0;
static VkExtent2D g_swapchainExtent = {};

// Additional state
static std::vector<VkImage> g_swapchainImages;
static std::vector<VkImageView> g_swapchainImageViews;
static VkSemaphore g_imageAvailableSemaphore = VK_NULL_HANDLE;
static VkSemaphore g_renderFinishedSemaphore = VK_NULL_HANDLE;
static VkFence g_inFlightFence = VK_NULL_HANDLE;
static uint32_t g_swapchainImageCount = 0;
static VkFormat g_swapchainImageFormat = VK_FORMAT_UNDEFINED;

// Depth buffer
static VkImage g_depthImage = VK_NULL_HANDLE;
static VkDeviceMemory g_depthImageMemory = VK_NULL_HANDLE;
static VkImageView g_depthImageView = VK_NULL_HANDLE;
static VkFormat g_depthFormat = VK_FORMAT_D32_SFLOAT;

// Descriptor sets
static VkDescriptorSetLayout g_descriptorSetLayout = VK_NULL_HANDLE;
static VkDescriptorPool g_descriptorPool = VK_NULL_HANDLE;
static std::vector<VkDescriptorSet> g_descriptorSets;
static std::vector<VkBuffer> g_uniformBuffers;
static std::vector<VkDeviceMemory> g_uniformBuffersMemory;

// ImGui state
#ifdef USE_IMGUI
static bool g_imguiInitialized = false;
static VkDescriptorPool g_imguiDescriptorPool = VK_NULL_HANDLE;
#endif

// Note: Triangle vertices are hardcoded in the shader, no vertex buffer needed

// Rotation state for spinning triangle
static float g_rotationAngle = 0.0f;
static std::chrono::high_resolution_clock::time_point g_lastTime = std::chrono::high_resolution_clock::now();
static float g_rotationSpeed = 1.0f;

// Shader module handles for hot-reload
static VkShaderModule g_vertShaderModule = VK_NULL_HANDLE;
static VkShaderModule g_fragShaderModule = VK_NULL_HANDLE;
static VkShaderModule g_cubeVertShaderModule = VK_NULL_HANDLE;
static VkShaderModule g_cubeFragShaderModule = VK_NULL_HANDLE;

// Exposed control so the example can drive rotation speed (e.g., via hot-reload)
extern "C" void heidic_set_rotation_speed(float speed) {
    g_rotationSpeed = speed;
}

// Helper to read binary file (for shaders)
static std::vector<char> readFile(const std::string& filename) {
    std::vector<std::string> paths = {
        filename,
        "../" + filename,
        "../../" + filename,
        "../../../" + filename,
        "../../../../" + filename,
        "shaders/" + filename,
        "../shaders/" + filename,
        "../../shaders/" + filename,
        "examples/spinning_cube/" + filename,
        "../examples/spinning_cube/" + filename,
        "../../examples/spinning_cube/" + filename,
        "examples/spinning_triangle/" + filename,
        "../examples/spinning_triangle/" + filename,
        "../../examples/spinning_triangle/" + filename,
        "../spinning_triangle/" + filename,
        "../../spinning_triangle/" + filename
    };
    
    for (const auto& path : paths) {
        std::ifstream file(path, std::ios::ate | std::ios::binary);
        if (file.is_open()) {
            size_t fileSize = (size_t) file.tellg();
            std::vector<char> buffer(fileSize);
            file.seekg(0);
            file.read(buffer.data(), fileSize);
            file.close();
            return buffer;
        }
    }
    
    throw std::runtime_error("failed to open file: " + filename);
}

// Helper to find graphics queue family
static uint32_t findGraphicsQueueFamily(VkPhysicalDevice device, VkSurfaceKHR surface) {
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());
    
    for (uint32_t i = 0; i < queueFamilyCount; i++) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
            if (presentSupport) {
                return i;
            }
        }
    }
    return UINT32_MAX;
}

// Helper to find memory type
static uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(g_physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    return 0; 
}

// Helper to create buffer
static void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(g_device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        std::cerr << "[EDEN] Failed to create buffer!" << std::endl;
        return;
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(g_device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(g_device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        std::cerr << "[EDEN] Failed to allocate buffer memory!" << std::endl;
        return;
    }

    vkBindBufferMemory(g_device, buffer, bufferMemory, 0);
}

// Helper to find supported format
static VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
    for (VkFormat format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(g_physicalDevice, format, &props);
        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
            return format;
        } else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
            return format;
        }
    }
    return VK_FORMAT_UNDEFINED;
}

// Helper to create image
static void createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory) {
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

    if (vkCreateImage(g_device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
        std::cerr << "[EDEN] Failed to create image!" << std::endl;
        return;
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(g_device, image, &memRequirements);

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(g_device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
        std::cerr << "[EDEN] Failed to allocate image memory!" << std::endl;
        return;
    }

    vkBindImageMemory(g_device, image, imageMemory, 0);
}

// Create depth buffer
static void createDepthResources() {
    g_depthFormat = findSupportedFormat(
        {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
    );

    createImage(g_swapchainExtent.width, g_swapchainExtent.height, g_depthFormat,
                VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, g_depthImage, g_depthImageMemory);

    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = g_depthImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = g_depthFormat;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(g_device, &viewInfo, nullptr, &g_depthImageView) != VK_SUCCESS) {
        std::cerr << "[EDEN] Failed to create depth image view!" << std::endl;
    }
}

// Configure GLFW for Vulkan
extern "C" void heidic_glfw_vulkan_hints() {
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
}

// Initialize Vulkan renderer
extern "C" int heidic_init_renderer(GLFWwindow* window) {
    if (window == nullptr) {
        std::cerr << "[EDEN] ERROR: Invalid window!" << std::endl;
        return 0;
    }
    
    std::cout << "[EDEN] Initializing Vulkan renderer..." << std::endl;
    
    // 1. Create Vulkan instance
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "EDEN ENGINE";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "EDEN Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;
    
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    
    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = glfwExtensionCount;
    createInfo.ppEnabledExtensionNames = glfwExtensions;
    createInfo.enabledLayerCount = 0;
    
    if (vkCreateInstance(&createInfo, nullptr, &g_instance) != VK_SUCCESS) {
        std::cerr << "[EDEN] ERROR: Failed to create Vulkan instance!" << std::endl;
        return 0;
    }
    
    // 2. Create surface
    if (glfwCreateWindowSurface(g_instance, window, nullptr, &g_surface) != VK_SUCCESS) {
        std::cerr << "[EDEN] ERROR: Failed to create window surface!" << std::endl;
        vkDestroyInstance(g_instance, nullptr);
        return 0;
    }
    
    // 3. Select physical device
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(g_instance, &deviceCount, nullptr);
    if (deviceCount == 0) {
        std::cerr << "[EDEN] ERROR: No Vulkan-capable devices found!" << std::endl;
        vkDestroySurfaceKHR(g_instance, g_surface, nullptr);
        vkDestroyInstance(g_instance, nullptr);
        return 0;
    }
    
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(g_instance, &deviceCount, devices.data());
    g_physicalDevice = devices[0];
    
    // 4. Find graphics queue family
    g_graphicsQueueFamilyIndex = findGraphicsQueueFamily(g_physicalDevice, g_surface);
    if (g_graphicsQueueFamilyIndex == UINT32_MAX) {
        std::cerr << "[EDEN] ERROR: No suitable queue family found!" << std::endl;
        vkDestroySurfaceKHR(g_instance, g_surface, nullptr);
        vkDestroyInstance(g_instance, nullptr);
        return 0;
    }
    
    // 5. Create logical device
    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo = {};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = g_graphicsQueueFamilyIndex;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;
    
    VkPhysicalDeviceFeatures deviceFeatures = {};
    
    VkDeviceCreateInfo deviceCreateInfo = {};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.pEnabledFeatures = &deviceFeatures;
    
    const char* deviceExtensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    deviceCreateInfo.enabledExtensionCount = 1;
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions;
    
    if (vkCreateDevice(g_physicalDevice, &deviceCreateInfo, nullptr, &g_device) != VK_SUCCESS) {
        std::cerr << "[EDEN] ERROR: Failed to create logical device!" << std::endl;
        vkDestroySurfaceKHR(g_instance, g_surface, nullptr);
        vkDestroyInstance(g_instance, nullptr);
        return 0;
    }
    
    vkGetDeviceQueue(g_device, g_graphicsQueueFamilyIndex, 0, &g_graphicsQueue);
    
    // 6. Create swapchain
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g_physicalDevice, g_surface, &capabilities);
    
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(g_physicalDevice, g_surface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(g_physicalDevice, g_surface, &formatCount, formats.data());
    
    VkSurfaceFormatKHR surfaceFormat = formats[0];
    for (const auto& format : formats) {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            surfaceFormat = format;
            break;
        }
    }
    g_swapchainImageFormat = surfaceFormat.format;
    
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    
    VkExtent2D swapchainExtent;
    if (capabilities.currentExtent.width != UINT32_MAX) {
        swapchainExtent = capabilities.currentExtent;
    } else {
        swapchainExtent.width = std::clamp(static_cast<uint32_t>(width), capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        swapchainExtent.height = std::clamp(static_cast<uint32_t>(height), capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
    }
    g_swapchainExtent = swapchainExtent;
    
    uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
        imageCount = capabilities.maxImageCount;
    }
    
    VkSwapchainCreateInfoKHR swapchainCreateInfo = {};
    swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainCreateInfo.surface = g_surface;
    swapchainCreateInfo.minImageCount = imageCount;
    swapchainCreateInfo.imageFormat = surfaceFormat.format;
    swapchainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;
    swapchainCreateInfo.imageExtent = swapchainExtent;
    swapchainCreateInfo.imageArrayLayers = 1;
    swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchainCreateInfo.preTransform = capabilities.currentTransform;
    swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchainCreateInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    swapchainCreateInfo.clipped = VK_TRUE;
    swapchainCreateInfo.oldSwapchain = VK_NULL_HANDLE;
    
    if (vkCreateSwapchainKHR(g_device, &swapchainCreateInfo, nullptr, &g_swapchain) != VK_SUCCESS) {
        std::cerr << "[EDEN] ERROR: Failed to create swapchain!" << std::endl;
        vkDestroyDevice(g_device, nullptr);
        vkDestroySurfaceKHR(g_instance, g_surface, nullptr);
        vkDestroyInstance(g_instance, nullptr);
        return 0;
    }
    
    vkGetSwapchainImagesKHR(g_device, g_swapchain, &g_swapchainImageCount, nullptr);
    g_swapchainImages.resize(g_swapchainImageCount);
    vkGetSwapchainImagesKHR(g_device, g_swapchain, &g_swapchainImageCount, g_swapchainImages.data());
    
    g_swapchainImageViews.resize(g_swapchainImageCount);
    for (uint32_t i = 0; i < g_swapchainImageCount; i++) {
        VkImageViewCreateInfo viewInfo = {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = g_swapchainImages[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = g_swapchainImageFormat;
        viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;
        
        if (vkCreateImageView(g_device, &viewInfo, nullptr, &g_swapchainImageViews[i]) != VK_SUCCESS) {
            std::cerr << "[EDEN] ERROR: Failed to create image view!" << std::endl;
            return 0;
        }
    }
    
    // 7. Create render pass
    VkAttachmentDescription colorAttachment = {};
    colorAttachment.format = g_swapchainImageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    
    VkAttachmentDescription depthAttachment = {};
    depthAttachment.format = VK_FORMAT_D32_SFLOAT;
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    
    VkAttachmentReference colorAttachmentRef = {};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    
    VkAttachmentReference depthAttachmentRef = {};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    
    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;
    
    VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    
    std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};
    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;
    
    if (vkCreateRenderPass(g_device, &renderPassInfo, nullptr, &g_renderPass) != VK_SUCCESS) {
        std::cerr << "[EDEN] ERROR: Failed to create render pass!" << std::endl;
        return 0;
    }
    
    // 8. Create descriptor set layout
    VkDescriptorSetLayoutBinding uboLayoutBinding = {};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    
    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &uboLayoutBinding;
    
    if (vkCreateDescriptorSetLayout(g_device, &layoutInfo, nullptr, &g_descriptorSetLayout) != VK_SUCCESS) {
        std::cerr << "[EDEN] ERROR: Failed to create descriptor set layout!" << std::endl;
        return 0;
    }
    
    // 9. Create graphics pipeline
    // Try to load custom shaders first (if they exist), otherwise use default shaders
    std::vector<char> vertShaderCode, fragShaderCode;
    bool loadedCustomShaders = false;
    
    // Try loading custom shaders first
    std::vector<std::string> vertPaths = {
        "shaders/my_shader.vert.spv",
        "my_shader.vert.spv"
    };
    std::vector<std::string> fragPaths = {
        "shaders/my_shader.frag.spv",
        "my_shader.frag.spv"
    };
    
    // Try to load custom vertex shader
    for (const auto& path : vertPaths) {
        try {
            vertShaderCode = readFile(path);
            loadedCustomShaders = true;
            g_usingCustomShaders = true;  // Mark that we're using custom shaders
            std::cout << "[EDEN] Loaded custom vertex shader: " << path << std::endl;
            break;
        } catch (...) {
            // Try next path
        }
    }
    
    // Try to load custom fragment shader
    for (const auto& path : fragPaths) {
        try {
            fragShaderCode = readFile(path);
            if (loadedCustomShaders) {
                std::cout << "[EDEN] Loaded custom fragment shader: " << path << std::endl;
                break;
            }
        } catch (...) {
            // Try next path
        }
    }
    
    // Fall back to default shaders if custom ones weren't found
    if (!loadedCustomShaders || fragShaderCode.empty()) {
        try {
            vertShaderCode = readFile("vert_3d.spv");
            fragShaderCode = readFile("frag_3d.spv");
            g_usingCustomShaders = false;  // Using default shaders
            std::cout << "[EDEN] Using default shaders (vert_3d.spv, frag_3d.spv)" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "[EDEN] ERROR: Could not load shader files!" << std::endl;
            std::cerr << "[EDEN] Tried custom and default shaders, error: " << e.what() << std::endl;
            return 0;
        }
    }
    
    VkShaderModuleCreateInfo vertCreateInfo = {};
    vertCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    vertCreateInfo.codeSize = vertShaderCode.size();
    vertCreateInfo.pCode = reinterpret_cast<const uint32_t*>(vertShaderCode.data());
    
    if (vkCreateShaderModule(g_device, &vertCreateInfo, nullptr, &g_vertShaderModule) != VK_SUCCESS) {
        std::cerr << "[EDEN] ERROR: Failed to create vertex shader module!" << std::endl;
        return 0;
    }
    
    VkShaderModuleCreateInfo fragCreateInfo = {};
    fragCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    fragCreateInfo.codeSize = fragShaderCode.size();
    fragCreateInfo.pCode = reinterpret_cast<const uint32_t*>(fragShaderCode.data());
    
    if (vkCreateShaderModule(g_device, &fragCreateInfo, nullptr, &g_fragShaderModule) != VK_SUCCESS) {
        std::cerr << "[EDEN] ERROR: Failed to create fragment shader module!" << std::endl;
        vkDestroyShaderModule(g_device, g_vertShaderModule, nullptr);
        return 0;
    }
    
    VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = g_vertShaderModule;
    vertShaderStageInfo.pName = "main";
    
    VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = g_fragShaderModule;
    fragShaderStageInfo.pName = "main";
    
    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};
    
    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    
    // Set up vertex input if using custom shaders
    VkVertexInputBindingDescription bindingDescription = {};
    VkVertexInputAttributeDescription attributeDescriptions[2] = {};
    
    if (g_usingCustomShaders) {
        // Set up vertex input for custom shaders
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(Vertex, pos);
        
        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Vertex, color);
        
        vertexInputInfo.vertexBindingDescriptionCount = 1;
        vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
        vertexInputInfo.vertexAttributeDescriptionCount = 2;
        vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions;
        
        // Create vertex buffer for triangle
        std::vector<Vertex> triangleVertices = {
            {{ 0.0f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}},  // Bottom (red)
            {{ 0.5f,  0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}},  // Top-right (green)
            {{-0.5f,  0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}}   // Top-left (blue)
        };
        
        VkDeviceSize bufferSize = sizeof(Vertex) * triangleVertices.size();
        createBuffer(bufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                   g_triangleVertexBuffer, g_triangleVertexBufferMemory);
        
        // Copy vertex data to buffer
        void* data;
        vkMapMemory(g_device, g_triangleVertexBufferMemory, 0, bufferSize, 0, &data);
        memcpy(data, triangleVertices.data(), (size_t)bufferSize);
        vkUnmapMemory(g_device, g_triangleVertexBufferMemory);
        
        std::cout << "[EDEN] Created vertex buffer for custom shaders" << std::endl;
    } else {
        // Default shaders use hardcoded vertices (no vertex input)
        vertexInputInfo.vertexBindingDescriptionCount = 0;
        vertexInputInfo.vertexAttributeDescriptionCount = 0;
    }
    
    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;
    
    VkViewport viewport = {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)swapchainExtent.width;
    viewport.height = (float)swapchainExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    
    VkRect2D scissor = {};
    scissor.offset = {0, 0};
    scissor.extent = swapchainExtent;
    
    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;
    
    VkPipelineRasterizationStateCreateInfo rasterizer = {};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;
    
    VkPipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    
    VkPipelineDepthStencilStateCreateInfo depthStencil = {};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_FALSE;
    depthStencil.depthWriteEnable = VK_FALSE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_ALWAYS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;
    
    VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;
    
    VkPipelineColorBlendStateCreateInfo colorBlending = {};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    
    // Push constant for per-ball model matrix (so each draw has unique transform)
    VkPushConstantRange pushConstantRange = {};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(Mat4);
    
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &g_descriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
    
    if (vkCreatePipelineLayout(g_device, &pipelineLayoutInfo, nullptr, &g_pipelineLayout) != VK_SUCCESS) {
        std::cerr << "[EDEN] ERROR: Failed to create pipeline layout!" << std::endl;
        vkDestroyShaderModule(g_device, g_fragShaderModule, nullptr);
        vkDestroyShaderModule(g_device, g_vertShaderModule, nullptr);
        return 0;
    }
    
    VkGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.layout = g_pipelineLayout;
    pipelineInfo.renderPass = g_renderPass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    
    if (vkCreateGraphicsPipelines(g_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &g_pipeline) != VK_SUCCESS) {
        std::cerr << "[EDEN] ERROR: Failed to create graphics pipeline!" << std::endl;
        vkDestroyPipelineLayout(g_device, g_pipelineLayout, nullptr);
        vkDestroyShaderModule(g_device, g_fragShaderModule, nullptr);
        vkDestroyShaderModule(g_device, g_vertShaderModule, nullptr);
        return 0;
    }
    
    // Note: We keep shader modules alive for hot-reload support
    // They will be destroyed in cleanup
    
    // 10. Create depth resources
    createDepthResources();
    
    // 11. Create framebuffers
    g_framebuffers.resize(g_swapchainImageCount);
    for (uint32_t i = 0; i < g_swapchainImageCount; i++) {
        std::array<VkImageView, 2> attachments = {
            g_swapchainImageViews[i],
            g_depthImageView
        };
        
        VkFramebufferCreateInfo framebufferInfo = {};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = g_renderPass;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = swapchainExtent.width;
        framebufferInfo.height = swapchainExtent.height;
        framebufferInfo.layers = 1;
        
        if (vkCreateFramebuffer(g_device, &framebufferInfo, nullptr, &g_framebuffers[i]) != VK_SUCCESS) {
            std::cerr << "[EDEN] ERROR: Failed to create framebuffer!" << std::endl;
            return 0;
        }
    }
    
    // 12. Create command pool
    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = g_graphicsQueueFamilyIndex;
    
    if (vkCreateCommandPool(g_device, &poolInfo, nullptr, &g_commandPool) != VK_SUCCESS) {
        std::cerr << "[EDEN] ERROR: Failed to create command pool!" << std::endl;
        return 0;
    }
    
    // 13. Create command buffers
    g_commandBuffers.resize(g_swapchainImageCount);
    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = g_commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(g_commandBuffers.size());
    
    if (vkAllocateCommandBuffers(g_device, &allocInfo, g_commandBuffers.data()) != VK_SUCCESS) {
        std::cerr << "[EDEN] ERROR: Failed to allocate command buffers!" << std::endl;
        return 0;
    }
    
    // 14. Create uniform buffers
    VkDeviceSize bufferSize = sizeof(UniformBufferObject);
    g_uniformBuffers.resize(g_swapchainImageCount);
    g_uniformBuffersMemory.resize(g_swapchainImageCount);
    
    for (size_t i = 0; i < g_swapchainImageCount; i++) {
        createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                    g_uniformBuffers[i], g_uniformBuffersMemory[i]);
    }
    
    // 15. Create descriptor pool
    std::array<VkDescriptorPoolSize, 1> poolSizes = {};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = static_cast<uint32_t>(g_swapchainImageCount);
    
    VkDescriptorPoolCreateInfo poolCreateInfo = {};
    poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolCreateInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolCreateInfo.pPoolSizes = poolSizes.data();
    poolCreateInfo.maxSets = static_cast<uint32_t>(g_swapchainImageCount);
    
    if (vkCreateDescriptorPool(g_device, &poolCreateInfo, nullptr, &g_descriptorPool) != VK_SUCCESS) {
        std::cerr << "[EDEN] ERROR: Failed to create descriptor pool!" << std::endl;
        return 0;
    }
    
    // 16. Create descriptor sets
    std::vector<VkDescriptorSetLayout> layouts(g_swapchainImageCount, g_descriptorSetLayout);
    VkDescriptorSetAllocateInfo allocInfo2 = {};
    allocInfo2.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo2.descriptorPool = g_descriptorPool;
    allocInfo2.descriptorSetCount = static_cast<uint32_t>(g_swapchainImageCount);
    allocInfo2.pSetLayouts = layouts.data();
    
    g_descriptorSets.resize(g_swapchainImageCount);
    if (vkAllocateDescriptorSets(g_device, &allocInfo2, g_descriptorSets.data()) != VK_SUCCESS) {
        std::cerr << "[EDEN] ERROR: Failed to allocate descriptor sets!" << std::endl;
        return 0;
    }
    
    // Update descriptor sets
    for (size_t i = 0; i < g_swapchainImageCount; i++) {
        VkDescriptorBufferInfo bufferInfo = {};
        bufferInfo.buffer = g_uniformBuffers[i];
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(UniformBufferObject);
        
        VkWriteDescriptorSet descriptorWrite = {};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = g_descriptorSets[i];
        descriptorWrite.dstBinding = 0;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pBufferInfo = &bufferInfo;
        
        vkUpdateDescriptorSets(g_device, 1, &descriptorWrite, 0, nullptr);
    }
    
    // 17. Create synchronization objects
    VkSemaphoreCreateInfo semaphoreInfo = {};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    
    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    
    if (vkCreateSemaphore(g_device, &semaphoreInfo, nullptr, &g_imageAvailableSemaphore) != VK_SUCCESS ||
        vkCreateSemaphore(g_device, &semaphoreInfo, nullptr, &g_renderFinishedSemaphore) != VK_SUCCESS ||
        vkCreateFence(g_device, &fenceInfo, nullptr, &g_inFlightFence) != VK_SUCCESS) {
        std::cerr << "[EDEN] ERROR: Failed to create synchronization objects!" << std::endl;
        return 0;
    }
    
    // 18. Note: Triangle vertices are hardcoded in the shader (vert_3d.glsl)
    // No vertex buffer needed for this simple case
    
    std::cout << "[EDEN] Vulkan renderer initialized successfully!" << std::endl;
    return 1;
}

// Render a frame
extern "C" void heidic_render_frame(GLFWwindow* window) {
    if (g_device == VK_NULL_HANDLE || g_swapchain == VK_NULL_HANDLE) {
        return;
    }
    
    // Update rotation angle (spinning triangle)
    auto currentTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(currentTime - g_lastTime);
    float deltaTime = duration.count() / 1000000.0f;
    g_lastTime = currentTime;
    
    g_rotationAngle += g_rotationSpeed * deltaTime; // Speed controlled by hot-reload
    if (g_rotationAngle > 2.0f * 3.14159f) {
        g_rotationAngle -= 2.0f * 3.14159f;
    }
    
    // Wait for previous frame to finish
    vkWaitForFences(g_device, 1, &g_inFlightFence, VK_TRUE, UINT64_MAX);
    vkResetFences(g_device, 1, &g_inFlightFence);
    
    // Acquire next image
    uint32_t imageIndex;
    vkAcquireNextImageKHR(g_device, g_swapchain, UINT64_MAX, g_imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
    
    // Update uniform buffer with rotation using GLM
    UniformBufferObject ubo = {};
    
    // Model matrix - rotate around Z axis using GLM
    glm::mat4 modelMat = glm::rotate(glm::mat4(1.0f), g_rotationAngle, glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.model = Mat4(modelMat);
    
    // View matrix - look at triangle from above
    Vec3 eye = {0.0f, 0.0f, 2.0f};
    Vec3 center = {0.0f, 0.0f, 0.0f};
    Vec3 up = {0.0f, 1.0f, 0.0f};
    ubo.view = mat4_lookat(eye, center, up);
    
    // Projection matrix
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    float aspect = (float)width / (float)height;
    ubo.proj = mat4_perspective(1.0472f, aspect, 0.1f, 100.0f); // 60 degree FOV
    
    // Vulkan clip space has inverted Y and half Z.
    // GLM is designed for OpenGL (Y up, -1 to 1 Z).
    // We need to flip Y in the projection matrix.
    ubo.proj[1][1] *= -1;
    
    // Update uniform buffer
    void* data;
    vkMapMemory(g_device, g_uniformBuffersMemory[imageIndex], 0, sizeof(UniformBufferObject), 0, &data);
    memcpy(data, &ubo, sizeof(UniformBufferObject));
    vkUnmapMemory(g_device, g_uniformBuffersMemory[imageIndex]);
    
    // Record command buffer
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    
    if (vkBeginCommandBuffer(g_commandBuffers[imageIndex], &beginInfo) != VK_SUCCESS) {
        return;
    }
    
    VkRenderPassBeginInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = g_renderPass;
    renderPassInfo.framebuffer = g_framebuffers[imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = g_swapchainExtent;
    
    std::array<VkClearValue, 2> clearValues = {};
    clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}}; // Black background
    clearValues[1].depthStencil = {1.0f, 0};
    
    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();
    
    vkCmdBeginRenderPass(g_commandBuffers[imageIndex], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    
    vkCmdBindPipeline(g_commandBuffers[imageIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, g_pipeline);
    
    // Bind descriptor set (contains model/view/proj matrices)
    vkCmdBindDescriptorSets(g_commandBuffers[imageIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, g_pipelineLayout, 0, 1, &g_descriptorSets[imageIndex], 0, nullptr);
    
    // Draw triangle
    if (g_usingCustomShaders && g_triangleVertexBuffer != VK_NULL_HANDLE) {
        // Custom shaders use vertex buffers
        VkBuffer vertexBuffers[] = {g_triangleVertexBuffer};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(g_commandBuffers[imageIndex], 0, 1, vertexBuffers, offsets);
        vkCmdDraw(g_commandBuffers[imageIndex], 3, 1, 0, 0);
    } else {
        // Default shaders use hardcoded vertices (3 vertices - hardcoded in shader)
        vkCmdDraw(g_commandBuffers[imageIndex], 3, 1, 0, 0);
    }
    
    vkCmdEndRenderPass(g_commandBuffers[imageIndex]);
    
    if (vkEndCommandBuffer(g_commandBuffers[imageIndex]) != VK_SUCCESS) {
        return;
    }
    
    // Submit command buffer
    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    
    VkSemaphore waitSemaphores[] = {g_imageAvailableSemaphore};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &g_commandBuffers[imageIndex];
    
    VkSemaphore signalSemaphores[] = {g_renderFinishedSemaphore};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;
    
    if (vkQueueSubmit(g_graphicsQueue, 1, &submitInfo, g_inFlightFence) != VK_SUCCESS) {
        return;
    }
    
    // Present
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;
    
    VkSwapchainKHR swapChains[] = {g_swapchain};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;
    presentInfo.pResults = nullptr;
    
    vkQueuePresentKHR(g_graphicsQueue, &presentInfo);
    
    g_currentFrame = (g_currentFrame + 1) % g_swapchainImageCount;
}

// Cleanup renderer
extern "C" void heidic_cleanup_renderer() {
    if (g_device == VK_NULL_HANDLE) {
        return;
    }
    
    vkDeviceWaitIdle(g_device);
    
    // Cleanup uniform buffers
    for (size_t i = 0; i < g_uniformBuffers.size(); i++) {
        vkDestroyBuffer(g_device, g_uniformBuffers[i], nullptr);
        vkFreeMemory(g_device, g_uniformBuffersMemory[i], nullptr);
    }
    
    // Cleanup descriptor pool
    if (g_descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(g_device, g_descriptorPool, nullptr);
    }
    
    // Cleanup descriptor set layout
    if (g_descriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(g_device, g_descriptorSetLayout, nullptr);
    }
    
    // Cleanup synchronization objects
    if (g_imageAvailableSemaphore != VK_NULL_HANDLE) {
        vkDestroySemaphore(g_device, g_imageAvailableSemaphore, nullptr);
    }
    if (g_renderFinishedSemaphore != VK_NULL_HANDLE) {
        vkDestroySemaphore(g_device, g_renderFinishedSemaphore, nullptr);
    }
    if (g_inFlightFence != VK_NULL_HANDLE) {
        vkDestroyFence(g_device, g_inFlightFence, nullptr);
    }
    
    // Cleanup triangle vertex buffer (for custom shaders)
    if (g_triangleVertexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(g_device, g_triangleVertexBuffer, nullptr);
        g_triangleVertexBuffer = VK_NULL_HANDLE;
    }
    if (g_triangleVertexBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(g_device, g_triangleVertexBufferMemory, nullptr);
        g_triangleVertexBufferMemory = VK_NULL_HANDLE;
    }
    
    // Cleanup command pool
    if (g_commandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(g_device, g_commandPool, nullptr);
    }
    
    // Cleanup framebuffers
    for (auto framebuffer : g_framebuffers) {
        vkDestroyFramebuffer(g_device, framebuffer, nullptr);
    }
    
    // Cleanup pipeline
    if (g_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(g_device, g_pipeline, nullptr);
    }
    
    // Cleanup shader modules
    if (g_vertShaderModule != VK_NULL_HANDLE) {
        vkDestroyShaderModule(g_device, g_vertShaderModule, nullptr);
    }
    if (g_fragShaderModule != VK_NULL_HANDLE) {
        vkDestroyShaderModule(g_device, g_fragShaderModule, nullptr);
    }
    if (g_cubeVertShaderModule != VK_NULL_HANDLE) {
        vkDestroyShaderModule(g_device, g_cubeVertShaderModule, nullptr);
    }
    if (g_cubeFragShaderModule != VK_NULL_HANDLE) {
        vkDestroyShaderModule(g_device, g_cubeFragShaderModule, nullptr);
    }
    
    // Cleanup pipeline layout
    if (g_pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(g_device, g_pipelineLayout, nullptr);
    }
    
    // Cleanup render pass
    if (g_renderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(g_device, g_renderPass, nullptr);
    }
    
    // Cleanup depth resources
    if (g_depthImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(g_device, g_depthImageView, nullptr);
    }
    if (g_depthImage != VK_NULL_HANDLE) {
        vkDestroyImage(g_device, g_depthImage, nullptr);
    }
    if (g_depthImageMemory != VK_NULL_HANDLE) {
        vkFreeMemory(g_device, g_depthImageMemory, nullptr);
    }
    
    // Cleanup swapchain image views
    for (auto imageView : g_swapchainImageViews) {
        vkDestroyImageView(g_device, imageView, nullptr);
    }
    
    // Cleanup swapchain
    if (g_swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(g_device, g_swapchain, nullptr);
    }
    
    // Cleanup device
    if (g_device != VK_NULL_HANDLE) {
        vkDestroyDevice(g_device, nullptr);
    }
    
    // Cleanup surface
    if (g_surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(g_instance, g_surface, nullptr);
    }
    
    // Cleanup instance
    if (g_instance != VK_NULL_HANDLE) {
        vkDestroyInstance(g_instance, nullptr);
    }
    
    std::cout << "[EDEN] Renderer cleaned up" << std::endl;
}

// Sleep for milliseconds
extern "C" void heidic_sleep_ms(uint32_t milliseconds) {
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

// Hot-reload shader function
extern "C" void heidic_reload_shader(const char* shader_path) {
    if (g_device == VK_NULL_HANDLE) {
        std::cerr << "[Shader Hot-Reload] ERROR: Device not initialized!" << std::endl;
        return;
    }
    
    // Wait for device to be idle before reloading
    vkDeviceWaitIdle(g_device);
    
    std::string shaderPathStr(shader_path);
    
    // Determine the .spv path from the source path (shader_path is now the original source)
    // Helper lambda for C++17-compatible ends_with check
    auto endsWith = [](const std::string& str, const std::string& suffix) -> bool {
        if (suffix.length() > str.length()) return false;
        return str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0;
    };
    
    // Compile to .vert.spv, .frag.spv, etc. to avoid conflicts
    std::string spv_path = shaderPathStr;
    if (endsWith(spv_path, ".vert")) {
        spv_path = spv_path + ".spv";  // my_shader.vert.spv
    } else if (endsWith(spv_path, ".frag")) {
        spv_path = spv_path + ".spv";  // my_shader.frag.spv
    } else if (endsWith(spv_path, ".glsl")) {
        spv_path = spv_path.substr(0, spv_path.length() - 5) + ".spv";
    } else if (endsWith(spv_path, ".comp")) {
        spv_path = spv_path + ".spv";  // my_shader.comp.spv
    } else if (!endsWith(spv_path, ".spv")) {
        spv_path = spv_path + ".spv";
    }
    
    // Extract just the filename for matching
    std::string filename = shaderPathStr;
    size_t lastSlash = shaderPathStr.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        filename = shaderPathStr.substr(lastSlash + 1);
    }
    
    // Check for various shader naming patterns
    bool isTriangleShader = (shaderPathStr.find("vert_3d") != std::string::npos || 
                             shaderPathStr.find("frag_3d") != std::string::npos ||
                             shaderPathStr.find("my_shader") != std::string::npos ||
                             filename.find("my_shader") != std::string::npos ||
                             shaderPathStr.find("/vert") != std::string::npos ||
                             shaderPathStr.find("\\vert") != std::string::npos);
    bool isCubeShader = (shaderPathStr.find("vert_cube") != std::string::npos || 
                         shaderPathStr.find("frag_cube") != std::string::npos);
    
    // Default to triangle pipeline if not cube shader
    if (!isTriangleShader && !isCubeShader) {
        // Try to infer from path - if it's a .vert or .frag (and not cube), assume triangle pipeline
        if (shaderPathStr.find(".vert") != std::string::npos || 
            shaderPathStr.find(".frag") != std::string::npos) {
            isTriangleShader = true;
        } else {
            std::cerr << "[Shader Hot-Reload] WARNING: Unknown shader path: " << shader_path << std::endl;
            return;
        }
    }
    
    // Determine which shader stage we're reloading from the source file extension
    bool isVertex = (shaderPathStr.find(".vert") != std::string::npos || filename.find(".vert") != std::string::npos);
    bool isFragment = (shaderPathStr.find(".frag") != std::string::npos || filename.find(".frag") != std::string::npos);
    
    // Detect if this is a custom shader (my_shader pattern) that needs vertex input
    bool isCustomShader = (shaderPathStr.find("my_shader") != std::string::npos || filename.find("my_shader") != std::string::npos);
    if (isCustomShader) {
        g_usingCustomShaders = true;
    }
    
    VkShaderModule* targetModule = nullptr;
    VkPipeline* targetPipeline = nullptr;
    
    if (isTriangleShader) {
        if (isVertex) {
            targetModule = &g_vertShaderModule;
        } else if (isFragment) {
            targetModule = &g_fragShaderModule;
        }
        targetPipeline = &g_pipeline;
    } else if (isCubeShader) {
        if (isVertex) {
            targetModule = &g_cubeVertShaderModule;
        } else if (isFragment) {
            targetModule = &g_cubeFragShaderModule;
        }
        targetPipeline = &g_cubePipeline;
    }
    
    if (targetModule == nullptr || targetPipeline == nullptr) {
        std::cerr << "[Shader Hot-Reload] ERROR: Could not determine shader module!" << std::endl;
        return;
    }
    
    // Read new shader code from the .spv file
    std::vector<char> shaderCode;
    try {
        shaderCode = readFile(spv_path);
    } catch (const std::exception& e) {
        std::cerr << "[Shader Hot-Reload] ERROR: Failed to read shader file " << spv_path << ": " << e.what() << std::endl;
        return;
    }
    
    // Destroy old shader module
    if (*targetModule != VK_NULL_HANDLE) {
        vkDestroyShaderModule(g_device, *targetModule, nullptr);
        *targetModule = VK_NULL_HANDLE;
    }
    
    // Create new shader module
    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = shaderCode.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(shaderCode.data());
    
    if (vkCreateShaderModule(g_device, &createInfo, nullptr, targetModule) != VK_SUCCESS) {
        std::cerr << "[Shader Hot-Reload] ERROR: Failed to create new shader module!" << std::endl;
        return;
    }
    
    // Destroy old pipeline
    if (*targetPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(g_device, *targetPipeline, nullptr);
        *targetPipeline = VK_NULL_HANDLE;
    }
    
    // Get the other shader module (vertex or fragment)
    VkShaderModule otherModule = VK_NULL_HANDLE;
    if (isTriangleShader) {
        if (isVertex) {
            otherModule = g_fragShaderModule;
        } else {
            otherModule = g_vertShaderModule;
        }
    } else if (isCubeShader) {
        if (isVertex) {
            otherModule = g_cubeFragShaderModule;
        } else {
            otherModule = g_cubeVertShaderModule;
        }
    }
    
    if (otherModule == VK_NULL_HANDLE) {
        std::cerr << "[Shader Hot-Reload] ERROR: Other shader module not found!" << std::endl;
        return;
    }
    
    // Create shader stage info for both shaders
    VkPipelineShaderStageCreateInfo vertStageInfo = {};
    vertStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertStageInfo.module = isVertex ? *targetModule : otherModule;
    vertStageInfo.pName = "main";
    
    VkPipelineShaderStageCreateInfo fragStageInfo = {};
    fragStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStageInfo.module = isFragment ? *targetModule : otherModule;
    fragStageInfo.pName = "main";
    
    VkPipelineShaderStageCreateInfo shaderStages[] = {vertStageInfo, fragStageInfo};
    
    // Recreate pipeline (triangle or cube)
    if (isTriangleShader) {
        // Triangle pipeline setup
        // Custom shaders (my_shader) need vertex input, default shaders don't
        VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        
        VkVertexInputBindingDescription bindingDescription = {};
        VkVertexInputAttributeDescription attributeDescriptions[2] = {};
        
        if (g_usingCustomShaders) {
            // Set up vertex input for custom shaders
            bindingDescription.binding = 0;
            bindingDescription.stride = sizeof(Vertex);
            bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
            
            attributeDescriptions[0].binding = 0;
            attributeDescriptions[0].location = 0;
            attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
            attributeDescriptions[0].offset = offsetof(Vertex, pos);
            
            attributeDescriptions[1].binding = 0;
            attributeDescriptions[1].location = 1;
            attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
            attributeDescriptions[1].offset = offsetof(Vertex, color);
            
            vertexInputInfo.vertexBindingDescriptionCount = 1;
            vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
            vertexInputInfo.vertexAttributeDescriptionCount = 2;
            vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions;
            
            // Create vertex buffer for triangle if it doesn't exist
            if (g_triangleVertexBuffer == VK_NULL_HANDLE) {
                std::vector<Vertex> triangleVertices = {
                    {{ 0.0f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}},  // Bottom (red)
                    {{ 0.5f,  0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}},  // Top-right (green)
                    {{-0.5f,  0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}}   // Top-left (blue)
                };
                
                VkDeviceSize bufferSize = sizeof(Vertex) * triangleVertices.size();
                createBuffer(bufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                           g_triangleVertexBuffer, g_triangleVertexBufferMemory);
                
                // Copy vertex data to buffer
                void* data;
                vkMapMemory(g_device, g_triangleVertexBufferMemory, 0, bufferSize, 0, &data);
                memcpy(data, triangleVertices.data(), (size_t)bufferSize);
                vkUnmapMemory(g_device, g_triangleVertexBufferMemory);
                
                std::cout << "[Shader Hot-Reload] Created vertex buffer for custom shaders" << std::endl;
            }
        } else {
            // Default shaders use hardcoded vertices (no vertex input)
            vertexInputInfo.vertexBindingDescriptionCount = 0;
            vertexInputInfo.vertexAttributeDescriptionCount = 0;
        }
        
        VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;
        
        VkViewport viewport = {};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float)g_swapchainExtent.width;
        viewport.height = (float)g_swapchainExtent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        
        VkRect2D scissor = {};
        scissor.offset = {0, 0};
        scissor.extent = g_swapchainExtent;
        
        VkPipelineViewportStateCreateInfo viewportState = {};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.pViewports = &viewport;
        viewportState.scissorCount = 1;
        viewportState.pScissors = &scissor;
        
        VkPipelineRasterizationStateCreateInfo rasterizer = {};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_NONE;
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_FALSE;
        
        VkPipelineMultisampleStateCreateInfo multisampling = {};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        
        VkPipelineDepthStencilStateCreateInfo depthStencil = {};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = VK_FALSE;
        depthStencil.depthWriteEnable = VK_FALSE;
        depthStencil.depthCompareOp = VK_COMPARE_OP_ALWAYS;
        depthStencil.depthBoundsTestEnable = VK_FALSE;
        depthStencil.stencilTestEnable = VK_FALSE;
        
        VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE;
        
        VkPipelineColorBlendStateCreateInfo colorBlending = {};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;
        
        VkGraphicsPipelineCreateInfo pipelineInfo = {};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.layout = g_pipelineLayout;
        pipelineInfo.renderPass = g_renderPass;
        pipelineInfo.subpass = 0;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
        
        if (vkCreateGraphicsPipelines(g_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &g_pipeline) != VK_SUCCESS) {
            std::cerr << "[Shader Hot-Reload] ERROR: Failed to recreate triangle pipeline!" << std::endl;
            return;
        }
    } else if (isCubeShader) {
        // Cube pipeline setup (with vertex input)
        VkVertexInputBindingDescription bindingDescription = {};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        
        VkVertexInputAttributeDescription attributeDescriptions[2] = {};
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(Vertex, pos);
        
        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Vertex, color);
        
        VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 1;
        vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
        vertexInputInfo.vertexAttributeDescriptionCount = 2;
        vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions;
        
        VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;
        
        VkViewport viewport = {};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float)g_swapchainExtent.width;
        viewport.height = (float)g_swapchainExtent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        
        VkRect2D scissor = {};
        scissor.offset = {0, 0};
        scissor.extent = g_swapchainExtent;
        
        VkPipelineViewportStateCreateInfo viewportState = {};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.pViewports = &viewport;
        viewportState.scissorCount = 1;
        viewportState.pScissors = &scissor;
        
        VkPipelineRasterizationStateCreateInfo rasterizer = {};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_FALSE;
        
        VkPipelineMultisampleStateCreateInfo multisampling = {};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        
        VkPipelineDepthStencilStateCreateInfo depthStencil = {};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = VK_TRUE;
        depthStencil.depthWriteEnable = VK_TRUE;
        depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
        depthStencil.depthBoundsTestEnable = VK_FALSE;
        depthStencil.stencilTestEnable = VK_FALSE;
        
        VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE;
        
        VkPipelineColorBlendStateCreateInfo colorBlending = {};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;
        
        VkGraphicsPipelineCreateInfo pipelineInfo = {};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.layout = g_pipelineLayout;
        pipelineInfo.renderPass = g_renderPass;
        pipelineInfo.subpass = 0;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
        
        if (vkCreateGraphicsPipelines(g_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &g_cubePipeline) != VK_SUCCESS) {
            std::cerr << "[Shader Hot-Reload] ERROR: Failed to recreate cube pipeline!" << std::endl;
            return;
        }
    }
    
    // Recreate command buffers (they reference the pipeline)
    // Free old command buffers
    if (g_commandPool != VK_NULL_HANDLE) {
        vkFreeCommandBuffers(g_device, g_commandPool, static_cast<uint32_t>(g_commandBuffers.size()), g_commandBuffers.data());
    }
    
    // Reallocate command buffers
    g_commandBuffers.resize(g_swapchainImageCount);
    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = g_commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(g_commandBuffers.size());
    
    if (vkAllocateCommandBuffers(g_device, &allocInfo, g_commandBuffers.data()) != VK_SUCCESS) {
        std::cerr << "[Shader Hot-Reload] ERROR: Failed to reallocate command buffers!" << std::endl;
        return;
    }
    
    std::cout << "[Shader Hot-Reload] Successfully reloaded shader: " << shader_path << std::endl;
}

// ============================================================================
// CUBE RENDERING FUNCTIONS
// ============================================================================

// Cube-specific state
static VkBuffer g_cubeVertexBuffer = VK_NULL_HANDLE;
static VkDeviceMemory g_cubeVertexBufferMemory = VK_NULL_HANDLE;
static VkBuffer g_cubeIndexBuffer = VK_NULL_HANDLE;
static VkDeviceMemory g_cubeIndexBufferMemory = VK_NULL_HANDLE;
// g_cubePipeline is already declared at the top of the file (line 43)
static uint32_t g_cubeIndexCount = 0;
static float g_cubeRotationAngle = 0.0f;
static std::chrono::high_resolution_clock::time_point g_cubeLastTime = std::chrono::high_resolution_clock::now();

// Cube vertices (8 vertices with position and color)
static const std::vector<Vertex> cubeVertices = {
    {{-1.0f, -1.0f,  1.0f}, {1.0f, 0.0f, 0.0f}},  // Front bottom-left (red)
    {{ 1.0f, -1.0f,  1.0f}, {0.0f, 1.0f, 0.0f}},  // Front bottom-right (green)
    {{ 1.0f,  1.0f,  1.0f}, {0.0f, 0.0f, 1.0f}},  // Front top-right (blue)
    {{-1.0f,  1.0f,  1.0f}, {1.0f, 1.0f, 0.0f}},  // Front top-left (yellow)
    {{-1.0f, -1.0f, -1.0f}, {1.0f, 0.0f, 1.0f}},  // Back bottom-left (magenta)
    {{ 1.0f, -1.0f, -1.0f}, {0.0f, 1.0f, 1.0f}},  // Back bottom-right (cyan)
    {{ 1.0f,  1.0f, -1.0f}, {1.0f, 0.5f, 0.0f}},  // Back top-right (orange)
    {{-1.0f,  1.0f, -1.0f}, {0.5f, 0.5f, 1.0f}},  // Back top-left (light blue)
};

// Cube indices (12 triangles = 6 faces * 2 triangles per face)
static const std::vector<uint16_t> cubeIndices = {
    0, 1, 2,  2, 3, 0,  // Front face
    4, 6, 5,  6, 4, 7,  // Back face
    3, 2, 6,  6, 7, 3,  // Top face
    0, 4, 5,  5, 1, 0,  // Bottom face
    1, 5, 6,  6, 2, 1,  // Right face
    0, 3, 7,  7, 4, 0,  // Left face
};

// Initialize cube renderer
extern "C" int heidic_init_renderer_cube(GLFWwindow* window) {
    // Reuse the base Vulkan initialization (instance, device, swapchain, etc.)
    // This will also create the triangle pipeline, but we'll create our own cube pipeline
    if (heidic_init_renderer(window) == 0) {
        return 0;
    }
    
    // Destroy the triangle pipeline since we'll create our own cube pipeline
    if (g_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(g_device, g_pipeline, nullptr);
        g_pipeline = VK_NULL_HANDLE;
    }
    
    // Note: heidic_init_renderer creates g_pipeline for triangle, but we'll create g_cubePipeline
    // Both can coexist, we just use g_cubePipeline for rendering
    
    // Now create vertex and index buffers for the cube
    // Create vertex buffer
    VkDeviceSize vertexBufferSize = sizeof(cubeVertices[0]) * cubeVertices.size();
    createBuffer(vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 g_cubeVertexBuffer, g_cubeVertexBufferMemory);
    
    // Copy vertex data to buffer
    void* data;
    vkMapMemory(g_device, g_cubeVertexBufferMemory, 0, vertexBufferSize, 0, &data);
    memcpy(data, cubeVertices.data(), (size_t)vertexBufferSize);
    vkUnmapMemory(g_device, g_cubeVertexBufferMemory);
    
    // Create index buffer
    VkDeviceSize indexBufferSize = sizeof(cubeIndices[0]) * cubeIndices.size();
    g_cubeIndexCount = static_cast<uint32_t>(cubeIndices.size());
    createBuffer(indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 g_cubeIndexBuffer, g_cubeIndexBufferMemory);
    
    // Copy index data to buffer
    vkMapMemory(g_device, g_cubeIndexBufferMemory, 0, indexBufferSize, 0, &data);
    memcpy(data, cubeIndices.data(), (size_t)indexBufferSize);
    vkUnmapMemory(g_device, g_cubeIndexBufferMemory);
    
    // Create a new pipeline for cube (with vertex input)
    // Load shaders - readFile already checks multiple paths including examples/spinning_cube/
    std::vector<char> vertShaderCode, fragShaderCode;
    try {
        vertShaderCode = readFile("vert_cube.spv");
        fragShaderCode = readFile("frag_cube.spv");
        std::cout << "[EDEN] Loaded cube shaders successfully" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[EDEN] ERROR: Could not find cube shader files (vert_cube.spv, frag_cube.spv)!" << std::endl;
        std::cerr << "[EDEN] Error: " << e.what() << std::endl;
        std::cerr << "[EDEN] Make sure shaders are compiled and in examples/spinning_cube/ or current directory" << std::endl;
        return 0;
    }
    
    VkShaderModuleCreateInfo vertCreateInfo = {};
    vertCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    vertCreateInfo.codeSize = vertShaderCode.size();
    vertCreateInfo.pCode = reinterpret_cast<const uint32_t*>(vertShaderCode.data());
    
    if (vkCreateShaderModule(g_device, &vertCreateInfo, nullptr, &g_cubeVertShaderModule) != VK_SUCCESS) {
        std::cerr << "[EDEN] ERROR: Failed to create vertex shader module!" << std::endl;
        return 0;
    }
    
    VkShaderModuleCreateInfo fragCreateInfo = {};
    fragCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    fragCreateInfo.codeSize = fragShaderCode.size();
    fragCreateInfo.pCode = reinterpret_cast<const uint32_t*>(fragShaderCode.data());
    
    if (vkCreateShaderModule(g_device, &fragCreateInfo, nullptr, &g_cubeFragShaderModule) != VK_SUCCESS) {
        std::cerr << "[EDEN] ERROR: Failed to create fragment shader module!" << std::endl;
        vkDestroyShaderModule(g_device, g_cubeVertShaderModule, nullptr);
        return 0;
    }
    
    VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = g_cubeVertShaderModule;
    vertShaderStageInfo.pName = "main";
    
    VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = g_cubeFragShaderModule;
    fragShaderStageInfo.pName = "main";
    
    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};
    
    // Vertex input description (for cube with vertex buffers)
    VkVertexInputBindingDescription bindingDescription = {};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(Vertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    
    VkVertexInputAttributeDescription attributeDescriptions[2] = {};
    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[0].offset = offsetof(Vertex, pos);
    
    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[1].offset = offsetof(Vertex, color);
    
    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = 2;
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions;
    
    // Input assembly (same as triangle)
    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;
    
    // Viewport and scissor (same as triangle)
    VkViewport viewport = {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)g_swapchainExtent.width;
    viewport.height = (float)g_swapchainExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    
    VkRect2D scissor = {};
    scissor.offset = {0, 0};
    scissor.extent = g_swapchainExtent;
    
    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;
    
    // Rasterization (same as triangle)
    VkPipelineRasterizationStateCreateInfo rasterizer = {};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;
    
    // Multisampling (same as triangle)
    VkPipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    
    // Depth stencil (same as triangle)
    VkPipelineDepthStencilStateCreateInfo depthStencil = {};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;
    
    // Color blending (same as triangle)
    VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;
    
    VkPipelineColorBlendStateCreateInfo colorBlending = {};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    
    // Pipeline layout (same as triangle - reuse existing)
    VkGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.layout = g_pipelineLayout;
    pipelineInfo.renderPass = g_renderPass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    
    if (vkCreateGraphicsPipelines(g_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &g_cubePipeline) != VK_SUCCESS) {
        std::cerr << "[EDEN] ERROR: Failed to create cube graphics pipeline!" << std::endl;
        vkDestroyShaderModule(g_device, g_cubeFragShaderModule, nullptr);
        vkDestroyShaderModule(g_device, g_cubeVertShaderModule, nullptr);
        return 0;
    }
    
    // Note: We keep shader modules alive for hot-reload support
    // They will be destroyed in cleanup
    
    std::cout << "[EDEN] Cube renderer initialized successfully!" << std::endl;
    return 1;
}

// Render cube frame
extern "C" void heidic_render_frame_cube(GLFWwindow* window) {
    if (g_device == VK_NULL_HANDLE || g_swapchain == VK_NULL_HANDLE || g_cubePipeline == VK_NULL_HANDLE) {
        return;
    }
    
    // Update rotation angle
    auto currentTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(currentTime - g_cubeLastTime);
    float deltaTime = duration.count() / 1000000.0f;
    g_cubeLastTime = currentTime;
    
    g_cubeRotationAngle += 1.0f * deltaTime;
    if (g_cubeRotationAngle > 2.0f * 3.14159f) {
        g_cubeRotationAngle -= 2.0f * 3.14159f;
    }
    
    // Wait for previous frame
    vkWaitForFences(g_device, 1, &g_inFlightFence, VK_TRUE, UINT64_MAX);
    vkResetFences(g_device, 1, &g_inFlightFence);
    
    // Acquire next image
    uint32_t imageIndex;
    vkAcquireNextImageKHR(g_device, g_swapchain, UINT64_MAX, g_imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
    
    // Reset command buffer
    vkResetCommandBuffer(g_commandBuffers[imageIndex], 0);
    
    // Begin command buffer
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(g_commandBuffers[imageIndex], &beginInfo);
    
    // Begin render pass
    VkRenderPassBeginInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = g_renderPass;
    renderPassInfo.framebuffer = g_framebuffers[imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = g_swapchainExtent;
    
    std::array<VkClearValue, 2> clearValues = {};
    clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};
    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();
    
    vkCmdBeginRenderPass(g_commandBuffers[imageIndex], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    
    // Bind pipeline
    vkCmdBindPipeline(g_commandBuffers[imageIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, g_cubePipeline);
    
    // Update uniform buffer
    UniformBufferObject ubo = {};
    
    // Model matrix - rotate the cube
    Vec3 axis = {1.0f, 1.0f, 0.0f};
    ubo.model = mat4_rotate(axis, g_cubeRotationAngle);
    
    // View matrix - look at cube from above and to the side
    Vec3 eye = {3.0f, 3.0f, 3.0f};
    Vec3 center = {0.0f, 0.0f, 0.0f};
    Vec3 up = {0.0f, 1.0f, 0.0f};
    ubo.view = mat4_lookat(eye, center, up);
    
    // Projection matrix
    float fov = 45.0f * 3.14159f / 180.0f;
    float aspect = (float)g_swapchainExtent.width / (float)g_swapchainExtent.height;
    float nearPlane = 0.1f;
    float farPlane = 100.0f;
    ubo.proj = mat4_perspective(fov, aspect, nearPlane, farPlane);
    
    // Vulkan clip space has inverted Y and half Z
    ubo.proj[1][1] *= -1.0f;
    
    // Update uniform buffer
    void* data;
    vkMapMemory(g_device, g_uniformBuffersMemory[imageIndex], 0, sizeof(ubo), 0, &data);
    memcpy(data, &ubo, sizeof(ubo));
    vkUnmapMemory(g_device, g_uniformBuffersMemory[imageIndex]);
    
    // Bind descriptor set
    vkCmdBindDescriptorSets(g_commandBuffers[imageIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, g_pipelineLayout, 0, 1, &g_descriptorSets[imageIndex], 0, nullptr);
    
    // Bind vertex buffer
    VkBuffer vertexBuffers[] = {g_cubeVertexBuffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(g_commandBuffers[imageIndex], 0, 1, vertexBuffers, offsets);
    
    // Bind index buffer
    vkCmdBindIndexBuffer(g_commandBuffers[imageIndex], g_cubeIndexBuffer, 0, VK_INDEX_TYPE_UINT16);
    
    // Draw cube using indexed drawing
    vkCmdDrawIndexed(g_commandBuffers[imageIndex], g_cubeIndexCount, 1, 0, 0, 0);
    
    // Render ImGui overlay (if initialized)
    #ifdef USE_IMGUI
    if (g_imguiInitialized) {
        heidic_imgui_render(g_commandBuffers[imageIndex]);
    }
    #endif
    
    vkCmdEndRenderPass(g_commandBuffers[imageIndex]);
    
    if (vkEndCommandBuffer(g_commandBuffers[imageIndex]) != VK_SUCCESS) {
        std::cerr << "[EDEN] ERROR: Failed to record command buffer!" << std::endl;
        return;
    }
    
    // Submit command buffer
    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    
    VkSemaphore waitSemaphores[] = {g_imageAvailableSemaphore};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &g_commandBuffers[imageIndex];
    
    VkSemaphore signalSemaphores[] = {g_renderFinishedSemaphore};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;
    
    if (vkQueueSubmit(g_graphicsQueue, 1, &submitInfo, g_inFlightFence) != VK_SUCCESS) {
        std::cerr << "[EDEN] ERROR: Failed to submit draw command buffer!" << std::endl;
        return;
    }
    
    // Present
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;
    
    VkSwapchainKHR swapChains[] = {g_swapchain};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;
    presentInfo.pResults = nullptr;
    
    vkQueuePresentKHR(g_graphicsQueue, &presentInfo);
}

// Cleanup cube renderer
extern "C" void heidic_cleanup_renderer_cube() {
    // Cleanup ImGui first
    #ifdef USE_IMGUI
    heidic_cleanup_imgui();
    #endif
    
    if (g_device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(g_device);
        
        if (g_cubeIndexBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(g_device, g_cubeIndexBuffer, nullptr);
            g_cubeIndexBuffer = VK_NULL_HANDLE;
        }
        if (g_cubeIndexBufferMemory != VK_NULL_HANDLE) {
            vkFreeMemory(g_device, g_cubeIndexBufferMemory, nullptr);
            g_cubeIndexBufferMemory = VK_NULL_HANDLE;
        }
        if (g_cubeVertexBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(g_device, g_cubeVertexBuffer, nullptr);
            g_cubeVertexBuffer = VK_NULL_HANDLE;
        }
        if (g_cubeVertexBufferMemory != VK_NULL_HANDLE) {
            vkFreeMemory(g_device, g_cubeVertexBufferMemory, nullptr);
            g_cubeVertexBufferMemory = VK_NULL_HANDLE;
        }
        if (g_cubePipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(g_device, g_cubePipeline, nullptr);
            g_cubePipeline = VK_NULL_HANDLE;
        }
    }
    
    // Cleanup the base renderer (triangle cleanup)
    heidic_cleanup_renderer();
    
    std::cout << "[EDEN] Cube renderer cleaned up" << std::endl;
}

// ============================================================================
// IMGUI FUNCTIONS
// ============================================================================

#ifdef USE_IMGUI

// Initialize ImGui
extern "C" int heidic_init_imgui(GLFWwindow* window) {
    if (g_device == VK_NULL_HANDLE || g_renderPass == VK_NULL_HANDLE) {
        std::cerr << "[EDEN] ERROR: Vulkan must be initialized before ImGui!" << std::endl;
        return 0;
    }
    
    if (g_imguiInitialized) {
        std::cout << "[EDEN] ImGui already initialized" << std::endl;
        return 1;
    }
    
    std::cout << "[EDEN] Initializing ImGui..." << std::endl;
    
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    
    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    
    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForVulkan(window, true);
    
    // Create descriptor pool for ImGui
    VkDescriptorPoolSize pool_sizes[] = {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
    };
    
    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000;
    pool_info.poolSizeCount = sizeof(pool_sizes) / sizeof(pool_sizes[0]);
    pool_info.pPoolSizes = pool_sizes;
    
    if (vkCreateDescriptorPool(g_device, &pool_info, nullptr, &g_imguiDescriptorPool) != VK_SUCCESS) {
        std::cerr << "[EDEN] ERROR: Failed to create ImGui descriptor pool!" << std::endl;
        ImGui::DestroyContext();
        return 0;
    }
    
    // Initialize ImGui Vulkan backend
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = g_instance;
    init_info.PhysicalDevice = g_physicalDevice;
    init_info.Device = g_device;
    init_info.QueueFamily = g_graphicsQueueFamilyIndex;
    init_info.Queue = g_graphicsQueue;
    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.DescriptorPool = g_imguiDescriptorPool;
    init_info.RenderPass = g_renderPass;
    init_info.Subpass = 0;
    init_info.MinImageCount = g_swapchainImageCount;
    init_info.ImageCount = g_swapchainImageCount;
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.Allocator = nullptr;
    init_info.CheckVkResultFn = nullptr;
    
    ImGui_ImplVulkan_Init(&init_info);
    
    // Note: Fonts are automatically created on the first NewFrame() call
    // No need to manually upload fonts - the backend handles it
    
    g_imguiInitialized = true;
    std::cout << "[EDEN] ImGui initialized successfully!" << std::endl;
    return 1;
}

// Start new ImGui frame
extern "C" void heidic_imgui_new_frame() {
    if (!g_imguiInitialized) return;
    
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

// Render ImGui (call this after rendering your 3D scene, before ending render pass)
extern "C" void heidic_imgui_render(VkCommandBuffer commandBuffer) {
    if (!g_imguiInitialized) return;
    
    ImGui::Render();
    ImDrawData* draw_data = ImGui::GetDrawData();
    if (draw_data && draw_data->CmdListsCount > 0) {
        ImGui_ImplVulkan_RenderDrawData(draw_data, commandBuffer);
    }
}

// Helper to render a simple demo overlay
extern "C" void heidic_imgui_render_demo_overlay(float fps, float camera_x, float camera_y, float camera_z) {
    if (!g_imguiInitialized) return;
    
    // Create a simple overlay window
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 150), ImGuiCond_FirstUseEver);
    
    if (ImGui::Begin("EDEN Engine Info", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("FPS: %.1f", fps);
        ImGui::Separator();
        ImGui::Text("Camera Position:");
        ImGui::Text("  X: %.2f", camera_x);
        ImGui::Text("  Y: %.2f", camera_y);
        ImGui::Text("  Z: %.2f", camera_z);
        ImGui::Separator();
        ImGuiIO& io = ImGui::GetIO();
        ImGui::Text("Application average %.3f ms/frame", 1000.0f / io.Framerate);
    }
    ImGui::End();
}

// Cleanup ImGui
extern "C" void heidic_cleanup_imgui() {
    if (!g_imguiInitialized) return;
    
    vkDeviceWaitIdle(g_device);
    
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    
    if (g_imguiDescriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(g_device, g_imguiDescriptorPool, nullptr);
        g_imguiDescriptorPool = VK_NULL_HANDLE;
    }
    
    g_imguiInitialized = false;
    std::cout << "[EDEN] ImGui cleaned up" << std::endl;
}

#else

// Stub functions when ImGui is not available
extern "C" int heidic_init_imgui(GLFWwindow* window) {
    std::cerr << "[EDEN] WARNING: ImGui not compiled in (USE_IMGUI not defined)" << std::endl;
    return 0;
}

extern "C" void heidic_imgui_new_frame() {}
extern "C" void heidic_imgui_render(VkCommandBuffer commandBuffer) {}
extern "C" void heidic_cleanup_imgui() {}

#endif // USE_IMGUI

// ============================================================================
// Ball Rendering Functions (for bouncing_balls test case)
// ============================================================================

// Static variables for ball rendering
static VkPipeline g_ballsPipeline = VK_NULL_HANDLE;
static bool g_ballsInitialized = false;
static const int MAX_BALLS = 10;

// Note: Ball data is now managed in ECS (EntityStorage) in the generated C++ code

extern "C" int heidic_init_renderer_balls(GLFWwindow* window) {
    // Initialize base renderer (creates device, swapchain, etc.)
    if (heidic_init_renderer(window) == 0) {
        return 0;
    }
    
    // Destroy triangle pipeline since we'll use our own
    if (g_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(g_device, g_pipeline, nullptr);
        g_pipeline = VK_NULL_HANDLE;
    }
    
    // Reuse cube vertex/index buffers for balls (balls are rendered as small cubes)
    if (g_cubeVertexBuffer == VK_NULL_HANDLE) {
        // Create cube vertex buffer
        VkDeviceSize vertexBufferSize = sizeof(cubeVertices[0]) * cubeVertices.size();
        createBuffer(vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     g_cubeVertexBuffer, g_cubeVertexBufferMemory);
        
        void* data;
        vkMapMemory(g_device, g_cubeVertexBufferMemory, 0, vertexBufferSize, 0, &data);
        memcpy(data, cubeVertices.data(), (size_t)vertexBufferSize);
        vkUnmapMemory(g_device, g_cubeVertexBufferMemory);
        
        // Create cube index buffer
        VkDeviceSize indexBufferSize = sizeof(cubeIndices[0]) * cubeIndices.size();
        g_cubeIndexCount = static_cast<uint32_t>(cubeIndices.size());
        createBuffer(indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     g_cubeIndexBuffer, g_cubeIndexBufferMemory);
        
        vkMapMemory(g_device, g_cubeIndexBufferMemory, 0, indexBufferSize, 0, &data);
        memcpy(data, cubeIndices.data(), (size_t)indexBufferSize);
        vkUnmapMemory(g_device, g_cubeIndexBufferMemory);
    }
    
    // Load ball shaders (ball.vert.spv, ball.frag.spv)
    std::vector<char> vertShaderCode, fragShaderCode;
    try {
        vertShaderCode = readFile("shaders/ball.vert.spv");
        fragShaderCode = readFile("shaders/ball.frag.spv");
        std::cout << "[EDEN] Loaded ball shaders successfully" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[EDEN] Failed to load ball shaders: " << e.what() << std::endl;
        std::cerr << "[EDEN] Falling back to default shaders" << std::endl;
        try {
            vertShaderCode = readFile("vert_3d.spv");
            fragShaderCode = readFile("frag_3d.spv");
        } catch (const std::exception& e2) {
            std::cerr << "[EDEN] Failed to load default shaders: " << e2.what() << std::endl;
            return 0;
        }
    }
    
    // Create shader modules
    VkShaderModuleCreateInfo vertCreateInfo = {};
    vertCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    vertCreateInfo.codeSize = vertShaderCode.size();
    vertCreateInfo.pCode = reinterpret_cast<const uint32_t*>(vertShaderCode.data());
    
    VkShaderModule vertShaderModule = VK_NULL_HANDLE;
    if (vkCreateShaderModule(g_device, &vertCreateInfo, nullptr, &vertShaderModule) != VK_SUCCESS) {
        std::cerr << "[EDEN] ERROR: Failed to create vertex shader module!" << std::endl;
        return 0;
    }
    
    VkShaderModuleCreateInfo fragCreateInfo = {};
    fragCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    fragCreateInfo.codeSize = fragShaderCode.size();
    fragCreateInfo.pCode = reinterpret_cast<const uint32_t*>(fragShaderCode.data());
    
    VkShaderModule fragShaderModule = VK_NULL_HANDLE;
    if (vkCreateShaderModule(g_device, &fragCreateInfo, nullptr, &fragShaderModule) != VK_SUCCESS) {
        std::cerr << "[EDEN] ERROR: Failed to create fragment shader module!" << std::endl;
        vkDestroyShaderModule(g_device, vertShaderModule, nullptr);
        return 0;
    }
    
    // Create pipeline for balls (same as cube pipeline but with ball shaders)
    VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";
    
    VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";
    
    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};
    
    // Vertex input (same as cube)
    VkVertexInputBindingDescription bindingDescription = {};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(Vertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    
    std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions = {};
    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[0].offset = offsetof(Vertex, pos);
    
    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[1].offset = offsetof(Vertex, color);
    
    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();
    
    // Pipeline state (same as cube)
    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;
    
    VkViewport viewport = {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)g_swapchainExtent.width;
    viewport.height = (float)g_swapchainExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    
    VkRect2D scissor = {};
    scissor.offset = {0, 0};
    scissor.extent = g_swapchainExtent;
    
    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;
    
    VkPipelineRasterizationStateCreateInfo rasterizer = {};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;
    
    VkPipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    
    VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;
    
    VkPipelineColorBlendStateCreateInfo colorBlending = {};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &g_descriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    
    if (vkCreatePipelineLayout(g_device, &pipelineLayoutInfo, nullptr, &g_pipelineLayout) != VK_SUCCESS) {
        std::cerr << "[EDEN] Failed to create pipeline layout for balls" << std::endl;
        vkDestroyShaderModule(g_device, fragShaderModule, nullptr);
        vkDestroyShaderModule(g_device, vertShaderModule, nullptr);
        return 0;
    }
    
    VkGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.layout = g_pipelineLayout;
    pipelineInfo.renderPass = g_renderPass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    
    if (vkCreateGraphicsPipelines(g_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &g_ballsPipeline) != VK_SUCCESS) {
        std::cerr << "[EDEN] Failed to create graphics pipeline for balls" << std::endl;
        vkDestroyPipelineLayout(g_device, g_pipelineLayout, nullptr);
        vkDestroyShaderModule(g_device, fragShaderModule, nullptr);
        vkDestroyShaderModule(g_device, vertShaderModule, nullptr);
        return 0;
    }
    
    vkDestroyShaderModule(g_device, fragShaderModule, nullptr);
    vkDestroyShaderModule(g_device, vertShaderModule, nullptr);
    
    g_ballsInitialized = true;
    std::cout << "[EDEN] Ball renderer initialized successfully!" << std::endl;
    return 1;
}

extern "C" void heidic_render_balls(GLFWwindow* window, int32_t ball_count, float* positions, float* sizes) {
    if (!g_ballsInitialized || g_device == VK_NULL_HANDLE || g_swapchain == VK_NULL_HANDLE) {
        return;
    }
    
    // Fallback: if no positions provided, use default static positions
    static float default_positions[10 * 3] = {
        0.0f, 0.0f, 0.0f,
        1.5f, 0.5f, -1.0f,
        -1.0f, 1.0f, 0.5f,
        0.5f, -1.2f, 1.0f,
        -1.5f, -0.5f, -1.5f
    };
    static float default_sizes[10] = {0.2f, 0.2f, 0.2f, 0.2f, 0.2f};
    
    static int render_frame_count = 0;
    if (render_frame_count++ % 60 == 0) {
        if (!positions) {
            std::cout << "[Renderer] WARNING: positions array is null, using defaults!" << std::endl;
        } else if (ball_count > 0) {
            std::cout << "[Renderer] Ball 0 pos=(" << positions[0] << "," << positions[1] << "," << positions[2] << ")" << std::endl;
        }
    }
    
    if (!positions) {
        positions = default_positions;
        sizes = default_sizes;
    }
    
    // Wait for previous frame
    vkWaitForFences(g_device, 1, &g_inFlightFence, VK_TRUE, UINT64_MAX);
    vkResetFences(g_device, 1, &g_inFlightFence);
    
    // Acquire next image
    uint32_t imageIndex;
    vkAcquireNextImageKHR(g_device, g_swapchain, UINT64_MAX, g_imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
    
    // Reset command buffer
    vkResetCommandBuffer(g_commandBuffers[imageIndex], 0);
    
    // Begin command buffer
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(g_commandBuffers[imageIndex], &beginInfo);
    
    // Begin render pass
    VkRenderPassBeginInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = g_renderPass;
    renderPassInfo.framebuffer = g_framebuffers[imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = g_swapchainExtent;
    
    // Clear both color and depth so multiple cubes show up correctly each frame
    VkClearValue clearValues[2];
    clearValues[0].color = {{0.1f, 0.1f, 0.15f, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};
    renderPassInfo.clearValueCount = 2;
    renderPassInfo.pClearValues = clearValues;
    
    vkCmdBeginRenderPass(g_commandBuffers[imageIndex], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    
    // Bind pipeline
    vkCmdBindPipeline(g_commandBuffers[imageIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, g_ballsPipeline);
    
    // Bind vertex and index buffers
    VkBuffer vertexBuffers[] = {g_cubeVertexBuffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(g_commandBuffers[imageIndex], 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(g_commandBuffers[imageIndex], g_cubeIndexBuffer, 0, VK_INDEX_TYPE_UINT16);
    
    // Render each ball as a small cube
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    float aspect = (float)width / (float)height;
    
    // View matrix - look at scene from above and to the side
    Vec3 eye = {5.0f, 5.0f, 5.0f};
    Vec3 center = {0.0f, 0.0f, 0.0f};
    Vec3 up = {0.0f, 1.0f, 0.0f};
    
    // Projection matrix
    float fov = 45.0f * 3.14159f / 180.0f;
    float nearPlane = 0.1f;
    float farPlane = 100.0f;
    
    UniformBufferObject ubo = {};
    ubo.view = mat4_lookat(eye, center, up);
    ubo.proj = mat4_perspective(fov, aspect, nearPlane, farPlane);
    ubo.proj[1][1] *= -1.0f;  // Vulkan clip space
    
    // Render each ball (using positions/sizes from ECS)
    int actual_count = (ball_count < MAX_BALLS) ? ball_count : MAX_BALLS;
    for (int32_t i = 0; i < actual_count; i++) {
        int idx = i * 3;
        
        // Model matrix - translate to ball position and scale based on provided size (default 0.2)
        float sx = (sizes && sizes[i] > 0.0f) ? sizes[i] : 0.2f;
        glm::vec3 pos(positions[idx], positions[idx + 1], positions[idx + 2]);
        glm::mat4 model = glm::translate(glm::mat4(1.0f), pos);
        model = glm::scale(model, glm::vec3(sx, sx, sx));
        ubo.model = Mat4(model);
        
        // Update uniform buffer (view/proj only; model via push constants)
        void* data;
        vkMapMemory(g_device, g_uniformBuffersMemory[imageIndex], 0, sizeof(ubo), 0, &data);
        memcpy(data, &ubo, sizeof(ubo));
        vkUnmapMemory(g_device, g_uniformBuffersMemory[imageIndex]);
        
        // Push per-draw model matrix so each cube uses its own transform
        vkCmdPushConstants(g_commandBuffers[imageIndex], g_pipelineLayout,
                           VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Mat4), &ubo.model);
        
        // Bind descriptor set
        vkCmdBindDescriptorSets(g_commandBuffers[imageIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, 
                                g_pipelineLayout, 0, 1, &g_descriptorSets[imageIndex], 0, nullptr);
        
        // Draw cube (ball)
        vkCmdDrawIndexed(g_commandBuffers[imageIndex], g_cubeIndexCount, 1, 0, 0, 0);
    }
    
    vkCmdEndRenderPass(g_commandBuffers[imageIndex]);
    vkEndCommandBuffer(g_commandBuffers[imageIndex]);
    
    // Submit
    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    
    VkSemaphore waitSemaphores[] = {g_imageAvailableSemaphore};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &g_commandBuffers[imageIndex];
    
    VkSemaphore signalSemaphores[] = {g_renderFinishedSemaphore};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;
    
    vkQueueSubmit(g_graphicsQueue, 1, &submitInfo, g_inFlightFence);
    
    // Present
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;
    
    VkSwapchainKHR swapChains[] = {g_swapchain};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;
    
    vkQueuePresentKHR(g_graphicsQueue, &presentInfo);
}

extern "C" void heidic_cleanup_renderer_balls() {
    if (g_ballsPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(g_device, g_ballsPipeline, nullptr);
        g_ballsPipeline = VK_NULL_HANDLE;
    }
    g_ballsInitialized = false;
    // Note: We don't destroy base renderer resources here since they're shared
}

// ============================================================================
// DDS Quad Renderer (for testing DDS loader)
// ============================================================================

// Static variables for DDS quad rendering
static VkImage g_ddsQuadImage = VK_NULL_HANDLE;
static VkImageView g_ddsQuadImageView = VK_NULL_HANDLE;
static VkSampler g_ddsQuadSampler = VK_NULL_HANDLE;
static VkDeviceMemory g_ddsQuadImageMemory = VK_NULL_HANDLE;
static VkPipeline g_ddsQuadPipeline = VK_NULL_HANDLE;
static VkPipelineLayout g_ddsQuadPipelineLayout = VK_NULL_HANDLE;
static VkDescriptorSetLayout g_ddsQuadDescriptorSetLayout = VK_NULL_HANDLE;
static VkDescriptorPool g_ddsQuadDescriptorPool = VK_NULL_HANDLE;
static VkDescriptorSet g_ddsQuadDescriptorSet = VK_NULL_HANDLE;
static VkBuffer g_ddsQuadVertexBuffer = VK_NULL_HANDLE;
static VkDeviceMemory g_ddsQuadVertexBufferMemory = VK_NULL_HANDLE;
static VkBuffer g_ddsQuadIndexBuffer = VK_NULL_HANDLE;
static VkDeviceMemory g_ddsQuadIndexBufferMemory = VK_NULL_HANDLE;
static VkShaderModule g_ddsQuadVertShaderModule = VK_NULL_HANDLE;
static VkShaderModule g_ddsQuadFragShaderModule = VK_NULL_HANDLE;
static bool g_ddsQuadInitialized = false;

// Fullscreen quad vertices (NDC coordinates, covers entire screen)
struct QuadVertex {
    float pos[2];  // X, Y
    float uv[2];   // Texture coordinates
};

static const QuadVertex g_quadVertices[] = {
    {{-1.0f, -1.0f}, {0.0f, 0.0f}},  // Bottom-left
    {{ 1.0f, -1.0f}, {1.0f, 0.0f}},  // Bottom-right
    {{ 1.0f,  1.0f}, {1.0f, 1.0f}},  // Top-right
    {{-1.0f,  1.0f}, {0.0f, 1.0f}}   // Top-left
};

static const uint16_t g_quadIndices[] = {
    0, 1, 2,
    2, 3, 0
};

// Helper to begin single-time command buffer (reuse existing pattern)
static VkCommandBuffer beginSingleTimeCommands() {
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
static void endSingleTimeCommands(VkCommandBuffer commandBuffer) {
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(g_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(g_graphicsQueue);

    vkFreeCommandBuffers(g_device, g_commandPool, 1, &commandBuffer);
}

extern "C" int heidic_init_renderer_dds_quad(GLFWwindow* window, const char* ddsPath) {
    if (window == nullptr || ddsPath == nullptr) {
        std::cerr << "[DDS] ERROR: Invalid parameters!" << std::endl;
        return 0;
    }
    
    // Initialize base renderer (creates device, swapchain, etc.)
    if (heidic_init_renderer(window) == 0) {
        return 0;
    }
    
    // Destroy triangle pipeline since we'll use our own
    if (g_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(g_device, g_pipeline, nullptr);
        g_pipeline = VK_NULL_HANDLE;
    }
    
    std::cout << "[DDS] Loading texture: " << ddsPath << std::endl;
    
    // Load DDS file
    DDSData ddsData = load_dds(ddsPath);
    if (ddsData.format == VK_FORMAT_UNDEFINED) {
        std::cerr << "[DDS] ERROR: Failed to load DDS file: " << ddsPath << std::endl;
        std::cerr << "[DDS] Make sure the file exists and is a valid DDS format" << std::endl;
        return 0;
    }
    
    std::cout << "[DDS] Loaded: " << ddsData.width << "x" << ddsData.height 
              << ", Format: " << ddsData.format << ", Mipmaps: " << ddsData.mipmapCount << std::endl;
    
    // Create Vulkan image
    VkImageCreateInfo imageInfo = {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = ddsData.width;
    imageInfo.extent.height = ddsData.height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = ddsData.mipmapCount;
    imageInfo.arrayLayers = 1;
    imageInfo.format = ddsData.format;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(g_device, &imageInfo, nullptr, &g_ddsQuadImage) != VK_SUCCESS) {
        std::cerr << "[DDS] ERROR: Failed to create image!" << std::endl;
        return 0;
    }

    // Allocate memory for image
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(g_device, g_ddsQuadImage, &memRequirements);

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    
    if (allocInfo.memoryTypeIndex == UINT32_MAX || allocInfo.memoryTypeIndex == 0) {
        std::cerr << "[DDS] ERROR: Failed to find suitable memory type!" << std::endl;
        vkDestroyImage(g_device, g_ddsQuadImage, nullptr);
        return 0;
    }

    if (vkAllocateMemory(g_device, &allocInfo, nullptr, &g_ddsQuadImageMemory) != VK_SUCCESS) {
        std::cerr << "[DDS] ERROR: Failed to allocate image memory!" << std::endl;
        vkDestroyImage(g_device, g_ddsQuadImage, nullptr);
        return 0;
    }

    vkBindImageMemory(g_device, g_ddsQuadImage, g_ddsQuadImageMemory, 0);

    // Create staging buffer to upload compressed data
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = ddsData.compressedData.size();
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(g_device, &bufferInfo, nullptr, &stagingBuffer) != VK_SUCCESS) {
        vkFreeMemory(g_device, g_ddsQuadImageMemory, nullptr);
        vkDestroyImage(g_device, g_ddsQuadImage, nullptr);
        return 0;
    }

    vkGetBufferMemoryRequirements(g_device, stagingBuffer, &memRequirements);
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, 
                                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    
    if (allocInfo.memoryTypeIndex == UINT32_MAX || allocInfo.memoryTypeIndex == 0) {
        std::cerr << "[DDS] ERROR: Failed to find suitable memory type for staging buffer!" << std::endl;
        vkDestroyBuffer(g_device, stagingBuffer, nullptr);
        vkFreeMemory(g_device, g_ddsQuadImageMemory, nullptr);
        vkDestroyImage(g_device, g_ddsQuadImage, nullptr);
        return 0;
    }

    if (vkAllocateMemory(g_device, &allocInfo, nullptr, &stagingBufferMemory) != VK_SUCCESS) {
        vkDestroyBuffer(g_device, stagingBuffer, nullptr);
        vkFreeMemory(g_device, g_ddsQuadImageMemory, nullptr);
        vkDestroyImage(g_device, g_ddsQuadImage, nullptr);
        return 0;
    }

    vkBindBufferMemory(g_device, stagingBuffer, stagingBufferMemory, 0);

    // Copy compressed data to staging buffer
    void* data;
    vkMapMemory(g_device, stagingBufferMemory, 0, ddsData.compressedData.size(), 0, &data);
    memcpy(data, ddsData.compressedData.data(), ddsData.compressedData.size());
    vkUnmapMemory(g_device, stagingBufferMemory);

    // Transition image layout for transfer and upload data
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = g_ddsQuadImage;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = ddsData.mipmapCount;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);

    // Copy buffer to image (for compressed formats, we copy raw blocks)
    VkBufferImageCopy region = {};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {ddsData.width, ddsData.height, 1};

    vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, g_ddsQuadImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    // Transition image layout to shader-readable
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);

    endSingleTimeCommands(commandBuffer);

    // Cleanup staging buffer
    vkDestroyBuffer(g_device, stagingBuffer, nullptr);
    vkFreeMemory(g_device, stagingBufferMemory, nullptr);

    // Create image view
    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = g_ddsQuadImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = ddsData.format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = ddsData.mipmapCount;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(g_device, &viewInfo, nullptr, &g_ddsQuadImageView) != VK_SUCCESS) {
        std::cerr << "[DDS] ERROR: Failed to create image view!" << std::endl;
        vkFreeMemory(g_device, g_ddsQuadImageMemory, nullptr);
        vkDestroyImage(g_device, g_ddsQuadImage, nullptr);
        return 0;
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
    samplerInfo.maxLod = static_cast<float>(ddsData.mipmapCount);

    if (vkCreateSampler(g_device, &samplerInfo, nullptr, &g_ddsQuadSampler) != VK_SUCCESS) {
        std::cerr << "[DDS] ERROR: Failed to create sampler!" << std::endl;
        vkDestroyImageView(g_device, g_ddsQuadImageView, nullptr);
        vkFreeMemory(g_device, g_ddsQuadImageMemory, nullptr);
        vkDestroyImage(g_device, g_ddsQuadImage, nullptr);
        return 0;
    }
    
    // Create descriptor set layout (for texture binding)
    VkDescriptorSetLayoutBinding samplerLayoutBinding = {};
    samplerLayoutBinding.binding = 0;
    samplerLayoutBinding.descriptorCount = 1;
    samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBinding.pImmutableSamplers = nullptr;
    samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &samplerLayoutBinding;

    if (vkCreateDescriptorSetLayout(g_device, &layoutInfo, nullptr, &g_ddsQuadDescriptorSetLayout) != VK_SUCCESS) {
        std::cerr << "[DDS] ERROR: Failed to create descriptor set layout!" << std::endl;
        vkDestroySampler(g_device, g_ddsQuadSampler, nullptr);
        vkDestroyImageView(g_device, g_ddsQuadImageView, nullptr);
        vkFreeMemory(g_device, g_ddsQuadImageMemory, nullptr);
        vkDestroyImage(g_device, g_ddsQuadImage, nullptr);
        return 0;
    }

    // Create descriptor pool
    VkDescriptorPoolSize poolSize = {};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = 1;

    if (vkCreateDescriptorPool(g_device, &poolInfo, nullptr, &g_ddsQuadDescriptorPool) != VK_SUCCESS) {
        std::cerr << "[DDS] ERROR: Failed to create descriptor pool!" << std::endl;
        vkDestroyDescriptorSetLayout(g_device, g_ddsQuadDescriptorSetLayout, nullptr);
        vkDestroySampler(g_device, g_ddsQuadSampler, nullptr);
        vkDestroyImageView(g_device, g_ddsQuadImageView, nullptr);
        vkFreeMemory(g_device, g_ddsQuadImageMemory, nullptr);
        vkDestroyImage(g_device, g_ddsQuadImage, nullptr);
        return 0;
    }

    // Allocate descriptor set
    VkDescriptorSetAllocateInfo allocInfo2 = {};
    allocInfo2.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo2.descriptorPool = g_ddsQuadDescriptorPool;
    allocInfo2.descriptorSetCount = 1;
    allocInfo2.pSetLayouts = &g_ddsQuadDescriptorSetLayout;

    if (vkAllocateDescriptorSets(g_device, &allocInfo2, &g_ddsQuadDescriptorSet) != VK_SUCCESS) {
        std::cerr << "[DDS] ERROR: Failed to allocate descriptor set!" << std::endl;
        vkDestroyDescriptorPool(g_device, g_ddsQuadDescriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(g_device, g_ddsQuadDescriptorSetLayout, nullptr);
        vkDestroySampler(g_device, g_ddsQuadSampler, nullptr);
        vkDestroyImageView(g_device, g_ddsQuadImageView, nullptr);
        vkFreeMemory(g_device, g_ddsQuadImageMemory, nullptr);
        vkDestroyImage(g_device, g_ddsQuadImage, nullptr);
        return 0;
    }

    // Update descriptor set
    VkDescriptorImageInfo imageInfo2 = {};
    imageInfo2.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo2.imageView = g_ddsQuadImageView;
    imageInfo2.sampler = g_ddsQuadSampler;

    VkWriteDescriptorSet descriptorWrite = {};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = g_ddsQuadDescriptorSet;
    descriptorWrite.dstBinding = 0;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pImageInfo = &imageInfo2;

    vkUpdateDescriptorSets(g_device, 1, &descriptorWrite, 0, nullptr);

    // Create quad vertex buffer
    VkDeviceSize vertexBufferSize = sizeof(g_quadVertices);
    createBuffer(vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 g_ddsQuadVertexBuffer, g_ddsQuadVertexBufferMemory);

    vkMapMemory(g_device, g_ddsQuadVertexBufferMemory, 0, vertexBufferSize, 0, &data);
    memcpy(data, g_quadVertices, (size_t)vertexBufferSize);
    vkUnmapMemory(g_device, g_ddsQuadVertexBufferMemory);

    // Create quad index buffer
    VkDeviceSize indexBufferSize = sizeof(g_quadIndices);
    createBuffer(indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 g_ddsQuadIndexBuffer, g_ddsQuadIndexBufferMemory);

    vkMapMemory(g_device, g_ddsQuadIndexBufferMemory, 0, indexBufferSize, 0, &data);
    memcpy(data, g_quadIndices, (size_t)indexBufferSize);
    vkUnmapMemory(g_device, g_ddsQuadIndexBufferMemory);

    // Load quad shaders
    std::vector<char> vertShaderCode, fragShaderCode;
    std::vector<std::string> quadVertPaths = {
        "shaders/quad.vert.spv",
        "quad.vert.spv",
        "examples/dds_quad_test/quad.vert.spv",
        "../examples/dds_quad_test/quad.vert.spv"
    };
    std::vector<std::string> quadFragPaths = {
        "shaders/quad.frag.spv",
        "quad.frag.spv",
        "examples/dds_quad_test/quad.frag.spv",
        "../examples/dds_quad_test/quad.frag.spv"
    };

    bool foundVert = false, foundFrag = false;
    for (const auto& path : quadVertPaths) {
        try {
            vertShaderCode = readFile(path);
            foundVert = true;
            std::cout << "[DDS] Loaded quad vertex shader: " << path << std::endl;
            break;
        } catch (...) {
            // Try next path
        }
    }

    for (const auto& path : quadFragPaths) {
        try {
            fragShaderCode = readFile(path);
            foundFrag = true;
            std::cout << "[DDS] Loaded quad fragment shader: " << path << std::endl;
            break;
        } catch (...) {
            // Try next path
        }
    }

    if (!foundVert || !foundFrag) {
        std::cerr << "[DDS] ERROR: Could not find quad shaders (quad.vert.spv, quad.frag.spv)!" << std::endl;
        std::cerr << "[DDS] Please compile quad.vert and quad.frag shaders first" << std::endl;
        // Cleanup...
        vkDestroyDescriptorPool(g_device, g_ddsQuadDescriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(g_device, g_ddsQuadDescriptorSetLayout, nullptr);
        vkDestroySampler(g_device, g_ddsQuadSampler, nullptr);
        vkDestroyImageView(g_device, g_ddsQuadImageView, nullptr);
        vkFreeMemory(g_device, g_ddsQuadImageMemory, nullptr);
        vkDestroyImage(g_device, g_ddsQuadImage, nullptr);
        return 0;
    }

    // Create shader modules
    VkShaderModuleCreateInfo vertCreateInfo = {};
    vertCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    vertCreateInfo.codeSize = vertShaderCode.size();
    vertCreateInfo.pCode = reinterpret_cast<const uint32_t*>(vertShaderCode.data());

    if (vkCreateShaderModule(g_device, &vertCreateInfo, nullptr, &g_ddsQuadVertShaderModule) != VK_SUCCESS) {
        std::cerr << "[DDS] ERROR: Failed to create vertex shader module!" << std::endl;
        return 0;
    }

    VkShaderModuleCreateInfo fragCreateInfo = {};
    fragCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    fragCreateInfo.codeSize = fragShaderCode.size();
    fragCreateInfo.pCode = reinterpret_cast<const uint32_t*>(fragShaderCode.data());

    if (vkCreateShaderModule(g_device, &fragCreateInfo, nullptr, &g_ddsQuadFragShaderModule) != VK_SUCCESS) {
        std::cerr << "[DDS] ERROR: Failed to create fragment shader module!" << std::endl;
        vkDestroyShaderModule(g_device, g_ddsQuadVertShaderModule, nullptr);
        return 0;
    }

    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &g_ddsQuadDescriptorSetLayout;

    if (vkCreatePipelineLayout(g_device, &pipelineLayoutInfo, nullptr, &g_ddsQuadPipelineLayout) != VK_SUCCESS) {
        std::cerr << "[DDS] ERROR: Failed to create pipeline layout!" << std::endl;
        vkDestroyShaderModule(g_device, g_ddsQuadFragShaderModule, nullptr);
        vkDestroyShaderModule(g_device, g_ddsQuadVertShaderModule, nullptr);
        return 0;
    }

    // Create graphics pipeline
    VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = g_ddsQuadVertShaderModule;
    vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = g_ddsQuadFragShaderModule;
    fragShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

    // Vertex input
    VkVertexInputBindingDescription bindingDescription = {};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(QuadVertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attributeDescriptions[2] = {};
    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[0].offset = offsetof(QuadVertex, pos);

    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[1].offset = offsetof(QuadVertex, uv);

    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = 2;
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport = {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)g_swapchainExtent.width;
    viewport.height = (float)g_swapchainExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor = {};
    scissor.offset = {0, 0};
    scissor.extent = g_swapchainExtent;

    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer = {};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending = {};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    VkGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.layout = g_ddsQuadPipelineLayout;
    pipelineInfo.renderPass = g_renderPass;
    pipelineInfo.subpass = 0;

    if (vkCreateGraphicsPipelines(g_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &g_ddsQuadPipeline) != VK_SUCCESS) {
        std::cerr << "[DDS] ERROR: Failed to create graphics pipeline!" << std::endl;
        vkDestroyPipelineLayout(g_device, g_ddsQuadPipelineLayout, nullptr);
        vkDestroyShaderModule(g_device, g_ddsQuadFragShaderModule, nullptr);
        vkDestroyShaderModule(g_device, g_ddsQuadVertShaderModule, nullptr);
        return 0;
    }

    std::cout << "[DDS] DDS quad renderer initialized successfully!" << std::endl;
    g_ddsQuadInitialized = true;
    return 1;
}

extern "C" void heidic_render_dds_quad(GLFWwindow* window) {
    if (!g_ddsQuadInitialized || g_device == VK_NULL_HANDLE || g_swapchain == VK_NULL_HANDLE) {
        return;
    }

    // Wait for previous frame
    vkWaitForFences(g_device, 1, &g_inFlightFence, VK_TRUE, UINT64_MAX);
    vkResetFences(g_device, 1, &g_inFlightFence);

    // Acquire next image
    uint32_t imageIndex;
    vkAcquireNextImageKHR(g_device, g_swapchain, UINT64_MAX, g_imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

    // Reset command buffer
    vkResetCommandBuffer(g_commandBuffers[imageIndex], 0);

    // Begin command buffer
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(g_commandBuffers[imageIndex], &beginInfo);

    // Begin render pass
    VkRenderPassBeginInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = g_renderPass;
    renderPassInfo.framebuffer = g_framebuffers[imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = g_swapchainExtent;

    VkClearValue clearValues[2] = {};
    clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};
    renderPassInfo.clearValueCount = 2;
    renderPassInfo.pClearValues = clearValues;

    vkCmdBeginRenderPass(g_commandBuffers[imageIndex], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Bind pipeline
    vkCmdBindPipeline(g_commandBuffers[imageIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, g_ddsQuadPipeline);

    // Bind descriptor set (contains texture)
    vkCmdBindDescriptorSets(g_commandBuffers[imageIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, 
                            g_ddsQuadPipelineLayout, 0, 1, &g_ddsQuadDescriptorSet, 0, nullptr);

    // Bind vertex and index buffers
    VkBuffer vertexBuffers[] = {g_ddsQuadVertexBuffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(g_commandBuffers[imageIndex], 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(g_commandBuffers[imageIndex], g_ddsQuadIndexBuffer, 0, VK_INDEX_TYPE_UINT16);

    // Draw quad (2 triangles = 6 indices)
    vkCmdDrawIndexed(g_commandBuffers[imageIndex], 6, 1, 0, 0, 0);

    vkCmdEndRenderPass(g_commandBuffers[imageIndex]);
    vkEndCommandBuffer(g_commandBuffers[imageIndex]);

    // Submit
    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = {g_imageAvailableSemaphore};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &g_commandBuffers[imageIndex];

    VkSemaphore signalSemaphores[] = {g_renderFinishedSemaphore};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    vkQueueSubmit(g_graphicsQueue, 1, &submitInfo, g_inFlightFence);

    // Present
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = {g_swapchain};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;

    vkQueuePresentKHR(g_graphicsQueue, &presentInfo);
}

extern "C" void heidic_cleanup_renderer_dds_quad() {
    if (g_ddsQuadSampler != VK_NULL_HANDLE) {
        vkDestroySampler(g_device, g_ddsQuadSampler, nullptr);
        g_ddsQuadSampler = VK_NULL_HANDLE;
    }
    if (g_ddsQuadImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(g_device, g_ddsQuadImageView, nullptr);
        g_ddsQuadImageView = VK_NULL_HANDLE;
    }
    if (g_ddsQuadImage != VK_NULL_HANDLE) {
        vkDestroyImage(g_device, g_ddsQuadImage, nullptr);
        g_ddsQuadImage = VK_NULL_HANDLE;
    }
    if (g_ddsQuadImageMemory != VK_NULL_HANDLE) {
        vkFreeMemory(g_device, g_ddsQuadImageMemory, nullptr);
        g_ddsQuadImageMemory = VK_NULL_HANDLE;
    }
    if (g_ddsQuadPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(g_device, g_ddsQuadPipeline, nullptr);
        g_ddsQuadPipeline = VK_NULL_HANDLE;
    }
    if (g_ddsQuadPipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(g_device, g_ddsQuadPipelineLayout, nullptr);
        g_ddsQuadPipelineLayout = VK_NULL_HANDLE;
    }
    if (g_ddsQuadDescriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(g_device, g_ddsQuadDescriptorPool, nullptr);
        g_ddsQuadDescriptorPool = VK_NULL_HANDLE;
    }
    if (g_ddsQuadDescriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(g_device, g_ddsQuadDescriptorSetLayout, nullptr);
        g_ddsQuadDescriptorSetLayout = VK_NULL_HANDLE;
    }
    if (g_ddsQuadVertexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(g_device, g_ddsQuadVertexBuffer, nullptr);
        g_ddsQuadVertexBuffer = VK_NULL_HANDLE;
    }
    if (g_ddsQuadVertexBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(g_device, g_ddsQuadVertexBufferMemory, nullptr);
        g_ddsQuadVertexBufferMemory = VK_NULL_HANDLE;
    }
    if (g_ddsQuadIndexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(g_device, g_ddsQuadIndexBuffer, nullptr);
        g_ddsQuadIndexBuffer = VK_NULL_HANDLE;
    }
    if (g_ddsQuadIndexBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(g_device, g_ddsQuadIndexBufferMemory, nullptr);
        g_ddsQuadIndexBufferMemory = VK_NULL_HANDLE;
    }
    if (g_ddsQuadVertShaderModule != VK_NULL_HANDLE) {
        vkDestroyShaderModule(g_device, g_ddsQuadVertShaderModule, nullptr);
        g_ddsQuadVertShaderModule = VK_NULL_HANDLE;
    }
    if (g_ddsQuadFragShaderModule != VK_NULL_HANDLE) {
        vkDestroyShaderModule(g_device, g_ddsQuadFragShaderModule, nullptr);
        g_ddsQuadFragShaderModule = VK_NULL_HANDLE;
    }
    g_ddsQuadInitialized = false;
}

// ========== PNG QUAD RENDERER ==========
// PNG Quad Renderer Global State (similar to DDS but for uncompressed RGBA8)
static VkImage g_pngQuadImage = VK_NULL_HANDLE;
static VkImageView g_pngQuadImageView = VK_NULL_HANDLE;
static VkSampler g_pngQuadSampler = VK_NULL_HANDLE;
static VkDeviceMemory g_pngQuadImageMemory = VK_NULL_HANDLE;
static VkPipeline g_pngQuadPipeline = VK_NULL_HANDLE;
static VkPipelineLayout g_pngQuadPipelineLayout = VK_NULL_HANDLE;
static VkDescriptorSetLayout g_pngQuadDescriptorSetLayout = VK_NULL_HANDLE;
static VkDescriptorPool g_pngQuadDescriptorPool = VK_NULL_HANDLE;
static VkDescriptorSet g_pngQuadDescriptorSet = VK_NULL_HANDLE;
static VkBuffer g_pngQuadVertexBuffer = VK_NULL_HANDLE;
static VkDeviceMemory g_pngQuadVertexBufferMemory = VK_NULL_HANDLE;
static VkBuffer g_pngQuadIndexBuffer = VK_NULL_HANDLE;
static VkDeviceMemory g_pngQuadIndexBufferMemory = VK_NULL_HANDLE;
static VkShaderModule g_pngQuadVertShaderModule = VK_NULL_HANDLE;
static VkShaderModule g_pngQuadFragShaderModule = VK_NULL_HANDLE;
static bool g_pngQuadInitialized = false;

extern "C" int heidic_init_renderer_png_quad(GLFWwindow* window, const char* pngPath) {
    if (window == nullptr || pngPath == nullptr) {
        std::cerr << "[PNG] ERROR: Invalid parameters!" << std::endl;
        return 0;
    }
    
    // Initialize base renderer (creates device, swapchain, etc.)
    if (heidic_init_renderer(window) == 0) {
        return 0;
    }
    
    // Destroy triangle pipeline since we'll use our own
    if (g_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(g_device, g_pipeline, nullptr);
        g_pipeline = VK_NULL_HANDLE;
    }
    
    std::cout << "[PNG] Loading texture: " << pngPath << std::endl;
    
    // Load PNG file
    PNGData pngData;
    try {
        pngData = load_png(pngPath);
    } catch (const std::exception& e) {
        std::cerr << "[PNG] ERROR: " << e.what() << std::endl;
        return 0;
    }
    
    std::cout << "[PNG] Loaded: " << pngData.width << "x" << pngData.height 
              << ", Format: " << pngData.format << std::endl;
    
    // Create Vulkan image (PNG is uncompressed RGBA8, no mipmaps)
    VkImageCreateInfo imageInfo = {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = pngData.width;
    imageInfo.extent.height = pngData.height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;  // PNG has no mipmaps
    imageInfo.arrayLayers = 1;
    imageInfo.format = pngData.format;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    if (vkCreateImage(g_device, &imageInfo, nullptr, &g_pngQuadImage) != VK_SUCCESS) {
        std::cerr << "[PNG] ERROR: Failed to create image!" << std::endl;
        return 0;
    }
    
    // Allocate memory for image
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(g_device, g_pngQuadImage, &memRequirements);
    
    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    
    if (allocInfo.memoryTypeIndex == UINT32_MAX || allocInfo.memoryTypeIndex == 0) {
        std::cerr << "[PNG] ERROR: Failed to find suitable memory type!" << std::endl;
        vkDestroyImage(g_device, g_pngQuadImage, nullptr);
        return 0;
    }
    
    if (vkAllocateMemory(g_device, &allocInfo, nullptr, &g_pngQuadImageMemory) != VK_SUCCESS) {
        std::cerr << "[PNG] ERROR: Failed to allocate image memory!" << std::endl;
        vkDestroyImage(g_device, g_pngQuadImage, nullptr);
        return 0;
    }
    
    vkBindImageMemory(g_device, g_pngQuadImage, g_pngQuadImageMemory, 0);
    
    // Create staging buffer to upload uncompressed RGBA8 data
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = pngData.pixelData.size();
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    if (vkCreateBuffer(g_device, &bufferInfo, nullptr, &stagingBuffer) != VK_SUCCESS) {
        vkFreeMemory(g_device, g_pngQuadImageMemory, nullptr);
        vkDestroyImage(g_device, g_pngQuadImage, nullptr);
        std::cerr << "[PNG] ERROR: Failed to create staging buffer!" << std::endl;
        return 0;
    }
    
    vkGetBufferMemoryRequirements(g_device, stagingBuffer, &memRequirements);
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, 
                                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    
    if (vkAllocateMemory(g_device, &allocInfo, nullptr, &stagingBufferMemory) != VK_SUCCESS) {
        vkDestroyBuffer(g_device, stagingBuffer, nullptr);
        vkFreeMemory(g_device, g_pngQuadImageMemory, nullptr);
        vkDestroyImage(g_device, g_pngQuadImage, nullptr);
        std::cerr << "[PNG] ERROR: Failed to allocate staging buffer memory!" << std::endl;
        return 0;
    }
    
    vkBindBufferMemory(g_device, stagingBuffer, stagingBufferMemory, 0);
    
    // Copy pixel data to staging buffer
    void* data;
    vkMapMemory(g_device, stagingBufferMemory, 0, pngData.pixelData.size(), 0, &data);
    memcpy(data, pngData.pixelData.data(), pngData.pixelData.size());
    vkUnmapMemory(g_device, stagingBufferMemory);
    
    // Transition image layout for transfer and upload data
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();
    
    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = g_pngQuadImage;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);
    
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
    region.imageExtent = {pngData.width, pngData.height, 1};
    
    vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, g_pngQuadImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    
    // Transition image layout to shader-readable
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);
    
    endSingleTimeCommands(commandBuffer);
    
    // Cleanup staging buffer
    vkDestroyBuffer(g_device, stagingBuffer, nullptr);
    vkFreeMemory(g_device, stagingBufferMemory, nullptr);
    
    // Create image view
    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = g_pngQuadImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = pngData.format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    
    if (vkCreateImageView(g_device, &viewInfo, nullptr, &g_pngQuadImageView) != VK_SUCCESS) {
        std::cerr << "[PNG] ERROR: Failed to create image view!" << std::endl;
        vkFreeMemory(g_device, g_pngQuadImageMemory, nullptr);
        vkDestroyImage(g_device, g_pngQuadImage, nullptr);
        return 0;
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
    samplerInfo.maxLod = 0.0f;  // No mipmaps for PNG
    
    if (vkCreateSampler(g_device, &samplerInfo, nullptr, &g_pngQuadSampler) != VK_SUCCESS) {
        std::cerr << "[PNG] ERROR: Failed to create sampler!" << std::endl;
        vkDestroyImageView(g_device, g_pngQuadImageView, nullptr);
        vkFreeMemory(g_device, g_pngQuadImageMemory, nullptr);
        vkDestroyImage(g_device, g_pngQuadImage, nullptr);
        return 0;
    }
    
    // Create descriptor set layout (same as DDS)
    VkDescriptorSetLayoutBinding samplerLayoutBinding = {};
    samplerLayoutBinding.binding = 0;
    samplerLayoutBinding.descriptorCount = 1;
    samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBinding.pImmutableSamplers = nullptr;
    samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    
    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &samplerLayoutBinding;
    
    if (vkCreateDescriptorSetLayout(g_device, &layoutInfo, nullptr, &g_pngQuadDescriptorSetLayout) != VK_SUCCESS) {
        std::cerr << "[PNG] ERROR: Failed to create descriptor set layout!" << std::endl;
        vkDestroySampler(g_device, g_pngQuadSampler, nullptr);
        vkDestroyImageView(g_device, g_pngQuadImageView, nullptr);
        vkFreeMemory(g_device, g_pngQuadImageMemory, nullptr);
        vkDestroyImage(g_device, g_pngQuadImage, nullptr);
        return 0;
    }
    
    // Create descriptor pool
    VkDescriptorPoolSize poolSize = {};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = 1;
    
    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = 1;
    
    if (vkCreateDescriptorPool(g_device, &poolInfo, nullptr, &g_pngQuadDescriptorPool) != VK_SUCCESS) {
        std::cerr << "[PNG] ERROR: Failed to create descriptor pool!" << std::endl;
        vkDestroyDescriptorSetLayout(g_device, g_pngQuadDescriptorSetLayout, nullptr);
        vkDestroySampler(g_device, g_pngQuadSampler, nullptr);
        vkDestroyImageView(g_device, g_pngQuadImageView, nullptr);
        vkFreeMemory(g_device, g_pngQuadImageMemory, nullptr);
        vkDestroyImage(g_device, g_pngQuadImage, nullptr);
        return 0;
    }
    
    // Allocate descriptor set
    VkDescriptorSetAllocateInfo allocInfo2 = {};
    allocInfo2.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo2.descriptorPool = g_pngQuadDescriptorPool;
    allocInfo2.descriptorSetCount = 1;
    allocInfo2.pSetLayouts = &g_pngQuadDescriptorSetLayout;
    
    if (vkAllocateDescriptorSets(g_device, &allocInfo2, &g_pngQuadDescriptorSet) != VK_SUCCESS) {
        std::cerr << "[PNG] ERROR: Failed to allocate descriptor set!" << std::endl;
        vkDestroyDescriptorPool(g_device, g_pngQuadDescriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(g_device, g_pngQuadDescriptorSetLayout, nullptr);
        vkDestroySampler(g_device, g_pngQuadSampler, nullptr);
        vkDestroyImageView(g_device, g_pngQuadImageView, nullptr);
        vkFreeMemory(g_device, g_pngQuadImageMemory, nullptr);
        vkDestroyImage(g_device, g_pngQuadImage, nullptr);
        return 0;
    }
    
    // Update descriptor set
    VkDescriptorImageInfo imageInfo2 = {};
    imageInfo2.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo2.imageView = g_pngQuadImageView;
    imageInfo2.sampler = g_pngQuadSampler;
    
    VkWriteDescriptorSet descriptorWrite = {};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = g_pngQuadDescriptorSet;
    descriptorWrite.dstBinding = 0;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pImageInfo = &imageInfo2;
    
    vkUpdateDescriptorSets(g_device, 1, &descriptorWrite, 0, nullptr);
    
    // Create quad vertex buffer (reuse same quad vertices as DDS)
    VkDeviceSize vertexBufferSize = sizeof(g_quadVertices);
    createBuffer(vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 g_pngQuadVertexBuffer, g_pngQuadVertexBufferMemory);
    
    vkMapMemory(g_device, g_pngQuadVertexBufferMemory, 0, vertexBufferSize, 0, &data);
    memcpy(data, g_quadVertices, (size_t)vertexBufferSize);
    vkUnmapMemory(g_device, g_pngQuadVertexBufferMemory);
    
    // Create quad index buffer
    VkDeviceSize indexBufferSize = sizeof(g_quadIndices);
    createBuffer(indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 g_pngQuadIndexBuffer, g_pngQuadIndexBufferMemory);
    
    vkMapMemory(g_device, g_pngQuadIndexBufferMemory, 0, indexBufferSize, 0, &data);
    memcpy(data, g_quadIndices, (size_t)indexBufferSize);
    vkUnmapMemory(g_device, g_pngQuadIndexBufferMemory);
    
    // Load quad shaders (same paths as DDS)
    std::vector<char> vertShaderCode, fragShaderCode;
    std::vector<std::string> quadVertPaths = {
        "shaders/quad.vert.spv",
        "quad.vert.spv",
        "examples/dds_quad_test/quad.vert.spv",
        "../examples/dds_quad_test/quad.vert.spv"
    };
    std::vector<std::string> quadFragPaths = {
        "shaders/quad.frag.spv",
        "quad.frag.spv",
        "examples/dds_quad_test/quad.frag.spv",
        "../examples/dds_quad_test/quad.frag.spv"
    };
    
    bool foundVert = false, foundFrag = false;
    for (const auto& path : quadVertPaths) {
        try {
            vertShaderCode = readFile(path);
            foundVert = true;
            std::cout << "[PNG] Loaded quad vertex shader: " << path << std::endl;
            break;
        } catch (...) {
            // Try next path
        }
    }
    
    for (const auto& path : quadFragPaths) {
        try {
            fragShaderCode = readFile(path);
            foundFrag = true;
            std::cout << "[PNG] Loaded quad fragment shader: " << path << std::endl;
            break;
        } catch (...) {
            // Try next path
        }
    }
    
    if (!foundVert || !foundFrag) {
        std::cerr << "[PNG] ERROR: Failed to load quad shaders!" << std::endl;
        vkDestroyBuffer(g_device, g_pngQuadIndexBuffer, nullptr);
        vkFreeMemory(g_device, g_pngQuadIndexBufferMemory, nullptr);
        vkDestroyBuffer(g_device, g_pngQuadVertexBuffer, nullptr);
        vkFreeMemory(g_device, g_pngQuadVertexBufferMemory, nullptr);
        vkDestroyDescriptorPool(g_device, g_pngQuadDescriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(g_device, g_pngQuadDescriptorSetLayout, nullptr);
        vkDestroySampler(g_device, g_pngQuadSampler, nullptr);
        vkDestroyImageView(g_device, g_pngQuadImageView, nullptr);
        vkFreeMemory(g_device, g_pngQuadImageMemory, nullptr);
        vkDestroyImage(g_device, g_pngQuadImage, nullptr);
        return 0;
    }
    
    // Create shader modules
    VkShaderModuleCreateInfo vertCreateInfo = {};
    vertCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    vertCreateInfo.codeSize = vertShaderCode.size();
    vertCreateInfo.pCode = reinterpret_cast<const uint32_t*>(vertShaderCode.data());
    
    if (vkCreateShaderModule(g_device, &vertCreateInfo, nullptr, &g_pngQuadVertShaderModule) != VK_SUCCESS) {
        std::cerr << "[PNG] ERROR: Failed to create vertex shader module!" << std::endl;
        return 0;
    }
    
    VkShaderModuleCreateInfo fragCreateInfo = {};
    fragCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    fragCreateInfo.codeSize = fragShaderCode.size();
    fragCreateInfo.pCode = reinterpret_cast<const uint32_t*>(fragShaderCode.data());
    
    if (vkCreateShaderModule(g_device, &fragCreateInfo, nullptr, &g_pngQuadFragShaderModule) != VK_SUCCESS) {
        std::cerr << "[PNG] ERROR: Failed to create fragment shader module!" << std::endl;
        vkDestroyShaderModule(g_device, g_pngQuadVertShaderModule, nullptr);
        return 0;
    }
    
    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &g_pngQuadDescriptorSetLayout;
    
    if (vkCreatePipelineLayout(g_device, &pipelineLayoutInfo, nullptr, &g_pngQuadPipelineLayout) != VK_SUCCESS) {
        std::cerr << "[PNG] ERROR: Failed to create pipeline layout!" << std::endl;
        vkDestroyShaderModule(g_device, g_pngQuadFragShaderModule, nullptr);
        vkDestroyShaderModule(g_device, g_pngQuadVertShaderModule, nullptr);
        return 0;
    }
    
    // Create graphics pipeline (same as DDS)
    VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = g_pngQuadVertShaderModule;
    vertShaderStageInfo.pName = "main";
    
    VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = g_pngQuadFragShaderModule;
    fragShaderStageInfo.pName = "main";
    
    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};
    
    // Vertex input
    VkVertexInputBindingDescription bindingDescription = {};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(QuadVertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    
    VkVertexInputAttributeDescription attributeDescriptions[2] = {};
    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[0].offset = offsetof(QuadVertex, pos);
    
    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[1].offset = offsetof(QuadVertex, uv);
    
    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = 2;
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions;
    
    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;
    
    VkViewport viewport = {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)g_swapchainExtent.width;
    viewport.height = (float)g_swapchainExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    
    VkRect2D scissor = {};
    scissor.offset = {0, 0};
    scissor.extent = g_swapchainExtent;
    
    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;
    
    VkPipelineRasterizationStateCreateInfo rasterizer = {};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;
    
    VkPipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    
    VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;
    
    VkPipelineColorBlendStateCreateInfo colorBlending = {};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    
    VkGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.layout = g_pngQuadPipelineLayout;
    pipelineInfo.renderPass = g_renderPass;
    pipelineInfo.subpass = 0;
    
    if (vkCreateGraphicsPipelines(g_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &g_pngQuadPipeline) != VK_SUCCESS) {
        std::cerr << "[PNG] ERROR: Failed to create graphics pipeline!" << std::endl;
        vkDestroyPipelineLayout(g_device, g_pngQuadPipelineLayout, nullptr);
        vkDestroyShaderModule(g_device, g_pngQuadFragShaderModule, nullptr);
        vkDestroyShaderModule(g_device, g_pngQuadVertShaderModule, nullptr);
        return 0;
    }
    
    std::cout << "[PNG] PNG quad renderer initialized successfully!" << std::endl;
    g_pngQuadInitialized = true;
    return 1;
}

extern "C" void heidic_render_png_quad(GLFWwindow* window) {
    if (!g_pngQuadInitialized || g_device == VK_NULL_HANDLE || g_swapchain == VK_NULL_HANDLE) {
        return;
    }
    
    // Wait for previous frame
    vkWaitForFences(g_device, 1, &g_inFlightFence, VK_TRUE, UINT64_MAX);
    vkResetFences(g_device, 1, &g_inFlightFence);
    
    // Acquire next image
    uint32_t imageIndex;
    vkAcquireNextImageKHR(g_device, g_swapchain, UINT64_MAX, g_imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
    
    // Reset command buffer
    vkResetCommandBuffer(g_commandBuffers[imageIndex], 0);
    
    // Begin command buffer
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(g_commandBuffers[imageIndex], &beginInfo);
    
    // Begin render pass
    VkRenderPassBeginInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = g_renderPass;
    renderPassInfo.framebuffer = g_framebuffers[imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = g_swapchainExtent;
    
    VkClearValue clearValues[2] = {};
    clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};
    renderPassInfo.clearValueCount = 2;
    renderPassInfo.pClearValues = clearValues;
    
    vkCmdBeginRenderPass(g_commandBuffers[imageIndex], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    
    // Bind pipeline
    vkCmdBindPipeline(g_commandBuffers[imageIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, g_pngQuadPipeline);
    
    // Bind descriptor set (contains texture)
    vkCmdBindDescriptorSets(g_commandBuffers[imageIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, 
                            g_pngQuadPipelineLayout, 0, 1, &g_pngQuadDescriptorSet, 0, nullptr);
    
    // Bind vertex and index buffers
    VkBuffer vertexBuffers[] = {g_pngQuadVertexBuffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(g_commandBuffers[imageIndex], 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(g_commandBuffers[imageIndex], g_pngQuadIndexBuffer, 0, VK_INDEX_TYPE_UINT16);
    
    // Draw quad (2 triangles = 6 indices)
    vkCmdDrawIndexed(g_commandBuffers[imageIndex], 6, 1, 0, 0, 0);
    
    vkCmdEndRenderPass(g_commandBuffers[imageIndex]);
    vkEndCommandBuffer(g_commandBuffers[imageIndex]);
    
    // Submit
    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    
    VkSemaphore waitSemaphores[] = {g_imageAvailableSemaphore};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &g_commandBuffers[imageIndex];
    
    VkSemaphore signalSemaphores[] = {g_renderFinishedSemaphore};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;
    
    vkQueueSubmit(g_graphicsQueue, 1, &submitInfo, g_inFlightFence);
    
    // Present
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;
    
    VkSwapchainKHR swapChains[] = {g_swapchain};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;
    
    vkQueuePresentKHR(g_graphicsQueue, &presentInfo);
}

extern "C" void heidic_cleanup_renderer_png_quad() {
    if (g_pngQuadSampler != VK_NULL_HANDLE) {
        vkDestroySampler(g_device, g_pngQuadSampler, nullptr);
        g_pngQuadSampler = VK_NULL_HANDLE;
    }
    if (g_pngQuadImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(g_device, g_pngQuadImageView, nullptr);
        g_pngQuadImageView = VK_NULL_HANDLE;
    }
    if (g_pngQuadImage != VK_NULL_HANDLE) {
        vkDestroyImage(g_device, g_pngQuadImage, nullptr);
        g_pngQuadImage = VK_NULL_HANDLE;
    }
    if (g_pngQuadImageMemory != VK_NULL_HANDLE) {
        vkFreeMemory(g_device, g_pngQuadImageMemory, nullptr);
        g_pngQuadImageMemory = VK_NULL_HANDLE;
    }
    if (g_pngQuadPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(g_device, g_pngQuadPipeline, nullptr);
        g_pngQuadPipeline = VK_NULL_HANDLE;
    }
    if (g_pngQuadPipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(g_device, g_pngQuadPipelineLayout, nullptr);
        g_pngQuadPipelineLayout = VK_NULL_HANDLE;
    }
    if (g_pngQuadDescriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(g_device, g_pngQuadDescriptorPool, nullptr);
        g_pngQuadDescriptorPool = VK_NULL_HANDLE;
    }
    if (g_pngQuadDescriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(g_device, g_pngQuadDescriptorSetLayout, nullptr);
        g_pngQuadDescriptorSetLayout = VK_NULL_HANDLE;
    }
    if (g_pngQuadVertexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(g_device, g_pngQuadVertexBuffer, nullptr);
        g_pngQuadVertexBuffer = VK_NULL_HANDLE;
    }
    if (g_pngQuadVertexBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(g_device, g_pngQuadVertexBufferMemory, nullptr);
        g_pngQuadVertexBufferMemory = VK_NULL_HANDLE;
    }
    if (g_pngQuadIndexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(g_device, g_pngQuadIndexBuffer, nullptr);
        g_pngQuadIndexBuffer = VK_NULL_HANDLE;
    }
    if (g_pngQuadIndexBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(g_device, g_pngQuadIndexBufferMemory, nullptr);
        g_pngQuadIndexBufferMemory = VK_NULL_HANDLE;
    }
    if (g_pngQuadVertShaderModule != VK_NULL_HANDLE) {
        vkDestroyShaderModule(g_device, g_pngQuadVertShaderModule, nullptr);
        g_pngQuadVertShaderModule = VK_NULL_HANDLE;
    }
    if (g_pngQuadFragShaderModule != VK_NULL_HANDLE) {
        vkDestroyShaderModule(g_device, g_pngQuadFragShaderModule, nullptr);
        g_pngQuadFragShaderModule = VK_NULL_HANDLE;
    }
    g_pngQuadInitialized = false;
}

// ========== TEXTURERESOURCE QUAD RENDERER (TEST) ==========
// Test renderer that uses TextureResource class to load textures automatically
static std::unique_ptr<TextureResource> g_textureResourceQuad = nullptr;
static VkPipeline g_textureResourceQuadPipeline = VK_NULL_HANDLE;
static VkPipelineLayout g_textureResourceQuadPipelineLayout = VK_NULL_HANDLE;
static VkDescriptorSetLayout g_textureResourceQuadDescriptorSetLayout = VK_NULL_HANDLE;
static VkDescriptorPool g_textureResourceQuadDescriptorPool = VK_NULL_HANDLE;
static VkDescriptorSet g_textureResourceQuadDescriptorSet = VK_NULL_HANDLE;
static VkBuffer g_textureResourceQuadVertexBuffer = VK_NULL_HANDLE;
static VkDeviceMemory g_textureResourceQuadVertexBufferMemory = VK_NULL_HANDLE;
static VkBuffer g_textureResourceQuadIndexBuffer = VK_NULL_HANDLE;
static VkDeviceMemory g_textureResourceQuadIndexBufferMemory = VK_NULL_HANDLE;
static VkShaderModule g_textureResourceQuadVertShaderModule = VK_NULL_HANDLE;
static VkShaderModule g_textureResourceQuadFragShaderModule = VK_NULL_HANDLE;
static bool g_textureResourceQuadInitialized = false;

extern "C" int heidic_init_renderer_texture_quad(GLFWwindow* window, const char* texturePath) {
    if (window == nullptr || texturePath == nullptr) {
        std::cerr << "[TextureResource] ERROR: Invalid parameters!" << std::endl;
        return 0;
    }
    
    // Initialize base renderer (only if not already initialized)
    if (g_device == VK_NULL_HANDLE) {
        if (heidic_init_renderer(window) == 0) {
            return 0;
        }
    } else {
        // Already initialized, just destroy triangle pipeline if it exists
        if (g_pipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(g_device, g_pipeline, nullptr);
            g_pipeline = VK_NULL_HANDLE;
        }
    }
    
    std::cout << "[TextureResource] Loading texture: " << texturePath << std::endl;
    
    // Debug: Print current working directory (helps diagnose path issues)
    #ifdef _WIN32
    char cwd[1024];
    if (GetCurrentDirectoryA(1024, cwd)) {
        std::cout << "[TextureResource] Current working directory: " << cwd << std::endl;
    }
    #else
    char* cwd = getcwd(nullptr, 0);
    if (cwd) {
        std::cout << "[TextureResource] Current working directory: " << cwd << std::endl;
        free(cwd);
    }
    #endif
    
    std::cout << "[TextureResource] Trying paths:" << std::endl;
    
    // Try multiple path variations to help debug path issues
    std::vector<std::string> testPaths = {
        texturePath,  // Original path
        std::string("./") + texturePath,  // Explicit relative
    };
    
    std::string actualPath = texturePath;
    bool found = false;
    for (const auto& testPath : testPaths) {
        std::cout << "  - " << testPath;
        std::ifstream testFile(testPath, std::ios::binary);
        if (testFile.is_open()) {
            testFile.close();
            actualPath = testPath;
            found = true;
            std::cout << " [FOUND]" << std::endl;
            break;
        } else {
            std::cout << " [NOT FOUND]" << std::endl;
        }
    }
    
    if (!found) {
        std::cerr << "[TextureResource] ERROR: Cannot find texture file!" << std::endl;
        std::cerr << "[TextureResource] Make sure the texture file exists relative to the working directory!" << std::endl;
        std::cerr << "[TextureResource] Expected location: textures/test.dds or textures/test.png" << std::endl;
        return 0;
    }
    
    // Use TextureResource to load texture (auto-detects DDS vs PNG)
    try {
        g_textureResourceQuad = std::make_unique<TextureResource>(actualPath);
        std::cout << "[TextureResource] Loaded: " << g_textureResourceQuad->getWidth() 
                  << "x" << g_textureResourceQuad->getHeight() 
                  << ", Format: " << g_textureResourceQuad->getFormat() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[TextureResource] ERROR: " << e.what() << std::endl;
        return 0;
    }
    
    // Create descriptor set layout
    VkDescriptorSetLayoutBinding samplerLayoutBinding = {};
    samplerLayoutBinding.binding = 0;
    samplerLayoutBinding.descriptorCount = 1;
    samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBinding.pImmutableSamplers = nullptr;
    samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    
    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &samplerLayoutBinding;
    
    if (vkCreateDescriptorSetLayout(g_device, &layoutInfo, nullptr, &g_textureResourceQuadDescriptorSetLayout) != VK_SUCCESS) {
        std::cerr << "[TextureResource] ERROR: Failed to create descriptor set layout!" << std::endl;
        g_textureResourceQuad.reset();
        return 0;
    }
    
    // Create descriptor pool
    VkDescriptorPoolSize poolSize = {};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = 1;
    
    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = 1;
    
    if (vkCreateDescriptorPool(g_device, &poolInfo, nullptr, &g_textureResourceQuadDescriptorPool) != VK_SUCCESS) {
        std::cerr << "[TextureResource] ERROR: Failed to create descriptor pool!" << std::endl;
        vkDestroyDescriptorSetLayout(g_device, g_textureResourceQuadDescriptorSetLayout, nullptr);
        g_textureResourceQuad.reset();
        return 0;
    }
    
    // Allocate descriptor set
    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = g_textureResourceQuadDescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &g_textureResourceQuadDescriptorSetLayout;
    
    if (vkAllocateDescriptorSets(g_device, &allocInfo, &g_textureResourceQuadDescriptorSet) != VK_SUCCESS) {
        std::cerr << "[TextureResource] ERROR: Failed to allocate descriptor set!" << std::endl;
        vkDestroyDescriptorPool(g_device, g_textureResourceQuadDescriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(g_device, g_textureResourceQuadDescriptorSetLayout, nullptr);
        g_textureResourceQuad.reset();
        return 0;
    }
    
    // Update descriptor set using TextureResource
    VkDescriptorImageInfo imageInfo = g_textureResourceQuad->getDescriptorImageInfo();
    VkWriteDescriptorSet descriptorWrite = {};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = g_textureResourceQuadDescriptorSet;
    descriptorWrite.dstBinding = 0;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pImageInfo = &imageInfo;
    
    vkUpdateDescriptorSets(g_device, 1, &descriptorWrite, 0, nullptr);
    
    // Create quad vertex/index buffers (reuse same vertices as DDS/PNG renderers)
    VkDeviceSize vertexBufferSize = sizeof(g_quadVertices);
    createBuffer(vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 g_textureResourceQuadVertexBuffer, g_textureResourceQuadVertexBufferMemory);
    
    void* data;
    vkMapMemory(g_device, g_textureResourceQuadVertexBufferMemory, 0, vertexBufferSize, 0, &data);
    memcpy(data, g_quadVertices, (size_t)vertexBufferSize);
    vkUnmapMemory(g_device, g_textureResourceQuadVertexBufferMemory);
    
    VkDeviceSize indexBufferSize = sizeof(g_quadIndices);
    createBuffer(indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 g_textureResourceQuadIndexBuffer, g_textureResourceQuadIndexBufferMemory);
    
    vkMapMemory(g_device, g_textureResourceQuadIndexBufferMemory, 0, indexBufferSize, 0, &data);
    memcpy(data, g_quadIndices, (size_t)indexBufferSize);
    vkUnmapMemory(g_device, g_textureResourceQuadIndexBufferMemory);
    
    // Load quad shaders
    std::vector<char> vertShaderCode, fragShaderCode;
    std::vector<std::string> quadVertPaths = {
        "shaders/quad.vert.spv",
        "quad.vert.spv",
        "examples/dds_quad_test/quad.vert.spv",
        "../examples/dds_quad_test/quad.vert.spv"
    };
    std::vector<std::string> quadFragPaths = {
        "shaders/quad.frag.spv",
        "quad.frag.spv",
        "examples/dds_quad_test/quad.frag.spv",
        "../examples/dds_quad_test/quad.frag.spv"
    };
    
    bool foundVert = false, foundFrag = false;
    for (const auto& path : quadVertPaths) {
        try {
            vertShaderCode = readFile(path);
            foundVert = true;
            std::cout << "[TextureResource] Loaded vertex shader: " << path << std::endl;
            break;
        } catch (...) {}
    }
    
    for (const auto& path : quadFragPaths) {
        try {
            fragShaderCode = readFile(path);
            foundFrag = true;
            std::cout << "[TextureResource] Loaded fragment shader: " << path << std::endl;
            break;
        } catch (...) {}
    }
    
    if (!foundVert || !foundFrag) {
        std::cerr << "[TextureResource] ERROR: Failed to load quad shaders!" << std::endl;
        vkDestroyBuffer(g_device, g_textureResourceQuadIndexBuffer, nullptr);
        vkFreeMemory(g_device, g_textureResourceQuadIndexBufferMemory, nullptr);
        vkDestroyBuffer(g_device, g_textureResourceQuadVertexBuffer, nullptr);
        vkFreeMemory(g_device, g_textureResourceQuadVertexBufferMemory, nullptr);
        vkDestroyDescriptorPool(g_device, g_textureResourceQuadDescriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(g_device, g_textureResourceQuadDescriptorSetLayout, nullptr);
        g_textureResourceQuad.reset();
        return 0;
    }
    
    // Create shader modules
    VkShaderModuleCreateInfo vertCreateInfo = {};
    vertCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    vertCreateInfo.codeSize = vertShaderCode.size();
    vertCreateInfo.pCode = reinterpret_cast<const uint32_t*>(vertShaderCode.data());
    
    if (vkCreateShaderModule(g_device, &vertCreateInfo, nullptr, &g_textureResourceQuadVertShaderModule) != VK_SUCCESS) {
        std::cerr << "[TextureResource] ERROR: Failed to create vertex shader module!" << std::endl;
        g_textureResourceQuad.reset();
        return 0;
    }
    
    VkShaderModuleCreateInfo fragCreateInfo = {};
    fragCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    fragCreateInfo.codeSize = fragShaderCode.size();
    fragCreateInfo.pCode = reinterpret_cast<const uint32_t*>(fragShaderCode.data());
    
    if (vkCreateShaderModule(g_device, &fragCreateInfo, nullptr, &g_textureResourceQuadFragShaderModule) != VK_SUCCESS) {
        std::cerr << "[TextureResource] ERROR: Failed to create fragment shader module!" << std::endl;
        vkDestroyShaderModule(g_device, g_textureResourceQuadVertShaderModule, nullptr);
        g_textureResourceQuad.reset();
        return 0;
    }
    
    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &g_textureResourceQuadDescriptorSetLayout;
    
    if (vkCreatePipelineLayout(g_device, &pipelineLayoutInfo, nullptr, &g_textureResourceQuadPipelineLayout) != VK_SUCCESS) {
        std::cerr << "[TextureResource] ERROR: Failed to create pipeline layout!" << std::endl;
        vkDestroyShaderModule(g_device, g_textureResourceQuadFragShaderModule, nullptr);
        vkDestroyShaderModule(g_device, g_textureResourceQuadVertShaderModule, nullptr);
        g_textureResourceQuad.reset();
        return 0;
    }
    
    // Create graphics pipeline (same setup as DDS/PNG renderers)
    VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = g_textureResourceQuadVertShaderModule;
    vertShaderStageInfo.pName = "main";
    
    VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = g_textureResourceQuadFragShaderModule;
    fragShaderStageInfo.pName = "main";
    
    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};
    
    VkVertexInputBindingDescription bindingDescription = {};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(QuadVertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    
    VkVertexInputAttributeDescription attributeDescriptions[2] = {};
    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[0].offset = offsetof(QuadVertex, pos);
    
    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[1].offset = offsetof(QuadVertex, uv);
    
    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = 2;
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions;
    
    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;
    
    VkViewport viewport = {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)g_swapchainExtent.width;
    viewport.height = (float)g_swapchainExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    
    VkRect2D scissor = {};
    scissor.offset = {0, 0};
    scissor.extent = g_swapchainExtent;
    
    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;
    
    VkPipelineRasterizationStateCreateInfo rasterizer = {};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;
    
    VkPipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    
    VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;
    
    VkPipelineColorBlendStateCreateInfo colorBlending = {};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    
    VkGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.layout = g_textureResourceQuadPipelineLayout;
    pipelineInfo.renderPass = g_renderPass;
    pipelineInfo.subpass = 0;
    
    if (vkCreateGraphicsPipelines(g_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &g_textureResourceQuadPipeline) != VK_SUCCESS) {
        std::cerr << "[TextureResource] ERROR: Failed to create graphics pipeline!" << std::endl;
        vkDestroyPipelineLayout(g_device, g_textureResourceQuadPipelineLayout, nullptr);
        vkDestroyShaderModule(g_device, g_textureResourceQuadFragShaderModule, nullptr);
        vkDestroyShaderModule(g_device, g_textureResourceQuadVertShaderModule, nullptr);
        g_textureResourceQuad.reset();
        return 0;
    }
    
    std::cout << "[TextureResource] TextureResource quad renderer initialized successfully!" << std::endl;
    g_textureResourceQuadInitialized = true;
    return 1;
}

extern "C" void heidic_render_texture_quad(GLFWwindow* window) {
    if (!g_textureResourceQuadInitialized || !g_textureResourceQuad || g_device == VK_NULL_HANDLE || g_swapchain == VK_NULL_HANDLE) {
        return;
    }
    
    vkWaitForFences(g_device, 1, &g_inFlightFence, VK_TRUE, UINT64_MAX);
    vkResetFences(g_device, 1, &g_inFlightFence);
    
    uint32_t imageIndex;
    vkAcquireNextImageKHR(g_device, g_swapchain, UINT64_MAX, g_imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
    
    vkResetCommandBuffer(g_commandBuffers[imageIndex], 0);
    
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(g_commandBuffers[imageIndex], &beginInfo);
    
    VkRenderPassBeginInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = g_renderPass;
    renderPassInfo.framebuffer = g_framebuffers[imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = g_swapchainExtent;
    
    VkClearValue clearValues[2] = {};
    clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};
    renderPassInfo.clearValueCount = 2;
    renderPassInfo.pClearValues = clearValues;
    
    vkCmdBeginRenderPass(g_commandBuffers[imageIndex], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    
    vkCmdBindPipeline(g_commandBuffers[imageIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, g_textureResourceQuadPipeline);
    
    vkCmdBindDescriptorSets(g_commandBuffers[imageIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, 
                            g_textureResourceQuadPipelineLayout, 0, 1, &g_textureResourceQuadDescriptorSet, 0, nullptr);
    
    VkBuffer vertexBuffers[] = {g_textureResourceQuadVertexBuffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(g_commandBuffers[imageIndex], 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(g_commandBuffers[imageIndex], g_textureResourceQuadIndexBuffer, 0, VK_INDEX_TYPE_UINT16);
    
    vkCmdDrawIndexed(g_commandBuffers[imageIndex], 6, 1, 0, 0, 0);
    
    vkCmdEndRenderPass(g_commandBuffers[imageIndex]);
    vkEndCommandBuffer(g_commandBuffers[imageIndex]);
    
    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    
    VkSemaphore waitSemaphores[] = {g_imageAvailableSemaphore};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &g_commandBuffers[imageIndex];
    
    VkSemaphore signalSemaphores[] = {g_renderFinishedSemaphore};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;
    
    vkQueueSubmit(g_graphicsQueue, 1, &submitInfo, g_inFlightFence);
    
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;
    
    VkSwapchainKHR swapChains[] = {g_swapchain};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;
    
    vkQueuePresentKHR(g_graphicsQueue, &presentInfo);
}

extern "C" void heidic_cleanup_renderer_texture_quad() {
    if (g_textureResourceQuadPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(g_device, g_textureResourceQuadPipeline, nullptr);
        g_textureResourceQuadPipeline = VK_NULL_HANDLE;
    }
    if (g_textureResourceQuadPipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(g_device, g_textureResourceQuadPipelineLayout, nullptr);
        g_textureResourceQuadPipelineLayout = VK_NULL_HANDLE;
    }
    if (g_textureResourceQuadDescriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(g_device, g_textureResourceQuadDescriptorPool, nullptr);
        g_textureResourceQuadDescriptorPool = VK_NULL_HANDLE;
    }
    if (g_textureResourceQuadDescriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(g_device, g_textureResourceQuadDescriptorSetLayout, nullptr);
        g_textureResourceQuadDescriptorSetLayout = VK_NULL_HANDLE;
    }
    if (g_textureResourceQuadVertexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(g_device, g_textureResourceQuadVertexBuffer, nullptr);
        g_textureResourceQuadVertexBuffer = VK_NULL_HANDLE;
    }
    if (g_textureResourceQuadVertexBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(g_device, g_textureResourceQuadVertexBufferMemory, nullptr);
        g_textureResourceQuadVertexBufferMemory = VK_NULL_HANDLE;
    }
    if (g_textureResourceQuadIndexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(g_device, g_textureResourceQuadIndexBuffer, nullptr);
        g_textureResourceQuadIndexBuffer = VK_NULL_HANDLE;
    }
    if (g_textureResourceQuadIndexBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(g_device, g_textureResourceQuadIndexBufferMemory, nullptr);
        g_textureResourceQuadIndexBufferMemory = VK_NULL_HANDLE;
    }
    if (g_textureResourceQuadVertShaderModule != VK_NULL_HANDLE) {
        vkDestroyShaderModule(g_device, g_textureResourceQuadVertShaderModule, nullptr);
        g_textureResourceQuadVertShaderModule = VK_NULL_HANDLE;
    }
    if (g_textureResourceQuadFragShaderModule != VK_NULL_HANDLE) {
        vkDestroyShaderModule(g_device, g_textureResourceQuadFragShaderModule, nullptr);
        g_textureResourceQuadFragShaderModule = VK_NULL_HANDLE;
    }
    g_textureResourceQuad.reset();
    g_textureResourceQuadInitialized = false;
}

// ============================================================================
// OBJ MESH RENDERING FUNCTIONS (using MeshResource)
// ============================================================================

// OBJ mesh renderer state
static std::unique_ptr<MeshResource> g_objMeshResource = nullptr;
static std::unique_ptr<TextureResource> g_objMeshTexture = nullptr; // Optional texture for OBJ
static VkPipeline g_objMeshPipeline = VK_NULL_HANDLE;
static VkPipelineLayout g_objMeshPipelineLayout = VK_NULL_HANDLE;
static VkDescriptorSetLayout g_objMeshDescriptorSetLayout = VK_NULL_HANDLE;
static VkDescriptorPool g_objMeshDescriptorPool = VK_NULL_HANDLE;
static VkDescriptorSet g_objMeshDescriptorSet = VK_NULL_HANDLE;
static VkBuffer g_objMeshUniformBuffer = VK_NULL_HANDLE;
static VkDeviceMemory g_objMeshUniformBufferMemory = VK_NULL_HANDLE;
static VkShaderModule g_objMeshVertShaderModule = VK_NULL_HANDLE;
static VkShaderModule g_objMeshFragShaderModule = VK_NULL_HANDLE;
static VkImage g_objMeshDummyTexture = VK_NULL_HANDLE;
static VkImageView g_objMeshDummyTextureView = VK_NULL_HANDLE;
static VkSampler g_objMeshDummySampler = VK_NULL_HANDLE;
static VkDeviceMemory g_objMeshDummyTextureMemory = VK_NULL_HANDLE;
static bool g_objMeshInitialized = false;
static float g_objMeshRotationAngle = 0.0f;
static std::chrono::high_resolution_clock::time_point g_objMeshLastTime = std::chrono::high_resolution_clock::now();

// Initialize OBJ mesh renderer
extern "C" int heidic_init_renderer_obj_mesh(GLFWwindow* window, const char* objPath, const char* texturePath) {
    if (g_device == VK_NULL_HANDLE) {
        // Initialize Vulkan if not already done
        if (heidic_init_renderer(window) == 0) {
            return 0;
        }
    } else {
        // Vulkan already initialized, just destroy default triangle pipeline
        if (g_pipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(g_device, g_pipeline, nullptr);
            g_pipeline = VK_NULL_HANDLE;
        }
    }
    
    // Load OBJ mesh using MeshResource
    try {
        std::cout << "[EDEN] Loading OBJ mesh: " << objPath << std::endl;
        g_objMeshResource = std::make_unique<MeshResource>(objPath);
        std::cout << "[EDEN] OBJ mesh loaded successfully!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[EDEN] ERROR: Failed to load OBJ mesh: " << e.what() << std::endl;
        return 0;
    }
    
    // Load shaders
    std::vector<char> vertShaderCode, fragShaderCode;
    try {
        vertShaderCode = readFile("mesh.vert.spv");
        fragShaderCode = readFile("mesh.frag.spv");
        std::cout << "[EDEN] Loaded mesh shaders successfully" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[EDEN] ERROR: Could not find mesh shader files (mesh.vert.spv, mesh.frag.spv)!" << std::endl;
        std::cerr << "[EDEN] Error: " << e.what() << std::endl;
        return 0;
    }
    
    // Create shader modules
    VkShaderModuleCreateInfo vertCreateInfo = {};
    vertCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    vertCreateInfo.codeSize = vertShaderCode.size();
    vertCreateInfo.pCode = reinterpret_cast<const uint32_t*>(vertShaderCode.data());
    
    if (vkCreateShaderModule(g_device, &vertCreateInfo, nullptr, &g_objMeshVertShaderModule) != VK_SUCCESS) {
        std::cerr << "[EDEN] ERROR: Failed to create vertex shader module!" << std::endl;
        return 0;
    }
    
    VkShaderModuleCreateInfo fragCreateInfo = {};
    fragCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    fragCreateInfo.codeSize = fragShaderCode.size();
    fragCreateInfo.pCode = reinterpret_cast<const uint32_t*>(fragShaderCode.data());
    
    if (vkCreateShaderModule(g_device, &fragCreateInfo, nullptr, &g_objMeshFragShaderModule) != VK_SUCCESS) {
        std::cerr << "[EDEN] ERROR: Failed to create fragment shader module!" << std::endl;
        vkDestroyShaderModule(g_device, g_objMeshVertShaderModule, nullptr);
        return 0;
    }
    
    // Create descriptor set layout (UBO + texture sampler)
    VkDescriptorSetLayoutBinding uboLayoutBinding = {};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    
    VkDescriptorSetLayoutBinding samplerLayoutBinding = {};
    samplerLayoutBinding.binding = 1;
    samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBinding.descriptorCount = 1;
    samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    
    VkDescriptorSetLayoutBinding bindings[] = {uboLayoutBinding, samplerLayoutBinding};
    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 2;
    layoutInfo.pBindings = bindings;
    
    if (vkCreateDescriptorSetLayout(g_device, &layoutInfo, nullptr, &g_objMeshDescriptorSetLayout) != VK_SUCCESS) {
        std::cerr << "[EDEN] ERROR: Failed to create descriptor set layout!" << std::endl;
        vkDestroyShaderModule(g_device, g_objMeshVertShaderModule, nullptr);
        vkDestroyShaderModule(g_device, g_objMeshFragShaderModule, nullptr);
        return 0;
    }
    
    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &g_objMeshDescriptorSetLayout;
    
    if (vkCreatePipelineLayout(g_device, &pipelineLayoutInfo, nullptr, &g_objMeshPipelineLayout) != VK_SUCCESS) {
        std::cerr << "[EDEN] ERROR: Failed to create pipeline layout!" << std::endl;
        vkDestroyDescriptorSetLayout(g_device, g_objMeshDescriptorSetLayout, nullptr);
        vkDestroyShaderModule(g_device, g_objMeshVertShaderModule, nullptr);
        vkDestroyShaderModule(g_device, g_objMeshFragShaderModule, nullptr);
        return 0;
    }
    
    // Create uniform buffer
    VkDeviceSize bufferSize = sizeof(UniformBufferObject);
    createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 g_objMeshUniformBuffer, g_objMeshUniformBufferMemory);
    
    // Load texture if provided, otherwise create dummy white texture
    if (texturePath != nullptr && strlen(texturePath) > 0) {
        try {
            std::cout << "[EDEN] Loading texture for OBJ mesh: " << texturePath << std::endl;
            g_objMeshTexture = std::make_unique<TextureResource>(texturePath);
            std::cout << "[EDEN] Texture loaded successfully!" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "[EDEN] WARNING: Failed to load texture: " << e.what() << std::endl;
            std::cerr << "[EDEN] Falling back to dummy white texture" << std::endl;
            // Continue with dummy texture creation below
        }
    }
    
    // Create dummy texture if no texture was loaded
    if (!g_objMeshTexture) {
        uint32_t dummyData = 0xFFFFFFFF; // White RGBA
        createImage(1, 1, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL,
                    VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                    g_objMeshDummyTexture, g_objMeshDummyTextureMemory);
        
        // Transition image layout and upload data
        VkCommandBuffer commandBuffer = beginSingleTimeCommands();
        VkImageMemoryBarrier barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = g_objMeshDummyTexture;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        
        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &barrier);
        
        VkBufferImageCopy region = {};
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.layerCount = 1;
        region.imageExtent = {1, 1, 1};
        
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        createBuffer(4, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     stagingBuffer, stagingBufferMemory);
        
        void* data;
        vkMapMemory(g_device, stagingBufferMemory, 0, 4, 0, &data);
        memcpy(data, &dummyData, 4);
        vkUnmapMemory(g_device, stagingBufferMemory);
        
        vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, g_objMeshDummyTexture, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        
        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &barrier);
        
        endSingleTimeCommands(commandBuffer);
        
        vkDestroyBuffer(g_device, stagingBuffer, nullptr);
        vkFreeMemory(g_device, stagingBufferMemory, nullptr);
        
        // Create image view for dummy texture
        VkImageViewCreateInfo viewInfo = {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = g_objMeshDummyTexture;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;
        
        if (vkCreateImageView(g_device, &viewInfo, nullptr, &g_objMeshDummyTextureView) != VK_SUCCESS) {
            std::cerr << "[EDEN] ERROR: Failed to create dummy texture image view!" << std::endl;
            vkDestroyImage(g_device, g_objMeshDummyTexture, nullptr);
            vkFreeMemory(g_device, g_objMeshDummyTextureMemory, nullptr);
            vkDestroyBuffer(g_device, g_objMeshUniformBuffer, nullptr);
            vkFreeMemory(g_device, g_objMeshUniformBufferMemory, nullptr);
            vkDestroyPipelineLayout(g_device, g_objMeshPipelineLayout, nullptr);
            vkDestroyDescriptorSetLayout(g_device, g_objMeshDescriptorSetLayout, nullptr);
            vkDestroyShaderModule(g_device, g_objMeshVertShaderModule, nullptr);
            vkDestroyShaderModule(g_device, g_objMeshFragShaderModule, nullptr);
            return 0;
        }
        
        // Create sampler for dummy texture
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
        samplerInfo.maxLod = 0.0f;
        
        if (vkCreateSampler(g_device, &samplerInfo, nullptr, &g_objMeshDummySampler) != VK_SUCCESS) {
            std::cerr << "[EDEN] ERROR: Failed to create sampler!" << std::endl;
            vkDestroyImageView(g_device, g_objMeshDummyTextureView, nullptr);
            vkDestroyImage(g_device, g_objMeshDummyTexture, nullptr);
            vkFreeMemory(g_device, g_objMeshDummyTextureMemory, nullptr);
            vkDestroyBuffer(g_device, g_objMeshUniformBuffer, nullptr);
            vkFreeMemory(g_device, g_objMeshUniformBufferMemory, nullptr);
            vkDestroyPipelineLayout(g_device, g_objMeshPipelineLayout, nullptr);
            vkDestroyDescriptorSetLayout(g_device, g_objMeshDescriptorSetLayout, nullptr);
            vkDestroyShaderModule(g_device, g_objMeshVertShaderModule, nullptr);
            vkDestroyShaderModule(g_device, g_objMeshFragShaderModule, nullptr);
            return 0;
        }
    }
    
    // Create descriptor pool
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
    
    if (vkCreateDescriptorPool(g_device, &poolInfo, nullptr, &g_objMeshDescriptorPool) != VK_SUCCESS) {
        std::cerr << "[EDEN] ERROR: Failed to create descriptor pool!" << std::endl;
        vkDestroySampler(g_device, g_objMeshDummySampler, nullptr);
        vkDestroyImageView(g_device, g_objMeshDummyTextureView, nullptr);
        vkDestroyImage(g_device, g_objMeshDummyTexture, nullptr);
        vkFreeMemory(g_device, g_objMeshDummyTextureMemory, nullptr);
        vkDestroyBuffer(g_device, g_objMeshUniformBuffer, nullptr);
        vkFreeMemory(g_device, g_objMeshUniformBufferMemory, nullptr);
        vkDestroyPipelineLayout(g_device, g_objMeshPipelineLayout, nullptr);
        vkDestroyDescriptorSetLayout(g_device, g_objMeshDescriptorSetLayout, nullptr);
        vkDestroyShaderModule(g_device, g_objMeshVertShaderModule, nullptr);
        vkDestroyShaderModule(g_device, g_objMeshFragShaderModule, nullptr);
        return 0;
    }
    
    // Allocate descriptor set
    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = g_objMeshDescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &g_objMeshDescriptorSetLayout;
    
    if (vkAllocateDescriptorSets(g_device, &allocInfo, &g_objMeshDescriptorSet) != VK_SUCCESS) {
        std::cerr << "[EDEN] ERROR: Failed to allocate descriptor set!" << std::endl;
        vkDestroyDescriptorPool(g_device, g_objMeshDescriptorPool, nullptr);
        vkDestroySampler(g_device, g_objMeshDummySampler, nullptr);
        vkDestroyImageView(g_device, g_objMeshDummyTextureView, nullptr);
        vkDestroyImage(g_device, g_objMeshDummyTexture, nullptr);
        vkFreeMemory(g_device, g_objMeshDummyTextureMemory, nullptr);
        vkDestroyBuffer(g_device, g_objMeshUniformBuffer, nullptr);
        vkFreeMemory(g_device, g_objMeshUniformBufferMemory, nullptr);
        vkDestroyPipelineLayout(g_device, g_objMeshPipelineLayout, nullptr);
        vkDestroyDescriptorSetLayout(g_device, g_objMeshDescriptorSetLayout, nullptr);
        vkDestroyShaderModule(g_device, g_objMeshVertShaderModule, nullptr);
        vkDestroyShaderModule(g_device, g_objMeshFragShaderModule, nullptr);
        return 0;
    }
    
    // Update descriptor set
    VkDescriptorBufferInfo bufferInfo = {};
    bufferInfo.buffer = g_objMeshUniformBuffer;
    bufferInfo.offset = 0;
    bufferInfo.range = sizeof(UniformBufferObject);
    
    // Use TextureResource if available, otherwise use dummy texture
    VkDescriptorImageInfo imageInfo = {};
    if (g_objMeshTexture) {
        imageInfo = g_objMeshTexture->getDescriptorImageInfo();
    } else {
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = g_objMeshDummyTextureView;
        imageInfo.sampler = g_objMeshDummySampler;
    }
    
    VkWriteDescriptorSet descriptorWrites[2] = {};
    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = g_objMeshDescriptorSet;
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pBufferInfo = &bufferInfo;
    
    descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[1].dstSet = g_objMeshDescriptorSet;
    descriptorWrites[1].dstBinding = 1;
    descriptorWrites[1].dstArrayElement = 0;
    descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].pImageInfo = &imageInfo;
    
    vkUpdateDescriptorSets(g_device, 2, descriptorWrites, 0, nullptr);
    
    // Create graphics pipeline
    VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = g_objMeshVertShaderModule;
    vertShaderStageInfo.pName = "main";
    
    VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = g_objMeshFragShaderModule;
    fragShaderStageInfo.pName = "main";
    
    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};
    
    // Vertex input (MeshVertex: pos[3], normal[3], uv[2])
    VkVertexInputBindingDescription bindingDescription = {};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(MeshVertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    
    VkVertexInputAttributeDescription attributeDescriptions[3] = {};
    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[0].offset = offsetof(MeshVertex, pos);
    
    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[1].offset = offsetof(MeshVertex, normal);
    
    attributeDescriptions[2].binding = 0;
    attributeDescriptions[2].location = 2;
    attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[2].offset = offsetof(MeshVertex, uv);
    
    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = 3;
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions;
    
    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;
    
    VkViewport viewport = {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)g_swapchainExtent.width;
    viewport.height = (float)g_swapchainExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    
    VkRect2D scissor = {};
    scissor.offset = {0, 0};
    scissor.extent = g_swapchainExtent;
    
    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;
    
    VkPipelineRasterizationStateCreateInfo rasterizer = {};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;
    
    VkPipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    
    VkPipelineDepthStencilStateCreateInfo depthStencil = {};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;
    
    VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;
    
    VkPipelineColorBlendStateCreateInfo colorBlending = {};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    
    VkGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.layout = g_objMeshPipelineLayout;
    pipelineInfo.renderPass = g_renderPass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    
    if (vkCreateGraphicsPipelines(g_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &g_objMeshPipeline) != VK_SUCCESS) {
        std::cerr << "[EDEN] ERROR: Failed to create graphics pipeline!" << std::endl;
        vkDestroyDescriptorPool(g_device, g_objMeshDescriptorPool, nullptr);
        vkDestroySampler(g_device, g_objMeshDummySampler, nullptr);
        vkDestroyImageView(g_device, g_objMeshDummyTextureView, nullptr);
        vkDestroyImage(g_device, g_objMeshDummyTexture, nullptr);
        vkFreeMemory(g_device, g_objMeshDummyTextureMemory, nullptr);
        vkDestroyBuffer(g_device, g_objMeshUniformBuffer, nullptr);
        vkFreeMemory(g_device, g_objMeshUniformBufferMemory, nullptr);
        vkDestroyPipelineLayout(g_device, g_objMeshPipelineLayout, nullptr);
        vkDestroyDescriptorSetLayout(g_device, g_objMeshDescriptorSetLayout, nullptr);
        vkDestroyShaderModule(g_device, g_objMeshVertShaderModule, nullptr);
        vkDestroyShaderModule(g_device, g_objMeshFragShaderModule, nullptr);
        return 0;
    }
    
    g_objMeshInitialized = true;
    std::cout << "[EDEN] OBJ mesh renderer initialized successfully!" << std::endl;
    return 1;
}

// Render OBJ mesh
extern "C" void heidic_render_obj_mesh(GLFWwindow* window) {
    if (g_device == VK_NULL_HANDLE || g_swapchain == VK_NULL_HANDLE || g_objMeshPipeline == VK_NULL_HANDLE || !g_objMeshResource) {
        return;
    }
    
    // Update rotation angle
    auto currentTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(currentTime - g_objMeshLastTime);
    float deltaTime = duration.count() / 1000000.0f;
    g_objMeshLastTime = currentTime;
    
    g_objMeshRotationAngle += 1.0f * deltaTime;
    if (g_objMeshRotationAngle > 2.0f * 3.14159f) {
        g_objMeshRotationAngle -= 2.0f * 3.14159f;
    }
    
    // Wait for previous frame
    vkWaitForFences(g_device, 1, &g_inFlightFence, VK_TRUE, UINT64_MAX);
    vkResetFences(g_device, 1, &g_inFlightFence);
    
    // Acquire next image
    uint32_t imageIndex;
    vkAcquireNextImageKHR(g_device, g_swapchain, UINT64_MAX, g_imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
    
    // Reset command buffer
    vkResetCommandBuffer(g_commandBuffers[imageIndex], 0);
    
    // Begin command buffer
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(g_commandBuffers[imageIndex], &beginInfo);
    
    // Begin render pass
    VkRenderPassBeginInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = g_renderPass;
    renderPassInfo.framebuffer = g_framebuffers[imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = g_swapchainExtent;
    
    std::array<VkClearValue, 2> clearValues = {};
    clearValues[0].color = {{0.1f, 0.1f, 0.1f, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};
    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();
    
    vkCmdBeginRenderPass(g_commandBuffers[imageIndex], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    
    // Bind pipeline
    vkCmdBindPipeline(g_commandBuffers[imageIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, g_objMeshPipeline);
    
    // Update uniform buffer
    UniformBufferObject ubo = {};
    
    // Model matrix - rotate the mesh
    Vec3 axis = {0.0f, 1.0f, 0.0f};
    ubo.model = mat4_rotate(axis, g_objMeshRotationAngle);
    
    // View matrix - look at mesh from above and to the side
    Vec3 eye = {3.0f, 3.0f, 3.0f};
    Vec3 center = {0.0f, 0.0f, 0.0f};
    Vec3 up = {0.0f, 1.0f, 0.0f};
    ubo.view = mat4_lookat(eye, center, up);
    
    // Projection matrix
    float fov = 45.0f * 3.14159f / 180.0f;
    float aspect = (float)g_swapchainExtent.width / (float)g_swapchainExtent.height;
    float nearPlane = 0.1f;
    float farPlane = 100.0f;
    ubo.proj = mat4_perspective(fov, aspect, nearPlane, farPlane);
    
    // Vulkan clip space has inverted Y and half Z
    ubo.proj[1][1] *= -1.0f;
    
    // Update uniform buffer
    void* data;
    vkMapMemory(g_device, g_objMeshUniformBufferMemory, 0, sizeof(ubo), 0, &data);
    memcpy(data, &ubo, sizeof(ubo));
    vkUnmapMemory(g_device, g_objMeshUniformBufferMemory);
    
    // Bind descriptor set
    vkCmdBindDescriptorSets(g_commandBuffers[imageIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, g_objMeshPipelineLayout, 0, 1, &g_objMeshDescriptorSet, 0, nullptr);
    
    // Bind vertex buffer (from MeshResource)
    VkBuffer vertexBuffer = g_objMeshResource->getVertexBuffer();
    VkBuffer indexBuffer = g_objMeshResource->getIndexBuffer();
    uint32_t indexCount = g_objMeshResource->getIndexCount();
    
    VkBuffer vertexBuffers[] = {vertexBuffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(g_commandBuffers[imageIndex], 0, 1, vertexBuffers, offsets);
    
    // Bind index buffer
    vkCmdBindIndexBuffer(g_commandBuffers[imageIndex], indexBuffer, 0, VK_INDEX_TYPE_UINT32);
    
    // Draw mesh using indexed drawing
    vkCmdDrawIndexed(g_commandBuffers[imageIndex], indexCount, 1, 0, 0, 0);
    
    vkCmdEndRenderPass(g_commandBuffers[imageIndex]);
    vkEndCommandBuffer(g_commandBuffers[imageIndex]);
    
    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    
    VkSemaphore waitSemaphores[] = {g_imageAvailableSemaphore};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &g_commandBuffers[imageIndex];
    
    VkSemaphore signalSemaphores[] = {g_renderFinishedSemaphore};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;
    
    vkQueueSubmit(g_graphicsQueue, 1, &submitInfo, g_inFlightFence);
    
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;
    
    VkSwapchainKHR swapChains[] = {g_swapchain};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;
    
    vkQueuePresentKHR(g_graphicsQueue, &presentInfo);
}

// Cleanup OBJ mesh renderer
extern "C" void heidic_cleanup_renderer_obj_mesh() {
    if (g_objMeshPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(g_device, g_objMeshPipeline, nullptr);
        g_objMeshPipeline = VK_NULL_HANDLE;
    }
    if (g_objMeshPipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(g_device, g_objMeshPipelineLayout, nullptr);
        g_objMeshPipelineLayout = VK_NULL_HANDLE;
    }
    if (g_objMeshDescriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(g_device, g_objMeshDescriptorPool, nullptr);
        g_objMeshDescriptorPool = VK_NULL_HANDLE;
    }
    if (g_objMeshDescriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(g_device, g_objMeshDescriptorSetLayout, nullptr);
        g_objMeshDescriptorSetLayout = VK_NULL_HANDLE;
    }
    if (g_objMeshUniformBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(g_device, g_objMeshUniformBuffer, nullptr);
        g_objMeshUniformBuffer = VK_NULL_HANDLE;
    }
    if (g_objMeshUniformBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(g_device, g_objMeshUniformBufferMemory, nullptr);
        g_objMeshUniformBufferMemory = VK_NULL_HANDLE;
    }
    // Cleanup texture (TextureResource handles its own cleanup via RAII)
    if (g_objMeshTexture) {
        g_objMeshTexture.reset();
    }
    // Cleanup dummy texture resources (only created if TextureResource wasn't used)
    if (g_objMeshDummySampler != VK_NULL_HANDLE) {
        vkDestroySampler(g_device, g_objMeshDummySampler, nullptr);
        g_objMeshDummySampler = VK_NULL_HANDLE;
    }
    if (g_objMeshDummyTextureView != VK_NULL_HANDLE) {
        vkDestroyImageView(g_device, g_objMeshDummyTextureView, nullptr);
        g_objMeshDummyTextureView = VK_NULL_HANDLE;
    }
    if (g_objMeshDummyTexture != VK_NULL_HANDLE) {
        vkDestroyImage(g_device, g_objMeshDummyTexture, nullptr);
        g_objMeshDummyTexture = VK_NULL_HANDLE;
    }
    if (g_objMeshDummyTextureMemory != VK_NULL_HANDLE) {
        vkFreeMemory(g_device, g_objMeshDummyTextureMemory, nullptr);
        g_objMeshDummyTextureMemory = VK_NULL_HANDLE;
    }
    if (g_objMeshVertShaderModule != VK_NULL_HANDLE) {
        vkDestroyShaderModule(g_device, g_objMeshVertShaderModule, nullptr);
        g_objMeshVertShaderModule = VK_NULL_HANDLE;
    }
    if (g_objMeshFragShaderModule != VK_NULL_HANDLE) {
        vkDestroyShaderModule(g_device, g_objMeshFragShaderModule, nullptr);
        g_objMeshFragShaderModule = VK_NULL_HANDLE;
    }
    g_objMeshResource.reset();
    g_objMeshInitialized = false;
}

