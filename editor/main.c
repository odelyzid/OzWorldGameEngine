#include <gtk/gtk.h>
#include "oz/oz_core.h"
#include "oz/oz_log.h"
#include "oz/oz_bsp.h"
#include "oz/oz_assets.h"
#include "oz/oz_tex.h"
#include "oz/oz_bundle.h"
#include "editor.h"
#include "oz/oz_camera.h"
#include <GL/gl.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
static GtkApplication* g_app_singleton = NULL;

// Forward declarations for cross-calls
struct EditorUi;
static void open_object_properties(struct EditorUi* ui, int index);
// Property editors callbacks
typedef struct PropCtx { struct EditorUi* ui; int index; } PropCtx;
static void on_zone_radius_changed(GtkEditable* editable, gpointer user_data) {
    PropCtx* ctx = (PropCtx*)user_data; if (!ctx || !ctx->ui) return;
    const char* t = gtk_entry_get_text(GTK_ENTRY(editable));
    float v = (float)g_ascii_strtod(t ? t : "0", NULL); if (v < 0.01f) v = 0.01f;
    if ((size_t)ctx->index < ctx->ui->obj_count && ctx->ui->objects[ctx->index].type == OBJ_ZONE) {
        ctx->ui->objects[ctx->index].as.zone.radius = v;
        if (ctx->ui->viewport) gtk_widget_queue_draw(ctx->ui->viewport);
    }
}
static void on_pstart_cam_changed(GtkComboBox* combo, gpointer user_data) {
    PropCtx* ctx = (PropCtx*)user_data; if (!ctx || !ctx->ui) return;
    int a = gtk_combo_box_get_active(combo); if (a < 0) a = 0; if (a > 2) a = 2;
    if ((size_t)ctx->index < ctx->ui->obj_count && ctx->ui->objects[ctx->index].type == OBJ_PLAYER_START) {
        ctx->ui->objects[ctx->index].as.pstart.camera_mode = (OzCameraMode)a;
        if (ctx->ui->viewport) gtk_widget_queue_draw(ctx->ui->viewport);
    }
}
static void on_pstart_yaw_changed(GtkEditable* editable, gpointer user_data) {
    PropCtx* ctx = (PropCtx*)user_data; if (!ctx || !ctx->ui) return;
    const char* t = gtk_entry_get_text(GTK_ENTRY(editable));
    float deg = (float)g_ascii_strtod(t ? t : "0", NULL);
    float rad = deg * (float)G_PI / 180.0f;
    if ((size_t)ctx->index < ctx->ui->obj_count && ctx->ui->objects[ctx->index].type == OBJ_PLAYER_START) {
        ctx->ui->objects[ctx->index].as.pstart.yaw = rad;
        if (ctx->ui->viewport) gtk_widget_queue_draw(ctx->ui->viewport);
    }
}

static gboolean env_flag_is_true(const char* name) {
    const char* v = g_getenv(name);
    if (!v) return FALSE;
    return g_ascii_strcasecmp(v, "1") == 0 || g_ascii_strcasecmp(v, "true") == 0 || g_ascii_strcasecmp(v, "yes") == 0;
}

// EditorUi is now in editor/editor.h

static void action_quit(GSimpleAction* action, GVariant* parameter, gpointer user_data) {
    (void)action; (void)parameter;
    EditorUi* ui = (EditorUi*)user_data;
    if (ui) {
        ui->shutting_down = TRUE;
        if (ui->tick_id) { g_source_remove(ui->tick_id); ui->tick_id = 0; }
        if (ui->redraw_id) { g_source_remove(ui->redraw_id); ui->redraw_id = 0; }
        if (ui->window && GTK_IS_WIDGET(ui->window)) {
            gtk_widget_destroy(ui->window);
        }
    }
    GApplication* app = g_application_get_default();
    if (app) g_application_quit(app);
}

static void action_open(GSimpleAction* action, GVariant* parameter, gpointer user_data) {
    (void)action; (void)parameter;
    EditorUi* ui = (EditorUi*)user_data;
    GtkWindow* parent = GTK_WINDOW(ui->window);
    GtkWidget* dialog = gtk_file_chooser_dialog_new("Open Map",
        GTK_WINDOW(ui->window),
        GTK_FILE_CHOOSER_ACTION_OPEN,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Open", GTK_RESPONSE_ACCEPT,
        NULL);
    GtkFileFilter* filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "OzMap files (*.ozone; *.ozmap)");
    gtk_file_filter_add_pattern(filter, "*.ozone");
    gtk_file_filter_add_pattern(filter, "*.ozmap");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char* filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        OZ_INFO("Open file: %s", filename);
        OzMap loaded; oz_map_init(&loaded);
        if (!oz_map_load_text(filename, &loaded)) {
            OZ_ERROR("Failed to load %s", filename);
        } else {
            oz_map_free(&ui->map);
            ui->map = loaded;
            g_free(ui->current_path);
            ui->current_path = g_strdup(filename);
            // request a redraw
            OZ_INFO("Map loaded: %zu brushes from %s", ui->map.count, ui->current_path);
            if (ui->viewport && GTK_IS_WIDGET(ui->viewport)) gtk_widget_queue_draw(ui->viewport);
        }
        g_free(filename);
    }
    gtk_widget_destroy(dialog);
}

static void action_save(GSimpleAction* action, GVariant* parameter, gpointer user_data) {
    (void)action; (void)parameter;
    EditorUi* ui = (EditorUi*)user_data;
    GtkWidget* dialog = gtk_file_chooser_dialog_new("Save Map",
        GTK_WINDOW(ui->window),
        GTK_FILE_CHOOSER_ACTION_SAVE,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Save", GTK_RESPONSE_ACCEPT,
        NULL);
    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog), TRUE);
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char* filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        // Ensure .ozone extension by default (accept legacy .ozmap too)
        char* to_save = NULL;
        if (filename && !(g_str_has_suffix(filename, ".ozone") || g_str_has_suffix(filename, ".ozmap"))) {
            to_save = g_strconcat(filename, ".ozone", NULL);
        }
        const char* path = to_save ? to_save : filename;
        OZ_INFO("Save file: %s", path);
        if (!oz_map_save_text(path, &ui->map)) {
            OZ_ERROR("Failed to save %s", filename);
        } else {
            g_free(ui->current_path);
            ui->current_path = g_strdup(path);
            OZ_INFO("Map saved: %zu brushes to %s", ui->map.count, ui->current_path);
            if (ui->viewport && GTK_IS_WIDGET(ui->viewport)) gtk_widget_queue_draw(ui->viewport);
        }
        if (to_save) g_free(to_save);
        g_free(filename);
    }
    gtk_widget_destroy(dialog);
}

static void choose_and_remember(EditorUi* ui, const char* title, const char* filter_name, const char* pattern, char** last_path_out) {
    GtkWidget* dialog = gtk_file_chooser_dialog_new(title,
        GTK_WINDOW(ui->window), GTK_FILE_CHOOSER_ACTION_OPEN,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Open", GTK_RESPONSE_ACCEPT,
        NULL);
    GtkFileFilter* filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, filter_name);
    gtk_file_filter_add_pattern(filter, pattern);
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char* filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        if (last_path_out) { g_free(*last_path_out); *last_path_out = g_strdup(filename); }
        OZ_INFO("Imported: %s", filename);
        g_free(filename);
    }
    gtk_widget_destroy(dialog);
}

static void on_import_texture(GSimpleAction* a, GVariant* p, gpointer user_data) {
    (void)a; (void)p; EditorUi* ui = (EditorUi*)user_data; if (!ui) return;
    choose_and_remember(ui, "Import Texture", "OzTex files (*.oztex)", "*.oztex", &ui->last_texture_path);
    // If GL texture is not set, upload the imported texture as preview
    if (ui->last_texture_path && ui->gl_area) {
        int w=0,h=0,c=0; unsigned char* px=NULL;
        if (oz_tex_load_oztex(ui->last_texture_path, &w,&h,&c, &px)) {
            GLuint tex = 0; glGenTextures(1, &tex);
            glBindTexture(GL_TEXTURE_2D, tex);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            GLenum fmt = (c == 4) ? GL_RGBA : GL_RGB;
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexImage2D(GL_TEXTURE_2D, 0, (c==4?GL_RGBA:GL_RGB), w, h, 0, fmt, GL_UNSIGNED_BYTE, px);
            ui->gl_tex_brush = tex; ui->gl_tex_w = w; ui->gl_tex_h = h;
            oz_tex_free(px);
            if (ui->viewport) gtk_widget_queue_draw(ui->viewport);
        } else {
            OZ_WARN("Failed to load texture: %s", ui->last_texture_path);
        }
    }
}
static void on_import_bundle(GSimpleAction* a, GVariant* p, gpointer user_data) {
    (void)a; (void)p; EditorUi* ui = (EditorUi*)user_data; if (!ui) return;
    choose_and_remember(ui, "Import Mesh Bundle", "OzBag files (*.ozbag)", "*.ozbag", &ui->last_bundle_path);
    if (ui->last_bundle_path) {
        OzBundle b = {0};
        if (oz_bundle_load(ui->last_bundle_path, &b)) {
            OZ_INFO("Bundle loaded: %zu entries", b.count);
            // For now, just keep it ephemeral and free. Later: populate object browser from entries.
            oz_bundle_free(&b);
        } else {
            OZ_WARN("Failed to load bundle: %s", ui->last_bundle_path);
        }
    }
}
static void on_import_music(GSimpleAction* a, GVariant* p, gpointer user_data) {
    (void)a; (void)p; EditorUi* ui = (EditorUi*)user_data; if (!ui) return;
    choose_and_remember(ui, "Import Music", "OzMux files (*.ozmux)", "*.ozmux", &ui->last_music_path);
    // Stub: actual streaming via SDL_mixer/OpenAL would be handled in runtime app; editor just stores path.
}

static void action_build_common(const char* what) {
    OZ_INFO("Build: %s (stub)", what);
}

static void action_build_map(GSimpleAction* a, GVariant* p, gpointer u) { (void)a; (void)p; (void)u; action_build_common("Map"); }
static void action_build_light(GSimpleAction* a, GVariant* p, gpointer u) { (void)a; (void)p; (void)u; action_build_common("Light"); }
static void action_build_bsp(GSimpleAction* a, GVariant* p, gpointer u) { (void)a; (void)p; (void)u; action_build_common("BSP"); }
static void action_build_brushes(GSimpleAction* a, GVariant* p, gpointer u) { (void)a; (void)p; (void)u; action_build_common("Brushes"); }

static void spawn_process(char const* const argv[]) {
    GError* error = NULL;
    // Ensure SDL respects remote X stability: prefer indirect GL unless user overrides
    gchar** envp = g_get_environ();
    const gchar* disp = g_environ_getenv(envp, "DISPLAY");
    const gboolean is_remote = (disp && disp[0] != ':');
    const gboolean allow_gl = env_flag_is_true("OZ_ALLOW_GL_REMOTE");
    // Always enable driver debug verbosity and GL error reporting in child processes
    envp = g_environ_setenv(envp, "LIBGL_DEBUG", "verbose", TRUE);
    envp = g_environ_setenv(envp, "MESA_DEBUG", "1", TRUE);
    if (is_remote && !allow_gl) {
        envp = g_environ_setenv(envp, "LIBGL_ALWAYS_INDIRECT", "1", TRUE);
        envp = g_environ_setenv(envp, "GDK_GL", "disable", TRUE);
        envp = g_environ_setenv(envp, "OZ_FORCE_SOFTWARE", "1", TRUE);
    }
    gboolean ok = g_spawn_async(NULL, (gchar**)argv, envp, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, &error);
    if (!ok) {
        OZ_ERROR("Failed to launch: %s", error ? error->message : "unknown error");
        if (error) g_error_free(error);
    }
    if (envp) g_strfreev(envp);
}

static void action_launch_editor(GSimpleAction* a, GVariant* p, gpointer u) { (void)a; (void)p; (void)u; const char* argv[] = { "./build/oz_editor", NULL }; spawn_process(argv); }
static void action_launch_game(GSimpleAction* a, GVariant* p, gpointer u) {
    (void)a; (void)p; EditorUi* ui = (EditorUi*)u;
    // Pass a start position/yaw if a PlayerStart exists
    char arg_pos[128] = {0};
    int found = -1;
    if (ui && ui->obj_count) {
        for (size_t i = 0; i < ui->obj_count; ++i) if (ui->objects[i].type == OBJ_PLAYER_START) { found = (int)i; break; }
    }
    const char* argv_default[] = { "./build/oz_demo", NULL };
    char* argv_custom[5] = { (char*)"./build/oz_demo", (char*)"--playerstart", arg_pos, NULL, NULL };
    if (found >= 0) {
        const ObjPlayerStartProps* ps = &ui->objects[found].as.pstart;
        g_snprintf(arg_pos, sizeof(arg_pos), "%g,%g,%g,%g", (double)ps->position[0], (double)ps->position[1], (double)ps->position[2], (double)ps->yaw);
        spawn_process((const char* const*)argv_custom);
    } else {
        spawn_process(argv_default);
    }
}
static void action_launch_server(GSimpleAction* a, GVariant* p, gpointer u) { (void)a; (void)p; (void)u; OZ_WARN("Server launch not implemented"); }

// Dialog helpers
static void dialog_set_entry(GtkDialog* dlg, const char* label, GtkWidget** out_entry, const char* initial) {
    GtkWidget* content = gtk_dialog_get_content_area(dlg);
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget* lab = gtk_label_new(label);
    GtkWidget* ent = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(ent), initial);
    gtk_box_pack_start(GTK_BOX(box), lab, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), ent, TRUE, TRUE, 0);
    gtk_container_add(GTK_CONTAINER(content), box);
    if (out_entry) *out_entry = ent;
}

