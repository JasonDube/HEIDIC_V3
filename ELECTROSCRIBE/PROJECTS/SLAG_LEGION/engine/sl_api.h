// SLAG LEGION C API - For HEIDIC Integration
// This provides the extern "C" functions that HEIDIC's slag_legion.hd calls

#ifndef SL_API_H
#define SL_API_H

#ifdef __cplusplus
extern "C" {
#endif

// Forward declaration
struct GLFWwindow;

// ============================================================================
// Core Engine
// ============================================================================

// Initialize GLFW with Vulkan hints
void sl_glfw_vulkan_hints();

// Create a fullscreen window
GLFWwindow* sl_create_fullscreen_window(const char* title);

// Initialize the FPS renderer
int sl_init_renderer_fps(GLFWwindow* window);

// Render a frame
void sl_render_fps(GLFWwindow* window, float cam_x, float cam_y, float cam_z, float yaw, float pitch);

// Cleanup
void sl_cleanup_renderer_fps();

// Sleep
void sl_sleep_ms(int milliseconds);

// ============================================================================
// Input
// ============================================================================

double sl_get_cursor_x(GLFWwindow* window);
double sl_get_cursor_y(GLFWwindow* window);
void sl_hide_cursor(GLFWwindow* window);

// ============================================================================
// Math
// ============================================================================

float sl_sin(float radians);
float sl_cos(float radians);
float sl_sqrt(float value);
float sl_convert_degrees_to_radians(float degrees);

// ============================================================================
// Cube/Object System
// ============================================================================

typedef struct {
    float x, y, z;
} SLVec3;

SLVec3 sl_get_cube_position(int cube_index);
void sl_set_cube_position(int cube_index, float x, float y, float z);
void sl_set_cube_rotation(int cube_index, float yaw_degrees);
void sl_set_cube_color(int cube_index, float r, float g, float b);
void sl_restore_cube_color(int cube_index);
float sl_get_cube_size(int cube_index);
SLVec3 sl_get_cube_size_xyz(int cube_index);

// ============================================================================
// Raycasting
// ============================================================================

// Raycast from screen center against ALL selectable objects, returns object index or -1
int sl_raycast_from_center();

int sl_raycast_cube_hit_center(GLFWwindow* window, float cx, float cy, float cz, float sx, float sy, float sz);
SLVec3 sl_raycast_cube_hit_point_center(GLFWwindow* window, float cx, float cy, float cz, float sx, float sy, float sz);
float sl_raycast_downward_distance(float x, float y, float z);
int sl_raycast_downward_big_cube(float x, float y, float z);
SLVec3 sl_get_center_ray_origin(GLFWwindow* window);
SLVec3 sl_get_center_ray_dir(GLFWwindow* window);
void sl_draw_line(float x1, float y1, float z1, float x2, float y2, float z2, float r, float g, float b);

// ============================================================================
// Vehicle System
// ============================================================================

void sl_attach_cube_to_vehicle(int cube_index, float local_x, float local_y, float local_z);
void sl_detach_cube_from_vehicle(int cube_index);
int sl_is_cube_attached(int cube_index);
void sl_update_attached_cubes(float vx, float vy, float vz, float yaw, float size_y);

// ============================================================================
// Item Properties
// ============================================================================

int sl_get_item_type_id(int cube_index);
const char* sl_get_item_name(int cube_index);

// ============================================================================
// Neuroshell UI
// ============================================================================

int sl_neuroshell_init(GLFWwindow* window);
void sl_neuroshell_update(float delta_time);
void sl_neuroshell_shutdown();
int sl_neuroshell_create_image(float x, float y, float w, float h, const char* texture_path);
int sl_neuroshell_create_panel(float x, float y, float w, float h);
void sl_neuroshell_set_visible(int element_id, int visible);
void sl_neuroshell_set_depth(int element_id, float depth);
void sl_neuroshell_set_color(int element_id, float r, float g, float b, float a);
int sl_neuroshell_load_font(const char* font_path);
int sl_neuroshell_create_text(float x, float y, const char* text, int font_id, float char_w, float char_h);
void sl_neuroshell_set_text_string(int element_id, const char* text);

// ============================================================================
// Small Block System (snapping to big blocks)
// ============================================================================

int sl_is_small_block(int index);
void sl_drop_small_block(int index, float x, float y, float z);
int sl_find_big_block_under(float x, float y, float z);
int sl_get_snapped_to_block(int small_index);

// ============================================================================
// Crosshair - handled by NEUROSHELL (2D UI layer), not this API
// ============================================================================

#ifdef __cplusplus
}
#endif

#endif // SL_API_H

