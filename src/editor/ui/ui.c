#include <gtk/gtk.h>
#include <GL/gl.h>
#include "oz/oz_log.h"
#include "oz/oz_tex.h"
#include "oz/oz_bundle.h"
#include "oz/oz_debug.h"
#include "oz/oz_core.h"
#include "oz/editor_ui.h"
#include "editor.h"
#include "oz/oz_camera.h"

// Implementations of handlers moved from editor/main.c
static GtkWidget* build_template_list(const char** items, int count, GCallback on_activate, gpointer user_data);
static gboolean env_flag_is_true(const char* name) {
    const char* v = g_getenv(name); if (!v) return FALSE;
    return g_ascii_strcasecmp(v, "1") == 0 || g_ascii_strcasecmp(v, "true") == 0 || g_ascii_strcasecmp(v, "yes") == 0;
}

void action_quit(GSimpleAction* action, GVariant* parameter, gpointer user_data) {
    (void)action; (void)parameter; EditorUi* ui = (EditorUi*)user_data;
    if (ui) {
        ui->shutting_down = TRUE;
        if (ui->tick_id) { g_source_remove(ui->tick_id); ui->tick_id = 0; }
        if (ui->redraw_id) { g_source_remove(ui->redraw_id); ui->redraw_id = 0; }
        if (ui->window && GTK_IS_WIDGET(ui->window)) { gtk_widget_destroy(ui->window); }
    }
    GApplication* app = g_application_get_default(); if (app) g_application_quit(app);
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
        OZ_INFO("Selected: %s", filename);
        g_free(filename);
    }
    gtk_widget_destroy(dialog);
}

void action_open(GSimpleAction* action, GVariant* parameter, gpointer user_data) {
    (void)action; (void)parameter; EditorUi* ui = (EditorUi*)user_data;
    GtkWidget* dialog = gtk_file_chooser_dialog_new("Open Map",
        GTK_WINDOW(ui->window), GTK_FILE_CHOOSER_ACTION_OPEN,
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
        if (!oz_map_load_text(filename, &loaded)) { OZ_ERROR("Failed to load %s", filename); }
        else { oz_map_free(&ui->map); ui->map = loaded; g_free(ui->current_path); ui->current_path = g_strdup(filename); if (ui->viewport) gtk_widget_queue_draw(ui->viewport); }
        g_free(filename);
    }
    gtk_widget_destroy(dialog);
}

void action_save(GSimpleAction* action, GVariant* parameter, gpointer user_data) {
    (void)action; (void)parameter; EditorUi* ui = (EditorUi*)user_data;
    GtkWidget* dialog = gtk_file_chooser_dialog_new("Save Map",
        GTK_WINDOW(ui->window), GTK_FILE_CHOOSER_ACTION_SAVE,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Save", GTK_RESPONSE_ACCEPT,
        NULL);
    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog), TRUE);
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char* filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        char* to_save = NULL;
        if (filename && !(g_str_has_suffix(filename, ".ozone") || g_str_has_suffix(filename, ".ozmap"))) { to_save = g_strconcat(filename, ".ozone", NULL); }
        const char* path = to_save ? to_save : filename;
        OZ_INFO("Save file: %s", path);
        if (!oz_map_save_text(path, &ui->map)) { OZ_ERROR("Failed to save %s", filename); }
        else { g_free(ui->current_path); ui->current_path = g_strdup(path); if (ui->viewport) gtk_widget_queue_draw(ui->viewport); }
        if (to_save) g_free(to_save); g_free(filename);
    }
    gtk_widget_destroy(dialog);
}

// Import actions
static void choose_and_remember(EditorUi* ui, const char* title, const char* filter_name, const char* pattern, char** last_path_out);
static void on_import_texture(GSimpleAction* a, GVariant* p, gpointer user_data) {
    (void)a; (void)p; EditorUi* ui = (EditorUi*)user_data; if (!ui) return;
    choose_and_remember(ui, "Import Texture", "OzTex files (*.oztex)", "*.oztex", &ui->last_texture_path);
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
            oz_bundle_free(&b);
        } else {
            OZ_WARN("Failed to load bundle: %s", ui->last_bundle_path);
        }
    }
}
static void on_import_music(GSimpleAction* a, GVariant* p, gpointer user_data) {
    (void)a; (void)p; EditorUi* ui = (EditorUi*)user_data; if (!ui) return;
    choose_and_remember(ui, "Import Music", "OzMux files (*.ozmux)", "*.ozmux", &ui->last_music_path);
}