static void action_bsp_box_dialog(GSimpleAction* a, GVariant* p, gpointer user_data) {
    (void)a; (void)p; EditorUi* ui = (EditorUi*)user_data;
    if (ui->last_box_w <= 0) { ui->last_box_w = 1.0f; ui->last_box_h = 1.0f; ui->last_box_d = 1.0f; }
    char wbuf[32], hbuf[32], dbuf[32];
    g_snprintf(wbuf, sizeof(wbuf), "%.2f", ui->last_box_w);
    g_snprintf(hbuf, sizeof(hbuf), "%.2f", ui->last_box_h);
    g_snprintf(dbuf, sizeof(dbuf), "%.2f", ui->last_box_d);
    GtkWidget* dlg = gtk_dialog_new_with_buttons("Box Brush", GTK_WINDOW(ui->window), GTK_DIALOG_MODAL,
        "_Cancel", GTK_RESPONSE_CANCEL, "_Save", GTK_RESPONSE_ACCEPT, NULL);
    GtkWidget *ew=NULL, *eh=NULL, *ed=NULL;
    dialog_set_entry(GTK_DIALOG(dlg), "Width", &ew, wbuf);
    dialog_set_entry(GTK_DIALOG(dlg), "Height", &eh, hbuf);
    dialog_set_entry(GTK_DIALOG(dlg), "Depth", &ed, dbuf);
    gtk_widget_show_all(dlg);
    if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_ACCEPT) {
        ui->last_box_w = (float)g_ascii_strtod(gtk_entry_get_text(GTK_ENTRY(ew)), NULL);
        ui->last_box_h = (float)g_ascii_strtod(gtk_entry_get_text(GTK_ENTRY(eh)), NULL);
        ui->last_box_d = (float)g_ascii_strtod(gtk_entry_get_text(GTK_ENTRY(ed)), NULL);
        OZ_INFO("Box brush saved: %.2fx%.2fx%.2f", ui->last_box_w, ui->last_box_h, ui->last_box_d);
        // Add a new box brush at origin and redraw
        oz_map_add_box(&ui->map, (OzVec3){0,0,0}, (OzVec3){ui->last_box_w, ui->last_box_h, ui->last_box_d});
        if (ui->viewport && GTK_IS_WIDGET(ui->viewport)) gtk_widget_queue_draw(ui->viewport);
    }
    gtk_widget_destroy(dlg);
}

static void action_bsp_cyl_dialog(GSimpleAction* a, GVariant* p, gpointer user_data) {
    (void)a; (void)p; EditorUi* ui = (EditorUi*)user_data;
    if (ui->last_cyl_rx <= 0) { ui->last_cyl_rx = 0.5f; ui->last_cyl_ry = 0.5f; ui->last_cyl_h = 1.0f; ui->last_cyl_seg = 16; }
    char rxbuf[32], rybuf[32], hbuf[32], sbuf[32];
    g_snprintf(rxbuf, sizeof(rxbuf), "%.2f", ui->last_cyl_rx);
    g_snprintf(rybuf, sizeof(rybuf), "%.2f", ui->last_cyl_ry);
    g_snprintf(hbuf, sizeof(hbuf), "%.2f", ui->last_cyl_h);
    g_snprintf(sbuf, sizeof(sbuf), "%d", ui->last_cyl_seg);
    GtkWidget* dlg = gtk_dialog_new_with_buttons("Cylinder Brush", GTK_WINDOW(ui->window), GTK_DIALOG_MODAL,
        "_Cancel", GTK_RESPONSE_CANCEL, "_Save", GTK_RESPONSE_ACCEPT, NULL);
    GtkWidget *erx=NULL, *ery=NULL, *eh=NULL, *es=NULL;
    dialog_set_entry(GTK_DIALOG(dlg), "Radius X", &erx, rxbuf);
    dialog_set_entry(GTK_DIALOG(dlg), "Radius Y", &ery, rybuf);
    dialog_set_entry(GTK_DIALOG(dlg), "Height", &eh, hbuf);
    dialog_set_entry(GTK_DIALOG(dlg), "Segments", &es, sbuf);
    gtk_widget_show_all(dlg);
    if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_ACCEPT) {
        ui->last_cyl_rx = (float)g_ascii_strtod(gtk_entry_get_text(GTK_ENTRY(erx)), NULL);
        ui->last_cyl_ry = (float)g_ascii_strtod(gtk_entry_get_text(GTK_ENTRY(ery)), NULL);
        ui->last_cyl_h  = (float)g_ascii_strtod(gtk_entry_get_text(GTK_ENTRY(eh)), NULL);
        ui->last_cyl_seg = (int)g_ascii_strtoll(gtk_entry_get_text(GTK_ENTRY(es)), NULL, 10);
        if (ui->last_cyl_seg < 3) ui->last_cyl_seg = 3;
        OZ_INFO("Cylinder brush saved: rx=%.2f ry=%.2f h=%.2f seg=%d", ui->last_cyl_rx, ui->last_cyl_ry, ui->last_cyl_h, ui->last_cyl_seg);
        oz_map_add_cylinder(&ui->map, (OzVec3){0,0,0}, ui->last_cyl_rx, ui->last_cyl_ry, ui->last_cyl_h, ui->last_cyl_seg);
        if (ui->viewport && GTK_IS_WIDGET(ui->viewport)) gtk_widget_queue_draw(ui->viewport);
    }
    gtk_widget_destroy(dlg);
}

// Stubs for CSG/tools
static void action_csg_add(GSimpleAction* a, GVariant* p, gpointer user_data) {
    (void)a; (void)p; EditorUi* ui = (EditorUi*)user_data;
    if (!ui || ui->map.count < 2) { OZ_WARN("CSG Add requires at least 2 brushes"); return; }
    size_t i0 = (size_t)(ui->selected_index >= 0 ? ui->selected_index : 0);
    size_t i1 = (i0 + 1) % ui->map.count;
    OzBrush out;
    if (oz_csg_union(&ui->map.brushes[i0], &ui->map.brushes[i1], &out)) {
        ui->csg_preview = out; ui->has_csg_preview = TRUE;
        OZ_INFO("CSG Union preview ready (not committed)");
        if (ui->viewport) gtk_widget_queue_draw(ui->viewport);
    } else {
        OZ_WARN("CSG Union produced no result");
    }
}

static void action_csg_sub(GSimpleAction* a, GVariant* p, gpointer user_data) {
    (void)a; (void)p; EditorUi* ui = (EditorUi*)user_data;
    if (!ui || ui->map.count < 2) { OZ_WARN("CSG Subtract requires at least 2 brushes"); return; }
    size_t i0 = (size_t)(ui->selected_index >= 0 ? ui->selected_index : 0);
    size_t i1 = (i0 + 1) % ui->map.count;
    OzBrush out;
    if (oz_csg_subtract(&ui->map.brushes[i0], &ui->map.brushes[i1], &out)) {
        ui->csg_preview = out; ui->has_csg_preview = TRUE;
        OZ_INFO("CSG Subtract preview ready (not committed)");
        if (ui->viewport) gtk_widget_queue_draw(ui->viewport);
    } else {
        OZ_WARN("CSG Subtract produced no result");
    }
}

static void action_csg_isect(GSimpleAction* a, GVariant* p, gpointer user_data) {
    (void)a; (void)p; EditorUi* ui = (EditorUi*)user_data;
    if (!ui || ui->map.count < 2) { OZ_WARN("CSG Intersect requires at least 2 brushes"); return; }
    size_t i0 = (size_t)(ui->selected_index >= 0 ? ui->selected_index : 0);
    size_t i1 = (i0 + 1) % ui->map.count;
    OzBrush out;
    if (oz_csg_intersect(&ui->map.brushes[i0], &ui->map.brushes[i1], &out)) {
        ui->csg_preview = out; ui->has_csg_preview = TRUE;
        OZ_INFO("CSG Intersect preview ready (not committed)");
        if (ui->viewport) gtk_widget_queue_draw(ui->viewport);
    } else {
        OZ_WARN("CSG Intersect produced no result");
    }
}
static void action_tool_carve(GSimpleAction* a, GVariant* p, gpointer u) { (void)a;(void)p;(void)u; OZ_INFO("Carve (stub)"); }
static void action_tool_slope(GSimpleAction* a, GVariant* p, gpointer u) { (void)a;(void)p;(void)u; OZ_INFO("Slope (stub)"); }

// Toolbar button shims (match GTK signal signature)
static void toolbar_open_clicked(GtkToolButton* btn, gpointer user_data) { (void)btn; action_open(NULL, NULL, user_data); }
static void toolbar_save_clicked(GtkToolButton* btn, gpointer user_data) { (void)btn; action_save(NULL, NULL, user_data); }

// --- Camera + input state for editor viewport ---
typedef struct EditorInputState {
    gboolean keys[256];
    OzCamera cam;
    double last_tick;
    float last_inst_speed;
    float fps;
    gboolean mouse_look_active;
    double last_mouse_x;
    double last_mouse_y;
    float mouse_sensitivity; // radians per pixel
} EditorInputState;

static EditorInputState* g_editor_state = NULL;
static bool editor_key_down(enum OzKey key) {
    if (!g_editor_state) return false;
    switch (key) {
        case OZ_KEY_W: return g_editor_state->keys['W'] || g_editor_state->keys['w'];
        case OZ_KEY_A: return g_editor_state->keys['A'] || g_editor_state->keys['a'];
        case OZ_KEY_S: return g_editor_state->keys['S'] || g_editor_state->keys['s'];
        case OZ_KEY_D: return g_editor_state->keys['D'] || g_editor_state->keys['d'];
        case OZ_KEY_Q: return g_editor_state->keys['Q'] || g_editor_state->keys['q'] || g_editor_state->keys['C'] || g_editor_state->keys['c'];
        case OZ_KEY_E: return g_editor_state->keys['E'] || g_editor_state->keys['e'] || g_editor_state->keys[' '];
        case OZ_KEY_LEFT: return g_editor_state->keys[GDK_KEY_Left & 0xFF];
        case OZ_KEY_RIGHT: return g_editor_state->keys[GDK_KEY_Right & 0xFF];
        case OZ_KEY_UP: return g_editor_state->keys[GDK_KEY_Up & 0xFF];
        case OZ_KEY_DOWN: return g_editor_state->keys[GDK_KEY_Down & 0xFF];
        default: return false;
    }
}
static void ensure_viewport_focus(EditorUi* ui) {
    if (ui && ui->viewport) {
        gtk_widget_set_can_focus(ui->viewport, TRUE);
        gtk_widget_grab_focus(ui->viewport);
    }
}

static void editor_project_point(EditorUi* ui, float x, float y, float z, float* out_x, float* out_y) {
    int w = gtk_widget_get_allocated_width(ui->viewport);
    int h = gtk_widget_get_allocated_height(ui->viewport);
    float yaw = g_editor_state ? g_editor_state->cam.yaw : 0.0f;
    float pitch = g_editor_state ? g_editor_state->cam.pitch : 0.0f;
    float px = g_editor_state ? g_editor_state->cam.position.x : 0.0f;
    float py = g_editor_state ? g_editor_state->cam.position.y : -5.0f;
    float pz = g_editor_state ? g_editor_state->cam.position.z : -2.5f;
    x -= px; y -= py; z -= pz;
    float cy = cosf(-yaw), sy = sinf(-yaw);
    float x1 =  cy * x + sy * y;
    float y1 = -sy * x + cy * y;
    float z1 = z;
    float cp = cosf(-pitch), sp = sinf(-pitch);
    float x2 = x1;
    float y2 = cp * y1 - sp * z1;
    float z2 = sp * y1 + cp * z1;
    float fov = 60.0f; float f = 1.0f / tanf(fov * (float)G_PI / 360.0f);
    float ndc_x = (x2 * f) / (z2 + 5.0f);
    float ndc_y = (y2 * f) / (z2 + 5.0f);
    *out_x = (float)w * 0.5f + ndc_x * (float)w * 0.5f;
    *out_y = (float)h * 0.5f - ndc_y * (float)h * 0.5f;
}


static gboolean on_key_press(GtkWidget* w, GdkEventKey* e, gpointer user_data) {
    (void)w; EditorInputState* s = (EditorInputState*)user_data;
    guint k = e->keyval;
    if (k >= 'a' && k <= 'z') {
        s->keys[k] = TRUE;
        s->keys[g_ascii_toupper((int)k)] = TRUE;
    } else if (k < 256) {
        s->keys[k] = TRUE;
    } else {
        // Map some special keys into our compact table
        if (k == GDK_KEY_Left || k == GDK_KEY_Right || k == GDK_KEY_Up || k == GDK_KEY_Down) {
            s->keys[k & 0xFF] = TRUE;
        }
    }
    if (env_flag_is_true("OZ_DEBUG_INPUT")) {
        OZ_INFO("key down: %u '%c' state=%u", k, (k>=32 && k<127)?(int)k:'.', (unsigned) e->state);
    }
    return FALSE;
}

static gboolean on_key_release(GtkWidget* w, GdkEventKey* e, gpointer user_data) {
    (void)w; EditorInputState* s = (EditorInputState*)user_data;
    guint k = e->keyval;
    if (k >= 'a' && k <= 'z') {
        s->keys[k] = FALSE;
        s->keys[g_ascii_toupper((int)k)] = FALSE;
    } else if (k < 256) {
        s->keys[k] = FALSE;
    } else {
        if (k == GDK_KEY_Left || k == GDK_KEY_Right || k == GDK_KEY_Up || k == GDK_KEY_Down) {
            s->keys[k & 0xFF] = FALSE;
        }
    }
    // Space and Ctrl need explicit clearing
    if (k == GDK_KEY_space) s->keys[' '] = FALSE;
    if (k == GDK_KEY_Control_L || k == GDK_KEY_Control_R) { s->keys['C'] = FALSE; s->keys['c'] = FALSE; }
    if (env_flag_is_true("OZ_DEBUG_INPUT")) {
        OZ_INFO("key up: %u '%c' state=%u", k, (k>=32 && k<127)?(int)k:'.', (unsigned) e->state);
    }
    return FALSE;
}

