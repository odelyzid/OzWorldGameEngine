#include "oz/editor/ui.h"
#include "oz/editor/editor.h"
#include <gtk/gtk.h>

void editor_ui_show_about_dialog(EditorUI* ui) {
    if (!ui || !ui->window) return;
    
    GtkWidget* dialog = gtk_about_dialog_new();
    
    gtk_about_dialog_set_program_name(GTK_ABOUT_DIALOG(dialog), "OzWorld Editor");
    gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(dialog), oz_editor_get_version());
    gtk_about_dialog_set_copyright(GTK_ABOUT_DIALOG(dialog), "Copyright © 2024 OzWorld Project");
    gtk_about_dialog_set_comments(GTK_ABOUT_DIALOG(dialog), 
                                  "A modern BSP editor with OpenGL 3.2+ support\n"
                                  "Built with modular architecture for scalability");
    gtk_about_dialog_set_license_type(GTK_ABOUT_DIALOG(dialog), GTK_LICENSE_MIT_X11);
    gtk_about_dialog_set_website(GTK_ABOUT_DIALOG(dialog), "https://github.com/ozworld/ozworld");
    gtk_about_dialog_set_website_label(GTK_ABOUT_DIALOG(dialog), "OzWorld on GitHub");
    
    // Set parent window
    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(ui->window));
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    
    // Add build info as additional information
    char* build_info = g_strdup_printf("Build: %s\n"
                                      "OpenGL Backend: Modern VBO/VAO + Shaders\n"
                                      "GTK Version: %d.%d.%d",
                                      oz_editor_get_build_info(),
                                      gtk_get_major_version(),
                                      gtk_get_minor_version(),
                                      gtk_get_micro_version());
    
    // gtk_about_dialog_set_system_information(GTK_ABOUT_DIALOG(dialog), build_info); // GTK 3.12+ only
    g_free(build_info);
    
    // Run dialog
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

void editor_ui_show_preferences_dialog(EditorUI* ui) {
    if (!ui || !ui->window) return;
    
    GtkWidget* dialog = gtk_dialog_new_with_buttons("Preferences",
        GTK_WINDOW(ui->window),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Apply", GTK_RESPONSE_APPLY,
        "_OK", GTK_RESPONSE_OK,
        NULL);
    
    gtk_window_set_default_size(GTK_WINDOW(dialog), 400, 300);
    
    // Create notebook for tabbed preferences
    GtkWidget* notebook = gtk_notebook_new();
    gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), notebook);
    
    // General tab
    GtkWidget* general_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_container_set_border_width(GTK_CONTAINER(general_box), 12);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), general_box, gtk_label_new("General"));
    
    // Rendering preferences
    GtkWidget* render_frame = gtk_frame_new("Rendering");
    gtk_box_pack_start(GTK_BOX(general_box), render_frame, FALSE, FALSE, 0);
    
    GtkWidget* render_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(render_box), 6);
    gtk_container_add(GTK_CONTAINER(render_frame), render_box);
    
    GtkWidget* grid_check = gtk_check_button_new_with_label("Show grid by default");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(grid_check), ui->dbg_show_grid);
    gtk_box_pack_start(GTK_BOX(render_box), grid_check, FALSE, FALSE, 0);
    
    GtkWidget* axes_check = gtk_check_button_new_with_label("Show axes by default");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(axes_check), ui->dbg_show_axes);
    gtk_box_pack_start(GTK_BOX(render_box), axes_check, FALSE, FALSE, 0);
    
    GtkWidget* fps_check = gtk_check_button_new_with_label("Show FPS counter");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(fps_check), ui->dbg_show_fps);
    gtk_box_pack_start(GTK_BOX(render_box), fps_check, FALSE, FALSE, 0);
    
    // Camera preferences
    GtkWidget* camera_frame = gtk_frame_new("Camera");
    gtk_box_pack_start(GTK_BOX(general_box), camera_frame, FALSE, FALSE, 0);
    
    GtkWidget* camera_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(camera_box), 6);
    gtk_container_add(GTK_CONTAINER(camera_frame), camera_box);
    
    // Mouse sensitivity
    GtkWidget* sens_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_pack_start(GTK_BOX(camera_box), sens_box, FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(sens_box), gtk_label_new("Mouse sensitivity:"), FALSE, FALSE, 0);
    GtkWidget* sens_scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.001, 0.01, 0.001);
    gtk_range_set_value(GTK_RANGE(sens_scale), ui->input ? ui->input->mouse_sensitivity : 0.003);
    gtk_box_pack_start(GTK_BOX(sens_box), sens_scale, TRUE, TRUE, 0);
    
    // Paths tab
    GtkWidget* paths_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_container_set_border_width(GTK_CONTAINER(paths_box), 12);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), paths_box, gtk_label_new("Paths"));
    
    // Assets path
    GtkWidget* assets_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_pack_start(GTK_BOX(paths_box), assets_box, FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(assets_box), gtk_label_new("Assets folder:"), FALSE, FALSE, 0);
    GtkWidget* assets_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(assets_entry), ui->last_texture_path ? ui->last_texture_path : "");
    gtk_box_pack_start(GTK_BOX(assets_box), assets_entry, TRUE, TRUE, 0);
    
    GtkWidget* assets_button = gtk_button_new_with_label("Browse...");
    gtk_box_pack_start(GTK_BOX(assets_box), assets_button, FALSE, FALSE, 0);
    
    // Projects path
    GtkWidget* projects_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_pack_start(GTK_BOX(paths_box), projects_box, FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(projects_box), gtk_label_new("Projects folder:"), FALSE, FALSE, 0);
    GtkWidget* projects_entry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(projects_box), projects_entry, TRUE, TRUE, 0);
    
    GtkWidget* projects_button = gtk_button_new_with_label("Browse...");
    gtk_box_pack_start(GTK_BOX(projects_box), projects_button, FALSE, FALSE, 0);
    
    // Show dialog
    gtk_widget_show_all(dialog);
    
    int response = gtk_dialog_run(GTK_DIALOG(dialog));
    
    if (response == GTK_RESPONSE_OK || response == GTK_RESPONSE_APPLY) {
        // Apply preferences
        ui->dbg_show_grid = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(grid_check));
        ui->dbg_show_axes = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(axes_check));
        ui->dbg_show_fps = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(fps_check));
        
        if (ui->input) {
            ui->input->mouse_sensitivity = (float)gtk_range_get_value(GTK_RANGE(sens_scale));
        }
        
        // Update paths
        const char* assets_path = gtk_entry_get_text(GTK_ENTRY(assets_entry));
        if (assets_path && strlen(assets_path) > 0) {
            free(ui->last_texture_path);
            ui->last_texture_path = g_strdup(assets_path);
        }
        
        editor_ui_update_status(ui, "Preferences updated");
        
        if (response == GTK_RESPONSE_APPLY) {
            // Keep dialog open for Apply
            gtk_widget_destroy(dialog);
            editor_ui_show_preferences_dialog(ui); // Reopen
            return;
        }
    }
    
    gtk_widget_destroy(dialog);
}

