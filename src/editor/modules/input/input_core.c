#include "oz/editor/input.h"
#include "oz/editor/ui.h"
#include "oz/editor/scene.h"
#include "oz/oz_log.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

static void input_update_key_states(EditorInput* input);
static void input_handle_camera_movement(EditorInput* input, float dt);
static void input_handle_selection_picking(EditorInput* input, double x, double y);
static void input_handle_transform_gizmo(EditorInput* input, double x, double y);
static bool input_oz_key_query(enum OzKey key);

// Global pointer for OzKey compatibility
static EditorInput* g_current_input = NULL;

EditorInput* editor_input_create(EditorUI* ui, EditorScene* scene) {
    EditorInput* input = calloc(1, sizeof(EditorInput));
    if (!input) {
        OZ_ERROR("Failed to allocate EditorInput");
        return NULL;
    }
    
    // Initialize state
    memset(input->keys, 0, sizeof(input->keys));
    input->mouse_look_active = false;
    input->last_mouse_x = input->last_mouse_y = 0.0;
    input->mouse_sensitivity = 0.003f; // radians per pixel
    
    input->dragging = false;
    input->drag_last_x = input->drag_last_y = 0.0;
    
    input->gizmo_dragging = false;
    input->gizmo_start_x = input->gizmo_start_y = 0.0;
    
    input->ui = ui;
    input->scene = scene;
    
    // Set global pointer for OzKey compatibility
    g_current_input = input;
    
    OZ_INFO("Editor input system created");
    return input;
}

void editor_input_destroy(EditorInput* input) {
    if (!input) return;
    
    if (g_current_input == input) {
        g_current_input = NULL;
    }
    
    free(input);
    OZ_INFO("Editor input system destroyed");
}

void editor_input_reset(EditorInput* input) {
    if (!input) return;
    
    memset(input->keys, 0, sizeof(input->keys));
    input->mouse_look_active = false;
    input->dragging = false;
    input->gizmo_dragging = false;
}

// Key state queries
bool editor_input_is_key_down(EditorInput* input, enum OzKey key) {
    if (!input) return false;
    
    switch (key) {
        case OZ_KEY_W: return input->keys['W'] || input->keys['w'];
        case OZ_KEY_A: return input->keys['A'] || input->keys['a'];
        case OZ_KEY_S: return input->keys['S'] || input->keys['s'];
        case OZ_KEY_D: return input->keys['D'] || input->keys['d'];
        case OZ_KEY_Q: return input->keys['Q'] || input->keys['q'] || 
                              input->keys['C'] || input->keys['c'];
        case OZ_KEY_E: return input->keys['E'] || input->keys['e'] || 
                              input->keys[' '];
        case OZ_KEY_LEFT: return input->keys[GDK_KEY_Left & 0xFF];
        case OZ_KEY_RIGHT: return input->keys[GDK_KEY_Right & 0xFF];
        case OZ_KEY_UP: return input->keys[GDK_KEY_Up & 0xFF];
        case OZ_KEY_DOWN: return input->keys[GDK_KEY_Down & 0xFF];
        default: return false;
    }
}

// Compatibility function for oz_camera_update_*
static bool input_oz_key_query(enum OzKey key) {
    return editor_input_is_key_down(g_current_input, key);
}

bool editor_input_is_mouse_look_active(EditorInput* input) {
    return input ? input->mouse_look_active : false;
}

void editor_input_get_mouse_delta(EditorInput* input, float* dx, float* dy) {
    if (!input || !dx || !dy) return;
    
    // Mouse delta is calculated in motion event handler
    *dx = 0.0f; // TODO: Store delta between frames
    *dy = 0.0f;
}

void editor_input_get_mouse_position(EditorInput* input, double* x, double* y) {
    if (!input || !x || !y) return;
    
    *x = input->last_mouse_x;
    *y = input->last_mouse_y;
}