// --- Mouse look handlers ---
static gboolean on_button_press(GtkWidget* w, GdkEventButton* e, gpointer user_data) {
    EditorUi* ui = (EditorUi*)g_object_get_data(G_OBJECT(w), "oz_editor_ui");
    EditorInputState* s = (EditorInputState*)user_data;
    ensure_viewport_focus(ui);
    if (e->button == 1) {
        // Object picking (single-click)
        if (ui && ui->obj_count) {
            float best_d2 = 1e9f; int best_idx = -1;
            for (size_t i = 0; i < ui->obj_count; ++i) {
                const EditorObject* o = &ui->objects[i];
                float ox, oy, oz;
                if (o->type == OBJ_ZONE) { ox = o->as.zone.center[0]; oy = o->as.zone.center[1]; oz = o->as.zone.center[2]; }
                else if (o->type == OBJ_PICKUP) { ox = o->as.pickup.position[0]; oy = o->as.pickup.position[1]; oz = o->as.pickup.position[2]; }
                else if (o->type == OBJ_PLAYER_START) { ox = o->as.pstart.position[0]; oy = o->as.pstart.position[1]; oz = o->as.pstart.position[2]; }
                else continue;
                float sx, sy; editor_project_point(ui, ox, oy, oz, &sx, &sy);
                float dx = (float)e->x - sx; float dy = (float)e->y - sy; float d2 = dx*dx + dy*dy;
                if (d2 < best_d2) { best_d2 = d2; best_idx = (int)i; }
            }
            if (best_idx >= 0 && best_d2 < 14.0f*14.0f) {
                ui->selected_object = best_idx;
                open_object_properties(ui, best_idx);
                if (GTK_IS_WIDGET(w)) gtk_widget_queue_draw(w);
                return TRUE;
            }
        }
        // If Shift held, start drag-select/move; else mouse-look or gizmo drag
        gboolean shift = (e->state & GDK_SHIFT_MASK) != 0;
        s->last_mouse_x = e->x;
        s->last_mouse_y = e->y;
        if (shift && ui) {
            ui->dragging = TRUE;
            ui->drag_last_x = e->x;
            ui->drag_last_y = e->y;
            // If nothing selected yet, select nearest brush by simple screen-space hit
            if (ui->selected_index < 0 && ui->map.count > 0) {
                ui->selected_index = 0; // TODO: implement proper picking; select first for now
                OZ_INFO("Selected brush #%d", ui->selected_index);
            }
            // If an object is selected, start dragging it instead; cancel brush drag
            if (ui->selected_object >= 0 && (size_t)ui->selected_object < ui->obj_count) {
                ui->selected_index = -1;
            }
        } else if (ui && (ui->selected_index >= 0 || (ui->selected_object >= 0 && (size_t)ui->selected_object < ui->obj_count))) {
            // Begin gizmo drag for brush or object; axis chosen by current setting or dominant delta (set later)
            ui->gizmo_dragging = TRUE;
            ui->gizmo_start_x = e->x;
            ui->gizmo_start_y = e->y;
            if (ui->selected_index >= 0) ui->gizmo_start_brush = ui->map.brushes[ui->selected_index];
            if (ui->selected_object >= 0 && (size_t)ui->selected_object < ui->obj_count) {
                const EditorObject* o = &ui->objects[ui->selected_object];
                if (o->type == OBJ_ZONE) { ui->gizmo_start_obj_pos[0]=o->as.zone.center[0]; ui->gizmo_start_obj_pos[1]=o->as.zone.center[1]; ui->gizmo_start_obj_pos[2]=o->as.zone.center[2]; }
                else if (o->type == OBJ_PICKUP) { ui->gizmo_start_obj_pos[0]=o->as.pickup.position[0]; ui->gizmo_start_obj_pos[1]=o->as.pickup.position[1]; ui->gizmo_start_obj_pos[2]=o->as.pickup.position[2]; }
                else { ui->gizmo_start_obj_pos[0]=o->as.pstart.position[0]; ui->gizmo_start_obj_pos[1]=o->as.pstart.position[1]; ui->gizmo_start_obj_pos[2]=o->as.pstart.position[2]; }
            }
            s->mouse_look_active = FALSE;
            gdk_window_set_cursor(gtk_widget_get_window(w), gdk_cursor_new_from_name(gdk_display_get_default(), "crosshair"));
        } else {
            s->mouse_look_active = TRUE;
            gdk_window_set_cursor(gtk_widget_get_window(w), gdk_cursor_new_from_name(gdk_display_get_default(), "none"));
        }
    }
    return TRUE;
}

static gboolean on_button_release(GtkWidget* w, GdkEventButton* e, gpointer user_data) {
    EditorUi* ui = (EditorUi*)g_object_get_data(G_OBJECT(w), "oz_editor_ui");
    EditorInputState* s = (EditorInputState*)user_data;
    if (e->button == 1) {
        if (s->mouse_look_active) {
            s->mouse_look_active = FALSE;
            gdk_window_set_cursor(gtk_widget_get_window(w), NULL);
        }
        if (ui && ui->dragging) ui->dragging = FALSE;
        if (ui && ui->gizmo_dragging) { ui->gizmo_dragging = FALSE; gdk_window_set_cursor(gtk_widget_get_window(w), NULL); }
    }
    return TRUE;
}

static gboolean on_motion(GtkWidget* w, GdkEventMotion* e, gpointer user_data) {
    EditorUi* ui = (EditorUi*)g_object_get_data(G_OBJECT(w), "oz_editor_ui");
    EditorInputState* s = (EditorInputState*)user_data;
    double dx = e->x - s->last_mouse_x;
    double dy = e->y - s->last_mouse_y;
    s->last_mouse_x = e->x;
    s->last_mouse_y = e->y;
    if (s->mouse_look_active) {
        // Flip pitch direction and invert yaw X-axis mapping (Unreal-like)
        s->cam.yaw   -= (float)(dx) * s->mouse_sensitivity;
        s->cam.pitch -= (float)(dy) * s->mouse_sensitivity;
        const float kMaxPitch = 1.55334306f;
        if (s->cam.pitch > kMaxPitch) s->cam.pitch = kMaxPitch;
        if (s->cam.pitch < -kMaxPitch) s->cam.pitch = -kMaxPitch;
    } else if (ui && ui->gizmo_dragging && (ui->selected_index >= 0 || (ui->selected_object >= 0 && (size_t)ui->selected_object < ui->obj_count))) {
        // Gizmo drag: move along selected axis; if axis is -1, choose dominant mouse delta X->X axis, Y->Y axis
        double ddx = e->x - ui->gizmo_start_x;
        double ddy = e->y - ui->gizmo_start_y;
        int axis = ui->gizmo_axis;
        if (axis < 0) axis = (fabs(ddx) > fabs(ddy)) ? 0 : 1;
        float scale = 0.01f;
        if (ui->selected_index >= 0 && ui->selected_index < (int)ui->map.count) {
            OzBrush* br = &ui->map.brushes[ui->selected_index];
            if (ui->gizmo_translate) {
                if (br->type == OZ_BRUSH_BOX) {
                    *br = ui->gizmo_start_brush;
                    if (axis == 0) br->as.box.center.x += (float)(ddx * scale);
                    else if (axis == 1) br->as.box.center.y -= (float)(ddy * scale);
                    else br->as.box.center.z += (float)((-ddy) * scale);
                }
            } else if (ui->gizmo_scale) {
                if (br->type == OZ_BRUSH_BOX) {
                    *br = ui->gizmo_start_brush;
                    float delta = (float)((fabs(ddx) > fabs(ddy) ? ddx : -ddy) * scale);
                    if (axis == 0) br->as.box.half.x = fmaxf(0.05f, ui->gizmo_start_brush.as.box.half.x + delta);
                    else if (axis == 1) br->as.box.half.y = fmaxf(0.05f, ui->gizmo_start_brush.as.box.half.y + delta);
                    else br->as.box.half.z = fmaxf(0.05f, ui->gizmo_start_brush.as.box.half.z + delta);
                }
            } else if (ui->gizmo_rotate) {
                if (br->type == OZ_BRUSH_BOX) {
                    *br = ui->gizmo_start_brush;
                    float angle = (float)(ddx * 0.005f); // rotate around Z by horizontal drag
                    br->as.box.rotation_z = ui->gizmo_start_brush.as.box.rotation_z + angle;
                }
            }
        } else if (ui->selected_object >= 0 && (size_t)ui->selected_object < ui->obj_count) {
            EditorObject* o = &ui->objects[ui->selected_object];
            float dx = (float)(ddx * scale), dy = (float)(-ddy * scale);
            float nx=0, ny=0, nz=0;
            if (axis == 0) nx = dx; else if (axis == 1) ny = dy; else nz = dy; // map vertical mouse movement to Z when axis=2
            if (o->type == OBJ_ZONE) { o->as.zone.center[0] = ui->gizmo_start_obj_pos[0] + nx; o->as.zone.center[1] = ui->gizmo_start_obj_pos[1] + ny; o->as.zone.center[2] = ui->gizmo_start_obj_pos[2] + nz; }
            else if (o->type == OBJ_PICKUP) { o->as.pickup.position[0] = ui->gizmo_start_obj_pos[0] + nx; o->as.pickup.position[1] = ui->gizmo_start_obj_pos[1] + ny; o->as.pickup.position[2] = ui->gizmo_start_obj_pos[2] + nz; }
            else { o->as.pstart.position[0] = ui->gizmo_start_obj_pos[0] + nx; o->as.pstart.position[1] = ui->gizmo_start_obj_pos[1] + ny; o->as.pstart.position[2] = ui->gizmo_start_obj_pos[2] + nz; }
        }
        if (GTK_IS_WIDGET(w)) gtk_widget_queue_draw(w);
    } else if (ui && ui->dragging && ui->selected_index >= 0 && ui->selected_index < (int)ui->map.count) {
        // Simple screen-space drag: move selected brush in X/Y plane according to dx/dy, scaled by camera
        OzBrush* br = &ui->map.brushes[ui->selected_index];
        float pixelsPerUnit = 100.0f; // tweakable
        float mx = (float)(dx) / pixelsPerUnit;
        float my = (float)(-dy) / pixelsPerUnit;
        if (br->type == OZ_BRUSH_BOX) {
            br->as.box.center.x += mx;
            br->as.box.center.y += my;
        } else if (br->type == OZ_BRUSH_CYLINDER) {
            br->as.cyl.center.x += mx;
            br->as.cyl.center.y += my;
        }
        if (GTK_IS_WIDGET(w)) gtk_widget_queue_draw(w);
    } else if (ui && ui->dragging && ui->selected_object >= 0 && (size_t)ui->selected_object < ui->obj_count) {
        // Drag selected object in X/Y plane
        EditorObject* o = &ui->objects[ui->selected_object];
        float pixelsPerUnit = 100.0f;
        float mx = (float)(dx) / pixelsPerUnit;
        float my = (float)(-dy) / pixelsPerUnit;
        if (o->type == OBJ_ZONE) { o->as.zone.center[0] += mx; o->as.zone.center[1] += my; }
        else if (o->type == OBJ_PICKUP) { o->as.pickup.position[0] += mx; o->as.pickup.position[1] += my; }
        else if (o->type == OBJ_PLAYER_START) { o->as.pstart.position[0] += mx; o->as.pstart.position[1] += my; }
        if (GTK_IS_WIDGET(w)) gtk_widget_queue_draw(w);
    }
    return TRUE;
}

static gboolean tick_update(gpointer user_data) {
    EditorUi* ui = (EditorUi*)user_data;
    if (ui && ui->shutting_down) return FALSE; // stop timer cleanly
    if (!g_editor_state) return TRUE;
    double now = g_get_monotonic_time() / 1000000.0;
    float dt = (float)(now - g_editor_state->last_tick);
    if (dt < 0.0001f) dt = 0.0001f;
    g_editor_state->last_tick = now;
    // Update using our editor key adapter
    g_editor_state->last_inst_speed = oz_camera_update_freemove(&g_editor_state->cam, dt, editor_key_down);
    // Here, just update fps and request redraw
    g_editor_state->fps = 1.0f / dt;
    if (env_flag_is_true("OZ_DEBUG_LOOP")) {
        OZ_INFO("tick dt=%.3f fps=%.0f pos=(%.2f,%.2f,%.2f) yaw=%.2f pitch=%.2f speed=%.2f",
                dt, g_editor_state->fps,
                g_editor_state->cam.position.x, g_editor_state->cam.position.y, g_editor_state->cam.position.z,
                g_editor_state->cam.yaw, g_editor_state->cam.pitch, g_editor_state->last_inst_speed);
    }
    // Update HUD label
    if (ui && ui->info_label && GTK_IS_LABEL(ui->info_label) && ui->dbg_show_fps) {
        char buf[256];
        snprintf(buf, sizeof(buf), "Pos: (%.2f, %.2f, %.2f)  Yaw: %.1f  Pitch: %.1f  FPS: %.0f",
                 g_editor_state->cam.position.x, g_editor_state->cam.position.y, g_editor_state->cam.position.z,
                 g_editor_state->cam.yaw * 57.29578f, g_editor_state->cam.pitch * 57.29578f, g_editor_state->fps);
        gtk_label_set_text(GTK_LABEL(ui->info_label), buf);
    }
    if (ui && ui->viewport && GTK_IS_WIDGET(ui->viewport)) gtk_widget_queue_draw(ui->viewport);
    return TRUE; // keep timer
}

