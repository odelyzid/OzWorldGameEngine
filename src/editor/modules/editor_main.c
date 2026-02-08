#include "oz/editor/editor.h"
#include "oz/oz_log.h"
#include <stdlib.h>
#include <sys/time.h>

// Global editor instance for signal handlers
OzEditor* g_editor_instance = NULL;

static double get_current_time(void);
static void editor_setup_default_config(OzEditor* editor);
static gboolean editor_tick_callback(gpointer user_data);

OzEditor* oz_editor_create(void) {
    OzEditor* editor = calloc(1, sizeof(OzEditor));
    if (!editor) {
        OZ_ERROR("Failed to allocate OzEditor");
        return NULL;
    }
    
    // Initialize core state
    editor->app = NULL;
    editor->running = false;
    editor->initialized = false;
    editor->last_update_time = 0.0;
    editor->target_fps = 60.0;
    
    // Initialize module pointers
    editor->ui = NULL;
    editor->scene = NULL;
    editor->input = NULL;
    editor->renderer = NULL;
    
    // Initialize config paths
    editor->config_path = NULL;
    editor->assets_path = NULL;
    editor->projects_path = NULL;
    
    OZ_INFO("OzEditor instance created");
    return editor;
}

void oz_editor_destroy(OzEditor* editor) {
    if (!editor) return;
    
    oz_editor_shutdown(editor);
    
    // Free config paths
    free(editor->config_path);
    free(editor->assets_path);
    free(editor->projects_path);
    
    free(editor);
    
    if (g_editor_instance == editor) {
        g_editor_instance = NULL;
    }
    
    OZ_INFO("OzEditor instance destroyed");
}

bool oz_editor_initialize(OzEditor* editor, int argc, char** argv) {
    (void)argc; (void)argv; // Suppress unused warnings
    if (!editor || editor->initialized) return false;
    
    OZ_INFO("Initializing OzWorld Editor...");
    
    // Store global reference
    g_editor_instance = editor;
    
    // Initialize GTK application
    editor->app = gtk_application_new("com.ozworld.editor", G_APPLICATION_FLAGS_NONE);
    if (!editor->app) {
        OZ_ERROR("Failed to create GTK application");
        return false;
    }
    
    // Connect application signals
    g_signal_connect(editor->app, "activate", G_CALLBACK(oz_editor_on_activate), editor);
    g_signal_connect(editor->app, "startup", G_CALLBACK(oz_editor_on_startup), editor);
    g_signal_connect(editor->app, "shutdown", G_CALLBACK(oz_editor_on_shutdown), editor);
    
    // Create core modules
    editor->scene = editor_scene_create();
    if (!editor->scene) {
        OZ_ERROR("Failed to create scene module");
        return false;
    }
    
    editor->renderer = editor_renderer_create();
    if (!editor->renderer) {
        OZ_ERROR("Failed to create renderer module");
        return false;
    }
    
    editor->initialized = true;
    OZ_INFO("OzWorld Editor initialized successfully");
    
    return true;
}

void oz_editor_shutdown(OzEditor* editor) {
    if (!editor || !editor->initialized) return;
    
    OZ_INFO("Shutting down OzWorld Editor...");
    
    editor->running = false;
    
    // Save configuration
    oz_editor_save_config(editor);
    
    // Cleanup modules in reverse order
    if (editor->input) {
        editor_input_destroy(editor->input);
        editor->input = NULL;
    }
    
    if (editor->ui) {
        editor_ui_destroy(editor->ui);
        editor->ui = NULL;
    }
    
    if (editor->renderer) {
        editor_renderer_destroy(editor->renderer);
        editor->renderer = NULL;
    }
    
    if (editor->scene) {
        editor_scene_destroy(editor->scene);
        editor->scene = NULL;
    }
    
    // Cleanup GTK application
    if (editor->app) {
        g_object_unref(editor->app);
        editor->app = NULL;
    }
    
    editor->initialized = false;
    OZ_INFO("OzWorld Editor shutdown complete");
}

int oz_editor_run(OzEditor* editor) {
    if (!editor || !editor->initialized) {
        OZ_ERROR("Editor not initialized");
        return 1;
    }
    
    OZ_INFO("Starting OzWorld Editor main loop...");
    
    editor->running = true;
    editor->last_update_time = get_current_time();
    
    // Run GTK application
    int result = g_application_run(G_APPLICATION(editor->app), 0, NULL);

    OZ_INFO("OzWorld Editor main loop ended with result: %d", result);
    return result;
}

