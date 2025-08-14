#include "oz/editor/ui.h"
#include "oz/editor/rendering.h"
#include "oz/editor/input.h"
#include "oz/editor/scene.h"
#include "oz/oz_log.h"
#include <gtk/gtk.h>
#include <math.h>
#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// GL Area callbacks
static gboolean on_gl_realize(GtkGLArea* area, EditorUI* ui);
static gboolean on_gl_render(GtkGLArea* area, GdkGLContext* context, EditorUI* ui);
static void on_gl_unrealize(GtkGLArea* area, EditorUI* ui);

// Software rendering callbacks
static gboolean on_draw_software(GtkWidget* widget, cairo_t* cr, EditorUI* ui);
static gboolean on_software_redraw_timer(gpointer user_data);

void editor_ui_setup_gl_viewport(EditorUI* ui) {
    if (!ui) return;
    
    OZ_INFO("Setting up OpenGL viewport");
    
    // Create GL area
    ui->gl_area = gtk_gl_area_new();
    if (!ui->gl_area) {
        OZ_ERROR("Failed to create GtkGLArea");
        editor_ui_setup_software_viewport(ui);
        return;
    }
    
    // Configure GL area
    gtk_gl_area_set_required_version(GTK_GL_AREA(ui->gl_area), 2, 1);
    gtk_gl_area_set_use_es(GTK_GL_AREA(ui->gl_area), FALSE);
    gtk_gl_area_set_has_depth_buffer(GTK_GL_AREA(ui->gl_area), TRUE);
    gtk_gl_area_set_has_stencil_buffer(GTK_GL_AREA(ui->gl_area), FALSE);
    
    // Connect signals
    g_signal_connect(ui->gl_area, "realize", G_CALLBACK(on_gl_realize), ui);
    g_signal_connect(ui->gl_area, "render", G_CALLBACK(on_gl_render), ui);
    g_signal_connect(ui->gl_area, "unrealize", G_CALLBACK(on_gl_unrealize), ui);
    
    // Set as viewport
    ui->viewport = ui->gl_area;
    
    // Add to layout
    gtk_box_pack_start(GTK_BOX(ui->vbox), ui->viewport, TRUE, TRUE, 0);
    
    // Setup overlay for status info
    ui->overlay = gtk_overlay_new();
    gtk_container_remove(GTK_CONTAINER(ui->vbox), ui->viewport);
    gtk_container_add(GTK_CONTAINER(ui->overlay), ui->viewport);
    gtk_overlay_add_overlay(GTK_OVERLAY(ui->overlay), ui->info_label);
    gtk_box_pack_start(GTK_BOX(ui->vbox), ui->overlay, TRUE, TRUE, 0);
    
    // Enable input events
    gtk_widget_set_can_focus(ui->viewport, TRUE);
    gtk_widget_add_events(ui->viewport, 
                          GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK |
                          GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
                          GDK_POINTER_MOTION_MASK | GDK_SCROLL_MASK |
                          GDK_FOCUS_CHANGE_MASK);
    
    // Input events will be connected later when input system is set up
    
    OZ_INFO("OpenGL viewport configured");
}

void editor_ui_setup_software_viewport(EditorUI* ui) {
    if (!ui) return;
    
    OZ_INFO("Setting up software viewport");
    
    // Create drawing area for software rendering
    ui->viewport = gtk_drawing_area_new();
    if (!ui->viewport) {
        OZ_ERROR("Failed to create drawing area");
        return;
    }
    
    // Set minimum size
    gtk_widget_set_size_request(ui->viewport, 640, 480);
    
    // Connect draw signal
    g_signal_connect(ui->viewport, "draw", G_CALLBACK(on_draw_software), ui);
    
    // Add to layout
    gtk_box_pack_start(GTK_BOX(ui->vbox), ui->viewport, TRUE, TRUE, 0);
    
    // Setup overlay for status info
    ui->overlay = gtk_overlay_new();
    gtk_container_remove(GTK_CONTAINER(ui->vbox), ui->viewport);
    gtk_container_add(GTK_CONTAINER(ui->overlay), ui->viewport);
    gtk_overlay_add_overlay(GTK_OVERLAY(ui->overlay), ui->info_label);
    gtk_box_pack_start(GTK_BOX(ui->vbox), ui->overlay, TRUE, TRUE, 0);
    
    // Enable input events
    gtk_widget_set_can_focus(ui->viewport, TRUE);
    gtk_widget_add_events(ui->viewport, 
                          GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK |
                          GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
                          GDK_POINTER_MOTION_MASK | GDK_SCROLL_MASK |
                          GDK_FOCUS_CHANGE_MASK);
    
    // Input events will be connected later when input system is set up
    
    // Start redraw timer for software rendering
    ui->redraw_id = g_timeout_add(16, on_software_redraw_timer, ui); // ~60 FPS
    
    OZ_INFO("Software viewport configured");
}