// GL rendering helpers for the viewport
static gboolean gl_area_render(GtkGLArea* area, GdkGLContext* context, gpointer user_data) {
    (void)area; (void)context;
    EditorUi* ui = (EditorUi*)user_data;
    gtk_gl_area_make_current(area);
    if (gtk_gl_area_get_error(area)) return FALSE;
    int w = gtk_widget_get_allocated_width(GTK_WIDGET(area));
    int h = gtk_widget_get_allocated_height(GTK_WIDGET(area));
    if (env_flag_is_true("OZ_DEBUG_RENDER")) {
        OZ_INFO("gl_area_render w=%d h=%d brushes=%zu", w, h, ui ? ui->map.count : 0);
    }
    glViewport(0, 0, w, h);
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.12f, 0.12f, 0.14f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    float aspect = (float)(h ? w / (float)h : 1.0f);
    float f = 60.0f, n = 0.1f, fa = 100.0f;
    float top = n * tanf(f * 3.14159f/360.0f);
    float right = top * aspect;
    glFrustum(-right, right, -top, top, n, fa);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    if (g_editor_state) {
        // View matrix: inverse of camera transform -> rotate by -pitch, -yaw, then translate by -position
        glRotatef(-g_editor_state->cam.pitch * 57.29578f, 1,0,0);
        glRotatef(-g_editor_state->cam.yaw * 57.29578f, 0,0,1);
        glTranslatef(-g_editor_state->cam.position.x, -g_editor_state->cam.position.y, -g_editor_state->cam.position.z);
    }

    // Grid + axes (toggle)
    if (ui->dbg_show_grid) {
        glDisable(GL_CULL_FACE);
        glColor3f(0.3f, 0.32f, 0.35f);
        const float gridExtent = 20.0f;
        const float gridStep = 1.0f;
        glBegin(GL_LINES);
        for (float v = -gridExtent; v <= gridExtent + 0.001f; v += gridStep) {
            glVertex3f(-gridExtent, v, 0.0f); glVertex3f(gridExtent, v, 0.0f);
            glVertex3f(v, -gridExtent, 0.0f); glVertex3f(v, gridExtent, 0.0f);
        }
        glEnd();
        if (ui->dbg_show_axes) {
            glColor3f(0.6f, 0.6f, 0.7f);
            glBegin(GL_LINES);
            glVertex3f(-gridExtent, 0.0f, 0.0f); glVertex3f(gridExtent, 0.0f, 0.0f); // X axis
            glVertex3f(0.0f, -gridExtent, 0.0f); glVertex3f(0.0f, gridExtent, 0.0f); // Y axis
            glEnd();
        }
    }

    // Draw brushes with a basic checker texture for visibility
    if (ui && ui->gl_tex_brush) { glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D, (GLuint)ui->gl_tex_brush); }
    glColor3f(1.0f,1.0f,1.0f);
    glEnable(GL_CULL_FACE);
    for (size_t i = 0; i < ui->map.count; ++i) {
        const OzBrush* br = &ui->map.brushes[i];
        if (br->type == OZ_BRUSH_BOX) {
            const OzBrushBox* b = &br->as.box;
            float cx=b->center.x, cy=b->center.y, cz=b->center.z;
            float hx=b->half.x, hy=b->half.y, hz=b->half.z;
            float v[8][3] = {
                {cx - hx, cy - hy, cz - hz},
                {cx + hx, cy - hy, cz - hz},
                {cx + hx, cy + hy, cz - hz},
                {cx - hx, cy + hy, cz - hz},
                {cx - hx, cy - hy, cz + hz},
                {cx + hx, cy - hy, cz + hz},
                {cx + hx, cy + hy, cz + hz},
                {cx - hx, cy + hy, cz + hz},
            };
            int faces[6][4] = {{0,1,2,3},{4,5,6,7},{0,1,5,4},{2,3,7,6},{1,2,6,5},{0,3,7,4}};
            float uv[4][2] = {{0,0},{2,0},{2,2},{0,2}};
            glBegin(GL_QUADS);
            for (int fidx = 0; fidx < 6; ++fidx) {
                for (int vi = 0; vi < 4; ++vi) {
                    float* a = v[faces[fidx][vi]]; glTexCoord2f(uv[vi][0], uv[vi][1]); glVertex3f(a[0],a[1],a[2]);
                }
            }
            glEnd();
        } else if (br->type == OZ_BRUSH_CYLINDER) {
            const OzBrushCylinder* c = &br->as.cyl;
            int seg = c->segments > 3 ? c->segments : 16;
            float hz = c->height * 0.5f;
            for (int ring = -1; ring <= 1; ring += 2) {
                float z = c->center.z + (ring < 0 ? -hz : hz);
                glBegin(GL_TRIANGLE_STRIP);
                for (int i2 = 0; i2 <= seg; ++i2) {
                    float a = (float)(i2 % seg) / (float)seg * 6.2831853f;
                    float x = c->center.x + cosf(a)*c->radius_x;
                    float y = c->center.y + sinf(a)*c->radius_y;
                    float u = (float)i2 / (float)seg * 2.0f; float v = ring<0 ? 0.0f : 2.0f;
                    glTexCoord2f(u, v); glVertex3f(x, y, z);
                    glTexCoord2f(u, 2.0f - v); glVertex3f(x, y, z + (ring<0? hz*2.0f: -hz*2.0f));
                }
                glEnd();
            }
        } else if (br->type == OZ_BRUSH_SPHERE) {
            const OzBrushSphere* s = &br->as.sph;
            int seg = s->segments > 6 ? s->segments : 12;
            for (int i2 = 0; i2 < seg; ++i2) {
                float th0 = (float)i2 / (float)seg * 3.1415926f;
                float th1 = (float)(i2+1) / (float)seg * 3.1415926f;
                glBegin(GL_TRIANGLE_STRIP);
                for (int j = 0; j <= seg*2; ++j) {
                    float ph = (float)j / (float)(seg*2) * 6.2831853f;
                    float ct0 = cosf(th0), st0 = sinf(th0);
                    float ct1 = cosf(th1), st1 = sinf(th1);
                    float cph = cosf(ph), sph = sinf(ph);
                    float u = (float)j / (float)(seg*2);
                    float v0 = (float)i2 / (float)seg;
                    float v1 = (float)(i2+1) / (float)seg;
                    glTexCoord2f(u*2.0f, v0*2.0f);
                    glVertex3f(s->center.x + s->radius*st0*cph, s->center.y + s->radius*st0*sph, s->center.z + s->radius*ct0);
                    glTexCoord2f(u*2.0f, v1*2.0f);
                    glVertex3f(s->center.x + s->radius*st1*cph, s->center.y + s->radius*st1*sph, s->center.z + s->radius*ct1);
                }
                glEnd();
            }
        } else if (br->type == OZ_BRUSH_PYRAMID) {
            const OzBrushPyramid* p = &br->as.pyr;
            float cx=p->center.x, cy=p->center.y, cz=p->center.z;
            float hx=p->half_x, hy=p->half_y, h=p->height;
            float top[3] = {cx, cy, cz + h};
            float base[4][3] = {{cx-hx,cy-hy,cz-h*0.0f},{cx+hx,cy-hy,cz},{cx+hx,cy+hy,cz},{cx-hx,cy+hy,cz}};
            glBegin(GL_TRIANGLES);
            for (int f=0; f<4; ++f) {
                int n=(f+1)&3;
                glTexCoord2f(0.5f, 0.0f); glVertex3fv(top);
                glTexCoord2f(0.0f, 1.0f); glVertex3fv(base[f]);
                glTexCoord2f(1.0f, 1.0f); glVertex3fv(base[n]);
            }
            glEnd();
        } else if (br->type == OZ_BRUSH_PLANE) {
            const OzBrushPlane* pl = &br->as.plane;
            // Visualize as square aligned to world axes for now
            float he = pl->half_extent;
            glBegin(GL_QUADS);
            glTexCoord2f(0.0f,0.0f); glVertex3f(pl->center.x - he, pl->center.y - he, pl->center.z);
            glTexCoord2f(2.0f,0.0f); glVertex3f(pl->center.x + he, pl->center.y - he, pl->center.z);
            glTexCoord2f(2.0f,2.0f); glVertex3f(pl->center.x + he, pl->center.y + he, pl->center.z);
            glTexCoord2f(0.0f,2.0f); glVertex3f(pl->center.x - he, pl->center.y + he, pl->center.z);
            glEnd();
        }
    }
    if (ui && ui->gl_tex_brush) { glDisable(GL_TEXTURE_2D); }
    // Draw objects (Zones, Pickups, PlayerStart)
    if (ui && ui->obj_count) {
        glDisable(GL_CULL_FACE);
        for (size_t i = 0; i < ui->obj_count; ++i) {
            const EditorObject* o = &ui->objects[i];
            if (o->type == OBJ_ZONE) {
                // Draw a wire circle at zone center with radius
                glColor3f(0.3f, 0.9f, 0.9f);
                glBegin(GL_LINE_LOOP);
                int seg = 48; float r = o->as.zone.radius; float cx=o->as.zone.center[0], cy=o->as.zone.center[1], cz=o->as.zone.center[2];
                for (int k=0;k<seg;++k){float a = (float)k/(float)seg*6.2831853f; glVertex3f(cx + cosf(a)*r, cy + sinf(a)*r, cz);} glEnd();
            } else if (o->type == OBJ_PICKUP) {
                // Small solid cube
                glColor3f(0.9f, 0.85f, 0.2f);
                float cx=o->as.pickup.position[0], cy=o->as.pickup.position[1], cz=o->as.pickup.position[2];
                float s=0.1f; float v[8][3]={{cx-s,cy-s,cz-s},{cx+s,cy-s,cz-s},{cx+s,cy+s,cz-s},{cx-s,cy+s,cz-s},{cx-s,cy-s,cz+s},{cx+s,cy-s,cz+s},{cx+s,cy+s,cz+s},{cx-s,cy+s,cz+s}}; int faces[6][4]={{0,1,2,3},{4,5,6,7},{0,1,5,4},{2,3,7,6},{1,2,6,5},{0,3,7,4}};
                glBegin(GL_QUADS); for (int f=0;f<6;++f){for(int vi=0;vi<4;++vi){float* a=v[faces[f][vi]]; glVertex3f(a[0],a[1],a[2]);}} glEnd();
            } else if (o->type == OBJ_PLAYER_START) {
                // Arrow glyph: base at position, pointing by yaw
                glColor3f(0.2f, 1.0f, 0.3f);
                float px=o->as.pstart.position[0], py=o->as.pstart.position[1], pz=o->as.pstart.position[2];
                float yaw = o->as.pstart.yaw; float fx=cosf(yaw), fy=sinf(yaw);
                float len = 0.6f; float hx = -fy*0.2f, hy = fx*0.2f;
                glBegin(GL_LINES);
                glVertex3f(px,py,pz); glVertex3f(px+fx*len, py+fy*len, pz);
                glEnd();
                glBegin(GL_TRIANGLES);
                glVertex3f(px+fx*len, py+fy*len, pz);
                glVertex3f(px+fx*(len-0.2f)+hx, py+fy*(len-0.2f)+hy, pz);
                glVertex3f(px+fx*(len-0.2f)-hx, py+fy*(len-0.2f)-hy, pz);
                glEnd();
                // Small visibility cube icon at the base
                glColor3f(1.0f, 0.3f, 0.6f);
                float s=0.12f; float v[8][3]={{px-s,py-s,pz-s},{px+s,py-s,pz-s},{px+s,py+s,pz-s},{px-s,py+s,pz-s},{px-s,py-s,pz+s},{px+s,py-s,pz+s},{px+s,py+s,pz+s},{px-s,py+s,pz+s}}; int faces[6][4]={{0,1,2,3},{4,5,6,7},{0,1,5,4},{2,3,7,6},{1,2,6,5},{0,3,7,4}};
                glBegin(GL_QUADS); for (int f=0;f<6;++f){for(int vi=0;vi<4;++vi){float* a=v[faces[f][vi]]; glVertex3f(a[0],a[1],a[2]);}} glEnd();
            }
        }
    }
    // Wireframe overlay (and selection highlight)
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    for (size_t i = 0; i < ui->map.count; ++i) {
        const OzBrush* br = &ui->map.brushes[i];
        if (br->type == OZ_BRUSH_BOX) {
            const OzBrushBox* b = &br->as.box;
            float cx=b->center.x, cy=b->center.y, cz=b->center.z;
            float hx=b->half.x, hy=b->half.y, hz=b->half.z;
            float v[8][3] = {
                {cx - hx, cy - hy, cz - hz}, {cx + hx, cy - hy, cz - hz}, {cx + hx, cy + hy, cz - hz}, {cx - hx, cy + hy, cz - hz},
                {cx - hx, cy - hy, cz + hz}, {cx + hx, cy - hy, cz + hz}, {cx + hx, cy + hy, cz + hz}, {cx - hx, cy + hy, cz + hz},
            };
            int faces[6][4] = {{0,1,2,3},{4,5,6,7},{0,1,5,4},{2,3,7,6},{1,2,6,5},{0,3,7,4}};
            if ((int)i == ui->selected_index) glColor3f(1.0f, 0.2f, 0.2f); else glColor3f(0.05f,0.05f,0.05f);
            glBegin(GL_QUADS);
            for (int fidx = 0; fidx < 6; ++fidx) {
                for (int vi = 0; vi < 4; ++vi) { float* a = v[faces[fidx][vi]]; glVertex3f(a[0],a[1],a[2]); }
            }
            glEnd();
        }
    }
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    // Draw CSG preview (filled, translucent)
    if (ui->has_csg_preview) {
        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glColor4f(0.2f, 0.8f, 0.4f, 0.35f);
        if (ui->csg_preview.type == OZ_BRUSH_BOX) {
            const OzBrushBox* b = &ui->csg_preview.as.box;
            float cx=b->center.x, cy=b->center.y, cz=b->center.z;
            float hx=b->half.x, hy=b->half.y, hz=b->half.z;
            float v[8][3] = {
                {cx - hx, cy - hy, cz - hz}, {cx + hx, cy - hy, cz - hz}, {cx + hx, cy + hy, cz - hz}, {cx - hx, cy + hy, cz - hz},
                {cx - hx, cy - hy, cz + hz}, {cx + hx, cy - hy, cz + hz}, {cx + hx, cy + hy, cz + hz}, {cx - hx, cy + hy, cz + hz},
            };
            int faces[6][4] = {{0,1,2,3},{4,5,6,7},{0,1,5,4},{2,3,7,6},{1,2,6,5},{0,3,7,4}};
            glBegin(GL_QUADS);
            for (int fidx = 0; fidx < 6; ++fidx) {
                for (int vi = 0; vi < 4; ++vi) { float* a = v[faces[fidx][vi]]; glVertex3f(a[0],a[1],a[2]); }
            }
            glEnd();
        }
        glDisable(GL_BLEND);
    }
    // Draw transform gizmo at selected brush or object center (3 axes, prominent)
    if ((ui->selected_index >= 0 && ui->selected_index < (int)ui->map.count) ||
        (ui->selected_object >= 0 && (size_t)ui->selected_object < ui->obj_count)) {
        float gx, gy, gz;
        if (ui->selected_index >= 0) {
            const OzBrush* sel = &ui->map.brushes[ui->selected_index];
            OzVec3 p = (sel->type == OZ_BRUSH_BOX) ? sel->as.box.center : sel->as.cyl.center;
            gx = p.x; gy = p.y; gz = p.z;
        } else {
            const EditorObject* o = &ui->objects[ui->selected_object];
            if (o->type == OBJ_ZONE) { gx=o->as.zone.center[0]; gy=o->as.zone.center[1]; gz=o->as.zone.center[2]; }
            else if (o->type == OBJ_PICKUP) { gx=o->as.pickup.position[0]; gy=o->as.pickup.position[1]; gz=o->as.pickup.position[2]; }
            else { gx=o->as.pstart.position[0]; gy=o->as.pstart.position[1]; gz=o->as.pstart.position[2]; }
        }
        float axisLen = 3.0f;
        glDisable(GL_DEPTH_TEST);
        glLineWidth(3.0f);
        glBegin(GL_LINES);
        glColor3f(1.0f, 0.2f, 0.2f); glVertex3f(gx, gy, gz); glVertex3f(gx + axisLen, gy, gz);
        glColor3f(0.2f, 1.0f, 0.2f); glVertex3f(gx, gy, gz); glVertex3f(gx, gy + axisLen, gz);
        glColor3f(0.3f, 0.6f, 1.0f); glVertex3f(gx, gy, gz); glVertex3f(gx, gy, gz + axisLen);
        glEnd();
        glLineWidth(1.0f);
        glEnable(GL_DEPTH_TEST);
    }
    GLenum glerr = glGetError();
    if (glerr != GL_NO_ERROR) {
        OZ_WARN("OpenGL error: 0x%X", (unsigned)glerr);
    }
    return TRUE;
}

