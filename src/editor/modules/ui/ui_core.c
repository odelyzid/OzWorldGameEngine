#include "oz/editor/ui.h"
#include "oz/editor/scene.h"
#include "oz/editor/input.h"
#include "oz/editor/rendering.h"
#include "oz/oz_log.h"
#include <stdlib.h>
#include <string.h>

static void ui_setup_main_window(EditorUI* ui);
static void ui_setup_layout(EditorUI* ui);
static void ui_connect_signals(EditorUI* ui);

EditorUI* editor_ui_create(GtkApplication* app) {
    EditorUI* ui = calloc(1, sizeof(EditorUI));
    if (!ui) {
        OZ_ERROR("Failed to allocate EditorUI");
        return NULL;
    }
    
    // Initialize state
    ui->shutting_down = FALSE;
    ui->tick_id = 0;
    ui->redraw_id = 0;
    ui->modern_renderer = NULL;
    ui->gl_tex_brush = 0;
    ui->gl_tex_w = ui->gl_tex_h = 0;
    
    // Debug defaults
    ui->dbg_show_grid = TRUE;
    ui->dbg_show_axes = TRUE;
    ui->dbg_show_fps = TRUE;
    ui->dbg_lighting = FALSE;
    
    // Create main window
    ui->window = gtk_application_window_new(app);
    if (!ui->window) {
        OZ_ERROR("Failed to create main window");
        free(ui);
        return NULL;
    }
    
    // Setup UI components
    ui_setup_main_window(ui);
    ui_setup_layout(ui);
    editor_ui_build_menus(ui, app);
    editor_ui_build_toolbar(ui);
    ui_connect_signals(ui);
    
    OZ_INFO("Editor UI created successfully");
    return ui;
}

void editor_ui_destroy(EditorUI* ui) {
    if (!ui) return;
    
    ui->shutting_down = TRUE;
    
    // Stop timers
    if (ui->tick_id) {
        g_source_remove(ui->tick_id);
        ui->tick_id = 0;
    }
    if (ui->redraw_id) {
        g_source_remove(ui->redraw_id);
        ui->redraw_id = 0;
    }
    
    // Cleanup strings
    free(ui->current_path);
    free(ui->last_texture_path);
    free(ui->last_bundle_path);
    free(ui->last_music_path);
    
    // Destroy window (this will cleanup all child widgets)
    if (ui->window && GTK_IS_WIDGET(ui->window)) {
        gtk_widget_destroy(ui->window);
    }
    
    free(ui);
    OZ_INFO("Editor UI destroyed");
}

void editor_ui_show(EditorUI* ui) {
    if (!ui || !ui->window) return;
    
    gtk_widget_show_all(ui->window);
    
    // Focus the viewport for input
    if (ui->viewport) {
        gtk_widget_set_can_focus(ui->viewport, TRUE);
        gtk_widget_grab_focus(ui->viewport);
    }
}

void editor_ui_set_scene(EditorUI* ui, EditorScene* scene) {
    if (!ui) return;
    ui->scene = scene;
}

void editor_ui_set_input(EditorUI* ui, EditorInput* input) {
    if (!ui) return;
    ui->input = input;
}

static void ui_setup_main_window(EditorUI* ui) {
    gtk_window_set_title(GTK_WINDOW(ui->window), "OzWorld Editor");
    gtk_window_set_default_size(GTK_WINDOW(ui->window), 1024, 768);
    gtk_window_set_position(GTK_WINDOW(ui->window), GTK_WIN_POS_CENTER);
    
    // Set window icon if available
    const char* icon_candidates[] = {
        "gfx/ozworld_icon.png",
        "../gfx/ozworld_icon.png", 
        NULL
    };
    
    for (int i = 0; icon_candidates[i]; i++) {
        if (g_file_test(icon_candidates[i], G_FILE_TEST_IS_REGULAR)) {
            GError* error = NULL;
            if (gtk_window_set_icon_from_file(GTK_WINDOW(ui->window), icon_candidates[i], &error)) {
                break;
            }
            if (error) {
                g_error_free(error);
            }
        }
    }
}

static void ui_setup_layout(EditorUI* ui) {
    // Create main vertical box
    ui->vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(ui->window), ui->vbox);
    
    // Create info label for status/fps display
    ui->info_label = gtk_label_new("");
    gtk_widget_set_halign(ui->info_label, GTK_ALIGN_END);
    gtk_widget_set_valign(ui->info_label, GTK_ALIGN_START);
    gtk_label_set_markup(GTK_LABEL(ui->info_label), 
                        "<span font_family='monospace' size='small'>Ready</span>");
}

static void on_window_delete_event(GtkWidget* widget, GdkEvent* event, EditorUI* ui) {
    (void)widget; (void)event;
    
    // TODO: Check for unsaved changes and prompt user
    ui->shutting_down = TRUE;
    
    GApplication* app = g_application_get_default();
    if (app) {
        g_application_quit(app);
    }
}

static void ui_connect_signals(EditorUI* ui) {
    // Window signals
    g_signal_connect(ui->window, "delete-event", 
                     G_CALLBACK(on_window_delete_event), ui);
}

void editor_ui_update_title(EditorUI* ui, const char* filename) {
    if (!ui || !ui->window) return;
    
    char title[512];
    if (filename && strlen(filename) > 0) {
        // Extract just the filename from the path
        const char* basename = strrchr(filename, '/');
        basename = basename ? basename + 1 : filename;
        snprintf(title, sizeof(title), "%s - OzWorld Editor", basename);
    } else {
        snprintf(title, sizeof(title), "Untitled - OzWorld Editor");
    }
    
    gtk_window_set_title(GTK_WINDOW(ui->window), title);
}

void editor_ui_update_status(EditorUI* ui, const char* message) {
    if (!ui || !ui->info_label || !message) return;
    
    char markup[512];
    snprintf(markup, sizeof(markup), 
             "<span font_family='monospace' size='small'>%s</span>", message);
    gtk_label_set_markup(GTK_LABEL(ui->info_label), markup);
}

void editor_ui_update_camera_info(EditorUI* ui, const OzCamera* camera, float fps) {
    if (!ui || !ui->info_label || !camera) return;
    
    if (!ui->dbg_show_fps) return;
    
    char info[256];
    snprintf(info, sizeof(info),
             "Pos: (%.2f, %.2f, %.2f)  Yaw: %.1f°  Pitch: %.1f°  FPS: %.0f",
             camera->position.x, camera->position.y, camera->position.z,
             camera->yaw * 57.29578f, camera->pitch * 57.29578f, fps);
    
    editor_ui_update_status(ui, info);
}

void editor_ui_toggle_debug_overlay(EditorUI* ui) {
    if (!ui) return;
    
    ui->dbg_show_fps = !ui->dbg_show_fps;
    
    if (!ui->dbg_show_fps) {
        editor_ui_update_status(ui, "Debug overlay disabled");
    }
}

void editor_ui_show_debug_info(EditorUI* ui, gboolean show) {
    if (!ui) return;
    
    ui->dbg_show_fps = show;
    ui->dbg_show_grid = show;
    ui->dbg_show_axes = show;
    
    if (!show) {
        editor_ui_update_status(ui, "Ready");
    }
}