void editor_ui_switch_to_software(EditorUI* ui) {
    if (!ui) return;
    
    OZ_INFO("Switching to software rendering");
    
    // Remove existing viewport
    if (ui->viewport) {
        if (ui->overlay) {
            gtk_container_remove(GTK_CONTAINER(ui->overlay), ui->viewport);
            gtk_container_remove(GTK_CONTAINER(ui->vbox), ui->overlay);
        } else {
            gtk_container_remove(GTK_CONTAINER(ui->vbox), ui->viewport);
        }
    }
    
    // Stop GL redraw timer if running
    if (ui->redraw_id) {
        g_source_remove(ui->redraw_id);
        ui->redraw_id = 0;
    }
    
    // Clear references
    ui->viewport = NULL;
    ui->gl_area = NULL;
    ui->overlay = NULL;
    
    // Setup software viewport
    editor_ui_setup_software_viewport(ui);
    
    // Show new viewport
    if (ui->viewport) {
        gtk_widget_show_all(ui->viewport);
    }
    
    editor_ui_update_status(ui, "Switched to software rendering");
}

void editor_ui_build_toolbar(EditorUI* ui) {
    if (!ui) return;
    
    // Create toolbar
    ui->toolbar = gtk_toolbar_new();
    gtk_toolbar_set_style(GTK_TOOLBAR(ui->toolbar), GTK_TOOLBAR_BOTH);
    gtk_box_pack_start(GTK_BOX(ui->vbox), ui->toolbar, FALSE, FALSE, 0);
    
    // File operations
    GtkToolItem* new_item = gtk_tool_button_new(NULL, "New");
    gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(new_item), "document-new");
    gtk_actionable_set_action_name(GTK_ACTIONABLE(new_item), "app.file_new");
    gtk_toolbar_insert(GTK_TOOLBAR(ui->toolbar), new_item, -1);
    
    GtkToolItem* open_item = gtk_tool_button_new(NULL, "Open");
    gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(open_item), "document-open");
    gtk_actionable_set_action_name(GTK_ACTIONABLE(open_item), "app.file_open");
    gtk_toolbar_insert(GTK_TOOLBAR(ui->toolbar), open_item, -1);
    
    GtkToolItem* save_item = gtk_tool_button_new(NULL, "Save");
    gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(save_item), "document-save");
    gtk_actionable_set_action_name(GTK_ACTIONABLE(save_item), "app.file_save");
    gtk_toolbar_insert(GTK_TOOLBAR(ui->toolbar), save_item, -1);
    
    // Separator
    gtk_toolbar_insert(GTK_TOOLBAR(ui->toolbar), gtk_separator_tool_item_new(), -1);
    
    // BSP operations
    GtkToolItem* box_item = gtk_tool_button_new(NULL, "Box");
    gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(box_item), "insert-object");
    gtk_actionable_set_action_name(GTK_ACTIONABLE(box_item), "app.bsp_add_box");
    gtk_toolbar_insert(GTK_TOOLBAR(ui->toolbar), box_item, -1);
    
    GtkToolItem* cyl_item = gtk_tool_button_new(NULL, "Cylinder");
    gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(cyl_item), "insert-object");
    gtk_actionable_set_action_name(GTK_ACTIONABLE(cyl_item), "app.bsp_add_cylinder");
    gtk_toolbar_insert(GTK_TOOLBAR(ui->toolbar), cyl_item, -1);
    
    GtkToolItem* sphere_item = gtk_tool_button_new(NULL, "Sphere");
    gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(sphere_item), "insert-object");
    gtk_actionable_set_action_name(GTK_ACTIONABLE(sphere_item), "app.bsp_add_sphere");
    gtk_toolbar_insert(GTK_TOOLBAR(ui->toolbar), sphere_item, -1);
    
    // Separator
    gtk_toolbar_insert(GTK_TOOLBAR(ui->toolbar), gtk_separator_tool_item_new(), -1);
    
    // View toggles
    GtkToolItem* grid_item = gtk_toggle_tool_button_new();
    gtk_tool_button_set_label(GTK_TOOL_BUTTON(grid_item), "Grid");
    gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(grid_item), "view-grid-symbolic");
    gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(grid_item), ui->dbg_show_grid);
    gtk_actionable_set_action_name(GTK_ACTIONABLE(grid_item), "app.view_grid");
    gtk_toolbar_insert(GTK_TOOLBAR(ui->toolbar), grid_item, -1);
    
    GtkToolItem* axes_item = gtk_toggle_tool_button_new();
    gtk_tool_button_set_label(GTK_TOOL_BUTTON(axes_item), "Axes");
    gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(axes_item), "insert-link");
    gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(axes_item), ui->dbg_show_axes);
    gtk_actionable_set_action_name(GTK_ACTIONABLE(axes_item), "app.view_axes");
    gtk_toolbar_insert(GTK_TOOLBAR(ui->toolbar), axes_item, -1);
    
    GtkToolItem* wireframe_item = gtk_toggle_tool_button_new();
    gtk_tool_button_set_label(GTK_TOOL_BUTTON(wireframe_item), "Wireframe");
    gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(wireframe_item), "view-restore");
    gtk_actionable_set_action_name(GTK_ACTIONABLE(wireframe_item), "app.view_wireframe");
    gtk_toolbar_insert(GTK_TOOLBAR(ui->toolbar), wireframe_item, -1);
    
    OZ_INFO("Toolbar created");
}