static void project_point(float x, float y, float z, int w, int h, float* out_x, float* out_y) {
    // Fixed camera similar to GL view used above
    float yaw = 0.0f;
    float pitch = -15.0f * (float)G_PI / 180.0f;
    float px = 0.0f, py = -5.0f, pz = -2.5f;
    // Translate
    x -= px; y -= py; z -= pz;
    // Rotate yaw around Z
    float cy = cosf(yaw), sy = sinf(yaw);
    float x1 =  cy * x + sy * y;
    float y1 = -sy * x + cy * y;
    float z1 = z;
    // Rotate pitch around X
    float cp = cosf(pitch), sp = sinf(pitch);
    float x2 = x1;
    float y2 = cp * y1 - sp * z1;
    float z2 = sp * y1 + cp * z1;
    // Simple perspective
    float f = 1.0f / tanf(60.0f * (float)G_PI / 360.0f);
    float ndc_x = (x2 * f) / (z2 + 5.0f);
    float ndc_y = (y2 * f) / (z2 + 5.0f);
    *out_x = (float)w * 0.5f + ndc_x * (float)w * 0.5f;
    *out_y = (float)h * 0.5f - ndc_y * (float)h * 0.5f;
}

static gboolean fallback_draw(GtkWidget* widget, cairo_t* cr, gpointer user_data) {
    EditorUi* ui = (EditorUi*)user_data;
    static gboolean logged_once = FALSE;
    if (!logged_once) { OZ_INFO("fallback_draw called"); logged_once = TRUE; }
    int w = gtk_widget_get_allocated_width(widget);
    int h = gtk_widget_get_allocated_height(widget);
    cairo_set_source_rgb(cr, 0.1, 0.1, 0.12);
    cairo_paint(cr);
    // Grid plane at z=0 for horizon reference
    cairo_set_source_rgba(cr, 0.45, 0.48, 0.52, 0.6);
    const float gridExtent = 20.0f;
    const float gridStep = 1.0f;
    for (float v = -gridExtent; v <= gridExtent + 0.001f; v += gridStep) {
        float x0,y0,x1,y1;
        // lines parallel to X axis (vary y)
        project_point(-gridExtent, v, 0.0f, w,h,&x0,&y0);
        project_point( gridExtent, v, 0.0f, w,h,&x1,&y1);
        cairo_move_to(cr, x0, y0); cairo_line_to(cr, x1, y1);
        // lines parallel to Y axis (vary x)
        project_point(v, -gridExtent, 0.0f, w,h,&x0,&y0);
        project_point(v,  gridExtent, 0.0f, w,h,&x1,&y1);
        cairo_move_to(cr, x0, y0); cairo_line_to(cr, x1, y1);
        cairo_stroke(cr);
    }
    // Axis highlight
    cairo_set_source_rgba(cr, 0.7, 0.7, 0.8, 0.9);
    float ax0,ay0,ax1,ay1;
    project_point(-gridExtent, 0.0f, 0.0f, w,h,&ax0,&ay0);
    project_point( gridExtent, 0.0f, 0.0f, w,h,&ax1,&ay1);
    cairo_move_to(cr, ax0, ay0); cairo_line_to(cr, ax1, ay1); cairo_stroke(cr);
    project_point(0.0f, -gridExtent, 0.0f, w,h,&ax0,&ay0);
    project_point(0.0f,  gridExtent, 0.0f, w,h,&ax1,&ay1);
    cairo_move_to(cr, ax0, ay0); cairo_line_to(cr, ax1, ay1); cairo_stroke(cr);
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 12);
    cairo_move_to(cr, 8, 18);
    cairo_show_text(cr, "Fallback viewport (software)");

    // Draw CSG preview (semi-transparent fill)
    if (ui->has_csg_preview && ui->csg_preview.type == OZ_BRUSH_BOX) {
        const OzBrushBox* b = &ui->csg_preview.as.box;
        float cx=b->center.x, cy=b->center.y, cz=b->center.z;
        float hx=b->half.x, hy=b->half.y, hz=b->half.z;
        float v[8][3] = {
            {cx - hx, cy - hy, cz - hz}, {cx + hx, cy - hy, cz - hz}, {cx + hx, cy + hy, cz - hz}, {cx - hx, cy + hy, cz - hz},
            {cx - hx, cy - hy, cz + hz}, {cx + hx, cy - hy, cz + hz}, {cx + hx, cy + hy, cz + hz}, {cx - hx, cy + hy, cz + hz},
        };
        int faces[6][4] = {{0,1,2,3},{4,5,6,7},{0,1,5,4},{2,3,7,6},{1,2,6,5},{0,3,7,4}};
        cairo_set_source_rgba(cr, 0.2, 0.8, 0.4, 0.3);
        for (int f=0; f<6; ++f) {
            float x0,y0; project_point(v[faces[f][0]][0], v[faces[f][0]][1], v[faces[f][0]][2], w,h,&x0,&y0);
            cairo_move_to(cr, x0, y0);
            for (int vi=1; vi<4; ++vi) { float x,y; project_point(v[faces[f][vi]][0], v[faces[f][vi]][1], v[faces[f][vi]][2], w,h,&x,&y); cairo_line_to(cr, x,y);} 
            cairo_close_path(cr); cairo_fill(cr);
        }
    }

    // Draw brushes wireframe
    cairo_set_source_rgb(cr, 0.9, 0.9, 0.95);
    for (size_t i = 0; i < ui->map.count; ++i) {
        const OzBrush* br = &ui->map.brushes[i];
        if (br->type == OZ_BRUSH_BOX) {
            const OzBrushBox* b = &br->as.box;
            const float cx=b->center.x, cy=b->center.y, cz=b->center.z;
            const float hx=b->half.x, hy=b->half.y, hz=b->half.z;
            float v[8][3] = {
                {cx - hx, cy - hy, cz - hz}, {cx + hx, cy - hy, cz - hz}, {cx + hx, cy + hy, cz - hz}, {cx - hx, cy + hy, cz - hz},
                {cx - hx, cy - hy, cz + hz}, {cx + hx, cy - hy, cz + hz}, {cx + hx, cy + hy, cz + hz}, {cx - hx, cy + hy, cz + hz},
            };
            int edges[12][2] = { {0,1},{1,2},{2,3},{3,0}, {4,5},{5,6},{6,7},{7,4}, {0,4},{1,5},{2,6},{3,7} };
            for (int e = 0; e < 12; ++e) {
                float x0,y0,x1,y1;
                project_point(v[edges[e][0]][0], v[edges[e][0]][1], v[edges[e][0]][2], w,h,&x0,&y0);
                project_point(v[edges[e][1]][0], v[edges[e][1]][1], v[edges[e][1]][2], w,h,&x1,&y1);
                cairo_move_to(cr, x0, y0);
                cairo_line_to(cr, x1, y1);
            }
            cairo_stroke(cr);
        } else if (br->type == OZ_BRUSH_CYLINDER) {
            const OzBrushCylinder* c = &br->as.cyl;
            int seg = c->segments > 3 ? c->segments : 16;
            float hz = c->height * 0.5f;
            // draw rings
            for (int ring = -1; ring <= 1; ring += 2) {
                float z = c->center.z + (ring < 0 ? -hz : hz);
                cairo_new_path(cr);
                for (int i2 = 0; i2 <= seg; ++i2) {
                    float a = (float)(i2 % seg) / (float)seg * 6.2831853f;
                    float x = c->center.x + cosf(a)*c->radius_x;
                    float y = c->center.y + sinf(a)*c->radius_y;
                    float sx, sy; project_point(x,y,z,w,h,&sx,&sy);
                    if (i2==0) cairo_move_to(cr, sx, sy); else cairo_line_to(cr, sx, sy);
                }
                cairo_stroke(cr);
            }
            // verticals
            for (int i = 0; i < seg; ++i) {
                float a = (float)i / (float)seg * 6.2831853f;
                float x = c->center.x + cosf(a)*c->radius_x;
                float y = c->center.y + sinf(a)*c->radius_y;
                float sx0,sy0,sx1,sy1; project_point(x,y,c->center.z-hz,w,h,&sx0,&sy0); project_point(x,y,c->center.z+hz,w,h,&sx1,&sy1);
                cairo_move_to(cr, sx0, sy0); cairo_line_to(cr, sx1, sy1);
                cairo_stroke(cr);
            }
        }
    }
    // We handled drawing fully; stop further processing to avoid overdraw
    return TRUE;
}

// Periodic redraw helper for software DrawingArea
static gboolean queue_draw_cb(gpointer data) {
    EditorUi* ui = (EditorUi*)data;
    if (ui && ui->shutting_down) return FALSE;
    if (ui && ui->viewport && GTK_IS_WIDGET(ui->viewport)) {
        gtk_widget_queue_draw(ui->viewport);
    }
    return TRUE;
}

static gboolean on_window_delete(GtkWidget* widget, GdkEvent* event, gpointer user_data) {
    (void)widget; (void)event;
    action_quit(NULL, NULL, user_data);
    return TRUE; // we destroy explicitly in action_quit
}

static void gl_area_realize(GtkGLArea* area, gpointer user_data) {
    EditorUi* ui = (EditorUi*)user_data;
    // Trap potential X/GLX errors and fallback gracefully
    gdk_error_trap_push();
    gtk_gl_area_make_current(area);
    int xerr = gdk_error_trap_pop();
    if (xerr != 0 || gtk_gl_area_get_error(area)) {
        OZ_WARN("GLX/GDK error during GtkGLArea realize (code=%d). Falling back to software viewport.", xerr);
        // Fallback: replace GLArea within its current parent container
        GtkWidget* parent = gtk_widget_get_parent(GTK_WIDGET(area));
        GtkWidget* da = gtk_drawing_area_new();
        gtk_widget_set_hexpand(da, TRUE);
        gtk_widget_set_vexpand(da, TRUE);
        gtk_widget_set_app_paintable(da, TRUE);
        gtk_widget_set_size_request(da, 320, 200);
        g_signal_connect(da, "draw", G_CALLBACK(fallback_draw), ui);
        if (parent) gtk_container_remove(GTK_CONTAINER(parent), GTK_WIDGET(area));
        ui->gl_area = NULL;
        ui->viewport = da;
        if (!ui->redraw_id) ui->redraw_id = g_timeout_add(16, queue_draw_cb, ui);
        if (parent) gtk_box_pack_start(GTK_BOX(parent), da, TRUE, TRUE, 0);
        if (ui->window) gtk_widget_show_all(ui->window);
        return;
    }
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    const GLubyte* vendor = glGetString(GL_VENDOR);
    const GLubyte* renderer = glGetString(GL_RENDERER);
    const GLubyte* version = glGetString(GL_VERSION);
    OZ_INFO("GL realized: vendor=%s renderer=%s version=%s",
            vendor ? (const char*)vendor : "?",
            renderer ? (const char*)renderer : "?",
            version ? (const char*)version : "?");

    // Create a simple checker texture for brush visibility
    if (ui) {
        ui->gl_tex_w = 64; ui->gl_tex_h = 64; ui->gl_tex_brush = 0;
        unsigned char* pixels = (unsigned char*)malloc((size_t)ui->gl_tex_w * (size_t)ui->gl_tex_h * 3);
        if (pixels) {
            for (int y = 0; y < ui->gl_tex_h; ++y) {
                for (int x = 0; x < ui->gl_tex_w; ++x) {
                    int idx = (y * ui->gl_tex_w + x) * 3;
                    int c = (((x >> 3) ^ (y >> 3)) & 1) ? 200 : 120;
                    pixels[idx+0] = (unsigned char)c;
                    pixels[idx+1] = (unsigned char)c;
                    pixels[idx+2] = (unsigned char)c;
                }
            }
            GLuint tex = 0; glGenTextures(1, &tex);
            glBindTexture(GL_TEXTURE_2D, tex);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, ui->gl_tex_w, ui->gl_tex_h, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels);
            ui->gl_tex_brush = tex;
            free(pixels);
        }
    }
}

// --- Menu wiring (split by domain) ---

static void build_menu_file(GtkApplication* app, EditorUi* ui, GMenu* menubar) {
    GMenu* file_menu = g_menu_new();
    g_menu_append(file_menu, "Open",  "app.open");
    g_menu_append(file_menu, "Save",  "app.save");
    // Import submenu for assets
    GMenu* import_menu = g_menu_new();
    g_menu_append(import_menu, "Import Texture (.oztex)", "app.import_texture");
    g_menu_append(import_menu, "Import Mesh Bundle (.ozbag)", "app.import_bundle");
    g_menu_append(import_menu, "Import Music (.ozmux)", "app.import_music");
    g_menu_append_submenu(file_menu, "Import", G_MENU_MODEL(import_menu));
    GMenu* file_section_launch = g_menu_new();
    g_menu_append(file_section_launch, "Launch Editor", "app.launch_editor");
    g_menu_append(file_section_launch, "Launch Game",   "app.launch_game");
    g_menu_append(file_section_launch, "Launch Server", "app.launch_server");
    g_menu_append_section(file_menu, NULL, G_MENU_MODEL(file_section_launch));
    g_menu_append(file_menu, "Quit",  "app.quit");
    g_menu_append_submenu(menubar, "File", G_MENU_MODEL(file_menu));
    g_object_unref(file_menu);
    g_object_unref(import_menu);
    g_object_unref(file_section_launch);
}

// --- Object system helpers ---
static void objects_reserve(EditorUi* ui, size_t cap) {
    if (!ui) return;
    if (cap <= ui->obj_capacity) return;
    size_t nc = ui->obj_capacity ? ui->obj_capacity : 8;
    while (nc < cap) nc *= 2;
    EditorObject* mem = (EditorObject*)realloc(ui->objects, nc * sizeof(EditorObject));
    if (!mem) return;
    ui->objects = mem; ui->obj_capacity = nc;
}

static int objects_add(EditorUi* ui, const EditorObject* obj) {
    if (!ui || !obj) return -1;
    objects_reserve(ui, ui->obj_count + 1);
    ui->objects[ui->obj_count] = *obj;
    return (int)ui->obj_count++;
}