static void dialog_set_entry(GtkDialog* dlg, const char* label, GtkWidget** out_entry, const char* initial) {
    GtkWidget* content = gtk_dialog_get_content_area(dlg);
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget* lab = gtk_label_new(label);
    GtkWidget* ent = gtk_entry_new(); gtk_entry_set_text(GTK_ENTRY(ent), initial);
    gtk_box_pack_start(GTK_BOX(box), lab, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), ent, TRUE, TRUE, 0);
    gtk_container_add(GTK_CONTAINER(content), box);
    if (out_entry) *out_entry = ent;
}

static void action_build_common(const char* what) { OZ_INFO("Build: %s (stub)", what); }
void action_build_map(GSimpleAction* a, GVariant* p, gpointer u) { (void)a; (void)p; (void)u; action_build_common("Map"); }
void action_build_light(GSimpleAction* a, GVariant* p, gpointer u) { (void)a; (void)p; (void)u; action_build_common("Light"); }
void action_build_bsp(GSimpleAction* a, GVariant* p, gpointer u) { (void)a; (void)p; (void)u; action_build_common("BSP"); }
void action_build_brushes(GSimpleAction* a, GVariant* p, gpointer u) { (void)a; (void)p; (void)u; action_build_common("Brushes"); }

static void spawn_process(char const* const argv[]) {
    GError* error = NULL; gchar** envp = g_get_environ();
    const gchar* disp = g_environ_getenv(envp, "DISPLAY"); const gboolean is_remote = (disp && disp[0] != ':');
    const gboolean allow_gl = env_flag_is_true("OZ_ALLOW_GL_REMOTE");
    envp = g_environ_setenv(envp, "LIBGL_DEBUG", "verbose", TRUE);
    envp = g_environ_setenv(envp, "MESA_DEBUG", "1", TRUE);
    if (is_remote && !allow_gl) {
        envp = g_environ_setenv(envp, "LIBGL_ALWAYS_INDIRECT", "1", TRUE);
        envp = g_environ_setenv(envp, "GDK_GL", "disable", TRUE);
        envp = g_environ_setenv(envp, "OZ_FORCE_SOFTWARE", "1", TRUE);
    }
    gboolean ok = g_spawn_async(NULL, (gchar**)argv, envp, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, &error);
    if (!ok) { OZ_ERROR("Failed to launch: %s", error ? error->message : "unknown error"); if (error) g_error_free(error); }
    if (envp) g_strfreev(envp);
}
void action_launch_editor(GSimpleAction* a, GVariant* p, gpointer u) { (void)a; (void)p; (void)u; const char* argv[] = { "./build/oz_editor", NULL }; spawn_process(argv); }
void action_launch_game(GSimpleAction* a, GVariant* p, gpointer u) {
    (void)a; (void)p; EditorUi* ui = (EditorUi*)u; char arg_pos[128] = {0}; int found = -1;
    if (ui && ui->obj_count) { for (size_t i = 0; i < ui->obj_count; ++i) if (ui->objects[i].type == OBJ_PLAYER_START) { found = (int)i; break; } }
    const char* argv_default[] = { "./build/oz_demo", NULL };
    char* argv_custom[7] = { (char*)"./build/oz_demo", (char*)"--playerstart", arg_pos, (char*)"--mode", (char*)"freemove", NULL, NULL };
    if (found >= 0) {
        const ObjPlayerStartProps* ps = &ui->objects[found].as.pstart;
        g_snprintf(arg_pos, sizeof(arg_pos), "%g,%g,%g,%g", (double)ps->position[0], (double)ps->position[1], (double)ps->position[2], (double)ps->yaw);
        const char* mode = "freemove";
        if (ps->camera_mode == OZ_CAMERA_FPS) mode = "fps";
        else if (ps->camera_mode == OZ_CAMERA_CINEMATIC) mode = "cinematic";
        argv_custom[4] = (char*)mode;
        spawn_process((const char* const*)argv_custom);
    } else { spawn_process(argv_default); }
}
void action_launch_server(GSimpleAction* a, GVariant* p, gpointer u) { (void)a; (void)p; (void)u; OZ_WARN("Server launch not implemented"); }

