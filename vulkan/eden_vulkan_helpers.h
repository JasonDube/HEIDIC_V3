// EDEN ENGINE Vulkan Helper Functions Header

#ifndef EDEN_VULKAN_HELPERS_H
#define EDEN_VULKAN_HELPERS_H

#include "../stdlib/vulkan.h"
#include "../stdlib/glfw.h"
#include "../stdlib/math.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Configure GLFW for Vulkan (call this before creating the window)
void heidic_glfw_vulkan_hints();

// Initialize a basic renderer (returns 1 on success, 0 on failure)
// This function handles all Vulkan setup internally
int heidic_init_renderer(GLFWwindow* window);

// Render a frame (renders a spinning triangle)
void heidic_render_frame(GLFWwindow* window);

// Cleanup renderer resources
void heidic_cleanup_renderer();

// Sleep for milliseconds (to prevent CPU spinning)
void heidic_sleep_ms(uint32_t milliseconds);

// Cube rendering functions
int heidic_init_renderer_cube(GLFWwindow* window);
void heidic_render_frame_cube(GLFWwindow* window);
void heidic_cleanup_renderer_cube();

// ImGui functions
int heidic_init_imgui(GLFWwindow* window);
void heidic_imgui_new_frame();
void heidic_imgui_render(VkCommandBuffer commandBuffer);
void heidic_cleanup_imgui();
// Helper to render a simple demo overlay (FPS, camera info, etc.)
void heidic_imgui_render_demo_overlay(float fps, float camera_x, float camera_y, float camera_z);

#ifdef __cplusplus
}
#endif

#endif // EDEN_VULKAN_HELPERS_H

