#include "oz/editor/ui.h"
#include "oz/oz_log.h"
#include <gtk/gtk.h>

// Some distributions lack g_menu_append_separator; emulate with empty section
static void add_menu_separator(GMenu* menu) {
    GMenu* section = g_menu_new();
    g_menu_append_section(menu, NULL, G_MENU_MODEL(section));
    g_object_unref(section);
}

// Forward declarations for action handlers
static void action_file_new(GSimpleAction* action, GVariant* parameter, gpointer user_data);
static void action_file_open(GSimpleAction* action, GVariant* parameter, gpointer user_data);
static void action_file_save(GSimpleAction* action, GVariant* parameter, gpointer user_data);
static void action_file_save_as(GSimpleAction* action, GVariant* parameter, gpointer user_data);
static void action_file_export_bsp(GSimpleAction* action, GVariant* parameter, gpointer user_data);
static void action_file_quit(GSimpleAction* action, GVariant* parameter, gpointer user_data);

static void action_edit_undo(GSimpleAction* action, GVariant* parameter, gpointer user_data);
static void action_edit_redo(GSimpleAction* action, GVariant* parameter, gpointer user_data);
static void action_edit_cut(GSimpleAction* action, GVariant* parameter, gpointer user_data);
static void action_edit_copy(GSimpleAction* action, GVariant* parameter, gpointer user_data);
static void action_edit_paste(GSimpleAction* action, GVariant* parameter, gpointer user_data);
static void action_edit_delete(GSimpleAction* action, GVariant* parameter, gpointer user_data);
static void action_edit_select_all(GSimpleAction* action, GVariant* parameter, gpointer user_data);

static void action_view_grid(GSimpleAction* action, GVariant* parameter, gpointer user_data);
static void action_view_axes(GSimpleAction* action, GVariant* parameter, gpointer user_data);
static void action_view_wireframe(GSimpleAction* action, GVariant* parameter, gpointer user_data);
static void action_view_lighting(GSimpleAction* action, GVariant* parameter, gpointer user_data);
static void action_view_reset_camera(GSimpleAction* action, GVariant* parameter, gpointer user_data);

static void action_bsp_add_box(GSimpleAction* action, GVariant* parameter, gpointer user_data);
static void action_bsp_add_cylinder(GSimpleAction* action, GVariant* parameter, gpointer user_data);
static void action_bsp_add_sphere(GSimpleAction* action, GVariant* parameter, gpointer user_data);
static void action_bsp_csg_union(GSimpleAction* action, GVariant* parameter, gpointer user_data);
static void action_bsp_csg_subtract(GSimpleAction* action, GVariant* parameter, gpointer user_data);
static void action_bsp_csg_intersect(GSimpleAction* action, GVariant* parameter, gpointer user_data);

static void action_tools_preferences(GSimpleAction* action, GVariant* parameter, gpointer user_data);
static void action_help_about(GSimpleAction* action, GVariant* parameter, gpointer user_data);

