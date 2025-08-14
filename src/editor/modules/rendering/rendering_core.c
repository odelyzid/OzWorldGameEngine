#include "oz/editor/rendering.h"
#include "oz/editor/scene.h"
#include "oz/oz_log.h"
#include <stdlib.h>
#include <string.h>
#include <cairo.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static bool renderer_init_modern_gl(EditorRenderer* renderer, GtkGLArea* gl_area);
static bool renderer_init_legacy_gl(EditorRenderer* renderer);
static bool renderer_init_software(EditorRenderer* renderer, int width, int height);

static void renderer_software_render_scene(EditorRenderer* renderer, const EditorScene* scene);
static void renderer_software_render_grid(EditorRenderer* renderer, float extent, float step);
static void renderer_software_render_axes(EditorRenderer* renderer, float length);

EditorRenderer* editor_renderer_create(void) {
    EditorRenderer* renderer = calloc(1, sizeof(EditorRenderer));
    if (!renderer) {
        OZ_ERROR("Failed to allocate EditorRenderer");
        return NULL;
    }
    
    // Initialize state
    renderer->backend = RENDER_BACKEND_MODERN_GL; // Try modern first
    renderer->modern_gl = NULL;
    renderer->legacy_texture = 0;
    renderer->legacy_tex_w = renderer->legacy_tex_h = 0;
    renderer->software_surface = NULL;
    renderer->software_context = NULL;
    
    renderer->viewport_width = 800;
    renderer->viewport_height = 600;
    
    // Default rendering options
    renderer->show_grid = true;
    renderer->show_axes = true;
    renderer->show_wireframe = false;
    renderer->show_lighting = false;
    renderer->show_textures = true;
    
    renderer->show_debug_info = false;
    renderer->show_performance_overlay = false;
    renderer->show_memory_usage = false;
    
    OZ_INFO("Editor renderer created");
    return renderer;
}

void editor_renderer_destroy(EditorRenderer* renderer) {
    if (!renderer) return;
    
    // Cleanup modern GL
    if (renderer->modern_gl) {
        oz_gl_modern_destroy(renderer->modern_gl);
    }
    
    // Cleanup legacy GL resources
    if (renderer->legacy_texture) {
        glDeleteTextures(1, &renderer->legacy_texture);
    }
    
    // Cleanup software rendering
    if (renderer->software_context) {
        cairo_destroy(renderer->software_context);
    }
    if (renderer->software_surface) {
        cairo_surface_destroy(renderer->software_surface);
    }
    
    free(renderer);
    OZ_INFO("Editor renderer destroyed");
}

bool editor_renderer_initialize_gl(EditorRenderer* renderer, GtkGLArea* gl_area) {
    if (!renderer || !gl_area) return false;
    
    // Try modern GL first
    if (renderer_init_modern_gl(renderer, gl_area)) {
        renderer->backend = RENDER_BACKEND_MODERN_GL;
        OZ_INFO("Initialized modern OpenGL renderer");
        return true;
    }
    
    // Fall back to legacy GL
    if (renderer_init_legacy_gl(renderer)) {
        renderer->backend = RENDER_BACKEND_LEGACY_GL;
        OZ_INFO("Initialized legacy OpenGL renderer");
        return true;
    }
    
    OZ_ERROR("Failed to initialize OpenGL renderer");
    return false;
}

bool editor_renderer_initialize_software(EditorRenderer* renderer, int width, int height) {
    if (!renderer) return false;
    
    if (renderer_init_software(renderer, width, height)) {
        renderer->backend = RENDER_BACKEND_SOFTWARE;
        OZ_INFO("Initialized software renderer (%dx%d)", width, height);
        return true;
    }
    
    OZ_ERROR("Failed to initialize software renderer");
    return false;
}

static bool renderer_init_modern_gl(EditorRenderer* renderer, GtkGLArea* gl_area) {
    (void)gl_area; // Suppress unused warning
    
    renderer->modern_gl = oz_gl_modern_init();
    return renderer->modern_gl != NULL;
}