void action_bsp_box_dialog(GSimpleAction* a, GVariant* p, gpointer user_data);
void action_bsp_cyl_dialog(GSimpleAction* a, GVariant* p, gpointer user_data);
void action_csg_add(GSimpleAction* a, GVariant* p, gpointer user_data);
void action_csg_sub(GSimpleAction* a, GVariant* p, gpointer user_data);
void action_csg_isect(GSimpleAction* a, GVariant* p, gpointer user_data);
void action_tool_carve(GSimpleAction* a, GVariant* p, gpointer user_data);
void action_tool_slope(GSimpleAction* a, GVariant* p, gpointer user_data);
void action_view_grab_focus(GSimpleAction* a, GVariant* p, gpointer user_data) {
    (void)a; (void)p; EditorUi* ui = (EditorUi*)user_data;
    if (ui && ui->viewport) { gtk_widget_set_can_focus(ui->viewport, TRUE); gtk_widget_grab_focus(ui->viewport); OZ_INFO("Viewport focus grabbed"); }
}
void action_gizmo_translate(GSimpleAction* a, GVariant* p, gpointer user_data) { (void)a; (void)p; EditorUi* ui=(EditorUi*)user_data; ui->gizmo_translate=TRUE; ui->gizmo_rotate=FALSE; ui->gizmo_scale=FALSE; }
void action_gizmo_rotate(GSimpleAction* a, GVariant* p, gpointer user_data)    { (void)a; (void)p; EditorUi* ui=(EditorUi*)user_data; ui->gizmo_translate=FALSE; ui->gizmo_rotate=TRUE; ui->gizmo_scale=FALSE; }
void action_gizmo_scale(GSimpleAction* a, GVariant* p, gpointer user_data)     { (void)a; (void)p; EditorUi* ui=(EditorUi*)user_data; ui->gizmo_translate=FALSE; ui->gizmo_rotate=FALSE; ui->gizmo_scale=TRUE; }
void action_debug_toggle_grid(GSimpleAction* a, GVariant* p, gpointer user_data) { (void)a; (void)p; EditorUi* ui=(EditorUi*)user_data; ui->dbg_show_grid = !ui->dbg_show_grid; if (ui->viewport) gtk_widget_queue_draw(ui->viewport); }
void action_debug_toggle_axes(GSimpleAction* a, GVariant* p, gpointer user_data) { (void)a; (void)p; EditorUi* ui=(EditorUi*)user_data; ui->dbg_show_axes = !ui->dbg_show_axes; if (ui->viewport) gtk_widget_queue_draw(ui->viewport); }
void action_debug_toggle_fps(GSimpleAction* a, GVariant* p, gpointer user_data) { (void)a; (void)p; EditorUi* ui=(EditorUi*)user_data; ui->dbg_show_fps = !ui->dbg_show_fps; if (!ui->dbg_show_fps) gtk_label_set_text(GTK_LABEL(ui->info_label), ""); }

