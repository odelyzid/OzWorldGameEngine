#ifndef OZ_EDITOR_UI_H
#define OZ_EDITOR_UI_H

#include <gtk/gtk.h>
#include "oz/oz_bsp.h"
#include "oz/oz_camera.h"

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
typedef struct OzGLModern OzGLModern;
typedef struct EditorScene EditorScene;
typedef struct EditorInput EditorInput;

// UI State and Management
typedef struct EditorUI {
    // GTK Components
    GtkWidget* window;
    GtkWidget* menubar;
    GtkWidget* toolbar;
    GtkWidget* vbox;
    GtkWidget* gl_area;
    GtkWidget* viewport;          // GLArea or DrawingArea
    GtkWidget* overlay;           // overlay container for HUD
    GtkWidget* info_label;        // top-right debug info
    
    // Rendering backend
    OzGLModern* modern_renderer;  // NULL if using immediate mode or software
    unsigned int gl_tex_brush;    // 0 if not created (legacy immediate mode)
    int gl_tex_w, gl_tex_h;
    
    // State management
    gboolean shutting_down;
    unsigned int tick_id;
    unsigned int redraw_id;       // for software viewport redraw timer
    
    // Module references
    EditorScene* scene;
    EditorInput* input;
    
    // Current file
    char* current_path;
    
    // Debug toggles
    gboolean dbg_show_grid;
    gboolean dbg_show_axes;
    gboolean dbg_show_fps;
    gboolean dbg_lighting;        // toggle via 'L'
    
    // Dialog windows
    GtkWidget* debug_window;
    GtkWidget* obj_browser_win;
    GtkWidget* obj_scene_list;
    
    // Last imported assets (paths)
    char* last_texture_path;
    char* last_bundle_path;
    char* last_music_path;
} EditorUI;

// UI Initialization and Management
EditorUI* editor_ui_create(GtkApplication* app);
void editor_ui_destroy(EditorUI* ui);
void editor_ui_show(EditorUI* ui);
void editor_ui_set_scene(EditorUI* ui, EditorScene* scene);
void editor_ui_set_input(EditorUI* ui, EditorInput* input);

// Viewport Management
void editor_ui_setup_gl_viewport(EditorUI* ui);
void editor_ui_setup_software_viewport(EditorUI* ui);
void editor_ui_switch_to_software(EditorUI* ui);
void editor_ui_connect_input_events(EditorUI* ui);

// Menu and Toolbar
void editor_ui_build_menus(EditorUI* ui, GtkApplication* app);
void editor_ui_build_toolbar(EditorUI* ui);
void editor_ui_update_title(EditorUI* ui, const char* filename);

// Status and Info
void editor_ui_update_status(EditorUI* ui, const char* message);
void editor_ui_update_camera_info(EditorUI* ui, const OzCamera* camera, float fps);

// Dialogs
void editor_ui_show_about_dialog(EditorUI* ui);
void editor_ui_show_preferences_dialog(EditorUI* ui);
void editor_ui_show_brush_dialog(EditorUI* ui, const char* type);

// Debug and Development
void editor_ui_toggle_debug_overlay(EditorUI* ui);
void editor_ui_show_debug_info(EditorUI* ui, gboolean show);

#ifdef __cplusplus
}
#endif

#endif // OZ_EDITOR_UI_H