static void open_object_properties(EditorUi* ui, int index) {
    if (!ui || index < 0 || (size_t)index >= ui->obj_count) return;
    EditorObject* o = &ui->objects[index];
    GtkWidget* win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), "Object Properties");
    gtk_window_set_default_size(GTK_WINDOW(win), 360, 260);
    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_add(GTK_CONTAINER(win), vbox);
    char title[128];
    switch (o->type) {
        case OBJ_ZONE:
            snprintf(title, sizeof(title), "Zone: %s", o->as.zone.name ? o->as.zone.name : "(unnamed)");
            gtk_box_pack_start(GTK_BOX(vbox), gtk_label_new(title), FALSE, FALSE, 0);
            // Radius editor (persist to object)
            {
                GtkWidget* hb = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
                gtk_box_pack_start(GTK_BOX(vbox), hb, FALSE, FALSE, 0);
                gtk_box_pack_start(GTK_BOX(hb), gtk_label_new("Radius"), FALSE, FALSE, 0);
                GtkWidget* entry = gtk_entry_new();
                char buf[32]; g_snprintf(buf, sizeof(buf), "%.2f", o->as.zone.radius);
                gtk_entry_set_text(GTK_ENTRY(entry), buf);
                PropCtx* pc = g_new0(PropCtx, 1); pc->ui = ui; pc->index = index;
                g_signal_connect(entry, "activate", G_CALLBACK(on_zone_radius_changed), pc);
                g_signal_connect(entry, "changed", G_CALLBACK(on_zone_radius_changed), pc);
                gtk_box_pack_start(GTK_BOX(hb), entry, TRUE, TRUE, 0);
            }
            break;
        case OBJ_PICKUP:
            snprintf(title, sizeof(title), "Pickup: %s", o->as.pickup.name ? o->as.pickup.name : "(item)");
            gtk_box_pack_start(GTK_BOX(vbox), gtk_label_new(title), FALSE, FALSE, 0);
            break;
        case OBJ_PLAYER_START:
            gtk_box_pack_start(GTK_BOX(vbox), gtk_label_new("PlayerStart"), FALSE, FALSE, 0);
            // Camera mode chooser
            gtk_box_pack_start(GTK_BOX(vbox), gtk_label_new("Camera Mode"), FALSE, FALSE, 0);
            GtkWidget* combo = gtk_combo_box_text_new();
            gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), "FreeMove");
            gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), "FPS");
            gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), "Cinematic");
            gtk_combo_box_set_active(GTK_COMBO_BOX(combo), (int)o->as.pstart.camera_mode);
            PropCtx* pc_cam = g_new0(PropCtx, 1); pc_cam->ui = ui; pc_cam->index = index;
            g_signal_connect(combo, "changed", G_CALLBACK(on_pstart_cam_changed), pc_cam);
            gtk_box_pack_start(GTK_BOX(vbox), combo, FALSE, FALSE, 0);
            // Yaw editor (persist to object)
            {
                GtkWidget* hb = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
                gtk_box_pack_start(GTK_BOX(vbox), hb, FALSE, FALSE, 0);
                gtk_box_pack_start(GTK_BOX(hb), gtk_label_new("Yaw (deg)"), FALSE, FALSE, 0);
                GtkWidget* entry = gtk_entry_new();
                char buf[32]; g_snprintf(buf, sizeof(buf), "%.1f", o->as.pstart.yaw * 57.29578f);
                gtk_entry_set_text(GTK_ENTRY(entry), buf);
                PropCtx* pc_yaw = g_new0(PropCtx, 1); pc_yaw->ui = ui; pc_yaw->index = index;
                g_signal_connect(entry, "activate", G_CALLBACK(on_pstart_yaw_changed), pc_yaw);
                g_signal_connect(entry, "changed", G_CALLBACK(on_pstart_yaw_changed), pc_yaw);
                gtk_box_pack_start(GTK_BOX(hb), entry, TRUE, TRUE, 0);
            }
            break;
        default: break;
    }
    gtk_widget_show_all(win);
}

static void object_list_row_activated(GtkListBox* box, GtkListBoxRow* row, gpointer user_data) {
    (void)box; EditorUi* ui = (EditorUi*)user_data; if (!ui) return;
    int idx = gtk_list_box_row_get_index(row);
    if (idx >= 0 && (size_t)idx < ui->obj_count) {
        ui->selected_object = idx;
        open_object_properties(ui, idx);
    }
}

static void add_zone_clicked(GtkButton* b, gpointer user_data) {
    (void)b; EditorUi* ui = (EditorUi*)user_data; if (!ui) return;
    EditorObject o = {0}; o.type = OBJ_ZONE; o.as.zone.name = g_strdup("Zone"); o.as.zone.center[0]=0; o.as.zone.center[1]=0; o.as.zone.center[2]=0; o.as.zone.radius=5.0f;
    int idx = objects_add(ui, &o); ui->selected_object = idx;
    if (ui->viewport) gtk_widget_queue_draw(ui->viewport);
}
static void add_pickup_clicked(GtkButton* b, gpointer user_data) {
    (void)b; EditorUi* ui = (EditorUi*)user_data; if (!ui) return;
    EditorObject o = {0}; o.type = OBJ_PICKUP; o.as.pickup.name = g_strdup("Health"); o.as.pickup.position[0]=0; o.as.pickup.position[1]=0; o.as.pickup.position[2]=1.0f; o.as.pickup.respawn_seconds=30.0f;
    int idx = objects_add(ui, &o); ui->selected_object = idx;
    if (ui->viewport) gtk_widget_queue_draw(ui->viewport);
}
static void add_player_start_clicked(GtkButton* b, gpointer user_data) {
    (void)b; EditorUi* ui = (EditorUi*)user_data; if (!ui) return;
    EditorObject o = {0}; o.type = OBJ_PLAYER_START; o.as.pstart.position[0]=0; o.as.pstart.position[1]=0; o.as.pstart.position[2]=1.0f; o.as.pstart.yaw=0.0f; o.as.pstart.camera_mode=OZ_CAMERA_FREEMOVE;
    int idx = objects_add(ui, &o); ui->selected_object = idx;
    if (ui->viewport) gtk_widget_queue_draw(ui->viewport);
}

// Placement helpers: drop near camera
static void place_near_camera(float pos[3]) {
    if (!g_editor_state) { pos[0]=0; pos[1]=0; pos[2]=0; return; }
    OzVec3 p = g_editor_state->cam.position;
    float yaw = g_editor_state->cam.yaw;
    float fx = cosf(yaw), fy = sinf(yaw);
    pos[0] = p.x + fx * 2.0f;
    pos[1] = p.y + fy * 2.0f;
    pos[2] = p.z;
}

static void add_zone_template(GtkListBox* box, GtkListBoxRow* row, gpointer user_data) {
    (void)box; (void)row; EditorUi* ui = (EditorUi*)user_data; if (!ui) return;
    EditorObject o = {0}; o.type = OBJ_ZONE; o.as.zone.name = g_strdup("Zone"); o.as.zone.radius = 5.0f; place_near_camera(o.as.zone.center);
    int idx = objects_add(ui, &o); ui->selected_object = idx; if (ui->obj_scene_list && GTK_IS_LIST_BOX(ui->obj_scene_list)) {
        char buf[128]; snprintf(buf, sizeof(buf), "Zone #%d", idx);
        gtk_list_box_insert(GTK_LIST_BOX(ui->obj_scene_list), gtk_label_new(buf), -1);
        gtk_widget_show_all(ui->obj_scene_list);
    }
    if (ui->viewport) gtk_widget_queue_draw(ui->viewport);
}
static void add_pickup_template(GtkListBox* box, GtkListBoxRow* row, gpointer user_data) {
    (void)box; (void)row; EditorUi* ui = (EditorUi*)user_data; if (!ui) return;
    EditorObject o = {0}; o.type = OBJ_PICKUP; o.as.pickup.name = g_strdup("Health"); o.as.pickup.respawn_seconds = 30.0f; place_near_camera(o.as.pickup.position);
    int idx = objects_add(ui, &o); ui->selected_object = idx; if (ui->obj_scene_list && GTK_IS_LIST_BOX(ui->obj_scene_list)) {
        char buf[128]; snprintf(buf, sizeof(buf), "Pickup #%d", idx);
        gtk_list_box_insert(GTK_LIST_BOX(ui->obj_scene_list), gtk_label_new(buf), -1);
        gtk_widget_show_all(ui->obj_scene_list);
    }
    if (ui->viewport) gtk_widget_queue_draw(ui->viewport);
}
static void add_playerstart_template(GtkListBox* box, GtkListBoxRow* row, gpointer user_data) {
    (void)box; (void)row; EditorUi* ui = (EditorUi*)user_data; if (!ui) return;
    EditorObject o = {0}; o.type = OBJ_PLAYER_START; place_near_camera(o.as.pstart.position); o.as.pstart.yaw = g_editor_state? g_editor_state->cam.yaw : 0.0f; o.as.pstart.camera_mode = OZ_CAMERA_FPS;
    int idx = objects_add(ui, &o); ui->selected_object = idx; if (ui->obj_scene_list && GTK_IS_LIST_BOX(ui->obj_scene_list)) {
        char buf[128]; snprintf(buf, sizeof(buf), "PlayerStart #%d", idx);
        gtk_list_box_insert(GTK_LIST_BOX(ui->obj_scene_list), gtk_label_new(buf), -1);
        gtk_widget_show_all(ui->obj_scene_list);
    }
    if (ui->viewport) gtk_widget_queue_draw(ui->viewport);
}

static GtkWidget* build_template_list(const char** items, int count, GCallback on_activate, gpointer user_data) {
    GtkWidget* list = gtk_list_box_new();
    for (int i = 0; i < count; ++i) {
        gtk_list_box_insert(GTK_LIST_BOX(list), gtk_label_new(items[i]), -1);
    }
    g_signal_connect(list, "row-activated", on_activate, user_data);
    return list;
}

static void action_open_object_browser(GSimpleAction* a, GVariant* p, gpointer user_data) {
    (void)a; (void)p; EditorUi* ui = (EditorUi*)user_data; if (!ui) return;
    if (ui->obj_browser_win && GTK_IS_WIDGET(ui->obj_browser_win)) { gtk_window_present(GTK_WINDOW(ui->obj_browser_win)); return; }
    GtkWidget* win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), "Object Browser");
    gtk_window_set_default_size(GTK_WINDOW(win), 520, 420);
    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_add(GTK_CONTAINER(win), vbox);
    GtkWidget* notebook = gtk_notebook_new();
    gtk_box_pack_start(GTK_BOX(vbox), notebook, TRUE, TRUE, 0);
    // Zones tab
    const char* zones[] = { "Zone" };
    GtkWidget* zones_list = build_template_list(zones, 1, G_CALLBACK(add_zone_template), ui);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), zones_list, gtk_label_new("Zones"));
    // Pickups tab
    const char* picks[] = { "Health", "Ammo" };
    GtkWidget* picks_list = build_template_list(picks, 2, G_CALLBACK(add_pickup_template), ui);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), picks_list, gtk_label_new("Pickups"));
    // Assets tab: show last imported asset paths (if any)
    GtkWidget* assets_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    if (ui->last_texture_path) gtk_box_pack_start(GTK_BOX(assets_box), gtk_label_new(ui->last_texture_path), FALSE, FALSE, 0);
    if (ui->last_bundle_path)  gtk_box_pack_start(GTK_BOX(assets_box), gtk_label_new(ui->last_bundle_path), FALSE, FALSE, 0);
    if (ui->last_music_path)   gtk_box_pack_start(GTK_BOX(assets_box), gtk_label_new(ui->last_music_path), FALSE, FALSE, 0);
    if (!ui->last_texture_path && !ui->last_bundle_path && !ui->last_music_path) {
        gtk_box_pack_start(GTK_BOX(assets_box), gtk_label_new("No assets imported yet. Use File → Import."), FALSE, FALSE, 0);
    }
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), assets_box, gtk_label_new("Assets"));
    // Nodes tab (Player Start)
    const char* nodes[] = { "Player Start" };
    GtkWidget* nodes_list = build_template_list(nodes, 1, G_CALLBACK(add_playerstart_template), ui);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), nodes_list, gtk_label_new("Nodes"));
    // Scene objects list at bottom
    GtkWidget* scene_frame = gtk_frame_new("Scene Objects (double-click to edit)");
    GtkWidget* scene_list = gtk_list_box_new();
    for (size_t i = 0; i < ui->obj_count; ++i) {
        const EditorObject* o = &ui->objects[i];
        char buf[128]; const char* tp = (o->type==OBJ_ZONE?"Zone":(o->type==OBJ_PICKUP?"Pickup":"PlayerStart"));
        snprintf(buf, sizeof(buf), "%s #%zu", tp, i);
        gtk_list_box_insert(GTK_LIST_BOX(scene_list), gtk_label_new(buf), -1);
    }
    g_signal_connect(scene_list, "row-activated", G_CALLBACK(object_list_row_activated), ui);
    gtk_container_add(GTK_CONTAINER(scene_frame), scene_list);
    gtk_box_pack_start(GTK_BOX(vbox), scene_frame, FALSE, FALSE, 0);
    gtk_widget_show_all(win);
    ui->obj_browser_win = win;
    ui->obj_scene_list = scene_list;
}

// Shim to open object browser from a GtkButton clicked
static void open_object_browser_clicked(GtkButton* b, gpointer user_data) { (void)b; action_open_object_browser(NULL, NULL, user_data); }

static void build_menu_bsp(GtkApplication* app, EditorUi* ui, GMenu* menubar) {
    (void)app; (void)ui;
    GMenu* bsp_menu = g_menu_new();
    GMenu* bsp_section_brushes = g_menu_new();
    g_menu_append(bsp_section_brushes, "Box...",  "app.bsp_box_dialog");
    g_menu_append(bsp_section_brushes, "Cylinder...",  "app.bsp_cyl_dialog");
    g_menu_append_section(bsp_menu, "Brushes", G_MENU_MODEL(bsp_section_brushes));

    GMenu* bsp_section_csg = g_menu_new();
    g_menu_append(bsp_section_csg, "CSG Add", "app.csg_add");
    g_menu_append(bsp_section_csg, "CSG Subtract", "app.csg_sub");
    g_menu_append(bsp_section_csg, "CSG Intersect", "app.csg_isect");
    g_menu_append_section(bsp_menu, "CSG", G_MENU_MODEL(bsp_section_csg));

    GMenu* bsp_section_tools = g_menu_new();
    g_menu_append(bsp_section_tools, "Carve", "app.tool_carve");
    g_menu_append(bsp_section_tools, "Slope", "app.tool_slope");
    g_menu_append_section(bsp_menu, "Tools", G_MENU_MODEL(bsp_section_tools));

    g_menu_append_submenu(menubar, "BSP", G_MENU_MODEL(bsp_menu));
    g_object_unref(bsp_menu);
    g_object_unref(bsp_section_brushes);
    g_object_unref(bsp_section_csg);
    g_object_unref(bsp_section_tools);
}

static void build_menu_light(GtkApplication* app, EditorUi* ui, GMenu* menubar) {
    (void)app; (void)ui;
    GMenu* light_menu = g_menu_new();
    g_menu_append(light_menu, "Build Light", "app.build_light");
    g_menu_append_submenu(menubar, "Light", G_MENU_MODEL(light_menu));
    g_object_unref(light_menu);
}