// Log window and debug info actions
typedef struct LogWindowCtx { GtkWidget* win; GtkWidget* view; GtkTextBuffer* buf; } LogWindowCtx;
static LogWindowCtx* g_log_ctx = NULL;
static void log_sink_bridge(OzLogLevel level, const char* line) { (void)level; if (!g_log_ctx || !g_log_ctx->buf) return; GtkTextIter end; gtk_text_buffer_get_end_iter(g_log_ctx->buf, &end); gtk_text_buffer_insert(g_log_ctx->buf, &end, line, -1); gtk_text_buffer_insert(g_log_ctx->buf, &end, "\n", -1); }
void action_debug_open_log(GSimpleAction* a, GVariant* p, gpointer user_data) {
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
#ifndef _WIN32
#include <execinfo.h>
#include <sys/resource.h>
#include <link.h>
#endif
#include <math.h>
#include <stdint.h>
#include <stdio.h>
static gchar* get_memory_info_string(void) {
#ifndef _WIN32
    struct rusage ru; getrusage(RUSAGE_SELF, &ru);
    return g_strdup_printf("RSS: %ld KB  MinorFaults: %ld  MajorFaults: %ld", (long)ru.ru_maxrss, (long)ru.ru_minflt, (long)ru.ru_majflt);
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
void action_debug_open_debug(GSimpleAction* a, GVariant* p, gpointer user_data) {
    (void)a; (void)p; EditorUi* ui = (EditorUi*)user_data;
    if (ui->debug_window && GTK_IS_WIDGET(ui->debug_window)) { gtk_window_present(GTK_WINDOW(ui->debug_window)); return; }
    GtkWidget* win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), "Debug Info");
    gtk_window_set_default_size(GTK_WINDOW(win), 560, 380);
    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_add(GTK_CONTAINER(win), vbox);
    gchar* mem = get_memory_info_string(); GtkWidget* mem_label = gtk_label_new(mem); g_free(mem);
    gtk_box_pack_start(GTK_BOX(vbox), mem_label, FALSE, FALSE, 0);
    GtkWidget* watch_sc = gtk_scrolled_window_new(NULL, NULL); gtk_box_pack_start(GTK_BOX(vbox), watch_sc, TRUE, TRUE, 0);
    GtkWidget* watch_tv = gtk_text_view_new(); gtk_text_view_set_editable(GTK_TEXT_VIEW(watch_tv), FALSE); gtk_container_add(GTK_CONTAINER(watch_sc), watch_tv);
    GtkTextBuffer* watch_buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(watch_tv)); GtkTextIter wend; gtk_text_buffer_get_end_iter(watch_buf, &wend);
    gtk_text_buffer_insert(watch_buf, &wend, "Watches:\n", -1);
    for (size_t i = 0; i < oz_debug_watch_count(); ++i) {
        const char* nm = oz_debug_watch_name(i); OzDebugType tp = oz_debug_watch_type(i); const void* addr = oz_debug_watch_address(i);
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
    GtkWidget* sc = gtk_scrolled_window_new(NULL, NULL); gtk_box_pack_start(GTK_BOX(vbox), sc, TRUE, TRUE, 0);
    GtkWidget* tv = gtk_text_view_new(); gtk_text_view_set_editable(GTK_TEXT_VIEW(tv), FALSE); gtk_container_add(GTK_CONTAINER(sc), tv);
    GtkTextBuffer* buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(tv)); fill_stack_trace(buf);
    GtkWidget* asm_sc = gtk_scrolled_window_new(NULL, NULL); gtk_box_pack_start(GTK_BOX(vbox), asm_sc, TRUE, TRUE, 0);
    GtkWidget* asm_tv = gtk_text_view_new(); gtk_text_view_set_editable(GTK_TEXT_VIEW(asm_tv), FALSE); gtk_container_add(GTK_CONTAINER(asm_sc), asm_tv);
    GtkTextBuffer* asm_buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(asm_tv)); GtkTextIter aend; gtk_text_buffer_get_end_iter(asm_buf, &aend);
#ifndef _WIN32
    void* addrs[64]; int n = backtrace(addrs, 64);
    if (n >= 2) {
        uintptr_t addr = (uintptr_t)addrs[1]; uintptr_t start = addr > 64 ? addr - 64 : addr; uintptr_t stop  = addr + 128; char cmd[1024];
        snprintf(cmd, sizeof(cmd), "sh -lc 'objdump -d --no-show-raw-insn --start-address=0x%lx --stop-address=0x%lx /proc/self/exe 2>/dev/null || llvm-objdump -d --no-show-raw-insn --start-address=0x%lx --stop-address=0x%lx /proc/self/exe 2>/dev/null'",
                 (unsigned long)start, (unsigned long)stop, (unsigned long)start, (unsigned long)stop);
        FILE* fp = popen(cmd, "r");
        if (fp) { char line[512]; while (fgets(line, sizeof(line), fp)) { gtk_text_buffer_insert(asm_buf, &aend, line, -1); } pclose(fp); }
        else { gtk_text_buffer_insert(asm_buf, &aend, "Failed to run objdump/llvm-objdump.\n", -1); }
    } else { gtk_text_buffer_insert(asm_buf, &aend, "No stack frames available for disassembly.\n", -1); }