void editor_ui_build_menus(EditorUI* ui, GtkApplication* app) {
    if (!ui || !app) return;
    
    // Action entries
    static const GActionEntry action_entries[] = {
        // File menu
        {"file_new", action_file_new, NULL, NULL, NULL},
        {"file_open", action_file_open, NULL, NULL, NULL},
        {"file_save", action_file_save, NULL, NULL, NULL},
        {"file_save_as", action_file_save_as, NULL, NULL, NULL},
        {"file_export_bsp", action_file_export_bsp, NULL, NULL, NULL},
        {"file_quit", action_file_quit, NULL, NULL, NULL},
        
        // Edit menu
        {"edit_undo", action_edit_undo, NULL, NULL, NULL},
        {"edit_redo", action_edit_redo, NULL, NULL, NULL},
        {"edit_cut", action_edit_cut, NULL, NULL, NULL},
        {"edit_copy", action_edit_copy, NULL, NULL, NULL},
        {"edit_paste", action_edit_paste, NULL, NULL, NULL},
        {"edit_delete", action_edit_delete, NULL, NULL, NULL},
        {"edit_select_all", action_edit_select_all, NULL, NULL, NULL},
        
        // View menu
        {"view_grid", action_view_grid, NULL, "true", NULL},
        {"view_axes", action_view_axes, NULL, "true", NULL},
        {"view_wireframe", action_view_wireframe, NULL, "false", NULL},
        {"view_lighting", action_view_lighting, NULL, "false", NULL},
        {"view_reset_camera", action_view_reset_camera, NULL, NULL, NULL},
        
        // BSP menu
        {"bsp_add_box", action_bsp_add_box, NULL, NULL, NULL},
        {"bsp_add_cylinder", action_bsp_add_cylinder, NULL, NULL, NULL},
        {"bsp_add_sphere", action_bsp_add_sphere, NULL, NULL, NULL},
        {"bsp_csg_union", action_bsp_csg_union, NULL, NULL, NULL},
        {"bsp_csg_subtract", action_bsp_csg_subtract, NULL, NULL, NULL},
        {"bsp_csg_intersect", action_bsp_csg_intersect, NULL, NULL, NULL},
        
        // Tools menu
        {"tools_preferences", action_tools_preferences, NULL, NULL, NULL},
        
        // Help menu
        {"help_about", action_help_about, NULL, NULL, NULL},
    };
    
    g_action_map_add_action_entries(G_ACTION_MAP(app), action_entries,
                                    G_N_ELEMENTS(action_entries), ui);
    
    // Create menu structure
    GMenu* menubar = g_menu_new();
    
    // File menu
    GMenu* file_menu = g_menu_new();
    g_menu_append(file_menu, "_New", "app.file_new");
    g_menu_append(file_menu, "_Open...", "app.file_open");
    add_menu_separator(file_menu);
    g_menu_append(file_menu, "_Save", "app.file_save");
    g_menu_append(file_menu, "Save _As...", "app.file_save_as");
    add_menu_separator(file_menu);
    g_menu_append(file_menu, "_Export BSP...", "app.file_export_bsp");
    add_menu_separator(file_menu);
    g_menu_append(file_menu, "_Quit", "app.file_quit");
    g_menu_append_submenu(menubar, "_File", G_MENU_MODEL(file_menu));
    
    // Edit menu
    GMenu* edit_menu = g_menu_new();
    g_menu_append(edit_menu, "_Undo", "app.edit_undo");
    g_menu_append(edit_menu, "_Redo", "app.edit_redo");
    add_menu_separator(edit_menu);
    g_menu_append(edit_menu, "Cu_t", "app.edit_cut");
    g_menu_append(edit_menu, "_Copy", "app.edit_copy");
    g_menu_append(edit_menu, "_Paste", "app.edit_paste");
    g_menu_append(edit_menu, "_Delete", "app.edit_delete");
    add_menu_separator(edit_menu);
    g_menu_append(edit_menu, "Select _All", "app.edit_select_all");
    g_menu_append_submenu(menubar, "_Edit", G_MENU_MODEL(edit_menu));
    
    // View menu
    GMenu* view_menu = g_menu_new();
    g_menu_append(view_menu, "Show _Grid", "app.view_grid");
    g_menu_append(view_menu, "Show _Axes", "app.view_axes");
    g_menu_append(view_menu, "Show _Wireframe", "app.view_wireframe");
    g_menu_append(view_menu, "Enable _Lighting", "app.view_lighting");
    add_menu_separator(view_menu);
    g_menu_append(view_menu, "_Reset Camera", "app.view_reset_camera");
    g_menu_append_submenu(menubar, "_View", G_MENU_MODEL(view_menu));
    
    // BSP menu
    GMenu* bsp_menu = g_menu_new();
    GMenu* bsp_add_section = g_menu_new();
    g_menu_append(bsp_add_section, "Add _Box", "app.bsp_add_box");
    g_menu_append(bsp_add_section, "Add _Cylinder", "app.bsp_add_cylinder");
    g_menu_append(bsp_add_section, "Add _Sphere", "app.bsp_add_sphere");
    g_menu_append_section(bsp_menu, "Add Brushes", G_MENU_MODEL(bsp_add_section));
    
    GMenu* bsp_csg_section = g_menu_new();
    g_menu_append(bsp_csg_section, "CSG _Union", "app.bsp_csg_union");
    g_menu_append(bsp_csg_section, "CSG _Subtract", "app.bsp_csg_subtract");
    g_menu_append(bsp_csg_section, "CSG _Intersect", "app.bsp_csg_intersect");
    g_menu_append_section(bsp_menu, "CSG Operations", G_MENU_MODEL(bsp_csg_section));
    g_menu_append_submenu(menubar, "_BSP", G_MENU_MODEL(bsp_menu));
    
    // Tools menu
    GMenu* tools_menu = g_menu_new();
    g_menu_append(tools_menu, "_Preferences...", "app.tools_preferences");
    g_menu_append_submenu(menubar, "_Tools", G_MENU_MODEL(tools_menu));
    
    // Help menu
    GMenu* help_menu = g_menu_new();
    g_menu_append(help_menu, "_About OzWorld Editor", "app.help_about");
    g_menu_append_submenu(menubar, "_Help", G_MENU_MODEL(help_menu));
    
    // Set the menubar
    gtk_application_set_menubar(app, G_MENU_MODEL(menubar));
    
    // Cleanup
    g_object_unref(menubar);
    g_object_unref(file_menu);
    g_object_unref(edit_menu);
    g_object_unref(view_menu);
    g_object_unref(bsp_menu);
    g_object_unref(bsp_add_section);
    g_object_unref(bsp_csg_section);
    g_object_unref(tools_menu);
    g_object_unref(help_menu);
    
    OZ_INFO("Menu system initialized");
}

