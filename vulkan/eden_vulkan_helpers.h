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

// Get current command buffer (for UI rendering within render pass)
// Returns current command buffer if render pass is active, VK_NULL_HANDLE otherwise
// Note: Only valid during heidic_render_frame() execution
void* heidic_get_current_command_buffer();

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
// Initialize renderer using HEIDIC Resource pointers (for hot-reload support)
// textureResource can be nullptr for no texture
int heidic_init_renderer_obj_mesh_with_resources(GLFWwindow* window, void* meshResource, void* textureResource);
// Render OBJ mesh
void heidic_render_obj_mesh(GLFWwindow* window);
// Cleanup OBJ mesh renderer
void heidic_cleanup_renderer_obj_mesh();

// FPS Camera renderer (for FPS camera test with floor cube)
// Initialize renderer and create programmatic floor cube
int heidic_init_renderer_fps(GLFWwindow* window);
// Render floor cube with FPS camera (camera_pos_x/y/z, camera_yaw/pitch in degrees)
void heidic_render_fps(GLFWwindow* window, float camera_pos_x, float camera_pos_y, float camera_pos_z, float camera_yaw, float camera_pitch);
// Cleanup FPS renderer
void heidic_cleanup_renderer_fps();

// GLFW mouse helper functions (wrappers for pointer-based GLFW functions)
// Get cursor X position
double heidic_get_cursor_x(GLFWwindow* window);
// Get cursor Y position
double heidic_get_cursor_y(GLFWwindow* window);
// Hide cursor (set to disabled mode)
void heidic_hide_cursor(GLFWwindow* window);

// Raycast functions (for FPS camera pickup system)
// Raycast from screen center (crosshair) against a cube - returns 1 if hit, 0 if miss
int heidic_raycast_cube_hit_center(GLFWwindow* window, float cubeX, float cubeY, float cubeZ, float cubeSx, float cubeSy, float cubeSz);
// Get hit point from raycast (call after heidic_raycast_cube_hit_center returns 1) - returns world-space hit position
Vec3 heidic_raycast_cube_hit_point_center(GLFWwindow* window, float cubeX, float cubeY, float cubeZ, float cubeSx, float cubeSy, float cubeSz);
// Get cube position (for HEIDIC to read) - returns Vec3
Vec3 heidic_get_cube_position(int cube_index);
// Set cube position (for HEIDIC to update picked-up cube)
void heidic_set_cube_position(int cube_index, float x, float y, float z);
// Set cube color (for visual feedback when selected/picked up) - bright white when selected
void heidic_set_cube_color(int cube_index, float r, float g, float b);
// Restore cube to original color
void heidic_restore_cube_color(int cube_index);
// Cast ray downward from position and return distance to floor (or -1 if no hit)
float heidic_raycast_downward_distance(float x, float y, float z);
// Cast ray downward from position and return index of big cube hit (or -1 if no hit or hit small cube)
// Returns cube index if ray hits a big cube (size >= 1.0), -1 otherwise
int heidic_raycast_downward_big_cube(float x, float y, float z);
// Get cube size (1.0 for big, 0.5 for small)
float heidic_get_cube_size(int cube_index);
// Get ray origin and direction from screen center (for debug visualization)
Vec3 heidic_get_center_ray_origin(GLFWwindow* window);
Vec3 heidic_get_center_ray_dir(GLFWwindow* window);
// Draw a debug line (placeholder - not yet implemented for FPS renderer)
void heidic_draw_line(float x1, float y1, float z1, float x2, float y2, float z2, float r, float g, float b);

// UI Window Manager functions (optional - for game interface windows)
// These functions are only available if UI windows are enabled in project config
bool ui_manager_init();
bool ui_manager_is_enabled();
void ui_manager_update();
void ui_manager_render();
void ui_manager_shutdown();

#ifdef __cplusplus
}
#endif

#endif // EDEN_VULKAN_HELPERS_H