#else
    gtk_text_buffer_insert(asm_buf, &aend, "Disassembly not available on this platform.\n", -1);
#endif
    gtk_widget_show_all(win); ui->debug_window = win;
}
void action_debug_jit_example(GSimpleAction* a, GVariant* p, gpointer user_data) {
    (void)a; (void)p; (void)user_data; const char* src = "#include <stdio.h>\nint ozjit_add(int a,int b){return a+b;}\n"; void* h = NULL;
    if (!oz_jit_compile_and_load("ozjit_tmp", src, "-O2 -fPIC", &h)) { OZ_ERROR("JIT compile failed"); return; }
    typedef int (*AddFn)(int,int); AddFn fn = (AddFn)oz_jit_get_symbol(h, "ozjit_add"); if (!fn) { OZ_ERROR("JIT: symbol not found"); oz_jit_unload(h); return; }
    int r = fn(40, 2); OZ_INFO("JIT result ozjit_add(40,2)=%d", r); oz_jit_unload(h);
}
// Object browser and helpers migrated from main.c
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

static void place_near_camera(float pos[3]);
static void place_near_camera(float pos[3]) {
    // Fallback placement near origin; avoids depending on editor input internals here
    pos[0] = 2.0f; pos[1] = 0.0f; pos[2] = 0.0f;
}

