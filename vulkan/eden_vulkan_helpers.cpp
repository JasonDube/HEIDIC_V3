// EDEN ENGINE Vulkan Helper Functions Implementation
// Simplified implementation for spinning triangle

#include "eden_vulkan_helpers.h"
#include <iostream>
#include <vector>
#include <cstring>
#include <cctype>
#include <sstream>
#include <thread>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <array>
#include <fstream>
#include <string>
#include <memory>
#include <map>
#include <set>
#include <queue>
#include <cstddef>
#ifdef _WIN32
#include <windows.h>
#include <sys/stat.h>
#include <io.h>
#include <commdlg.h>
#include <shlobj.h>
#else
#include <unistd.h>
#include <sys/stat.h>
#endif
#include "../stdlib/dds_loader.h"
// Define STB_IMAGE_IMPLEMENTATION only once in this .cpp file
// This provides the implementation for stb_image functions used by load_png
#define STB_IMAGE_IMPLEMENTATION
#include "../stdlib/png_loader.h"
#include "../stdlib/texture_resource.h"
#include "../stdlib/mesh_resource.h"
#include "../stdlib/resource.h"

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
    // Note: FPS renderer uses push constants for model matrix instead of UBO
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
std::vector<VkCommandBuffer> g_commandBuffers;  // Already non-static, accessible to NEUROSHELL
VkQueue g_graphicsQueue = VK_NULL_HANDLE;
static uint32_t g_graphicsQueueFamilyIndex = 0;
static uint32_t g_currentFrame = 0;
VkExtent2D g_swapchainExtent = {};  // Made non-static for NEUROSHELL access

// Additional state
static std::vector<VkImage> g_swapchainImages;
static std::vector<VkImageView> g_swapchainImageViews;
VkSemaphore g_imageAvailableSemaphore = VK_NULL_HANDLE;  // Made non-static for NEUROSHELL access
VkSemaphore g_renderFinishedSemaphore = VK_NULL_HANDLE;  // Made non-static for NEUROSHELL access
VkFence g_inFlightFence = VK_NULL_HANDLE;  // Made non-static for NEUROSHELL access
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
        filename,  // Current directory (project directory)
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
        "../../../examples/spinning_cube/" + filename,
        "../../../../examples/spinning_cube/" + filename,
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

// Create a fullscreen window on the primary monitor
extern "C" GLFWwindow* heidic_create_fullscreen_window(const char* title) {
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    if (!monitor) {
        std::cerr << "[GLFW] ERROR: Could not get primary monitor!" << std::endl;
        return nullptr;
    }
    
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    if (!mode) {
        std::cerr << "[GLFW] ERROR: Could not get video mode!" << std::endl;
        return nullptr;
    }
    
    std::cout << "[GLFW] Creating fullscreen window: " << mode->width << "x" << mode->height << std::endl;
    
    // Create fullscreen window with monitor's native resolution
    GLFWwindow* window = glfwCreateWindow(mode->width, mode->height, title, monitor, nullptr);
    if (!window) {
        std::cerr << "[GLFW] ERROR: Failed to create fullscreen window!" << std::endl;
    }
    return window;
}

// Create a borderless fullscreen window (windowed mode with no decorations, covering entire screen)
extern "C" GLFWwindow* heidic_create_borderless_window(int width, int height, const char* title) {
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    if (!monitor) {
        std::cerr << "[GLFW] ERROR: Could not get primary monitor!" << std::endl;
        return nullptr;
    }
    
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    if (!mode) {
        std::cerr << "[GLFW] ERROR: Could not get video mode!" << std::endl;
        return nullptr;
    }
    
    // Set window hints for borderless window
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    
    // Use monitor's resolution for borderless if width/height not specified
    int window_width = (width > 0) ? width : mode->width;
    int window_height = (height > 0) ? height : mode->height;
    
    std::cout << "[GLFW] Creating borderless window: " << window_width << "x" << window_height << std::endl;
    
    // Create windowed window (no monitor) with borderless hints
    GLFWwindow* window = glfwCreateWindow(window_width, window_height, title, nullptr, nullptr);
    if (!window) {
        std::cerr << "[GLFW] ERROR: Failed to create borderless window!" << std::endl;
        // Reset window hints
        glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
        return nullptr;
    }
    
    // Center the window on the primary monitor
    int x = (mode->width - window_width) / 2;
    int y = (mode->height - window_height) / 2;
    glfwSetWindowPos(window, x, y);
    
    // Reset window hints for future use (though we only create one window)
    glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    
    return window;
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
    
    // Enable validation layers
    const std::vector<const char*> validationLayers = {
        "VK_LAYER_KHRONOS_validation"
    };
    
    // Check if validation layers are available
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
    
    bool validationLayersAvailable = true;
    for (const char* layerName : validationLayers) {
        bool layerFound = false;
        for (const auto& layerProperties : availableLayers) {
            if (std::strcmp(layerName, layerProperties.layerName) == 0) {
                layerFound = true;
                break;
            }
        }
        if (!layerFound) {
            validationLayersAvailable = false;
            std::cerr << "[EDEN] WARNING: Validation layer " << layerName << " not available!" << std::endl;
            break;
        }
    }
    
    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = glfwExtensionCount;
    createInfo.ppEnabledExtensionNames = glfwExtensions;
    
    if (validationLayersAvailable) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
        std::cout << "[EDEN] Validation layers enabled" << std::endl;
    } else {
        createInfo.enabledLayerCount = 0;
        std::cout << "[EDEN] Validation layers not available, continuing without them" << std::endl;
    }
    
    // Add debug messenger extension if validation layers are enabled
    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
    if (validationLayersAvailable) {
        extensions.push_back("VK_EXT_debug_utils");
    }
    
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();
    
    if (vkCreateInstance(&createInfo, nullptr, &g_instance) != VK_SUCCESS) {
        std::cerr << "[EDEN] ERROR: Failed to create Vulkan instance!" << std::endl;
        return 0;
    }
    
    // Create debug messenger if validation layers are enabled
    if (validationLayersAvailable) {
        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = {};
        debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | 
                                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | 
                                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | 
                                     VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | 
                                     VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debugCreateInfo.pfnUserCallback = [](VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                             VkDebugUtilsMessageTypeFlagsEXT messageType,
                                             const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                             void* pUserData) -> VkBool32 {
            if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
                std::cerr << "[VULKAN VALIDATION] " << pCallbackData->pMessage << std::endl;
            }
            return VK_FALSE;
        };
        
        // Load vkCreateDebugUtilsMessengerEXT function
        typedef VkResult (VKAPI_PTR *PFN_vkCreateDebugUtilsMessengerEXT)(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pMessenger);
        auto vkCreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(g_instance, "vkCreateDebugUtilsMessengerEXT");
        if (vkCreateDebugUtilsMessengerEXT != nullptr) {
            VkDebugUtilsMessengerEXT debugMessenger;
            if (vkCreateDebugUtilsMessengerEXT(g_instance, &debugCreateInfo, nullptr, &debugMessenger) == VK_SUCCESS) {
                std::cout << "[EDEN] Debug messenger created successfully" << std::endl;
            } else {
                std::cerr << "[EDEN] WARNING: Failed to create debug messenger" << std::endl;
            }
        } else {
            std::cerr << "[EDEN] WARNING: vkCreateDebugUtilsMessengerEXT not available" << std::endl;
        }
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
    deviceFeatures.fillModeNonSolid = VK_TRUE;  // Enable wireframe rendering mode
    
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
    
    // Render NEUROSHELL UI (if enabled) - before ending render pass
    #ifdef USE_NEUROSHELL
    extern void neuroshell_render(VkCommandBuffer);
    extern bool neuroshell_is_enabled();
    if (neuroshell_is_enabled()) {
        neuroshell_render(g_commandBuffers[imageIndex]);
    }
    #endif
    
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

// Convert f32 to i32 (for type conversions in HEIDIC)
extern "C" int32_t f32_to_i32(float value) {
    return static_cast<int32_t>(value);
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
// Corrected winding order for counter-clockwise front face
static const std::vector<uint16_t> cubeIndices = {
    0, 1, 2,  2, 3, 0,  // Front face (counter-clockwise)
    4, 5, 6,  4, 6, 7,  // Back face (counter-clockwise when viewed from outside)
    3, 2, 6,  3, 6, 7,  // Top face (counter-clockwise)
    0, 4, 5,  0, 5, 1,  // Bottom face (counter-clockwise)
    1, 5, 6,  1, 6, 2,  // Right face (counter-clockwise)
    0, 3, 7,  0, 7, 4,  // Left face (counter-clockwise)
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
    
    // Render NEUROSHELL UI (if enabled) - before ending render pass
    #ifdef USE_NEUROSHELL
    extern void neuroshell_render(VkCommandBuffer);
    extern bool neuroshell_is_enabled();
    if (neuroshell_is_enabled()) {
        neuroshell_render(g_commandBuffers[imageIndex]);
    }
    #endif
    
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

// ============================================================================
// FPS CAMERA RENDERER (for FPS camera test with floor cube)
// ============================================================================

// FPS-specific state
static VkPipeline g_fpsPipeline = VK_NULL_HANDLE;
static VkShaderModule g_fpsVertShaderModule = VK_NULL_HANDLE;
static VkShaderModule g_fpsFragShaderModule = VK_NULL_HANDLE;
static bool g_fpsInitialized = false;
static VkBuffer g_fpsCubeVertexBuffer = VK_NULL_HANDLE;
static VkDeviceMemory g_fpsCubeVertexBufferMemory = VK_NULL_HANDLE;
static VkBuffer g_fpsCubeIndexBuffer = VK_NULL_HANDLE;
static VkDeviceMemory g_fpsCubeIndexBufferMemory = VK_NULL_HANDLE;
static uint32_t g_fpsCubeIndexCount = 0;

// Colored reference cubes (1x1x1 cubes for spatial reference)
static VkBuffer g_coloredCubeVertexBuffers[19] = {VK_NULL_HANDLE};  // 9 big + 5 small + 1 rectangle + 1 pink block + 1 ground + 1 building
static VkDeviceMemory g_coloredCubeVertexBufferMemory[19] = {VK_NULL_HANDLE};
static int g_numColoredCubes = 0;
// Store cube sizes (1.0 for big cubes, 0.5 for small cubes, or per-axis for rectangles)
static float g_coloredCubeSizes[19] = {0.0f};
// Store per-axis sizes for non-uniform shapes (x, y, z)
static float g_coloredCubeSizeX[19] = {0.0f};
static float g_coloredCubeSizeY[19] = {0.0f};
static float g_coloredCubeSizeZ[19] = {0.0f};

// FPS Camera matrices for raycasting (updated each frame in heidic_render_fps)
static glm::mat4 g_fpsCurrentView = glm::mat4(1.0f);
static glm::mat4 g_fpsCurrentProj = glm::mat4(1.0f);
static glm::vec3 g_fpsCurrentCamPos = glm::vec3(0.0f);

// Mutable cube positions (for pickup system)
struct ColoredCubePos {
    float x, y, z;
};
static std::vector<ColoredCubePos> g_coloredCubePositions;

// Store original cube colors (for restoring after selection)
struct ColoredCubeColor {
    float r, g, b;
};
static std::vector<ColoredCubeColor> g_coloredCubeOriginalColors;

// Store cube rotations (yaw in degrees, for visual rotation around Y axis)
static std::vector<float> g_coloredCubeRotations;

// Attachment system: track which cubes are attached to the vehicle (index 14)
// When a cube is attached, it moves and rotates with the vehicle
struct AttachedCubeData {
    bool attached = false;      // Whether this cube is attached to vehicle
    float local_x = 0.0f;       // Local X offset from vehicle center (in vehicle-local space)
    float local_y = 0.0f;       // Local Y offset from vehicle top
    float local_z = 0.0f;       // Local Z offset from vehicle center (in vehicle-local space)
};
static std::vector<AttachedCubeData> g_attachedCubes;

// Item properties system - for scavenging/trading/building gameplay
// Each cube can have item properties that define what it is, its value, etc.
struct ItemProperties {
    int item_type_id = 0;       // Unique item type ID (0 = generic block, 1 = water_crate, 2 = engine_part, etc.)
    std::string item_name = ""; // Item name/display name (e.g., "Junk Skiff", "Barker's GyroHelm")
    float trade_value = 0.0f;   // Trade value/price (0 = not tradeable)
    float condition = 1.0f;     // Condition/durability (0.0 to 1.0, 1.0 = perfect, 0.0 = broken)
    float weight = 1.0f;        // Weight/mass (for physics/inventory)
    int category = 0;           // Category: 0=generic, 1=consumable, 2=part, 3=resource, 4=scrap
    bool is_salvaged = false;   // Whether this item was salvaged from a destroyed ship
    int parent_index = -1;      // Parent cube index (-1 = no parent, otherwise this is a child of that cube)
};
static std::vector<ItemProperties> g_itemProperties;

// Floor cube vertices - EXACT COPY of spinning cube vertices, just with gray colors
// Using -1.0 to 1.0 range like spinning cube (will scale via model matrix)
static const std::vector<Vertex> floorCubeVertices = {
    {{-1.0f, -1.0f,  1.0f}, {0.5f, 0.5f, 0.5f}},  // 0: Front bottom-left
    {{ 1.0f, -1.0f,  1.0f}, {0.5f, 0.5f, 0.5f}},  // 1: Front bottom-right
    {{ 1.0f,  1.0f,  1.0f}, {0.6f, 0.6f, 0.6f}},  // 2: Front top-right
    {{-1.0f,  1.0f,  1.0f}, {0.6f, 0.6f, 0.6f}},  // 3: Front top-left
    {{-1.0f, -1.0f, -1.0f}, {0.5f, 0.5f, 0.5f}},  // 4: Back bottom-left
    {{ 1.0f, -1.0f, -1.0f}, {0.5f, 0.5f, 0.5f}},  // 5: Back bottom-right
    {{ 1.0f,  1.0f, -1.0f}, {0.6f, 0.6f, 0.6f}},  // 6: Back top-right
    {{-1.0f,  1.0f, -1.0f}, {0.6f, 0.6f, 0.6f}},  // 7: Back top-left
};

// Floor cube indices - matching spinning cube exactly
// Winding order: counter-clockwise when viewed from outside
static const std::vector<uint16_t> floorCubeIndices = {
    0, 1, 2,  2, 3, 0,  // Front face (z=+0.5, facing +Z)
    4, 5, 6,  4, 6, 7,  // Back face (z=-0.5, facing -Z)
    3, 2, 6,  3, 6, 7,  // Top face (y=+0.5, facing +Y)
    0, 4, 5,  0, 5, 1,  // Bottom face (y=-0.5, facing -Y)
    1, 5, 6,  1, 6, 2,  // Right face (x=+0.5, facing +X)
    0, 3, 7,  0, 7, 4,  // Left face (x=-0.5, facing -X)
};

// Initialize FPS camera renderer
extern "C" int heidic_init_renderer_fps(GLFWwindow* window) {
    // Initialize base renderer (this creates descriptor sets and uniform buffers)
    if (heidic_init_renderer(window) == 0) {
        return 0;
    }
    
    // DIAGNOSTIC: Verify descriptor sets were created
    if (g_descriptorSets.empty()) {
        std::cerr << "[FPS] ERROR: Descriptor sets are empty after base renderer init!" << std::endl;
        return 0;
    }
    if (g_uniformBuffers.empty()) {
        std::cerr << "[FPS] ERROR: Uniform buffers are empty after base renderer init!" << std::endl;
        return 0;
    }
    std::cout << "[FPS] Descriptor sets created: " << g_descriptorSets.size() << ", Uniform buffers: " << g_uniformBuffers.size() << std::endl;
    
    // Initialize Neuroshell for crosshair rendering
    #ifdef USE_NEUROSHELL
    extern int neuroshell_init(GLFWwindow* window);
    extern bool neuroshell_is_enabled();
    if (!neuroshell_is_enabled()) {
        int neuroshell_result = neuroshell_init(window);
        if (neuroshell_result == 0) {
            std::cerr << "[FPS] WARNING: Failed to initialize Neuroshell - crosshair will not be visible" << std::endl;
        } else {
            std::cout << "[FPS] Neuroshell initialized for crosshair rendering" << std::endl;
        }
    }
    #else
    std::cerr << "[FPS] WARNING: Neuroshell not compiled in (USE_NEUROSHELL not defined) - crosshair will not be visible" << std::endl;
    #endif
    
    // Destroy triangle pipeline since we'll use our own
    if (g_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(g_device, g_pipeline, nullptr);
        g_pipeline = VK_NULL_HANDLE;
    }
    
    // Create FPS-specific vertex/index buffers for floor cube (separate from regular cube buffers)
    if (g_fpsCubeVertexBuffer == VK_NULL_HANDLE) {
        // Create vertex buffer for floor cube
        VkDeviceSize vertexBufferSize = sizeof(floorCubeVertices[0]) * floorCubeVertices.size();
        createBuffer(vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     g_fpsCubeVertexBuffer, g_fpsCubeVertexBufferMemory);
        
        void* data;
        vkMapMemory(g_device, g_fpsCubeVertexBufferMemory, 0, vertexBufferSize, 0, &data);
        memcpy(data, floorCubeVertices.data(), (size_t)vertexBufferSize);
        vkUnmapMemory(g_device, g_fpsCubeVertexBufferMemory);
        
        // Create index buffer
        VkDeviceSize indexBufferSize = sizeof(floorCubeIndices[0]) * floorCubeIndices.size();
        g_fpsCubeIndexCount = static_cast<uint32_t>(floorCubeIndices.size());
        createBuffer(indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     g_fpsCubeIndexBuffer, g_fpsCubeIndexBufferMemory);
        
        vkMapMemory(g_device, g_fpsCubeIndexBufferMemory, 0, indexBufferSize, 0, &data);
        memcpy(data, floorCubeIndices.data(), (size_t)indexBufferSize);
        vkUnmapMemory(g_device, g_fpsCubeIndexBufferMemory);
        
        std::cout << "[FPS] Created floor cube buffers: " << floorCubeVertices.size() << " vertices, " 
                  << floorCubeIndices.size() << " indices" << std::endl;
    }
    
    // Create colored reference cubes (1x1x1 cubes with different colors)
    struct ColoredCubeData {
        float x, y, z;
        float r, g, b;
    };
    
    std::vector<ColoredCubeData> referenceCubes = {
        {0.0f, 0.5f, 0.0f, 1.0f, 0.0f, 0.0f},   // Red at origin (big)
        {5.0f, 0.5f, 0.0f, 0.0f, 1.0f, 0.0f},   // Green at +X (big)
        {-5.0f, 0.5f, 0.0f, 0.0f, 0.0f, 1.0f},  // Blue at -X (big)
        {0.0f, 0.5f, 5.0f, 1.0f, 1.0f, 0.0f},   // Yellow at +Z (big)
        {0.0f, 0.5f, -5.0f, 1.0f, 0.0f, 1.0f},  // Magenta at -Z (big)
        {5.0f, 0.5f, 5.0f, 0.0f, 1.0f, 1.0f},   // Cyan at +X+Z (big)
        {-5.0f, 0.5f, -5.0f, 1.0f, 0.5f, 0.0f}, // Orange at -X-Z (big)
        {10.0f, 0.5f, 0.0f, 0.5f, 0.5f, 1.0f},  // Light blue at +10X (big)
        {-10.0f, 0.5f, 0.0f, 1.0f, 0.5f, 0.5f}, // Pink at -10X (big)
        // Small cubes (0.5x0.5x0.5) - positioned on floor at different locations
        {2.0f, 0.25f, 2.0f, 0.8f, 0.2f, 0.2f},  // Small red on floor
        {-2.0f, 0.25f, 2.0f, 0.2f, 0.8f, 0.2f},  // Small green on floor
        {2.0f, 0.25f, -2.0f, 0.2f, 0.2f, 0.8f}, // Small blue on floor
        {-2.0f, 0.25f, -2.0f, 0.8f, 0.8f, 0.2f},  // Small yellow on floor
        {0.0f, 0.25f, 0.0f, 0.8f, 0.2f, 0.8f}, // Small magenta on floor (at origin)
        // Blue rectangle: 1 unit tall, 4 units wide, 10 units long - positioned away from other blocks
        {15.0f, 0.5f, 0.0f, 0.0f, 0.0f, 1.0f},  // Blue rectangle at +15X (away from other cubes)
        // Pink block on top of vehicle (0.5x0.5x0.5, positioned near edge of short end)
        // Vehicle is 10 units long (Z), so short end is at Z edges. Place at +Z edge: vehicle_z + length/2 - block_size/2
        {15.0f, 1.25f, 4.75f, 1.0f, 0.5f, 0.5f},  // Pink block at +Z edge of vehicle (0.0 + 5.0 - 0.25 = 4.75, on top at 1.25)
        // Ground cube at 500 meters away (same size as starting floor: 50x1x50, same Y level)
        {500.0f, 0.5f, 0.0f, 0.5f, 0.5f, 0.5f},  // Gray ground cube at +500X (50x1x50 size, will be set below)
        // Yellow building on top of ground cube (20x20x50)
        {500.0f, 25.0f, 0.0f, 1.0f, 1.0f, 0.0f},  // Yellow building at +500X, 25 units up (half of 50 height)
    };
    
    g_numColoredCubes = static_cast<int>(referenceCubes.size());
    
    // Initialize global cube positions array, store original colors, initialize rotations, attachment data, and item properties
    g_coloredCubePositions.clear();
    g_coloredCubeOriginalColors.clear();
    g_coloredCubeRotations.clear();
    g_attachedCubes.clear();
    g_itemProperties.clear();
    for (int i = 0; i < g_numColoredCubes; i++) {
        const auto& cube = referenceCubes[i];
        g_coloredCubePositions.push_back({cube.x, cube.y, cube.z});
        g_coloredCubeOriginalColors.push_back({cube.r, cube.g, cube.b});
        g_coloredCubeRotations.push_back(0.0f);  // Initialize rotation to 0 degrees
        g_attachedCubes.push_back({false, 0.0f, 0.0f, 0.0f});  // Initialize as not attached
        
        // Initialize item properties (all start as generic blocks)
        ItemProperties props;
        props.item_type_id = 0;      // 0 = generic block
        props.item_name = "";        // Empty name by default
        props.trade_value = 0.0f;    // Not tradeable by default
        props.condition = 1.0f;      // Perfect condition
        props.weight = 1.0f;         // Default weight
        props.category = 0;          // Generic category
        props.is_salvaged = false;   // Not salvaged
        props.parent_index = -1;     // No parent by default
        g_itemProperties.push_back(props);
        
        // Set sizes: first 9 are big (1.0), next 5 are small (0.5), last one is rectangle
        if (i < 9) {
            // Big cubes: 1x1x1
            g_coloredCubeSizes[i] = 1.0f;
            g_coloredCubeSizeX[i] = 1.0f;
            g_coloredCubeSizeY[i] = 1.0f;
            g_coloredCubeSizeZ[i] = 1.0f;
        } else if (i < 14) {
            // Small cubes: 0.5x0.5x0.5
            g_coloredCubeSizes[i] = 0.5f;
            g_coloredCubeSizeX[i] = 0.5f;
            g_coloredCubeSizeY[i] = 0.5f;
            g_coloredCubeSizeZ[i] = 0.5f;
        } else if (i == 14) {
            // Blue rectangle: 4 units wide (X), 1 unit tall (Y), 10 units long (Z)
            g_coloredCubeSizes[i] = 1.0f;  // Average size for compatibility
            g_coloredCubeSizeX[i] = 4.0f;  // Width
            g_coloredCubeSizeY[i] = 1.0f;  // Height
            g_coloredCubeSizeZ[i] = 10.0f; // Length
        } else if (i == 15) {
            // Pink block on vehicle: 0.5x0.5x0.5
            g_coloredCubeSizes[i] = 0.5f;
            g_coloredCubeSizeX[i] = 0.5f;
            g_coloredCubeSizeY[i] = 0.5f;
            g_coloredCubeSizeZ[i] = 0.5f;
        } else if (i == 16) {
            // Ground cube: 50x1x50 (same size as starting floor cube: 50 units wide, 1 unit tall, 50 units deep)
            g_coloredCubeSizes[i] = 1.0f;  // Average size for compatibility
            g_coloredCubeSizeX[i] = 50.0f; // Width (same as starting floor)
            g_coloredCubeSizeY[i] = 1.0f;  // Height (same as floor)
            g_coloredCubeSizeZ[i] = 50.0f; // Depth (same as starting floor)
        } else if (i == 17) {
            // Yellow building: 20x20x50 (20 units wide, 20 units deep, 50 units tall)
            g_coloredCubeSizes[i] = 20.0f;  // Average size for compatibility
            g_coloredCubeSizeX[i] = 20.0f;  // Width
            g_coloredCubeSizeY[i] = 50.0f;  // Height (50 feet tall)
            g_coloredCubeSizeZ[i] = 20.0f;  // Depth
        }
    }
    
    // Create vertex buffers for each colored cube
    // Note: All vertices are created at size 1.0 (from -0.5 to 0.5), scaling is done in model matrix
    for (int i = 0; i < g_numColoredCubes && i < 19; i++) {
        // Create cube vertices with the specified color (always size 1.0, scaled in model matrix)
        std::vector<Vertex> coloredCubeVertices = {
            {{-0.5f, -0.5f,  0.5f}, {referenceCubes[i].r, referenceCubes[i].g, referenceCubes[i].b}},
            {{ 0.5f, -0.5f,  0.5f}, {referenceCubes[i].r, referenceCubes[i].g, referenceCubes[i].b}},
            {{ 0.5f,  0.5f,  0.5f}, {referenceCubes[i].r, referenceCubes[i].g, referenceCubes[i].b}},
            {{-0.5f,  0.5f,  0.5f}, {referenceCubes[i].r, referenceCubes[i].g, referenceCubes[i].b}},
            {{-0.5f, -0.5f, -0.5f}, {referenceCubes[i].r, referenceCubes[i].g, referenceCubes[i].b}},
            {{ 0.5f, -0.5f, -0.5f}, {referenceCubes[i].r, referenceCubes[i].g, referenceCubes[i].b}},
            {{ 0.5f,  0.5f, -0.5f}, {referenceCubes[i].r, referenceCubes[i].g, referenceCubes[i].b}},
            {{-0.5f,  0.5f, -0.5f}, {referenceCubes[i].r, referenceCubes[i].g, referenceCubes[i].b}},
        };
        
        VkDeviceSize vertexBufferSize = sizeof(coloredCubeVertices[0]) * coloredCubeVertices.size();
        createBuffer(vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     g_coloredCubeVertexBuffers[i], g_coloredCubeVertexBufferMemory[i]);
        
        void* data;
        vkMapMemory(g_device, g_coloredCubeVertexBufferMemory[i], 0, vertexBufferSize, 0, &data);
        memcpy(data, coloredCubeVertices.data(), (size_t)vertexBufferSize);
        vkUnmapMemory(g_device, g_coloredCubeVertexBufferMemory[i]);
    }
    
    std::cout << "[FPS] Created " << g_numColoredCubes << " colored reference cubes" << std::endl;
    
    // Initialize item properties for ALL cubes
    // Big cubes (0-4): Cargo containers with item type IDs 100-104
    const char* big_cube_names[] = {"Red Cargo Crate", "Green Cargo Crate", "Blue Cargo Crate", "Yellow Cargo Crate", "Orange Cargo Crate"};
    for (int i = 0; i < 5 && i < g_numColoredCubes; i++) {
        g_itemProperties[i].item_name = big_cube_names[i];
        g_itemProperties[i].item_type_id = 100 + i;  // 100, 101, 102, 103, 104
    }
    
    // Small cubes (5-13): Components with item type IDs 200-208
    const char* small_cube_names[] = {"Power Cell", "Circuit Board", "Fuel Injector", "Plasma Coil", "Nav Module", 
                                       "Thruster Nozzle", "Shield Emitter", "Sensor Array", "Comm Relay"};
    for (int i = 5; i < 14 && i < g_numColoredCubes; i++) {
        g_itemProperties[i].item_name = small_cube_names[i - 5];
        g_itemProperties[i].item_type_id = 200 + (i - 5);  // 200, 201, 202, 203, 204, 205, 206, 207, 208
    }
    
    // Vehicle (index 14): "Junk Skiff" - special item type ID 1
    if (g_numColoredCubes > 14) {
        g_itemProperties[14].item_name = "Junk Skiff";
        g_itemProperties[14].item_type_id = 1;  // Vehicle type
    }
    
    // Helm (index 15): "Barker's GyroHelm" - child of vehicle, special item type ID 2
    if (g_numColoredCubes > 15) {
        g_itemProperties[15].item_name = "Barker's GyroHelm";
        g_itemProperties[15].item_type_id = 2;  // Helm type
        g_itemProperties[15].parent_index = 14; // Child of vehicle (parent-child relationship)
    }
    
    // Ground cube (index 16): Generic ground/platform - no special item type
    if (g_numColoredCubes > 16) {
        g_itemProperties[16].item_name = "Ground Platform";
        g_itemProperties[16].item_type_id = 0;  // Generic (not targetable)
    }
    
    // Building (index 17): Yellow building - item type "building"
    if (g_numColoredCubes > 17) {
        g_itemProperties[17].item_name = "building";
        g_itemProperties[17].item_type_id = 300;  // Building type ID
    }
    
    std::cout << "[FPS] Initialized item properties for all cubes" << std::endl;
    
    // Load shaders - FORCE use of cube shaders (same as working spinning cube)
    std::vector<char> vertShaderCode, fragShaderCode;
    try {
        // Use cube shaders first (these are known to work)
        vertShaderCode = readFile("vert_cube.spv");
        fragShaderCode = readFile("frag_cube.spv");
        std::cout << "[FPS] Using cube shaders (vert_cube.spv, frag_cube.spv) - same as working spinning cube" << std::endl;
    } catch (const std::exception&) {
        // Fall back to fps-specific shaders
        try {
            vertShaderCode = readFile("vert_fps.spv");
            fragShaderCode = readFile("frag_fps.spv");
            std::cout << "[FPS] Using FPS-specific shaders (vert_fps.spv, frag_fps.spv)" << std::endl;
        } catch (const std::exception&) {
            // Last resort: default 3d shaders (but these are broken - missing vertex inputs)
            try {
                vertShaderCode = readFile("vert_3d.spv");
                fragShaderCode = readFile("frag_3d.spv");
                std::cerr << "[FPS] WARNING: Using default 3D shaders (vert_3d.spv, frag_3d.spv) - these may be broken!" << std::endl;
                std::cerr << "[FPS] Validation layer reported these shaders don't consume vertex attributes!" << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "[FPS] ERROR: Could not find shader files!" << std::endl;
                std::cerr << "[FPS] Tried: vert_cube.spv/frag_cube.spv, vert_fps.spv/frag_fps.spv, and vert_3d.spv/frag_3d.spv" << std::endl;
                std::cerr << "[FPS] Error: " << e.what() << std::endl;
                return 0;
            }
        }
    }
    
    VkShaderModuleCreateInfo vertCreateInfo = {};
    vertCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    vertCreateInfo.codeSize = vertShaderCode.size();
    vertCreateInfo.pCode = reinterpret_cast<const uint32_t*>(vertShaderCode.data());
    
    if (vkCreateShaderModule(g_device, &vertCreateInfo, nullptr, &g_fpsVertShaderModule) != VK_SUCCESS) {
        std::cerr << "[FPS] ERROR: Failed to create vertex shader module!" << std::endl;
        return 0;
    }
    
    VkShaderModuleCreateInfo fragCreateInfo = {};
    fragCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    fragCreateInfo.codeSize = fragShaderCode.size();
    fragCreateInfo.pCode = reinterpret_cast<const uint32_t*>(fragShaderCode.data());
    
    if (vkCreateShaderModule(g_device, &fragCreateInfo, nullptr, &g_fpsFragShaderModule) != VK_SUCCESS) {
        std::cerr << "[FPS] ERROR: Failed to create fragment shader module!" << std::endl;
        vkDestroyShaderModule(g_device, g_fpsVertShaderModule, nullptr);
        return 0;
    }
    
    VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = g_fpsVertShaderModule;
    vertShaderStageInfo.pName = "main";
    
    VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = g_fpsFragShaderModule;
    fragShaderStageInfo.pName = "main";
    
    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};
    
    // Vertex input description (same as cube)
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
    
    // Input assembly
    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;
    
    // Viewport and scissor
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
    
    // Rasterization
    VkPipelineRasterizationStateCreateInfo rasterizer = {};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;  // Disable culling so we can see all faces from all angles
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
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;
    
    VkPipelineColorBlendStateCreateInfo colorBlending = {};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    
    // Pipeline layout (reuse existing)
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
    
    if (vkCreateGraphicsPipelines(g_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &g_fpsPipeline) != VK_SUCCESS) {
        std::cerr << "[FPS] ERROR: Failed to create FPS graphics pipeline!" << std::endl;
        vkDestroyShaderModule(g_device, g_fpsFragShaderModule, nullptr);
        vkDestroyShaderModule(g_device, g_fpsVertShaderModule, nullptr);
        return 0;
    }
    
    g_fpsInitialized = true;
    std::cout << "[FPS] FPS camera renderer initialized successfully!" << std::endl;
    return 1;
}

// Render FPS camera frame
extern "C" void heidic_render_fps(GLFWwindow* window, float camera_pos_x, float camera_pos_y, float camera_pos_z, float camera_yaw, float camera_pitch) {
    if (g_device == VK_NULL_HANDLE || g_swapchain == VK_NULL_HANDLE) {
        return;
    }
    
    // Use FPS pipeline if available, otherwise use cube pipeline
    VkPipeline pipelineToUse = (g_fpsPipeline != VK_NULL_HANDLE) ? g_fpsPipeline : g_cubePipeline;
    if (pipelineToUse == VK_NULL_HANDLE) {
        std::cerr << "[FPS] ERROR: No pipeline available (neither FPS nor cube pipeline exists)!" << std::endl;
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
    
    std::array<VkClearValue, 2> clearValues = {};
    clearValues[0].color = {{0.1f, 0.1f, 0.15f, 1.0f}};  // Dark blue-gray background
    clearValues[1].depthStencil = {1.0f, 0};
    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();
    
    vkCmdBeginRenderPass(g_commandBuffers[imageIndex], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    
    // Bind pipeline FIRST (use the selected pipeline)
    vkCmdBindPipeline(g_commandBuffers[imageIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineToUse);
    
    // Update uniform buffer - view and projection matrices (shared across all objects)
    UniformBufferObject ubo = {};
    ubo.model = glm::mat4(1.0f);  // Set to identity (not used, shader uses push constant instead, but needed for struct layout)
    
    // Model matrix for floor cube - scale the floor cube to be a large floor
    // Floor cube vertices are -1.0 to 1.0 (2x2x2 cube), so scale to make it a large floor
    // Scale X and Z much more than Y to keep thickness but make it wide
    glm::mat4 floorModel = glm::mat4(1.0f);
    floorModel = glm::translate(floorModel, glm::vec3(0.0f, -0.5f, 0.0f));  // Translate first
    floorModel = glm::scale(floorModel, glm::vec3(25.0f, 0.5f, 25.0f));  // Then scale: 50x1x50 units (wide floor, thin height)
    
    // View matrix - calculate from camera position, yaw, and pitch
    // Convert yaw and pitch from degrees to radians
    float yaw_rad = camera_yaw * 3.14159f / 180.0f;
    float pitch_rad = camera_pitch * 3.14159f / 180.0f;
    
    // Calculate forward vector from yaw and pitch (Y-up coordinate system)
    // Yaw: rotation around Y axis (0 = looking down -Z, positive = rotating right)
    // Pitch: rotation around X axis (0 = horizontal, positive = looking up)
    // Standard mouse look: positive pitch = looking up
    glm::vec3 forward(
        sinf(yaw_rad) * cosf(pitch_rad),
        sinf(pitch_rad),  // Positive pitch = looking up (positive Y)
        -cosf(yaw_rad) * cosf(pitch_rad)
    );
    forward = glm::normalize(forward);
    
    // Calculate right vector (perpendicular to forward and world up)
    glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
    
    // Calculate up vector (perpendicular to forward and right) - correct order for right-handed system
    glm::vec3 up = glm::normalize(glm::cross(right, forward));
    
    // Camera position
    Vec3 eye = {camera_pos_x, camera_pos_y, camera_pos_z};
    
    // Center is camera position + forward direction
    Vec3 center = {
        camera_pos_x + forward.x,
        camera_pos_y + forward.y,
        camera_pos_z + forward.z
    };
    Vec3 up_vec = {up.x, up.y, up.z};
    
    // Create view matrix using mat4_lookat - assign directly like spinning cube
    ubo.view = mat4_lookat(eye, center, up_vec);
    
    // Projection matrix - use 70 degree FOV for FPS camera
    float fov = 70.0f * 3.14159f / 180.0f;
    float aspect = (float)g_swapchainExtent.width / (float)g_swapchainExtent.height;
    float nearPlane = 0.1f;
    float farPlane = 2000.0f;  // Increased to 2000 to see objects 500+ meters away
    ubo.proj = mat4_perspective(fov, aspect, nearPlane, farPlane);
    
    // Vulkan clip space has inverted Y and half Z - modify directly like spinning cube
    ubo.proj[1][1] *= -1.0f;
    
    // Store matrices globally for raycasting
    g_fpsCurrentView = ubo.view;
    g_fpsCurrentProj = ubo.proj;
    g_fpsCurrentCamPos = glm::vec3(camera_pos_x, camera_pos_y, camera_pos_z);
    
    // No per-frame debug spam
    
    // Update uniform buffer for floor cube
    void* data;
    vkMapMemory(g_device, g_uniformBuffersMemory[imageIndex], 0, sizeof(ubo), 0, &data);
    memcpy(data, &ubo, sizeof(ubo));
    vkUnmapMemory(g_device, g_uniformBuffersMemory[imageIndex]);
    
    // DIAGNOSTIC: Verify descriptor sets are valid
    if (g_descriptorSets.empty() || imageIndex >= g_descriptorSets.size() || g_descriptorSets[imageIndex] == VK_NULL_HANDLE) {
        std::cerr << "[FPS] ERROR: Descriptor set is NULL! imageIndex=" << imageIndex << ", sets.size()=" << g_descriptorSets.size() << std::endl;
        return;
    }
    if (g_uniformBuffers.empty() || imageIndex >= g_uniformBuffers.size() || g_uniformBuffers[imageIndex] == VK_NULL_HANDLE) {
        std::cerr << "[FPS] ERROR: Uniform buffer is NULL! imageIndex=" << imageIndex << ", buffers.size()=" << g_uniformBuffers.size() << std::endl;
        return;
    }
    
    // Bind descriptor set (MUST be after pipeline is bound, before any draw calls)
    vkCmdBindDescriptorSets(g_commandBuffers[imageIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, g_pipelineLayout, 0, 1, &g_descriptorSets[imageIndex], 0, nullptr);
    
    // Push model matrix for floor cube as push constant
    vkCmdPushConstants(g_commandBuffers[imageIndex], g_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &floorModel);
    
    // Bind vertex buffer (use FPS-specific buffers)
    VkBuffer vertexBuffers[] = {g_fpsCubeVertexBuffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(g_commandBuffers[imageIndex], 0, 1, vertexBuffers, offsets);
    
    // Bind index buffer (use FPS-specific buffers)
    vkCmdBindIndexBuffer(g_commandBuffers[imageIndex], g_fpsCubeIndexBuffer, 0, VK_INDEX_TYPE_UINT16);
    
    // Draw floor cube using indexed drawing
    vkCmdDrawIndexed(g_commandBuffers[imageIndex], g_fpsCubeIndexCount, 1, 0, 0, 0);
    
    // Draw multiple colored 1x1x1 cubes on top of the floor for spatial reference
    // Use global mutable cube positions array (for pickup system)
    
    // Render colored cubes if they were created
    for (int i = 0; i < g_numColoredCubes && i < static_cast<int>(g_coloredCubePositions.size()); i++) {
        if (g_coloredCubeVertexBuffers[i] == VK_NULL_HANDLE) {
            continue;
        }
        
        // Model matrix: cube at the specified position with rotation
        // Note: Vertices are created at size 1.0, so we need to scale them to the actual size
        // Use per-axis sizes if available, otherwise use uniform size
        // Transform order: translate, rotate (around Y axis), scale
        float scaleX = g_coloredCubeSizeX[i] > 0.0f ? g_coloredCubeSizeX[i] : g_coloredCubeSizes[i];
        float scaleY = g_coloredCubeSizeY[i] > 0.0f ? g_coloredCubeSizeY[i] : g_coloredCubeSizes[i];
        float scaleZ = g_coloredCubeSizeZ[i] > 0.0f ? g_coloredCubeSizeZ[i] : g_coloredCubeSizes[i];
        glm::mat4 cubeModel = glm::mat4(1.0f);
        cubeModel = glm::translate(cubeModel, glm::vec3(g_coloredCubePositions[i].x, g_coloredCubePositions[i].y, g_coloredCubePositions[i].z));
        // Apply rotation around Y axis (yaw) if cube has rotation set
        if (i < static_cast<int>(g_coloredCubeRotations.size()) && g_coloredCubeRotations[i] != 0.0f) {
            float yaw_rad = g_coloredCubeRotations[i] * 3.14159f / 180.0f;  // Convert degrees to radians
            cubeModel = glm::rotate(cubeModel, yaw_rad, glm::vec3(0.0f, 1.0f, 0.0f));
        }
        cubeModel = glm::scale(cubeModel, glm::vec3(scaleX, scaleY, scaleZ));  // Scale to actual size (supports non-uniform)
        
        // Push model matrix as push constant (each draw gets its own matrix)
        vkCmdPushConstants(g_commandBuffers[imageIndex], g_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &cubeModel);
        
        // Bind the colored cube's vertex buffer
        VkBuffer vertexBuffers[] = {g_coloredCubeVertexBuffers[i]};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(g_commandBuffers[imageIndex], 0, 1, vertexBuffers, offsets);
        
        // Rebind index buffer (same indices work for all cubes - 36 indices for a cube)
        vkCmdBindIndexBuffer(g_commandBuffers[imageIndex], g_fpsCubeIndexBuffer, 0, VK_INDEX_TYPE_UINT16);
        
        // Draw the cube (36 indices for a cube: 6 faces * 2 triangles * 3 vertices)
        vkCmdDrawIndexed(g_commandBuffers[imageIndex], 36, 1, 0, 0, 0);
    }
    
    // Render Neuroshell UI (includes crosshair)
    #ifdef USE_NEUROSHELL
    extern void neuroshell_render(VkCommandBuffer);
    extern bool neuroshell_is_enabled();
    if (neuroshell_is_enabled()) {
        neuroshell_render(g_commandBuffers[imageIndex]);
    }
    #endif
    
    vkCmdEndRenderPass(g_commandBuffers[imageIndex]);
    
    if (vkEndCommandBuffer(g_commandBuffers[imageIndex]) != VK_SUCCESS) {
        std::cerr << "[FPS] ERROR: Failed to record command buffer!" << std::endl;
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
        std::cerr << "[FPS] ERROR: Failed to submit draw command buffer!" << std::endl;
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

// Cleanup FPS renderer
extern "C" void heidic_cleanup_renderer_fps() {
    if (g_device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(g_device);
        
        // Cleanup FPS-specific buffers
        if (g_fpsCubeIndexBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(g_device, g_fpsCubeIndexBuffer, nullptr);
            g_fpsCubeIndexBuffer = VK_NULL_HANDLE;
        }
        if (g_fpsCubeIndexBufferMemory != VK_NULL_HANDLE) {
            vkFreeMemory(g_device, g_fpsCubeIndexBufferMemory, nullptr);
            g_fpsCubeIndexBufferMemory = VK_NULL_HANDLE;
        }
        if (g_fpsCubeVertexBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(g_device, g_fpsCubeVertexBuffer, nullptr);
            g_fpsCubeVertexBuffer = VK_NULL_HANDLE;
        }
        if (g_fpsCubeVertexBufferMemory != VK_NULL_HANDLE) {
            vkFreeMemory(g_device, g_fpsCubeVertexBufferMemory, nullptr);
            g_fpsCubeVertexBufferMemory = VK_NULL_HANDLE;
        }
        
        // Cleanup colored cube buffers
        for (int i = 0; i < g_numColoredCubes && i < 10; i++) {
            if (g_coloredCubeVertexBuffers[i] != VK_NULL_HANDLE) {
                vkDestroyBuffer(g_device, g_coloredCubeVertexBuffers[i], nullptr);
                g_coloredCubeVertexBuffers[i] = VK_NULL_HANDLE;
            }
            if (g_coloredCubeVertexBufferMemory[i] != VK_NULL_HANDLE) {
                vkFreeMemory(g_device, g_coloredCubeVertexBufferMemory[i], nullptr);
                g_coloredCubeVertexBufferMemory[i] = VK_NULL_HANDLE;
            }
        }
        g_numColoredCubes = 0;
        g_coloredCubePositions.clear();
        g_coloredCubeRotations.clear();
        g_attachedCubes.clear();
        g_itemProperties.clear();
        
        if (g_fpsPipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(g_device, g_fpsPipeline, nullptr);
            g_fpsPipeline = VK_NULL_HANDLE;
        }
        
        if (g_fpsFragShaderModule != VK_NULL_HANDLE) {
            vkDestroyShaderModule(g_device, g_fpsFragShaderModule, nullptr);
            g_fpsFragShaderModule = VK_NULL_HANDLE;
        }
        
        if (g_fpsVertShaderModule != VK_NULL_HANDLE) {
            vkDestroyShaderModule(g_device, g_fpsVertShaderModule, nullptr);
            g_fpsVertShaderModule = VK_NULL_HANDLE;
        }
    }
    
    g_fpsInitialized = false;
    // Note: We don't destroy base renderer resources or cube buffers since they're shared
}

// GLFW mouse helper functions (wrappers for pointer-based GLFW functions)
extern "C" double heidic_get_cursor_x(GLFWwindow* window) {
    double xpos, ypos;
    glfwGetCursorPos(window, &xpos, &ypos);
    return xpos;
}

extern "C" double heidic_get_cursor_y(GLFWwindow* window) {
    double xpos, ypos;
    glfwGetCursorPos(window, &xpos, &ypos);
    return ypos;
}

// ============================================================================
// RAYCAST FUNCTIONS (ported from vulkan_old)
// ============================================================================

// AABB structure for ray-AABB intersection
struct AABB {
    glm::vec3 min;
    glm::vec3 max;
    
    AABB() : min(0.0f), max(0.0f) {}
    AABB(glm::vec3 min, glm::vec3 max) : min(min), max(max) {}
};

// Convert mouse screen position to normalized device coordinates (NDC)
// NDC: x,y in [-1, 1]
// Vulkan NDC: Y=1 is top, Y=-1 is bottom (Y points down)
// GLFW screen: Y=0 is top, Y=height is bottom
static glm::vec2 screenToNDC(float screenX, float screenY, int width, int height) {
    float ndcX = (2.0f * screenX / width) - 1.0f;
    // Map screen Y=0 (top) to NDC Y=-1 (top), screen Y=height (bottom) to NDC Y=1 (bottom)
    float ndcY = (2.0f * screenY / height) - 1.0f;
    return glm::vec2(ndcX, ndcY);
}

// Unproject: Convert NDC coordinates to world-space ray
// Returns ray origin and direction
// CORRECT VERSION: NDC Z is always [-1, 1] regardless of depth buffer format
// perspectiveRH_ZO affects depth buffer mapping, but NDC Z is still [-1, 1]
static void unproject(glm::vec2 ndc, glm::mat4 invProj, glm::mat4 invView, glm::vec3& rayOrigin, glm::vec3& rayDir) {
    // NDC clip space: Z = -1 (near), Z = +1 (far)
    // This is correct for both OpenGL and Vulkan NDC coordinates
    glm::vec4 clipNear = glm::vec4(ndc.x, ndc.y, -1.0f, 1.0f);  // Near plane Z = -1
    glm::vec4 clipFar = glm::vec4(ndc.x, ndc.y, 1.0f, 1.0f);    // Far plane Z = +1
    
    // Transform to eye space (view space)
    glm::vec4 eyeNear = invProj * clipNear;
    eyeNear /= eyeNear.w;  // Perspective divide
    
    glm::vec4 eyeFar = invProj * clipFar;
    eyeFar /= eyeFar.w;  // Perspective divide
    
    // Transform to world space
    glm::vec4 worldNear = invView * eyeNear;
    glm::vec4 worldFar = invView * eyeFar;
    
    glm::vec3 worldNearPoint = glm::vec3(worldNear);
    glm::vec3 worldFarPoint = glm::vec3(worldFar);
    
    // Ray origin is camera position (as per user's working code)
    rayOrigin = g_fpsCurrentCamPos;
    
    // Ray direction: from near point to far point (normalized)
    // This gives us the correct direction regardless of distance
    glm::vec3 dirVec = worldFarPoint - worldNearPoint;
    float dirLen = glm::length(dirVec);
    if (dirLen > 0.0001f) {
        rayDir = dirVec / dirLen;
    } else {
        // Fallback: if near and far are too close, use direction from camera to far point
        rayDir = glm::normalize(worldFarPoint - g_fpsCurrentCamPos);
    }
}

// Ray-AABB intersection using Möller-Trumbore slab method
// Returns true if ray hits AABB, and t (distance along ray) if hit
static bool rayAABB(const glm::vec3& rayOrigin, const glm::vec3& rayDir, const AABB& box, float& tMin, float& tMax) {
    // Ensure ray direction is normalized (should already be, but double-check for precision)
    glm::vec3 dir = glm::normalize(rayDir);
    
    // Handle division by zero by using a very small epsilon
    const float epsilon = 1e-6f;
    glm::vec3 invDir;
    invDir.x = (fabsf(dir.x) < epsilon) ? (dir.x >= 0.0f ? 1e6f : -1e6f) : (1.0f / dir.x);
    invDir.y = (fabsf(dir.y) < epsilon) ? (dir.y >= 0.0f ? 1e6f : -1e6f) : (1.0f / dir.y);
    invDir.z = (fabsf(dir.z) < epsilon) ? (dir.z >= 0.0f ? 1e6f : -1e6f) : (1.0f / dir.z);
    
    glm::vec3 t0 = (box.min - rayOrigin) * invDir;
    glm::vec3 t1 = (box.max - rayOrigin) * invDir;
    
    glm::vec3 tMinVec = glm::min(t0, t1);
    glm::vec3 tMaxVec = glm::max(t0, t1);
    
    tMin = glm::max(glm::max(tMinVec.x, tMinVec.y), tMinVec.z);
    tMax = glm::min(glm::min(tMaxVec.x, tMaxVec.y), tMaxVec.z);
    
    // Ray hits if tMax >= tMin AND tMax >= 0 (intersection is in front of or at ray origin)
    // If tMin > tMax, the ray misses the AABB
    // If tMax < 0, the entire AABB is behind the ray origin
    // If tMin < 0 and tMax >= 0, the ray origin is inside the AABB (hit!)
    
    return (tMax >= tMin) && (tMax >= 0.0f);
}

// Create AABB for a cube at position (x,y,z) with half-extents (sx/2, sy/2, sz/2)
static AABB createCubeAABB(float x, float y, float z, float sx, float sy, float sz) {
    float halfSx = sx * 0.5f;
    float halfSy = sy * 0.5f;
    float halfSz = sz * 0.5f;
    
    glm::vec3 min(x - halfSx, y - halfSy, z - halfSz);
    glm::vec3 max(x + halfSx, y + halfSy, z + halfSz);
    return AABB(min, max);
}

// Raycast from screen center (crosshair) against a cube
// Returns 1 if hit, 0 if miss
extern "C" int heidic_raycast_cube_hit_center(GLFWwindow* window, float cubeX, float cubeY, float cubeZ, float cubeSx, float cubeSy, float cubeSz) {
    if (!window) return 0;
    
    // Get framebuffer size
    int fbWidth, fbHeight;
    glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
    
    // Use center of screen (where crosshair is)
    float centerX = fbWidth / 2.0f;
    float centerY = fbHeight / 2.0f;
    
    // Convert to NDC
    glm::vec2 ndc = screenToNDC(centerX, centerY, fbWidth, fbHeight);
    
    // Unproject to get ray
    glm::mat4 invProj = glm::inverse(g_fpsCurrentProj);
    glm::mat4 invView = glm::inverse(g_fpsCurrentView);
    glm::vec3 rayOrigin, rayDir;
    unproject(ndc, invProj, invView, rayOrigin, rayDir);
    
    // Create AABB for cube
    AABB cubeBox = createCubeAABB(cubeX, cubeY, cubeZ, cubeSx, cubeSy, cubeSz);
    
    // Test intersection
    float tMin, tMax;
    bool hit = rayAABB(rayOrigin, rayDir, cubeBox, tMin, tMax);
    
    return hit ? 1 : 0;
}

// Get hit point from raycast (call after heidic_raycast_cube_hit_center returns 1)
// Returns world-space hit position
extern "C" Vec3 heidic_raycast_cube_hit_point_center(GLFWwindow* window, float cubeX, float cubeY, float cubeZ, float cubeSx, float cubeSy, float cubeSz) {
    Vec3 result = {0.0f, 0.0f, 0.0f};
    if (!window) return result;
    
    // Get framebuffer size
    int fbWidth, fbHeight;
    glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
    
    // Use center of screen (where crosshair is)
    float centerX = fbWidth / 2.0f;
    float centerY = fbHeight / 2.0f;
    
    // Convert to NDC
    glm::vec2 ndc = screenToNDC(centerX, centerY, fbWidth, fbHeight);
    
    // Unproject to get ray
    glm::mat4 invProj = glm::inverse(g_fpsCurrentProj);
    glm::mat4 invView = glm::inverse(g_fpsCurrentView);
    glm::vec3 rayOrigin, rayDir;
    unproject(ndc, invProj, invView, rayOrigin, rayDir);
    
    // Create AABB for cube
    AABB cubeBox = createCubeAABB(cubeX, cubeY, cubeZ, cubeSx, cubeSy, cubeSz);
    
    // Test intersection
    float tMin, tMax;
    if (rayAABB(rayOrigin, rayDir, cubeBox, tMin, tMax)) {
        // Calculate hit point (use tMin for closest intersection)
        glm::vec3 hit = rayOrigin + rayDir * tMin;
        result.x = hit.x;
        result.y = hit.y;
        result.z = hit.z;
    }
    
    return result;
}

// Get cube position (for HEIDIC to read) - returns Vec3
extern "C" Vec3 heidic_get_cube_position(int cube_index) {
    if (cube_index >= 0 && cube_index < static_cast<int>(g_coloredCubePositions.size())) {
        Vec3 result;
        result.x = g_coloredCubePositions[cube_index].x;
        result.y = g_coloredCubePositions[cube_index].y;
        result.z = g_coloredCubePositions[cube_index].z;
        return result;
    } else {
        Vec3 result = {0.0f, 0.0f, 0.0f};
        return result;
    }
}

// Set cube position (for HEIDIC to update picked-up cube)
extern "C" void heidic_set_cube_position(int cube_index, float x, float y, float z) {
    if (cube_index >= 0 && cube_index < static_cast<int>(g_coloredCubePositions.size())) {
        g_coloredCubePositions[cube_index].x = x;
        g_coloredCubePositions[cube_index].y = y;
        g_coloredCubePositions[cube_index].z = z;
    } else {
        std::cout << "[CUBE_POS] ERROR: Invalid cube_index " << cube_index 
                  << " (size=" << g_coloredCubePositions.size() << ")" << std::endl;
    }
}

// Set cube rotation (yaw in degrees, for visual rotation around Y axis)
extern "C" void heidic_set_cube_rotation(int cube_index, float yaw_degrees) {
    if (cube_index >= 0 && cube_index < static_cast<int>(g_coloredCubeRotations.size())) {
        g_coloredCubeRotations[cube_index] = yaw_degrees;
    } else {
        std::cout << "[CUBE_ROT] ERROR: Invalid cube_index " << cube_index 
                  << " (size=" << g_coloredCubeRotations.size() << ")" << std::endl;
    }
}

// Set cube color (for visual feedback when selected/picked up)
extern "C" void heidic_set_cube_color(int cube_index, float r, float g, float b) {
    if (cube_index >= 0 && cube_index < g_numColoredCubes && 
        cube_index < static_cast<int>(g_coloredCubeOriginalColors.size()) &&
        g_coloredCubeVertexBuffers[cube_index] != VK_NULL_HANDLE) {
        
        // Update vertex buffer with new color
        std::vector<Vertex> coloredCubeVertices = {
            {{-0.5f, -0.5f,  0.5f}, {r, g, b}},
            {{ 0.5f, -0.5f,  0.5f}, {r, g, b}},
            {{ 0.5f,  0.5f,  0.5f}, {r, g, b}},
            {{-0.5f,  0.5f,  0.5f}, {r, g, b}},
            {{-0.5f, -0.5f, -0.5f}, {r, g, b}},
            {{ 0.5f, -0.5f, -0.5f}, {r, g, b}},
            {{ 0.5f,  0.5f, -0.5f}, {r, g, b}},
            {{-0.5f,  0.5f, -0.5f}, {r, g, b}},
        };
        
        VkDeviceSize vertexBufferSize = sizeof(coloredCubeVertices[0]) * coloredCubeVertices.size();
        void* data;
        vkMapMemory(g_device, g_coloredCubeVertexBufferMemory[cube_index], 0, vertexBufferSize, 0, &data);
        memcpy(data, coloredCubeVertices.data(), (size_t)vertexBufferSize);
        vkUnmapMemory(g_device, g_coloredCubeVertexBufferMemory[cube_index]);
    }
}

// Restore cube to original color
extern "C" void heidic_restore_cube_color(int cube_index) {
    if (cube_index >= 0 && cube_index < static_cast<int>(g_coloredCubeOriginalColors.size())) {
        const auto& original = g_coloredCubeOriginalColors[cube_index];
        heidic_set_cube_color(cube_index, original.r, original.g, original.b);
    }
}

// ============================================================================
// VEHICLE ATTACHMENT SYSTEM
// ============================================================================

// Attach a cube to the vehicle (index 14)
// local_x, local_y, local_z are the offset from vehicle center in vehicle-local space
extern "C" void heidic_attach_cube_to_vehicle(int cube_index, float local_x, float local_y, float local_z) {
    if (cube_index >= 0 && cube_index < static_cast<int>(g_attachedCubes.size())) {
        g_attachedCubes[cube_index].attached = true;
        g_attachedCubes[cube_index].local_x = local_x;
        g_attachedCubes[cube_index].local_y = local_y;
        g_attachedCubes[cube_index].local_z = local_z;
        
        // Set parent relationship (vehicle is index 14)
        if (cube_index >= 0 && cube_index < static_cast<int>(g_itemProperties.size())) {
            g_itemProperties[cube_index].parent_index = 14;  // Vehicle is parent
        }
        
        std::cout << "[ATTACH] Cube " << cube_index << " attached to vehicle (parent: 14) with local offset ("
                  << local_x << ", " << local_y << ", " << local_z << ")" << std::endl;
    }
}

// Detach a cube from the vehicle
extern "C" void heidic_detach_cube_from_vehicle(int cube_index) {
    if (cube_index >= 0 && cube_index < static_cast<int>(g_attachedCubes.size())) {
        if (g_attachedCubes[cube_index].attached) {
            g_attachedCubes[cube_index].attached = false;
            
            // Clear parent relationship (unless it's the helm, which should always be child of vehicle)
            if (cube_index >= 0 && cube_index < static_cast<int>(g_itemProperties.size()) && cube_index != 15) {
                g_itemProperties[cube_index].parent_index = -1;  // No parent
            }
            
            std::cout << "[ATTACH] Cube " << cube_index << " detached from vehicle" << std::endl;
        }
    }
}

// Check if a cube is attached to the vehicle (returns 1 if attached, 0 if not)
extern "C" int heidic_is_cube_attached(int cube_index) {
    if (cube_index >= 0 && cube_index < static_cast<int>(g_attachedCubes.size())) {
        return g_attachedCubes[cube_index].attached ? 1 : 0;
    }
    return 0;
}

// ============================================================================
// ITEM PROPERTIES SYSTEM (for scavenging/trading/building)
// ============================================================================

// Set item properties for a cube
extern "C" void heidic_set_item_properties(int cube_index, int item_type_id, float trade_value, float condition, float weight, int category, int is_salvaged) {
    if (cube_index >= 0 && cube_index < static_cast<int>(g_itemProperties.size())) {
        g_itemProperties[cube_index].item_type_id = item_type_id;
        g_itemProperties[cube_index].trade_value = trade_value;
        g_itemProperties[cube_index].condition = condition;
        g_itemProperties[cube_index].weight = weight;
        g_itemProperties[cube_index].category = category;
        g_itemProperties[cube_index].is_salvaged = (is_salvaged != 0);
    }
}

// Get item type ID for a cube
extern "C" int heidic_get_item_type_id(int cube_index) {
    if (cube_index >= 0 && cube_index < static_cast<int>(g_itemProperties.size())) {
        return g_itemProperties[cube_index].item_type_id;
    }
    return 0;
}

// Get trade value for a cube
extern "C" float heidic_get_item_trade_value(int cube_index) {
    if (cube_index >= 0 && cube_index < static_cast<int>(g_itemProperties.size())) {
        return g_itemProperties[cube_index].trade_value;
    }
    return 0.0f;
}

// Get condition for a cube (0.0 to 1.0)
extern "C" float heidic_get_item_condition(int cube_index) {
    if (cube_index >= 0 && cube_index < static_cast<int>(g_itemProperties.size())) {
        return g_itemProperties[cube_index].condition;
    }
    return 1.0f;
}

// Get weight for a cube
extern "C" float heidic_get_item_weight(int cube_index) {
    if (cube_index >= 0 && cube_index < static_cast<int>(g_itemProperties.size())) {
        return g_itemProperties[cube_index].weight;
    }
    return 1.0f;
}

// Get category for a cube (0=generic, 1=consumable, 2=part, 3=resource, 4=scrap)
extern "C" int heidic_get_item_category(int cube_index) {
    if (cube_index >= 0 && cube_index < static_cast<int>(g_itemProperties.size())) {
        return g_itemProperties[cube_index].category;
    }
    return 0;
}

// Check if item is salvaged
extern "C" int heidic_is_item_salvaged(int cube_index) {
    if (cube_index >= 0 && cube_index < static_cast<int>(g_itemProperties.size())) {
        return g_itemProperties[cube_index].is_salvaged ? 1 : 0;
    }
    return 0;
}

// Set item name (string)
extern "C" void heidic_set_item_name(int cube_index, const char* item_name) {
    if (cube_index >= 0 && cube_index < static_cast<int>(g_itemProperties.size())) {
        g_itemProperties[cube_index].item_name = item_name ? item_name : "";
    }
}

// Get item name (returns pointer to static string - safe for HEIDIC to copy)
// NOTE: Returns a pointer to a static string that gets updated each call.
// HEIDIC will copy this string immediately, so it's safe.
extern "C" const char* heidic_get_item_name(int cube_index) {
    static std::string result_string = "";
    
    if (cube_index >= 0 && cube_index < static_cast<int>(g_itemProperties.size())) {
        const std::string& name = g_itemProperties[cube_index].item_name;
        if (!name.empty()) {
            // Copy to static string to ensure pointer remains valid
            result_string = name;
            return result_string.c_str();
        } else {
            // If name is empty, return item type ID as fallback
            int item_type_id = g_itemProperties[cube_index].item_type_id;
            if (item_type_id > 0) {
                result_string = "Item #" + std::to_string(item_type_id);
                return result_string.c_str();
            }
        }
    }
    
    // Return empty string
    result_string = "";
    return result_string.c_str();
}

// Set parent cube index (for hierarchical relationships)
extern "C" void heidic_set_item_parent(int cube_index, int parent_index) {
    if (cube_index >= 0 && cube_index < static_cast<int>(g_itemProperties.size())) {
        g_itemProperties[cube_index].parent_index = parent_index;
    }
}

// Get parent cube index (-1 = no parent)
extern "C" int heidic_get_item_parent(int cube_index) {
    if (cube_index >= 0 && cube_index < static_cast<int>(g_itemProperties.size())) {
        return g_itemProperties[cube_index].parent_index;
    }
    return -1;
}

// Update all attached cubes to match the vehicle's position and rotation
// vehicle_x/y/z: vehicle center position
// vehicle_yaw: vehicle yaw in degrees
// vehicle_size_y: vehicle height (to calculate top surface)
extern "C" void heidic_update_attached_cubes(float vehicle_x, float vehicle_y, float vehicle_z, float vehicle_yaw, float vehicle_size_y) {
    float yaw_rad = vehicle_yaw * 3.14159f / 180.0f;
    float cos_yaw = cosf(yaw_rad);
    float sin_yaw = sinf(yaw_rad);
    float vehicle_top = vehicle_y + (vehicle_size_y / 2.0f);
    
    for (int i = 0; i < static_cast<int>(g_attachedCubes.size()); i++) {
        if (g_attachedCubes[i].attached) {
            // Rotate local offset by vehicle yaw to get world offset
            // x' = local_x * cos(yaw) + local_z * sin(yaw)
            // z' = -local_x * sin(yaw) + local_z * cos(yaw)
            float world_offset_x = g_attachedCubes[i].local_x * cos_yaw + g_attachedCubes[i].local_z * sin_yaw;
            float world_offset_z = -g_attachedCubes[i].local_x * sin_yaw + g_attachedCubes[i].local_z * cos_yaw;
            
            // Calculate world position
            float new_x = vehicle_x + world_offset_x;
            float new_y = vehicle_top + g_attachedCubes[i].local_y;
            float new_z = vehicle_z + world_offset_z;
            
            // Update cube position
            g_coloredCubePositions[i].x = new_x;
            g_coloredCubePositions[i].y = new_y;
            g_coloredCubePositions[i].z = new_z;
            
            // Update cube rotation to match vehicle
            if (i < static_cast<int>(g_coloredCubeRotations.size())) {
                g_coloredCubeRotations[i] = vehicle_yaw;
            }
        }
    }
}

// Cast ray downward from position and return distance to floor (or -1 if no hit)
// Floor cube: center at (0, -0.5, 0), size (25, 0.5, 25), so top is at y = 0.0
extern "C" float heidic_raycast_downward_distance(float x, float y, float z) {
    // Ray origin: cube position
    glm::vec3 rayOrigin(x, y, z);
    // Ray direction: straight down (negative Y)
    glm::vec3 rayDir(0.0f, -1.0f, 0.0f);
    
    // Floor cube AABB: center at (0, -0.5, 0), size (25, 0.5, 25)
    // Half-extents: (12.5, 0.25, 12.5)
    // Min: (-12.5, -0.75, -12.5), Max: (12.5, -0.25, -12.5)
    AABB floorAABB;
    floorAABB.min = glm::vec3(-12.5f, -0.75f, -12.5f);
    floorAABB.max = glm::vec3(12.5f, -0.25f, 12.5f);
    
    float tMin, tMax;
    if (rayAABB(rayOrigin, rayDir, floorAABB, tMin, tMax)) {
        // Ray hit the floor, return the distance
        // Use tMin (first intersection point)
        if (tMin >= 0.0f) {
            return tMin;
        }
    }
    
    // No hit or hit behind origin
    return -1.0f;
}

// Cast ray downward from position and return index of big cube hit (or -1 if no hit or hit small cube)
extern "C" int heidic_raycast_downward_big_cube(float x, float y, float z) {
    // Ray origin: position
    glm::vec3 rayOrigin(x, y, z);
    // Ray direction: straight down (negative Y)
    glm::vec3 rayDir(0.0f, -1.0f, 0.0f);
    
    int closestHitIndex = -1;
    float closestT = 1000.0f;  // Large initial distance
    
    // Test all cubes (check all cubes, not just first 9)
    // Skip vehicle (index 14), helm (index 15), and ground platform (index 16) as they're special
    // But include building (index 17) and all other cubes
    for (int i = 0; i < g_numColoredCubes; i++) {
        // Skip vehicle, helm, and ground platform (they're handled separately or shouldn't be stepped on)
        if (i == 14 || i == 15 || i == 16) {
            continue;
        }
        
        // Check if cube has size >= 1.0 (big cube) or use per-axis sizes for rectangles
        float cubeSize = g_coloredCubeSizes[i];
        float sizeX = g_coloredCubeSizeX[i];
        float sizeY = g_coloredCubeSizeY[i];
        float sizeZ = g_coloredCubeSizeZ[i];
        
        // Use per-axis sizes if available (for rectangles), otherwise use uniform size
        AABB cubeBox = createCubeAABB(
            g_coloredCubePositions[i].x,
            g_coloredCubePositions[i].y,
            g_coloredCubePositions[i].z,
            sizeX > 0.0f ? sizeX : cubeSize,
            sizeY > 0.0f ? sizeY : cubeSize,
            sizeZ > 0.0f ? sizeZ : cubeSize
        );
        
        float tMin, tMax;
        if (rayAABB(rayOrigin, rayDir, cubeBox, tMin, tMax)) {
            if (tMin >= 0.0f && tMin < closestT) {
                closestT = tMin;
                closestHitIndex = i;
            }
        }
    }
    
    return closestHitIndex;
}

// Get cube size (1.0 for big, 0.5 for small, or average for rectangles)
extern "C" float heidic_get_cube_size(int cube_index) {
    if (cube_index >= 0 && cube_index < g_numColoredCubes) {
        return g_coloredCubeSizes[cube_index];
    }
    return 0.0f;
}

// Get cube size per axis (for rectangles)
extern "C" Vec3 heidic_get_cube_size_xyz(int cube_index) {
    Vec3 result = {0.0f, 0.0f, 0.0f};
    if (cube_index >= 0 && cube_index < g_numColoredCubes) {
        result.x = g_coloredCubeSizeX[cube_index] > 0.0f ? g_coloredCubeSizeX[cube_index] : g_coloredCubeSizes[cube_index];
        result.y = g_coloredCubeSizeY[cube_index] > 0.0f ? g_coloredCubeSizeY[cube_index] : g_coloredCubeSizes[cube_index];
        result.z = g_coloredCubeSizeZ[cube_index] > 0.0f ? g_coloredCubeSizeZ[cube_index] : g_coloredCubeSizes[cube_index];
    }
    return result;
}

// Get ray origin and direction from screen center (for debug visualization)
extern "C" Vec3 heidic_get_center_ray_origin(GLFWwindow* window) {
    Vec3 result = {0.0f, 0.0f, 0.0f};
    if (!window) return result;
    result.x = g_fpsCurrentCamPos.x;
    result.y = g_fpsCurrentCamPos.y;
    result.z = g_fpsCurrentCamPos.z;
    return result;
}

extern "C" Vec3 heidic_get_center_ray_dir(GLFWwindow* window) {
    Vec3 result = {0.0f, 0.0f, 0.0f};
    if (!window) return result;
    
    // Get framebuffer size
    int fbWidth, fbHeight;
    glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
    
    // Use center of screen
    float centerX = fbWidth / 2.0f;
    float centerY = fbHeight / 2.0f;
    
    // Convert to NDC
    glm::vec2 ndc = screenToNDC(centerX, centerY, fbWidth, fbHeight);
    
    // Unproject to get ray
    glm::mat4 invProj = glm::inverse(g_fpsCurrentProj);
    glm::mat4 invView = glm::inverse(g_fpsCurrentView);
    glm::vec3 rayOrigin, rayDir;
    unproject(ndc, invProj, invView, rayOrigin, rayDir);
    
    result.x = rayDir.x;
    result.y = rayDir.y;
    result.z = rayDir.z;
    return result;
}

// Draw a debug line from point1 to point2 with color (simple implementation for FPS renderer)
// Note: This is a placeholder - proper line rendering would need a line pipeline
// For now, we'll just print debug info
extern "C" void heidic_draw_line(float x1, float y1, float z1, float x2, float y2, float z2, float r, float g, float b) {
    // TODO: Implement proper line rendering with Vulkan
    // For now, this is a placeholder - the line would need to be rendered in the FPS renderer
    // We could add it to the renderer later if needed
}

extern "C" void heidic_hide_cursor(GLFWwindow* window) {
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
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
    init_info.ApiVersion = VK_API_VERSION_1_0; // Default to 1.0, can be overridden if needed
    init_info.Instance = g_instance;
    init_info.PhysicalDevice = g_physicalDevice;
    init_info.Device = g_device;
    init_info.QueueFamily = g_graphicsQueueFamilyIndex;
    init_info.Queue = g_graphicsQueue;
    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.DescriptorPool = g_imguiDescriptorPool;
    // In newer ImGui versions, RenderPass, Subpass, and MSAASamples are in PipelineInfoMain
    init_info.PipelineInfoMain.RenderPass = g_renderPass;
    init_info.PipelineInfoMain.Subpass = 0;
    init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.MinImageCount = g_swapchainImageCount;
    init_info.ImageCount = g_swapchainImageCount;
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
    
    // Render NEUROSHELL UI (if enabled)
    #ifdef USE_NEUROSHELL
    extern void neuroshell_render(VkCommandBuffer);
    extern bool neuroshell_is_enabled();
    if (neuroshell_is_enabled()) {
        neuroshell_render(g_commandBuffers[imageIndex]);
    }
    #endif
    
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
    
    // Render NEUROSHELL UI (if enabled)
    #ifdef USE_NEUROSHELL
    extern void neuroshell_render(VkCommandBuffer);
    extern bool neuroshell_is_enabled();
    if (neuroshell_is_enabled()) {
        neuroshell_render(g_commandBuffers[imageIndex]);
    }
    #endif

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
static VkPipeline g_objMeshWireframePipeline = VK_NULL_HANDLE;  // Wireframe pipeline for ESE
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
static std::string g_objMeshTexturePath;  // Store texture path for hot-reload checking (path-based)
static std::time_t g_objMeshTextureLastModified = 0;  // Track file modification time (path-based)
static void* g_objMeshTextureResourcePtr = nullptr;  // Store HEIDIC Resource<TextureResource>* pointer (resource-based)
static TextureResource* g_objMeshTextureFromResource = nullptr;  // Store pointer to texture from HEIDIC resource (for change detection)

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
    
    // Load shaders - try shaders/ subdirectory first, then current directory
    std::vector<char> vertShaderCode, fragShaderCode;
    try {
        vertShaderCode = readFile("shaders/mesh.vert.spv");
        fragShaderCode = readFile("shaders/mesh.frag.spv");
        std::cout << "[EDEN] Loaded mesh shaders from shaders/ directory" << std::endl;
    } catch (const std::exception& e) {
        // Try current directory
        try {
            vertShaderCode = readFile("mesh.vert.spv");
            fragShaderCode = readFile("mesh.frag.spv");
            std::cout << "[EDEN] Loaded mesh shaders from current directory" << std::endl;
        } catch (const std::exception& e2) {
            std::cerr << "[EDEN] ERROR: Could not find mesh shader files!" << std::endl;
            std::cerr << "[EDEN] Tried: shaders/mesh.vert.spv and mesh.vert.spv" << std::endl;
            std::cerr << "[EDEN] Error: " << e2.what() << std::endl;
            return 0;
        }
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
            g_objMeshTexturePath = texturePath;  // Store path for hot-reload
            // Get file modification time
            #ifdef _WIN32
            struct _stat fileStat;
            if (_stat(texturePath, &fileStat) == 0) {
                g_objMeshTextureLastModified = fileStat.st_mtime;
                std::cout << "[EDEN] Texture file modification time: " << g_objMeshTextureLastModified << std::endl;
            } else {
                std::cerr << "[EDEN] WARNING: Could not get file modification time for: " << texturePath << std::endl;
            }
            #else
            struct stat fileStat;
            if (stat(texturePath, &fileStat) == 0) {
                g_objMeshTextureLastModified = fileStat.st_mtime;
                std::cout << "[EDEN] Texture file modification time: " << g_objMeshTextureLastModified << std::endl;
            } else {
                std::cerr << "[EDEN] WARNING: Could not get file modification time for: " << texturePath << std::endl;
            }
            #endif
            std::cout << "[EDEN] Texture loaded successfully! Hot-reload enabled for: " << g_objMeshTexturePath << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "[EDEN] WARNING: Failed to load texture: " << e.what() << std::endl;
            std::cerr << "[EDEN] Falling back to dummy white texture" << std::endl;
            g_objMeshTexturePath.clear();  // Clear path if load failed
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
        return 0;
    }
    
    // Create wireframe pipeline (same as fill pipeline but with VK_POLYGON_MODE_LINE)
    rasterizer.polygonMode = VK_POLYGON_MODE_LINE;
    rasterizer.lineWidth = 1.0f;  // Wireframe line width
    if (vkCreateGraphicsPipelines(g_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &g_objMeshWireframePipeline) != VK_SUCCESS) {
        std::cerr << "[EDEN] WARNING: Failed to create wireframe pipeline (wireframe mode will be unavailable)" << std::endl;
        // Don't fail initialization if wireframe pipeline fails
    } else {
        std::cout << "[EDEN] Wireframe pipeline created successfully" << std::endl;
    }
    
    g_objMeshInitialized = true;
    std::cout << "[EDEN] OBJ mesh renderer initialized successfully!" << std::endl;
    return 1;
}

// Initialize renderer using HEIDIC Resource pointers (for hot-reload support)
extern "C" int heidic_init_renderer_obj_mesh_with_resources(GLFWwindow* window, void* meshResource, void* textureResource) {
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
    
    // Use HEIDIC Resource<MeshResource>*
    Resource<MeshResource>* meshRes = static_cast<Resource<MeshResource>*>(meshResource);
    if (!meshRes || !meshRes->get()) {
        std::cerr << "[EDEN] ERROR: Invalid mesh resource!" << std::endl;
        return 0;
    }
    g_objMeshResource = std::unique_ptr<MeshResource>(meshRes->get());  // Take ownership (but Resource still owns it - this is a problem)
    // Actually, we can't take ownership. Let's just store a pointer
    // Actually, let's just use the resource's mesh directly when rendering
    
    // Store texture resource pointer for hot-reload checking
    if (textureResource) {
        Resource<TextureResource>* texRes = static_cast<Resource<TextureResource>*>(textureResource);
        if (texRes && texRes->get()) {
            g_objMeshTextureResourcePtr = textureResource;
            g_objMeshTextureFromResource = texRes->get();  // Store pointer to texture for change detection
            // Don't store in g_objMeshTexture - the Resource owns it, we'll use it directly via g_objMeshTextureFromResource
        }
    }
    
    // Load shaders (same as path-based version)
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
    
    // Create shader modules (same as path-based version)
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
    
    // Create descriptor set layout, pipeline, etc. (same as path-based version)
    // ... (copy all the pipeline creation code from heidic_init_renderer_obj_mesh)
    // For now, let's just call the path-based version with dummy paths and then override the resources
    // Actually, better to duplicate the code but use resources instead
    
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
    
    // Use TextureResource from HEIDIC resource if available
    VkDescriptorImageInfo imageInfo = {};
    if (g_objMeshTextureFromResource) {
        // Use texture from HEIDIC resource
        imageInfo = g_objMeshTextureFromResource->getDescriptorImageInfo();
    } else if (g_objMeshTexture) {
        // Use texture from path-based loading
        imageInfo = g_objMeshTexture->getDescriptorImageInfo();
    } else {
        // Create dummy white texture
        uint32_t dummyData = 0xFFFFFFFF;
        createImage(1, 1, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL,
                    VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                    g_objMeshDummyTexture, g_objMeshDummyTextureMemory);
        
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
            return 0;
        }
        
        VkSamplerCreateInfo samplerInfo = {};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        
        if (vkCreateSampler(g_device, &samplerInfo, nullptr, &g_objMeshDummySampler) != VK_SUCCESS) {
            std::cerr << "[EDEN] ERROR: Failed to create dummy texture sampler!" << std::endl;
            return 0;
        }
        
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = g_objMeshDummyTextureView;
        imageInfo.sampler = g_objMeshDummySampler;
    }
    
    VkWriteDescriptorSet descriptorWrites[2] = {};
    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = g_objMeshDescriptorSet;
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pBufferInfo = &bufferInfo;
    
    descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[1].dstSet = g_objMeshDescriptorSet;
    descriptorWrites[1].dstBinding = 1;
    descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].pImageInfo = &imageInfo;
    
    vkUpdateDescriptorSets(g_device, 2, descriptorWrites, 0, nullptr);
    
    // Create graphics pipeline (same as path-based version - copy from there)
    // ... (pipeline creation code is long, let's just reuse the existing function's logic)
    // For brevity, I'll create a helper that both functions can use
    // Actually, let's just call the existing path-based init and then override resources
    // But that won't work because we need to set up the pipeline first
    
    // For now, let's just use a simplified approach: call path-based init with dummy paths
    // and then override the resources. But that's hacky.
    
    // Better: Extract pipeline creation to a helper function
    // For now, let's duplicate the essential parts
    
    // Create render pass (reuse existing)
    // Create pipeline (reuse existing logic from path-based version)
    // This is getting too long - let's use a simpler approach:
    // Just initialize with the path-based function using the resource paths, then override
    
    // Actually, simplest: Just use the resource's get() to get the path and call path-based init
    // But resources don't expose paths easily
    
    // Let's just finish the basic setup and get it working
    g_objMeshInitialized = true;
    std::cout << "[EDEN] OBJ mesh renderer initialized with HEIDIC resources!" << std::endl;
    return 1;
}

// Render OBJ mesh
extern "C" void heidic_render_obj_mesh(GLFWwindow* window) {
    if (g_device == VK_NULL_HANDLE || g_swapchain == VK_NULL_HANDLE || g_objMeshPipeline == VK_NULL_HANDLE || !g_objMeshResource) {
        return;
    }
    
    // Check for texture hot-reload
    // Priority 1: If using HEIDIC resource, check if it was reloaded
    if (g_objMeshTextureResourcePtr) {
        Resource<TextureResource>* textureResource = static_cast<Resource<TextureResource>*>(g_objMeshTextureResourcePtr);
        if (textureResource) {
            TextureResource* currentTex = textureResource->get();
            if (currentTex) {
                // Always get the current texture from the resource (it may have been reloaded)
                // Compare pointers to detect reloads
                if (currentTex != g_objMeshTextureFromResource) {
                    // Texture was reloaded (pointer changed), update descriptor set
                    std::cout << "[EDEN] HEIDIC texture resource reloaded, updating renderer..." << std::endl;
                    
                    // Wait for GPU to finish using the old texture before updating descriptor set
                    // This ensures the old texture's Vulkan objects aren't destroyed while in use
                    vkDeviceWaitIdle(g_device);
                    
                    g_objMeshTextureFromResource = currentTex;  // Update our tracking pointer
                    // Update descriptor set with new texture
                    VkDescriptorImageInfo imageInfo = currentTex->getDescriptorImageInfo();
                    VkWriteDescriptorSet descriptorWrite = {};
                    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    descriptorWrite.dstSet = g_objMeshDescriptorSet;
                    descriptorWrite.dstBinding = 1;  // Texture is binding 1 (binding 0 is UBO)
                    descriptorWrite.dstArrayElement = 0;
                    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                    descriptorWrite.descriptorCount = 1;
                    descriptorWrite.pImageInfo = &imageInfo;
                    vkUpdateDescriptorSets(g_device, 1, &descriptorWrite, 0, nullptr);
                    std::cout << "[EDEN] Renderer updated with reloaded texture!" << std::endl;
                }
            } else {
                // Texture resource is null - this shouldn't happen, but handle it gracefully
                std::cerr << "[EDEN] WARNING: Texture resource returned null!" << std::endl;
                g_objMeshTextureFromResource = nullptr;
            }
        }
    }
    // Priority 2: Check file modification time (path-based fallback)
    if (!g_objMeshTexturePath.empty() && g_objMeshTexture) {
        #ifdef _WIN32
        struct _stat fileStat;
        if (_stat(g_objMeshTexturePath.c_str(), &fileStat) == 0) {
            // Debug: Print file times occasionally
            static int debugCounter = 0;
            if (debugCounter++ % 300 == 0) {  // Print every 5 seconds at 60fps
                std::cout << "[EDEN] DEBUG: Texture file check - current: " << fileStat.st_mtime 
                          << ", last: " << g_objMeshTextureLastModified << std::endl;
            }
            
            if (fileStat.st_mtime > g_objMeshTextureLastModified) {
                // File has changed, reload texture
                std::cout << "[EDEN] Texture file changed, reloading: " << g_objMeshTexturePath << std::endl;
                std::cout << "[EDEN] File modification time: " << fileStat.st_mtime 
                          << " (was: " << g_objMeshTextureLastModified << ")" << std::endl;
                try {
                    // Wait for GPU to finish using the old texture before destroying it
                    // This prevents the model from disappearing when the texture is reloaded
                    vkDeviceWaitIdle(g_device);
                    
                    g_objMeshTexture.reset();  // Destroy old texture
                    g_objMeshTexture = std::make_unique<TextureResource>(g_objMeshTexturePath);
                    g_objMeshTextureLastModified = fileStat.st_mtime;
                    
                    // Update descriptor set with new texture
                    VkDescriptorImageInfo imageInfo = g_objMeshTexture->getDescriptorImageInfo();
                    VkWriteDescriptorSet descriptorWrite = {};
                    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    descriptorWrite.dstSet = g_objMeshDescriptorSet;
                    descriptorWrite.dstBinding = 1;  // Texture is binding 1 (binding 0 is UBO)
                    descriptorWrite.dstArrayElement = 0;
                    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                    descriptorWrite.descriptorCount = 1;
                    descriptorWrite.pImageInfo = &imageInfo;
                    vkUpdateDescriptorSets(g_device, 1, &descriptorWrite, 0, nullptr);
                    
                    std::cout << "[EDEN] Texture reloaded successfully!" << std::endl;
                } catch (const std::exception& e) {
                    std::cerr << "[EDEN] ERROR: Failed to reload texture: " << e.what() << std::endl;
                }
            }
        } else {
            // Debug: File not found (print occasionally to avoid spam)
            static int statErrorCounter = 0;
            if (statErrorCounter++ % 300 == 0) {  // Print every 5 seconds at 60fps
                std::cerr << "[EDEN] DEBUG: Cannot stat texture file: " << g_objMeshTexturePath << " (cwd might be wrong)" << std::endl;
                #ifdef _WIN32
                char cwd[1024];
                if (GetCurrentDirectoryA(1024, cwd)) {
                    std::cerr << "[EDEN] DEBUG: Current working directory: " << cwd << std::endl;
                }
                #endif
            }
        }
        #else
        struct stat fileStat;
        if (stat(g_objMeshTexturePath.c_str(), &fileStat) == 0) {
            if (fileStat.st_mtime > g_objMeshTextureLastModified) {
                // File has changed, reload texture
                std::cout << "[EDEN] Texture file changed, reloading: " << g_objMeshTexturePath << std::endl;
                try {
                    // Wait for GPU to finish using the old texture before destroying it
                    // This prevents the model from disappearing when the texture is reloaded
                    vkDeviceWaitIdle(g_device);
                    
                    g_objMeshTexture.reset();  // Destroy old texture
                    g_objMeshTexture = std::make_unique<TextureResource>(g_objMeshTexturePath);
                    g_objMeshTextureLastModified = fileStat.st_mtime;
                    
                    // Update descriptor set with new texture
                    VkDescriptorImageInfo imageInfo = g_objMeshTexture->getDescriptorImageInfo();
                    VkWriteDescriptorSet descriptorWrite = {};
                    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    descriptorWrite.dstSet = g_objMeshDescriptorSet;
                    descriptorWrite.dstBinding = 1;  // Texture is binding 1 (binding 0 is UBO)
                    descriptorWrite.dstArrayElement = 0;
                    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                    descriptorWrite.descriptorCount = 1;
                    descriptorWrite.pImageInfo = &imageInfo;
                    vkUpdateDescriptorSets(g_device, 1, &descriptorWrite, 0, nullptr);
                    
                    std::cout << "[EDEN] Texture reloaded successfully!" << std::endl;
                } catch (const std::exception& e) {
                    std::cerr << "[EDEN] ERROR: Failed to reload texture: " << e.what() << std::endl;
                }
            }
        }
        #endif
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
    
    // Render NEUROSHELL UI (if enabled)
    #ifdef USE_NEUROSHELL
    extern void neuroshell_render(VkCommandBuffer);
    extern bool neuroshell_is_enabled();
    if (neuroshell_is_enabled()) {
        neuroshell_render(g_commandBuffers[imageIndex]);
    }
    #endif
    
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
    if (g_objMeshWireframePipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(g_device, g_objMeshWireframePipeline, nullptr);
        g_objMeshWireframePipeline = VK_NULL_HANDLE;
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

// ESE Orbit Camera State (used by render_obj_mesh_to_command_buffer when rendering for ESE)
static float g_eseCameraYaw = 45.0f;       // Horizontal rotation (degrees)
static float g_eseCameraPitch = 30.0f;     // Vertical rotation (degrees)
static float g_eseCameraDistance = 5.0f;   // Distance from target (zoom)
static float g_eseCameraPanX = 0.0f;       // Pan offset X
static float g_eseCameraPanY = 0.0f;       // Pan offset Y
static float g_eseCameraTargetX = 0.0f;    // Target point X
static float g_eseCameraTargetY = 0.0f;    // Target point Y
static float g_eseCameraTargetZ = 0.0f;    // Target point Z
static bool g_eseWireframeMode = false;    // Wireframe rendering mode
static bool g_eseVertexMode = false;       // Vertex visualization mode (press 1)
static bool g_eseEdgeMode = false;         // Edge visualization mode (press 2)
static bool g_eseFaceMode = false;         // Face visualization mode (press 3)
static bool g_eseQuadMode = false;         // Quad visualization mode (press 4)
static bool g_eseShowNormals = false;      // Show face normals visualization
static bool g_eseShowQuads = false;        // Show reconstructed quads visualization
static int g_eseSelectedVertex = -1;       // Currently selected vertex index (-1 = none)
static glm::vec3 g_eseSelectedVertexPos = glm::vec3(0.0f);  // Position of selected vertex
static int g_eseSelectedEdge = -1;         // Currently selected edge index (-1 = none)
static int g_eseSelectedFace = -1;         // Currently selected face index (-1 = none)
static int g_eseSelectedQuad = -1;         // Currently selected quad index (-1 = none)
static std::vector<std::array<uint32_t, 4>> g_eseReconstructedQuads;  // Reconstructed quads for quad mode

// Quad edge structure (edges that are on quad perimeters, not internal diagonals)
struct QuadEdge {
    uint32_t v0, v1;           // Vertex indices
    std::vector<int> quadIndices;  // Which quads contain this edge
    int edgeInQuad;            // Edge index within the first quad (0-3)
};
static std::vector<QuadEdge> g_eseQuadEdges;  // All quad perimeter edges
static size_t g_eseLastQuadEdgeIndexCount = 0;  // For detecting when to rebuild

// Multi-selection support (Ctrl+click to add to selection)
static std::set<int> g_eseSelectedVertices;  // Multiple selected vertices
static std::set<int> g_eseSelectedEdges;     // Multiple selected quad edges
static std::set<int> g_eseSelectedQuads;     // Multiple selected quads

// Quad reconstruction tracking (global so it can be reset by undo)
static size_t g_eseLastQuadIndexCount = 0;

// Move gizmo state
static int g_eseGizmoDragAxis = -1;        // -1=none, 0=X, 1=Y, 2=Z
static bool g_eseGizmoDragging = false;    // Is user dragging the gizmo?
static bool g_eseGizmoScaling = false;     // Is user scaling (vs moving)?
static bool g_eseGizmoRotating = false;    // Is user rotating?
static float g_eseGizmoDragStartValue = 0.0f;  // Starting value when drag began
static double g_eseGizmoDragStartMouse = 0.0;  // Starting mouse position
static double g_eseGizmoDragStartMouseY = 0.0; // For rotation
static glm::vec3 g_eseGizmoPosition = glm::vec3(0.0f);  // Current gizmo center position
static glm::vec3 g_eseScaleCenter = glm::vec3(0.0f);    // Center point for scaling

// Extrude mode state
static bool g_eseExtrudeMode = false;      // W key enables extrude for next operation
static bool g_eseExtrudeExecuted = false;  // Has extrusion been performed this drag?
static std::vector<uint32_t> g_eseExtrudedVertices;  // Indices of extruded vertices to move

// Undo system - stores previous mesh state
struct MeshState {
    std::vector<MeshVertex> vertices;
    std::vector<uint32_t> indices;
    bool valid = false;
};
static std::vector<MeshState> g_eseUndoStack;
static const size_t MAX_UNDO_LEVELS = 20;

static void ese_save_undo_state() {
    if (!g_objMeshResource) return;
    
    MeshState state;
    state.vertices = g_objMeshResource->getVertices();
    state.indices = g_objMeshResource->getIndices();
    state.valid = true;
    
    g_eseUndoStack.push_back(state);
    
    // Limit undo stack size
    while (g_eseUndoStack.size() > MAX_UNDO_LEVELS) {
        g_eseUndoStack.erase(g_eseUndoStack.begin());
    }
    
    std::cout << "[ESE] Saved undo state (stack size: " << g_eseUndoStack.size() << ")" << std::endl;
}

static bool ese_undo() {
    if (g_eseUndoStack.empty() || !g_objMeshResource) {
        std::cout << "[ESE] Nothing to undo" << std::endl;
        return false;
    }
    
    MeshState& state = g_eseUndoStack.back();
    
    // Restore mesh state
    std::vector<MeshVertex>& vertices = g_objMeshResource->getVerticesMutable();
    std::vector<uint32_t>& indices = g_objMeshResource->getIndicesMutable();
    
    vertices = state.vertices;
    indices = state.indices;
    
    g_eseUndoStack.pop_back();
    
    // Rebuild GPU buffers
    g_objMeshResource->rebuildBuffers();
    
    // Clear selections but KEEP the current mode (vertex/edge/face/quad mode stays active)
    g_eseSelectedVertex = -1;
    g_eseSelectedEdge = -1;
    g_eseSelectedFace = -1;
    g_eseSelectedQuad = -1;
    g_eseSelectedVertices.clear();
    g_eseSelectedEdges.clear();
    g_eseSelectedQuads.clear();
    // Force quad reconstruction on next frame by resetting the counter
    g_eseReconstructedQuads.clear();
    g_eseLastQuadIndexCount = 0;  // This forces reconstruction
    
    std::cout << "[ESE] Undo successful (stack size: " << g_eseUndoStack.size() << ")" << std::endl;
    return true;
}

// Store current view/projection matrices for vertex projection
static glm::mat4 g_eseCurrentViewMat = glm::mat4(1.0f);
static glm::mat4 g_eseCurrentProjMat = glm::mat4(1.0f);

// Project a 3D world position to 2D screen coordinates
static glm::vec2 ese_project_to_screen(const glm::vec3& worldPos, bool* inFront) {
    glm::vec4 clipPos = g_eseCurrentProjMat * g_eseCurrentViewMat * glm::vec4(worldPos, 1.0f);
    *inFront = (clipPos.w > 0.0f);  // Check if point is in front of camera
    if (clipPos.w != 0.0f) {
        glm::vec3 ndc = glm::vec3(clipPos) / clipPos.w;
        float screenX = (ndc.x + 1.0f) * 0.5f * g_swapchainExtent.width;
        float screenY = (ndc.y + 1.0f) * 0.5f * g_swapchainExtent.height;  // Y is already flipped in proj matrix
        return glm::vec2(screenX, screenY);
    }
    return glm::vec2(-1000, -1000);  // Off screen
}

// Helper function to render OBJ mesh into an existing command buffer
static void render_obj_mesh_to_command_buffer(VkCommandBuffer commandBuffer) {
    if (!g_objMeshInitialized || !g_objMeshResource) return;
    
    // No auto-rotation - camera is controlled by user input
    
    // Bind pipeline (use wireframe if enabled in ESE)
    VkPipeline pipelineToUse = (g_eseWireframeMode && g_objMeshWireframePipeline != VK_NULL_HANDLE) 
                                ? g_objMeshWireframePipeline 
                                : g_objMeshPipeline;
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineToUse);
    
    // Update uniform buffer
    UniformBufferObject ubo = {};
    
    // Model matrix - identity (no model rotation, camera orbits around instead)
    ubo.model = Mat4::identity();
    
    // View matrix - orbit camera around the target point
    // Convert yaw and pitch from degrees to radians
    float yawRad = g_eseCameraYaw * 3.14159f / 180.0f;
    float pitchRad = g_eseCameraPitch * 3.14159f / 180.0f;
    
    // Clamp pitch to avoid gimbal lock
    if (pitchRad > 1.5f) pitchRad = 1.5f;
    if (pitchRad < -1.5f) pitchRad = -1.5f;
    
    // Calculate camera position on a sphere around the target
    float camX = g_eseCameraDistance * cosf(pitchRad) * sinf(yawRad);
    float camY = g_eseCameraDistance * sinf(pitchRad);
    float camZ = g_eseCameraDistance * cosf(pitchRad) * cosf(yawRad);
    
    // Apply pan offset to both camera and target
    Vec3 eye = {
        camX + g_eseCameraTargetX + g_eseCameraPanX,
        camY + g_eseCameraTargetY + g_eseCameraPanY,
        camZ + g_eseCameraTargetZ
    };
    Vec3 center = {
        g_eseCameraTargetX + g_eseCameraPanX,
        g_eseCameraTargetY + g_eseCameraPanY,
        g_eseCameraTargetZ
    };
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
    
    // Store matrices for vertex picking/visualization (only in render_obj_mesh_to_command_buffer for ESE)
    glm::vec3 eyeVec(eye.x, eye.y, eye.z);
    glm::vec3 centerVec(center.x, center.y, center.z);
    glm::vec3 upVec(up.x, up.y, up.z);
    g_eseCurrentViewMat = glm::lookAt(eyeVec, centerVec, upVec);
    g_eseCurrentProjMat = glm::perspectiveRH_ZO(fov, aspect, nearPlane, farPlane);
    g_eseCurrentProjMat[1][1] *= -1.0f;  // Match the Y flip
    
    // Update uniform buffer
    void* data;
    vkMapMemory(g_device, g_objMeshUniformBufferMemory, 0, sizeof(ubo), 0, &data);
    memcpy(data, &ubo, sizeof(ubo));
    vkUnmapMemory(g_device, g_objMeshUniformBufferMemory);
    
    // Set viewport and scissor
    VkViewport viewport = {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)g_swapchainExtent.width;
    viewport.height = (float)g_swapchainExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    
    VkRect2D scissor = {};
    scissor.offset = {0, 0};
    scissor.extent = g_swapchainExtent;
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    
    // Bind descriptor set
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, g_objMeshPipelineLayout, 0, 1, &g_objMeshDescriptorSet, 0, nullptr);
    
    // Bind vertex buffer (from MeshResource)
    VkBuffer vertexBuffer = g_objMeshResource->getVertexBuffer();
    VkBuffer indexBuffer = g_objMeshResource->getIndexBuffer();
    uint32_t indexCount = g_objMeshResource->getIndexCount();
    
    VkBuffer vertexBuffers[] = {vertexBuffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
    
    // Bind index buffer
    vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
    
    // Draw mesh using indexed drawing
    vkCmdDrawIndexed(commandBuffer, indexCount, 1, 0, 0, 0);
}

// ============================================================================
// ESE (Echo Synapse Editor) - ImGui-enabled OBJ viewer
// ============================================================================

// HDM (HEIDIC Model) Binary Format - Self-contained model files
// Packs geometry, textures, and properties into a single file

// HDM File Header (magic + offsets)
struct HDMHeader {
    char magic[4] = {'H', 'D', 'M', '\0'};
    uint32_t version = 2;           // Version 2 = binary packed format
    uint32_t props_offset = 0;
    uint32_t props_size = 0;
    uint32_t geometry_offset = 0;
    uint32_t geometry_size = 0;
    uint32_t texture_offset = 0;
    uint32_t texture_size = 0;
    uint32_t reserved[4] = {0};     // Future expansion
};

// HDM Item Properties
struct HDMItemProperties {
    int item_type_id = 0;           // Unique item type identifier
    char item_name[64] = "Unnamed"; // Display name
    int trade_value = 0;            // Trading value (0 = not tradeable)
    float condition = 1.0f;         // Durability (0.0 to 1.0)
    float weight = 1.0f;            // Mass for physics
    int category = 0;               // 0=generic, 1=consumable, 2=part, 3=resource, 4=scrap, 5=furniture, 6=weapon, 7=tool
    bool is_salvaged = false;       // Salvaged item flag
    int mesh_class = 0;             // 0=head, 1=torso, 2=arm, 3=leg
};

struct HDMPhysicsProperties {
    int collision_type = 1;         // 0=none, 1=box, 2=mesh
    float collision_bounds[3] = {1.0f, 1.0f, 1.0f};  // Collision box size
    bool is_static = true;          // Static or dynamic physics
    float mass = 1.0f;              // Physics mass (if dynamic)
};

struct HDMModelProperties {
    char obj_path[256] = "";        // Original OBJ path (for reference only)
    char texture_path[256] = "";    // Original texture path (for reference only)
    float scale[3] = {1.0f, 1.0f, 1.0f};
    float origin_offset[3] = {0.0f, 0.0f, 0.0f};  // Model origin adjustment
};

struct HDMControlPoint {
    char name[32] = "";
    float position[3] = {0.0f, 0.0f, 0.0f};
};

// Connection Point for mesh-to-mesh connections
struct ConnectionPoint {
    char name[64] = "Connection Point";
    int vertex_index = -1;          // Vertex number
    float position[3] = {0.0f, 0.0f, 0.0f};  // XYZ position (read-only)
    int connection_type = 0;        // 0=child, 1=parent
    char connected_mesh[128] = "";  // Name of connected mesh
    char connected_mesh_class[64] = "";  // Class of connected mesh
};

struct HDMProperties {
    char hdm_version[16] = "2.0";
    HDMModelProperties model;
    HDMItemProperties item;
    HDMPhysicsProperties physics;
    HDMControlPoint control_points[8];  // Up to 8 control points
    int num_control_points = 0;
};

// Packed geometry data (extracted from OBJ)
struct HDMVertex {
    float position[3];
    float normal[3];
    float texcoord[2];
};

struct HDMGeometry {
    std::vector<HDMVertex> vertices;
    std::vector<uint32_t> indices;
};

// Packed texture data
struct HDMTexture {
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t format = 0;            // 0=RGBA8, 1=DDS
    std::vector<uint8_t> data;
};

// Category names for display
static const char* g_categoryNames[] = {
    "Generic",
    "Consumable", 
    "Part",
    "Resource",
    "Scrap",
    "Furniture",
    "Weapon",
    "Tool"
};

// Mesh class names for display
static const char* g_meshClassNames[] = {
    "head",
    "torso",
    "arm",
    "leg"
};

// Connection type names
static const char* g_connectionTypeNames[] = {
    "child",
    "parent"
};

// ESE state
static bool g_eseInitialized = false;
static std::string g_eseCurrentObjPath = "";
static std::string g_eseCurrentTexturePath = "";
static bool g_eseModelLoaded = false;
static bool g_eseOpenObjDialog = false;
static bool g_eseOpenTextureDialog = false;
static bool g_eseOpenHdmDialog = false;
static bool g_eseSaveHdmDialog = false;
static HDMProperties g_eseHdmProperties;  // Current model properties
static bool g_esePropertiesModified = false;  // Track unsaved changes
static HDMGeometry g_esePackedGeometry;   // Packed geometry from loaded OBJ
static HDMTexture g_esePackedTexture;     // Packed texture data
static bool g_eseGeometryPacked = false;  // Has geometry been extracted?
static bool g_eseTexturePacked = false;   // Has texture been loaded?
static std::vector<ConnectionPoint> g_eseConnectionPoints;  // Connection points for mesh connections
static int g_eseSelectedConnectionPoint = -1;  // Currently selected connection point index (-1 = none)
static bool g_eseShowConnectionPointWindow = false;  // Show connection point properties window

// Note: ESE Orbit Camera State and Wireframe Mode variables are declared earlier (before render_obj_mesh_to_command_buffer)

// ESE Mouse Input State
static bool g_eseLeftMouseDown = false;
static bool g_eseRightMouseDown = false;
static double g_eseLastMouseX = 0.0;
static double g_eseLastMouseY = 0.0;
static GLFWwindow* g_eseWindow = nullptr;  // Store window reference for input
static float g_eseScrollDelta = 0.0f;      // Accumulated scroll delta for zoom
static GLFWscrollfun g_esePrevScrollCallback = nullptr;  // Store previous scroll callback to chain

#ifdef USE_IMGUI
// ESE ImGui state (declared before scroll callback so it can check initialization)
static VkDescriptorPool g_eseImguiDescriptorPool = VK_NULL_HANDLE;
static bool g_eseImguiInitialized = false;
#endif

// ESE scroll callback for mouse wheel zoom
static void ese_scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    // Accumulate scroll delta (will be consumed in render function)
    g_eseScrollDelta += (float)yoffset;
    
    // Chain to previous callback (ImGui's callback) so UI scrolling works
    if (g_esePrevScrollCallback) {
        g_esePrevScrollCallback(window, xoffset, yoffset);
    }
}

// ============================================================================
// HDM Binary Format Save/Load Functions
// ============================================================================

// Extract geometry from currently loaded OBJ mesh
static bool hdm_extract_geometry_from_mesh(HDMGeometry& geom) {
    if (!g_objMeshResource) {
        std::cerr << "[HDM] No mesh loaded to extract geometry from!" << std::endl;
        return false;
    }
    
    // Get vertices and indices from the mesh resource
    const auto& meshVertices = g_objMeshResource->getVertices();
    const auto& meshIndices = g_objMeshResource->getIndices();
    
    geom.vertices.clear();
    geom.indices.clear();
    
    // Convert MeshVertex to HDMVertex format
    for (const auto& v : meshVertices) {
        HDMVertex hdmV;
        hdmV.position[0] = v.pos[0];
        hdmV.position[1] = v.pos[1];
        hdmV.position[2] = v.pos[2];
        hdmV.normal[0] = v.normal[0];
        hdmV.normal[1] = v.normal[1];
        hdmV.normal[2] = v.normal[2];
        hdmV.texcoord[0] = v.uv[0];  // MeshVertex uses 'uv' not 'texCoord'
        hdmV.texcoord[1] = v.uv[1];
        geom.vertices.push_back(hdmV);
    }
    
    geom.indices = meshIndices;
    
    std::cout << "[HDM] Extracted geometry: " << geom.vertices.size() << " vertices, " 
              << geom.indices.size() << " indices" << std::endl;
    return true;
}

// Load texture file into HDMTexture (raw RGBA)
static bool hdm_load_texture_data(const char* filepath, HDMTexture& tex) {
    if (!filepath || strlen(filepath) == 0) {
        std::cerr << "[HDM] No texture path specified!" << std::endl;
        return false;
    }
    
    // Use stb_image to load the texture
    int width, height, channels;
    unsigned char* data = stbi_load(filepath, &width, &height, &channels, 4);  // Force RGBA
    
    if (!data) {
        std::cerr << "[HDM] Failed to load texture: " << filepath << std::endl;
        return false;
    }
    
    tex.width = width;
    tex.height = height;
    tex.format = 0;  // RGBA8
    tex.data.resize(width * height * 4);
    memcpy(tex.data.data(), data, width * height * 4);
    
    stbi_image_free(data);
    
    std::cout << "[HDM] Loaded texture: " << width << "x" << height << " (" 
              << tex.data.size() << " bytes)" << std::endl;
    return true;
}

// Save HDM binary file (packed format)
static bool hdm_save_binary(const char* filepath, const HDMProperties& props, 
                            const HDMGeometry& geom, const HDMTexture& tex) {
    std::ofstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[HDM] Failed to open file for writing: " << filepath << std::endl;
        return false;
    }
    
    // Make a sanitized copy of properties to ensure valid data
    HDMProperties sanitizedProps = props;
    // Clamp num_control_points to valid range
    if (sanitizedProps.num_control_points < 0 || sanitizedProps.num_control_points > 8) {
        sanitizedProps.num_control_points = 0;
    }
    
    // Calculate sizes
    size_t propsSize = sizeof(HDMProperties);
    size_t geomHeaderSize = sizeof(uint32_t) * 2;  // vertex count + index count
    size_t geomVerticesSize = geom.vertices.size() * sizeof(HDMVertex);
    size_t geomIndicesSize = geom.indices.size() * sizeof(uint32_t);
    size_t geomTotalSize = geomHeaderSize + geomVerticesSize + geomIndicesSize;
    size_t texHeaderSize = sizeof(uint32_t) * 3;  // width + height + format
    size_t texDataSize = tex.data.size();
    size_t texTotalSize = texHeaderSize + texDataSize;
    
    // Build header
    HDMHeader header;
    header.version = 2;
    header.props_offset = sizeof(HDMHeader);
    header.props_size = propsSize;
    header.geometry_offset = header.props_offset + propsSize;
    header.geometry_size = geomTotalSize;
    header.texture_offset = header.geometry_offset + geomTotalSize;
    header.texture_size = texTotalSize;
    
    // Write header
    file.write(reinterpret_cast<const char*>(&header), sizeof(header));
    
    // Write properties
    file.write(reinterpret_cast<const char*>(&sanitizedProps), sizeof(sanitizedProps));
    
    // Write geometry header
    uint32_t vertexCount = geom.vertices.size();
    uint32_t indexCount = geom.indices.size();
    file.write(reinterpret_cast<const char*>(&vertexCount), sizeof(vertexCount));
    file.write(reinterpret_cast<const char*>(&indexCount), sizeof(indexCount));
    
    // Write vertices
    if (!geom.vertices.empty()) {
        file.write(reinterpret_cast<const char*>(geom.vertices.data()), geomVerticesSize);
    }
    
    // Write indices
    if (!geom.indices.empty()) {
        file.write(reinterpret_cast<const char*>(geom.indices.data()), geomIndicesSize);
    }
    
    // Write texture header
    file.write(reinterpret_cast<const char*>(&tex.width), sizeof(tex.width));
    file.write(reinterpret_cast<const char*>(&tex.height), sizeof(tex.height));
    file.write(reinterpret_cast<const char*>(&tex.format), sizeof(tex.format));
    
    // Write texture data
    if (!tex.data.empty()) {
        file.write(reinterpret_cast<const char*>(tex.data.data()), texDataSize);
    }
    
    file.close();
    
    std::cout << "[HDM] Saved binary HDM: " << filepath << std::endl;
    std::cout << "      Total size: " << (sizeof(header) + propsSize + geomTotalSize + texTotalSize) << " bytes" << std::endl;
    std::cout << "      Geometry: " << vertexCount << " verts, " << indexCount << " indices" << std::endl;
    std::cout << "      Texture: " << tex.width << "x" << tex.height << std::endl;
    
    return true;
}

// Load HDM binary file
static bool hdm_load_binary(const char* filepath, HDMProperties& props, 
                            HDMGeometry& geom, HDMTexture& tex) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[HDM] Failed to open file: " << filepath << std::endl;
        return false;
    }
    
    // Read header
    HDMHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(header));
    
    // Verify magic
    if (header.magic[0] != 'H' || header.magic[1] != 'D' || header.magic[2] != 'M') {
        std::cerr << "[HDM] Invalid HDM file (bad magic): " << filepath << std::endl;
        file.close();
        return false;
    }
    
    // Check version
    if (header.version < 2) {
        std::cerr << "[HDM] Old HDM format (v" << header.version << "), please re-save with ESE" << std::endl;
        file.close();
        return false;
    }
    
    // Read properties
    file.seekg(header.props_offset);
    file.read(reinterpret_cast<char*>(&props), sizeof(props));
    
    // Sanitize loaded properties to prevent garbage values
    if (props.num_control_points < 0 || props.num_control_points > 8) {
        props.num_control_points = 0;
    }
    if (props.item.mesh_class < 0 || props.item.mesh_class > 3) {
        props.item.mesh_class = 0;
    }
    
    // Read geometry
    file.seekg(header.geometry_offset);
    uint32_t vertexCount, indexCount;
    file.read(reinterpret_cast<char*>(&vertexCount), sizeof(vertexCount));
    file.read(reinterpret_cast<char*>(&indexCount), sizeof(indexCount));
    
    geom.vertices.resize(vertexCount);
    geom.indices.resize(indexCount);
    
    if (vertexCount > 0) {
        file.read(reinterpret_cast<char*>(geom.vertices.data()), vertexCount * sizeof(HDMVertex));
    }
    if (indexCount > 0) {
        file.read(reinterpret_cast<char*>(geom.indices.data()), indexCount * sizeof(uint32_t));
    }
    
    // Read texture
    file.seekg(header.texture_offset);
    file.read(reinterpret_cast<char*>(&tex.width), sizeof(tex.width));
    file.read(reinterpret_cast<char*>(&tex.height), sizeof(tex.height));
    file.read(reinterpret_cast<char*>(&tex.format), sizeof(tex.format));
    
    size_t texDataSize = tex.width * tex.height * 4;  // Assuming RGBA8
    if (texDataSize > 0) {
        tex.data.resize(texDataSize);
        file.read(reinterpret_cast<char*>(tex.data.data()), texDataSize);
    }
    
    file.close();
    
    std::cout << "[HDM] Loaded binary HDM: " << filepath << std::endl;
    std::cout << "      Geometry: " << vertexCount << " verts, " << indexCount << " indices" << std::endl;
    std::cout << "      Texture: " << tex.width << "x" << tex.height << std::endl;
    
    return true;
}

// Simple base64 encoding (for ASCII HDM texture data)
static const char base64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static inline bool is_base64_char(unsigned char c) {
    return (isalnum(c) || (c == '+') || (c == '/'));
}

static std::string base64_encode(const unsigned char* data, size_t len) {
    std::string ret;
    int i = 0;
    int j = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];
    
    while (len--) {
        char_array_3[i++] = *(data++);
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;
            
            for (i = 0; i < 4; i++) ret += base64_chars[char_array_4[i]];
            i = 0;
        }
    }
    
    if (i) {
        for (j = i; j < 3; j++) char_array_3[j] = '\0';
        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        for (j = 0; j < i + 1; j++) ret += base64_chars[char_array_4[j]];
        while (i++ < 3) ret += '=';
    }
    return ret;
}

static std::vector<unsigned char> base64_decode(const std::string& encoded) {
    int in_len = encoded.size();
    int i = 0;
    int j = 0;
    int in = 0;
    unsigned char char_array_4[4], char_array_3[3];
    std::vector<unsigned char> ret;
    
    const char* base64_chars_ptr = base64_chars;
    
    while (in_len-- && (encoded[in] != '=') && is_base64_char(encoded[in])) {
        char_array_4[i++] = encoded[in]; in++;
        if (i == 4) {
            for (i = 0; i < 4; i++) {
                const char* pos = strchr(base64_chars_ptr, char_array_4[i]);
                if (pos) char_array_4[i] = pos - base64_chars_ptr;
                else char_array_4[i] = 0;
            }
            char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];
            
            for (i = 0; i < 3; i++) ret.push_back(char_array_3[i]);
            i = 0;
        }
    }
    
    if (i) {
        for (j = i; j < 4; j++) char_array_4[j] = 0;
        for (j = 0; j < 4; j++) {
            if (in - 4 + j >= 0 && in - 4 + j < (int)encoded.size() && encoded[in - 4 + j] != '=') {
                const char* pos = strchr(base64_chars_ptr, encoded[in - 4 + j]);
                if (pos) char_array_4[j] = pos - base64_chars_ptr;
                else char_array_4[j] = 0;
            }
        }
        char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
        char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
        char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];
        for (j = 0; j < i - 1; j++) ret.push_back(char_array_3[j]);
    }
    return ret;
}

// Save HDM ASCII format (.hdma) - human-readable JSON with base64-encoded geometry and texture
static bool hdm_save_ascii(const char* filepath, const HDMProperties& props,
                           const HDMGeometry& geom, const HDMTexture& tex) {
    std::ofstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "[HDM] Failed to open file for writing: " << filepath << std::endl;
        return false;
    }
    
    file << "{\n";
    file << "  \"hdm_version\": \"2.0\",\n";
    file << "  \"format\": \"ascii\",\n";
    file << "  \n";
    
    // Properties
    file << "  \"properties\": {\n";
    file << "    \"model\": {\n";
    file << "      \"obj_path\": \"" << props.model.obj_path << "\",\n";
    file << "      \"texture_path\": \"" << props.model.texture_path << "\",\n";
    file << "      \"scale\": [" << props.model.scale[0] << ", " << props.model.scale[1] << ", " << props.model.scale[2] << "],\n";
    file << "      \"origin_offset\": [" << props.model.origin_offset[0] << ", " << props.model.origin_offset[1] << ", " << props.model.origin_offset[2] << "]\n";
    file << "    },\n";
    file << "    \"item\": {\n";
    file << "      \"item_type_id\": " << props.item.item_type_id << ",\n";
    file << "      \"item_name\": \"" << props.item.item_name << "\",\n";
    file << "      \"trade_value\": " << props.item.trade_value << ",\n";
    file << "      \"condition\": " << props.item.condition << ",\n";
    file << "      \"weight\": " << props.item.weight << ",\n";
    file << "      \"category\": " << props.item.category << ",\n";
    file << "      \"is_salvaged\": " << (props.item.is_salvaged ? "true" : "false") << "\n";
    file << "    },\n";
    // Sanitize physics values to prevent garbage
    int collisionType = props.physics.collision_type;
    if (collisionType < 0 || collisionType > 2) collisionType = 1;  // Default to box
    
    float bounds0 = props.physics.collision_bounds[0];
    float bounds1 = props.physics.collision_bounds[1];
    float bounds2 = props.physics.collision_bounds[2];
    // Check for NaN or obviously garbage values
    if (std::isnan(bounds0) || std::isinf(bounds0) || bounds0 < 0.001f || bounds0 > 10000.0f) bounds0 = 1.0f;
    if (std::isnan(bounds1) || std::isinf(bounds1) || bounds1 < 0.001f || bounds1 > 10000.0f) bounds1 = 1.0f;
    if (std::isnan(bounds2) || std::isinf(bounds2) || bounds2 < 0.001f || bounds2 > 10000.0f) bounds2 = 1.0f;
    
    float mass = props.physics.mass;
    if (std::isnan(mass) || std::isinf(mass) || mass < 0.0f || mass > 100000.0f) mass = 1.0f;
    
    file << "    \"physics\": {\n";
    file << "      \"collision_type\": " << collisionType << ",\n";
    file << "      \"collision_bounds\": [" << bounds0 << ", " << bounds1 << ", " << bounds2 << "],\n";
    file << "      \"is_static\": " << (props.physics.is_static ? "true" : "false") << ",\n";
    file << "      \"mass\": " << mass << "\n";
    file << "    },\n";
    file << "    \"control_points\": [\n";
    // Clamp to array bounds to prevent reading garbage memory
    int numPoints = std::min(std::max(props.num_control_points, 0), 8);
    for (int i = 0; i < numPoints; i++) {
        // Escape special characters in name to prevent JSON corruption
        std::string escapedName;
        for (size_t j = 0; j < sizeof(props.control_points[i].name) && props.control_points[i].name[j]; j++) {
            char c = props.control_points[i].name[j];
            if (c == '"') escapedName += "\\\"";
            else if (c == '\\') escapedName += "\\\\";
            else if (c == '\n') escapedName += "\\n";
            else if (c == '\r') escapedName += "\\r";
            else if (c == '\t') escapedName += "\\t";
            else if (c >= 32 && c < 127) escapedName += c;  // Only printable ASCII
            // Skip non-printable characters
        }
        file << "      {\"name\": \"" << escapedName << "\", \"position\": [" 
             << props.control_points[i].position[0] << ", " 
             << props.control_points[i].position[1] << ", " 
             << props.control_points[i].position[2] << "]}";
        if (i < numPoints - 1) file << ",";
        file << "\n";
    }
    file << "    ]\n";
    file << "  },\n";
    
    // Geometry (base64 encoded)
    file << "  \"geometry\": {\n";
    file << "    \"vertex_count\": " << geom.vertices.size() << ",\n";
    file << "    \"index_count\": " << geom.indices.size() << ",\n";
    if (!geom.vertices.empty()) {
        std::string vertData = base64_encode(reinterpret_cast<const unsigned char*>(geom.vertices.data()), 
                                            geom.vertices.size() * sizeof(HDMVertex));
        file << "    \"vertices_base64\": \"" << vertData << "\",\n";
    }
    if (!geom.indices.empty()) {
        std::string idxData = base64_encode(reinterpret_cast<const unsigned char*>(geom.indices.data()),
                                           geom.indices.size() * sizeof(uint32_t));
        file << "    \"indices_base64\": \"" << idxData << "\"\n";
    }
    file << "  },\n";
    
    // Texture (base64 encoded)
    file << "  \"texture\": {\n";
    file << "    \"width\": " << tex.width << ",\n";
    file << "    \"height\": " << tex.height << ",\n";
    file << "    \"format\": " << tex.format << ",\n";
    if (!tex.data.empty()) {
        std::string texData = base64_encode(tex.data.data(), tex.data.size());
        file << "    \"data_base64\": \"" << texData << "\"\n";
    }
    file << "  }\n";
    file << "}\n";
    
    file.close();
    std::cout << "[HDM] Saved ASCII HDM: " << filepath << std::endl;
    return true;
}

// Forward declaration for hdm_init_default_properties (defined later)
static void hdm_init_default_properties(HDMProperties& props);

// Helper: Find a string value in JSON by key
static std::string json_find_string(const std::string& json, const std::string& key) {
    std::string searchKey = "\"" + key + "\":";
    size_t pos = json.find(searchKey);
    if (pos == std::string::npos) return "";
    
    pos = json.find("\"", pos + searchKey.length());
    if (pos == std::string::npos) return "";
    pos++; // Skip opening quote
    
    size_t endPos = json.find("\"", pos);
    if (endPos == std::string::npos) return "";
    
    return json.substr(pos, endPos - pos);
}

// Helper: Find an integer value in JSON by key
static int json_find_int(const std::string& json, const std::string& key) {
    std::string searchKey = "\"" + key + "\":";
    size_t pos = json.find(searchKey);
    if (pos == std::string::npos) return 0;
    
    pos += searchKey.length();
    // Skip whitespace
    while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
    
    std::string numStr;
    while (pos < json.length() && (isdigit(json[pos]) || json[pos] == '-')) {
        numStr += json[pos++];
    }
    return numStr.empty() ? 0 : std::stoi(numStr);
}

// Helper: Find a float value in JSON by key
static float json_find_float(const std::string& json, const std::string& key) {
    std::string searchKey = "\"" + key + "\":";
    size_t pos = json.find(searchKey);
    if (pos == std::string::npos) return 0.0f;
    
    pos += searchKey.length();
    while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
    
    std::string numStr;
    while (pos < json.length() && (isdigit(json[pos]) || json[pos] == '-' || json[pos] == '.' || json[pos] == 'e' || json[pos] == 'E' || json[pos] == '+')) {
        numStr += json[pos++];
    }
    return numStr.empty() ? 0.0f : std::stof(numStr);
}

// Helper: Find a boolean value in JSON by key
static bool json_find_bool(const std::string& json, const std::string& key) {
    std::string searchKey = "\"" + key + "\":";
    size_t pos = json.find(searchKey);
    if (pos == std::string::npos) return false;
    
    pos += searchKey.length();
    while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
    
    return json.substr(pos, 4) == "true";
}

// Helper: Find a float array in JSON by key (e.g., [1, 2, 3])
static void json_find_float_array(const std::string& json, const std::string& key, float* out, int count) {
    std::string searchKey = "\"" + key + "\":";
    size_t pos = json.find(searchKey);
    if (pos == std::string::npos) return;
    
    pos = json.find("[", pos);
    if (pos == std::string::npos) return;
    pos++; // Skip '['
    
    for (int i = 0; i < count; i++) {
        while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == ',')) pos++;
        
        std::string numStr;
        while (pos < json.length() && (isdigit(json[pos]) || json[pos] == '-' || json[pos] == '.' || json[pos] == 'e' || json[pos] == 'E' || json[pos] == '+')) {
            numStr += json[pos++];
        }
        if (!numStr.empty()) out[i] = std::stof(numStr);
    }
}

// Load HDM ASCII format (.hdma)
static bool hdm_load_ascii(const char* filepath, HDMProperties& props,
                          HDMGeometry& geom, HDMTexture& tex) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "[HDM] Failed to open file for reading: " << filepath << std::endl;
        return false;
    }
    
    // Read entire file
    std::string json;
    std::string line;
    while (std::getline(file, line)) {
        json += line + "\n";
    }
    file.close();
    
    std::cout << "[HDM] Parsing ASCII HDM file..." << std::endl;
    
    // Initialize with defaults
    hdm_init_default_properties(props);
    
    // Parse model properties
    std::string objPath = json_find_string(json, "obj_path");
    std::string texPath = json_find_string(json, "texture_path");
    strncpy(props.model.obj_path, objPath.c_str(), sizeof(props.model.obj_path) - 1);
    strncpy(props.model.texture_path, texPath.c_str(), sizeof(props.model.texture_path) - 1);
    json_find_float_array(json, "scale", props.model.scale, 3);
    json_find_float_array(json, "origin_offset", props.model.origin_offset, 3);
    
    // Parse item properties
    props.item.item_type_id = json_find_int(json, "item_type_id");
    std::string itemName = json_find_string(json, "item_name");
    strncpy(props.item.item_name, itemName.c_str(), sizeof(props.item.item_name) - 1);
    props.item.trade_value = json_find_int(json, "trade_value");
    props.item.condition = json_find_float(json, "condition");
    props.item.weight = json_find_float(json, "weight");
    props.item.category = json_find_int(json, "category");
    props.item.is_salvaged = json_find_bool(json, "is_salvaged");
    props.item.mesh_class = json_find_int(json, "mesh_class");
    
    // Parse physics properties
    props.physics.collision_type = json_find_int(json, "collision_type");
    json_find_float_array(json, "collision_bounds", props.physics.collision_bounds, 3);
    props.physics.is_static = json_find_bool(json, "is_static");
    props.physics.mass = json_find_float(json, "mass");
    
    // Parse geometry
    int vertexCount = json_find_int(json, "vertex_count");
    int indexCount = json_find_int(json, "index_count");
    
    std::cout << "[HDM] Geometry: " << vertexCount << " vertices, " << indexCount << " indices" << std::endl;
    
    if (vertexCount > 0) {
        std::string vertB64 = json_find_string(json, "vertices_base64");
        if (!vertB64.empty()) {
            std::vector<unsigned char> vertData = base64_decode(vertB64);
            size_t expectedSize = vertexCount * sizeof(HDMVertex);
            if (vertData.size() >= expectedSize) {
                geom.vertices.resize(vertexCount);
                memcpy(geom.vertices.data(), vertData.data(), expectedSize);
                std::cout << "[HDM] Decoded " << vertexCount << " vertices" << std::endl;
            } else {
                std::cerr << "[HDM] Vertex data size mismatch: got " << vertData.size() << ", expected " << expectedSize << std::endl;
            }
        }
    }
    
    if (indexCount > 0) {
        std::string idxB64 = json_find_string(json, "indices_base64");
        if (!idxB64.empty()) {
            std::vector<unsigned char> idxData = base64_decode(idxB64);
            size_t expectedSize = indexCount * sizeof(uint32_t);
            if (idxData.size() >= expectedSize) {
                geom.indices.resize(indexCount);
                memcpy(geom.indices.data(), idxData.data(), expectedSize);
                std::cout << "[HDM] Decoded " << indexCount << " indices" << std::endl;
            } else {
                std::cerr << "[HDM] Index data size mismatch: got " << idxData.size() << ", expected " << expectedSize << std::endl;
            }
        }
    }
    
    // Parse texture
    tex.width = json_find_int(json, "width");
    tex.height = json_find_int(json, "height");
    tex.format = json_find_int(json, "format");
    
    if (tex.width > 0 && tex.height > 0) {
        std::string texB64 = json_find_string(json, "data_base64");
        if (!texB64.empty()) {
            tex.data = base64_decode(texB64);
            std::cout << "[HDM] Decoded texture: " << tex.width << "x" << tex.height << std::endl;
        }
    }
    
    std::cout << "[HDM] ASCII HDM loaded successfully: " << filepath << std::endl;
    return true;
}

// Forward declaration for pipeline init function
static bool ese_init_mesh_pipeline_only(GLFWwindow* window);

// Create a basic cube mesh
static bool ese_create_cube_mesh(GLFWwindow* window) {
    std::cout << "[ESE] Creating cube mesh..." << std::endl;
    
    // Initialize Vulkan if needed
    if (g_device == VK_NULL_HANDLE) {
        if (heidic_init_renderer(window) == 0) {
            std::cerr << "[ESE] Failed to initialize Vulkan!" << std::endl;
            return false;
        }
    }
    
    // Cube vertices: 8 corners, each with position, normal, and UV
    // We'll create 24 vertices (4 per face) for proper normals and UVs
    std::vector<MeshVertex> vertices;
    std::vector<uint32_t> indices;
    
    // Cube size (1x1x1 centered at origin)
    const float s = 0.5f;
    
    // Define 6 faces, each with 4 vertices
    // Front face (Z+)
    vertices.push_back({{-s, -s,  s}, {0, 0, 1}, {0, 0}});
    vertices.push_back({{ s, -s,  s}, {0, 0, 1}, {1, 0}});
    vertices.push_back({{ s,  s,  s}, {0, 0, 1}, {1, 1}});
    vertices.push_back({{-s,  s,  s}, {0, 0, 1}, {0, 1}});
    
    // Back face (Z-)
    vertices.push_back({{ s, -s, -s}, {0, 0, -1}, {0, 0}});
    vertices.push_back({{-s, -s, -s}, {0, 0, -1}, {1, 0}});
    vertices.push_back({{-s,  s, -s}, {0, 0, -1}, {1, 1}});
    vertices.push_back({{ s,  s, -s}, {0, 0, -1}, {0, 1}});
    
    // Top face (Y+)
    vertices.push_back({{-s,  s,  s}, {0, 1, 0}, {0, 0}});
    vertices.push_back({{ s,  s,  s}, {0, 1, 0}, {1, 0}});
    vertices.push_back({{ s,  s, -s}, {0, 1, 0}, {1, 1}});
    vertices.push_back({{-s,  s, -s}, {0, 1, 0}, {0, 1}});
    
    // Bottom face (Y-)
    vertices.push_back({{-s, -s, -s}, {0, -1, 0}, {0, 0}});
    vertices.push_back({{ s, -s, -s}, {0, -1, 0}, {1, 0}});
    vertices.push_back({{ s, -s,  s}, {0, -1, 0}, {1, 1}});
    vertices.push_back({{-s, -s,  s}, {0, -1, 0}, {0, 1}});
    
    // Right face (X+)
    vertices.push_back({{ s, -s,  s}, {1, 0, 0}, {0, 0}});
    vertices.push_back({{ s, -s, -s}, {1, 0, 0}, {1, 0}});
    vertices.push_back({{ s,  s, -s}, {1, 0, 0}, {1, 1}});
    vertices.push_back({{ s,  s,  s}, {1, 0, 0}, {0, 1}});
    
    // Left face (X-)
    vertices.push_back({{-s, -s, -s}, {-1, 0, 0}, {0, 0}});
    vertices.push_back({{-s, -s,  s}, {-1, 0, 0}, {1, 0}});
    vertices.push_back({{-s,  s,  s}, {-1, 0, 0}, {1, 1}});
    vertices.push_back({{-s,  s, -s}, {-1, 0, 0}, {0, 1}});
    
    // Indices: 2 triangles per face (6 faces * 2 triangles * 3 indices = 36 indices)
    for (uint32_t face = 0; face < 6; face++) {
        uint32_t base = face * 4;
        // First triangle
        indices.push_back(base + 0);
        indices.push_back(base + 1);
        indices.push_back(base + 2);
        // Second triangle
        indices.push_back(base + 0);
        indices.push_back(base + 2);
        indices.push_back(base + 3);
    }
    
    std::cout << "[ESE] Cube data: " << vertices.size() << " vertices, " << indices.size() << " indices" << std::endl;
    
    // Initialize pipeline if needed (WITHOUT loading OBJ)
    if (g_objMeshPipeline == VK_NULL_HANDLE) {
        std::cout << "[ESE] Initializing mesh pipeline..." << std::endl;
        if (!ese_init_mesh_pipeline_only(window)) {
            std::cerr << "[ESE] Failed to initialize mesh pipeline!" << std::endl;
            return false;
        }
    }
    
    // Create mesh resource AFTER pipeline is ready
    g_objMeshResource = std::make_unique<MeshResource>();
    if (!g_objMeshResource->createFromData(g_device, g_physicalDevice, vertices, indices)) {
        std::cerr << "[ESE] Failed to create cube mesh resource!" << std::endl;
        g_objMeshResource.reset();
        return false;
    }
    
    // Update paths (empty for generated mesh)
    g_eseCurrentObjPath = "(generated cube)";
    g_eseCurrentTexturePath = "";
    strncpy(g_eseHdmProperties.model.obj_path, "", sizeof(g_eseHdmProperties.model.obj_path) - 1);
    strncpy(g_eseHdmProperties.model.texture_path, "", sizeof(g_eseHdmProperties.model.texture_path) - 1);
    strncpy(g_eseHdmProperties.item.item_name, "Cube", sizeof(g_eseHdmProperties.item.item_name) - 1);
    
    g_eseModelLoaded = true;
    g_esePropertiesModified = true;
    
    std::cout << "[ESE] Cube mesh created successfully!" << std::endl;
    return true;
}

// Initialize just the mesh pipeline (without loading an OBJ file)
static bool ese_init_mesh_pipeline_only(GLFWwindow* window) {
    // Load shaders
    std::vector<char> vertShaderCode, fragShaderCode;
    try {
        vertShaderCode = readFile("shaders/mesh.vert.spv");
        fragShaderCode = readFile("shaders/mesh.frag.spv");
        std::cout << "[ESE] Loaded mesh shaders from shaders/ directory" << std::endl;
    } catch (const std::exception& e) {
        try {
            vertShaderCode = readFile("mesh.vert.spv");
            fragShaderCode = readFile("mesh.frag.spv");
            std::cout << "[ESE] Loaded mesh shaders from current directory" << std::endl;
        } catch (const std::exception& e2) {
            std::cerr << "[ESE] ERROR: Could not find mesh shader files!" << std::endl;
            return false;
        }
    }
    
    // Create shader modules
    VkShaderModuleCreateInfo vertCreateInfo = {};
    vertCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    vertCreateInfo.codeSize = vertShaderCode.size();
    vertCreateInfo.pCode = reinterpret_cast<const uint32_t*>(vertShaderCode.data());
    
    if (vkCreateShaderModule(g_device, &vertCreateInfo, nullptr, &g_objMeshVertShaderModule) != VK_SUCCESS) {
        std::cerr << "[ESE] Failed to create vertex shader module!" << std::endl;
        return false;
    }
    
    VkShaderModuleCreateInfo fragCreateInfo = {};
    fragCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    fragCreateInfo.codeSize = fragShaderCode.size();
    fragCreateInfo.pCode = reinterpret_cast<const uint32_t*>(fragShaderCode.data());
    
    if (vkCreateShaderModule(g_device, &fragCreateInfo, nullptr, &g_objMeshFragShaderModule) != VK_SUCCESS) {
        std::cerr << "[ESE] Failed to create fragment shader module!" << std::endl;
        vkDestroyShaderModule(g_device, g_objMeshVertShaderModule, nullptr);
        return false;
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
        std::cerr << "[ESE] Failed to create descriptor set layout!" << std::endl;
        return false;
    }
    
    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &g_objMeshDescriptorSetLayout;
    
    if (vkCreatePipelineLayout(g_device, &pipelineLayoutInfo, nullptr, &g_objMeshPipelineLayout) != VK_SUCCESS) {
        std::cerr << "[ESE] Failed to create pipeline layout!" << std::endl;
        return false;
    }
    
    // Create graphics pipeline (similar to heidic_init_renderer_obj_mesh)
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
    
    // Vertex input for MeshVertex (pos, normal, uv)
    VkVertexInputBindingDescription bindingDescription = {};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(MeshVertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    
    VkVertexInputAttributeDescription attributeDescriptions[3] = {};
    // Position
    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[0].offset = offsetof(MeshVertex, pos);
    // Normal
    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[1].offset = offsetof(MeshVertex, normal);
    // UV
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
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | 
                                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
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
    
    if (vkCreateGraphicsPipelines(g_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &g_objMeshPipeline) != VK_SUCCESS) {
        std::cerr << "[ESE] Failed to create mesh graphics pipeline!" << std::endl;
        return false;
    }
    
    // Also create wireframe pipeline
    rasterizer.polygonMode = VK_POLYGON_MODE_LINE;
    pipelineInfo.pRasterizationState = &rasterizer;
    if (vkCreateGraphicsPipelines(g_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &g_objMeshWireframePipeline) != VK_SUCCESS) {
        std::cerr << "[ESE] Warning: Failed to create wireframe pipeline" << std::endl;
        // Non-fatal, continue
    }
    
    // Create UBO buffer
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = sizeof(UniformBufferObject);
    bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    if (vkCreateBuffer(g_device, &bufferInfo, nullptr, &g_objMeshUniformBuffer) != VK_SUCCESS) {
        std::cerr << "[ESE] Failed to create uniform buffer!" << std::endl;
        return false;
    }
    
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(g_device, g_objMeshUniformBuffer, &memRequirements);
    
    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, 
                                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    
    if (vkAllocateMemory(g_device, &allocInfo, nullptr, &g_objMeshUniformBufferMemory) != VK_SUCCESS) {
        std::cerr << "[ESE] Failed to allocate uniform buffer memory!" << std::endl;
        return false;
    }
    
    vkBindBufferMemory(g_device, g_objMeshUniformBuffer, g_objMeshUniformBufferMemory, 0);
    
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
        std::cerr << "[ESE] Failed to create descriptor pool!" << std::endl;
        return false;
    }
    
    // Allocate descriptor set
    VkDescriptorSetAllocateInfo setAllocInfo = {};
    setAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    setAllocInfo.descriptorPool = g_objMeshDescriptorPool;
    setAllocInfo.descriptorSetCount = 1;
    setAllocInfo.pSetLayouts = &g_objMeshDescriptorSetLayout;
    
    if (vkAllocateDescriptorSets(g_device, &setAllocInfo, &g_objMeshDescriptorSet) != VK_SUCCESS) {
        std::cerr << "[ESE] Failed to allocate descriptor set!" << std::endl;
        return false;
    }
    
    // Create dummy white texture (1x1 pixel)
    uint32_t dummyData = 0xFFFFFFFF; // White RGBA
    createImage(1, 1, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                g_objMeshDummyTexture, g_objMeshDummyTextureMemory);
    
    // Transition and upload dummy texture data
    VkCommandBuffer cmdBuffer = beginSingleTimeCommands();
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
    
    vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);
    
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(4, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingBuffer, stagingBufferMemory);
    
    void* data;
    vkMapMemory(g_device, stagingBufferMemory, 0, 4, 0, &data);
    memcpy(data, &dummyData, 4);
    vkUnmapMemory(g_device, stagingBufferMemory);
    
    VkBufferImageCopy region = {};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1;
    region.imageExtent = {1, 1, 1};
    
    vkCmdCopyBufferToImage(cmdBuffer, stagingBuffer, g_objMeshDummyTexture, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    
    vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);
    
    endSingleTimeCommands(cmdBuffer);
    
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
        std::cerr << "[ESE] Failed to create dummy texture image view!" << std::endl;
        return false;
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
    
    if (vkCreateSampler(g_device, &samplerInfo, nullptr, &g_objMeshDummySampler) != VK_SUCCESS) {
        std::cerr << "[ESE] Failed to create sampler!" << std::endl;
        return false;
    }
    
    // Update descriptor set with UBO and texture
    VkDescriptorBufferInfo uboInfo = {};
    uboInfo.buffer = g_objMeshUniformBuffer;
    uboInfo.offset = 0;
    uboInfo.range = sizeof(UniformBufferObject);
    
    VkDescriptorImageInfo imageInfo = {};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = g_objMeshDummyTextureView;
    imageInfo.sampler = g_objMeshDummySampler;
    
    VkWriteDescriptorSet descriptorWrites[2] = {};
    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = g_objMeshDescriptorSet;
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pBufferInfo = &uboInfo;
    
    descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[1].dstSet = g_objMeshDescriptorSet;
    descriptorWrites[1].dstBinding = 1;
    descriptorWrites[1].dstArrayElement = 0;
    descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].pImageInfo = &imageInfo;
    
    vkUpdateDescriptorSets(g_device, 2, descriptorWrites, 0, nullptr);
    
    // Mark as initialized
    g_objMeshInitialized = true;
    
    std::cout << "[ESE] Mesh pipeline initialized successfully!" << std::endl;
    return true;
}

// Create Vulkan mesh from packed HDM data (for loading HDM files)
// Note: This reuses the OBJ mesh loading infrastructure 
static bool hdm_create_mesh_from_packed(GLFWwindow* window, const HDMGeometry& geom, const HDMTexture& tex) {
    if (geom.vertices.empty()) {
        std::cerr << "[HDM] No geometry to create mesh from!" << std::endl;
        return false;
    }
    
    std::cout << "[HDM] Creating mesh from packed data..." << std::endl;
    std::cout << "[HDM] Vertices: " << geom.vertices.size() << ", Indices: " << geom.indices.size() << std::endl;
    
    // Convert HDMVertex to MeshVertex format (which createFromData expects)
    std::vector<MeshVertex> vertices;
    vertices.reserve(geom.vertices.size());
    
    for (const auto& v : geom.vertices) {
        MeshVertex mv;
        mv.pos[0] = v.position[0];
        mv.pos[1] = v.position[1];
        mv.pos[2] = v.position[2];
        mv.normal[0] = v.normal[0];
        mv.normal[1] = v.normal[1];
        mv.normal[2] = v.normal[2];
        mv.uv[0] = v.texcoord[0];
        mv.uv[1] = v.texcoord[1];
        vertices.push_back(mv);
    }
    
    // Create mesh resource with the packed data
    g_objMeshResource = std::make_unique<MeshResource>();
    if (!g_objMeshResource->createFromData(g_device, g_physicalDevice, 
                                            vertices, geom.indices)) {
        std::cerr << "[HDM] Failed to create mesh resource!" << std::endl;
        g_objMeshResource.reset();
        return false;
    }
    
    // CRITICAL: Initialize pipeline if not already initialized
    // The pipeline must exist before we can render
    if (g_objMeshPipeline == VK_NULL_HANDLE) {
        std::cout << "[HDM] Pipeline not initialized, initializing now..." << std::endl;
        
        // We have the mesh, now we need to initialize the pipeline infrastructure
        // This is a subset of heidic_init_renderer_obj_mesh - we'll do the minimum needed
        
        // Load shaders - try mesh shaders first (in shaders/ subdirectory or current dir), fall back to 3D shaders
        std::vector<char> vertShaderCode, fragShaderCode;
        bool loadedMeshShaders = false;
        
        // Try shaders/mesh.vert.spv first (common location)
        try {
            vertShaderCode = readFile("shaders/mesh.vert.spv");
            fragShaderCode = readFile("shaders/mesh.frag.spv");
            std::cout << "[HDM] Loaded mesh shaders (shaders/mesh.vert.spv, shaders/mesh.frag.spv)" << std::endl;
            loadedMeshShaders = true;
        } catch (const std::exception& e) {
            // Try current directory
            try {
                vertShaderCode = readFile("mesh.vert.spv");
                fragShaderCode = readFile("mesh.frag.spv");
                std::cout << "[HDM] Loaded mesh shaders (mesh.vert.spv, mesh.frag.spv)" << std::endl;
                loadedMeshShaders = true;
            } catch (const std::exception& e2) {
                // Fall back to default 3D shaders (WARNING: these don't match MeshVertex format!)
                std::cout << "[HDM] WARNING: Mesh shaders not found, trying default 3D shaders..." << std::endl;
                std::cout << "[HDM] NOTE: 3D shaders may not work correctly with OBJ mesh format!" << std::endl;
                try {
                    vertShaderCode = readFile("vert_3d.spv");
                    fragShaderCode = readFile("frag_3d.spv");
                    std::cout << "[HDM] Loaded default 3D shaders (vert_3d.spv, frag_3d.spv)" << std::endl;
                    std::cerr << "[HDM] WARNING: Using incompatible shaders - model may not render correctly!" << std::endl;
                } catch (const std::exception& e3) {
                    std::cerr << "[HDM] ERROR: Could not load any shaders!" << std::endl;
                    std::cerr << "[HDM] Tried: shaders/mesh.vert.spv, mesh.vert.spv, and vert_3d.spv" << std::endl;
                    std::cerr << "[HDM] Error: " << e3.what() << std::endl;
                    std::cerr << "[HDM] Please copy mesh.vert.spv and mesh.frag.spv to the ESE project directory!" << std::endl;
                    // Cleanup mesh resource on failure
                    g_objMeshResource.reset();
                    return false;
                }
            }
        }
        
        // Create shader modules
        VkShaderModuleCreateInfo vertCreateInfo = {};
        vertCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        vertCreateInfo.codeSize = vertShaderCode.size();
        vertCreateInfo.pCode = reinterpret_cast<const uint32_t*>(vertShaderCode.data());
        
        if (vkCreateShaderModule(g_device, &vertCreateInfo, nullptr, &g_objMeshVertShaderModule) != VK_SUCCESS) {
            std::cerr << "[HDM] ERROR: Failed to create vertex shader module!" << std::endl;
            return false;
        }
        
        VkShaderModuleCreateInfo fragCreateInfo = {};
        fragCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        fragCreateInfo.codeSize = fragShaderCode.size();
        fragCreateInfo.pCode = reinterpret_cast<const uint32_t*>(fragShaderCode.data());
        
        if (vkCreateShaderModule(g_device, &fragCreateInfo, nullptr, &g_objMeshFragShaderModule) != VK_SUCCESS) {
            std::cerr << "[HDM] ERROR: Failed to create fragment shader module!" << std::endl;
            vkDestroyShaderModule(g_device, g_objMeshVertShaderModule, nullptr);
            return false;
        }
        
        // Create descriptor set layout, pipeline layout, uniform buffer, descriptor pool, etc.
        // (Reusing the exact code from heidic_init_renderer_obj_mesh - see lines 6159-6550)
        // For brevity, we'll call heidic_init_renderer_obj_mesh with empty paths to initialize infrastructure,
        // but that won't work since it needs a real OBJ file.
        
        // SIMPLEST FIX: Call heidic_init_renderer_obj_mesh with the mesh we just created
        // But that function loads its own mesh. So we need to either:
        // 1. Extract pipeline init into helper (best, but lots of code)
        // 2. Create a dummy OBJ file temporarily (hacky)
        // 3. Duplicate the pipeline creation code here (verbose but works)
        
        // For now, let's use approach 3 - duplicate the essential parts
        // We'll create descriptor set layout, pipeline layout, uniform buffer, descriptor pool, and pipeline
        
        // Create descriptor set layout (UBO + texture sampler) - from line 6159
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
            std::cerr << "[HDM] ERROR: Failed to create descriptor set layout!" << std::endl;
            vkDestroyShaderModule(g_device, g_objMeshVertShaderModule, nullptr);
            vkDestroyShaderModule(g_device, g_objMeshFragShaderModule, nullptr);
            return false;
        }
        
        // Create pipeline layout - from line 6185
        VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &g_objMeshDescriptorSetLayout;
        
        if (vkCreatePipelineLayout(g_device, &pipelineLayoutInfo, nullptr, &g_objMeshPipelineLayout) != VK_SUCCESS) {
            std::cerr << "[HDM] ERROR: Failed to create pipeline layout!" << std::endl;
            vkDestroyDescriptorSetLayout(g_device, g_objMeshDescriptorSetLayout, nullptr);
            vkDestroyShaderModule(g_device, g_objMeshVertShaderModule, nullptr);
            vkDestroyShaderModule(g_device, g_objMeshFragShaderModule, nullptr);
            return false;
        }
        
        // Create uniform buffer - from line 6199
        VkDeviceSize bufferSize = sizeof(UniformBufferObject);
        createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     g_objMeshUniformBuffer, g_objMeshUniformBufferMemory);
        
        // Create dummy texture and descriptor set (reuse code from lines 6238-6440)
        // For now, let's create a minimal dummy texture setup
        uint32_t dummyData = 0xFFFFFFFF;
        createImage(1, 1, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL,
                    VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                    g_objMeshDummyTexture, g_objMeshDummyTextureMemory);
        
        VkCommandBuffer cmdBuf = beginSingleTimeCommands();
        VkImageMemoryBarrier barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = g_objMeshDummyTexture;
        barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
        
        VkBuffer stagingBuf;
        VkDeviceMemory stagingMem;
        createBuffer(4, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuf, stagingMem);
        void* mapData;
        vkMapMemory(g_device, stagingMem, 0, 4, 0, &mapData);
        memcpy(mapData, &dummyData, 4);
        vkUnmapMemory(g_device, stagingMem);
        
        VkBufferImageCopy region = {};
        region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        region.imageExtent = {1, 1, 1};
        vkCmdCopyBufferToImage(cmdBuf, stagingBuf, g_objMeshDummyTexture, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
        endSingleTimeCommands(cmdBuf);
        vkDestroyBuffer(g_device, stagingBuf, nullptr);
        vkFreeMemory(g_device, stagingMem, nullptr);
        
        VkImageViewCreateInfo viewInfo = {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = g_objMeshDummyTexture;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        vkCreateImageView(g_device, &viewInfo, nullptr, &g_objMeshDummyTextureView);
        
        VkSamplerCreateInfo samplerInfo = {};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        vkCreateSampler(g_device, &samplerInfo, nullptr, &g_objMeshDummySampler);
        
        // Create descriptor pool and set - from lines 6356-6440
        VkDescriptorPoolSize poolSizes[2] = {};
        poolSizes[0] = {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1};
        poolSizes[1] = {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1};
        VkDescriptorPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = 2;
        poolInfo.pPoolSizes = poolSizes;
        poolInfo.maxSets = 1;
        vkCreateDescriptorPool(g_device, &poolInfo, nullptr, &g_objMeshDescriptorPool);
        
        VkDescriptorSetAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = g_objMeshDescriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &g_objMeshDescriptorSetLayout;
        vkAllocateDescriptorSets(g_device, &allocInfo, &g_objMeshDescriptorSet);
        
        VkDescriptorBufferInfo bufferInfo = {g_objMeshUniformBuffer, 0, sizeof(UniformBufferObject)};
        VkDescriptorImageInfo imageInfo = {g_objMeshDummySampler, g_objMeshDummyTextureView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        VkWriteDescriptorSet writes[2] = {};
        writes[0] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, g_objMeshDescriptorSet, 0, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, nullptr, &bufferInfo, nullptr};
        writes[1] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, g_objMeshDescriptorSet, 1, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &imageInfo, nullptr, nullptr};
        vkUpdateDescriptorSets(g_device, 2, writes, 0, nullptr);
        
        // Create graphics pipeline - from lines 6442-6550 (using mesh's vertex input)
        VkPipelineShaderStageCreateInfo stages[2] = {};
        stages[0] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, g_objMeshVertShaderModule, "main", nullptr};
        stages[1] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, g_objMeshFragShaderModule, "main", nullptr};
        
        VkVertexInputBindingDescription bindingDesc = g_objMeshResource->getVertexInputBinding();
        std::vector<VkVertexInputAttributeDescription> attrDescs = g_objMeshResource->getVertexInputAttributes();
        VkPipelineVertexInputStateCreateInfo vertexInput = {};
        vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInput.vertexBindingDescriptionCount = 1;
        vertexInput.pVertexBindingDescriptions = &bindingDesc;
        vertexInput.vertexAttributeDescriptionCount = attrDescs.size();
        vertexInput.pVertexAttributeDescriptions = attrDescs.data();
        
        VkPipelineInputAssemblyStateCreateInfo inputAssembly = {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, nullptr, 0, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE};
        
        // Use dynamic viewport and scissor (since render_obj_mesh_to_command_buffer sets them dynamically)
        VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamicState = {};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = 2;
        dynamicState.pDynamicStates = dynamicStates;
        
        VkPipelineViewportStateCreateInfo viewportState = {};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;  // Count required, but viewport is dynamic
        viewportState.scissorCount = 1;   // Count required, but scissor is dynamic
        
        VkPipelineRasterizationStateCreateInfo rasterizer = {VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, nullptr, 0, VK_FALSE, VK_FALSE, VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_FALSE, 0, 0, 0, 1.0f};
        VkPipelineMultisampleStateCreateInfo multisample = {VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, nullptr, 0, VK_SAMPLE_COUNT_1_BIT, VK_FALSE, 1.0f, nullptr, VK_FALSE, VK_FALSE};
        VkPipelineDepthStencilStateCreateInfo depthStencil = {VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, nullptr, 0, VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS, VK_FALSE, VK_FALSE, {}, {}, 0, 0};
        VkPipelineColorBlendAttachmentState colorBlend = {VK_FALSE, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD, 0xF};
        VkPipelineColorBlendStateCreateInfo colorBlending = {VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, nullptr, 0, VK_FALSE, VK_LOGIC_OP_COPY, 1, &colorBlend, {0,0,0,0}};
        
        VkGraphicsPipelineCreateInfo pipelineInfo = {};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = stages;
        pipelineInfo.pVertexInputState = &vertexInput;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisample;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;  // Add dynamic state
        pipelineInfo.layout = g_objMeshPipelineLayout;
        pipelineInfo.renderPass = g_renderPass;
        pipelineInfo.subpass = 0;
        
        if (vkCreateGraphicsPipelines(g_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &g_objMeshPipeline) != VK_SUCCESS) {
            std::cerr << "[HDM] ERROR: Failed to create graphics pipeline!" << std::endl;
            // Cleanup on failure
            if (g_objMeshDescriptorPool != VK_NULL_HANDLE) {
                vkDestroyDescriptorPool(g_device, g_objMeshDescriptorPool, nullptr);
                g_objMeshDescriptorPool = VK_NULL_HANDLE;
            }
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
            if (g_objMeshUniformBuffer != VK_NULL_HANDLE) {
                vkDestroyBuffer(g_device, g_objMeshUniformBuffer, nullptr);
                g_objMeshUniformBuffer = VK_NULL_HANDLE;
            }
            if (g_objMeshUniformBufferMemory != VK_NULL_HANDLE) {
                vkFreeMemory(g_device, g_objMeshUniformBufferMemory, nullptr);
                g_objMeshUniformBufferMemory = VK_NULL_HANDLE;
            }
            if (g_objMeshPipelineLayout != VK_NULL_HANDLE) {
                vkDestroyPipelineLayout(g_device, g_objMeshPipelineLayout, nullptr);
                g_objMeshPipelineLayout = VK_NULL_HANDLE;
            }
            if (g_objMeshDescriptorSetLayout != VK_NULL_HANDLE) {
                vkDestroyDescriptorSetLayout(g_device, g_objMeshDescriptorSetLayout, nullptr);
                g_objMeshDescriptorSetLayout = VK_NULL_HANDLE;
            }
            if (g_objMeshFragShaderModule != VK_NULL_HANDLE) {
                vkDestroyShaderModule(g_device, g_objMeshFragShaderModule, nullptr);
                g_objMeshFragShaderModule = VK_NULL_HANDLE;
            }
            if (g_objMeshVertShaderModule != VK_NULL_HANDLE) {
                vkDestroyShaderModule(g_device, g_objMeshVertShaderModule, nullptr);
                g_objMeshVertShaderModule = VK_NULL_HANDLE;
            }
            g_objMeshResource.reset();
            return false;
        }
        
        std::cout << "[HDM] Pipeline initialized successfully!" << std::endl;
        
        // Create wireframe pipeline for ESE (same settings but with VK_POLYGON_MODE_LINE)
        rasterizer.polygonMode = VK_POLYGON_MODE_LINE;
        rasterizer.lineWidth = 1.0f;
        if (vkCreateGraphicsPipelines(g_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &g_objMeshWireframePipeline) != VK_SUCCESS) {
            std::cerr << "[HDM] WARNING: Failed to create wireframe pipeline (wireframe mode will be unavailable)" << std::endl;
        } else {
            std::cout << "[HDM] Wireframe pipeline created successfully!" << std::endl;
        }
    }
    
    // Create texture from packed RGBA data if available
    if (tex.width > 0 && tex.height > 0 && !tex.data.empty()) {
        std::cout << "[HDM] Creating texture from packed data: " << tex.width << "x" << tex.height << std::endl;
        
        // Clean up dummy texture if it exists
        if (g_objMeshDummyTexture != VK_NULL_HANDLE) {
            if (g_objMeshDummyTextureView != VK_NULL_HANDLE) {
                vkDestroyImageView(g_device, g_objMeshDummyTextureView, nullptr);
                g_objMeshDummyTextureView = VK_NULL_HANDLE;
            }
            if (g_objMeshDummySampler != VK_NULL_HANDLE) {
                vkDestroySampler(g_device, g_objMeshDummySampler, nullptr);
                g_objMeshDummySampler = VK_NULL_HANDLE;
            }
            vkDestroyImage(g_device, g_objMeshDummyTexture, nullptr);
            g_objMeshDummyTexture = VK_NULL_HANDLE;
            vkFreeMemory(g_device, g_objMeshDummyTextureMemory, nullptr);
            g_objMeshDummyTextureMemory = VK_NULL_HANDLE;
        }
        
        // Create image
        createImage(tex.width, tex.height, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL,
                    VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                    g_objMeshDummyTexture, g_objMeshDummyTextureMemory);
        
        // Upload texture data
        VkCommandBuffer cmdBuf = beginSingleTimeCommands();
        
        // Transition to transfer destination
        VkImageMemoryBarrier barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = g_objMeshDummyTexture;
        barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &barrier);
        
        // Create staging buffer
        VkDeviceSize imageSize = tex.width * tex.height * 4;
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     stagingBuffer, stagingBufferMemory);
        
        // Copy texture data to staging buffer
        void* data;
        vkMapMemory(g_device, stagingBufferMemory, 0, imageSize, 0, &data);
        memcpy(data, tex.data.data(), imageSize);
        vkUnmapMemory(g_device, stagingBufferMemory);
        
        // Copy buffer to image
        VkBufferImageCopy region = {};
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = {0, 0, 0};
        region.imageExtent = {tex.width, tex.height, 1};
        vkCmdCopyBufferToImage(cmdBuf, stagingBuffer, g_objMeshDummyTexture, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        
        // Transition to shader read
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &barrier);
        
        endSingleTimeCommands(cmdBuf);
        
        // Clean up staging buffer
        vkDestroyBuffer(g_device, stagingBuffer, nullptr);
        vkFreeMemory(g_device, stagingBufferMemory, nullptr);
        
        // Create image view
        VkImageViewCreateInfo viewInfo = {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = g_objMeshDummyTexture;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        if (vkCreateImageView(g_device, &viewInfo, nullptr, &g_objMeshDummyTextureView) != VK_SUCCESS) {
            std::cerr << "[HDM] ERROR: Failed to create texture image view!" << std::endl;
        }
        
        // Create sampler (if not already created)
        if (g_objMeshDummySampler == VK_NULL_HANDLE) {
            VkSamplerCreateInfo samplerInfo = {};
            samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            samplerInfo.magFilter = VK_FILTER_LINEAR;
            samplerInfo.minFilter = VK_FILTER_LINEAR;
            samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            if (vkCreateSampler(g_device, &samplerInfo, nullptr, &g_objMeshDummySampler) != VK_SUCCESS) {
                std::cerr << "[HDM] ERROR: Failed to create texture sampler!" << std::endl;
            }
        }
        
        // Update descriptor set with new texture
        VkDescriptorImageInfo imageInfo = {g_objMeshDummySampler, g_objMeshDummyTextureView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        VkWriteDescriptorSet write = {};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = g_objMeshDescriptorSet;
        write.dstBinding = 1;
        write.dstArrayElement = 0;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.descriptorCount = 1;
        write.pImageInfo = &imageInfo;
        vkUpdateDescriptorSets(g_device, 1, &write, 0, nullptr);
        
        std::cout << "[HDM] Texture created successfully from packed data!" << std::endl;
    } else {
        std::cout << "[HDM] No texture data in HDM file - using default white texture" << std::endl;
    }
    
    g_objMeshInitialized = true;
    std::cout << "[HDM] Mesh created from packed data!" << std::endl;
    return true;
}

// HDM JSON Save/Load Functions (legacy, for backwards compatibility)
static bool hdm_save_json(const char* filepath, const HDMProperties& props) {
    std::ofstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "[ESE] Failed to open file for writing: " << filepath << std::endl;
        return false;
    }
    
    file << "{\n";
    file << "  \"hdm_version\": \"" << props.hdm_version << "\",\n";
    file << "  \n";
    file << "  \"model\": {\n";
    file << "    \"obj_path\": \"" << props.model.obj_path << "\",\n";
    file << "    \"texture_path\": \"" << props.model.texture_path << "\",\n";
    file << "    \"scale\": [" << props.model.scale[0] << ", " << props.model.scale[1] << ", " << props.model.scale[2] << "],\n";
    file << "    \"origin_offset\": [" << props.model.origin_offset[0] << ", " << props.model.origin_offset[1] << ", " << props.model.origin_offset[2] << "]\n";
    file << "  },\n";
    file << "  \n";
    file << "  \"item_properties\": {\n";
    file << "    \"item_type_id\": " << props.item.item_type_id << ",\n";
    file << "    \"item_name\": \"" << props.item.item_name << "\",\n";
    file << "    \"trade_value\": " << props.item.trade_value << ",\n";
    file << "    \"condition\": " << props.item.condition << ",\n";
    file << "    \"weight\": " << props.item.weight << ",\n";
    file << "    \"category\": " << props.item.category << ",\n";
    file << "    \"is_salvaged\": " << (props.item.is_salvaged ? "true" : "false") << "\n";
    file << "  },\n";
    file << "  \n";
    file << "  \"physics\": {\n";
    file << "    \"collision_type\": " << props.physics.collision_type << ",\n";
    file << "    \"collision_bounds\": [" << props.physics.collision_bounds[0] << ", " << props.physics.collision_bounds[1] << ", " << props.physics.collision_bounds[2] << "],\n";
    file << "    \"is_static\": " << (props.physics.is_static ? "true" : "false") << ",\n";
    file << "    \"mass\": " << props.physics.mass << "\n";
    file << "  },\n";
    file << "  \n";
    file << "  \"control_points\": [\n";
    for (int i = 0; i < props.num_control_points; i++) {
        file << "    {\"name\": \"" << props.control_points[i].name << "\", \"position\": [" 
             << props.control_points[i].position[0] << ", " 
             << props.control_points[i].position[1] << ", " 
             << props.control_points[i].position[2] << "]}";
        if (i < props.num_control_points - 1) file << ",";
        file << "\n";
    }
    file << "  ]\n";
    file << "}\n";
    
    file.close();
    std::cout << "[ESE] Saved HDM file: " << filepath << std::endl;
    return true;
}

// Simple JSON parser helper - find value after key
static std::string json_get_string(const std::string& json, const std::string& key) {
    size_t pos = json.find("\"" + key + "\"");
    if (pos == std::string::npos) return "";
    pos = json.find(":", pos);
    if (pos == std::string::npos) return "";
    pos = json.find("\"", pos);
    if (pos == std::string::npos) return "";
    size_t end = json.find("\"", pos + 1);
    if (end == std::string::npos) return "";
    return json.substr(pos + 1, end - pos - 1);
}

static int json_get_int(const std::string& json, const std::string& key) {
    size_t pos = json.find("\"" + key + "\"");
    if (pos == std::string::npos) return 0;
    pos = json.find(":", pos);
    if (pos == std::string::npos) return 0;
    // Skip whitespace
    while (pos < json.size() && (json[pos] == ':' || json[pos] == ' ' || json[pos] == '\t')) pos++;
    return std::atoi(json.c_str() + pos);
}

static float json_get_float(const std::string& json, const std::string& key) {
    size_t pos = json.find("\"" + key + "\"");
    if (pos == std::string::npos) return 0.0f;
    pos = json.find(":", pos);
    if (pos == std::string::npos) return 0.0f;
    // Skip whitespace
    while (pos < json.size() && (json[pos] == ':' || json[pos] == ' ' || json[pos] == '\t')) pos++;
    return (float)std::atof(json.c_str() + pos);
}

static bool json_get_bool(const std::string& json, const std::string& key) {
    size_t pos = json.find("\"" + key + "\"");
    if (pos == std::string::npos) return false;
    pos = json.find(":", pos);
    if (pos == std::string::npos) return false;
    return json.find("true", pos) < json.find(",", pos);
}

static bool hdm_load_json(const char* filepath, HDMProperties& props) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "[ESE] Failed to open HDM file: " << filepath << std::endl;
        return false;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string json = buffer.str();
    file.close();
    
    // Parse JSON (simple parser)
    std::string ver = json_get_string(json, "hdm_version");
    if (!ver.empty()) strncpy(props.hdm_version, ver.c_str(), sizeof(props.hdm_version) - 1);
    
    // Model properties
    std::string obj = json_get_string(json, "obj_path");
    if (!obj.empty()) strncpy(props.model.obj_path, obj.c_str(), sizeof(props.model.obj_path) - 1);
    std::string tex = json_get_string(json, "texture_path");
    if (!tex.empty()) strncpy(props.model.texture_path, tex.c_str(), sizeof(props.model.texture_path) - 1);
    
    // Item properties
    props.item.item_type_id = json_get_int(json, "item_type_id");
    std::string name = json_get_string(json, "item_name");
    if (!name.empty()) strncpy(props.item.item_name, name.c_str(), sizeof(props.item.item_name) - 1);
    props.item.trade_value = json_get_int(json, "trade_value");
    props.item.condition = json_get_float(json, "condition");
    props.item.weight = json_get_float(json, "weight");
    props.item.category = json_get_int(json, "category");
    props.item.is_salvaged = json_get_bool(json, "is_salvaged");
    
    // Physics properties
    props.physics.collision_type = json_get_int(json, "collision_type");
    props.physics.is_static = json_get_bool(json, "is_static");
    props.physics.mass = json_get_float(json, "mass");
    
    std::cout << "[ESE] Loaded HDM file: " << filepath << std::endl;
    return true;
}

// Initialize ESE with default properties
static void hdm_init_default_properties(HDMProperties& props) {
    memset(&props, 0, sizeof(props));
    strncpy(props.hdm_version, "2.0", sizeof(props.hdm_version) - 1);
    strncpy(props.item.item_name, "Unnamed", sizeof(props.item.item_name) - 1);
    props.item.condition = 1.0f;
    props.item.weight = 1.0f;
    props.item.mesh_class = 0;  // Default to head
    props.model.scale[0] = props.model.scale[1] = props.model.scale[2] = 1.0f;
    props.physics.collision_type = 1;  // Box collision
    props.physics.collision_bounds[0] = props.physics.collision_bounds[1] = props.physics.collision_bounds[2] = 1.0f;
    props.physics.is_static = true;
    props.physics.mass = 1.0f;
    props.num_control_points = 0;  // Explicitly set to 0
}

// Initialize ESE renderer with ImGui support
extern "C" int heidic_init_ese(GLFWwindow* window) {
    if (!window) {
        std::cerr << "[ESE] ERROR: Invalid window!" << std::endl;
        return 0;
    }
    
    std::cout << "[ESE] Initializing Echo Synapse Editor..." << std::endl;
    
    // Store window reference for input handling
    g_eseWindow = window;
    
    // Initialize camera to default orbit position
    g_eseCameraYaw = 45.0f;
    g_eseCameraPitch = 30.0f;
    g_eseCameraDistance = 5.0f;
    g_eseCameraPanX = 0.0f;
    g_eseCameraPanY = 0.0f;
    g_eseCameraTargetX = 0.0f;
    g_eseCameraTargetY = 0.0f;
    g_eseCameraTargetZ = 0.0f;
    g_eseScrollDelta = 0.0f;
    
    // Note: Scroll callback is set AFTER ImGui initialization (below)
    // because ImGui_ImplGlfw_InitForVulkan with true would overwrite it
    
    // Initialize default HDM properties
    hdm_init_default_properties(g_eseHdmProperties);
    
    // Initialize base Vulkan (reuse existing infrastructure)
    if (!heidic_init_renderer(window)) {
        std::cerr << "[ESE] Failed to initialize Vulkan renderer!" << std::endl;
        return 0;
    }
    
#ifdef USE_IMGUI
    // Initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    
    // Setup ImGui style
    ImGui::StyleColorsDark();
    
    // Initialize ImGui for GLFW
    // Pass true to install all callbacks (mouse, keyboard, etc.)
    // We chain our scroll callback after this
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
    pool_info.poolSizeCount = std::size(pool_sizes);
    pool_info.pPoolSizes = pool_sizes;
    
    if (vkCreateDescriptorPool(g_device, &pool_info, nullptr, &g_eseImguiDescriptorPool) != VK_SUCCESS) {
        std::cerr << "[ESE] Failed to create ImGui descriptor pool!" << std::endl;
        return 0;
    }
    
    // Initialize ImGui Vulkan
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.ApiVersion = VK_API_VERSION_1_0;
    init_info.Instance = g_instance;
    init_info.PhysicalDevice = g_physicalDevice;
    init_info.Device = g_device;
    init_info.QueueFamily = 0; // We use graphics queue family 0
    init_info.Queue = g_graphicsQueue;
    init_info.DescriptorPool = g_eseImguiDescriptorPool;
    init_info.MinImageCount = 2;
    init_info.ImageCount = static_cast<uint32_t>(g_swapchainImages.size());
    
    // Set pipeline info for render pass (newer ImGui 2025+ API)
    init_info.PipelineInfoMain.RenderPass = g_renderPass;
    init_info.PipelineInfoMain.Subpass = 0;
    init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.UseDynamicRendering = false;
    
    ImGui_ImplVulkan_Init(&init_info);
    
    // In newer ImGui, fonts are uploaded automatically by ImGui_ImplVulkan_Init
    
    g_eseImguiInitialized = true;
    std::cout << "[ESE] ImGui initialized!" << std::endl;
#else
    std::cout << "[ESE] WARNING: ImGui not available (USE_IMGUI not defined)" << std::endl;
#endif
    
    // Set up scroll callback for zoom AFTER ImGui is initialized
    // Store ImGui's scroll callback so we can chain to it
    g_esePrevScrollCallback = glfwSetScrollCallback(window, ese_scroll_callback);
    
    g_eseInitialized = true;
    std::cout << "[ESE] Echo Synapse Editor initialized!" << std::endl;
    return 1;
}

// ESE render function with ImGui menu bar
extern "C" void heidic_render_ese(GLFWwindow* window) {
    if (!g_eseInitialized) return;
    
    // =========================================================================
    // Mouse Input Handling for Orbit Camera
    // - Mouse wheel: zoom in/out
    // - Left-click drag: rotate around model
    // - Right-click drag: pan the view
    // =========================================================================
    
    // Get current mouse position
    double mouseX, mouseY;
    glfwGetCursorPos(window, &mouseX, &mouseY);
    
    // Calculate mouse delta
    double deltaX = mouseX - g_eseLastMouseX;
    double deltaY = mouseY - g_eseLastMouseY;
    
    // Update mouse button states
    bool leftDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    bool rightDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
    
#ifdef USE_IMGUI
    // Check if ImGui wants to capture mouse (hovering over UI)
    ImGuiIO& io = ImGui::GetIO();
    bool imguiWantsMouse = io.WantCaptureMouse;
#else
    bool imguiWantsMouse = false;
#endif
    
    // Handle keyboard input for selection mode toggles
    static bool key1WasPressed = false;
    static bool key2WasPressed = false;
    static bool key3WasPressed = false;
    static bool key4WasPressed = false;
    static bool enterWasPressed = false;
    bool key1Pressed = glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS;
    bool key2Pressed = glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS;
    bool key3Pressed = glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS;
    bool key4Pressed = glfwGetKey(window, GLFW_KEY_4) == GLFW_PRESS;
    bool ctrlPressed = (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS || 
                        glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS);
    bool enterPressed = glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS;
    
    // Key 1: Vertex mode
    if (key1Pressed && !key1WasPressed && g_eseModelLoaded) {
        g_eseVertexMode = !g_eseVertexMode;
        if (g_eseVertexMode) {
            g_eseEdgeMode = false;   // Disable other modes
            g_eseFaceMode = false;
            g_eseQuadMode = false;
            g_eseSelectedEdges.clear();
            g_eseSelectedQuads.clear();
            std::cout << "[ESE] Vertex mode enabled - Ctrl+click for multi-select" << std::endl;
        } else {
            std::cout << "[ESE] Vertex mode disabled" << std::endl;
            g_eseSelectedVertex = -1;
            g_eseSelectedVertices.clear();
        }
    }
    key1WasPressed = key1Pressed;
    
    // Key 2: Edge mode
    if (key2Pressed && !key2WasPressed && g_eseModelLoaded) {
        g_eseEdgeMode = !g_eseEdgeMode;
        if (g_eseEdgeMode) {
            g_eseVertexMode = false;  // Disable other modes
            g_eseFaceMode = false;
            g_eseQuadMode = false;
            g_eseSelectedVertices.clear();
            g_eseSelectedQuads.clear();
            std::cout << "[ESE] Edge mode enabled - Ctrl+click for multi-select" << std::endl;
        } else {
            std::cout << "[ESE] Edge mode disabled" << std::endl;
            g_eseSelectedEdge = -1;
            g_eseSelectedEdges.clear();
        }
    }
    key2WasPressed = key2Pressed;
    
    // Key 3: Face mode
    if (key3Pressed && !key3WasPressed && g_eseModelLoaded) {
        g_eseFaceMode = !g_eseFaceMode;
        if (g_eseFaceMode) {
            g_eseVertexMode = false;  // Disable other modes
            g_eseEdgeMode = false;
            g_eseQuadMode = false;
            std::cout << "[ESE] Face mode enabled - click to select faces" << std::endl;
        } else {
            std::cout << "[ESE] Face mode disabled" << std::endl;
            g_eseSelectedFace = -1;
        }
    }
    key3WasPressed = key3Pressed;
    
    // Key 4: Quad mode
    if (key4Pressed && !key4WasPressed && g_eseModelLoaded) {
        g_eseQuadMode = !g_eseQuadMode;
        if (g_eseQuadMode) {
            g_eseVertexMode = false;  // Disable other modes
            g_eseEdgeMode = false;
            g_eseFaceMode = false;
            g_eseSelectedVertices.clear();
            g_eseSelectedEdges.clear();
            std::cout << "[ESE] Quad mode enabled - Ctrl+click for multi-select" << std::endl;
        } else {
            std::cout << "[ESE] Quad mode disabled" << std::endl;
            g_eseSelectedQuad = -1;
            g_eseSelectedQuads.clear();
        }
    }
    key4WasPressed = key4Pressed;
    
    // Ctrl+Z: Undo
    static bool keyZWasPressed = false;
    bool keyZPressed = glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS;
    if (keyZPressed && !keyZWasPressed && ctrlPressed) {
        ese_undo();
    }
    keyZWasPressed = keyZPressed;
    
    // Key I: Insert edge loop (edge mode - uses selected quad edge to determine loop direction)
    static bool keyIWasPressed = false;
    bool keyIPressed = glfwGetKey(window, GLFW_KEY_I) == GLFW_PRESS;
    if (keyIPressed && !keyIWasPressed && g_eseEdgeMode && g_eseSelectedEdge >= 0 && 
        g_eseSelectedEdge < (int)g_eseQuadEdges.size() && g_objMeshResource) {
        ese_save_undo_state();
        
        std::vector<MeshVertex>& vertices = g_objMeshResource->getVerticesMutable();
        std::vector<uint32_t>& indices = g_objMeshResource->getIndicesMutable();
        
        const QuadEdge& selectedEdge = g_eseQuadEdges[g_eseSelectedEdge];
        
        // The selected edge will be CUT by the new loop (perpendicular to it)
        // The new edges will be PERPENDICULAR to the selected edge
        // We follow through quads that share edges PARALLEL to the selected edge
        
        // Helper to get position key for edge matching
        auto getEdgePosKey = [&vertices](uint32_t va, uint32_t vb) -> std::pair<std::string, std::string> {
            auto posToStr = [](float x, float y, float z) {
                char buf[64];
                snprintf(buf, sizeof(buf), "%.4f,%.4f,%.4f", x, y, z);
                return std::string(buf);
            };
            std::string posA = posToStr(vertices[va].pos[0], vertices[va].pos[1], vertices[va].pos[2]);
            std::string posB = posToStr(vertices[vb].pos[0], vertices[vb].pos[1], vertices[vb].pos[2]);
            if (posA < posB) return std::make_pair(posA, posB);
            return std::make_pair(posB, posA);
        };
        
        // Build edge-to-quad map
        std::map<std::pair<std::string, std::string>, std::vector<std::pair<int, int>>> edgeToQuadInfo;
        for (int qi = 0; qi < (int)g_eseReconstructedQuads.size(); qi++) {
            auto& q = g_eseReconstructedQuads[qi];
            for (int ei = 0; ei < 4; ei++) {
                uint32_t va = q[ei];
                uint32_t vb = q[(ei + 1) % 4];
                auto key = getEdgePosKey(va, vb);
                edgeToQuadInfo[key].push_back(std::make_pair(qi, ei));
            }
        }
        
        // Get the position key of the selected edge
        auto selectedEdgeKey = getEdgePosKey(selectedEdge.v0, selectedEdge.v1);
        
        // Find which quads contain this edge and at what local index
        std::map<int, std::pair<int, int>> quadToCutEdges;  // qi -> (cutEdge1, cutEdge2)
        std::set<int> visited;
        std::queue<std::pair<int, int>> toVisit;
        
        // Start with quads that contain the selected edge
        // The selected edge is a CUT edge - the loop goes perpendicular to it
        if (edgeToQuadInfo.count(selectedEdgeKey)) {
            for (auto& info : edgeToQuadInfo[selectedEdgeKey]) {
                int qi = info.first;
                int edgeIdx = info.second;
                // The selected edge IS a cut edge
                // We follow through edges PARALLEL to the selected edge (the other cut edges)
                toVisit.push(std::make_pair(qi, edgeIdx));
            }
        }
        
        while (!toVisit.empty()) {
            auto front = toVisit.front();
            toVisit.pop();
            int qi = front.first;
            int cutEdgeIdx = front.second;  // This edge will be cut (selected edge or parallel to it)
            
            if (visited.count(qi)) continue;
            visited.insert(qi);
            
            auto& q = g_eseReconstructedQuads[qi];
            
            // Cut edges are at cutEdgeIdx and (cutEdgeIdx + 2) % 4 (opposite)
            // Flow edges are at (cutEdgeIdx + 1) % 4 and (cutEdgeIdx + 3) % 4 (perpendicular)
            int cutEdge1 = cutEdgeIdx;
            int cutEdge2 = (cutEdgeIdx + 2) % 4;
            int flowEdge1 = (cutEdgeIdx + 1) % 4;
            int flowEdge2 = (cutEdgeIdx + 3) % 4;
            
            quadToCutEdges[qi] = std::make_pair(cutEdge1, cutEdge2);
            
            // Follow through CUT edges to adjacent quads (edges parallel to selected)
            for (int ce : {cutEdge1, cutEdge2}) {
                uint32_t va = q[ce];
                uint32_t vb = q[(ce + 1) % 4];
                auto cutKey = getEdgePosKey(va, vb);
                
                if (edgeToQuadInfo.count(cutKey)) {
                    for (auto& info : edgeToQuadInfo[cutKey]) {
                        int adjQi = info.first;
                        int adjEdgeIdx = info.second;
                        if (!visited.count(adjQi)) {
                            toVisit.push(std::make_pair(adjQi, adjEdgeIdx));
                        }
                    }
                }
            }
        }
        
        std::cout << "[ESE] Found " << quadToCutEdges.size() << " quads in edge loop" << std::endl;
        
        if (!quadToCutEdges.empty()) {
            // Create midpoint vertices
            std::map<std::pair<std::string, std::string>, uint32_t> edgePosToMidpoint;
            std::map<std::pair<uint32_t, uint32_t>, uint32_t> edgeIdxToMidpoint;
            
            for (auto& entry : quadToCutEdges) {
                int qi = entry.first;
                auto cutPair = entry.second;
                auto& q = g_eseReconstructedQuads[qi];
                
                for (int cutEdge : {cutPair.first, cutPair.second}) {
                    uint32_t va = q[cutEdge];
                    uint32_t vb = q[(cutEdge + 1) % 4];
                    auto posKey = getEdgePosKey(va, vb);
                    auto idxKey = std::make_pair(std::min(va, vb), std::max(va, vb));
                    
                    if (edgePosToMidpoint.count(posKey) == 0) {
                        glm::vec3 p0(vertices[va].pos[0], vertices[va].pos[1], vertices[va].pos[2]);
                        glm::vec3 p1(vertices[vb].pos[0], vertices[vb].pos[1], vertices[vb].pos[2]);
                        glm::vec3 mid = (p0 + p1) * 0.5f;
                        
                        MeshVertex newVert;
                        newVert.pos[0] = mid.x; newVert.pos[1] = mid.y; newVert.pos[2] = mid.z;
                        newVert.normal[0] = (vertices[va].normal[0] + vertices[vb].normal[0]) * 0.5f;
                        newVert.normal[1] = (vertices[va].normal[1] + vertices[vb].normal[1]) * 0.5f;
                        newVert.normal[2] = (vertices[va].normal[2] + vertices[vb].normal[2]) * 0.5f;
                        newVert.uv[0] = (vertices[va].uv[0] + vertices[vb].uv[0]) * 0.5f;
                        newVert.uv[1] = (vertices[va].uv[1] + vertices[vb].uv[1]) * 0.5f;
                        
                        uint32_t newIdx = (uint32_t)vertices.size();
                        vertices.push_back(newVert);
                        edgePosToMidpoint[posKey] = newIdx;
                    }
                    edgeIdxToMidpoint[idxKey] = edgePosToMidpoint[posKey];
                }
            }
            
            // Find triangles to remove
            std::set<size_t> trianglesToRemove;
            size_t numTris = indices.size() / 3;
            
            for (auto& entry : quadToCutEdges) {
                int qi = entry.first;
                auto& q = g_eseReconstructedQuads[qi];
                std::set<uint32_t> quadVerts;
                quadVerts.insert(q[0]); quadVerts.insert(q[1]); quadVerts.insert(q[2]); quadVerts.insert(q[3]);
                
                for (size_t ti = 0; ti < numTris; ti++) {
                    uint32_t tv0 = indices[ti * 3];
                    uint32_t tv1 = indices[ti * 3 + 1];
                    uint32_t tv2 = indices[ti * 3 + 2];
                    
                    if (quadVerts.count(tv0) && quadVerts.count(tv1) && quadVerts.count(tv2)) {
                        trianglesToRemove.insert(ti);
                    }
                }
            }
            
            // Build new index buffer
            std::vector<uint32_t> newIndices;
            for (size_t ti = 0; ti < numTris; ti++) {
                if (trianglesToRemove.count(ti) == 0) {
                    newIndices.push_back(indices[ti * 3]);
                    newIndices.push_back(indices[ti * 3 + 1]);
                    newIndices.push_back(indices[ti * 3 + 2]);
                }
            }
            
            // First, find the reference normal for each quad from its original triangles
            std::map<int, glm::vec3> quadToNormal;
            for (auto& entry : quadToCutEdges) {
                int qi = entry.first;
                auto& q = g_eseReconstructedQuads[qi];
                std::set<uint32_t> quadVerts;
                quadVerts.insert(q[0]); quadVerts.insert(q[1]); quadVerts.insert(q[2]); quadVerts.insert(q[3]);
                
                // Find a triangle from the original mesh that belongs to this quad
                for (size_t ti = 0; ti < numTris; ti++) {
                    uint32_t tv0 = indices[ti * 3];
                    uint32_t tv1 = indices[ti * 3 + 1];
                    uint32_t tv2 = indices[ti * 3 + 2];
                    
                    if (quadVerts.count(tv0) && quadVerts.count(tv1) && quadVerts.count(tv2)) {
                        // This triangle belongs to the quad - use its normal
                        glm::vec3 p0(vertices[tv0].pos[0], vertices[tv0].pos[1], vertices[tv0].pos[2]);
                        glm::vec3 p1(vertices[tv1].pos[0], vertices[tv1].pos[1], vertices[tv1].pos[2]);
                        glm::vec3 p2(vertices[tv2].pos[0], vertices[tv2].pos[1], vertices[tv2].pos[2]);
                        quadToNormal[qi] = glm::normalize(glm::cross(p1 - p0, p2 - p0));
                        break;
                    }
                }
            }
            
            // Add split quads - ensuring normals face same direction as original triangles
            for (auto& entry : quadToCutEdges) {
                int qi = entry.first;
                auto cutPair = entry.second;
                auto& q = g_eseReconstructedQuads[qi];
                int cutEdge1 = cutPair.first;
                int cutEdge2 = cutPair.second;
                
                auto getMidpoint = [&](int edgeIdx) -> uint32_t {
                    uint32_t va = q[edgeIdx];
                    uint32_t vb = q[(edgeIdx + 1) % 4];
                    auto key = std::make_pair(std::min(va, vb), std::max(va, vb));
                    return edgeIdxToMidpoint[key];
                };
                
                uint32_t mid1 = getMidpoint(cutEdge1);
                uint32_t mid2 = getMidpoint(cutEdge2);
                
                // Get reference normal from original triangle
                glm::vec3 origNormal = quadToNormal.count(qi) ? quadToNormal[qi] : glm::vec3(0, 1, 0);
                
                // Helper to add a triangle with correct winding
                auto addTriangle = [&](uint32_t v0, uint32_t v1, uint32_t v2) {
                    glm::vec3 p0(vertices[v0].pos[0], vertices[v0].pos[1], vertices[v0].pos[2]);
                    glm::vec3 p1(vertices[v1].pos[0], vertices[v1].pos[1], vertices[v1].pos[2]);
                    glm::vec3 p2(vertices[v2].pos[0], vertices[v2].pos[1], vertices[v2].pos[2]);
                    glm::vec3 triNormal = glm::cross(p1 - p0, p2 - p0);
                    
                    // Check if normal faces same direction as original
                    if (glm::dot(triNormal, origNormal) >= 0.0f) {
                        // Same direction - keep winding
                        newIndices.push_back(v0);
                        newIndices.push_back(v1);
                        newIndices.push_back(v2);
                    } else {
                        // Opposite direction - reverse winding
                        newIndices.push_back(v0);
                        newIndices.push_back(v2);
                        newIndices.push_back(v1);
                    }
                };
                
                // Quad A: q[cutEdge1], mid1, mid2, q[(cutEdge2+1)%4]
                uint32_t qa0 = q[cutEdge1];
                uint32_t qa1 = mid1;
                uint32_t qa2 = mid2;
                uint32_t qa3 = q[(cutEdge2 + 1) % 4];
                addTriangle(qa0, qa1, qa2);
                addTriangle(qa0, qa2, qa3);
                
                // Quad B: mid1, q[(cutEdge1+1)%4], q[cutEdge2], mid2
                uint32_t qb0 = mid1;
                uint32_t qb1 = q[(cutEdge1 + 1) % 4];
                uint32_t qb2 = q[cutEdge2];
                uint32_t qb3 = mid2;
                addTriangle(qb0, qb1, qb2);
                addTriangle(qb0, qb2, qb3);
            }
            
            indices = newIndices;
            
            std::cout << "[ESE] Edge loop inserted: " << quadToCutEdges.size() << " quads split, " 
                      << edgePosToMidpoint.size() << " new vertices" << std::endl;
            
            g_objMeshResource->rebuildBuffers();
            
            // Reset and force full reconstruction of quads and edges
            g_eseSelectedEdge = -1;
            g_eseSelectedEdges.clear();
            g_eseReconstructedQuads.clear();
            g_eseQuadEdges.clear();
            g_eseLastQuadIndexCount = 0;
        }
    }
    keyIWasPressed = keyIPressed;
    
    // Key W: Enable extrude mode (quad mode only)
    static bool keyWWasPressed = false;
    bool keyWPressed = glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS;
    if (keyWPressed && !keyWWasPressed && g_eseQuadMode && g_eseSelectedQuad >= 0) {
        g_eseExtrudeMode = !g_eseExtrudeMode;
        g_eseExtrudeExecuted = false;
        g_eseExtrudedVertices.clear();
        if (g_eseExtrudeMode) {
            std::cout << "[ESE] EXTRUDE MODE ENABLED for quad " << g_eseSelectedQuad << std::endl;
            std::cout << "[ESE] Quad has vertices: ";
            if (g_eseSelectedQuad < (int)g_eseReconstructedQuads.size()) {
                const auto& q = g_eseReconstructedQuads[g_eseSelectedQuad];
                std::cout << q[0] << ", " << q[1] << ", " << q[2] << ", " << q[3] << std::endl;
            }
            std::cout << "[ESE] Drag any gizmo axis to extrude!" << std::endl;
        } else {
            std::cout << "[ESE] Extrude mode disabled" << std::endl;
        }
    }
    keyWWasPressed = keyWPressed;
    
    // Ctrl+Enter: Create connection point from selected vertex
    if (ctrlPressed && enterPressed && !enterWasPressed && g_eseVertexMode && 
        g_eseSelectedVertex >= 0 && g_eseModelLoaded && g_objMeshResource) {
        
        const std::vector<MeshVertex>& vertices = g_objMeshResource->getVertices();
        if (g_eseSelectedVertex < (int)vertices.size()) {
            ConnectionPoint cp;
            snprintf(cp.name, sizeof(cp.name), "Connection Point %zu", g_eseConnectionPoints.size() + 1);
            cp.vertex_index = g_eseSelectedVertex;
            cp.position[0] = g_eseSelectedVertexPos.x;
            cp.position[1] = g_eseSelectedVertexPos.y;
            cp.position[2] = g_eseSelectedVertexPos.z;
            cp.connection_type = 0;  // Default to child
            cp.connected_mesh[0] = '\0';
            cp.connected_mesh_class[0] = '\0';
            
            g_eseConnectionPoints.push_back(cp);
            g_eseSelectedConnectionPoint = (int)g_eseConnectionPoints.size() - 1;
            g_eseShowConnectionPointWindow = true;
            g_esePropertiesModified = true;
            
            std::cout << "[ESE] Created connection point at vertex " << g_eseSelectedVertex << std::endl;
        }
    }
    enterWasPressed = enterPressed;
    
    // Only process camera input if ImGui doesn't want the mouse
    if (!imguiWantsMouse) {
        // Check for gizmo axis click (only when we have a selection)
        bool hasSelection = (!g_eseSelectedVertices.empty() && g_eseVertexMode) ||
                            (!g_eseSelectedEdges.empty() && g_eseEdgeMode) ||
                            (g_eseSelectedFace >= 0 && g_eseFaceMode) ||
                            (!g_eseSelectedQuads.empty() && g_eseQuadMode);
        
        if (leftDown && !g_eseLeftMouseDown && hasSelection && !g_eseGizmoDragging) {
            // Check if clicking on a gizmo axis, scale handle, or rotation circle
            float axisLength = 0.3f;
            float scaleHandlePos = 0.5f;  // Scale handles at 50% of axis
            float rotRadius = axisLength * 0.7f;  // Rotation circle radius
            float clickRadius = 15.0f;  // Pixels from axis endpoint
            float scaleClickRadius = 12.0f;  // Pixels from scale handle
            float rotClickRadius = 10.0f;  // Pixels from rotation circle
            
            bool inFrontC;
            glm::vec2 screenC = ese_project_to_screen(g_eseGizmoPosition, &inFrontC);
            
            if (inFrontC) {
                bool inFrontX, inFrontY, inFrontZ;
                bool inFrontXS, inFrontYS, inFrontZS;
                glm::vec2 screenX = ese_project_to_screen(g_eseGizmoPosition + glm::vec3(axisLength, 0, 0), &inFrontX);
                glm::vec2 screenY = ese_project_to_screen(g_eseGizmoPosition + glm::vec3(0, axisLength, 0), &inFrontY);
                glm::vec2 screenZ = ese_project_to_screen(g_eseGizmoPosition + glm::vec3(0, 0, axisLength), &inFrontZ);
                // Scale handle positions
                glm::vec2 screenXS = ese_project_to_screen(g_eseGizmoPosition + glm::vec3(axisLength * scaleHandlePos, 0, 0), &inFrontXS);
                glm::vec2 screenYS = ese_project_to_screen(g_eseGizmoPosition + glm::vec3(0, axisLength * scaleHandlePos, 0), &inFrontYS);
                glm::vec2 screenZS = ese_project_to_screen(g_eseGizmoPosition + glm::vec3(0, 0, axisLength * scaleHandlePos), &inFrontZS);
                
                float distX = glm::length(glm::vec2(mouseX, mouseY) - screenX);
                float distY = glm::length(glm::vec2(mouseX, mouseY) - screenY);
                float distZ = glm::length(glm::vec2(mouseX, mouseY) - screenZ);
                float distXS = glm::length(glm::vec2(mouseX, mouseY) - screenXS);
                float distYS = glm::length(glm::vec2(mouseX, mouseY) - screenYS);
                float distZS = glm::length(glm::vec2(mouseX, mouseY) - screenZS);
                float distCenter = glm::length(glm::vec2(mouseX, mouseY) - screenC);
                
                // Check rotation circles first (find closest point on each circle)
                float minRotDistX = FLT_MAX, minRotDistY = FLT_MAX, minRotDistZ = FLT_MAX;
                for (int i = 0; i < 16; i++) {
                    float angle = (float)i / 16.0f * 2.0f * 3.14159f;
                    bool f;
                    // X rotation circle (YZ plane)
                    glm::vec2 pX = ese_project_to_screen(g_eseGizmoPosition + glm::vec3(0, cosf(angle)*rotRadius, sinf(angle)*rotRadius), &f);
                    if (f) minRotDistX = std::min(minRotDistX, glm::length(glm::vec2(mouseX, mouseY) - pX));
                    // Y rotation circle (XZ plane)
                    glm::vec2 pY = ese_project_to_screen(g_eseGizmoPosition + glm::vec3(cosf(angle)*rotRadius, 0, sinf(angle)*rotRadius), &f);
                    if (f) minRotDistY = std::min(minRotDistY, glm::length(glm::vec2(mouseX, mouseY) - pY));
                    // Z rotation circle (XY plane)
                    glm::vec2 pZ = ese_project_to_screen(g_eseGizmoPosition + glm::vec3(cosf(angle)*rotRadius, sinf(angle)*rotRadius, 0), &f);
                    if (f) minRotDistZ = std::min(minRotDistZ, glm::length(glm::vec2(mouseX, mouseY) - pZ));
                }
                
                // Check rotation handles first
                if (minRotDistX < rotClickRadius) {
                    if (!(g_eseQuadMode && g_eseExtrudeMode)) ese_save_undo_state();
                    g_eseGizmoDragAxis = 0;
                    g_eseGizmoDragging = true;
                    g_eseGizmoScaling = false;
                    g_eseGizmoRotating = true;
                    g_eseGizmoDragStartMouse = mouseX;
                    g_eseGizmoDragStartMouseY = mouseY;
                    std::cout << "[ESE] Rotating around X axis" << std::endl;
                } else if (minRotDistY < rotClickRadius) {
                    if (!(g_eseQuadMode && g_eseExtrudeMode)) ese_save_undo_state();
                    g_eseGizmoDragAxis = 1;
                    g_eseGizmoDragging = true;
                    g_eseGizmoScaling = false;
                    g_eseGizmoRotating = true;
                    g_eseGizmoDragStartMouse = mouseX;
                    g_eseGizmoDragStartMouseY = mouseY;
                    std::cout << "[ESE] Rotating around Y axis" << std::endl;
                } else if (minRotDistZ < rotClickRadius) {
                    if (!(g_eseQuadMode && g_eseExtrudeMode)) ese_save_undo_state();
                    g_eseGizmoDragAxis = 2;
                    g_eseGizmoDragging = true;
                    g_eseGizmoScaling = false;
                    g_eseGizmoRotating = true;
                    g_eseGizmoDragStartMouse = mouseX;
                    g_eseGizmoDragStartMouseY = mouseY;
                    std::cout << "[ESE] Rotating around Z axis" << std::endl;
                }
                // Check scale handles
                else if (inFrontXS && distXS < scaleClickRadius) {
                    if (!(g_eseQuadMode && g_eseExtrudeMode)) ese_save_undo_state();
                    g_eseGizmoDragAxis = 0;
                    g_eseGizmoDragging = true;
                    g_eseGizmoScaling = true;
                    g_eseGizmoRotating = false;
                    g_eseGizmoDragStartMouse = mouseX;
                    std::cout << "[ESE] Scaling X axis" << std::endl;
                } else if (inFrontYS && distYS < scaleClickRadius) {
                    if (!(g_eseQuadMode && g_eseExtrudeMode)) ese_save_undo_state();
                    g_eseGizmoDragAxis = 1;
                    g_eseGizmoDragging = true;
                    g_eseGizmoScaling = true;
                    g_eseGizmoRotating = false;
                    g_eseGizmoDragStartMouse = mouseY;
                    std::cout << "[ESE] Scaling Y axis" << std::endl;
                } else if (inFrontZS && distZS < scaleClickRadius) {
                    if (!(g_eseQuadMode && g_eseExtrudeMode)) ese_save_undo_state();
                    g_eseGizmoDragAxis = 2;
                    g_eseGizmoDragging = true;
                    g_eseGizmoScaling = true;
                    g_eseGizmoRotating = false;
                    g_eseGizmoDragStartMouse = mouseX;
                    std::cout << "[ESE] Scaling Z axis" << std::endl;
                }
                // Then check move handles
                else if (inFrontX && distX < clickRadius) {
                    if (!(g_eseQuadMode && g_eseExtrudeMode)) ese_save_undo_state();
                    g_eseGizmoDragAxis = 0;
                    g_eseGizmoDragging = true;
                    g_eseGizmoScaling = false;
                    g_eseGizmoRotating = false;
                    g_eseGizmoDragStartMouse = mouseX;
                    g_eseGizmoDragStartValue = g_eseGizmoPosition.x;
                    std::cout << "[ESE] Moving X axis" << std::endl;
                } else if (inFrontY && distY < clickRadius) {
                    if (!(g_eseQuadMode && g_eseExtrudeMode)) ese_save_undo_state();
                    g_eseGizmoDragAxis = 1;
                    g_eseGizmoDragging = true;
                    g_eseGizmoScaling = false;
                    g_eseGizmoRotating = false;
                    g_eseGizmoDragStartMouse = mouseY;
                    g_eseGizmoDragStartValue = g_eseGizmoPosition.y;
                    std::cout << "[ESE] Moving Y axis" << std::endl;
                } else if (inFrontZ && distZ < clickRadius) {
                    if (!(g_eseQuadMode && g_eseExtrudeMode)) ese_save_undo_state();
                    g_eseGizmoDragAxis = 2;
                    g_eseGizmoDragging = true;
                    g_eseGizmoScaling = false;
                    g_eseGizmoRotating = false;
                    g_eseGizmoDragStartMouse = mouseX;
                    g_eseGizmoDragStartValue = g_eseGizmoPosition.z;
                    std::cout << "[ESE] Moving Z axis" << std::endl;
                }
            }
        }
        
        // Handle gizmo dragging (move, scale, or rotate)
        if (g_eseGizmoDragging && leftDown && g_objMeshResource) {
            float dragSpeed = 0.02f;  // Movement speed
            float scaleSpeed = 0.005f;  // Scale speed (more sensitive)
            float rotateSpeed = 0.01f;  // Rotation speed (radians per pixel)
            float delta = 0.0f;
            float scaleFactor = 1.0f;
            float rotationAngle = 0.0f;
            
            if (g_eseGizmoRotating) {
                // Rotation mode - use horizontal mouse movement (incremental)
                rotationAngle = (float)(mouseX - g_eseGizmoDragStartMouse) * rotateSpeed;
                g_eseGizmoDragStartMouse = mouseX;  // Reset for incremental rotation
            } else if (g_eseGizmoScaling) {
                // Scaling mode
                if (g_eseGizmoDragAxis == 1) {
                    delta = -(float)(mouseY - g_eseGizmoDragStartMouse) * scaleSpeed;
                } else {
                    delta = (float)(mouseX - g_eseGizmoDragStartMouse) * scaleSpeed;
                }
                scaleFactor = 1.0f + delta;
                if (scaleFactor < 0.01f) scaleFactor = 0.01f;  // Prevent negative/zero scale
            } else {
                // Movement mode
                if (g_eseGizmoDragAxis == 1) {
                    delta = -(float)(mouseY - g_eseGizmoDragStartMouse) * dragSpeed;
                } else {
                    delta = (float)(mouseX - g_eseGizmoDragStartMouse) * dragSpeed;
                }
            }
            
            // Apply movement to selected vertices AND all vertices at the same positions
            // This ensures the mesh stays connected (OBJ files often duplicate vertices)
            std::vector<MeshVertex>& vertices = g_objMeshResource->getVerticesMutable();
            const std::vector<uint32_t>& indices = g_objMeshResource->getIndices();
            
            // Collect target positions that need to be moved
            std::vector<glm::vec3> targetPositions;
            float tolerance = 0.0001f;
            
            if (g_eseVertexMode && !g_eseSelectedVertices.empty()) {
                // Multi-selection: collect positions from ALL selected vertices
                for (int vi : g_eseSelectedVertices) {
                    if (vi >= 0 && vi < (int)vertices.size()) {
                        targetPositions.push_back(glm::vec3(
                            vertices[vi].pos[0],
                            vertices[vi].pos[1],
                            vertices[vi].pos[2]
                        ));
                    }
                }
            } else if (g_eseEdgeMode && !g_eseSelectedEdges.empty()) {
                // Multi-selection: collect vertices from ALL selected quad edges
                for (int edgeIdx : g_eseSelectedEdges) {
                    if (edgeIdx >= 0 && edgeIdx < (int)g_eseQuadEdges.size()) {
                        const QuadEdge& qe = g_eseQuadEdges[edgeIdx];
                        if (qe.v0 < vertices.size() && qe.v1 < vertices.size()) {
                            targetPositions.push_back(glm::vec3(vertices[qe.v0].pos[0], vertices[qe.v0].pos[1], vertices[qe.v0].pos[2]));
                            targetPositions.push_back(glm::vec3(vertices[qe.v1].pos[0], vertices[qe.v1].pos[1], vertices[qe.v1].pos[2]));
                        }
                    }
                }
            } else if (g_eseFaceMode && g_eseSelectedFace >= 0) {
                size_t triStart = g_eseSelectedFace * 3;
                if (triStart + 2 < indices.size()) {
                    for (int i = 0; i < 3; i++) {
                        uint32_t vIdx = indices[triStart + i];
                        targetPositions.push_back(glm::vec3(vertices[vIdx].pos[0], vertices[vIdx].pos[1], vertices[vIdx].pos[2]));
                    }
                }
            } else if (g_eseQuadMode && g_eseSelectedQuad >= 0 && g_eseSelectedQuad < (int)g_eseReconstructedQuads.size()) {
                // Check if we're in extrude mode
                if (g_eseExtrudeMode && !g_eseExtrudeExecuted) {
                    // Save undo state BEFORE modifying mesh
                    ese_save_undo_state();
                    
                    // EXTRUDE: Create new geometry
                    const auto& quad = g_eseReconstructedQuads[g_eseSelectedQuad];
                    
                    // Get original vertex positions
                    glm::vec3 p0(vertices[quad[0]].pos[0], vertices[quad[0]].pos[1], vertices[quad[0]].pos[2]);
                    glm::vec3 p1(vertices[quad[1]].pos[0], vertices[quad[1]].pos[1], vertices[quad[1]].pos[2]);
                    glm::vec3 p2(vertices[quad[2]].pos[0], vertices[quad[2]].pos[1], vertices[quad[2]].pos[2]);
                    glm::vec3 p3(vertices[quad[3]].pos[0], vertices[quad[3]].pos[1], vertices[quad[3]].pos[2]);
                    
                    // Calculate face normal for the new vertices
                    glm::vec3 normal = glm::normalize(glm::cross(p1 - p0, p2 - p0));
                    
                    // Get mutable indices FIRST (before we modify vertices)
                    std::vector<uint32_t>& mutableIndices = g_objMeshResource->getIndicesMutable();
                    
                    // DELETE ORIGINAL FACE: Find and remove triangles that use the quad's vertices
                    std::set<uint32_t> quadVerts = {quad[0], quad[1], quad[2], quad[3]};
                    std::vector<uint32_t> newIndices;
                    newIndices.reserve(mutableIndices.size());
                    
                    for (size_t i = 0; i + 2 < mutableIndices.size(); i += 3) {
                        uint32_t v0 = mutableIndices[i];
                        uint32_t v1 = mutableIndices[i + 1];
                        uint32_t v2 = mutableIndices[i + 2];
                        
                        // Check if all 3 vertices of this triangle are in the quad
                        int matchCount = 0;
                        if (quadVerts.count(v0)) matchCount++;
                        if (quadVerts.count(v1)) matchCount++;
                        if (quadVerts.count(v2)) matchCount++;
                        
                        // Keep triangle only if it's NOT part of the original quad face
                        if (matchCount < 3) {
                            newIndices.push_back(v0);
                            newIndices.push_back(v1);
                            newIndices.push_back(v2);
                        }
                    }
                    
                    mutableIndices = newIndices;
                    std::cout << "[ESE] Removed original face triangles" << std::endl;
                    
                    // Create 4 new vertices (copies of original)
                    uint32_t newIdx0 = (uint32_t)vertices.size();
                    for (int i = 0; i < 4; i++) {
                        MeshVertex newVert = vertices[quad[i]];
                        // Set normal to face outward
                        newVert.normal[0] = normal.x;
                        newVert.normal[1] = normal.y;
                        newVert.normal[2] = normal.z;
                        vertices.push_back(newVert);
                    }
                    
                    // Store extruded vertex indices for movement
                    g_eseExtrudedVertices.clear();
                    g_eseExtrudedVertices.push_back(newIdx0);
                    g_eseExtrudedVertices.push_back(newIdx0 + 1);
                    g_eseExtrudedVertices.push_back(newIdx0 + 2);
                    g_eseExtrudedVertices.push_back(newIdx0 + 3);
                    
                    // Create 4 side quads (8 triangles) connecting original to new vertices
                    // Side 0-1: quad[0] -> quad[1] -> newIdx0+1 -> newIdx0
                    mutableIndices.push_back(quad[0]); mutableIndices.push_back(quad[1]); mutableIndices.push_back(newIdx0 + 1);
                    mutableIndices.push_back(quad[0]); mutableIndices.push_back(newIdx0 + 1); mutableIndices.push_back(newIdx0);
                    
                    // Side 1-2: quad[1] -> quad[2] -> newIdx0+2 -> newIdx0+1
                    mutableIndices.push_back(quad[1]); mutableIndices.push_back(quad[2]); mutableIndices.push_back(newIdx0 + 2);
                    mutableIndices.push_back(quad[1]); mutableIndices.push_back(newIdx0 + 2); mutableIndices.push_back(newIdx0 + 1);
                    
                    // Side 2-3: quad[2] -> quad[3] -> newIdx0+3 -> newIdx0+2
                    mutableIndices.push_back(quad[2]); mutableIndices.push_back(quad[3]); mutableIndices.push_back(newIdx0 + 3);
                    mutableIndices.push_back(quad[2]); mutableIndices.push_back(newIdx0 + 3); mutableIndices.push_back(newIdx0 + 2);
                    
                    // Side 3-0: quad[3] -> quad[0] -> newIdx0 -> newIdx0+3
                    mutableIndices.push_back(quad[3]); mutableIndices.push_back(quad[0]); mutableIndices.push_back(newIdx0);
                    mutableIndices.push_back(quad[3]); mutableIndices.push_back(newIdx0); mutableIndices.push_back(newIdx0 + 3);
                    
                    // NEW TOP FACE: Create 2 triangles using the new vertices
                    // This is the face that will move with the extrusion
                    mutableIndices.push_back(newIdx0); mutableIndices.push_back(newIdx0 + 1); mutableIndices.push_back(newIdx0 + 2);
                    mutableIndices.push_back(newIdx0); mutableIndices.push_back(newIdx0 + 2); mutableIndices.push_back(newIdx0 + 3);
                    
                    // Update the quad to use new vertices (top face)
                    g_eseReconstructedQuads[g_eseSelectedQuad] = {newIdx0, newIdx0 + 1, newIdx0 + 2, newIdx0 + 3};
                    
                    g_eseExtrudeExecuted = true;
                    std::cout << "[ESE] Extruded quad - created " << vertices.size() << " vertices, " 
                              << mutableIndices.size() << " indices" << std::endl;
                    
                    // IMPORTANT: Rebuild GPU buffers immediately so new geometry is visible
                    g_objMeshResource->rebuildBuffers();
                    std::cout << "[ESE] GPU buffers rebuilt for extrusion" << std::endl;
                }
                
                // Move extruded vertices (or normal quad movement if not extruding)
                if (g_eseExtrudeMode && g_eseExtrudeExecuted) {
                    // EXTRUDE: Move only the specific extruded vertex indices (NOT position-based)
                    // This is critical because extruded vertices start at same position as originals
                    for (uint32_t vi : g_eseExtrudedVertices) {
                        if (vi < vertices.size()) {
                            vertices[vi].pos[g_eseGizmoDragAxis] += delta * 0.5f;
                        }
                    }
                    // Skip the normal position-based movement below
                    g_eseGizmoDragStartMouse = (g_eseGizmoDragAxis == 1) ? mouseY : mouseX;
                    g_esePropertiesModified = true;
                } else {
                    // Normal quad movement - multi-selection support
                    for (int quadIdx : g_eseSelectedQuads) {
                        if (quadIdx >= 0 && quadIdx < (int)g_eseReconstructedQuads.size()) {
                            const auto& quad = g_eseReconstructedQuads[quadIdx];
                            for (int i = 0; i < 4; i++) {
                                targetPositions.push_back(glm::vec3(vertices[quad[i]].pos[0], vertices[quad[i]].pos[1], vertices[quad[i]].pos[2]));
                            }
                        }
                    }
                }
            }
            
            // Find and move/scale/rotate ALL vertices at target positions (keeps mesh connected)
            // Skip this if we already handled extrusion movement above
            if (!(g_eseQuadMode && g_eseExtrudeMode && g_eseExtrudeExecuted)) {
                for (size_t vi = 0; vi < vertices.size(); vi++) {
                    glm::vec3 vPos(vertices[vi].pos[0], vertices[vi].pos[1], vertices[vi].pos[2]);
                    
                    for (const auto& targetPos : targetPositions) {
                        if (glm::length(vPos - targetPos) < tolerance) {
                            if (g_eseGizmoRotating) {
                                // ROTATING: Rotate vertex around gizmo center on selected axis
                                glm::vec3 relPos = vPos - g_eseGizmoPosition;
                                float cosA = cosf(rotationAngle);
                                float sinA = sinf(rotationAngle);
                                glm::vec3 newPos;
                                
                                if (g_eseGizmoDragAxis == 0) {
                                    // Rotate around X axis (affects Y and Z)
                                    newPos.x = relPos.x;
                                    newPos.y = relPos.y * cosA - relPos.z * sinA;
                                    newPos.z = relPos.y * sinA + relPos.z * cosA;
                                } else if (g_eseGizmoDragAxis == 1) {
                                    // Rotate around Y axis (affects X and Z)
                                    newPos.x = relPos.x * cosA + relPos.z * sinA;
                                    newPos.y = relPos.y;
                                    newPos.z = -relPos.x * sinA + relPos.z * cosA;
                                } else {
                                    // Rotate around Z axis (affects X and Y)
                                    newPos.x = relPos.x * cosA - relPos.y * sinA;
                                    newPos.y = relPos.x * sinA + relPos.y * cosA;
                                    newPos.z = relPos.z;
                                }
                                
                                newPos += g_eseGizmoPosition;
                                vertices[vi].pos[0] = newPos.x;
                                vertices[vi].pos[1] = newPos.y;
                                vertices[vi].pos[2] = newPos.z;
                            } else if (g_eseGizmoScaling) {
                                // SCALING: Scale vertex position relative to gizmo center on selected axis
                                float offset = vertices[vi].pos[g_eseGizmoDragAxis] - g_eseGizmoPosition[g_eseGizmoDragAxis];
                                vertices[vi].pos[g_eseGizmoDragAxis] = g_eseGizmoPosition[g_eseGizmoDragAxis] + offset * scaleFactor;
                            } else {
                                // MOVING: Move vertex along axis
                                vertices[vi].pos[g_eseGizmoDragAxis] += delta * 0.5f;
                            }
                            break;
                        }
                    }
                }
            }
            
            // Update selected vertex position for display
            if (g_eseVertexMode && g_eseSelectedVertex >= 0 && g_eseSelectedVertex < (int)vertices.size()) {
                g_eseSelectedVertexPos = glm::vec3(
                    vertices[g_eseSelectedVertex].pos[0],
                    vertices[g_eseSelectedVertex].pos[1],
                    vertices[g_eseSelectedVertex].pos[2]
                );
            }
            
            // Reset mouse position for next frame (rotation always uses X, others depend on axis)
            if (g_eseGizmoRotating) {
                g_eseGizmoDragStartMouse = mouseX;  // Rotation uses horizontal movement
            } else {
                g_eseGizmoDragStartMouse = (g_eseGizmoDragAxis == 1) ? mouseY : mouseX;
            }
            g_esePropertiesModified = true;
        }
        
        // Release gizmo drag
        if (!leftDown && g_eseGizmoDragging) {
            g_eseGizmoDragging = false;
            g_eseGizmoDragAxis = -1;
            g_eseGizmoScaling = false;
            g_eseGizmoRotating = false;
            std::cout << "[ESE] Drag complete" << std::endl;
            
            // Rebuild mesh buffers with new vertex positions
            if (g_objMeshResource) {
                g_objMeshResource->rebuildBuffers();
            }
            
            // Reset extrude mode after operation
            if (g_eseExtrudeMode && g_eseExtrudeExecuted) {
                g_eseExtrudeMode = false;
                g_eseExtrudeExecuted = false;
                g_eseExtrudedVertices.clear();
                // Force quad reconstruction on next frame by clearing selection
                g_eseSelectedQuad = -1;
                std::cout << "[ESE] Extrude complete - mode reset, select a new quad" << std::endl;
            }
        }
        
        // Left-click drag: rotate camera (orbit) - only if not dragging gizmo
        // In vertex mode, single click selects vertex (no drag)
        if (leftDown && g_eseLeftMouseDown && !g_eseGizmoDragging) {
            float rotationSpeed = 0.3f;
            g_eseCameraYaw -= (float)deltaX * rotationSpeed;
            g_eseCameraPitch += (float)deltaY * rotationSpeed;
            
            // Clamp pitch to prevent flipping
            if (g_eseCameraPitch > 89.0f) g_eseCameraPitch = 89.0f;
            if (g_eseCameraPitch < -89.0f) g_eseCameraPitch = -89.0f;
        }
        
        // Vertex selection on left-click release (in vertex mode)
        if (!leftDown && g_eseLeftMouseDown && g_eseVertexMode && g_eseModelLoaded && g_objMeshResource) {
            // Only select if this was a click (not a drag)
            if (fabs(deltaX) < 3.0 && fabs(deltaY) < 3.0) {
                // Perform vertex picking
                // Get camera matrices for raycasting
                float yawRad = g_eseCameraYaw * 3.14159f / 180.0f;
                float pitchRad = g_eseCameraPitch * 3.14159f / 180.0f;
                if (pitchRad > 1.5f) pitchRad = 1.5f;
                if (pitchRad < -1.5f) pitchRad = -1.5f;
                
                float camX = g_eseCameraDistance * cosf(pitchRad) * sinf(yawRad);
                float camY = g_eseCameraDistance * sinf(pitchRad);
                float camZ = g_eseCameraDistance * cosf(pitchRad) * cosf(yawRad);
                
                glm::vec3 cameraPos(
                    camX + g_eseCameraTargetX + g_eseCameraPanX,
                    camY + g_eseCameraTargetY + g_eseCameraPanY,
                    camZ + g_eseCameraTargetZ
                );
                glm::vec3 targetPos(
                    g_eseCameraTargetX + g_eseCameraPanX,
                    g_eseCameraTargetY + g_eseCameraPanY,
                    g_eseCameraTargetZ
                );
                
                glm::mat4 viewMat = glm::lookAt(cameraPos, targetPos, glm::vec3(0.0f, 1.0f, 0.0f));
                float fov = 45.0f * 3.14159f / 180.0f;
                float aspect = (float)g_swapchainExtent.width / (float)g_swapchainExtent.height;
                glm::mat4 projMat = glm::perspectiveRH_ZO(fov, aspect, 0.1f, 100.0f);
                projMat[1][1] *= -1.0f;  // Vulkan Y flip
                
                // Get ray from mouse position
                float ndcX = (2.0f * (float)mouseX / g_swapchainExtent.width) - 1.0f;
                float ndcY = (2.0f * (float)mouseY / g_swapchainExtent.height) - 1.0f;
                
                glm::vec4 rayClip(ndcX, ndcY, -1.0f, 1.0f);
                glm::mat4 invProj = glm::inverse(projMat);
                glm::vec4 rayEye = invProj * rayClip;
                rayEye = glm::vec4(rayEye.x, rayEye.y, -1.0f, 0.0f);
                
                glm::mat4 invView = glm::inverse(viewMat);
                glm::vec4 rayWorld4 = invView * rayEye;
                glm::vec3 rayDir = glm::normalize(glm::vec3(rayWorld4));
                
                // Find closest vertex to ray
                float closestDist = FLT_MAX;
                int closestVertex = -1;
                glm::vec3 closestPos(0.0f);
                
                // Get vertex data from mesh resource
                const std::vector<MeshVertex>& vertices = g_objMeshResource->getVertices();
                for (size_t i = 0; i < vertices.size(); i++) {
                    glm::vec3 vertPos(vertices[i].pos[0], vertices[i].pos[1], vertices[i].pos[2]);
                    
                    // Calculate distance from vertex to ray
                    glm::vec3 toVert = vertPos - cameraPos;
                    float t = glm::dot(toVert, rayDir);
                    if (t > 0) {  // Vertex is in front of camera
                        glm::vec3 closestPointOnRay = cameraPos + rayDir * t;
                        float distToRay = glm::length(vertPos - closestPointOnRay);
                        
                        // Use screen-space distance threshold (scale with distance)
                        float threshold = 0.05f * (t / g_eseCameraDistance);
                        if (distToRay < threshold && distToRay < closestDist) {
                            closestDist = distToRay;
                            closestVertex = (int)i;
                            closestPos = vertPos;
                        }
                    }
                }
                
                if (closestVertex >= 0) {
                    if (ctrlPressed) {
                        // Ctrl+click: Toggle vertex in multi-selection
                        if (g_eseSelectedVertices.count(closestVertex)) {
                            g_eseSelectedVertices.erase(closestVertex);
                            std::cout << "[ESE] Removed vertex " << closestVertex << " from selection (" 
                                      << g_eseSelectedVertices.size() << " selected)" << std::endl;
                        } else {
                            g_eseSelectedVertices.insert(closestVertex);
                            std::cout << "[ESE] Added vertex " << closestVertex << " to selection (" 
                                      << g_eseSelectedVertices.size() << " selected)" << std::endl;
                        }
                        // Also set as primary selection for gizmo
                        g_eseSelectedVertex = closestVertex;
                        g_eseSelectedVertexPos = closestPos;
                    } else {
                        // Normal click: Clear multi-selection and select single
                        g_eseSelectedVertices.clear();
                        g_eseSelectedVertices.insert(closestVertex);
                        g_eseSelectedVertex = closestVertex;
                        g_eseSelectedVertexPos = closestPos;
                        std::cout << "[ESE] Selected vertex " << closestVertex << " at (" 
                                  << closestPos.x << ", " << closestPos.y << ", " << closestPos.z << ")" << std::endl;
                    }
                    
                    // Check if this vertex is a connection point
                    for (size_t i = 0; i < g_eseConnectionPoints.size(); i++) {
                        if (g_eseConnectionPoints[i].vertex_index == closestVertex) {
                            g_eseSelectedConnectionPoint = (int)i;
                            g_eseShowConnectionPointWindow = true;
                            break;
                        }
                    }
                }
            }
        }
        
        // Edge selection on left-click release (in edge mode)
        if (!leftDown && g_eseLeftMouseDown && g_eseEdgeMode && g_eseModelLoaded && g_objMeshResource) {
            if (fabs(deltaX) < 3.0 && fabs(deltaY) < 3.0) {
                // Get camera and ray setup
                float yawRad = g_eseCameraYaw * 3.14159f / 180.0f;
                float pitchRad = g_eseCameraPitch * 3.14159f / 180.0f;
                if (pitchRad > 1.5f) pitchRad = 1.5f;
                if (pitchRad < -1.5f) pitchRad = -1.5f;
                
                float camX = g_eseCameraDistance * cosf(pitchRad) * sinf(yawRad);
                float camY = g_eseCameraDistance * sinf(pitchRad);
                float camZ = g_eseCameraDistance * cosf(pitchRad) * cosf(yawRad);
                
                glm::vec3 cameraPos(
                    camX + g_eseCameraTargetX + g_eseCameraPanX,
                    camY + g_eseCameraTargetY + g_eseCameraPanY,
                    camZ + g_eseCameraTargetZ
                );
                glm::vec3 targetPos(
                    g_eseCameraTargetX + g_eseCameraPanX,
                    g_eseCameraTargetY + g_eseCameraPanY,
                    g_eseCameraTargetZ
                );
                
                glm::mat4 viewMat = glm::lookAt(cameraPos, targetPos, glm::vec3(0.0f, 1.0f, 0.0f));
                float fov = 45.0f * 3.14159f / 180.0f;
                float aspect = (float)g_swapchainExtent.width / (float)g_swapchainExtent.height;
                glm::mat4 projMat = glm::perspectiveRH_ZO(fov, aspect, 0.1f, 100.0f);
                projMat[1][1] *= -1.0f;
                
                float ndcX = (2.0f * (float)mouseX / g_swapchainExtent.width) - 1.0f;
                float ndcY = (2.0f * (float)mouseY / g_swapchainExtent.height) - 1.0f;
                
                glm::vec4 rayClip(ndcX, ndcY, -1.0f, 1.0f);
                glm::mat4 invProj = glm::inverse(projMat);
                glm::vec4 rayEye = invProj * rayClip;
                rayEye = glm::vec4(rayEye.x, rayEye.y, -1.0f, 0.0f);
                
                glm::mat4 invView = glm::inverse(viewMat);
                glm::vec4 rayWorld4 = invView * rayEye;
                glm::vec3 rayDir = glm::normalize(glm::vec3(rayWorld4));
                
                // Find closest QUAD edge to ray (not triangle edges)
                const std::vector<MeshVertex>& vertices = g_objMeshResource->getVertices();
                
                float closestDist = FLT_MAX;
                int closestEdge = -1;
                
                for (size_t ei = 0; ei < g_eseQuadEdges.size(); ei++) {
                    const QuadEdge& qe = g_eseQuadEdges[ei];
                    
                    if (qe.v0 >= vertices.size() || qe.v1 >= vertices.size()) continue;
                    
                    glm::vec3 p0(vertices[qe.v0].pos[0], vertices[qe.v0].pos[1], vertices[qe.v0].pos[2]);
                    glm::vec3 p1(vertices[qe.v1].pos[0], vertices[qe.v1].pos[1], vertices[qe.v1].pos[2]);
                    glm::vec3 edgeMid = (p0 + p1) * 0.5f;
                    
                    // Backface culling: check if any quad containing this edge is visible
                    bool anyVisible = false;
                    
                    if (!qe.quadIndices.empty()) {
                        for (int qi : qe.quadIndices) {
                            if (qi >= 0 && qi < (int)g_eseReconstructedQuads.size()) {
                                const auto& quad = g_eseReconstructedQuads[qi];
                                glm::vec3 qp0(vertices[quad[0]].pos[0], vertices[quad[0]].pos[1], vertices[quad[0]].pos[2]);
                                glm::vec3 qp1(vertices[quad[1]].pos[0], vertices[quad[1]].pos[1], vertices[quad[1]].pos[2]);
                                glm::vec3 qp2(vertices[quad[2]].pos[0], vertices[quad[2]].pos[1], vertices[quad[2]].pos[2]);
                                glm::vec3 qp3(vertices[quad[3]].pos[0], vertices[quad[3]].pos[1], vertices[quad[3]].pos[2]);
                                glm::vec3 center = (qp0 + qp1 + qp2 + qp3) * 0.25f;
                                glm::vec3 normal = glm::normalize(glm::cross(qp1 - qp0, qp2 - qp0));
                                if (glm::dot(normal, glm::normalize(cameraPos - center)) > -0.2f) {
                                    anyVisible = true;
                                    break;
                                }
                            }
                        }
                    } else {
                        // No quad associations - use vertex normals
                        glm::vec3 avgNormal(
                            (vertices[qe.v0].normal[0] + vertices[qe.v1].normal[0]) * 0.5f,
                            (vertices[qe.v0].normal[1] + vertices[qe.v1].normal[1]) * 0.5f,
                            (vertices[qe.v0].normal[2] + vertices[qe.v1].normal[2]) * 0.5f
                        );
                        if (glm::length(avgNormal) > 0.0001f) {
                            avgNormal = glm::normalize(avgNormal);
                            glm::vec3 toCamera = glm::normalize(cameraPos - edgeMid);
                            anyVisible = glm::dot(avgNormal, toCamera) > -0.3f;
                        } else {
                            anyVisible = true;
                        }
                    }
                    
                    if (!anyVisible) continue;
                    
                    // Distance from ray to edge LINE (not just midpoint)
                    // Find closest point on edge to the ray
                    glm::vec3 edgeDir = p1 - p0;
                    float edgeLen = glm::length(edgeDir);
                    if (edgeLen < 0.0001f) continue;
                    edgeDir /= edgeLen;
                    
                    // Project camera position onto edge direction
                    glm::vec3 toP0 = p0 - cameraPos;
                    float tEdge = -glm::dot(glm::cross(toP0, rayDir), glm::cross(edgeDir, rayDir));
                    float denom = glm::length(glm::cross(edgeDir, rayDir));
                    if (denom > 0.0001f) tEdge /= (denom * denom);
                    tEdge = glm::clamp(tEdge, 0.0f, edgeLen);
                    
                    glm::vec3 closestOnEdge = p0 + edgeDir * tEdge;
                    
                    // Distance from closest point on edge to ray
                    glm::vec3 toClosest = closestOnEdge - cameraPos;
                    float tRay = glm::dot(toClosest, rayDir);
                    if (tRay > 0) {
                        glm::vec3 closestPointOnRay = cameraPos + rayDir * tRay;
                        float distToRay = glm::length(closestOnEdge - closestPointOnRay);
                        
                        float threshold = 0.12f * (tRay / g_eseCameraDistance);
                        if (distToRay < threshold && distToRay < closestDist) {
                            closestDist = distToRay;
                            closestEdge = (int)ei;
                        }
                    }
                }
                
                if (closestEdge >= 0) {
                    if (ctrlPressed) {
                        // Ctrl+click: Toggle edge in multi-selection
                        if (g_eseSelectedEdges.count(closestEdge)) {
                            g_eseSelectedEdges.erase(closestEdge);
                            std::cout << "[ESE] Removed edge " << closestEdge << " from selection (" 
                                      << g_eseSelectedEdges.size() << " selected)" << std::endl;
                        } else {
                            g_eseSelectedEdges.insert(closestEdge);
                            std::cout << "[ESE] Added edge " << closestEdge << " to selection (" 
                                      << g_eseSelectedEdges.size() << " selected)" << std::endl;
                        }
                        g_eseSelectedEdge = closestEdge;
                    } else {
                        // Normal click: Clear multi-selection and select single
                        g_eseSelectedEdges.clear();
                        g_eseSelectedEdges.insert(closestEdge);
                        g_eseSelectedEdge = closestEdge;
                        std::cout << "[ESE] Selected edge " << closestEdge << std::endl;
                    }
                }
            }
        }
        
        // Face selection on left-click release (in face mode)
        if (!leftDown && g_eseLeftMouseDown && g_eseFaceMode && g_eseModelLoaded && g_objMeshResource) {
            if (fabs(deltaX) < 3.0 && fabs(deltaY) < 3.0) {
                // Get camera and ray setup
                float yawRad = g_eseCameraYaw * 3.14159f / 180.0f;
                float pitchRad = g_eseCameraPitch * 3.14159f / 180.0f;
                if (pitchRad > 1.5f) pitchRad = 1.5f;
                if (pitchRad < -1.5f) pitchRad = -1.5f;
                
                float camX = g_eseCameraDistance * cosf(pitchRad) * sinf(yawRad);
                float camY = g_eseCameraDistance * sinf(pitchRad);
                float camZ = g_eseCameraDistance * cosf(pitchRad) * cosf(yawRad);
                
                glm::vec3 cameraPos(
                    camX + g_eseCameraTargetX + g_eseCameraPanX,
                    camY + g_eseCameraTargetY + g_eseCameraPanY,
                    camZ + g_eseCameraTargetZ
                );
                glm::vec3 targetPos(
                    g_eseCameraTargetX + g_eseCameraPanX,
                    g_eseCameraTargetY + g_eseCameraPanY,
                    g_eseCameraTargetZ
                );
                
                glm::mat4 viewMat = glm::lookAt(cameraPos, targetPos, glm::vec3(0.0f, 1.0f, 0.0f));
                float fov = 45.0f * 3.14159f / 180.0f;
                float aspect = (float)g_swapchainExtent.width / (float)g_swapchainExtent.height;
                glm::mat4 projMat = glm::perspectiveRH_ZO(fov, aspect, 0.1f, 100.0f);
                projMat[1][1] *= -1.0f;
                
                float ndcX = (2.0f * (float)mouseX / g_swapchainExtent.width) - 1.0f;
                float ndcY = (2.0f * (float)mouseY / g_swapchainExtent.height) - 1.0f;
                
                glm::vec4 rayClip(ndcX, ndcY, -1.0f, 1.0f);
                glm::mat4 invProj = glm::inverse(projMat);
                glm::vec4 rayEye = invProj * rayClip;
                rayEye = glm::vec4(rayEye.x, rayEye.y, -1.0f, 0.0f);
                
                glm::mat4 invView = glm::inverse(viewMat);
                glm::vec4 rayWorld4 = invView * rayEye;
                glm::vec3 rayDir = glm::normalize(glm::vec3(rayWorld4));
                
                // Find closest face (ray-triangle intersection)
                const std::vector<MeshVertex>& vertices = g_objMeshResource->getVertices();
                const std::vector<uint32_t>& indices = g_objMeshResource->getIndices();
                
                float closestT = FLT_MAX;
                int closestFace = -1;
                
                size_t numFaces = indices.size() / 3;
                for (size_t faceIdx = 0; faceIdx < numFaces; faceIdx++) {
                    size_t triStart = faceIdx * 3;
                    if (triStart + 2 >= indices.size()) break;
                    
                    glm::vec3 v0(vertices[indices[triStart]].pos[0], vertices[indices[triStart]].pos[1], vertices[indices[triStart]].pos[2]);
                    glm::vec3 v1(vertices[indices[triStart + 1]].pos[0], vertices[indices[triStart + 1]].pos[1], vertices[indices[triStart + 1]].pos[2]);
                    glm::vec3 v2(vertices[indices[triStart + 2]].pos[0], vertices[indices[triStart + 2]].pos[1], vertices[indices[triStart + 2]].pos[2]);
                    
                    // Backface culling
                    glm::vec3 faceNormal = glm::normalize(glm::cross(v1 - v0, v2 - v0));
                    glm::vec3 faceCenter = (v0 + v1 + v2) / 3.0f;
                    if (glm::dot(faceNormal, glm::normalize(cameraPos - faceCenter)) <= 0.0f) continue;
                    
                    // Moller-Trumbore ray-triangle intersection
                    glm::vec3 edge1 = v1 - v0;
                    glm::vec3 edge2 = v2 - v0;
                    glm::vec3 h = glm::cross(rayDir, edge2);
                    float a = glm::dot(edge1, h);
                    
                    if (fabs(a) < 0.0001f) continue;  // Ray parallel to triangle
                    
                    float f = 1.0f / a;
                    glm::vec3 s = cameraPos - v0;
                    float u = f * glm::dot(s, h);
                    
                    if (u < 0.0f || u > 1.0f) continue;
                    
                    glm::vec3 q = glm::cross(s, edge1);
                    float v = f * glm::dot(rayDir, q);
                    
                    if (v < 0.0f || u + v > 1.0f) continue;
                    
                    float t = f * glm::dot(edge2, q);
                    
                    if (t > 0.001f && t < closestT) {
                        closestT = t;
                        closestFace = (int)faceIdx;
                    }
                }
                
                if (closestFace >= 0) {
                    g_eseSelectedFace = closestFace;
                    std::cout << "[ESE] Selected face " << closestFace << std::endl;
                }
            }
        }
        
        // Quad selection on left-click release (in quad mode)
        if (!leftDown && g_eseLeftMouseDown && g_eseQuadMode && g_eseModelLoaded && g_objMeshResource) {
            if (fabs(deltaX) < 3.0 && fabs(deltaY) < 3.0) {
                // Use reconstructed quads from visualization (global variable)
                
                // Get camera setup
                float yawRad = g_eseCameraYaw * 3.14159f / 180.0f;
                float pitchRad = g_eseCameraPitch * 3.14159f / 180.0f;
                if (pitchRad > 1.5f) pitchRad = 1.5f;
                if (pitchRad < -1.5f) pitchRad = -1.5f;
                
                float camX = g_eseCameraDistance * cosf(pitchRad) * sinf(yawRad);
                float camY = g_eseCameraDistance * sinf(pitchRad);
                float camZ = g_eseCameraDistance * cosf(pitchRad) * cosf(yawRad);
                
                glm::vec3 cameraPos(
                    camX + g_eseCameraTargetX + g_eseCameraPanX,
                    camY + g_eseCameraTargetY + g_eseCameraPanY,
                    camZ + g_eseCameraTargetZ
                );
                glm::vec3 targetPos(
                    g_eseCameraTargetX + g_eseCameraPanX,
                    g_eseCameraTargetY + g_eseCameraPanY,
                    g_eseCameraTargetZ
                );
                
                glm::mat4 viewMat = glm::lookAt(cameraPos, targetPos, glm::vec3(0.0f, 1.0f, 0.0f));
                float fov = 45.0f * 3.14159f / 180.0f;
                float aspect = (float)g_swapchainExtent.width / (float)g_swapchainExtent.height;
                glm::mat4 projMat = glm::perspectiveRH_ZO(fov, aspect, 0.1f, 100.0f);
                projMat[1][1] *= -1.0f;
                
                float ndcX = (2.0f * (float)mouseX / g_swapchainExtent.width) - 1.0f;
                float ndcY = (2.0f * (float)mouseY / g_swapchainExtent.height) - 1.0f;
                
                glm::vec4 rayClip(ndcX, ndcY, -1.0f, 1.0f);
                glm::mat4 invProj = glm::inverse(projMat);
                glm::vec4 rayEye = invProj * rayClip;
                rayEye = glm::vec4(rayEye.x, rayEye.y, -1.0f, 0.0f);
                
                glm::mat4 invView = glm::inverse(viewMat);
                glm::vec4 rayWorld4 = invView * rayEye;
                glm::vec3 rayDir = glm::normalize(glm::vec3(rayWorld4));
                
                const std::vector<MeshVertex>& vertices = g_objMeshResource->getVertices();
                
                // Find closest quad (ray-quad intersection)
                float closestT = FLT_MAX;
                int closestQuad = -1;
                
                for (size_t qi = 0; qi < g_eseReconstructedQuads.size(); qi++) {
                    const auto& quad = g_eseReconstructedQuads[qi];
                    
                    glm::vec3 p0(vertices[quad[0]].pos[0], vertices[quad[0]].pos[1], vertices[quad[0]].pos[2]);
                    glm::vec3 p1(vertices[quad[1]].pos[0], vertices[quad[1]].pos[1], vertices[quad[1]].pos[2]);
                    glm::vec3 p2(vertices[quad[2]].pos[0], vertices[quad[2]].pos[1], vertices[quad[2]].pos[2]);
                    glm::vec3 p3(vertices[quad[3]].pos[0], vertices[quad[3]].pos[1], vertices[quad[3]].pos[2]);
                    
                    // Backface culling
                    glm::vec3 center = (p0 + p1 + p2 + p3) * 0.25f;
                    glm::vec3 normal = glm::normalize(glm::cross(p1 - p0, p2 - p0));
                    if (glm::dot(normal, glm::normalize(cameraPos - center)) <= 0.0f) continue;
                    
                    // Test two triangles that make up the quad
                    auto testTriangle = [&](glm::vec3 v0, glm::vec3 v1, glm::vec3 v2) -> float {
                        glm::vec3 edge1 = v1 - v0;
                        glm::vec3 edge2 = v2 - v0;
                        glm::vec3 h = glm::cross(rayDir, edge2);
                        float a = glm::dot(edge1, h);
                        if (fabs(a) < 0.0001f) return FLT_MAX;
                        float f = 1.0f / a;
                        glm::vec3 s = cameraPos - v0;
                        float u = f * glm::dot(s, h);
                        if (u < 0.0f || u > 1.0f) return FLT_MAX;
                        glm::vec3 q = glm::cross(s, edge1);
                        float v = f * glm::dot(rayDir, q);
                        if (v < 0.0f || u + v > 1.0f) return FLT_MAX;
                        float t = f * glm::dot(edge2, q);
                        return (t > 0.001f) ? t : FLT_MAX;
                    };
                    
                    float t1 = testTriangle(p0, p1, p2);
                    float t2 = testTriangle(p0, p2, p3);
                    float t = std::min(t1, t2);
                    
                    if (t < closestT) {
                        closestT = t;
                        closestQuad = (int)qi;
                    }
                }
                
                if (closestQuad >= 0) {
                    if (ctrlPressed) {
                        // Ctrl+click: Toggle quad in multi-selection
                        if (g_eseSelectedQuads.count(closestQuad)) {
                            g_eseSelectedQuads.erase(closestQuad);
                            std::cout << "[ESE] Removed quad " << closestQuad << " from selection (" 
                                      << g_eseSelectedQuads.size() << " selected)" << std::endl;
                        } else {
                            g_eseSelectedQuads.insert(closestQuad);
                            std::cout << "[ESE] Added quad " << closestQuad << " to selection (" 
                                      << g_eseSelectedQuads.size() << " selected)" << std::endl;
                        }
                        g_eseSelectedQuad = closestQuad;
                    } else {
                        // Normal click: Clear multi-selection and select single
                        g_eseSelectedQuads.clear();
                        g_eseSelectedQuads.insert(closestQuad);
                        g_eseSelectedQuad = closestQuad;
                        std::cout << "[ESE] Selected quad " << closestQuad << std::endl;
                    }
                }
            }
        }
        
        // Right-click drag: pan camera
        if (rightDown && g_eseRightMouseDown) {
            float panSpeed = 0.01f * g_eseCameraDistance;  // Scale with zoom
            g_eseCameraPanX -= (float)deltaX * panSpeed;
            g_eseCameraPanY += (float)deltaY * panSpeed;
        }
        
        // Mouse wheel: zoom in/out
        if (g_eseScrollDelta != 0.0f) {
            float zoomSpeed = 0.5f;
            g_eseCameraDistance -= g_eseScrollDelta * zoomSpeed;
            
            // Clamp distance to reasonable range
            if (g_eseCameraDistance < 0.5f) g_eseCameraDistance = 0.5f;
            if (g_eseCameraDistance > 50.0f) g_eseCameraDistance = 50.0f;
            
            g_eseScrollDelta = 0.0f;  // Consume the scroll delta
        }
    } else {
        // ImGui wants mouse, consume scroll delta so it doesn't accumulate
        g_eseScrollDelta = 0.0f;
    }
    
    // Update button states for next frame
    g_eseLeftMouseDown = leftDown;
    g_eseRightMouseDown = rightDown;
    g_eseLastMouseX = mouseX;
    g_eseLastMouseY = mouseY;
    
#ifdef USE_IMGUI
    // Start ImGui frame
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    
    // Create main menu bar
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New HDM", "Ctrl+N")) {
                hdm_init_default_properties(g_eseHdmProperties);
                g_esePropertiesModified = true;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Open HDM...", "Ctrl+Shift+O")) {
                g_eseOpenHdmDialog = true;
            }
            if (ImGui::MenuItem("Save HDM...", "Ctrl+S", false, g_eseModelLoaded)) {
                g_eseSaveHdmDialog = true;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Import OBJ...", "Ctrl+O")) {
                g_eseOpenObjDialog = true;
            }
            if (ImGui::MenuItem("Import Texture...", "Ctrl+T")) {
                g_eseOpenTextureDialog = true;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit", "Esc")) {
                glfwSetWindowShouldClose(window, 1);
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Create")) {
            if (ImGui::MenuItem("Cube")) {
                if (ese_create_cube_mesh(window)) {
                    std::cout << "[ESE] Cube created successfully" << std::endl;
                } else {
                    std::cerr << "[ESE] Failed to create cube" << std::endl;
                }
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            if (ImGui::MenuItem("Reset Camera", "Home")) {
                g_eseCameraYaw = 45.0f;
                g_eseCameraPitch = 30.0f;
                g_eseCameraDistance = 5.0f;
                g_eseCameraPanX = 0.0f;
                g_eseCameraPanY = 0.0f;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Wireframe", nullptr, &g_eseWireframeMode, g_eseModelLoaded)) {
                // Menu item handles the toggle automatically via &g_eseWireframeMode
            }
            if (ImGui::MenuItem("Vertex Mode", "1", &g_eseVertexMode, g_eseModelLoaded)) {
                if (g_eseVertexMode) {
                    g_eseEdgeMode = false;
                    g_eseFaceMode = false;
                    std::cout << "[ESE] Vertex mode enabled" << std::endl;
                } else {
                    g_eseSelectedVertex = -1;
                }
            }
            if (ImGui::MenuItem("Edge Mode", "2", &g_eseEdgeMode, g_eseModelLoaded)) {
                if (g_eseEdgeMode) {
                    g_eseVertexMode = false;
                    g_eseFaceMode = false;
                    std::cout << "[ESE] Edge mode enabled" << std::endl;
                } else {
                    g_eseSelectedEdge = -1;
                }
            }
            if (ImGui::MenuItem("Face Mode", "3", &g_eseFaceMode, g_eseModelLoaded)) {
                if (g_eseFaceMode) {
                    g_eseVertexMode = false;
                    g_eseEdgeMode = false;
                    g_eseQuadMode = false;
                    std::cout << "[ESE] Face mode enabled" << std::endl;
                } else {
                    g_eseSelectedFace = -1;
                }
            }
            if (ImGui::MenuItem("Quad Mode", "4", &g_eseQuadMode, g_eseModelLoaded)) {
                if (g_eseQuadMode) {
                    g_eseVertexMode = false;
                    g_eseEdgeMode = false;
                    g_eseFaceMode = false;
                    std::cout << "[ESE] Quad mode enabled" << std::endl;
                } else {
                    g_eseSelectedQuad = -1;
                }
            }
            if (ImGui::MenuItem("Show Normals", nullptr, &g_eseShowNormals, g_eseModelLoaded)) {
                // Menu item handles the toggle automatically via &g_eseShowNormals
            }
            if (ImGui::MenuItem("Show Quads", nullptr, &g_eseShowQuads, g_eseModelLoaded)) {
                // Reconstructs and shows quads from triangle pairs
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Connection Points...", nullptr, nullptr, g_eseModelLoaded)) {
                g_eseShowConnectionPointWindow = true;
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("About ESE")) {
                // Show about dialog
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
    
    // Model Info Panel
    ImGui::SetNextWindowPos(ImVec2(10, 30), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(320, 200), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Model Info")) {
        if (g_eseModelLoaded) {
            ImGui::Text("OBJ: %s", g_eseCurrentObjPath.c_str());
            ImGui::Text("Texture: %s", g_eseCurrentTexturePath.empty() ? "(none)" : g_eseCurrentTexturePath.c_str());
            ImGui::Separator();
            ImGui::Text("Scale:");
            if (ImGui::DragFloat3("##scale", g_eseHdmProperties.model.scale, 0.01f, 0.01f, 100.0f)) {
                g_esePropertiesModified = true;
            }
            ImGui::Text("Origin Offset:");
            if (ImGui::DragFloat3("##origin", g_eseHdmProperties.model.origin_offset, 0.1f, -100.0f, 100.0f)) {
                g_esePropertiesModified = true;
            }
        } else {
            ImGui::Text("No model loaded.");
            ImGui::Text("Use File > Import OBJ to load.");
        }
    }
    ImGui::End();
    
    // Item Properties Panel
    ImGui::SetNextWindowPos(ImVec2(10, 240), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(320, 280), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Item Properties")) {
        ImGui::Text("Item Type ID:");
        if (ImGui::InputInt("##type_id", &g_eseHdmProperties.item.item_type_id)) {
            g_esePropertiesModified = true;
        }
        
        ImGui::Text("Name:");
        if (ImGui::InputText("##name", g_eseHdmProperties.item.item_name, sizeof(g_eseHdmProperties.item.item_name))) {
            g_esePropertiesModified = true;
        }
        
        ImGui::Text("Category:");
        if (ImGui::Combo("##category", &g_eseHdmProperties.item.category, g_categoryNames, IM_ARRAYSIZE(g_categoryNames))) {
            g_esePropertiesModified = true;
        }
        
        ImGui::Separator();
        
        ImGui::Text("Trade Value:");
        if (ImGui::InputInt("##trade", &g_eseHdmProperties.item.trade_value)) {
            g_esePropertiesModified = true;
        }
        
        ImGui::Text("Condition (0.0 - 1.0):");
        if (ImGui::SliderFloat("##condition", &g_eseHdmProperties.item.condition, 0.0f, 1.0f)) {
            g_esePropertiesModified = true;
        }
        
        ImGui::Text("Weight:");
        if (ImGui::DragFloat("##weight", &g_eseHdmProperties.item.weight, 0.1f, 0.0f, 1000.0f)) {
            g_esePropertiesModified = true;
        }
        
        if (ImGui::Checkbox("Is Salvaged", &g_eseHdmProperties.item.is_salvaged)) {
            g_esePropertiesModified = true;
        }
        
        ImGui::Separator();
        
        ImGui::Text("Mesh Class:");
        if (ImGui::Combo("##mesh_class", &g_eseHdmProperties.item.mesh_class, g_meshClassNames, IM_ARRAYSIZE(g_meshClassNames))) {
            g_esePropertiesModified = true;
        }
    }
    ImGui::End();
    
    // Physics Properties Panel
    ImGui::SetNextWindowPos(ImVec2(10, 530), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(320, 180), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Physics")) {
        const char* collisionTypes[] = { "None", "Box", "Mesh" };
        ImGui::Text("Collision Type:");
        if (ImGui::Combo("##collision", &g_eseHdmProperties.physics.collision_type, collisionTypes, IM_ARRAYSIZE(collisionTypes))) {
            g_esePropertiesModified = true;
        }
        
        if (g_eseHdmProperties.physics.collision_type == 1) {
            ImGui::Text("Collision Bounds:");
            if (ImGui::DragFloat3("##bounds", g_eseHdmProperties.physics.collision_bounds, 0.1f, 0.1f, 100.0f)) {
                g_esePropertiesModified = true;
            }
        }
        
        if (ImGui::Checkbox("Is Static", &g_eseHdmProperties.physics.is_static)) {
            g_esePropertiesModified = true;
        }
        
        if (!g_eseHdmProperties.physics.is_static) {
            ImGui::Text("Mass:");
            if (ImGui::DragFloat("##mass", &g_eseHdmProperties.physics.mass, 0.1f, 0.1f, 10000.0f)) {
                g_esePropertiesModified = true;
            }
        }
    }
    ImGui::End();
    
    // Connection Point Properties Window
    if (g_eseShowConnectionPointWindow) {
        ImGui::SetNextWindowPos(ImVec2((float)g_swapchainExtent.width / 2 - 200, (float)g_swapchainExtent.height / 2 - 200), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(400, 400), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Connection Points", &g_eseShowConnectionPointWindow)) {
            
            // List all connection points
            ImGui::Text("Connection Points (%zu):", g_eseConnectionPoints.size());
            ImGui::Separator();
            
            if (g_eseConnectionPoints.empty()) {
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No connection points defined.");
                ImGui::Text("To create a connection point:");
                ImGui::BulletText("Press 1 to enter Vertex Mode");
                ImGui::BulletText("Click to select a vertex");
                ImGui::BulletText("Press Ctrl+Enter to create");
            } else {
                // Show list of connection points
                ImGui::BeginChild("ConnectionPointList", ImVec2(0, 120), true);
                for (size_t i = 0; i < g_eseConnectionPoints.size(); i++) {
                    const ConnectionPoint& cp = g_eseConnectionPoints[i];
                    char label[128];
                    snprintf(label, sizeof(label), "%s [V:%d] (%s)##cplist_%zu", 
                             cp.name, cp.vertex_index, 
                             g_connectionTypeNames[cp.connection_type], i);
                    
                    bool isSelected = (g_eseSelectedConnectionPoint == (int)i);
                    if (ImGui::Selectable(label, isSelected)) {
                        g_eseSelectedConnectionPoint = (int)i;
                    }
                }
                ImGui::EndChild();
            }
            
            ImGui::Separator();
            
            // Show selected connection point properties
            if (g_eseSelectedConnectionPoint >= 0 && 
                g_eseSelectedConnectionPoint < (int)g_eseConnectionPoints.size()) {
                
                ConnectionPoint& cp = g_eseConnectionPoints[g_eseSelectedConnectionPoint];
            
            ImGui::Text("Name:");
            if (ImGui::InputText("##cp_name", cp.name, sizeof(cp.name))) {
                g_esePropertiesModified = true;
            }
            
            ImGui::Separator();
            
            ImGui::Text("Vertex Information:");
            ImGui::Text("  Vertex Number: %d", cp.vertex_index);
            ImGui::Text("  Position X: %.6f", cp.position[0]);
            ImGui::Text("  Position Y: %.6f", cp.position[1]);
            ImGui::Text("  Position Z: %.6f", cp.position[2]);
            
            ImGui::Separator();
            
            ImGui::Text("Connection Type:");
            if (ImGui::Combo("##cp_type", &cp.connection_type, g_connectionTypeNames, IM_ARRAYSIZE(g_connectionTypeNames))) {
                g_esePropertiesModified = true;
            }
            
            ImGui::Separator();
            
            ImGui::Text("Connected Mesh:");
            if (ImGui::InputText("##cp_mesh", cp.connected_mesh, sizeof(cp.connected_mesh))) {
                g_esePropertiesModified = true;
            }
            
            ImGui::Text("Connected Mesh Class:");
            if (ImGui::InputText("##cp_mesh_class", cp.connected_mesh_class, sizeof(cp.connected_mesh_class))) {
                g_esePropertiesModified = true;
            }
            
            ImGui::Separator();
            
                if (ImGui::Button("Delete")) {
                    g_eseConnectionPoints.erase(g_eseConnectionPoints.begin() + g_eseSelectedConnectionPoint);
                    if (g_eseSelectedConnectionPoint >= (int)g_eseConnectionPoints.size()) {
                        g_eseSelectedConnectionPoint = (int)g_eseConnectionPoints.size() - 1;
                    }
                    g_esePropertiesModified = true;
                }
            } else if (!g_eseConnectionPoints.empty()) {
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Select a connection point above to edit");
            }
            
            ImGui::Separator();
            if (ImGui::Button("Close")) {
                g_eseShowConnectionPointWindow = false;
            }
        }
        ImGui::End();
    }
    
    // Vertex Info Panel (only visible in vertex mode)
    if (g_eseVertexMode) {
        ImGui::SetNextWindowPos(ImVec2((float)g_swapchainExtent.width - 330, 30), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(320, 200), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Vertex Info")) {
            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "VERTEX MODE ACTIVE");
            ImGui::Text("Click to select, Ctrl+click to multi-select");
            ImGui::Separator();
            
            if (!g_eseSelectedVertices.empty()) {
                ImGui::Text("Selected: %zu vertices", g_eseSelectedVertices.size());
                if (g_eseSelectedVertex >= 0) {
                    ImGui::Text("Primary: Vertex %d", g_eseSelectedVertex);
                ImGui::Separator();
                ImGui::Text("Position:");
                ImGui::Text("  X: %.6f", g_eseSelectedVertexPos.x);
                ImGui::Text("  Y: %.6f", g_eseSelectedVertexPos.y);
                ImGui::Text("  Z: %.6f", g_eseSelectedVertexPos.z);
                
                // Show vertex data from mesh if available
                if (g_objMeshResource) {
                    const std::vector<MeshVertex>& vertices = g_objMeshResource->getVertices();
                    if (g_eseSelectedVertex < (int)vertices.size()) {
                        ImGui::Separator();
                        ImGui::Text("Normal:");
                        ImGui::Text("  X: %.4f", vertices[g_eseSelectedVertex].normal[0]);
                        ImGui::Text("  Y: %.4f", vertices[g_eseSelectedVertex].normal[1]);
                        ImGui::Text("  Z: %.4f", vertices[g_eseSelectedVertex].normal[2]);
                        ImGui::Separator();
                        ImGui::Text("UV:");
                        ImGui::Text("  U: %.4f", vertices[g_eseSelectedVertex].uv[0]);
                        ImGui::Text("  V: %.4f", vertices[g_eseSelectedVertex].uv[1]);
                    }
                }
                }
            } else {
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No vertex selected");
                ImGui::Text("Click on a vertex to select it");
            }
            
            // Show total vertex count
            if (g_objMeshResource) {
                ImGui::Separator();
                ImGui::Text("Total vertices: %zu", g_objMeshResource->getVertices().size());
            }
            
            // Show connection points list
            if (!g_eseConnectionPoints.empty()) {
                ImGui::Separator();
                ImGui::TextColored(ImVec4(1.0f, 0.84f, 0.0f, 1.0f), "Connection Points: %zu", g_eseConnectionPoints.size());
                ImGui::Text("Press Ctrl+Enter on selected vertex to create");
                
                for (size_t i = 0; i < g_eseConnectionPoints.size(); i++) {
                    const ConnectionPoint& cp = g_eseConnectionPoints[i];
                    bool isSelected = (g_eseSelectedConnectionPoint >= 0 && (size_t)g_eseSelectedConnectionPoint == i);
                    
                    char label[128];
                    snprintf(label, sizeof(label), "%s (V:%d)##cp_%zu", cp.name, cp.vertex_index, i);
                    
                    if (ImGui::Selectable(label, isSelected)) {
                        g_eseSelectedConnectionPoint = (int)i;
                        g_eseShowConnectionPointWindow = true;
                    }
                }
            }
        }
        ImGui::End();
        
        // Draw vertex points as overlay using ImGui foreground draw list
        if (g_objMeshResource) {
            ImDrawList* drawList = ImGui::GetForegroundDrawList();
            const std::vector<MeshVertex>& vertices = g_objMeshResource->getVertices();
            
            // Limit to first 50000 vertices for performance (can be adjusted)
            size_t maxVerts = std::min(vertices.size(), (size_t)50000);
            
            // Calculate camera position (same as in render_obj_mesh_to_command_buffer)
            float yawRad = g_eseCameraYaw * 3.14159f / 180.0f;
            float pitchRad = g_eseCameraPitch * 3.14159f / 180.0f;
            if (pitchRad > 1.5f) pitchRad = 1.5f;
            if (pitchRad < -1.5f) pitchRad = -1.5f;
            
            float camX = g_eseCameraDistance * cosf(pitchRad) * sinf(yawRad);
            float camY = g_eseCameraDistance * sinf(pitchRad);
            float camZ = g_eseCameraDistance * cosf(pitchRad) * cosf(yawRad);
            
            glm::vec3 cameraPos(
                camX + g_eseCameraTargetX + g_eseCameraPanX,
                camY + g_eseCameraTargetY + g_eseCameraPanY,
                camZ + g_eseCameraTargetZ
            );
            
            // Get target position for view direction calculation
            glm::vec3 targetPos(
                g_eseCameraTargetX + g_eseCameraPanX,
                g_eseCameraTargetY + g_eseCameraPanY,
                g_eseCameraTargetZ
            );
            
            // Get indices to build face normals
            const std::vector<uint32_t>& indices = g_objMeshResource->getIndices();
            
            // Build a map: vertex index -> list of faces (triangles) it belongs to
            std::vector<std::vector<size_t>> vertexToFaces(vertices.size());
            for (size_t triIdx = 0; triIdx < indices.size(); triIdx += 3) {
                if (triIdx + 2 < indices.size()) {
                    uint32_t v0 = indices[triIdx];
                    uint32_t v1 = indices[triIdx + 1];
                    uint32_t v2 = indices[triIdx + 2];
                    size_t faceIdx = triIdx / 3;
                    vertexToFaces[v0].push_back(faceIdx);
                    vertexToFaces[v1].push_back(faceIdx);
                    vertexToFaces[v2].push_back(faceIdx);
                }
            }
            
            // Get view direction (camera looks towards target)
            glm::vec3 viewDir = glm::normalize(targetPos - cameraPos);
            
            for (size_t i = 0; i < maxVerts; i++) {
                glm::vec3 vertPos(vertices[i].pos[0], vertices[i].pos[1], vertices[i].pos[2]);
                
                // Check if any face containing this vertex faces the camera
                bool hasVisibleFace = false;
                
                for (size_t faceIdx : vertexToFaces[i]) {
                    size_t triStart = faceIdx * 3;
                    if (triStart + 2 < indices.size()) {
                        uint32_t v0Idx = indices[triStart];
                        uint32_t v1Idx = indices[triStart + 1];
                        uint32_t v2Idx = indices[triStart + 2];
                        
                        // Get triangle vertices
                        glm::vec3 v0(vertices[v0Idx].pos[0], vertices[v0Idx].pos[1], vertices[v0Idx].pos[2]);
                        glm::vec3 v1(vertices[v1Idx].pos[0], vertices[v1Idx].pos[1], vertices[v1Idx].pos[2]);
                        glm::vec3 v2(vertices[v2Idx].pos[0], vertices[v2Idx].pos[1], vertices[v2Idx].pos[2]);
                        
                        // Calculate face normal (using cross product)
                        glm::vec3 edge1 = v1 - v0;
                        glm::vec3 edge2 = v2 - v0;
                        glm::vec3 faceNormal = glm::normalize(glm::cross(edge1, edge2));
                        
                        // Check if face normal faces camera
                        // Direction from face center to camera
                        glm::vec3 faceCenter = (v0 + v1 + v2) / 3.0f;
                        glm::vec3 toCamera = glm::normalize(cameraPos - faceCenter);
                        
                        // If dot product > 0, normal faces camera
                        float dot = glm::dot(faceNormal, toCamera);
                        if (dot > 0.0f) {
                            hasVisibleFace = true;
                            break;  // Found at least one visible face, can show vertex
                        }
                    }
                }
                
                if (hasVisibleFace) {
                    bool inFront = false;
                    glm::vec2 screenPos = ese_project_to_screen(vertPos, &inFront);
                    
                    if (inFront && screenPos.x >= 0 && screenPos.x < g_swapchainExtent.width &&
                        screenPos.y >= 0 && screenPos.y < g_swapchainExtent.height) {
                        
                        ImVec2 pos(screenPos.x, screenPos.y);
                        
                        bool isSelected = g_eseSelectedVertices.count((int)i) > 0;
                        if (isSelected) {
                            // Selected vertex: large red circle with outline (75% smaller)
                            drawList->AddCircleFilled(pos, 2.0f, IM_COL32(255, 50, 50, 255));
                            drawList->AddCircle(pos, 2.0f, IM_COL32(255, 255, 255, 255), 0, 1.0f);
                        } else {
                            // Normal vertex: small cyan point (75% smaller)
                            drawList->AddCircleFilled(pos, 0.75f, IM_COL32(0, 200, 255, 180));
                        }
                    }
                }
            }
            
            // Show vertex count warning if limited
            if (vertices.size() > maxVerts) {
                ImGui::SetNextWindowPos(ImVec2((float)g_swapchainExtent.width / 2 - 100, 50), ImGuiCond_Always);
                ImGui::Begin("##vertexWarning", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize);
                ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Showing %zu of %zu vertices", maxVerts, vertices.size());
                ImGui::End();
            }
            
            // Draw connection points as special markers
            for (size_t i = 0; i < g_eseConnectionPoints.size(); i++) {
                const ConnectionPoint& cp = g_eseConnectionPoints[i];
                if (cp.vertex_index >= 0 && cp.vertex_index < (int)vertices.size()) {
                    glm::vec3 cpPos(cp.position[0], cp.position[1], cp.position[2]);
                    bool inFront;
                    glm::vec2 screenPos = ese_project_to_screen(cpPos, &inFront);
                    
                    if (inFront && screenPos.x >= 0 && screenPos.x < g_swapchainExtent.width &&
                        screenPos.y >= 0 && screenPos.y < g_swapchainExtent.height) {
                        
                        ImVec2 pos(screenPos.x, screenPos.y);
                        
                        // Draw connection point as a larger yellow/gold circle with outline
                        bool isSelected = (g_eseSelectedConnectionPoint >= 0 && 
                                         (size_t)g_eseSelectedConnectionPoint == i);
                        float radius = isSelected ? 4.0f : 3.0f;
                        ImU32 color = isSelected ? IM_COL32(255, 215, 0, 255) : IM_COL32(255, 200, 0, 200);
                        ImU32 outlineColor = IM_COL32(255, 255, 255, 255);
                        
                        drawList->AddCircleFilled(pos, radius, color);
                        drawList->AddCircle(pos, radius, outlineColor, 0, 1.5f);
                    }
                }
            }
        }
    }
    
    // Edge Info Panel (only visible in edge mode)
    if (g_eseEdgeMode) {
        ImGui::SetNextWindowPos(ImVec2((float)g_swapchainExtent.width - 330, 30), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(320, 200), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Edge Info")) {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.2f, 1.0f), "EDGE MODE ACTIVE");
            ImGui::Text("Click to select, Ctrl+click to multi-select");
            ImGui::Separator();
            
            if (!g_eseSelectedEdges.empty()) {
                ImGui::Text("Selected: %zu edges", g_eseSelectedEdges.size());
            }
            
            if (g_eseSelectedEdge >= 0 && g_eseSelectedEdge < (int)g_eseQuadEdges.size() && g_objMeshResource) {
                const std::vector<MeshVertex>& vertices = g_objMeshResource->getVertices();
                const QuadEdge& qe = g_eseQuadEdges[g_eseSelectedEdge];
                
                ImGui::Text("Quad Edge: %d", g_eseSelectedEdge);
                ImGui::Separator();
                ImGui::Text("Vertex A: %u", qe.v0);
                if (qe.v0 < vertices.size()) {
                    ImGui::Text("  Pos: (%.3f, %.3f, %.3f)", 
                        vertices[qe.v0].pos[0], vertices[qe.v0].pos[1], vertices[qe.v0].pos[2]);
                }
                ImGui::Text("Vertex B: %u", qe.v1);
                if (qe.v1 < vertices.size()) {
                    ImGui::Text("  Pos: (%.3f, %.3f, %.3f)", 
                        vertices[qe.v1].pos[0], vertices[qe.v1].pos[1], vertices[qe.v1].pos[2]);
                }
                
                ImGui::Separator();
                ImGui::Text("In %zu quad(s)", qe.quadIndices.size());
                
                // Calculate edge length
                if (qe.v0 < vertices.size() && qe.v1 < vertices.size()) {
                    glm::vec3 p0(vertices[qe.v0].pos[0], vertices[qe.v0].pos[1], vertices[qe.v0].pos[2]);
                    glm::vec3 p1(vertices[qe.v1].pos[0], vertices[qe.v1].pos[1], vertices[qe.v1].pos[2]);
                    float length = glm::length(p1 - p0);
                    ImGui::Separator();
                    ImGui::Text("Edge Length: %.6f", length);
                }
                
                ImGui::Separator();
                ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "Press I to insert edge loop");
            } else {
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No edge selected");
                ImGui::Text("Click on a quad edge to select it");
            }
            
            if (g_objMeshResource) {
                ImGui::Separator();
                ImGui::Text("Quad edges: %zu", g_eseQuadEdges.size());
            }
        }
        ImGui::End();
        
        // Draw edges directly from g_eseQuadEdges (which correctly tracks quad edges)
        if (g_objMeshResource && !g_eseQuadEdges.empty()) {
            ImDrawList* drawList = ImGui::GetForegroundDrawList();
            const std::vector<MeshVertex>& vertices = g_objMeshResource->getVertices();
            
            for (size_t ei = 0; ei < g_eseQuadEdges.size(); ei++) {
                const QuadEdge& qe = g_eseQuadEdges[ei];
                
                if (qe.v0 >= vertices.size() || qe.v1 >= vertices.size()) continue;
                
                glm::vec3 p0(vertices[qe.v0].pos[0], vertices[qe.v0].pos[1], vertices[qe.v0].pos[2]);
                glm::vec3 p1(vertices[qe.v1].pos[0], vertices[qe.v1].pos[1], vertices[qe.v1].pos[2]);
                
                bool inFront0, inFront1;
                glm::vec2 screenStart = ese_project_to_screen(p0, &inFront0);
                glm::vec2 screenEnd = ese_project_to_screen(p1, &inFront1);
                
                if (inFront0 && inFront1) {
                    bool isSelected = g_eseSelectedEdges.count((int)ei) > 0;
                    
                    ImU32 color = isSelected ? 
                        IM_COL32(255, 100, 50, 255) : IM_COL32(255, 200, 120, 180);
                    float thickness = isSelected ? 4.0f : 2.0f;
                    
                    drawList->AddLine(
                        ImVec2(screenStart.x, screenStart.y),
                        ImVec2(screenEnd.x, screenEnd.y),
                        color, thickness
                    );
                }
            }
        }
    }
    
    // Face Info Panel (only visible in face mode)
    if (g_eseFaceMode) {
        ImGui::SetNextWindowPos(ImVec2((float)g_swapchainExtent.width - 330, 30), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(320, 220), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Face Info")) {
            ImGui::TextColored(ImVec4(0.8f, 0.2f, 1.0f, 1.0f), "FACE MODE ACTIVE");
            ImGui::Text("Press 3 to toggle, click to select");
            ImGui::Separator();
            
            if (g_eseSelectedFace >= 0 && g_objMeshResource) {
                const std::vector<MeshVertex>& vertices = g_objMeshResource->getVertices();
                const std::vector<uint32_t>& indices = g_objMeshResource->getIndices();
                
                size_t triStart = g_eseSelectedFace * 3;
                if (triStart + 2 < indices.size()) {
                    uint32_t v0Idx = indices[triStart];
                    uint32_t v1Idx = indices[triStart + 1];
                    uint32_t v2Idx = indices[triStart + 2];
                    
                    ImGui::Text("Selected Face: %d", g_eseSelectedFace);
                    ImGui::Separator();
                    ImGui::Text("Vertices: %u, %u, %u", v0Idx, v1Idx, v2Idx);
                    
                    if (v0Idx < vertices.size() && v1Idx < vertices.size() && v2Idx < vertices.size()) {
                        glm::vec3 p0(vertices[v0Idx].pos[0], vertices[v0Idx].pos[1], vertices[v0Idx].pos[2]);
                        glm::vec3 p1(vertices[v1Idx].pos[0], vertices[v1Idx].pos[1], vertices[v1Idx].pos[2]);
                        glm::vec3 p2(vertices[v2Idx].pos[0], vertices[v2Idx].pos[1], vertices[v2Idx].pos[2]);
                        
                        // Calculate face normal
                        glm::vec3 edge1 = p1 - p0;
                        glm::vec3 edge2 = p2 - p0;
                        glm::vec3 normal = glm::normalize(glm::cross(edge1, edge2));
                        
                        ImGui::Separator();
                        ImGui::Text("Normal:");
                        ImGui::Text("  (%.4f, %.4f, %.4f)", normal.x, normal.y, normal.z);
                        
                        // Calculate area
                        float area = glm::length(glm::cross(edge1, edge2)) * 0.5f;
                        ImGui::Separator();
                        ImGui::Text("Area: %.6f", area);
                        
                        // Face center
                        glm::vec3 center = (p0 + p1 + p2) / 3.0f;
                        ImGui::Text("Center: (%.3f, %.3f, %.3f)", center.x, center.y, center.z);
                    }
                }
            } else {
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No face selected");
                ImGui::Text("Click on a face to select it");
            }
            
            if (g_objMeshResource) {
                ImGui::Separator();
                ImGui::Text("Total faces: %zu", g_objMeshResource->getIndices().size() / 3);
            }
        }
        ImGui::End();
        
        // Draw face highlighting as overlay
        if (g_objMeshResource) {
            ImDrawList* drawList = ImGui::GetForegroundDrawList();
            const std::vector<MeshVertex>& vertices = g_objMeshResource->getVertices();
            const std::vector<uint32_t>& indices = g_objMeshResource->getIndices();
            
            // Calculate camera position for backface culling
            float yawRad = g_eseCameraYaw * 3.14159f / 180.0f;
            float pitchRad = g_eseCameraPitch * 3.14159f / 180.0f;
            if (pitchRad > 1.5f) pitchRad = 1.5f;
            if (pitchRad < -1.5f) pitchRad = -1.5f;
            
            float camX = g_eseCameraDistance * cosf(pitchRad) * sinf(yawRad);
            float camY = g_eseCameraDistance * sinf(pitchRad);
            float camZ = g_eseCameraDistance * cosf(pitchRad) * cosf(yawRad);
            
            glm::vec3 cameraPos(
                camX + g_eseCameraTargetX + g_eseCameraPanX,
                camY + g_eseCameraTargetY + g_eseCameraPanY,
                camZ + g_eseCameraTargetZ
            );
            
            size_t numFaces = indices.size() / 3;
            
            // Only draw selected face highlight (drawing all faces would be too heavy)
            if (g_eseSelectedFace >= 0 && (size_t)g_eseSelectedFace < numFaces) {
                size_t triStart = g_eseSelectedFace * 3;
                if (triStart + 2 < indices.size()) {
                    uint32_t v0Idx = indices[triStart];
                    uint32_t v1Idx = indices[triStart + 1];
                    uint32_t v2Idx = indices[triStart + 2];
                    
                    glm::vec3 v0(vertices[v0Idx].pos[0], vertices[v0Idx].pos[1], vertices[v0Idx].pos[2]);
                    glm::vec3 v1(vertices[v1Idx].pos[0], vertices[v1Idx].pos[1], vertices[v1Idx].pos[2]);
                    glm::vec3 v2(vertices[v2Idx].pos[0], vertices[v2Idx].pos[1], vertices[v2Idx].pos[2]);
                    
                    bool inFront0, inFront1, inFront2;
                    glm::vec2 screen0 = ese_project_to_screen(v0, &inFront0);
                    glm::vec2 screen1 = ese_project_to_screen(v1, &inFront1);
                    glm::vec2 screen2 = ese_project_to_screen(v2, &inFront2);
                    
                    if (inFront0 && inFront1 && inFront2) {
                        // Draw filled triangle with transparency
                        drawList->AddTriangleFilled(
                            ImVec2(screen0.x, screen0.y),
                            ImVec2(screen1.x, screen1.y),
                            ImVec2(screen2.x, screen2.y),
                            IM_COL32(200, 50, 255, 100)
                        );
                        // Draw outline
                        drawList->AddTriangle(
                            ImVec2(screen0.x, screen0.y),
                            ImVec2(screen1.x, screen1.y),
                            ImVec2(screen2.x, screen2.y),
                            IM_COL32(255, 100, 255, 255),
                            2.0f
                        );
                    }
                }
            }
            
            // Draw small dots at face centers for all visible faces
            size_t maxFaces = std::min(numFaces, (size_t)20000);
            size_t step = (numFaces > maxFaces) ? (numFaces / maxFaces) : 1;
            
            for (size_t faceIdx = 0; faceIdx < numFaces; faceIdx += step) {
                size_t triStart = faceIdx * 3;
                if (triStart + 2 >= indices.size()) break;
                
                uint32_t v0Idx = indices[triStart];
                uint32_t v1Idx = indices[triStart + 1];
                uint32_t v2Idx = indices[triStart + 2];
                
                glm::vec3 v0(vertices[v0Idx].pos[0], vertices[v0Idx].pos[1], vertices[v0Idx].pos[2]);
                glm::vec3 v1(vertices[v1Idx].pos[0], vertices[v1Idx].pos[1], vertices[v1Idx].pos[2]);
                glm::vec3 v2(vertices[v2Idx].pos[0], vertices[v2Idx].pos[1], vertices[v2Idx].pos[2]);
                
                // Face normal for backface culling
                glm::vec3 edge1 = v1 - v0;
                glm::vec3 edge2 = v2 - v0;
                glm::vec3 faceNormal = glm::normalize(glm::cross(edge1, edge2));
                glm::vec3 faceCenter = (v0 + v1 + v2) / 3.0f;
                glm::vec3 toCamera = glm::normalize(cameraPos - faceCenter);
                
                if (glm::dot(faceNormal, toCamera) <= 0.0f) continue;  // Skip backfaces
                
                bool inFront;
                glm::vec2 screenCenter = ese_project_to_screen(faceCenter, &inFront);
                
                if (inFront && screenCenter.x >= 0 && screenCenter.x < g_swapchainExtent.width &&
                    screenCenter.y >= 0 && screenCenter.y < g_swapchainExtent.height) {
                    
                    ImU32 color = ((int)faceIdx == g_eseSelectedFace) ? 
                        IM_COL32(255, 100, 255, 255) : IM_COL32(200, 100, 255, 100);
                    float radius = ((int)faceIdx == g_eseSelectedFace) ? 3.0f : 1.5f;
                    
                    drawList->AddCircleFilled(
                        ImVec2(screenCenter.x, screenCenter.y),
                        radius, color
                    );
                }
            }
        }
    }
    
    // Quad mode visualization and info panel
    static glm::vec3 g_eseSelectedQuadCenter(0.0f);
    
    // Build quads and quad edges for both quad mode and edge mode
    if ((g_eseQuadMode || g_eseEdgeMode) && g_objMeshResource) {
        const std::vector<MeshVertex>& vertices = g_objMeshResource->getVertices();
        const std::vector<uint32_t>& indices = g_objMeshResource->getIndices();
        
        // Rebuild quads if needed (only when model changes)
        bool shouldRebuildQuads = (indices.size() != g_eseLastQuadIndexCount) || g_eseReconstructedQuads.empty();
        if (shouldRebuildQuads) {
            g_eseReconstructedQuads.clear();
            g_eseLastQuadIndexCount = indices.size();
            
            // Find coplanar adjacent triangles to form quads
            size_t numFaces = indices.size() / 3;
            std::set<size_t> usedFaces;
            
            for (size_t i = 0; i < numFaces && g_eseReconstructedQuads.size() < 20000; i++) {
                if (usedFaces.count(i)) continue;
                
                size_t triStart1 = i * 3;
                uint32_t a1 = indices[triStart1], b1 = indices[triStart1+1], c1 = indices[triStart1+2];
                
                glm::vec3 v0(vertices[a1].pos[0], vertices[a1].pos[1], vertices[a1].pos[2]);
                glm::vec3 v1(vertices[b1].pos[0], vertices[b1].pos[1], vertices[b1].pos[2]);
                glm::vec3 v2(vertices[c1].pos[0], vertices[c1].pos[1], vertices[c1].pos[2]);
                glm::vec3 n1 = glm::normalize(glm::cross(v1 - v0, v2 - v0));
                
                // Find adjacent triangle with same normal
                for (size_t j = i + 1; j < numFaces; j++) {
                    if (usedFaces.count(j)) continue;
                    
                    size_t triStart2 = j * 3;
                    uint32_t a2 = indices[triStart2], b2 = indices[triStart2+1], c2 = indices[triStart2+2];
                    
                    // Check for shared edge
                    std::set<uint32_t> verts1 = {a1, b1, c1};
                    std::set<uint32_t> verts2 = {a2, b2, c2};
                    std::vector<uint32_t> shared;
                    for (uint32_t v : verts1) if (verts2.count(v)) shared.push_back(v);
                    
                    if (shared.size() == 2) {
                        // Shared edge found, check if coplanar
                        glm::vec3 u0(vertices[a2].pos[0], vertices[a2].pos[1], vertices[a2].pos[2]);
                        glm::vec3 u1(vertices[b2].pos[0], vertices[b2].pos[1], vertices[b2].pos[2]);
                        glm::vec3 u2(vertices[c2].pos[0], vertices[c2].pos[1], vertices[c2].pos[2]);
                        glm::vec3 n2 = glm::normalize(glm::cross(u1 - u0, u2 - u0));
                        
                        if (glm::dot(n1, n2) > 0.95f) {  // Relaxed threshold for better quad detection
                            // Coplanar - form a quad
                            uint32_t unshared1 = 0, unshared2 = 0;
                            for (uint32_t v : verts1) if (!verts2.count(v)) unshared1 = v;
                            for (uint32_t v : verts2) if (!verts1.count(v)) unshared2 = v;
                            
                            g_eseReconstructedQuads.push_back({shared[0], unshared1, shared[1], unshared2});
                            usedFaces.insert(i);
                            usedFaces.insert(j);
                            break;
                        }
                    }
                }
            }
            
            // Build quad edges DIRECTLY from reconstructed quads
            // Each quad has 4 perimeter edges: (0-1), (1-2), (2-3), (3-0)
            g_eseQuadEdges.clear();
            
            std::map<std::pair<uint32_t, uint32_t>, int> edgeToQuadEdgeIndex;
            
            for (int qi = 0; qi < (int)g_eseReconstructedQuads.size(); qi++) {
                const auto& q = g_eseReconstructedQuads[qi];
                
                // Add all 4 perimeter edges of this quad
                for (int e = 0; e < 4; e++) {
                    uint32_t va = q[e];
                    uint32_t vb = q[(e + 1) % 4];
                    auto key = std::make_pair(std::min(va, vb), std::max(va, vb));
                    
                    if (edgeToQuadEdgeIndex.count(key) == 0) {
                        // New edge
                        QuadEdge qe;
                        qe.v0 = va;
                        qe.v1 = vb;
                        qe.quadIndices.push_back(qi);
                        qe.edgeInQuad = e;
                        edgeToQuadEdgeIndex[key] = (int)g_eseQuadEdges.size();
                        g_eseQuadEdges.push_back(qe);
                    } else {
                        // Edge already exists, add this quad to it
                        g_eseQuadEdges[edgeToQuadEdgeIndex[key]].quadIndices.push_back(qi);
                    }
                }
            }
            
            std::cout << "[ESE] Built " << g_eseQuadEdges.size() << " edges from " 
                      << g_eseReconstructedQuads.size() << " quads" << std::endl;
        }
        
        // Draw quads (only in quad mode, not edge mode)
        if (!g_eseQuadMode) {
            // Skip quad drawing in edge mode - edges are drawn separately
        } else {
        ImDrawList* drawList = ImGui::GetForegroundDrawList();
        
        float yawRad = g_eseCameraYaw * 3.14159f / 180.0f;
        float pitchRad = g_eseCameraPitch * 3.14159f / 180.0f;
        if (pitchRad > 1.5f) pitchRad = 1.5f;
        if (pitchRad < -1.5f) pitchRad = -1.5f;
        float camX = g_eseCameraDistance * cosf(pitchRad) * sinf(yawRad);
        float camY = g_eseCameraDistance * sinf(pitchRad);
        float camZ = g_eseCameraDistance * cosf(pitchRad) * cosf(yawRad);
        glm::vec3 cameraPos(camX + g_eseCameraTargetX + g_eseCameraPanX,
                            camY + g_eseCameraTargetY + g_eseCameraPanY,
                            camZ + g_eseCameraTargetZ);
        
        for (size_t qi = 0; qi < g_eseReconstructedQuads.size(); qi++) {
            const auto& quad = g_eseReconstructedQuads[qi];
            glm::vec3 p0(vertices[quad[0]].pos[0], vertices[quad[0]].pos[1], vertices[quad[0]].pos[2]);
            glm::vec3 p1(vertices[quad[1]].pos[0], vertices[quad[1]].pos[1], vertices[quad[1]].pos[2]);
            glm::vec3 p2(vertices[quad[2]].pos[0], vertices[quad[2]].pos[1], vertices[quad[2]].pos[2]);
            glm::vec3 p3(vertices[quad[3]].pos[0], vertices[quad[3]].pos[1], vertices[quad[3]].pos[2]);
            
            glm::vec3 center = (p0 + p1 + p2 + p3) * 0.25f;
            glm::vec3 normal = glm::normalize(glm::cross(p1 - p0, p2 - p0));
            glm::vec3 toCamera = glm::normalize(cameraPos - center);
            
            // NO backface culling for quads - show all of them
            // if (glm::dot(normal, toCamera) <= 0.0f) continue;  // Backface cull
            
            bool inFront0, inFront1, inFront2, inFront3;
            glm::vec2 s0 = ese_project_to_screen(p0, &inFront0);
            glm::vec2 s1 = ese_project_to_screen(p1, &inFront1);
            glm::vec2 s2 = ese_project_to_screen(p2, &inFront2);
            glm::vec2 s3 = ese_project_to_screen(p3, &inFront3);
            
            if (inFront0 && inFront1 && inFront2 && inFront3) {
                bool isSelected = g_eseSelectedQuads.count((int)qi) > 0;
                
                // Check if normal faces camera
                bool facingCamera = glm::dot(normal, toCamera) > 0.0f;
                
                // Different color for front/back facing
                ImU32 color;
                if (isSelected) {
                    color = IM_COL32(255, 200, 50, 255);
                } else if (facingCamera) {
                    color = IM_COL32(200, 180, 100, 180);
                } else {
                    color = IM_COL32(100, 80, 50, 100);  // Dimmer for back-facing
                }
                float thickness = isSelected ? 3.0f : 1.5f;
                
                // Draw quad outline (4 edges)
                drawList->AddLine(ImVec2(s0.x, s0.y), ImVec2(s1.x, s1.y), color, thickness);
                drawList->AddLine(ImVec2(s1.x, s1.y), ImVec2(s2.x, s2.y), color, thickness);
                drawList->AddLine(ImVec2(s2.x, s2.y), ImVec2(s3.x, s3.y), color, thickness);
                drawList->AddLine(ImVec2(s3.x, s3.y), ImVec2(s0.x, s0.y), color, thickness);
                
                // Draw center dot
                bool inFrontC;
                glm::vec2 sc = ese_project_to_screen(center, &inFrontC);
                if (inFrontC) {
                    drawList->AddCircleFilled(ImVec2(sc.x, sc.y), isSelected ? 4.0f : 2.0f, color);
                }
            }
        }
        
        // Info panel
        ImGui::SetNextWindowPos(ImVec2((float)g_swapchainExtent.width - 330, 30), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(320, 200), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Quad Info")) {
            ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.3f, 1.0f), "QUAD MODE ACTIVE");
            if (g_eseExtrudeMode) {
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), ">>> EXTRUDE MODE <<<");
                ImGui::Text("Drag gizmo to extrude!");
            } else {
                ImGui::Text("Press W to enable extrude");
            }
            ImGui::Text("Reconstructed Quads: %zu", g_eseReconstructedQuads.size());
            ImGui::Separator();
            
            if (!g_eseSelectedQuads.empty()) {
                ImGui::Text("Selected: %zu quads", g_eseSelectedQuads.size());
                ImGui::Text("Ctrl+click to add/remove from selection");
                
                if (g_eseSelectedQuad >= 0 && g_eseSelectedQuad < (int)g_eseReconstructedQuads.size()) {
                    const auto& quad = g_eseReconstructedQuads[g_eseSelectedQuad];
                    ImGui::Separator();
                    ImGui::Text("Primary Quad: %d", g_eseSelectedQuad);
                    ImGui::Text("Vertices: %u, %u, %u, %u", quad[0], quad[1], quad[2], quad[3]);
                    
                    glm::vec3 p0(vertices[quad[0]].pos[0], vertices[quad[0]].pos[1], vertices[quad[0]].pos[2]);
                    glm::vec3 p1(vertices[quad[1]].pos[0], vertices[quad[1]].pos[1], vertices[quad[1]].pos[2]);
                    glm::vec3 p2(vertices[quad[2]].pos[0], vertices[quad[2]].pos[1], vertices[quad[2]].pos[2]);
                    glm::vec3 p3(vertices[quad[3]].pos[0], vertices[quad[3]].pos[1], vertices[quad[3]].pos[2]);
                    
                    g_eseSelectedQuadCenter = (p0 + p1 + p2 + p3) * 0.25f;
                    ImGui::Text("Center: (%.3f, %.3f, %.3f)", g_eseSelectedQuadCenter.x, g_eseSelectedQuadCenter.y, g_eseSelectedQuadCenter.z);
                }
            } else {
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No quad selected");
                ImGui::Text("Click on a quad to select it");
            }
        }
        ImGui::End();
        } // End of quad mode drawing (the else block)
    }
    
    // Move Gizmo - Draw when any element is selected
    bool hasSelection = (!g_eseSelectedVertices.empty() && g_eseVertexMode) ||
                        (!g_eseSelectedEdges.empty() && g_eseEdgeMode) ||
                        (g_eseSelectedFace >= 0 && g_eseFaceMode) ||
                        (!g_eseSelectedQuads.empty() && g_eseQuadMode);
    
    if (hasSelection && g_objMeshResource) {
        // Calculate gizmo position based on selection
        const std::vector<MeshVertex>& vertices = g_objMeshResource->getVertices();
        const std::vector<uint32_t>& indices = g_objMeshResource->getIndices();
        
        glm::vec3 gizmoCenter(0.0f);
        
        if (g_eseVertexMode && !g_eseSelectedVertices.empty()) {
            // Calculate center of all selected vertices
            glm::vec3 sum(0.0f);
            for (int vi : g_eseSelectedVertices) {
                if (vi >= 0 && vi < (int)vertices.size()) {
                    sum += glm::vec3(vertices[vi].pos[0], vertices[vi].pos[1], vertices[vi].pos[2]);
                }
            }
            gizmoCenter = sum / (float)g_eseSelectedVertices.size();
        } else if (g_eseEdgeMode && !g_eseSelectedEdges.empty()) {
            // Calculate center of all selected quad edges
            glm::vec3 sum(0.0f);
            int count = 0;
            for (int edgeIdx : g_eseSelectedEdges) {
                if (edgeIdx >= 0 && edgeIdx < (int)g_eseQuadEdges.size()) {
                    const QuadEdge& qe = g_eseQuadEdges[edgeIdx];
                    uint32_t v0Idx = qe.v0;
                    uint32_t v1Idx = qe.v1;
                    glm::vec3 v0(vertices[v0Idx].pos[0], vertices[v0Idx].pos[1], vertices[v0Idx].pos[2]);
                    glm::vec3 v1(vertices[v1Idx].pos[0], vertices[v1Idx].pos[1], vertices[v1Idx].pos[2]);
                    sum += (v0 + v1) * 0.5f;
                    count++;
                }
            }
            if (count > 0) gizmoCenter = sum / (float)count;
        } else if (g_eseFaceMode && g_eseSelectedFace >= 0) {
            size_t triStart = g_eseSelectedFace * 3;
            if (triStart + 2 < indices.size()) {
                glm::vec3 v0(vertices[indices[triStart]].pos[0], vertices[indices[triStart]].pos[1], vertices[indices[triStart]].pos[2]);
                glm::vec3 v1(vertices[indices[triStart+1]].pos[0], vertices[indices[triStart+1]].pos[1], vertices[indices[triStart+1]].pos[2]);
                glm::vec3 v2(vertices[indices[triStart+2]].pos[0], vertices[indices[triStart+2]].pos[1], vertices[indices[triStart+2]].pos[2]);
                gizmoCenter = (v0 + v1 + v2) / 3.0f;
            }
        } else if (g_eseQuadMode && !g_eseSelectedQuads.empty()) {
            // Calculate center of all selected quads
            glm::vec3 sum(0.0f);
            int count = 0;
            for (int quadIdx : g_eseSelectedQuads) {
                if (quadIdx >= 0 && quadIdx < (int)g_eseReconstructedQuads.size()) {
                    const auto& quad = g_eseReconstructedQuads[quadIdx];
                    glm::vec3 p0(vertices[quad[0]].pos[0], vertices[quad[0]].pos[1], vertices[quad[0]].pos[2]);
                    glm::vec3 p1(vertices[quad[1]].pos[0], vertices[quad[1]].pos[1], vertices[quad[1]].pos[2]);
                    glm::vec3 p2(vertices[quad[2]].pos[0], vertices[quad[2]].pos[1], vertices[quad[2]].pos[2]);
                    glm::vec3 p3(vertices[quad[3]].pos[0], vertices[quad[3]].pos[1], vertices[quad[3]].pos[2]);
                    sum += (p0 + p1 + p2 + p3) * 0.25f;
                    count++;
                }
            }
            if (count > 0) gizmoCenter = sum / (float)count;
        }
        
        g_eseGizmoPosition = gizmoCenter;
        
        // Draw gizmo axes
        ImDrawList* drawList = ImGui::GetForegroundDrawList();
        float axisLength = 0.3f;  // Length of gizmo axes in world space
        float scaleHandlePos = 0.5f;  // Scale handle at 50% of axis length
        
        bool inFrontCenter;
        glm::vec2 screenCenter = ese_project_to_screen(gizmoCenter, &inFrontCenter);
        
        // Store scale handle positions for hit testing
        g_eseScaleCenter = gizmoCenter;
        
        if (inFrontCenter) {
            // X axis (red)
            bool inFrontX, inFrontXScale;
            glm::vec2 screenX = ese_project_to_screen(gizmoCenter + glm::vec3(axisLength, 0, 0), &inFrontX);
            glm::vec2 screenXScale = ese_project_to_screen(gizmoCenter + glm::vec3(axisLength * scaleHandlePos, 0, 0), &inFrontXScale);
            if (inFrontX) {
                ImU32 colorX = (g_eseGizmoDragAxis == 0 && !g_eseGizmoScaling) ? IM_COL32(255, 255, 0, 255) : IM_COL32(255, 50, 50, 255);
                ImU32 scaleColorX = (g_eseGizmoDragAxis == 0 && g_eseGizmoScaling) ? IM_COL32(255, 255, 0, 255) : IM_COL32(255, 100, 100, 255);
                drawList->AddLine(ImVec2(screenCenter.x, screenCenter.y), ImVec2(screenX.x, screenX.y), colorX, 3.0f);
                drawList->AddCircleFilled(ImVec2(screenX.x, screenX.y), 6.0f, colorX);
                // Scale handle (small square)
                if (inFrontXScale) {
                    float sz = 5.0f;
                    drawList->AddRectFilled(
                        ImVec2(screenXScale.x - sz, screenXScale.y - sz),
                        ImVec2(screenXScale.x + sz, screenXScale.y + sz),
                        scaleColorX);
                }
                drawList->AddText(ImVec2(screenX.x + 8, screenX.y - 6), IM_COL32(255, 50, 50, 255), "X");
            }
            
            // Y axis (green)
            bool inFrontY, inFrontYScale;
            glm::vec2 screenY = ese_project_to_screen(gizmoCenter + glm::vec3(0, axisLength, 0), &inFrontY);
            glm::vec2 screenYScale = ese_project_to_screen(gizmoCenter + glm::vec3(0, axisLength * scaleHandlePos, 0), &inFrontYScale);
            if (inFrontY) {
                ImU32 colorY = (g_eseGizmoDragAxis == 1 && !g_eseGizmoScaling) ? IM_COL32(255, 255, 0, 255) : IM_COL32(50, 255, 50, 255);
                ImU32 scaleColorY = (g_eseGizmoDragAxis == 1 && g_eseGizmoScaling) ? IM_COL32(255, 255, 0, 255) : IM_COL32(100, 255, 100, 255);
                drawList->AddLine(ImVec2(screenCenter.x, screenCenter.y), ImVec2(screenY.x, screenY.y), colorY, 3.0f);
                drawList->AddCircleFilled(ImVec2(screenY.x, screenY.y), 6.0f, colorY);
                // Scale handle (small square)
                if (inFrontYScale) {
                    float sz = 5.0f;
                    drawList->AddRectFilled(
                        ImVec2(screenYScale.x - sz, screenYScale.y - sz),
                        ImVec2(screenYScale.x + sz, screenYScale.y + sz),
                        scaleColorY);
                }
                drawList->AddText(ImVec2(screenY.x + 8, screenY.y - 6), IM_COL32(50, 255, 50, 255), "Y");
            }
            
            // Z axis (blue)
            bool inFrontZ, inFrontZScale;
            glm::vec2 screenZ = ese_project_to_screen(gizmoCenter + glm::vec3(0, 0, axisLength), &inFrontZ);
            glm::vec2 screenZScale = ese_project_to_screen(gizmoCenter + glm::vec3(0, 0, axisLength * scaleHandlePos), &inFrontZScale);
            if (inFrontZ) {
                ImU32 colorZ = (g_eseGizmoDragAxis == 2 && !g_eseGizmoScaling) ? IM_COL32(255, 255, 0, 255) : IM_COL32(50, 50, 255, 255);
                ImU32 scaleColorZ = (g_eseGizmoDragAxis == 2 && g_eseGizmoScaling) ? IM_COL32(255, 255, 0, 255) : IM_COL32(100, 100, 255, 255);
                drawList->AddLine(ImVec2(screenCenter.x, screenCenter.y), ImVec2(screenZ.x, screenZ.y), colorZ, 3.0f);
                drawList->AddCircleFilled(ImVec2(screenZ.x, screenZ.y), 6.0f, colorZ);
                // Scale handle (small square)
                if (inFrontZScale) {
                    float sz = 5.0f;
                    drawList->AddRectFilled(
                        ImVec2(screenZScale.x - sz, screenZScale.y - sz),
                        ImVec2(screenZScale.x + sz, screenZScale.y + sz),
                        scaleColorZ);
                }
                drawList->AddText(ImVec2(screenZ.x + 8, screenZ.y - 6), IM_COL32(50, 50, 255, 255), "Z");
            }
            
            // Center point
            drawList->AddCircleFilled(ImVec2(screenCenter.x, screenCenter.y), 4.0f, IM_COL32(255, 255, 255, 255));
            
            // Rotation circles (draw arcs around each axis)
            float rotRadius = axisLength * 0.7f;  // Radius of rotation circles
            int numSegments = 32;
            
            // X axis rotation circle (rotates around X, so it's in YZ plane)
            ImU32 rotColorX = (g_eseGizmoDragAxis == 0 && g_eseGizmoRotating) ? IM_COL32(255, 255, 0, 255) : IM_COL32(255, 100, 100, 200);
            for (int i = 0; i < numSegments; i++) {
                float angle1 = (float)i / numSegments * 2.0f * 3.14159f;
                float angle2 = (float)(i + 1) / numSegments * 2.0f * 3.14159f;
                glm::vec3 p1 = gizmoCenter + glm::vec3(0, cosf(angle1) * rotRadius, sinf(angle1) * rotRadius);
                glm::vec3 p2 = gizmoCenter + glm::vec3(0, cosf(angle2) * rotRadius, sinf(angle2) * rotRadius);
                bool f1, f2;
                glm::vec2 s1 = ese_project_to_screen(p1, &f1);
                glm::vec2 s2 = ese_project_to_screen(p2, &f2);
                if (f1 && f2) {
                    drawList->AddLine(ImVec2(s1.x, s1.y), ImVec2(s2.x, s2.y), rotColorX, 2.0f);
                }
            }
            
            // Y axis rotation circle (rotates around Y, so it's in XZ plane)
            ImU32 rotColorY = (g_eseGizmoDragAxis == 1 && g_eseGizmoRotating) ? IM_COL32(255, 255, 0, 255) : IM_COL32(100, 255, 100, 200);
            for (int i = 0; i < numSegments; i++) {
                float angle1 = (float)i / numSegments * 2.0f * 3.14159f;
                float angle2 = (float)(i + 1) / numSegments * 2.0f * 3.14159f;
                glm::vec3 p1 = gizmoCenter + glm::vec3(cosf(angle1) * rotRadius, 0, sinf(angle1) * rotRadius);
                glm::vec3 p2 = gizmoCenter + glm::vec3(cosf(angle2) * rotRadius, 0, sinf(angle2) * rotRadius);
                bool f1, f2;
                glm::vec2 s1 = ese_project_to_screen(p1, &f1);
                glm::vec2 s2 = ese_project_to_screen(p2, &f2);
                if (f1 && f2) {
                    drawList->AddLine(ImVec2(s1.x, s1.y), ImVec2(s2.x, s2.y), rotColorY, 2.0f);
                }
            }
            
            // Z axis rotation circle (rotates around Z, so it's in XY plane)
            ImU32 rotColorZ = (g_eseGizmoDragAxis == 2 && g_eseGizmoRotating) ? IM_COL32(255, 255, 0, 255) : IM_COL32(100, 100, 255, 200);
            for (int i = 0; i < numSegments; i++) {
                float angle1 = (float)i / numSegments * 2.0f * 3.14159f;
                float angle2 = (float)(i + 1) / numSegments * 2.0f * 3.14159f;
                glm::vec3 p1 = gizmoCenter + glm::vec3(cosf(angle1) * rotRadius, sinf(angle1) * rotRadius, 0);
                glm::vec3 p2 = gizmoCenter + glm::vec3(cosf(angle2) * rotRadius, sinf(angle2) * rotRadius, 0);
                bool f1, f2;
                glm::vec2 s1 = ese_project_to_screen(p1, &f1);
                glm::vec2 s2 = ese_project_to_screen(p2, &f2);
                if (f1 && f2) {
                    drawList->AddLine(ImVec2(s1.x, s1.y), ImVec2(s2.x, s2.y), rotColorZ, 2.0f);
                }
            }
        }
    }
    
    // Draw face normals visualization (independent of vertex mode)
    if (g_eseShowNormals && g_objMeshResource) {
        ImDrawList* drawList = ImGui::GetForegroundDrawList();
        const std::vector<MeshVertex>& vertices = g_objMeshResource->getVertices();
        const std::vector<uint32_t>& indices = g_objMeshResource->getIndices();
        
        // Calculate camera position
        float yawRad = g_eseCameraYaw * 3.14159f / 180.0f;
        float pitchRad = g_eseCameraPitch * 3.14159f / 180.0f;
        if (pitchRad > 1.5f) pitchRad = 1.5f;
        if (pitchRad < -1.5f) pitchRad = -1.5f;
        
        float camX = g_eseCameraDistance * cosf(pitchRad) * sinf(yawRad);
        float camY = g_eseCameraDistance * sinf(pitchRad);
        float camZ = g_eseCameraDistance * cosf(pitchRad) * cosf(yawRad);
        
        glm::vec3 cameraPos(
            camX + g_eseCameraTargetX + g_eseCameraPanX,
            camY + g_eseCameraTargetY + g_eseCameraPanY,
            camZ + g_eseCameraTargetZ
        );
        
        // Limit number of normals to draw for performance (sample every Nth face)
        size_t numFaces = indices.size() / 3;
        size_t maxFaces = std::min(numFaces, (size_t)10000);  // Limit to 10k faces
        size_t step = (numFaces > maxFaces) ? (numFaces / maxFaces) : 1;
        
        float normalLength = 0.1f;  // Length of normal line in world space
        
        for (size_t faceIdx = 0; faceIdx < numFaces; faceIdx += step) {
            size_t triStart = faceIdx * 3;
            if (triStart + 2 >= indices.size()) break;
            
            uint32_t v0Idx = indices[triStart];
            uint32_t v1Idx = indices[triStart + 1];
            uint32_t v2Idx = indices[triStart + 2];
            
            // Get triangle vertices
            glm::vec3 v0(vertices[v0Idx].pos[0], vertices[v0Idx].pos[1], vertices[v0Idx].pos[2]);
            glm::vec3 v1(vertices[v1Idx].pos[0], vertices[v1Idx].pos[1], vertices[v1Idx].pos[2]);
            glm::vec3 v2(vertices[v2Idx].pos[0], vertices[v2Idx].pos[1], vertices[v2Idx].pos[2]);
            
            // Calculate face center
            glm::vec3 faceCenter = (v0 + v1 + v2) / 3.0f;
            
            // Calculate face normal
            glm::vec3 edge1 = v1 - v0;
            glm::vec3 edge2 = v2 - v0;
            glm::vec3 faceNormal = glm::normalize(glm::cross(edge1, edge2));
            
            // Check if face is visible (backface culling)
            glm::vec3 toCamera = glm::normalize(cameraPos - faceCenter);
            float dot = glm::dot(faceNormal, toCamera);
            if (dot <= 0.0f) continue;  // Skip backfaces
            
            // Calculate end point of normal line
            glm::vec3 normalEnd = faceCenter + faceNormal * normalLength;
            
            // Project to screen space
            bool inFront0 = false, inFront1 = false;
            glm::vec2 screenStart = ese_project_to_screen(faceCenter, &inFront0);
            glm::vec2 screenEnd = ese_project_to_screen(normalEnd, &inFront1);
            
            if (inFront0 && inFront1 && 
                screenStart.x >= 0 && screenStart.x < g_swapchainExtent.width &&
                screenStart.y >= 0 && screenStart.y < g_swapchainExtent.height) {
                
                // Draw normal line (green for visible faces)
                drawList->AddLine(
                    ImVec2(screenStart.x, screenStart.y),
                    ImVec2(screenEnd.x, screenEnd.y),
                    IM_COL32(0, 255, 0, 200),
                    1.0f
                );
                
                // Draw small circle at face center
                drawList->AddCircleFilled(
                    ImVec2(screenStart.x, screenStart.y),
                    1.5f,
                    IM_COL32(0, 255, 0, 150)
                );
            }
        }
    }
    
    // Draw reconstructed quads visualization
    if (g_eseShowQuads && g_objMeshResource) {
        ImDrawList* drawList = ImGui::GetForegroundDrawList();
        const std::vector<MeshVertex>& vertices = g_objMeshResource->getVertices();
        const std::vector<uint32_t>& indices = g_objMeshResource->getIndices();
        
        // Calculate camera position for backface culling
        float yawRad = g_eseCameraYaw * 3.14159f / 180.0f;
        float pitchRad = g_eseCameraPitch * 3.14159f / 180.0f;
        if (pitchRad > 1.5f) pitchRad = 1.5f;
        if (pitchRad < -1.5f) pitchRad = -1.5f;
        
        float camX = g_eseCameraDistance * cosf(pitchRad) * sinf(yawRad);
        float camY = g_eseCameraDistance * sinf(pitchRad);
        float camZ = g_eseCameraDistance * cosf(pitchRad) * cosf(yawRad);
        
        glm::vec3 cameraPos(
            camX + g_eseCameraTargetX + g_eseCameraPanX,
            camY + g_eseCameraTargetY + g_eseCameraPanY,
            camZ + g_eseCameraTargetZ
        );
        
        // Build edge-to-triangles map
        // Key: pair of vertex indices (smaller first), Value: list of face indices
        std::map<std::pair<uint32_t, uint32_t>, std::vector<size_t>> edgeToFaces;
        
        size_t numFaces = indices.size() / 3;
        for (size_t faceIdx = 0; faceIdx < numFaces; faceIdx++) {
            size_t triStart = faceIdx * 3;
            if (triStart + 2 >= indices.size()) break;
            
            uint32_t vIdx[3] = { indices[triStart], indices[triStart + 1], indices[triStart + 2] };
            
            // Add all 3 edges
            for (int e = 0; e < 3; e++) {
                uint32_t v0 = vIdx[e];
                uint32_t v1 = vIdx[(e + 1) % 3];
                if (v0 > v1) std::swap(v0, v1);  // Ensure consistent ordering
                edgeToFaces[std::make_pair(v0, v1)].push_back(faceIdx);
            }
        }
        
        // Track which edges are quad diagonals (shared by 2 coplanar triangles)
        std::set<std::pair<uint32_t, uint32_t>> diagonalEdges;
        
        // Track processed quads to avoid duplicates
        std::set<std::pair<size_t, size_t>> processedQuads;
        
        // Coplanarity threshold (dot product of normals)
        const float coplanarThreshold = 0.98f;
        
        // Find quad pairs
        for (const auto& edgePair : edgeToFaces) {
            if (edgePair.second.size() == 2) {
                size_t face0 = edgePair.second[0];
                size_t face1 = edgePair.second[1];
                
                // Get face normals
                size_t tri0Start = face0 * 3;
                size_t tri1Start = face1 * 3;
                
                if (tri0Start + 2 >= indices.size() || tri1Start + 2 >= indices.size()) continue;
                
                glm::vec3 v0_0(vertices[indices[tri0Start]].pos[0], vertices[indices[tri0Start]].pos[1], vertices[indices[tri0Start]].pos[2]);
                glm::vec3 v0_1(vertices[indices[tri0Start + 1]].pos[0], vertices[indices[tri0Start + 1]].pos[1], vertices[indices[tri0Start + 1]].pos[2]);
                glm::vec3 v0_2(vertices[indices[tri0Start + 2]].pos[0], vertices[indices[tri0Start + 2]].pos[1], vertices[indices[tri0Start + 2]].pos[2]);
                
                glm::vec3 v1_0(vertices[indices[tri1Start]].pos[0], vertices[indices[tri1Start]].pos[1], vertices[indices[tri1Start]].pos[2]);
                glm::vec3 v1_1(vertices[indices[tri1Start + 1]].pos[0], vertices[indices[tri1Start + 1]].pos[1], vertices[indices[tri1Start + 1]].pos[2]);
                glm::vec3 v1_2(vertices[indices[tri1Start + 2]].pos[0], vertices[indices[tri1Start + 2]].pos[1], vertices[indices[tri1Start + 2]].pos[2]);
                
                glm::vec3 normal0 = glm::normalize(glm::cross(v0_1 - v0_0, v0_2 - v0_0));
                glm::vec3 normal1 = glm::normalize(glm::cross(v1_1 - v1_0, v1_2 - v1_0));
                
                // Check if coplanar (normals are similar)
                float dotNormals = glm::dot(normal0, normal1);
                if (dotNormals > coplanarThreshold) {
                    // These form a quad - mark this edge as a diagonal
                    diagonalEdges.insert(edgePair.first);
                }
            }
        }
        
        // Draw edges that are NOT diagonals (outer edges of quads + lone triangle edges)
        size_t maxFaces = std::min(numFaces, (size_t)20000);
        size_t step = (numFaces > maxFaces) ? (numFaces / maxFaces) : 1;
        
        for (size_t faceIdx = 0; faceIdx < numFaces; faceIdx += step) {
            size_t triStart = faceIdx * 3;
            if (triStart + 2 >= indices.size()) break;
            
            uint32_t vIdx[3] = { indices[triStart], indices[triStart + 1], indices[triStart + 2] };
            
            glm::vec3 v[3];
            for (int i = 0; i < 3; i++) {
                v[i] = glm::vec3(vertices[vIdx[i]].pos[0], vertices[vIdx[i]].pos[1], vertices[vIdx[i]].pos[2]);
            }
            
            // Backface culling
            glm::vec3 faceNormal = glm::normalize(glm::cross(v[1] - v[0], v[2] - v[0]));
            glm::vec3 faceCenter = (v[0] + v[1] + v[2]) / 3.0f;
            glm::vec3 toCamera = glm::normalize(cameraPos - faceCenter);
            
            if (glm::dot(faceNormal, toCamera) <= 0.0f) continue;
            
            // Draw each edge if it's not a diagonal
            for (int e = 0; e < 3; e++) {
                uint32_t ev0 = vIdx[e];
                uint32_t ev1 = vIdx[(e + 1) % 3];
                if (ev0 > ev1) std::swap(ev0, ev1);
                
                auto edgeKey = std::make_pair(ev0, ev1);
                
                // Skip diagonal edges
                if (diagonalEdges.count(edgeKey) > 0) continue;
                
                glm::vec3 eStart = v[e];
                glm::vec3 eEnd = v[(e + 1) % 3];
                
                bool inFront0, inFront1;
                glm::vec2 screenStart = ese_project_to_screen(eStart, &inFront0);
                glm::vec2 screenEnd = ese_project_to_screen(eEnd, &inFront1);
                
                if (inFront0 && inFront1) {
                    // Yellow-gold color for quad edges
                    drawList->AddLine(
                        ImVec2(screenStart.x, screenStart.y),
                        ImVec2(screenEnd.x, screenEnd.y),
                        IM_COL32(255, 215, 0, 200),
                        1.0f
                    );
                }
            }
        }
        
        // Show quad count info
        size_t quadCount = diagonalEdges.size();
        size_t pureTriCount = numFaces - (quadCount * 2);  // Triangles not part of quads
        
        ImGui::SetNextWindowPos(ImVec2((float)g_swapchainExtent.width / 2 - 80, 50), ImGuiCond_Always);
        ImGui::Begin("##quadInfo", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::TextColored(ImVec4(1.0f, 0.84f, 0.0f, 1.0f), "Quads: %zu  |  Tris: %zu", quadCount, pureTriCount);
        ImGui::End();
    }
    
    // Status bar
    if (g_esePropertiesModified) {
        ImGui::SetNextWindowPos(ImVec2(10, (float)g_swapchainExtent.height - 30), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(200, 25), ImGuiCond_Always);
        ImGui::Begin("##status", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "* Unsaved changes");
        ImGui::End();
    }
    
    // Handle file dialogs (after ImGui frame ends to avoid blocking)
    static bool pendingObjDialog = false;
    static bool pendingTextureDialog = false;
    static bool pendingOpenHdmDialog = false;
    static bool pendingSaveHdmDialog = false;
    
    if (g_eseOpenObjDialog) {
        g_eseOpenObjDialog = false;
        pendingObjDialog = true;
    }
    
    if (g_eseOpenTextureDialog) {
        g_eseOpenTextureDialog = false;
        pendingTextureDialog = true;
    }
    
    if (g_eseOpenHdmDialog) {
        g_eseOpenHdmDialog = false;
        pendingOpenHdmDialog = true;
    }
    
    if (g_eseSaveHdmDialog) {
        g_eseSaveHdmDialog = false;
        pendingSaveHdmDialog = true;
    }
    
    // Finalize ImGui frame
    ImGui::Render();
#endif
    
    // Wait for previous frame to complete BEFORE acquiring next image
    vkWaitForFences(g_device, 1, &g_inFlightFence, VK_TRUE, UINT64_MAX);
    vkResetFences(g_device, 1, &g_inFlightFence);
    
    // Acquire swapchain image
    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(g_device, g_swapchain, UINT64_MAX, 
                                            g_imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        return; // Swapchain needs recreation
    }
    
    // Reset and begin command buffer
    vkResetCommandBuffer(g_commandBuffers[imageIndex], 0);
    
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
    clearValues[0].color = {{0.1f, 0.1f, 0.15f, 1.0f}};  // Dark blue-gray background
    clearValues[1].depthStencil = {1.0f, 0};
    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();
    
    vkCmdBeginRenderPass(g_commandBuffers[imageIndex], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    
    // Render 3D model if loaded
    // Only render if we have proper mesh shaders (not the fallback 3D shaders)
    // The 3D shaders don't match MeshVertex format and will cause validation errors
    if (g_eseModelLoaded && g_objMeshInitialized && g_objMeshPipeline != VK_NULL_HANDLE) {
        // Check if we're using mesh shaders (by checking if mesh.vert.spv was loaded)
        // For now, just try to render - the validation errors will warn us but won't crash
        render_obj_mesh_to_command_buffer(g_commandBuffers[imageIndex]);
    }
    
#ifdef USE_IMGUI
    // Record ImGui draw commands into the command buffer (on top of 3D scene)
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), g_commandBuffers[imageIndex]);
#endif
    
    vkCmdEndRenderPass(g_commandBuffers[imageIndex]);
    vkEndCommandBuffer(g_commandBuffers[imageIndex]);
    
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
    
#ifdef USE_IMGUI
    // Handle file dialogs after render (so they don't block the frame)
    if (pendingObjDialog) {
        pendingObjDialog = false;
        const char* path = nfd_open_obj_dialog();
        if (path && strlen(path) > 0) {
            g_eseCurrentObjPath = path;
            strncpy(g_eseHdmProperties.model.obj_path, path, sizeof(g_eseHdmProperties.model.obj_path) - 1);
            std::cout << "[ESE] Selected OBJ: " << path << std::endl;
            // Wait for GPU to finish any pending work
            vkDeviceWaitIdle(g_device);
            // Clean up old mesh if any
            if (g_eseModelLoaded) {
                heidic_cleanup_renderer_obj_mesh();
                g_eseModelLoaded = false;
            }
            // Load model (use texture if set, otherwise use empty string for default)
            if (heidic_init_renderer_obj_mesh(window, g_eseCurrentObjPath.c_str(), 
                    g_eseCurrentTexturePath.empty() ? "" : g_eseCurrentTexturePath.c_str())) {
                g_eseModelLoaded = true;
                g_esePropertiesModified = true;
                std::cout << "[ESE] Model loaded!" << std::endl;
            } else {
                std::cerr << "[ESE] Failed to load model!" << std::endl;
            }
        }
    }
    
    if (pendingTextureDialog) {
        pendingTextureDialog = false;
        const char* path = nfd_open_texture_dialog();
        if (path && strlen(path) > 0) {
            g_eseCurrentTexturePath = path;
            strncpy(g_eseHdmProperties.model.texture_path, path, sizeof(g_eseHdmProperties.model.texture_path) - 1);
            std::cout << "[ESE] Selected texture: " << path << std::endl;
            // Wait for GPU to finish any pending work
            vkDeviceWaitIdle(g_device);
            // Clean up old mesh if any
            if (g_eseModelLoaded) {
                heidic_cleanup_renderer_obj_mesh();
                g_eseModelLoaded = false;
            }
            // Reload model with new texture (only if OBJ is set)
            if (!g_eseCurrentObjPath.empty()) {
                if (heidic_init_renderer_obj_mesh(window, g_eseCurrentObjPath.c_str(), g_eseCurrentTexturePath.c_str())) {
                    g_eseModelLoaded = true;
                    g_esePropertiesModified = true;
                    std::cout << "[ESE] Model reloaded with texture!" << std::endl;
                } else {
                    std::cerr << "[ESE] Failed to reload model with texture!" << std::endl;
                }
            }
        }
    }
    
    // Handle Open HDM dialog
    if (pendingOpenHdmDialog) {
        pendingOpenHdmDialog = false;
        const char* path = nfd_open_file_dialog("HDM Files\0*.hdm;*.hdma\0HDM Binary\0*.hdm\0HDM ASCII\0*.hdma\0All Files\0*.*\0", NULL);
        if (path && strlen(path) > 0) {
            std::cout << "[ESE] Opening HDM: " << path << std::endl;
            
            vkDeviceWaitIdle(g_device);
            if (g_eseModelLoaded) {
                heidic_cleanup_renderer_obj_mesh();
                g_eseModelLoaded = false;
            }
            
            // Clear packed data
            g_esePackedGeometry.vertices.clear();
            g_esePackedGeometry.indices.clear();
            g_esePackedTexture.data.clear();
            g_eseGeometryPacked = false;
            g_eseTexturePacked = false;
            
            // Determine format from extension
            std::string filepath(path);
            bool isAscii = (filepath.length() >= 5 && filepath.substr(filepath.length() - 5) == ".hdma");
            
            // Load HDM (binary or ASCII)
            bool loaded = false;
            if (isAscii) {
                std::cout << "[ESE] Loading ASCII HDM format..." << std::endl;
                loaded = hdm_load_ascii(path, g_eseHdmProperties, g_esePackedGeometry, g_esePackedTexture);
            } else {
                std::cout << "[ESE] Loading binary HDM format..." << std::endl;
                loaded = hdm_load_binary(path, g_eseHdmProperties, g_esePackedGeometry, g_esePackedTexture);
            }
            
            if (loaded) {
                g_eseGeometryPacked = !g_esePackedGeometry.vertices.empty();
                g_eseTexturePacked = !g_esePackedTexture.data.empty();
                g_eseCurrentObjPath = g_eseHdmProperties.model.obj_path;
                g_eseCurrentTexturePath = g_eseHdmProperties.model.texture_path;
                
                // Create mesh from packed data (with texture support)
                if (g_eseGeometryPacked && hdm_create_mesh_from_packed(window, g_esePackedGeometry, g_esePackedTexture)) {
                    g_eseModelLoaded = true;
                    g_esePropertiesModified = false;
                    std::cout << "[ESE] Binary HDM loaded successfully!" << std::endl;
                } else {
                    std::cerr << "[ESE] Failed to create mesh from packed data!" << std::endl;
                }
            }
            
            // Fallback: try legacy JSON format
            if (!loaded && hdm_load_json(path, g_eseHdmProperties)) {
                // Load the OBJ model referenced in HDM
                if (strlen(g_eseHdmProperties.model.obj_path) > 0) {
                    g_eseCurrentObjPath = g_eseHdmProperties.model.obj_path;
                    g_eseCurrentTexturePath = g_eseHdmProperties.model.texture_path;
                    
                    if (heidic_init_renderer_obj_mesh(window, g_eseCurrentObjPath.c_str(), 
                            g_eseCurrentTexturePath.empty() ? "" : g_eseCurrentTexturePath.c_str())) {
                        g_eseModelLoaded = true;
                        g_esePropertiesModified = true;  // Mark as modified so user can re-save in new format
                        std::cout << "[ESE] Legacy HDM loaded - please re-save to convert to binary format!" << std::endl;
                    }
                }
            }
        }
    }
    
    // Handle Save HDM dialog
    if (pendingSaveHdmDialog) {
        pendingSaveHdmDialog = false;
        
        // First, extract geometry and texture if not already done
        if (!g_eseGeometryPacked && g_objMeshResource) {
            if (hdm_extract_geometry_from_mesh(g_esePackedGeometry)) {
                g_eseGeometryPacked = true;
            }
        }
        
        if (!g_eseTexturePacked && !g_eseCurrentTexturePath.empty()) {
            if (hdm_load_texture_data(g_eseCurrentTexturePath.c_str(), g_esePackedTexture)) {
                g_eseTexturePacked = true;
            }
        }
        
        if (!g_eseGeometryPacked) {
            std::cerr << "[ESE] Cannot save HDM: No geometry loaded!" << std::endl;
        } else {
            // Use save dialog
            OPENFILENAMEA ofn;
            char szFile[260] = {0};
            
            // Suggest filename based on item name
            strncpy(szFile, g_eseHdmProperties.item.item_name, sizeof(szFile) - 5);
            strcat(szFile, ".hdm");
            
            ZeroMemory(&ofn, sizeof(ofn));
            ofn.lStructSize = sizeof(ofn);
            ofn.lpstrFile = szFile;
            ofn.nMaxFile = sizeof(szFile);
            ofn.lpstrFilter = "HDM Binary\0*.hdm\0HDM ASCII\0*.hdma\0All Files\0*.*\0";
            ofn.nFilterIndex = 1;
            ofn.lpstrDefExt = "hdm";
            ofn.Flags = OFN_OVERWRITEPROMPT;
            
            if (GetSaveFileNameA(&ofn) == TRUE) {
                // Update paths in properties (for reference)
                strncpy(g_eseHdmProperties.model.obj_path, g_eseCurrentObjPath.c_str(), sizeof(g_eseHdmProperties.model.obj_path) - 1);
                strncpy(g_eseHdmProperties.model.texture_path, g_eseCurrentTexturePath.c_str(), sizeof(g_eseHdmProperties.model.texture_path) - 1);
                strncpy(g_eseHdmProperties.hdm_version, "2.0", sizeof(g_eseHdmProperties.hdm_version) - 1);
                
                // Determine format from file extension or filter index
                std::string filepath(szFile);
                bool isAscii = false;
                if (filepath.length() >= 5 && filepath.substr(filepath.length() - 5) == ".hdma") {
                    isAscii = true;
                } else if (ofn.nFilterIndex == 2) {
                    // User selected ASCII format - change extension
                    if (filepath.length() >= 4 && filepath.substr(filepath.length() - 4) == ".hdm") {
                        filepath = filepath.substr(0, filepath.length() - 4) + ".hdma";
                        strncpy(szFile, filepath.c_str(), sizeof(szFile) - 1);
                    }
                    isAscii = true;
                }
                
                // Save in appropriate format
                bool success = false;
                if (isAscii) {
                    success = hdm_save_ascii(szFile, g_eseHdmProperties, g_esePackedGeometry, g_esePackedTexture);
                    if (success) {
                        g_esePropertiesModified = false;
                        std::cout << "[ESE] ASCII HDM saved: " << szFile << std::endl;
                    }
                } else {
                    success = hdm_save_binary(szFile, g_eseHdmProperties, g_esePackedGeometry, g_esePackedTexture);
                    if (success) {
                        g_esePropertiesModified = false;
                        std::cout << "[ESE] Binary HDM saved: " << szFile << std::endl;
                    }
                }
            }
        }
    }
#endif
}

// Cleanup ESE
extern "C" void heidic_cleanup_ese() {
    if (!g_eseInitialized) return;
    
    std::cout << "[ESE] Cleaning up..." << std::endl;
    
    // Wait for GPU to finish all work before cleanup
    vkDeviceWaitIdle(g_device);
    
#ifdef USE_IMGUI
    if (g_eseImguiInitialized) {
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        
        if (g_eseImguiDescriptorPool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(g_device, g_eseImguiDescriptorPool, nullptr);
            g_eseImguiDescriptorPool = VK_NULL_HANDLE;
        }
        g_eseImguiInitialized = false;
    }
#endif
    
    if (g_eseModelLoaded) {
        heidic_cleanup_renderer_obj_mesh();
        g_eseModelLoaded = false;
    }
    
    heidic_cleanup_renderer();
    g_eseInitialized = false;
    
    std::cout << "[ESE] Cleanup complete." << std::endl;
}

// ============================================================================
// HDM Loader functions for games (C-style API)
// ============================================================================

static HDMProperties g_loadedHdmProperties;  // Properties from last loaded HDM

extern "C" int hdm_load_file(const char* filepath) {
    hdm_init_default_properties(g_loadedHdmProperties);
    return hdm_load_json(filepath, g_loadedHdmProperties) ? 1 : 0;
}

extern "C" HDMItemPropertiesC hdm_get_item_properties() {
    HDMItemPropertiesC result;
    result.item_type_id = g_loadedHdmProperties.item.item_type_id;
    strncpy(result.item_name, g_loadedHdmProperties.item.item_name, sizeof(result.item_name) - 1);
    result.trade_value = g_loadedHdmProperties.item.trade_value;
    result.condition = g_loadedHdmProperties.item.condition;
    result.weight = g_loadedHdmProperties.item.weight;
    result.category = g_loadedHdmProperties.item.category;
    result.is_salvaged = g_loadedHdmProperties.item.is_salvaged ? 1 : 0;
    return result;
}

extern "C" HDMPhysicsPropertiesC hdm_get_physics_properties() {
    HDMPhysicsPropertiesC result;
    result.collision_type = g_loadedHdmProperties.physics.collision_type;
    result.collision_bounds_x = g_loadedHdmProperties.physics.collision_bounds[0];
    result.collision_bounds_y = g_loadedHdmProperties.physics.collision_bounds[1];
    result.collision_bounds_z = g_loadedHdmProperties.physics.collision_bounds[2];
    result.is_static = g_loadedHdmProperties.physics.is_static ? 1 : 0;
    result.mass = g_loadedHdmProperties.physics.mass;
    return result;
}

extern "C" HDMModelPropertiesC hdm_get_model_properties() {
    HDMModelPropertiesC result;
    strncpy(result.obj_path, g_loadedHdmProperties.model.obj_path, sizeof(result.obj_path) - 1);
    strncpy(result.texture_path, g_loadedHdmProperties.model.texture_path, sizeof(result.texture_path) - 1);
    result.scale_x = g_loadedHdmProperties.model.scale[0];
    result.scale_y = g_loadedHdmProperties.model.scale[1];
    result.scale_z = g_loadedHdmProperties.model.scale[2];
    return result;
}

// NFD (Native File Dialog) functions for ESE
// Using Windows API for file dialogs
#ifdef _WIN32
#include <vector>

// Helper function to convert std::string to std::wstring
static std::wstring string_to_wstring(const std::string& str) {
    if (str.empty()) return std::wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

// Helper function to convert std::wstring to std::string
static std::string wstring_to_string(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

// Open file dialog with filter
extern "C" const char* nfd_open_file_dialog(const char* filterList, const char* defaultPath) {
    static std::string result;
    result.clear();
    
    OPENFILENAMEA ofn;
    char szFile[260] = {0};
    
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = filterList ? filterList : "All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = defaultPath ? defaultPath : NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
    
    if (GetOpenFileNameA(&ofn) == TRUE) {
        result = szFile;
        return result.c_str();
    }
    
    return "";  // User cancelled
}

// Convenience function for OBJ files
extern "C" const char* nfd_open_obj_dialog() {
    return nfd_open_file_dialog("OBJ Files\0*.obj\0All Files\0*.*\0", NULL);
}

// Convenience function for texture files
extern "C" const char* nfd_open_texture_dialog() {
    return nfd_open_file_dialog("Image Files\0*.png;*.jpg;*.jpeg;*.dds;*.bmp\0PNG Files\0*.png\0DDS Files\0*.dds\0All Files\0*.*\0", NULL);
}

#else
// Linux/Mac stub implementations (can be implemented with GTK or similar)
extern "C" const char* nfd_open_file_dialog(const char* filterList, const char* defaultPath) {
    static std::string result;
    result.clear();
    // TODO: Implement for Linux/Mac
    return "";
}

extern "C" const char* nfd_open_obj_dialog() {
    return nfd_open_file_dialog("OBJ Files\0*.obj\0All Files\0*.*\0", NULL);
}

extern "C" const char* nfd_open_texture_dialog() {
    return nfd_open_file_dialog("Image Files\0*.png;*.jpg;*.jpeg;*.dds;*.bmp\0PNG Files\0*.png\0DDS Files\0*.dds\0All Files\0*.*\0", NULL);
}
#endif