static void action_view_grab_focus(GSimpleAction* a, GVariant* p, gpointer user_data) {
    (void)a; (void)p; EditorUi* ui = (EditorUi*)user_data;
    if (ui && ui->viewport) {
        gtk_widget_set_can_focus(ui->viewport, TRUE);
        gtk_widget_grab_focus(ui->viewport);
        OZ_INFO("Viewport focus grabbed");
    }
}

static void action_gizmo_translate(GSimpleAction* a, GVariant* p, gpointer user_data) { (void)a; (void)p; EditorUi* ui=(EditorUi*)user_data; ui->gizmo_translate=TRUE; ui->gizmo_rotate=FALSE; ui->gizmo_scale=FALSE; }
static void action_gizmo_rotate(GSimpleAction* a, GVariant* p, gpointer user_data)    { (void)a; (void)p; EditorUi* ui=(EditorUi*)user_data; ui->gizmo_translate=FALSE; ui->gizmo_rotate=TRUE; ui->gizmo_scale=FALSE; }
static void action_gizmo_scale(GSimpleAction* a, GVariant* p, gpointer user_data)     { (void)a; (void)p; EditorUi* ui=(EditorUi*)user_data; ui->gizmo_translate=FALSE; ui->gizmo_rotate=FALSE; ui->gizmo_scale=TRUE; }

static void build_menu_view(GtkApplication* app, EditorUi* ui, GMenu* menubar) {
    (void)app; (void)ui;
    GMenu* view_menu = g_menu_new();
    g_menu_append(view_menu, "Grab Focus", "app.view_grab_focus");
    g_menu_append(view_menu, "Gizmo: Translate", "app.gizmo_translate");
    g_menu_append(view_menu, "Gizmo: Rotate", "app.gizmo_rotate");
    g_menu_append(view_menu, "Gizmo: Scale", "app.gizmo_scale");
    g_menu_append(view_menu, "Toggle Axes", "app.debug_toggle_axes");
    g_menu_append(view_menu, "Object Browser...", "app.open_object_browser");
    g_menu_append_submenu(menubar, "View", G_MENU_MODEL(view_menu));
    g_object_unref(view_menu);
}

// --- Debug actions ---
static void action_debug_toggle_grid(GSimpleAction* a, GVariant* p, gpointer user_data) { (void)a; (void)p; EditorUi* ui=(EditorUi*)user_data; ui->dbg_show_grid = !ui->dbg_show_grid; if (ui->viewport) gtk_widget_queue_draw(ui->viewport); }
static void action_debug_toggle_axes(GSimpleAction* a, GVariant* p, gpointer user_data) { (void)a; (void)p; EditorUi* ui=(EditorUi*)user_data; ui->dbg_show_axes = !ui->dbg_show_axes; if (ui->viewport) gtk_widget_queue_draw(ui->viewport); }
static void action_debug_toggle_fps(GSimpleAction* a, GVariant* p, gpointer user_data) { (void)a; (void)p; EditorUi* ui=(EditorUi*)user_data; ui->dbg_show_fps = !ui->dbg_show_fps; if (!ui->dbg_show_fps) gtk_label_set_text(GTK_LABEL(ui->info_label), ""); }

// Simple log window that receives oz_log sink lines
typedef struct LogWindowCtx { GtkWidget* win; GtkWidget* view; GtkTextBuffer* buf; } LogWindowCtx;
static LogWindowCtx* g_log_ctx = NULL;

static void log_sink_bridge(OzLogLevel level, const char* line) {
    (void)level; if (!g_log_ctx || !g_log_ctx->buf) return;
    GtkTextIter end; gtk_text_buffer_get_end_iter(g_log_ctx->buf, &end);
    gtk_text_buffer_insert(g_log_ctx->buf, &end, line, -1);
    gtk_text_buffer_insert(g_log_ctx->buf, &end, "\n", -1);
}

static void action_debug_open_log(GSimpleAction* a, GVariant* p, gpointer user_data) {
    (void)a; (void)p; (void)user_data;
    if (g_log_ctx && g_log_ctx->win) { gtk_window_present(GTK_WINDOW(g_log_ctx->win)); return; }
    g_log_ctx = g_new0(LogWindowCtx, 1);
    g_log_ctx->win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(g_log_ctx->win), "Log");
    gtk_window_set_default_size(GTK_WINDOW(g_log_ctx->win), 520, 320);
    GtkWidget* scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(g_log_ctx->win), scrolled);
    g_log_ctx->view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(g_log_ctx->view), FALSE);
    gtk_container_add(GTK_CONTAINER(scrolled), g_log_ctx->view);
    g_log_ctx->buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(g_log_ctx->view));
    gtk_widget_show_all(g_log_ctx->win);
    oz_log_set_sink(log_sink_bridge, true);
}

// Debug window: memory usage + backtrace (UNIX)
#ifndef _WIN32
#include <execinfo.h>
#include <sys/resource.h>
#include <link.h>
#endif
#include "oz/oz_debug.h"
#include <stdint.h>
#include <stdio.h>

static gchar* get_memory_info_string(void) {
#ifndef _WIN32
    struct rusage ru; getrusage(RUSAGE_SELF, &ru);
    gchar* s = g_strdup_printf("RSS: %ld KB  MinorFaults: %ld  MajorFaults: %ld",
                               (long)ru.ru_maxrss, (long)ru.ru_minflt, (long)ru.ru_majflt);
    return s;
#else
    return g_strdup("Memory info not available on this platform");
#endif
}

static void fill_stack_trace(GtkTextBuffer* buf) {
#ifndef _WIN32
    void* addrs[64]; int n = backtrace(addrs, 64);
    char** syms = backtrace_symbols(addrs, n);
    GtkTextIter end; gtk_text_buffer_get_end_iter(buf, &end);
    gtk_text_buffer_insert(buf, &end, "Stack trace:\n", -1);
    for (int i = 0; i < n; ++i) { gtk_text_buffer_insert(buf, &end, syms[i], -1); gtk_text_buffer_insert(buf, &end, "\n", -1); }
    free(syms);
#else
    (void)buf;
#endif
}

static void action_debug_open_debug(GSimpleAction* a, GVariant* p, gpointer user_data) {
    (void)a; (void)p; EditorUi* ui = (EditorUi*)user_data;
    if (ui->debug_window && GTK_IS_WIDGET(ui->debug_window)) { gtk_window_present(GTK_WINDOW(ui->debug_window)); return; }
    GtkWidget* win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), "Debug Info");
    gtk_window_set_default_size(GTK_WINDOW(win), 560, 380);
    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_add(GTK_CONTAINER(win), vbox);
    // Memory label
    gchar* mem = get_memory_info_string();
    GtkWidget* mem_label = gtk_label_new(mem); g_free(mem);
    gtk_box_pack_start(GTK_BOX(vbox), mem_label, FALSE, FALSE, 0);
    // Watch table
    GtkWidget* watch_sc = gtk_scrolled_window_new(NULL, NULL);
    gtk_box_pack_start(GTK_BOX(vbox), watch_sc, TRUE, TRUE, 0);
    GtkWidget* watch_tv = gtk_text_view_new(); gtk_text_view_set_editable(GTK_TEXT_VIEW(watch_tv), FALSE); gtk_container_add(GTK_CONTAINER(watch_sc), watch_tv);
    GtkTextBuffer* watch_buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(watch_tv));
    GtkTextIter wend; gtk_text_buffer_get_end_iter(watch_buf, &wend);
    gtk_text_buffer_insert(watch_buf, &wend, "Watches:\n", -1);
    for (size_t i = 0; i < oz_debug_watch_count(); ++i) {
        const char* nm = oz_debug_watch_name(i);
        OzDebugType tp = oz_debug_watch_type(i);
        const void* addr = oz_debug_watch_address(i);
        gchar line[256];
        switch (tp) {
            case OZ_DEBUG_T_I32: snprintf(line, sizeof(line), "%s = %d\n", nm, *(const int*)addr); break;
            case OZ_DEBUG_T_U32: snprintf(line, sizeof(line), "%s = %u\n", nm, *(const unsigned*)addr); break;
            case OZ_DEBUG_T_F32: snprintf(line, sizeof(line), "%s = %.3f\n", nm, *(const float*)addr); break;
            case OZ_DEBUG_T_F64: snprintf(line, sizeof(line), "%s = %.3f\n", nm, *(const double*)addr); break;
            default: snprintf(line, sizeof(line), "%s @ %p\n", nm, addr); break;
        }
        gtk_text_buffer_insert(watch_buf, &wend, line, -1);
    }

    // Stack trace view
    GtkWidget* sc = gtk_scrolled_window_new(NULL, NULL);
    gtk_box_pack_start(GTK_BOX(vbox), sc, TRUE, TRUE, 0);
    GtkWidget* tv = gtk_text_view_new(); gtk_text_view_set_editable(GTK_TEXT_VIEW(tv), FALSE); gtk_container_add(GTK_CONTAINER(sc), tv);
    GtkTextBuffer* buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(tv));
    fill_stack_trace(buf);

    // Disassembly around current frame (Unix)
    GtkWidget* asm_sc = gtk_scrolled_window_new(NULL, NULL);
    gtk_box_pack_start(GTK_BOX(vbox), asm_sc, TRUE, TRUE, 0);
    GtkWidget* asm_tv = gtk_text_view_new(); gtk_text_view_set_editable(GTK_TEXT_VIEW(asm_tv), FALSE); gtk_container_add(GTK_CONTAINER(asm_sc), asm_tv);
    GtkTextBuffer* asm_buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(asm_tv));
    GtkTextIter aend; gtk_text_buffer_get_end_iter(asm_buf, &aend);
#ifndef _WIN32
    void* addrs[64]; int n = backtrace(addrs, 64);
    if (n >= 2) {
        uintptr_t addr = (uintptr_t)addrs[1];
        uintptr_t start = addr > 64 ? addr - 64 : addr;
        uintptr_t stop  = addr + 128;
        char cmd[1024];
        snprintf(cmd, sizeof(cmd), "sh -lc 'objdump -d --no-show-raw-insn --start-address=0x%lx --stop-address=0x%lx /proc/self/exe 2>/dev/null || llvm-objdump -d --no-show-raw-insn --start-address=0x%lx --stop-address=0x%lx /proc/self/exe 2>/dev/null'",
                 (unsigned long)start, (unsigned long)stop, (unsigned long)start, (unsigned long)stop);
        FILE* fp = popen(cmd, "r");
        if (fp) {
            char line[512];
            while (fgets(line, sizeof(line), fp)) {
                gtk_text_buffer_insert(asm_buf, &aend, line, -1);
            }
            pclose(fp);
        } else {
            gtk_text_buffer_insert(asm_buf, &aend, "Failed to run objdump/llvm-objdump.\n", -1);
        }
    } else {
        gtk_text_buffer_insert(asm_buf, &aend, "No stack frames available for disassembly.\n", -1);
    }
#else
    gtk_text_buffer_insert(asm_buf, &aend, "Disassembly not available on this platform.\n", -1);
#endif
    gtk_widget_show_all(win);
    ui->debug_window = win;
}

// Demo JIT: compiles a simple function and calls it
static void action_debug_jit_example(GSimpleAction* a, GVariant* p, gpointer user_data) {
    (void)a; (void)p; (void)user_data;
    const char* src = "#include <stdio.h>\n" \
        "int ozjit_add(int a,int b){return a+b;}\n";
    void* h = NULL;
    if (!oz_jit_compile_and_load("ozjit_tmp", src, "-O2 -fPIC", &h)) { OZ_ERROR("JIT compile failed"); return; }
    typedef int (*AddFn)(int,int);
    AddFn fn = (AddFn)oz_jit_get_symbol(h, "ozjit_add");
    if (!fn) { OZ_ERROR("JIT: symbol not found"); oz_jit_unload(h); return; }
    int r = fn(40, 2);
    OZ_INFO("JIT result ozjit_add(40,2)=%d", r);
    oz_jit_unload(h);
}

static void populate_menus(GtkApplication* app, EditorUi* ui) {
    (void)ui;
    // Actions
    const GActionEntry entries[] = {
        { "open",  action_open,  NULL, NULL, NULL },
        { "save",  action_save,  NULL, NULL, NULL },
        { "import_texture", on_import_texture, NULL, NULL, NULL },
        { "import_bundle",  on_import_bundle,  NULL, NULL, NULL },
        { "import_music",   on_import_music,   NULL, NULL, NULL },
        { "view_grab_focus", action_view_grab_focus, NULL, NULL, NULL },
        { "build_map",     action_build_map,     NULL, NULL, NULL },
        { "build_light",   action_build_light,   NULL, NULL, NULL },
        { "build_bsp",     action_build_bsp,     NULL, NULL, NULL },
        { "build_brushes", action_build_brushes, NULL, NULL, NULL },
        { "bsp_box_dialog", action_bsp_box_dialog, NULL, NULL, NULL },
        { "bsp_cyl_dialog", action_bsp_cyl_dialog, NULL, NULL, NULL },
        { "csg_add", action_csg_add, NULL, NULL, NULL },
        { "csg_sub", action_csg_sub, NULL, NULL, NULL },
        { "csg_isect", action_csg_isect, NULL, NULL, NULL },
        { "tool_carve", action_tool_carve, NULL, NULL, NULL },
        { "tool_slope", action_tool_slope, NULL, NULL, NULL },
        { "launch_editor", action_launch_editor, NULL, NULL, NULL },
        { "launch_game",   action_launch_game,   NULL, NULL, NULL },
        { "launch_server", action_launch_server, NULL, NULL, NULL },
        { "gizmo_translate", action_gizmo_translate, NULL, NULL, NULL },
        { "gizmo_rotate", action_gizmo_rotate, NULL, NULL, NULL },
        { "gizmo_scale", action_gizmo_scale, NULL, NULL, NULL },
        { "debug_toggle_grid", action_debug_toggle_grid, NULL, NULL, NULL },
        { "debug_toggle_axes", action_debug_toggle_axes, NULL, NULL, NULL },
        { "debug_toggle_fps", action_debug_toggle_fps, NULL, NULL, NULL },
        { "debug_open_log", action_debug_open_log, NULL, NULL, NULL },
        { "debug_open_debug", action_debug_open_debug, NULL, NULL, NULL },
        { "debug_jit_example", action_debug_jit_example, NULL, NULL, NULL },
        { "open_object_browser", action_open_object_browser, NULL, NULL, NULL },
        { "quit",  action_quit,  NULL, NULL, NULL },
    };
    g_action_map_add_action_entries(G_ACTION_MAP(app), entries, G_N_ELEMENTS(entries), ui);

    // Menu model
    GMenu* menubar = g_menu_new();
    build_menu_file(app, ui, menubar);
    build_menu_view(app, ui, menubar);
    // BSP menu replaced by left-side buttons in the viewport
    build_menu_light(app, ui, menubar);
    // Debug menu
    GMenu* debug_menu = g_menu_new();
    g_menu_append(debug_menu, "Toggle Grid",  "app.debug_toggle_grid");
    g_menu_append(debug_menu, "Toggle Axes",  "app.debug_toggle_axes");
    g_menu_append(debug_menu, "Toggle FPS HUD",  "app.debug_toggle_fps");
    g_menu_append(debug_menu, "Open Log Window", "app.debug_open_log");
    g_menu_append(debug_menu, "Open Debug Window", "app.debug_open_debug");
    g_menu_append(debug_menu, "JIT: Compile+Run Example", "app.debug_jit_example");
    g_menu_append_submenu(menubar, "Debug", G_MENU_MODEL(debug_menu));
    g_object_unref(debug_menu);
    gtk_application_set_menubar(app, G_MENU_MODEL(menubar));

    g_object_unref(menubar);
}