// Action implementations (stubs for now)
static void action_file_new(GSimpleAction* action, GVariant* parameter, gpointer user_data) {
    (void)action; (void)parameter;
    EditorUI* ui = (EditorUI*)user_data;
    OZ_INFO("File -> New (TODO: implement)");
    editor_ui_update_status(ui, "New scene created");
}

static void action_file_open(GSimpleAction* action, GVariant* parameter, gpointer user_data) {
    (void)action; (void)parameter;
    EditorUI* ui = (EditorUI*)user_data;
    
    GtkWidget* dialog = gtk_file_chooser_dialog_new("Open Scene",
        GTK_WINDOW(ui->window), GTK_FILE_CHOOSER_ACTION_OPEN,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Open", GTK_RESPONSE_ACCEPT, NULL);
    
    // Add file filters
    GtkFileFilter* filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "OzWorld Scene Files (*.ozone)");
    gtk_file_filter_add_pattern(filter, "*.ozone");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);
    
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char* filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        OZ_INFO("Opening file: %s", filename);
        // TODO: Implement actual file loading
        editor_ui_update_title(ui, filename);
        editor_ui_update_status(ui, "Scene loaded");
        g_free(filename);
    }
    
    gtk_widget_destroy(dialog);
}

static void action_file_save(GSimpleAction* action, GVariant* parameter, gpointer user_data) {
    (void)action; (void)parameter;
    EditorUI* ui = (EditorUI*)user_data;
    OZ_INFO("File -> Save (TODO: implement)");
    editor_ui_update_status(ui, "Scene saved");
}

static void action_file_save_as(GSimpleAction* action, GVariant* parameter, gpointer user_data) {
    (void)action; (void)parameter;
    EditorUI* ui = (EditorUI*)user_data;
    OZ_INFO("File -> Save As (TODO: implement)");
    editor_ui_update_status(ui, "Scene saved as...");
}