static bool renderer_init_legacy_gl(EditorRenderer* renderer) {
    // Check for required OpenGL version/extensions
    const GLubyte* version = glGetString(GL_VERSION);
    if (!version) {
        OZ_ERROR("Failed to get OpenGL version");
        return false;
    }
    
    OZ_INFO("Legacy OpenGL version: %s", version);
    
    // Create default texture for legacy rendering
    glGenTextures(1, &renderer->legacy_texture);
    if (!renderer->legacy_texture) {
        OZ_ERROR("Failed to generate legacy texture");
        return false;
    }
    
    // Create a simple checker pattern texture
    const int tex_size = 64;
    unsigned char* tex_data = malloc(tex_size * tex_size * 4);
    if (tex_data) {
        for (int y = 0; y < tex_size; y++) {
            for (int x = 0; x < tex_size; x++) {
                int checker = ((x / 8) + (y / 8)) % 2;
                unsigned char color = checker ? 200 : 100;
                int idx = (y * tex_size + x) * 4;
                tex_data[idx + 0] = color;     // R
                tex_data[idx + 1] = color;     // G
                tex_data[idx + 2] = color;     // B
                tex_data[idx + 3] = 255;       // A
            }
        }
        
        glBindTexture(GL_TEXTURE_2D, renderer->legacy_texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tex_size, tex_size, 0, 
                     GL_RGBA, GL_UNSIGNED_BYTE, tex_data);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D, 0);
        
        renderer->legacy_tex_w = renderer->legacy_tex_h = tex_size;
        free(tex_data);
    }
    
    return true;
}

static bool renderer_init_software(EditorRenderer* renderer, int width, int height) {
    // Create Cairo surface for software rendering
    renderer->software_surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24, width, height);
    if (cairo_surface_status(renderer->software_surface) != CAIRO_STATUS_SUCCESS) {
        OZ_ERROR("Failed to create Cairo surface");
        return false;
    }
    
    renderer->software_context = cairo_create(renderer->software_surface);
    if (cairo_status(renderer->software_context) != CAIRO_STATUS_SUCCESS) {
        OZ_ERROR("Failed to create Cairo context");
        cairo_surface_destroy(renderer->software_surface);
        renderer->software_surface = NULL;
        return false;
    }
    
    return true;
}

EditorRenderBackend editor_renderer_detect_best_backend(void) {
    // Try to get OpenGL context info to determine best backend
    const GLubyte* version = glGetString(GL_VERSION);
    const GLubyte* renderer = glGetString(GL_RENDERER);
    
    if (version && renderer) {
        OZ_INFO("OpenGL Version: %s", version);
        OZ_INFO("OpenGL Renderer: %s", renderer);
        
        // Parse version to determine capabilities
        int major = 0, minor = 0;
        if (sscanf((const char*)version, "%d.%d", &major, &minor) == 2) {
            if (major > 3 || (major == 3 && minor >= 2)) {
                // OpenGL 3.2+ - can use modern renderer
                return RENDER_BACKEND_MODERN_GL;
            } else if (major >= 2) {
                // OpenGL 2.0+ - use legacy immediate mode
                return RENDER_BACKEND_LEGACY_GL;
            }
        }
    }
    
    // Fall back to software
    OZ_WARN("Unable to determine OpenGL capabilities, using software renderer");
    return RENDER_BACKEND_SOFTWARE;
}

bool editor_renderer_switch_backend(EditorRenderer* renderer, EditorRenderBackend backend) {
    if (!renderer || renderer->backend == backend) return true;
    
    OZ_INFO("Switching renderer backend from %d to %d", renderer->backend, backend);
    
    // Cleanup current backend
    if (renderer->modern_gl) {
        oz_gl_modern_destroy(renderer->modern_gl);
        renderer->modern_gl = NULL;
    }
    
    if (renderer->legacy_texture) {
        glDeleteTextures(1, &renderer->legacy_texture);
        renderer->legacy_texture = 0;
    }
    
    if (renderer->software_context) {
        cairo_destroy(renderer->software_context);
        renderer->software_context = NULL;
    }
    if (renderer->software_surface) {
        cairo_surface_destroy(renderer->software_surface);
        renderer->software_surface = NULL;
    }
    
    // Initialize new backend
    renderer->backend = backend;
    
    switch (backend) {
        case RENDER_BACKEND_MODERN_GL:
            return renderer_init_modern_gl(renderer, NULL);
            
        case RENDER_BACKEND_LEGACY_GL:
            return renderer_init_legacy_gl(renderer);
            
        case RENDER_BACKEND_SOFTWARE:
            return renderer_init_software(renderer, renderer->viewport_width, renderer->viewport_height);
    }
    
    return false;
}