// --- Left-side BSP creation buttons ---
static void select_last_and_invalidate(EditorUi* ui) {
    if (!ui) return;
    ui->selected_index = (int)(ui->map.count ? ui->map.count - 1 : -1);
    ui->has_csg_preview = FALSE; // any edit cancels preview
    if (ui->viewport) gtk_widget_queue_draw(ui->viewport);
}
static void btn_add_cube(GtkButton* b, gpointer user_data) { (void)b; EditorUi* ui=(EditorUi*)user_data; if (!ui) return; oz_map_add_box(&ui->map, (OzVec3){0,0,0}, (OzVec3){1,1,1}); select_last_and_invalidate(ui); }
static void btn_add_cylinder(GtkButton* b, gpointer user_data) { (void)b; action_bsp_cyl_dialog(NULL, NULL, user_data); }
static void btn_add_sphere(GtkButton* b, gpointer user_data) { (void)b; EditorUi* ui=(EditorUi*)user_data; if (!ui) return; oz_map_add_sphere(&ui->map, (OzVec3){0,0,0}, 0.5f, 16); select_last_and_invalidate(ui); }
static void btn_add_pyramid(GtkButton* b, gpointer user_data) { (void)b; EditorUi* ui=(EditorUi*)user_data; if (!ui) return; oz_map_add_pyramid(&ui->map, (OzVec3){0,0,0}, 0.5f, 0.5f, 1.0f); select_last_and_invalidate(ui); }
static void btn_add_plane(GtkButton* b, gpointer user_data) { (void)b; EditorUi* ui=(EditorUi*)user_data; if (!ui) return; oz_map_add_plane(&ui->map, (OzVec3){0,0,0}, (OzVec3){0,0,1}, 1.0f); select_last_and_invalidate(ui); }

// Simple splash window with animated GIF icon
static gboolean destroy_widget_cb(gpointer w) { if (w && GTK_IS_WIDGET(w)) gtk_widget_destroy(GTK_WIDGET(w)); return FALSE; }

static void maybe_show_splash(void) {
    const char* candidates[] = { "gfx/ozworld_icon.gif", "../gfx/ozworld_icon.gif", NULL };
    const char* path = NULL;
    for (int i = 0; candidates[i]; ++i) { if (g_file_test(candidates[i], G_FILE_TEST_IS_REGULAR)) { path = candidates[i]; break; } }
    if (!path) return;
    GError* err = NULL;
    GdkPixbufAnimation* anim = gdk_pixbuf_animation_new_from_file(path, &err);
    if (!anim) { if (err) { g_error_free(err); } return; }
    GtkWidget* splash = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_decorated(GTK_WINDOW(splash), FALSE);
    gtk_window_set_keep_above(GTK_WINDOW(splash), TRUE);
    gtk_window_set_skip_taskbar_hint(GTK_WINDOW(splash), TRUE);
    gtk_window_set_position(GTK_WINDOW(splash), GTK_WIN_POS_CENTER);
    GtkWidget* img = gtk_image_new_from_animation(anim);
    gtk_container_add(GTK_CONTAINER(splash), img);
    gtk_widget_show_all(splash);
    // Auto-destroy after 1.2s
    g_timeout_add(1200, destroy_widget_cb, splash);
}

static void on_activate(GtkApplication* app, gpointer user_data) {
    (void)user_data;
    EditorUi* ui = g_new0(EditorUi, 1);

    populate_menus(app, ui);

    // Show splash while we build the main window
    maybe_show_splash();

    ui->window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(ui->window), "OzWorld Editor");
    gtk_window_set_default_size(GTK_WINDOW(ui->window), 640, 480);

    ui->vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(ui->window), ui->vbox);

    // Menu bar from app menubar will be shown by shell; also add a simple toolbar placeholder
    ui->toolbar = gtk_toolbar_new();
    GtkToolItem* open_btn = gtk_tool_button_new(NULL, "Open");
    g_signal_connect(open_btn, "clicked", G_CALLBACK(toolbar_open_clicked), ui);
    gtk_toolbar_insert(GTK_TOOLBAR(ui->toolbar), open_btn, -1);

    GtkToolItem* save_btn = gtk_tool_button_new(NULL, "Save");
    g_signal_connect(save_btn, "clicked", G_CALLBACK(toolbar_save_clicked), ui);
    gtk_toolbar_insert(GTK_TOOLBAR(ui->toolbar), save_btn, -1);

    gtk_box_pack_start(GTK_BOX(ui->vbox), ui->toolbar, FALSE, FALSE, 0);

    // Initialize input state before wiring events and rendering
    static EditorInputState state;
    memset(&state, 0, sizeof(state));
    oz_camera_init(&state.cam, OZ_CAMERA_FREEMOVE);
    state.last_tick = g_get_monotonic_time() / 1000000.0;
    g_editor_state = &state;
    state.mouse_look_active = FALSE;
    // Editing state defaults
    ui->selected_index = -1;
    ui->dragging = FALSE;
    ui->drag_last_x = ui->drag_last_y = 0.0;
    state.last_mouse_x = 0.0;
    state.last_mouse_y = 0.0;
    state.mouse_sensitivity = 0.0035f;

    // Prepare left-side BSP buttons + viewport content area early
    GtkWidget* content = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkWidget* left = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_size_request(left, 96, -1);
    GtkWidget* btn_cube = gtk_button_new_with_label("Cube");
    GtkWidget* btn_cyl  = gtk_button_new_with_label("Cyl");
    GtkWidget* btn_sph  = gtk_button_new_with_label("Sphere");
    GtkWidget* btn_pyr  = gtk_button_new_with_label("Pyramid");
    GtkWidget* btn_pln  = gtk_button_new_with_label("Plane");
    GtkWidget* btn_obj  = gtk_button_new_with_label("Objects");
    g_signal_connect(btn_cube, "clicked", G_CALLBACK(btn_add_cube), ui);
    g_signal_connect(btn_cyl,  "clicked", G_CALLBACK(btn_add_cylinder), ui);
    g_signal_connect(btn_sph,  "clicked", G_CALLBACK(btn_add_sphere), ui);
    g_signal_connect(btn_pyr,  "clicked", G_CALLBACK(btn_add_pyramid), ui);
    g_signal_connect(btn_pln,  "clicked", G_CALLBACK(btn_add_plane), ui);
    g_signal_connect(btn_obj,  "clicked", G_CALLBACK(open_object_browser_clicked), ui);
    gtk_box_pack_start(GTK_BOX(left), btn_cube, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(left), btn_cyl,  FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(left), btn_sph,  FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(left), btn_pyr,  FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(left), btn_pln,  FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(left), btn_obj,  FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(content), left, FALSE, FALSE, 0);

    // Viewport: prefer GL when available and enabled; otherwise software DrawingArea
    const char* gdkgl_env = g_getenv("GDK_GL");
    const gboolean gl_disabled = (gdkgl_env && g_strcmp0(gdkgl_env, "disable") == 0);
    if (!gl_disabled) {
        GtkWidget* gl = gtk_gl_area_new();
        gtk_gl_area_set_required_version(GTK_GL_AREA(gl), 2, 1);
        gtk_gl_area_set_use_es(GTK_GL_AREA(gl), FALSE);
        gtk_gl_area_set_has_depth_buffer(GTK_GL_AREA(gl), TRUE);
        gtk_gl_area_set_has_alpha(GTK_GL_AREA(gl), FALSE);
        gtk_gl_area_set_has_stencil_buffer(GTK_GL_AREA(gl), FALSE);
        g_signal_connect(gl, "render", G_CALLBACK(gl_area_render), ui);
        g_signal_connect(gl, "realize", G_CALLBACK(gl_area_realize), ui);
        gtk_widget_set_hexpand(gl, TRUE);
        gtk_widget_set_vexpand(gl, TRUE);
        // Defer attaching to layout until after we try to make current once
        gtk_widget_realize(gl);
        gdk_error_trap_push();
        gtk_gl_area_make_current(GTK_GL_AREA(gl));
        int xerr = gdk_error_trap_pop();
        if (xerr == 0 && gtk_gl_area_get_error(GTK_GL_AREA(gl)) == NULL) {
            ui->gl_area = gl;
            ui->viewport = gl;
            gtk_box_pack_start(GTK_BOX(content), gl, TRUE, TRUE, 0);
            OZ_INFO("Using GtkGLArea viewport");
        } else {
            OZ_WARN("GLX/GDK error before attach (code=%d). Using software viewport.", xerr);
        }
    }
    if (!ui->viewport) {
        GtkWidget* da = gtk_drawing_area_new();
        OZ_INFO("Using software viewport (GtkDrawingArea)");
        gtk_widget_set_hexpand(da, TRUE);
        gtk_widget_set_vexpand(da, TRUE);
        gtk_widget_set_app_paintable(da, TRUE);
        gtk_widget_set_size_request(da, 320, 200);
        g_signal_connect(da, "draw", G_CALLBACK(fallback_draw), ui);
        if (!ui->redraw_id) ui->redraw_id = g_timeout_add(16, queue_draw_cb, ui);
        ui->gl_area = NULL;
        ui->viewport = da;
        gtk_box_pack_start(GTK_BOX(content), da, TRUE, TRUE, 0);
    }

    oz_map_init(&ui->map);
    // Defaults for gizmos and debug
    ui->gizmo_translate = TRUE; ui->gizmo_rotate = FALSE; ui->gizmo_scale = FALSE; ui->gizmo_axis = -1;
    ui->dbg_show_grid = TRUE; ui->dbg_show_axes = TRUE; ui->dbg_show_fps = TRUE;
    // Add content to window layout
    gtk_box_pack_start(GTK_BOX(ui->vbox), content, TRUE, TRUE, 0);

    // Simple HUD label below content
    ui->overlay = NULL;
    ui->info_label = gtk_label_new("");
    gtk_widget_set_halign(ui->info_label, GTK_ALIGN_END);
    gtk_widget_set_valign(ui->info_label, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(ui->vbox), ui->info_label, FALSE, FALSE, 0);
    // Input + timer
    gtk_widget_add_events(ui->window, GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK);
    gtk_widget_add_events(ui->viewport,
        GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK |
        GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
        GDK_POINTER_MOTION_MASK);
    // Route key/mouse to viewport; also connect at window level as a fallback
    GtkWidget* key_target = ui->viewport ? ui->viewport : ui->window;
    gtk_widget_set_can_focus(key_target, TRUE);
    gtk_widget_grab_focus(key_target);
    // Allow handlers to fetch EditorUi via widget data
    g_object_set_data(G_OBJECT(key_target), "oz_editor_ui", ui);
    g_signal_connect(key_target, "key-press-event", G_CALLBACK(on_key_press), &state);
    g_signal_connect(key_target, "key-release-event", G_CALLBACK(on_key_release), &state);
    g_signal_connect(key_target, "button-press-event", G_CALLBACK(on_button_press), &state);
    g_signal_connect(key_target, "button-release-event", G_CALLBACK(on_button_release), &state);
    g_signal_connect(key_target, "motion-notify-event", G_CALLBACK(on_motion), &state);
    // Redundant connections on window to ensure focus loss doesn't break movement
    g_object_set_data(G_OBJECT(ui->window), "oz_editor_ui", ui);
    g_signal_connect(ui->window, "key-press-event", G_CALLBACK(on_key_press), &state);
    g_signal_connect(ui->window, "key-release-event", G_CALLBACK(on_key_release), &state);
    ui->tick_id = g_timeout_add(16, tick_update, ui);
    g_signal_connect(ui->window, "delete-event", G_CALLBACK(on_window_delete), ui);
    gtk_widget_show_all(ui->window);
}

int main(int argc, char** argv) {
    OZ_INFO("OzWorld Editor starting (v%s)", oz_core_version());
    // Log environment diagnostics for GL/GTK troubleshooting
    const char* disp = g_getenv("DISPLAY");
    const char* libgl = g_getenv("LIBGL_ALWAYS_INDIRECT");
    const char* gdkgl = g_getenv("GDK_GL");
    OZ_INFO("DISPLAY=%s", disp ? disp : "(null)");
    OZ_INFO("LIBGL_ALWAYS_INDIRECT=%s", libgl ? libgl : "(null)");
    OZ_INFO("GDK_GL=%s", gdkgl ? gdkgl : "(null)");
    // Harden against GLX BadRequest on remote X servers (e.g., VcXsrv restarts):
    // Default to software rendering on remote DISPLAYs unless explicitly allowed.
    const gboolean is_remote_display = (disp && disp[0] != ':');
    const gboolean allow_gl_remote = env_flag_is_true("OZ_ALLOW_GL_REMOTE");
    if (is_remote_display) {
        OZ_WARN("Remote DISPLAY detected (%s).", disp);
        // Prefer indirect GL if present; otherwise disable GDK GL entirely to avoid GLX crashes.
        if (!allow_gl_remote) {
            if (!gdkgl || g_strcmp0(gdkgl, "disable") != 0) {
                g_setenv("GDK_GL", "disable", TRUE);
                OZ_WARN("Forcing GDK_GL=disable for stability on remote X. Set OZ_ALLOW_GL_REMOTE=1 to override.");
                gdkgl = g_getenv("GDK_GL");
            }
            if (!libgl || libgl[0] == '\0') {
                g_setenv("LIBGL_ALWAYS_INDIRECT", "1", FALSE);
                libgl = g_getenv("LIBGL_ALWAYS_INDIRECT");
            }
        }
    }
    OZ_INFO("Effective GDK_GL=%s", gdkgl ? gdkgl : "(null)");
    GtkApplication* app = gtk_application_new("com.ozworld.editor", G_APPLICATION_FLAGS_NONE);
    g_app_singleton = app;
    g_signal_connect(app, "activate", G_CALLBACK(on_activate), NULL);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