static void action_file_export_bsp(GSimpleAction* action, GVariant* parameter, gpointer user_data) {
    (void)action; (void)parameter;
    EditorUI* ui = (EditorUI*)user_data;
    OZ_INFO("File -> Export BSP (TODO: implement)");
    editor_ui_update_status(ui, "BSP exported");
}

static void action_file_quit(GSimpleAction* action, GVariant* parameter, gpointer user_data) {
    (void)action; (void)parameter;
    EditorUI* ui = (EditorUI*)user_data;
    
    // TODO: Check for unsaved changes
    ui->shutting_down = TRUE;
    
    GApplication* app = g_application_get_default();
    if (app) {
        g_application_quit(app);
    }
}

// Edit actions (stubs)
static void action_edit_undo(GSimpleAction* action, GVariant* parameter, gpointer user_data) {
    (void)action; (void)parameter; (void)user_data;
    OZ_INFO("Edit -> Undo (TODO: implement)");
}

static void action_edit_redo(GSimpleAction* action, GVariant* parameter, gpointer user_data) {
    (void)action; (void)parameter; (void)user_data;
    OZ_INFO("Edit -> Redo (TODO: implement)");
}

static void action_edit_cut(GSimpleAction* action, GVariant* parameter, gpointer user_data) {
    (void)action; (void)parameter; (void)user_data;
    OZ_INFO("Edit -> Cut (TODO: implement)");
}

static void action_edit_copy(GSimpleAction* action, GVariant* parameter, gpointer user_data) {
    (void)action; (void)parameter; (void)user_data;
    OZ_INFO("Edit -> Copy (TODO: implement)");
}

static void action_edit_paste(GSimpleAction* action, GVariant* parameter, gpointer user_data) {
    (void)action; (void)parameter; (void)user_data;
    OZ_INFO("Edit -> Paste (TODO: implement)");
}

static void action_edit_delete(GSimpleAction* action, GVariant* parameter, gpointer user_data) {
    (void)action; (void)parameter; (void)user_data;
    OZ_INFO("Edit -> Delete (TODO: implement)");
}

static void action_edit_select_all(GSimpleAction* action, GVariant* parameter, gpointer user_data) {
    (void)action; (void)parameter; (void)user_data;
    OZ_INFO("Edit -> Select All (TODO: implement)");
}

// View actions
static void action_view_grid(GSimpleAction* action, GVariant* parameter, gpointer user_data) {
    (void)parameter;
    EditorUI* ui = (EditorUI*)user_data;
    
    GVariant* state = g_action_get_state(G_ACTION(action));
    gboolean enabled = g_variant_get_boolean(state);
    g_variant_unref(state);
    
    enabled = !enabled;
    ui->dbg_show_grid = enabled;
    
    g_action_change_state(G_ACTION(action), g_variant_new_boolean(enabled));
    editor_ui_update_status(ui, enabled ? "Grid enabled" : "Grid disabled");
}

static void action_view_axes(GSimpleAction* action, GVariant* parameter, gpointer user_data) {
    (void)parameter;
    EditorUI* ui = (EditorUI*)user_data;
    
    GVariant* state = g_action_get_state(G_ACTION(action));
    gboolean enabled = g_variant_get_boolean(state);
    g_variant_unref(state);
    
    enabled = !enabled;
    ui->dbg_show_axes = enabled;
    
    g_action_change_state(G_ACTION(action), g_variant_new_boolean(enabled));
    editor_ui_update_status(ui, enabled ? "Axes enabled" : "Axes disabled");
}