const char* editor_renderer_get_backend_name(EditorRenderBackend backend) {
    switch (backend) {
        case RENDER_BACKEND_MODERN_GL: return "Modern OpenGL (VBO/VAO + Shaders)";
        case RENDER_BACKEND_LEGACY_GL: return "Legacy OpenGL (Immediate Mode)";
        case RENDER_BACKEND_SOFTWARE: return "Software Rendering (Cairo)";
        default: return "Unknown";
    }
}

// Viewport and rendering control
void editor_renderer_set_viewport(EditorRenderer* renderer, int width, int height) {
    if (!renderer) return;
    
    renderer->viewport_width = width;
    renderer->viewport_height = height;
    
    switch (renderer->backend) {
        case RENDER_BACKEND_MODERN_GL:
            if (renderer->modern_gl) {
                oz_gl_modern_set_viewport(renderer->modern_gl, width, height);
            }
            break;
            
        case RENDER_BACKEND_LEGACY_GL:
            glViewport(0, 0, width, height);
            break;
            
        case RENDER_BACKEND_SOFTWARE:
            // Recreate surface with new size
            if (renderer->software_context) {
                cairo_destroy(renderer->software_context);
                renderer->software_context = NULL;
            }
            if (renderer->software_surface) {
                cairo_surface_destroy(renderer->software_surface);
                renderer->software_surface = NULL;
            }
            renderer_init_software(renderer, width, height);
            break;
    }
}

void editor_renderer_clear(EditorRenderer* renderer, float r, float g, float b, float a) {
    if (!renderer) return;
    
    switch (renderer->backend) {
        case RENDER_BACKEND_MODERN_GL:
            if (renderer->modern_gl) {
                oz_gl_modern_clear(renderer->modern_gl, r, g, b, a);
            }
            break;
            
        case RENDER_BACKEND_LEGACY_GL:
            glClearColor(r, g, b, a);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            break;
            
        case RENDER_BACKEND_SOFTWARE:
            if (renderer->software_context) {
                cairo_set_source_rgba(renderer->software_context, r, g, b, a);
                cairo_paint(renderer->software_context);
            }
            break;
    }
}

void editor_renderer_begin_frame(EditorRenderer* renderer) {
    if (!renderer) return;
    
    switch (renderer->backend) {
        case RENDER_BACKEND_MODERN_GL:
            // Modern GL state is managed internally
            break;
            
        case RENDER_BACKEND_LEGACY_GL:
            glEnable(GL_DEPTH_TEST);
            glEnable(GL_CULL_FACE);
            if (renderer->show_wireframe) {
                glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            } else {
                glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            }
            break;
            
        case RENDER_BACKEND_SOFTWARE:
            // Clear to background color
            editor_renderer_clear(renderer, 0.2f, 0.2f, 0.2f, 1.0f);
            break;
    }
}

void editor_renderer_end_frame(EditorRenderer* renderer) {
    if (!renderer) return;
    
    switch (renderer->backend) {
        case RENDER_BACKEND_MODERN_GL:
        case RENDER_BACKEND_LEGACY_GL:
            // OpenGL presents automatically
            break;
            
        case RENDER_BACKEND_SOFTWARE:
            // Software rendering requires explicit present to GTK widget
            break;
    }
}

void editor_renderer_set_camera(EditorRenderer* renderer, const OzCamera* camera) {
    if (!renderer || !camera) return;
    
    switch (renderer->backend) {
        case RENDER_BACKEND_MODERN_GL:
            if (renderer->modern_gl) {
                oz_gl_modern_apply_camera(renderer->modern_gl, camera);
            }
            break;
            
        case RENDER_BACKEND_LEGACY_GL:
            // Setup legacy GL matrices
            glMatrixMode(GL_PROJECTION);
            glLoadIdentity();
            
            float aspect = (float)renderer->viewport_width / (float)renderer->viewport_height;
            gluPerspective(60.0, aspect, 0.1, 1000.0);
            
            glMatrixMode(GL_MODELVIEW);
            glLoadIdentity();
            
            // Apply camera transform
            glRotatef(-camera->pitch * 57.29578f, 1, 0, 0);
            glRotatef(-camera->yaw * 57.29578f, 0, 0, 1);
            glTranslatef(-camera->position.x, -camera->position.y, -camera->position.z);
            break;
            
        case RENDER_BACKEND_SOFTWARE:
            // Software rendering doesn't support 3D camera directly
            // Would need to implement 3D projection manually
            break;
    }
}