// GTK Event Handlers
gboolean editor_input_on_key_press(GtkWidget* widget, GdkEventKey* event, EditorInput* input) {
    (void)widget;
    
    if (!input || !event) return FALSE;
    
    guint key = event->keyval;
    if (key < 256) {
        input->keys[key] = true;
    }
    
    // Handle special shortcuts
    editor_input_handle_shortcut(input, key, event->state);
    
    return FALSE; // Allow other handlers to process
}

gboolean editor_input_on_key_release(GtkWidget* widget, GdkEventKey* event, EditorInput* input) {
    (void)widget;
    
    if (!input || !event) return FALSE;
    
    guint key = event->keyval;
    if (key < 256) {
        input->keys[key] = false;
    }
    
    return FALSE;
}

gboolean editor_input_on_button_press(GtkWidget* widget, GdkEventButton* event, EditorInput* input) {
    if (!input || !event) return FALSE;
    
    editor_input_ensure_viewport_focus(input);
    
    if (event->button == 1) { // Left mouse button
        bool shift_held = (event->state & GDK_SHIFT_MASK) != 0;
        
        input->last_mouse_x = event->x;
        input->last_mouse_y = event->y;
        
        if (shift_held) {
            // Start dragging for transform
            input->dragging = true;
            input->drag_last_x = event->x;
            input->drag_last_y = event->y;
        } else {
            // Check for gizmo interaction first
            if (input->scene && (input->scene->selected_brush_index >= 0 || 
                                input->scene->selected_object_index >= 0)) {
                // Start gizmo transform
                input->gizmo_dragging = true;
                input->gizmo_start_x = event->x;
                input->gizmo_start_y = event->y;
                
                if (input->scene->selected_brush_index >= 0) {
                    input->scene->gizmo_start_brush = 
                        input->scene->map.brushes[input->scene->selected_brush_index];
                }
                
                editor_input_set_cursor(input, "crosshair");
            } else {
                // Start mouse look
                input->mouse_look_active = true;
                editor_input_set_cursor(input, "none");
            }
        }
        
        // Handle selection picking
        input_handle_selection_picking(input, event->x, event->y);
    }
    
    return TRUE;
}

gboolean editor_input_on_button_release(GtkWidget* widget, GdkEventButton* event, EditorInput* input) {
    (void)widget;
    
    if (!input || !event) return FALSE;
    
    if (event->button == 1) { // Left mouse button
        if (input->mouse_look_active) {
            input->mouse_look_active = false;
            editor_input_reset_cursor(input);
        }
        
        if (input->dragging) {
            input->dragging = false;
        }
        
        if (input->gizmo_dragging) {
            input->gizmo_dragging = false;
            editor_input_reset_cursor(input);
        }
    }
    
    return TRUE;
}

gboolean editor_input_on_motion_notify(GtkWidget* widget, GdkEventMotion* event, EditorInput* input) {
    (void)widget;
    
    if (!input || !event) return FALSE;
    
    double dx = event->x - input->last_mouse_x;
    double dy = event->y - input->last_mouse_y;
    input->last_mouse_x = event->x;
    input->last_mouse_y = event->y;
    
    if (input->mouse_look_active && input->scene) {
        // Update camera rotation
        input->scene->camera.yaw -= (float)(dx * input->mouse_sensitivity);
        input->scene->camera.pitch -= (float)(dy * input->mouse_sensitivity);
        
        // Clamp pitch
        const float max_pitch = 1.55334306f; // ~89 degrees
        if (input->scene->camera.pitch > max_pitch) {
            input->scene->camera.pitch = max_pitch;
        }
        if (input->scene->camera.pitch < -max_pitch) {
            input->scene->camera.pitch = -max_pitch;
        }
    } else if (input->gizmo_dragging && input->scene) {
        // Handle gizmo transform
        editor_input_process_transform(input, event->x, event->y);
    } else if (input->dragging && input->scene) {
        // Handle simple object dragging
        if (input->scene->selected_brush_index >= 0) {
            // Move selected brush
            OzBrush* brush = &input->scene->map.brushes[input->scene->selected_brush_index];
            float scale = 0.01f; // pixels to world units
            
            if (brush->type == OZ_BRUSH_BOX) {
                brush->as.box.center.x += (float)(dx * scale);
                brush->as.box.center.y -= (float)(dy * scale); // Flip Y
            } else if (brush->type == OZ_BRUSH_CYLINDER) {
                brush->as.cyl.center.x += (float)(dx * scale);
                brush->as.cyl.center.y -= (float)(dy * scale);
            }
        } else if (input->scene->selected_object_index >= 0) {
            // Move selected object
            EditorObject* obj = &input->scene->objects[input->scene->selected_object_index];
            float scale = 0.01f;
            
            switch (obj->type) {
                case OBJ_ZONE:
                    obj->as.zone.center[0] += dx * scale;
                    obj->as.zone.center[1] -= dy * scale;
                    break;
                case OBJ_PICKUP:
                    obj->as.pickup.position[0] += dx * scale;
                    obj->as.pickup.position[1] -= dy * scale;
                    break;
                case OBJ_PLAYER_START:
                    obj->as.pstart.position[0] += dx * scale;
                    obj->as.pstart.position[1] -= dy * scale;
                    break;
            }
        }
    } else {
        // Handle hover detection for gizmo axis highlighting
        // TODO: Implement gizmo hover detection
    }
    
    return TRUE;
}