void oz_editor_quit(OzEditor* editor) {
    if (!editor) return;
    
    OZ_INFO("Quit requested");
    
    editor->running = false;
    
    if (editor->app) {
        g_application_quit(G_APPLICATION(editor->app));
    }
}

void oz_editor_restart(OzEditor* editor) {
    if (!editor) return;
    
    OZ_INFO("Restart requested (TODO: implement)");
    // TODO: Implement restart functionality
    oz_editor_quit(editor);
}

void oz_editor_update(OzEditor* editor, float dt) {
    if (!editor || !editor->running) return;
    
    // Update input system
    if (editor->input) {
        editor_input_update(editor->input, dt);
    }
    
    // Update scene (camera, animation, etc.)
    if (editor->scene) {
        editor_scene_update_camera(editor->scene, dt);
    }
    
    // Update UI info display
    if (editor->ui && editor->scene) {
        editor_ui_update_camera_info(editor->ui, &editor->scene->camera, editor->scene->fps);
    }
}

void oz_editor_render(OzEditor* editor) {
    if (!editor || !editor->renderer || !editor->scene) return;
    
    // Begin frame
    editor_renderer_begin_frame(editor->renderer);
    
    // Clear background
    editor_renderer_clear(editor->renderer, 0.2f, 0.2f, 0.25f, 1.0f);
    
    // Set camera
    editor_renderer_set_camera(editor->renderer, &editor->scene->camera);
    
    // Render scene
    editor_renderer_render_scene(editor->renderer, editor->scene);
    
    // End frame
    editor_renderer_end_frame(editor->renderer);
}

void oz_editor_present(OzEditor* editor) {
    (void)editor;
    // GTK/OpenGL handles presentation automatically
}

// File operations
bool oz_editor_new_scene(OzEditor* editor) {
    if (!editor || !editor->scene) return false;
    
    OZ_INFO("Creating new scene");
    
    // Clear current scene
    editor_scene_clear(editor->scene);
    
    // Update UI
    if (editor->ui) {
        editor_ui_update_title(editor->ui, NULL);
        editor_ui_update_status(editor->ui, "New scene created");
    }
    
    return true;
}

bool oz_editor_open_scene(OzEditor* editor, const char* filename) {
    if (!editor || !editor->scene || !filename) return false;
    
    OZ_INFO("Opening scene: %s", filename);
    
    if (!editor_scene_load(editor->scene, filename)) {
        if (editor->ui) {
            editor_ui_update_status(editor->ui, "Failed to load scene");
        }
        return false;
    }
    
    // Update UI
    if (editor->ui) {
        editor_ui_update_title(editor->ui, filename);
        editor_ui_update_status(editor->ui, "Scene loaded successfully");
    }
    
    return true;
}

bool oz_editor_save_scene(OzEditor* editor, const char* filename) {
    if (!editor || !editor->scene || !filename) return false;
    
    OZ_INFO("Saving scene: %s", filename);
    
    if (!editor_scene_save(editor->scene, filename)) {
        if (editor->ui) {
            editor_ui_update_status(editor->ui, "Failed to save scene");
        }
        return false;
    }
    
    // Update UI
    if (editor->ui) {
        editor_ui_update_title(editor->ui, filename);
        editor_ui_update_status(editor->ui, "Scene saved successfully");
    }
    
    return true;
}

bool oz_editor_save_scene_as(OzEditor* editor) {
    if (!editor || !editor->ui) return false;
    
    // TODO: Show save dialog and call oz_editor_save_scene
    OZ_INFO("Save As dialog (TODO: implement)");
    
    return true;
}

bool oz_editor_export_bsp(OzEditor* editor, const char* filename) {
    if (!editor || !editor->scene || !filename) return false;
    
    return editor_scene_export_bsp(editor->scene, filename);
}

