// EDEN ENGINE Vulkan Helper Functions Header
//
// =============================================================================
// MODULAR ARCHITECTURE (December 2024)
// =============================================================================
// This header maintains backward compatibility with existing C/HEIDIC code.
// For new C++ development, prefer using the extracted modules directly:
//
//   #include "vulkan/utils/raycast.h"         // Ray intersection
//   #include "vulkan/platform/file_dialog.h"  // Native dialogs
//   #include "vulkan/formats/hdm_format.h"    // HDM file format
//   #include "vulkan/eden_imgui.h"            // ImGui integration
//   #include "vulkan/renderers/mesh_renderer.h"  // OBJ mesh rendering
//
// ESE editor modules (for 3D mesh editing):
//   #include "ELECTROSCRIBE/PROJECTS/ESE/src/ese_editor.h"
//
// See vulkan/REFACTORING_STATUS.md for full documentation.
// =============================================================================

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

// Create a fullscreen window on the primary monitor
// Returns GLFWwindow* or NULL on failure
GLFWwindow* heidic_create_fullscreen_window(const char* title);

// Create a borderless fullscreen window (windowed mode with no decorations)
// Returns GLFWwindow* or NULL on failure
// If width or height is 0, uses monitor's native resolution
GLFWwindow* heidic_create_borderless_window(int width, int height, const char* title);

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

// Convert f32 to i32 (for type conversions in HEIDIC)
int32_t f32_to_i32(float value);

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
// Set cube rotation (yaw in degrees, for visual rotation around Y axis)
void heidic_set_cube_rotation(int cube_index, float yaw_degrees);
// Set cube color (for visual feedback when selected/picked up) - bright white when selected
void heidic_set_cube_color(int cube_index, float r, float g, float b);
// Restore cube to original color
void heidic_restore_cube_color(int cube_index);

// Vehicle attachment system - for blocks that move with the vehicle
// Attach a cube to the vehicle with a local offset (in vehicle-local space)
void heidic_attach_cube_to_vehicle(int cube_index, float local_x, float local_y, float local_z);
// Detach a cube from the vehicle
void heidic_detach_cube_from_vehicle(int cube_index);
// Check if a cube is attached to the vehicle (returns 1 if attached, 0 if not)
int heidic_is_cube_attached(int cube_index);
// Update all attached cubes to match the vehicle's position and rotation
void heidic_update_attached_cubes(float vehicle_x, float vehicle_y, float vehicle_z, float vehicle_yaw, float vehicle_size_y);

// Item properties system - for scavenging/trading/building gameplay
// Set item properties for a cube (item_type_id, trade_value, condition 0-1, weight, category, is_salvaged 0/1)
void heidic_set_item_properties(int cube_index, int item_type_id, float trade_value, float condition, float weight, int category, int is_salvaged);
// Get item type ID (0 = generic block, 1+ = specific item types)
int heidic_get_item_type_id(int cube_index);
// Get trade value/price (0 = not tradeable)
float heidic_get_item_trade_value(int cube_index);
// Get condition/durability (0.0 to 1.0, 1.0 = perfect)
float heidic_get_item_condition(int cube_index);
// Get weight/mass
float heidic_get_item_weight(int cube_index);
// Get category (0=generic, 1=consumable, 2=part, 3=resource, 4=scrap)
int heidic_get_item_category(int cube_index);
// Check if item is salvaged (returns 1 if salvaged, 0 if not)
int heidic_is_item_salvaged(int cube_index);
// Set item name (string)
void heidic_set_item_name(int cube_index, const char* item_name);
// Get item name (returns pointer to internal string - valid until next call or cube deletion)
const char* heidic_get_item_name(int cube_index);
// Set parent cube index (for hierarchical relationships, -1 = no parent)
void heidic_set_item_parent(int cube_index, int parent_index);
// Get parent cube index (-1 = no parent)
int heidic_get_item_parent(int cube_index);

// Cast ray downward from position and return distance to floor (or -1 if no hit)
float heidic_raycast_downward_distance(float x, float y, float z);
// Cast ray downward from position and return index of big cube hit (or -1 if no hit or hit small cube)
// Returns cube index if ray hits a big cube (size >= 1.0), -1 otherwise
int heidic_raycast_downward_big_cube(float x, float y, float z);
// Get cube size (1.0 for big, 0.5 for small, or average for rectangles)
float heidic_get_cube_size(int cube_index);
// Get cube size per axis (for rectangles)
Vec3 heidic_get_cube_size_xyz(int cube_index);
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

// ESE (Echo Synapse Editor) functions
int heidic_init_ese(GLFWwindow* window);
void heidic_render_ese(GLFWwindow* window);
void heidic_cleanup_ese();

// HDM (HEIDIC Model) format functions for game loading
// Returns item properties from loaded HDM file
typedef struct {
    int item_type_id;
    char item_name[64];
    int trade_value;
    float condition;
    float weight;
    int category;
    int is_salvaged;
} HDMItemPropertiesC;

typedef struct {
    int collision_type;
    float collision_bounds_x;
    float collision_bounds_y;
    float collision_bounds_z;
    int is_static;
    float mass;
} HDMPhysicsPropertiesC;

typedef struct {
    char obj_path[256];
    char texture_path[256];
    float scale_x;
    float scale_y;
    float scale_z;
} HDMModelPropertiesC;

// Load HDM file and get properties (for use in games)
int hdm_load_file(const char* filepath);
HDMItemPropertiesC hdm_get_item_properties();
HDMPhysicsPropertiesC hdm_get_physics_properties();
HDMModelPropertiesC hdm_get_model_properties();

// NFD (Native File Dialog) functions for ESE
// Returns file path or empty string if cancelled
const char* nfd_open_file_dialog(const char* filterList, const char* defaultPath);
const char* nfd_open_obj_dialog();  // Convenience function for OBJ files
const char* nfd_open_texture_dialog();  // Convenience function for texture files

#ifdef __cplusplus
}
#endif

#endif // EDEN_VULKAN_HELPERS_H