gboolean editor_input_on_scroll(GtkWidget* widget, GdkEventScroll* event, EditorInput* input) {
    (void)widget;
    
    if (!input || !event || !input->scene) return FALSE;
    
    // Handle camera zoom/movement speed
    float delta = 0.0f;
    if (event->direction == GDK_SCROLL_UP) {
        delta = 1.0f;
    } else if (event->direction == GDK_SCROLL_DOWN) {
        delta = -1.0f;
    }
    
    if (delta != 0.0f) {
        // Adjust camera move speed
        input->scene->camera.moveSpeed *= (1.0f + delta * 0.1f);
        if (input->scene->camera.moveSpeed < 0.1f) {
            input->scene->camera.moveSpeed = 0.1f;
        }
        if (input->scene->camera.moveSpeed > 20.0f) {
            input->scene->camera.moveSpeed = 20.0f;
        }
    }
    
    return TRUE;
}

gboolean editor_input_on_focus_out(GtkWidget* widget, GdkEvent* event, EditorInput* input) {
    (void)widget; (void)event;
    
    if (!input) return FALSE;
    
    // Clear all input state when losing focus
    editor_input_clear_all_keys(input);
    input->mouse_look_active = false;
    input->dragging = false;
    input->gizmo_dragging = false;
    
    return FALSE;
}

// Input processing
void editor_input_update(EditorInput* input, float dt) {
    if (!input) return;
    
    input_handle_camera_movement(input, dt);
}

void editor_input_process_camera_movement(EditorInput* input, float dt) {
    input_handle_camera_movement(input, dt);
}

static void input_handle_camera_movement(EditorInput* input, float dt) {
    if (!input || !input->scene) return;
    
    // Update camera using the OzKey query function
    input->scene->last_inst_speed = 
        oz_camera_update_freemove(&input->scene->camera, dt, input_oz_key_query);
}

void editor_input_process_selection(EditorInput* input, double x, double y) {
    input_handle_selection_picking(input, x, y);
}

static void input_handle_selection_picking(EditorInput* input, double x, double y) {
    if (!input || !input->scene || !input->ui) return;
    
    // TODO: Implement proper 3D picking
    // For now, just cycle through objects on click
    (void)x; (void)y;
    
    if (input->scene->obj_count > 0) {
        int next_index = (input->scene->selected_object_index + 1) % (int)input->scene->obj_count;
        editor_scene_select_object(input->scene, next_index);
    } else if (input->scene->map.count > 0) {
        int next_index = (input->scene->selected_brush_index + 1) % (int)input->scene->map.count;
        editor_scene_select_brush(input->scene, next_index);
    }
}

