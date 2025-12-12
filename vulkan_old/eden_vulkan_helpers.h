#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "../stdlib/math.h"
#include <string>

extern "C" {
    // Window/Input Wrappers (to avoid signature conflicts in generated C++)
    int heidic_glfw_init();
    void heidic_glfw_terminate();
    GLFWwindow* heidic_create_window(int width, int height, const char* title);
    void heidic_destroy_window(GLFWwindow* window);
    void heidic_set_window_should_close(GLFWwindow* window, int value);
    int heidic_get_key(GLFWwindow* window, int key);
    void heidic_set_video_mode(int windowed); // 0 = fullscreen, 1 = windowed

    // Helpers
    void heidic_glfw_vulkan_hints();
    int heidic_init_renderer(GLFWwindow* window);
    void heidic_cleanup_renderer();
    int heidic_window_should_close(GLFWwindow* window);
    void heidic_poll_events();
    int heidic_is_key_pressed(GLFWwindow* window, int key);
    int heidic_is_mouse_button_pressed(GLFWwindow* window, int button);

    // Frame Control
    void heidic_begin_frame();
    void heidic_end_frame();
    
    // Drawing
    void heidic_draw_cube(float x, float y, float z, float rx, float ry, float rz, float sx, float sy, float sz);
    void heidic_draw_cube_grey(float x, float y, float z, float rx, float ry, float rz, float sx, float sy, float sz);
    void heidic_draw_cube_blue(float x, float y, float z, float rx, float ry, float rz, float sx, float sy, float sz);
    void heidic_draw_cube_colored(float x, float y, float z, float rx, float ry, float rz, float sx, float sy, float sz, float r, float g, float b);
    void heidic_flush_colored_cubes();  // Flush current batch (draw immediately and clear)
    void heidic_draw_line(float x1, float y1, float z1, float x2, float y2, float z2, float r, float g, float b);
    void heidic_draw_model_origin(float x, float y, float z, float rx, float ry, float rz, float length);
    void heidic_update_camera(float px, float py, float pz, float rx, float ry, float rz);
    void heidic_update_camera_with_far(float px, float py, float pz, float rx, float ry, float rz, float far_plane);
    Camera heidic_create_camera(Vec3 pos, Vec3 rot, float clip_near, float clip_far);
    void heidic_update_camera_from_struct(Camera camera);
    
    // Mesh Loading
    int heidic_load_ascii_model(const char* filename);
    void heidic_draw_mesh(int mesh_id, float x, float y, float z, float rx, float ry, float rz);

    // ImGui
    void heidic_imgui_init(GLFWwindow* window);
    int heidic_imgui_begin(const char* name);  // Returns 1 if window is open, 0 if closed or already begun
    void heidic_imgui_begin_docked_with(const char* name, const char* dock_with_name);  // Begin window docked with another window
    void heidic_imgui_end();
    void heidic_imgui_text(const char* text);
    void heidic_imgui_text_float(const char* label, float value);
    const char* heidic_format_cube_name(int index);
    const char* heidic_format_cube_name_with_index(int index);  // Format with 5 digits (cube_00001)  // Format cube name like "cube_001"
    bool heidic_imgui_drag_float3(const char* label, Vec3* v, float speed);
    Vec3 heidic_imgui_drag_float3_val(const char* label, Vec3 v, float speed);
    float heidic_imgui_drag_float(const char* label, float v, float speed);
    
    // ImGui Menu Bar
    int heidic_imgui_begin_main_menu_bar();
    void heidic_imgui_end_main_menu_bar();
    
    // ImGui Dockspace Functions
    void heidic_imgui_setup_dockspace();  // Call once per frame after NewFrame() to enable docking
    void heidic_imgui_load_layout(const char* ini_path);  // Load layout from .ini file (use NULL or "" for default)
    void heidic_imgui_save_layout(const char* ini_path);  // Save layout to .ini file (use NULL or "" for default)
    
    int heidic_imgui_begin_menu(const char* label);
    void heidic_imgui_end_menu();
    int heidic_imgui_menu_item(const char* label);
    void heidic_imgui_separator();
    int heidic_imgui_button(const char* label);
    int heidic_imgui_collapsing_header(const char* label);  // Collapsible header (returns 1 if expanded)  // Button widget
    void heidic_imgui_same_line();  // Put next widget on same line
    void heidic_imgui_push_id(int id);  // Push unique ID for widget grouping
    void heidic_imgui_pop_id();  // Pop ID
    int heidic_imgui_selectable(const char* label);  // Selectable text (clickable)
    int heidic_imgui_is_item_clicked();  // Check if last item was clicked
}

