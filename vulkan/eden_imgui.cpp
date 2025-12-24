// EDEN ENGINE - ImGui Integration Implementation
// Extracted from eden_vulkan_helpers.cpp for modularity

#include "eden_imgui.h"
#include <iostream>

// External Vulkan state (defined in eden_vulkan_helpers.cpp)
extern VkInstance g_instance;
extern VkPhysicalDevice g_physicalDevice;
extern VkDevice g_device;
extern VkRenderPass g_renderPass;
extern VkQueue g_graphicsQueue;
extern uint32_t g_graphicsQueueFamilyIndex;
extern uint32_t g_swapchainImageCount;

#ifdef USE_IMGUI
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"

// ImGui state
static bool g_imguiInitialized = false;
static VkDescriptorPool g_imguiDescriptorPool = VK_NULL_HANDLE;

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
    init_info.ApiVersion = VK_API_VERSION_1_0;
    init_info.Instance = g_instance;
    init_info.PhysicalDevice = g_physicalDevice;
    init_info.Device = g_device;
    init_info.QueueFamily = g_graphicsQueueFamilyIndex;
    init_info.Queue = g_graphicsQueue;
    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.DescriptorPool = g_imguiDescriptorPool;
    init_info.PipelineInfoMain.RenderPass = g_renderPass;
    init_info.PipelineInfoMain.Subpass = 0;
    init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.MinImageCount = g_swapchainImageCount;
    init_info.ImageCount = g_swapchainImageCount;
    init_info.Allocator = nullptr;
    init_info.CheckVkResultFn = nullptr;
    
    ImGui_ImplVulkan_Init(&init_info);
    
    g_imguiInitialized = true;
    std::cout << "[EDEN] ImGui initialized successfully!" << std::endl;
    return 1;
}

extern "C" void heidic_imgui_new_frame() {
    if (!g_imguiInitialized) return;
    
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

extern "C" void heidic_imgui_render(VkCommandBuffer commandBuffer) {
    if (!g_imguiInitialized) return;
    
    ImGui::Render();
    ImDrawData* draw_data = ImGui::GetDrawData();
    if (draw_data && draw_data->CmdListsCount > 0) {
        ImGui_ImplVulkan_RenderDrawData(draw_data, commandBuffer);
    }
}

extern "C" void heidic_imgui_render_demo_overlay(float fps, float camera_x, float camera_y, float camera_z) {
    if (!g_imguiInitialized) return;
    
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

extern "C" int heidic_imgui_is_initialized() {
    return g_imguiInitialized ? 1 : 0;
}

extern "C" int heidic_imgui_want_capture_mouse() {
    if (!g_imguiInitialized) return 0;
    return ImGui::GetIO().WantCaptureMouse ? 1 : 0;
}

extern "C" int heidic_imgui_want_capture_keyboard() {
    if (!g_imguiInitialized) return 0;
    return ImGui::GetIO().WantCaptureKeyboard ? 1 : 0;
}

// C++ helpers
namespace eden {

bool ImGuiContext::init(GLFWwindow* window) {
    return heidic_init_imgui(window) == 1;
}

void ImGuiContext::cleanup() {
    heidic_cleanup_imgui();
}

bool ImGuiContext::isInitialized() {
    return g_imguiInitialized;
}

void ImGuiContext::beginFrame() {
    heidic_imgui_new_frame();
}

void ImGuiContext::endFrame(VkCommandBuffer commandBuffer) {
    heidic_imgui_render(commandBuffer);
}

bool ImGuiContext::wantCaptureMouse() {
    return heidic_imgui_want_capture_mouse() == 1;
}

bool ImGuiContext::wantCaptureKeyboard() {
    return heidic_imgui_want_capture_keyboard() == 1;
}

} // namespace eden

#else // !USE_IMGUI

// Stub functions when ImGui is not available
extern "C" int heidic_init_imgui(GLFWwindow* window) {
    (void)window;
    std::cerr << "[EDEN] WARNING: ImGui not compiled in (USE_IMGUI not defined)" << std::endl;
    return 0;
}

extern "C" void heidic_imgui_new_frame() {}
extern "C" void heidic_imgui_render(VkCommandBuffer commandBuffer) { (void)commandBuffer; }
extern "C" void heidic_imgui_render_demo_overlay(float fps, float camera_x, float camera_y, float camera_z) {
    (void)fps; (void)camera_x; (void)camera_y; (void)camera_z;
}
extern "C" void heidic_cleanup_imgui() {}
extern "C" int heidic_imgui_is_initialized() { return 0; }
extern "C" int heidic_imgui_want_capture_mouse() { return 0; }
extern "C" int heidic_imgui_want_capture_keyboard() { return 0; }

namespace eden {
bool ImGuiContext::init(GLFWwindow*) { return false; }
void ImGuiContext::cleanup() {}
bool ImGuiContext::isInitialized() { return false; }
void ImGuiContext::beginFrame() {}
void ImGuiContext::endFrame(VkCommandBuffer) {}
bool ImGuiContext::wantCaptureMouse() { return false; }
bool ImGuiContext::wantCaptureKeyboard() { return false; }
} // namespace eden

#endif // USE_IMGUI