// Configuration
static void editor_setup_default_config(OzEditor* editor) {
    if (!editor) return;
    
    // Setup default paths
    const char* home = g_get_home_dir();
    if (home) {
        editor->config_path = g_build_filename(home, ".config", "ozworld", NULL);
        editor->assets_path = g_build_filename(home, "Documents", "OzWorld", "Assets", NULL);
        editor->projects_path = g_build_filename(home, "Documents", "OzWorld", "Projects", NULL);
    } else {
        editor->config_path = g_strdup("./config");
        editor->assets_path = g_strdup("./assets");
        editor->projects_path = g_strdup("./projects");
    }
    
    // Create directories if they don't exist
    g_mkdir_with_parents(editor->config_path, 0755);
    g_mkdir_with_parents(editor->assets_path, 0755);
    g_mkdir_with_parents(editor->projects_path, 0755);
}

bool oz_editor_load_config(OzEditor* editor) {
    if (!editor || !editor->config_path) return false;
    
    char* config_file = g_build_filename(editor->config_path, "editor.conf", NULL);
    
    // TODO: Implement configuration file loading
    OZ_INFO("Loading configuration from: %s (TODO: implement)", config_file);
    
    g_free(config_file);
    return true;
}

bool oz_editor_save_config(OzEditor* editor) {
    if (!editor || !editor->config_path) return false;
    
    char* config_file = g_build_filename(editor->config_path, "editor.conf", NULL);
    
    // TODO: Implement configuration file saving
    OZ_INFO("Saving configuration to: %s (TODO: implement)", config_file);
    
    g_free(config_file);
    return true;
}

void oz_editor_reset_config(OzEditor* editor) {
    if (!editor) return;
    
    OZ_INFO("Resetting configuration to defaults");
    editor_setup_default_config(editor);
}

// Utility functions
const char* oz_editor_get_version(void) {
    return "0.1.0-dev";
}

const char* oz_editor_get_build_info(void) {
    return "Built with modern OpenGL support";
}

bool oz_editor_is_scene_modified(OzEditor* editor) {
    (void)editor;
    // TODO: Track scene modification state
    return false;
}

void oz_editor_set_scene_modified(OzEditor* editor, bool modified) {
    (void)editor; (void)modified;
    // TODO: Set scene modification state
}

// Application callbacks
void oz_editor_on_activate(GtkApplication* app, gpointer user_data) {
    (void)app;
    OzEditor* editor = (OzEditor*)user_data;

    editor->ui = editor_ui_create(editor->app);
    if (!editor->ui) {
        OZ_ERROR("Failed to create UI module");
    }

    editor->input = editor_input_create(editor->ui, editor->scene);
    if (!editor->input) {
        OZ_ERROR("Failed to create input module");
    }

    if (!editor || !editor->ui) return;

    // Link modules together
    editor_ui_set_scene(editor->ui, editor->scene);
    editor_ui_set_input(editor->ui, editor->input);

    // Connect input events after all modules are set up
    editor_ui_connect_input_events(editor->ui);

    // Setup default configuration
    editor_setup_default_config(editor);

    // Load configuration if available
    oz_editor_load_config(editor);
    // Show the main window
    editor_ui_show(editor->ui);

    OZ_INFO("Application activated");
    
    // Start update timer
    g_timeout_add(16, editor_tick_callback, editor); // ~60 FPS
}

void oz_editor_on_startup(GtkApplication* app, gpointer user_data) {
    (void)app;
    OzEditor* editor = (OzEditor*)user_data;
    
    OZ_INFO("Application startup");
    
    // Additional startup initialization can go here
    (void)editor;
}

void oz_editor_on_shutdown(GtkApplication* app, gpointer user_data) {
    (void)app;
    OzEditor* editor = (OzEditor*)user_data;
    
    OZ_INFO("Application shutdown");
    
    if (editor) {
        editor->running = false;
    }
}

static double get_current_time(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec * 1e-6;
}

static gboolean editor_tick_callback(gpointer user_data) {
    OzEditor* editor = (OzEditor*)user_data;
    
    if (!editor || !editor->running) {
        return G_SOURCE_REMOVE;
    }
    
    double current_time = get_current_time();
    float dt = (float)(current_time - editor->last_update_time);
    editor->last_update_time = current_time;
    
    // Update editor
    oz_editor_update(editor, dt);
    
    // Trigger viewport redraw
    if (editor->ui && editor->ui->viewport) {
        gtk_widget_queue_draw(editor->ui->viewport);
    }
    
    return G_SOURCE_CONTINUE;
}
