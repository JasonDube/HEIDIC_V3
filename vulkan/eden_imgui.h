// EDEN ENGINE - ImGui Integration
// Extracted from eden_vulkan_helpers.cpp for modularity

#ifndef EDEN_IMGUI_H
#define EDEN_IMGUI_H

#include <vulkan/vulkan.h>

// Forward declarations
struct GLFWwindow;

#ifdef __cplusplus
extern "C" {
#endif

// Initialize ImGui for Vulkan/GLFW
// Requires: Vulkan renderer must be initialized first (device, render pass, etc.)
// Returns: 1 on success, 0 on failure
int heidic_init_imgui(GLFWwindow* window);

// Start a new ImGui frame
// Call this at the beginning of each frame, before any ImGui calls
void heidic_imgui_new_frame();

// Render ImGui draw data to the command buffer
// Call this after your scene rendering, before ending the render pass
void heidic_imgui_render(VkCommandBuffer commandBuffer);

// Render a simple demo overlay showing FPS and camera position
void heidic_imgui_render_demo_overlay(float fps, float camera_x, float camera_y, float camera_z);

// Cleanup ImGui resources
void heidic_cleanup_imgui();

// Check if ImGui is initialized
int heidic_imgui_is_initialized();

// Check if ImGui wants to capture mouse input (for UI interaction)
int heidic_imgui_want_capture_mouse();

// Check if ImGui wants to capture keyboard input
int heidic_imgui_want_capture_keyboard();

#ifdef __cplusplus
}
#endif

// ============================================================================
// C++ Helper Class
// ============================================================================

#ifdef __cplusplus

namespace eden {

class ImGuiContext {
public:
    // Initialize ImGui
    static bool init(GLFWwindow* window);
    
    // Cleanup ImGui
    static void cleanup();
    
    // Check if initialized
    static bool isInitialized();
    
    // Begin frame
    static void beginFrame();
    
    // End frame and render
    static void endFrame(VkCommandBuffer commandBuffer);
    
    // Check if ImGui wants mouse input
    static bool wantCaptureMouse();
    
    // Check if ImGui wants keyboard input
    static bool wantCaptureKeyboard();
};

// RAII helper for ImGui frame
class ImGuiFrame {
public:
    ImGuiFrame() { ImGuiContext::beginFrame(); }
    void render(VkCommandBuffer cmd) { ImGuiContext::endFrame(cmd); }
};

} // namespace eden

#endif // __cplusplus

#endif // EDEN_IMGUI_H