static void action_view_wireframe(GSimpleAction* action, GVariant* parameter, gpointer user_data) {
    (void)parameter;
    EditorUI* ui = (EditorUI*)user_data;
    
    GVariant* state = g_action_get_state(G_ACTION(action));
    gboolean enabled = g_variant_get_boolean(state);
    g_variant_unref(state);
    
    enabled = !enabled;
    
    g_action_change_state(G_ACTION(action), g_variant_new_boolean(enabled));
    editor_ui_update_status(ui, enabled ? "Wireframe enabled" : "Wireframe disabled");
}

static void action_view_lighting(GSimpleAction* action, GVariant* parameter, gpointer user_data) {
    (void)parameter;
    EditorUI* ui = (EditorUI*)user_data;
    
    GVariant* state = g_action_get_state(G_ACTION(action));
    gboolean enabled = g_variant_get_boolean(state);
    g_variant_unref(state);
    
    enabled = !enabled;
    ui->dbg_lighting = enabled;
    
    g_action_change_state(G_ACTION(action), g_variant_new_boolean(enabled));
    editor_ui_update_status(ui, enabled ? "Lighting enabled" : "Lighting disabled");
}

static void action_view_reset_camera(GSimpleAction* action, GVariant* parameter, gpointer user_data) {
    (void)action; (void)parameter;
    EditorUI* ui = (EditorUI*)user_data;
    
    // TODO: Reset camera to default position
    OZ_INFO("View -> Reset Camera (TODO: implement)");
    editor_ui_update_status(ui, "Camera reset");
}

// BSP actions (stubs)
static void action_bsp_add_box(GSimpleAction* action, GVariant* parameter, gpointer user_data) {
    (void)action; (void)parameter;
    EditorUI* ui = (EditorUI*)user_data;
    OZ_INFO("BSP -> Add Box (TODO: implement)");
    editor_ui_update_status(ui, "Box added");
}

static void action_bsp_add_cylinder(GSimpleAction* action, GVariant* parameter, gpointer user_data) {
    (void)action; (void)parameter;
    EditorUI* ui = (EditorUI*)user_data;
    OZ_INFO("BSP -> Add Cylinder (TODO: implement)");
    editor_ui_update_status(ui, "Cylinder added");
}

static void action_bsp_add_sphere(GSimpleAction* action, GVariant* parameter, gpointer user_data) {
    (void)action; (void)parameter;
    EditorUI* ui = (EditorUI*)user_data;
    OZ_INFO("BSP -> Add Sphere (TODO: implement)");
    editor_ui_update_status(ui, "Sphere added");
}

static void action_bsp_csg_union(GSimpleAction* action, GVariant* parameter, gpointer user_data) {
    (void)action; (void)parameter;
    EditorUI* ui = (EditorUI*)user_data;
    OZ_INFO("BSP -> CSG Union (TODO: implement)");
    editor_ui_update_status(ui, "CSG Union applied");
}

static void action_bsp_csg_subtract(GSimpleAction* action, GVariant* parameter, gpointer user_data) {
    (void)action; (void)parameter;
    EditorUI* ui = (EditorUI*)user_data;
    OZ_INFO("BSP -> CSG Subtract (TODO: implement)");
    editor_ui_update_status(ui, "CSG Subtract applied");
}

static void action_bsp_csg_intersect(GSimpleAction* action, GVariant* parameter, gpointer user_data) {
    (void)action; (void)parameter;
    EditorUI* ui = (EditorUI*)user_data;
    OZ_INFO("BSP -> CSG Intersect (TODO: implement)");
    editor_ui_update_status(ui, "CSG Intersect applied");
}

// Tools actions
static void action_tools_preferences(GSimpleAction* action, GVariant* parameter, gpointer user_data) {
    (void)action; (void)parameter;
    EditorUI* ui = (EditorUI*)user_data;
    editor_ui_show_preferences_dialog(ui);
}

// Help actions
static void action_help_about(GSimpleAction* action, GVariant* parameter, gpointer user_data) {
    (void)action; (void)parameter;
    EditorUI* ui = (EditorUI*)user_data;
    editor_ui_show_about_dialog(ui);
}
