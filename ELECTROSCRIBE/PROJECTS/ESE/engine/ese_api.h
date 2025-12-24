// ESE API - C Interface for HEIDIC Integration
// Wraps ESE modules for use from HEIDIC scripts

#ifndef ESE_API_H
#define ESE_API_H

#include <GLFW/glfw3.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Lifecycle
// ============================================================================

int heidic_init_ese(GLFWwindow* window);
void heidic_render_ese(GLFWwindow* window);
void heidic_cleanup_ese();

// ============================================================================
// File Operations
// ============================================================================

int heidic_ese_load_obj(const char* path);
int heidic_ese_load_texture(const char* path);
int heidic_ese_load_hdm(const char* path);
int heidic_ese_save_hdm(const char* path);
int heidic_ese_create_cube();

// Open file dialogs
const char* heidic_ese_open_obj_dialog();
const char* heidic_ese_open_texture_dialog();
const char* heidic_ese_open_hdm_dialog();
const char* heidic_ese_save_hdm_dialog();

// ============================================================================
// Camera Control
// ============================================================================

void heidic_ese_camera_orbit(float dx, float dy);
void heidic_ese_camera_pan(float dx, float dy);
void heidic_ese_camera_zoom(float delta);
void heidic_ese_camera_reset();
void heidic_ese_camera_frame_model();

// Get camera state
float heidic_ese_camera_get_yaw();
float heidic_ese_camera_get_pitch();
float heidic_ese_camera_get_distance();

// ============================================================================
// Selection
// ============================================================================

void heidic_ese_set_selection_mode(int mode);  // 0=none, 1=vertex, 2=edge, 3=face, 4=quad
int heidic_ese_get_selection_mode();

void heidic_ese_clear_selection();
int heidic_ese_get_selected_vertex();
int heidic_ese_get_selected_edge();
int heidic_ese_get_selected_face();
int heidic_ese_get_selected_quad();

// Selection from mouse position
void heidic_ese_pick_at(float screen_x, float screen_y);
void heidic_ese_pick_add_at(float screen_x, float screen_y);  // Ctrl+click

// ============================================================================
// Gizmo Control
// ============================================================================

void heidic_ese_set_gizmo_mode(int mode);  // 0=none, 1=move, 2=scale, 3=rotate
int heidic_ese_get_gizmo_mode();

void heidic_ese_gizmo_start_drag(float screen_x, float screen_y);
void heidic_ese_gizmo_update_drag(float screen_x, float screen_y);
void heidic_ese_gizmo_end_drag();

int heidic_ese_gizmo_is_dragging();
int heidic_ese_gizmo_hit_test(float screen_x, float screen_y);  // Returns axis: 0=X, 1=Y, 2=Z, -1=none

// ============================================================================
// Mesh Operations
// ============================================================================

void heidic_ese_extrude();
void heidic_ese_insert_edge_loop();
void heidic_ese_delete_selected();
void heidic_ese_duplicate_selected();

// ============================================================================
// Undo/Redo
// ============================================================================

void heidic_ese_undo();
void heidic_ese_redo();
int heidic_ese_can_undo();
int heidic_ese_can_redo();

// ============================================================================
// View Options
// ============================================================================

void heidic_ese_toggle_wireframe();
void heidic_ese_toggle_normals();
void heidic_ese_set_wireframe(int enabled);
int heidic_ese_is_wireframe();

// ============================================================================
// Properties (HDM)
// ============================================================================

const char* heidic_ese_get_item_name();
void heidic_ese_set_item_name(const char* name);
int heidic_ese_get_item_category();
void heidic_ese_set_item_category(int category);
float heidic_ese_get_item_mass();
void heidic_ese_set_item_mass(float mass);

// ============================================================================
// Input Processing
// ============================================================================

void heidic_ese_process_input(GLFWwindow* window);
void heidic_ese_on_key(int key, int action, int mods);
void heidic_ese_on_mouse_button(int button, int action, int mods);
void heidic_ese_on_scroll(float delta);
void heidic_ese_on_cursor_pos(float x, float y);

// ============================================================================
// State Queries
// ============================================================================

int heidic_ese_is_initialized();
int heidic_ese_has_model();
int heidic_ese_is_modified();
int heidic_ese_get_vertex_count();
int heidic_ese_get_triangle_count();

#ifdef __cplusplus
}
#endif

#endif // ESE_API_H