// C++ overload for std::string (outside extern "C" since it uses C++ types)
extern "C" {
    int heidic_imgui_is_key_enter_pressed();  // Check if Enter key is pressed
    int heidic_imgui_is_key_escape_pressed();  // Check if Escape key is pressed
    
    
    // File I/O (accepts full file path)
    int heidic_save_level(const char* filepath);
    int heidic_load_level(const char* filepath);
    
    // Native file dialogs
    int heidic_show_save_dialog();  // Shows native save dialog, returns 1 if saved
    int heidic_show_open_dialog();  // Shows native open dialog, returns 1 if loaded
    float heidic_get_fps();
    
    // Vector Operations
    Vec3 heidic_vec3(float x, float y, float z);
    Vec3 heidic_vec3_add(Vec3 a, Vec3 b);
    Vec3 heidic_vec3_sub(Vec3 a, Vec3 b);
    float heidic_vec3_distance(Vec3 a, Vec3 b);
    Vec3 heidic_vec3_mul_scalar(Vec3 v, float s);
    Vec3 heidic_vec_copy(Vec3 src);
    Vec3 heidic_attach_camera_translation(Vec3 player_translation);
    Vec3 heidic_attach_camera_rotation(Vec3 player_rotation);
}

// C++ overloads for std::string (outside extern "C" since they use C++ types)
int heidic_save_level_str(const std::string& level_name);
int heidic_load_level_str(const std::string& level_name);
void heidic_imgui_text_str(const std::string& text);  // C++ overload for std::string
extern "C" void heidic_imgui_text_str_wrapper(const char* text);  // Wrapper for HEIDIC string type (extern "C" for linking)
extern "C" void heidic_imgui_text_bold(const char* text);  // Text in bold
extern "C" void heidic_imgui_text_colored(const char* text, float r, float g, float b, float a);  // Text with color
int heidic_imgui_button_str(const std::string& label);  // C++ overload for std::string (for HEIDIC string type)
extern "C" {
    int heidic_imgui_selectable_str(const char* label);  // extern "C" version matching HEIDIC forward declarations
    int heidic_imgui_selectable_colored(const char* label, float r, float g, float b, float a);  // Selectable with color
    int32_t heidic_imgui_image_button(const char* str_id, int64_t texture_id, float size_x, float size_y, float tint_r, float tint_g, float tint_b, float tint_a);  // Image button for texture previews
    void heidic_set_combination_name_wrapper_str(int combination_id, const char* name);  // extern "C" version matching HEIDIC forward declarations
    const char* heidic_string_to_char_ptr(const char* str);  // Helper: returns str as-is (for HEIDIC string conversion)
}

// C++ overloads (outside extern "C" so they can accept std::string&)
const char* heidic_string_to_char_ptr(const std::string& str);  // C++ overload for HEIDIC string type
int heidic_imgui_selectable_str(const std::string& label);  // C++ overload for HEIDIC string type
void heidic_set_combination_name_wrapper_str(int combination_id, const std::string& name);  // C++ overload for HEIDIC string type

extern "C" {
    // Wrappers for HEIDIC code (accepts const char* to avoid std::string issues)
    int heidic_save_level_str_wrapper(const char* level_name);
    int heidic_load_level_str_wrapper(const char* level_name);
    int heidic_imgui_button_str_wrapper(const char* label);  // Wrapper for HEIDIC string type
}