void editor_input_process_transform(EditorInput* input, double x, double y) {
    if (!input || !input->scene) return;
    
    double dx = x - input->gizmo_start_x;
    double dy = y - input->gizmo_start_y;
    
    // Determine dominant axis if not already set
    int axis = input->scene->gizmo_axis;
    if (axis < 0) {
        axis = (fabs(dx) > fabs(dy)) ? 0 : 1; // X or Y
    }
    
    float scale = 0.01f;
    
    if (input->scene->selected_brush_index >= 0) {
        OzBrush* brush = &input->scene->map.brushes[input->scene->selected_brush_index];
        
        if (input->scene->gizmo_translate && brush->type == OZ_BRUSH_BOX) {
            *brush = input->scene->gizmo_start_brush;
            
            if (axis == 0) { // X axis
                brush->as.box.center.x += (float)(dx * scale);
            } else if (axis == 1) { // Y axis
                brush->as.box.center.y -= (float)(dy * scale);
            } else { // Z axis
                brush->as.box.center.z += (float)(-dy * scale);
            }
        }
        // TODO: Handle scale and rotation transforms
    }
}

// Utility functions
void editor_input_ensure_viewport_focus(EditorInput* input) {
    if (!input || !input->ui || !input->ui->viewport) return;
    
    gtk_widget_set_can_focus(input->ui->viewport, TRUE);
    gtk_widget_grab_focus(input->ui->viewport);
}

void editor_input_set_cursor(EditorInput* input, const char* cursor_name) {
    if (!input || !input->ui || !input->ui->viewport || !cursor_name) return;
    
    GdkWindow* window = gtk_widget_get_window(input->ui->viewport);
    if (window) {
        GdkCursor* cursor = gdk_cursor_new_from_name(gdk_display_get_default(), cursor_name);
        gdk_window_set_cursor(window, cursor);
        if (cursor) g_object_unref(cursor);
    }
}

void editor_input_reset_cursor(EditorInput* input) {
    if (!input || !input->ui || !input->ui->viewport) return;
    
    GdkWindow* window = gtk_widget_get_window(input->ui->viewport);
    if (window) {
        gdk_window_set_cursor(window, NULL);
    }
}

void editor_input_clear_all_keys(EditorInput* input) {
    if (!input) return;
    
    memset(input->keys, 0, sizeof(input->keys));
}

void editor_input_handle_shortcut(EditorInput* input, guint keyval, guint state) {
    if (!input || !input->scene) return;
    
    bool ctrl = (state & GDK_CONTROL_MASK) != 0;
    bool shift = (state & GDK_SHIFT_MASK) != 0;
    
    (void)shift; // Suppress unused warning
    
    switch (keyval) {
        case GDK_KEY_l:
        case GDK_KEY_L:
            // Toggle lighting
            if (input->ui) {
                input->ui->dbg_lighting = !input->ui->dbg_lighting;
                editor_ui_update_status(input->ui, 
                    input->ui->dbg_lighting ? "Lighting enabled" : "Lighting disabled");
            }
            break;
            
        case GDK_KEY_g:
        case GDK_KEY_G:
            // Toggle grid
            if (input->ui) {
                input->ui->dbg_show_grid = !input->ui->dbg_show_grid;
                editor_ui_update_status(input->ui, 
                    input->ui->dbg_show_grid ? "Grid enabled" : "Grid disabled");
            }
            break;
            
        case GDK_KEY_f:
        case GDK_KEY_F:
            // Focus on selection
            if (!ctrl) {
                editor_scene_focus_on_selection(input->scene);
                if (input->ui) {
                    editor_ui_update_status(input->ui, "Focused on selection");
                }
            }
            break;
            
        case GDK_KEY_Delete:
        case GDK_KEY_BackSpace:
            // Delete selection
            if (input->scene->selected_brush_index >= 0) {
                editor_scene_delete_selected_brush(input->scene);
            } else if (input->scene->selected_object_index >= 0) {
                editor_scene_delete_selected_object(input->scene);
            }
            break;
    }
}
