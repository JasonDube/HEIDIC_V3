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

// Ball rendering functions (for bouncing_balls test case)
int heidic_init_renderer_balls(GLFWwindow* window);
// positions: float[ball_count * 3], sizes: float[ball_count]
void heidic_render_balls(GLFWwindow* window, int32_t ball_count, float* positions, float* sizes);
void heidic_cleanup_renderer_balls();

// ImGui functions
int heidic_init_imgui(GLFWwindow* window);
void heidic_imgui_new_frame();
void heidic_imgui_render(VkCommandBuffer commandBuffer);
void heidic_cleanup_imgui();
// Helper to render a simple demo overlay (FPS, camera info, etc.)
void heidic_imgui_render_demo_overlay(float fps, float camera_x, float camera_y, float camera_z);

// DDS texture quad rendering (for testing DDS loader)
// Initialize renderer and load DDS texture from path (relative to project directory)
int heidic_init_renderer_dds_quad(GLFWwindow* window, const char* ddsPath);
// Render fullscreen quad with loaded DDS texture
void heidic_render_dds_quad(GLFWwindow* window);
// Cleanup DDS quad renderer
void heidic_cleanup_renderer_dds_quad();

// PNG texture quad rendering (for testing PNG loader)
// Initialize renderer and load PNG texture from path (relative to project directory)
int heidic_init_renderer_png_quad(GLFWwindow* window, const char* pngPath);
// Render fullscreen quad with loaded PNG texture
void heidic_render_png_quad(GLFWwindow* window);
// Cleanup PNG quad renderer
void heidic_cleanup_renderer_png_quad();

// TextureResource test renderer (uses TextureResource class to load DDS or PNG automatically)
// Initialize renderer and load texture using TextureResource (auto-detects DDS/PNG)
int heidic_init_renderer_texture_quad(GLFWwindow* window, const char* texturePath);
// Render fullscreen quad with loaded texture
void heidic_render_texture_quad(GLFWwindow* window);
// Cleanup TextureResource quad renderer
void heidic_cleanup_renderer_texture_quad();

// OBJ Mesh renderer (uses MeshResource class to load OBJ files and render 3D meshes)
// Initialize renderer and load OBJ mesh using MeshResource
// texturePath can be nullptr for no texture, or path to DDS/PNG texture file
int heidic_init_renderer_obj_mesh(GLFWwindow* window, const char* objPath, const char* texturePath);
// Render OBJ mesh
void heidic_render_obj_mesh(GLFWwindow* window);
// Cleanup OBJ mesh renderer
void heidic_cleanup_renderer_obj_mesh();

#ifdef __cplusplus
}
#endif

#endif // EDEN_VULKAN_HELPERS_H

