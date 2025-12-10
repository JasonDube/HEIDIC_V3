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

// Global Vulkan state
static VkInstance g_instance = VK_NULL_HANDLE;
static VkPhysicalDevice g_physicalDevice = VK_NULL_HANDLE;
static VkSurfaceKHR g_surface = VK_NULL_HANDLE;
static VkDevice g_device = VK_NULL_HANDLE;
static VkSwapchainKHR g_swapchain = VK_NULL_HANDLE;
static VkRenderPass g_renderPass = VK_NULL_HANDLE;
static VkPipeline g_pipeline = VK_NULL_HANDLE;
static VkPipelineLayout g_pipelineLayout = VK_NULL_HANDLE;
static std::vector<VkFramebuffer> g_framebuffers;
static VkCommandPool g_commandPool = VK_NULL_HANDLE;
static std::vector<VkCommandBuffer> g_commandBuffers;
static VkQueue g_graphicsQueue = VK_NULL_HANDLE;
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
    auto vertShaderCode = readFile("vert_3d.spv");
    auto fragShaderCode = readFile("frag_3d.spv");
    
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
    vertexInputInfo.vertexBindingDescriptionCount = 0;
    vertexInputInfo.vertexAttributeDescriptionCount = 0;
    
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
    
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &g_descriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    
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
    
    // Draw triangle (3 vertices - hardcoded in shader)
    vkCmdDraw(g_commandBuffers[imageIndex], 3, 1, 0, 0);
    
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
    bool isTriangleShader = (shaderPathStr.find("vert_3d.spv") != std::string::npos || 
                             shaderPathStr.find("frag_3d.spv") != std::string::npos);
    bool isCubeShader = (shaderPathStr.find("vert_cube.spv") != std::string::npos || 
                         shaderPathStr.find("frag_cube.spv") != std::string::npos);
    
    if (!isTriangleShader && !isCubeShader) {
        std::cerr << "[Shader Hot-Reload] WARNING: Unknown shader path: " << shader_path << std::endl;
        return;
    }
    
    // Determine which shader stage and pipeline we're reloading
    bool isVertex = (shaderPathStr.find("vert") != std::string::npos);
    bool isFragment = (shaderPathStr.find("frag") != std::string::npos);
    
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
    
    // Read new shader code
    std::vector<char> shaderCode;
    try {
        shaderCode = readFile(shaderPathStr);
    } catch (const std::exception& e) {
        std::cerr << "[Shader Hot-Reload] ERROR: Failed to read shader file: " << e.what() << std::endl;
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
        VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 0;
        vertexInputInfo.vertexAttributeDescriptionCount = 0;
        
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
static VkPipeline g_cubePipeline = VK_NULL_HANDLE;
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
    float near = 0.1f;
    float far = 100.0f;
    ubo.proj = mat4_perspective(fov, aspect, near, far);
    
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