// GL Area callbacks
static gboolean on_gl_realize(GtkGLArea* area, EditorUI* ui) {
    (void)area;
    
    if (!ui) return FALSE;
    
    OZ_INFO("GL Area realized");
    
    gtk_gl_area_make_current(GTK_GL_AREA(ui->gl_area));
    
    // Check for GL errors during context creation
    GError* error = gtk_gl_area_get_error(GTK_GL_AREA(ui->gl_area));
    if (error) {
        OZ_ERROR("GL Area error: %s", error->message);
        editor_ui_switch_to_software(ui);
        return FALSE;
    }
    
    // Initialize renderer if available
    if (ui->modern_renderer) {
        // Modern renderer was already initialized, just set viewport
        int width = gtk_widget_get_allocated_width(GTK_WIDGET(area));
        int height = gtk_widget_get_allocated_height(GTK_WIDGET(area));
        oz_gl_modern_set_viewport(ui->modern_renderer, width, height);
    } else {
        // Try to initialize modern renderer
        ui->modern_renderer = oz_gl_modern_init();
        if (!ui->modern_renderer) {
            OZ_WARN("Failed to initialize modern GL renderer, using legacy mode");
            // TODO: Initialize legacy GL renderer if needed
        }
    }
    
    return TRUE;
}

static gboolean on_gl_render(GtkGLArea* area, GdkGLContext* context, EditorUI* ui) {
    (void)context;
    
    if (!ui) return FALSE;
    
    int width = gtk_widget_get_allocated_width(GTK_WIDGET(area));
    int height = gtk_widget_get_allocated_height(GTK_WIDGET(area));
    
    // Use modern renderer if available
    if (ui->modern_renderer) {
        oz_gl_modern_set_viewport(ui->modern_renderer, width, height);
        
        if (ui->scene) {
            // Apply camera
            oz_gl_modern_apply_camera(ui->modern_renderer, &ui->scene->camera);
            
            // Set lighting
            if (ui->dbg_lighting) {
                oz_gl_modern_set_ambient_light(ui->modern_renderer, 0.2f, 0.2f, 0.3f);
            } else {
                oz_gl_modern_set_ambient_light(ui->modern_renderer, 0.8f, 0.8f, 0.8f);
            }
            
            // Clear
            oz_gl_modern_clear(ui->modern_renderer, 0.2f, 0.2f, 0.25f, 1.0f);
            
            // Draw grid
            if (ui->dbg_show_grid) {
                oz_gl_modern_draw_grid(ui->modern_renderer, 10.0f, 1.0f, 
                                      &(OzVec3){0.3f, 0.3f, 0.3f});
            }
            
            // Draw axes
            if (ui->dbg_show_axes) {
                oz_gl_modern_draw_axes(ui->modern_renderer, 2.0f);
            }
            
            // Draw map
            oz_gl_modern_draw_map_filled(ui->modern_renderer, &ui->scene->map);
            
            // Draw objects
            for (size_t i = 0; i < ui->scene->obj_count; i++) {
                const EditorObject* obj = &ui->scene->objects[i];
                OzVec3 color = {1.0f, 1.0f, 0.0f}; // Yellow by default
                
                if ((int)i == ui->scene->selected_object_index) {
                    color = (OzVec3){1.0f, 0.0f, 0.0f}; // Red for selected
                }
                
                switch (obj->type) {
                    case OBJ_ZONE: {
                        OzVec3 center = {obj->as.zone.center[0], obj->as.zone.center[1], obj->as.zone.center[2]};
                        oz_gl_modern_draw_zone_circle(ui->modern_renderer, &center, 
                                                     obj->as.zone.radius, &color);
                        break;
                    }
                    case OBJ_PICKUP: {
                        OzVec3 center = {obj->as.pickup.position[0], obj->as.pickup.position[1], obj->as.pickup.position[2]};
                        oz_gl_modern_draw_pickup_cube(ui->modern_renderer, &center, 0.5f, &color);
                        break;
                    }
                    case OBJ_PLAYER_START: {
                        OzVec3 pos = {obj->as.pstart.position[0], obj->as.pstart.position[1], obj->as.pstart.position[2]};
                        oz_gl_modern_draw_player_start(ui->modern_renderer, &pos, 
                                                      obj->as.pstart.yaw, &color);
                        break;
                    }
                }
            }
            
            // Draw gizmo if something is selected
            if (ui->scene->selected_brush_index >= 0 || ui->scene->selected_object_index >= 0) {
                OzVec3 gizmo_center = {0, 0, 0};
                
                if (ui->scene->selected_brush_index >= 0 && (size_t)ui->scene->selected_brush_index < ui->scene->map.count) {
                    const OzBrush* brush = &ui->scene->map.brushes[ui->scene->selected_brush_index];
                    if (brush->type == OZ_BRUSH_BOX) {
                        gizmo_center = brush->as.box.center;
                    } else if (brush->type == OZ_BRUSH_CYLINDER) {
                        gizmo_center = brush->as.cyl.center;
                    }
                } else if (ui->scene->selected_object_index >= 0 && (size_t)ui->scene->selected_object_index < ui->scene->obj_count) {
                    const EditorObject* obj = &ui->scene->objects[ui->scene->selected_object_index];
                    if (obj->type == OBJ_ZONE) {
                        gizmo_center.x = obj->as.zone.center[0];
                        gizmo_center.y = obj->as.zone.center[1];
                        gizmo_center.z = obj->as.zone.center[2];
                    } else if (obj->type == OBJ_PICKUP) {
                        gizmo_center.x = obj->as.pickup.position[0];
                        gizmo_center.y = obj->as.pickup.position[1];
                        gizmo_center.z = obj->as.pickup.position[2];
                    } else if (obj->type == OBJ_PLAYER_START) {
                        gizmo_center.x = obj->as.pstart.position[0];
                        gizmo_center.y = obj->as.pstart.position[1];
                        gizmo_center.z = obj->as.pstart.position[2];
                    }
                }
                
                oz_gl_modern_draw_gizmo(ui->modern_renderer, &gizmo_center, 1.0f, ui->scene->gizmo_axis);
            }
        }
    } else {
        // Fallback to basic GL clear
        glViewport(0, 0, width, height);
        glClearColor(0.2f, 0.2f, 0.25f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }
    
    return TRUE;
}

static void on_gl_unrealize(GtkGLArea* area, EditorUI* ui) {
    (void)area;
    
    if (!ui) return;
    
    OZ_INFO("GL Area unrealized");
    
    // Cleanup modern renderer
    if (ui->modern_renderer) {
        oz_gl_modern_destroy(ui->modern_renderer);
        ui->modern_renderer = NULL;
    }
}

// Software rendering callbacks
static gboolean on_draw_software(GtkWidget* widget, cairo_t* cr, EditorUI* ui) {
    (void)widget;
    
    if (!ui || !cr) return FALSE;
    
    int width = gtk_widget_get_allocated_width(widget);
    int height = gtk_widget_get_allocated_height(widget);
    
    // Clear background
    cairo_set_source_rgb(cr, 0.2, 0.2, 0.25);
    cairo_paint(cr);
    
    // Simple 2D rendering for software mode
    if (ui->scene) {
        cairo_save(cr);
        
        // Set up coordinate system (center viewport, scale appropriately)
        cairo_translate(cr, width * 0.5, height * 0.5);
        cairo_scale(cr, 20.0, -20.0); // 20 pixels per unit, flip Y
        
        // Draw grid
        if (ui->dbg_show_grid) {
            cairo_set_source_rgb(cr, 0.3, 0.3, 0.3);
            cairo_set_line_width(cr, 0.02);
            
            for (float x = -10; x <= 10; x += 1.0f) {
                cairo_move_to(cr, x, -10);
                cairo_line_to(cr, x, 10);
            }
            for (float y = -10; y <= 10; y += 1.0f) {
                cairo_move_to(cr, -10, y);
                cairo_line_to(cr, 10, y);
            }
            cairo_stroke(cr);
        }
        
        // Draw axes
        if (ui->dbg_show_axes) {
            cairo_set_line_width(cr, 0.05);
            
            // X axis (red)
            cairo_set_source_rgb(cr, 1.0, 0.0, 0.0);
            cairo_move_to(cr, 0, 0);
            cairo_line_to(cr, 5, 0);
            cairo_stroke(cr);
            
            // Y axis (green)
            cairo_set_source_rgb(cr, 0.0, 1.0, 0.0);
            cairo_move_to(cr, 0, 0);
            cairo_line_to(cr, 0, 5);
            cairo_stroke(cr);
        }
        
        // Draw map brushes as 2D shapes
        for (size_t i = 0; i < ui->scene->map.count; i++) {
            const OzBrush* brush = &ui->scene->map.brushes[i];
            
            if ((int)i == ui->scene->selected_brush_index) {
                cairo_set_source_rgb(cr, 1.0, 0.0, 0.0); // Red for selected
            } else {
                cairo_set_source_rgb(cr, 0.7, 0.7, 0.7); // Gray for unselected
            }
            
            if (brush->type == OZ_BRUSH_BOX) {
                const OzBrushBox* box = &brush->as.box;
                cairo_rectangle(cr, 
                               box->center.x - box->half.x,
                               box->center.y - box->half.y,
                               box->half.x * 2,
                               box->half.y * 2);
                cairo_stroke(cr);
            } else if (brush->type == OZ_BRUSH_CYLINDER) {
                const OzBrushCylinder* cyl = &brush->as.cyl;
                cairo_arc(cr, cyl->center.x, cyl->center.y, cyl->radius_x, 0, 2 * M_PI);
                cairo_stroke(cr);
            }
        }
        
        cairo_restore(cr);
    }
    
    return TRUE;
}

static gboolean on_software_redraw_timer(gpointer user_data) {
    EditorUI* ui = (EditorUI*)user_data;
    
    if (!ui || ui->shutting_down || !ui->viewport) {
        return G_SOURCE_REMOVE;
    }
    
    gtk_widget_queue_draw(ui->viewport);
    return G_SOURCE_CONTINUE;
}

void editor_ui_connect_input_events(EditorUI* ui) {
    if (!ui || !ui->viewport || !ui->input) return;
    
    OZ_INFO("Connecting input events to viewport");
    
    g_signal_connect(ui->viewport, "key-press-event", 
                     G_CALLBACK(editor_input_on_key_press), ui->input);
    g_signal_connect(ui->viewport, "key-release-event", 
                     G_CALLBACK(editor_input_on_key_release), ui->input);
    g_signal_connect(ui->viewport, "button-press-event", 
                     G_CALLBACK(editor_input_on_button_press), ui->input);
    g_signal_connect(ui->viewport, "button-release-event", 
                     G_CALLBACK(editor_input_on_button_release), ui->input);
    g_signal_connect(ui->viewport, "motion-notify-event", 
                     G_CALLBACK(editor_input_on_motion_notify), ui->input);
    g_signal_connect(ui->viewport, "scroll-event", 
                     G_CALLBACK(editor_input_on_scroll), ui->input);
    g_signal_connect(ui->viewport, "focus-out-event", 
                     G_CALLBACK(editor_input_on_focus_out), ui->input);
}