static int objects_add(EditorUi* ui, const EditorObject* obj) {
    if (!ui || !obj) return -1;
    size_t nc = ui->obj_capacity ? ui->obj_capacity : 8;
    while (nc < ui->obj_count + 1) nc *= 2;
    if (nc != ui->obj_capacity) {
        EditorObject* mem = (EditorObject*)realloc(ui->objects, nc * sizeof(EditorObject));
        if (!mem) return -1; ui->objects = mem; ui->obj_capacity = nc;
    }
    ui->objects[ui->obj_count] = *obj; return (int)ui->obj_count++;
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
            {
                GtkWidget* hb = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
                gtk_box_pack_start(GTK_BOX(vbox), hb, FALSE, FALSE, 0);
                gtk_box_pack_start(GTK_BOX(hb), gtk_label_new("Radius"), FALSE, FALSE, 0);
                GtkWidget* entry = gtk_entry_new(); char buf[32]; g_snprintf(buf, sizeof(buf), "%.2f", o->as.zone.radius);
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
            gtk_box_pack_start(GTK_BOX(vbox), gtk_label_new("Camera Mode"), FALSE, FALSE, 0);
            GtkWidget* combo = gtk_combo_box_text_new();
            gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), "FreeMove");
            gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), "FPS");
            gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), "Cinematic");
            gtk_combo_box_set_active(GTK_COMBO_BOX(combo), (int)o->as.pstart.camera_mode);
            PropCtx* pc_cam = g_new0(PropCtx, 1); pc_cam->ui = ui; pc_cam->index = index;
            g_signal_connect(combo, "changed", G_CALLBACK(on_pstart_cam_changed), pc_cam);
            gtk_box_pack_start(GTK_BOX(vbox), combo, FALSE, FALSE, 0);
            {
                GtkWidget* hb = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
                gtk_box_pack_start(GTK_BOX(vbox), hb, FALSE, FALSE, 0);
                gtk_box_pack_start(GTK_BOX(hb), gtk_label_new("Yaw (deg)"), FALSE, FALSE, 0);
                GtkWidget* entry = gtk_entry_new(); char buf[32]; g_snprintf(buf, sizeof(buf), "%.1f", o->as.pstart.yaw * 57.29578f);
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
    if (idx >= 0 && (size_t)idx < ui->obj_count) { ui->selected_object = idx; open_object_properties(ui, idx); }
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
    EditorObject o = {0}; o.type = OBJ_PLAYER_START; place_near_camera(o.as.pstart.position); o.as.pstart.yaw = 0.0f; o.as.pstart.camera_mode = OZ_CAMERA_FPS;
    int idx = objects_add(ui, &o); ui->selected_object = idx; if (ui->obj_scene_list && GTK_IS_LIST_BOX(ui->obj_scene_list)) {
        char buf[128]; snprintf(buf, sizeof(buf), "PlayerStart #%d", idx);
        gtk_list_box_insert(GTK_LIST_BOX(ui->obj_scene_list), gtk_label_new(buf), -1);
        gtk_widget_show_all(ui->obj_scene_list);
    }
    if (ui->viewport) gtk_widget_queue_draw(ui->viewport);
}
void action_open_object_browser(GSimpleAction* a, GVariant* p, gpointer user_data) {
    (void)a; (void)p; EditorUi* ui = (EditorUi*)user_data; if (!ui) return;
    if (ui->obj_browser_win && GTK_IS_WIDGET(ui->obj_browser_win)) { gtk_window_present(GTK_WINDOW(ui->obj_browser_win)); return; }
    GtkWidget* win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), "Object Browser");
    gtk_window_set_default_size(GTK_WINDOW(win), 520, 420);
    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_add(GTK_CONTAINER(win), vbox);
    GtkWidget* notebook = gtk_notebook_new(); gtk_box_pack_start(GTK_BOX(vbox), notebook, TRUE, TRUE, 0);
    const char* zones[] = { "Zone" }; GtkWidget* zones_list = build_template_list(zones, 1, G_CALLBACK(add_zone_template), ui);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), zones_list, gtk_label_new("Zones"));
    const char* picks[] = { "Health", "Ammo" }; GtkWidget* picks_list = build_template_list(picks, 2, G_CALLBACK(add_pickup_template), ui);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), picks_list, gtk_label_new("Pickups"));
    GtkWidget* assets_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    if (ui->last_texture_path) gtk_box_pack_start(GTK_BOX(assets_box), gtk_label_new(ui->last_texture_path), FALSE, FALSE, 0);
    if (ui->last_bundle_path)  gtk_box_pack_start(GTK_BOX(assets_box), gtk_label_new(ui->last_bundle_path), FALSE, FALSE, 0);
    if (ui->last_music_path)   gtk_box_pack_start(GTK_BOX(assets_box), gtk_label_new(ui->last_music_path), FALSE, FALSE, 0);
    if (!ui->last_texture_path && !ui->last_bundle_path && !ui->last_music_path) gtk_box_pack_start(GTK_BOX(assets_box), gtk_label_new("No assets imported yet. Use File → Import."), FALSE, FALSE, 0);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), assets_box, gtk_label_new("Assets"));
    const char* nodes[] = { "Player Start" }; GtkWidget* nodes_list = build_template_list(nodes, 1, G_CALLBACK(add_playerstart_template), ui);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), nodes_list, gtk_label_new("Nodes"));
    GtkWidget* scene_frame = gtk_frame_new("Scene Objects (double-click to edit)"); GtkWidget* scene_list = gtk_list_box_new();
    for (size_t i = 0; i < ui->obj_count; ++i) { const EditorObject* o = &ui->objects[i]; char buf[128]; const char* tp = (o->type==OBJ_ZONE?"Zone":(o->type==OBJ_PICKUP?"Pickup":"PlayerStart")); snprintf(buf, sizeof(buf), "%s #%zu", tp, i); gtk_list_box_insert(GTK_LIST_BOX(scene_list), gtk_label_new(buf), -1); }
    g_signal_connect(scene_list, "row-activated", G_CALLBACK(object_list_row_activated), ui);
    gtk_container_add(GTK_CONTAINER(scene_frame), scene_list);
    gtk_box_pack_start(GTK_BOX(vbox), scene_frame, FALSE, FALSE, 0);
    gtk_widget_show_all(win); ui->obj_browser_win = win; ui->obj_scene_list = scene_list;
}

// Simple helper to build a list with activation callback
static GtkWidget* build_template_list(const char** items, int count, GCallback on_activate, gpointer user_data) {
    GtkWidget* list = gtk_list_box_new();
    for (int i = 0; i < count; ++i) gtk_list_box_insert(GTK_LIST_BOX(list), gtk_label_new(items[i]), -1);
    g_signal_connect(list, "row-activated", on_activate, user_data);
    return list;
}