void editor_ui_show_brush_dialog(EditorUI* ui, const char* type) {
    if (!ui || !ui->window || !type) return;
    
    char title[64];
    snprintf(title, sizeof(title), "Add %s", type);
    
    GtkWidget* dialog = gtk_dialog_new_with_buttons(title,
        GTK_WINDOW(ui->window),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Add", GTK_RESPONSE_OK,
        NULL);
    
    GtkWidget* content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget* grid = gtk_grid_new();
    gtk_container_set_border_width(GTK_CONTAINER(grid), 12);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
    gtk_container_add(GTK_CONTAINER(content), grid);
    
    int row = 0;
    
    // Position
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Position:"), 0, row, 1, 1);
    
    GtkWidget* pos_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_grid_attach(GTK_GRID(grid), pos_box, 1, row, 1, 1);
    
    GtkWidget* pos_x = gtk_spin_button_new_with_range(-100.0, 100.0, 0.1);
    GtkWidget* pos_y = gtk_spin_button_new_with_range(-100.0, 100.0, 0.1);
    GtkWidget* pos_z = gtk_spin_button_new_with_range(-100.0, 100.0, 0.1);
    
    gtk_box_pack_start(GTK_BOX(pos_box), gtk_label_new("X:"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(pos_box), pos_x, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(pos_box), gtk_label_new("Y:"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(pos_box), pos_y, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(pos_box), gtk_label_new("Z:"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(pos_box), pos_z, TRUE, TRUE, 0);
    
    row++;
    
    GtkWidget* size_widgets[6] = {NULL}; // For various size parameters
    
    if (strcmp(type, "Box") == 0) {
        // Size
        gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Size:"), 0, row, 1, 1);
        
        GtkWidget* size_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
        gtk_grid_attach(GTK_GRID(grid), size_box, 1, row, 1, 1);
        
        size_widgets[0] = gtk_spin_button_new_with_range(0.1, 100.0, 0.1);
        size_widgets[1] = gtk_spin_button_new_with_range(0.1, 100.0, 0.1);
        size_widgets[2] = gtk_spin_button_new_with_range(0.1, 100.0, 0.1);
        
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(size_widgets[0]), ui->scene ? ui->scene->last_box_w : 1.0);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(size_widgets[1]), ui->scene ? ui->scene->last_box_h : 1.0);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(size_widgets[2]), ui->scene ? ui->scene->last_box_d : 1.0);
        
        gtk_box_pack_start(GTK_BOX(size_box), gtk_label_new("W:"), FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(size_box), size_widgets[0], TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(size_box), gtk_label_new("H:"), FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(size_box), size_widgets[1], TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(size_box), gtk_label_new("D:"), FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(size_box), size_widgets[2], TRUE, TRUE, 0);
        
    } else if (strcmp(type, "Cylinder") == 0) {
        // Radius
        gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Radius:"), 0, row, 1, 1);
        
        GtkWidget* radius_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
        gtk_grid_attach(GTK_GRID(grid), radius_box, 1, row, 1, 1);
        
        size_widgets[0] = gtk_spin_button_new_with_range(0.1, 50.0, 0.1);
        size_widgets[1] = gtk_spin_button_new_with_range(0.1, 50.0, 0.1);
        
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(size_widgets[0]), ui->scene ? ui->scene->last_cyl_rx : 0.5);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(size_widgets[1]), ui->scene ? ui->scene->last_cyl_ry : 0.5);
        
        gtk_box_pack_start(GTK_BOX(radius_box), gtk_label_new("X:"), FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(radius_box), size_widgets[0], TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(radius_box), gtk_label_new("Y:"), FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(radius_box), size_widgets[1], TRUE, TRUE, 0);
        
        row++;
        
        // Height
        gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Height:"), 0, row, 1, 1);
        size_widgets[2] = gtk_spin_button_new_with_range(0.1, 100.0, 0.1);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(size_widgets[2]), ui->scene ? ui->scene->last_cyl_h : 1.0);
        gtk_grid_attach(GTK_GRID(grid), size_widgets[2], 1, row, 1, 1);
        
        row++;
        
        // Segments
        gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Segments:"), 0, row, 1, 1);
        size_widgets[3] = gtk_spin_button_new_with_range(3, 64, 1);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(size_widgets[3]), ui->scene ? ui->scene->last_cyl_seg : 16);
        gtk_grid_attach(GTK_GRID(grid), size_widgets[3], 1, row, 1, 1);
        
    } else if (strcmp(type, "Sphere") == 0) {
        // Radius
        gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Radius:"), 0, row, 1, 1);
        size_widgets[0] = gtk_spin_button_new_with_range(0.1, 50.0, 0.1);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(size_widgets[0]), 1.0);
        gtk_grid_attach(GTK_GRID(grid), size_widgets[0], 1, row, 1, 1);
        
        row++;
        
        // Segments
        gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Segments:"), 0, row, 1, 1);
        size_widgets[1] = gtk_spin_button_new_with_range(3, 32, 1);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(size_widgets[1]), 16);
        gtk_grid_attach(GTK_GRID(grid), size_widgets[1], 1, row, 1, 1);
    }
    
    gtk_widget_show_all(dialog);
    
    int response = gtk_dialog_run(GTK_DIALOG(dialog));
    
    if (response == GTK_RESPONSE_OK && ui->scene) {
        // Get position
        OzVec3 pos = {
            (float)gtk_spin_button_get_value(GTK_SPIN_BUTTON(pos_x)),
            (float)gtk_spin_button_get_value(GTK_SPIN_BUTTON(pos_y)),
            (float)gtk_spin_button_get_value(GTK_SPIN_BUTTON(pos_z))
        };
        
        // Add the brush based on type
        if (strcmp(type, "Box") == 0 && size_widgets[0] && size_widgets[1] && size_widgets[2]) {
            OzVec3 half_extents = {
                (float)gtk_spin_button_get_value(GTK_SPIN_BUTTON(size_widgets[0])) * 0.5f,
                (float)gtk_spin_button_get_value(GTK_SPIN_BUTTON(size_widgets[1])) * 0.5f,
                (float)gtk_spin_button_get_value(GTK_SPIN_BUTTON(size_widgets[2])) * 0.5f
            };
            editor_scene_add_box(ui->scene, &pos, &half_extents);
            
            // Update last used values
            ui->scene->last_box_w = half_extents.x * 2;
            ui->scene->last_box_h = half_extents.y * 2;
            ui->scene->last_box_d = half_extents.z * 2;
            
        } else if (strcmp(type, "Cylinder") == 0 && size_widgets[0] && size_widgets[1] && size_widgets[2] && size_widgets[3]) {
            float radius_x = (float)gtk_spin_button_get_value(GTK_SPIN_BUTTON(size_widgets[0]));
            float radius_y = (float)gtk_spin_button_get_value(GTK_SPIN_BUTTON(size_widgets[1]));
            float height = (float)gtk_spin_button_get_value(GTK_SPIN_BUTTON(size_widgets[2]));
            int segments = (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(size_widgets[3]));
            
            editor_scene_add_cylinder(ui->scene, &pos, radius_x, radius_y, height, segments);
            
            // Update last used values
            ui->scene->last_cyl_rx = radius_x;
            ui->scene->last_cyl_ry = radius_y;
            ui->scene->last_cyl_h = height;
            ui->scene->last_cyl_seg = segments;
            
        } else if (strcmp(type, "Sphere") == 0 && size_widgets[0] && size_widgets[1]) {
            float radius = (float)gtk_spin_button_get_value(GTK_SPIN_BUTTON(size_widgets[0]));
            int segments = (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(size_widgets[1]));
            
            editor_scene_add_sphere(ui->scene, &pos, radius, segments);
        }
        
        char status[128];
        snprintf(status, sizeof(status), "%s added at (%.2f, %.2f, %.2f)", type, pos.x, pos.y, pos.z);
        editor_ui_update_status(ui, status);
    }
    
    gtk_widget_destroy(dialog);
}