void editor_renderer_render_scene(EditorRenderer* renderer, const EditorScene* scene) {
    if (!renderer || !scene) return;
    
    switch (renderer->backend) {
        case RENDER_BACKEND_MODERN_GL:
            if (renderer->modern_gl) {
                // Set camera
                oz_gl_modern_apply_camera(renderer->modern_gl, &scene->camera);
                
                // Set lighting
                if (renderer->show_lighting) {
                    oz_gl_modern_set_ambient_light(renderer->modern_gl, 0.2f, 0.2f, 0.3f);
                } else {
                    oz_gl_modern_set_ambient_light(renderer->modern_gl, 0.8f, 0.8f, 0.8f);
                }
                
                // Render scene elements
                if (renderer->show_grid) {
                    oz_gl_modern_draw_grid(renderer->modern_gl, 10.0f, 1.0f, 
                                          &(OzVec3){0.3f, 0.3f, 0.3f});
                }
                
                if (renderer->show_axes) {
                    oz_gl_modern_draw_axes(renderer->modern_gl, 2.0f);
                }
                
                // Render map
                if (renderer->show_wireframe) {
                    oz_gl_modern_draw_map_wireframe(renderer->modern_gl, &scene->map, 
                                                   scene->selected_brush_index);
                } else {
                    oz_gl_modern_draw_map_filled(renderer->modern_gl, &scene->map);
                }
                
                // Render objects
                for (size_t i = 0; i < scene->obj_count; i++) {
                    const EditorObject* obj = &scene->objects[i];
                    OzVec3 color = {1.0f, 1.0f, 0.0f}; // Yellow by default
                    
                    if ((int)i == scene->selected_object_index) {
                        color = (OzVec3){1.0f, 0.0f, 0.0f}; // Red for selected
                    }
                    
                    switch (obj->type) {
                        case OBJ_ZONE: {
                            OzVec3 center = {obj->as.zone.center[0], obj->as.zone.center[1], obj->as.zone.center[2]};
                            oz_gl_modern_draw_zone_circle(renderer->modern_gl, &center, 
                                                         obj->as.zone.radius, &color);
                            break;
                        }
                        case OBJ_PICKUP: {
                            OzVec3 center = {obj->as.pickup.position[0], obj->as.pickup.position[1], obj->as.pickup.position[2]};
                            oz_gl_modern_draw_pickup_cube(renderer->modern_gl, &center, 0.5f, &color);
                            break;
                        }
                        case OBJ_PLAYER_START: {
                            OzVec3 pos = {obj->as.pstart.position[0], obj->as.pstart.position[1], obj->as.pstart.position[2]};
                            oz_gl_modern_draw_player_start(renderer->modern_gl, &pos, 
                                                          obj->as.pstart.yaw, &color);
                            break;
                        }
                    }
                }
                
                // Render gizmo if something is selected
                if (scene->selected_brush_index >= 0 || scene->selected_object_index >= 0) {
                    OzVec3 gizmo_center = {0, 0, 0};
                    
                    if (scene->selected_brush_index >= 0 && (size_t)scene->selected_brush_index < scene->map.count) {
                        const OzBrush* brush = &scene->map.brushes[scene->selected_brush_index];
                        if (brush->type == OZ_BRUSH_BOX) {
                            gizmo_center = brush->as.box.center;
                        } else if (brush->type == OZ_BRUSH_CYLINDER) {
                            gizmo_center = brush->as.cyl.center;
                        }
                    } else if (scene->selected_object_index >= 0 && (size_t)scene->selected_object_index < scene->obj_count) {
                        const EditorObject* obj = &scene->objects[scene->selected_object_index];
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
                    
                    oz_gl_modern_draw_gizmo(renderer->modern_gl, &gizmo_center, 1.0f, scene->gizmo_axis);
                }
            }
            break;
            
        case RENDER_BACKEND_LEGACY_GL:
            // TODO: Implement legacy GL scene rendering
            break;
            
        case RENDER_BACKEND_SOFTWARE:
            renderer_software_render_scene(renderer, scene);
            break;
    }
}

static void renderer_software_render_scene(EditorRenderer* renderer, const EditorScene* scene) {
    if (!renderer->software_context || !scene) return;
    
    cairo_t* cr = renderer->software_context;
    
    // Simple 2D top-down view for software rendering
    cairo_save(cr);
    
    // Set up coordinate system (center viewport, scale appropriately)
    cairo_translate(cr, renderer->viewport_width * 0.5, renderer->viewport_height * 0.5);
    cairo_scale(cr, 20.0, -20.0); // 20 pixels per unit, flip Y
    
    if (renderer->show_grid) {
        renderer_software_render_grid(renderer, 10.0f, 1.0f);
    }
    
    if (renderer->show_axes) {
        renderer_software_render_axes(renderer, 5.0f);
    }
    
    // Render map brushes as 2D shapes
    for (size_t i = 0; i < scene->map.count; i++) {
        const OzBrush* brush = &scene->map.brushes[i];
        
        if ((int)i == scene->selected_brush_index) {
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
    
    // Render objects
    for (size_t i = 0; i < scene->obj_count; i++) {
        const EditorObject* obj = &scene->objects[i];
        
        if ((int)i == scene->selected_object_index) {
            cairo_set_source_rgb(cr, 1.0, 0.0, 0.0); // Red for selected
        } else {
            cairo_set_source_rgb(cr, 1.0, 1.0, 0.0); // Yellow for objects
        }
        
        switch (obj->type) {
            case OBJ_ZONE:
                cairo_arc(cr, obj->as.zone.center[0], obj->as.zone.center[1], 
                         obj->as.zone.radius, 0, 2 * M_PI);
                cairo_stroke(cr);
                break;
                
            case OBJ_PICKUP:
                cairo_rectangle(cr, obj->as.pickup.position[0] - 0.25f, obj->as.pickup.position[1] - 0.25f,
                               0.5f, 0.5f);
                cairo_fill(cr);
                break;
                
            case OBJ_PLAYER_START:
                cairo_arc(cr, obj->as.pstart.position[0], obj->as.pstart.position[1], 
                         0.3f, 0, 2 * M_PI);
                cairo_fill(cr);
                break;
        }
    }
    
    cairo_restore(cr);
}

static void renderer_software_render_grid(EditorRenderer* renderer, float extent, float step) {
    if (!renderer->software_context) return;
    
    cairo_t* cr = renderer->software_context;
    
    cairo_save(cr);
    cairo_set_source_rgb(cr, 0.3, 0.3, 0.3);
    cairo_set_line_width(cr, 0.02);
    
    // Draw grid lines
    for (float x = -extent; x <= extent; x += step) {
        cairo_move_to(cr, x, -extent);
        cairo_line_to(cr, x, extent);
    }
    for (float y = -extent; y <= extent; y += step) {
        cairo_move_to(cr, -extent, y);
        cairo_line_to(cr, extent, y);
    }
    
    cairo_stroke(cr);
    cairo_restore(cr);
}

static void renderer_software_render_axes(EditorRenderer* renderer, float length) {
    if (!renderer->software_context) return;
    
    cairo_t* cr = renderer->software_context;
    
    cairo_save(cr);
    cairo_set_line_width(cr, 0.05);
    
    // X axis (red)
    cairo_set_source_rgb(cr, 1.0, 0.0, 0.0);
    cairo_move_to(cr, 0, 0);
    cairo_line_to(cr, length, 0);
    cairo_stroke(cr);
    
    // Y axis (green)
    cairo_set_source_rgb(cr, 0.0, 1.0, 0.0);
    cairo_move_to(cr, 0, 0);
    cairo_line_to(cr, 0, length);
    cairo_stroke(cr);
    
    cairo_restore(cr);
}

// Rendering options
void editor_renderer_set_wireframe_mode(EditorRenderer* renderer, bool enabled) {
    if (!renderer) return;
    renderer->show_wireframe = enabled;
}

void editor_renderer_set_lighting_enabled(EditorRenderer* renderer, bool enabled) {
    if (!renderer) return;
    renderer->show_lighting = enabled;
}

void editor_renderer_set_textures_enabled(EditorRenderer* renderer, bool enabled) {
    if (!renderer) return;
    renderer->show_textures = enabled;
}

void editor_renderer_set_grid_visible(EditorRenderer* renderer, bool visible) {
    if (!renderer) return;
    renderer->show_grid = visible;
}

void editor_renderer_set_axes_visible(EditorRenderer* renderer, bool visible) {
    if (!renderer) return;
    renderer->show_axes = visible;
}