static void on_toolbar_open(GtkToolButton* btn, gpointer user_data) {
    (void)btn; (void)user_data;
    GActionGroup* ag = G_ACTION_GROUP(g_application_get_default());
    if (ag) g_action_group_activate_action(ag, "open", NULL);
}
static void on_toolbar_save(GtkToolButton* btn, gpointer user_data) {
    (void)btn; (void)user_data;
    GActionGroup* ag = G_ACTION_GROUP(g_application_get_default());
    if (ag) g_action_group_activate_action(ag, "save", NULL);
}
GtkWidget* editor_ui_build_toolbar(struct EditorUi* ui) {
    (void)ui;
    GtkWidget* tb = gtk_toolbar_new();
    GtkToolItem* open_btn = gtk_tool_button_new(NULL, "Open");
    GtkToolItem* save_btn = gtk_tool_button_new(NULL, "Save");
    g_signal_connect(open_btn, "clicked", G_CALLBACK(on_toolbar_open), ui);
    g_signal_connect(save_btn, "clicked", G_CALLBACK(on_toolbar_save), ui);
    gtk_toolbar_insert(GTK_TOOLBAR(tb), open_btn, -1);
    gtk_toolbar_insert(GTK_TOOLBAR(tb), save_btn, -1);
    return tb;
}

static void build_menu_file(GtkApplication* app, struct EditorUi* ui, GMenu* menubar) {
    (void)app; (void)ui;
    GMenu* file_menu = g_menu_new();
    g_menu_append(file_menu, "Open",  "app.open");
    g_menu_append(file_menu, "Save",  "app.save");
    // Import submenu
    GMenu* import_menu = g_menu_new();
    g_menu_append(import_menu, "Import Texture (.oztex)", "app.import_texture");
    g_menu_append(import_menu, "Import Mesh Bundle (.ozbag)", "app.import_bundle");
    g_menu_append(import_menu, "Import Music (.ozmux)", "app.import_music");
    g_menu_append_submenu(file_menu, "Import", G_MENU_MODEL(import_menu));
    // Launch section
    GMenu* file_section_launch = g_menu_new();
    g_menu_append(file_section_launch, "Launch Editor", "app.launch_editor");
    g_menu_append(file_section_launch, "Launch Game",   "app.launch_game");
    g_menu_append(file_section_launch, "Launch Server", "app.launch_server");
    g_menu_append_section(file_menu, NULL, G_MENU_MODEL(file_section_launch));
    // Quit
    g_menu_append(file_menu, "Quit",  "app.quit");
    g_menu_append_submenu(menubar, "File", G_MENU_MODEL(file_menu));
    g_object_unref(file_menu);
    g_object_unref(import_menu);
    g_object_unref(file_section_launch);
}

static void build_menu_view(GtkApplication* app, struct EditorUi* ui, GMenu* menubar) {
    (void)app; (void)ui;
    GMenu* view_menu = g_menu_new();
    g_menu_append(view_menu, "Grab Focus", "app.view_grab_focus");
    g_menu_append(view_menu, "Gizmo: Translate", "app.gizmo_translate");
    g_menu_append(view_menu, "Gizmo: Rotate", "app.gizmo_rotate");
    g_menu_append(view_menu, "Gizmo: Scale", "app.gizmo_scale");
    g_menu_append(view_menu, "Object Browser...", "app.open_object_browser");
    g_menu_append_submenu(menubar, "View", G_MENU_MODEL(view_menu));
    g_object_unref(view_menu);
}

static void build_menu_light(GtkApplication* app, struct EditorUi* ui, GMenu* menubar) {
    (void)app; (void)ui;
    GMenu* light_menu = g_menu_new();
    g_menu_append(light_menu, "Build Light", "app.build_light");
    g_menu_append_submenu(menubar, "Light", G_MENU_MODEL(light_menu));
    g_object_unref(light_menu);
}

void editor_ui_populate_menus(GtkApplication* app, struct EditorUi* ui) {
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

    GMenu* menubar = g_menu_new();
    build_menu_file(app, ui, menubar);
    build_menu_view(app, ui, menubar);
    build_menu_light(app, ui, menubar);
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