extern "C" {
    // Math Helpers
    float heidic_convert_degrees_to_radians(float degrees);
    float heidic_convert_radians_to_degrees(float radians);
    float heidic_sin(float radians);
    float heidic_cos(float radians);
    float heidic_atan2(float y, float x);
    float heidic_asin(float value);

    // Utility
    void heidic_sleep_ms(int ms);
    
    // Raycasting
    float heidic_get_mouse_x(GLFWwindow* window);
    float heidic_get_mouse_y(GLFWwindow* window);
    float heidic_get_mouse_scroll_y(GLFWwindow* window); // Get vertical scroll delta
    float heidic_get_mouse_delta_x(GLFWwindow* window); // Get mouse delta X (horizontal movement)
    float heidic_get_mouse_delta_y(GLFWwindow* window); // Get mouse delta Y (vertical movement)
    void heidic_set_cursor_mode(GLFWwindow* window, int mode); // Set cursor mode: 0=normal, 1=hidden, 2=disabled(captured)
    Vec3 heidic_get_mouse_ray_origin(GLFWwindow* window);
    Vec3 heidic_get_mouse_ray_dir(GLFWwindow* window);
    int heidic_raycast_cube_hit(GLFWwindow* window, float cubeX, float cubeY, float cubeZ, float cubeSx, float cubeSy, float cubeSz);
    Vec3 heidic_raycast_cube_hit_point(GLFWwindow* window, float cubeX, float cubeY, float cubeZ, float cubeSx, float cubeSy, float cubeSz);
    void heidic_draw_cube_wireframe(float x, float y, float z, float rx, float ry, float rz, float sx, float sy, float sz, float r, float g, float b);
    void heidic_draw_ground_plane(float size, float r, float g, float b);
    int heidic_raycast_ground_hit(float x, float y, float z, float maxDistance);
    Vec3 heidic_raycast_ground_hit_point(float x, float y, float z, float maxDistance);
    
    // Debug: Print raycast info to console
    void heidic_debug_print_ray(GLFWwindow* window);
    // Draw mouse ray for visual debugging
    void heidic_draw_ray(GLFWwindow* window, float length, float r, float g, float b);

    // Gizmos
    Vec3 heidic_gizmo_translate(GLFWwindow* window, float x, float y, float z);
    int heidic_gizmo_is_interacting();
    
    // Dynamic Cube Storage System
    int heidic_create_cube(float x, float y, float z, float sx, float sy, float sz);
    int heidic_create_cube_with_color(float x, float y, float z, float sx, float sy, float sz, float r, float g, float b);
    int heidic_get_cube_count();  // Returns count of active cubes
    int heidic_get_cube_total_count();  // Returns total count (including deleted)
    float heidic_get_cube_x(int index);
    float heidic_get_cube_y(int index);
    float heidic_get_cube_z(int index);
    float heidic_get_cube_sx(int index);
    float heidic_get_cube_sy(int index);
    float heidic_get_cube_sz(int index);
    float heidic_get_cube_r(int index);
    float heidic_get_cube_g(int index);
    float heidic_get_cube_b(int index);
    const char* heidic_get_cube_texture_name(int index);  // Get cube texture name
    int heidic_load_texture_for_rendering(const char* texture_name);  // Load texture and update global texture for rendering
    int heidic_get_cube_active(int index);
    void heidic_set_cube_pos(int index, float x, float y, float z);
    void heidic_set_cube_pos_f(float index, float x, float y, float z);  // Float index version
    void heidic_delete_cube(int index);
    int heidic_find_next_active_cube_index(int start_index);  // Returns -1 if no more
    float heidic_int_to_float(int value);  // Convert i32 to f32
    int heidic_float_to_int(float value);  // Convert f32 to i32
    float heidic_random_float();  // Returns random float between 0.0 and 1.0
    void heidic_draw_cube_colored(float x, float y, float z, float rx, float ry, float rz, float sx, float sy, float sz, float r, float g, float b);
    
    // Combination System
    void heidic_combine_connected_cubes();  // Find and group all connected cubes
    void heidic_combine_connected_cubes_from_selection(int selected_cube_storage_index);  // Combine cubes connected to selected cube (-1 = combine all)
    int heidic_get_cube_combination_id(int cube_index);  // Get combination ID for a cube (-1 if none)
    int heidic_get_combination_cube_count(int combination_id);  // Get number of cubes in combination
    int heidic_get_combination_first_cube(int combination_id);  // Get first cube index in combination
    int heidic_get_combination_next_cube(int cube_index);  // Get next cube in same combination (-1 if none)
    int heidic_get_combination_count();  // Get total number of combinations
    const char* heidic_format_combination_name(int combination_id);  // Format name like "combination_001" (or custom name)
    const char* heidic_get_combination_name_buffer(int combination_id);  // Get name buffer for editing
    void heidic_set_combination_name(int combination_id, const char* name);  // Set custom name for combination
    void heidic_set_combination_name_wrapper(int combination_id, const char* name);  // Wrapper for HEIDIC string type (extern "C")
}

// C++ overload for std::string (outside extern "C" since it uses C++ types)
void heidic_set_combination_name_wrapper_str(int combination_id, const std::string& name);  // C++ overload for HEIDIC string type

extern "C" {
    void heidic_start_editing_combination_name(int combination_id);  // Start editing combination name
    void heidic_stop_editing_combination_name();  // Stop editing combination name
    int heidic_get_editing_combination_id();  // Get which combination is being edited (-1 if none)
    const char* heidic_get_combination_name_edit_buffer();  // Get editable buffer for combination name
    int heidic_imgui_input_text_combination_simple(int combination_id);  // Simple input text for combination (returns 1 on Enter)
    
    // Multi-selection functions
    void heidic_clear_selection();
    void heidic_add_to_selection(int cube_storage_index);
    void heidic_remove_from_selection(int cube_storage_index);
    void heidic_toggle_selection(int cube_storage_index);
    int heidic_is_cube_selected(int cube_storage_index);
    int heidic_get_selection_count();
    int* heidic_get_selected_cube_indices();
    void heidic_combine_selected_cubes();
    
    // Texture swatch functions
    void heidic_load_texture_list();
    int heidic_get_texture_count();
    const char* heidic_get_texture_name(int index);
    const char* heidic_get_selected_texture();
    void heidic_set_selected_texture(const char* texture_name);
    int64_t heidic_get_texture_preview_id(const char* texture_name);  // Get ImGui texture ID for preview
    void heidic_get_texture_preview_size(const char* texture_name, int* width, int* height);  // Get texture size
    int heidic_imgui_input_text_combination_name();  // Input text for editing combination name (returns 1 on Enter)
    int heidic_imgui_should_stop_editing();  // Check if we should stop editing (Escape or click outside)
    void heidic_toggle_combination_expanded(int combination_id);  // Toggle expand/collapse state
    int heidic_is_combination_expanded(int combination_id);  // Get expansion state (1=expanded, 0=collapsed)
}
