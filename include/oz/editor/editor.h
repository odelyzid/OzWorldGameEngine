#ifndef OZ_EDITOR_EDITOR_H
#define OZ_EDITOR_EDITOR_H

#include "oz/editor/ui.h"
#include "oz/editor/scene.h"
#include "oz/editor/input.h"
#include "oz/editor/rendering.h"
#include <gtk/gtk.h>

#ifdef __cplusplus
extern "C" {
#endif

// Main Editor Application
typedef struct OzEditor {
    // Core modules
    EditorUI* ui;
    EditorScene* scene;
    EditorInput* input;
    EditorRenderer* renderer;
    
    // Application state
    GtkApplication* app;
    bool running;
    bool initialized;
    
    // Timing
    double last_update_time;
    double target_fps;
    
    // Configuration
    char* config_path;
    char* assets_path;
    char* projects_path;
} OzEditor;

// Editor Lifecycle
OzEditor* oz_editor_create(void);
void oz_editor_destroy(OzEditor* editor);
bool oz_editor_initialize(OzEditor* editor, int argc, char** argv);
void oz_editor_shutdown(OzEditor* editor);

// Application Control
int oz_editor_run(OzEditor* editor);
void oz_editor_quit(OzEditor* editor);
void oz_editor_restart(OzEditor* editor);

// Main Loop
void oz_editor_update(OzEditor* editor, float dt);
void oz_editor_render(OzEditor* editor);
void oz_editor_present(OzEditor* editor);

// File Operations
bool oz_editor_new_scene(OzEditor* editor);
bool oz_editor_open_scene(OzEditor* editor, const char* filename);
bool oz_editor_save_scene(OzEditor* editor, const char* filename);
bool oz_editor_save_scene_as(OzEditor* editor);
bool oz_editor_export_bsp(OzEditor* editor, const char* filename);

// Configuration
bool oz_editor_load_config(OzEditor* editor);
bool oz_editor_save_config(OzEditor* editor);
void oz_editor_reset_config(OzEditor* editor);

// Utility Functions
const char* oz_editor_get_version(void);
const char* oz_editor_get_build_info(void);
bool oz_editor_is_scene_modified(OzEditor* editor);
void oz_editor_set_scene_modified(OzEditor* editor, bool modified);

// Global Editor Instance (for signal handlers)
extern OzEditor* g_editor_instance;

// Application Entry Points
void oz_editor_on_activate(GtkApplication* app, gpointer user_data);
void oz_editor_on_startup(GtkApplication* app, gpointer user_data);
void oz_editor_on_shutdown(GtkApplication* app, gpointer user_data);

#ifdef __cplusplus
}
#endif

#endif // OZ_EDITOR_EDITOR_H
