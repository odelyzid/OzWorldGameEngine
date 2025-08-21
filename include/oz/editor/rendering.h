#ifndef OZ_EDITOR_RENDERING_H
#define OZ_EDITOR_RENDERING_H

#include "../render/oz_bsp.h"
#include "../render/oz_camera.h"
#include "../render/oz_gl_modern.h"
#include "../render/oz_light.h"
#include <cairo/cairo.h>
#include <gtk-3.0/gtk/gtk.h>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
typedef struct EditorScene EditorScene;
typedef struct EditorUI EditorUI;

// Rendering Backend Types
typedef enum EditorRenderBackend {
  RENDER_BACKEND_MODERN_GL, // VBO/VAO + shaders (OpenGL 3.2+ core)
  RENDER_BACKEND_LEGACY_GL, // Immediate mode (OpenGL 2.1 compatibility)
  RENDER_BACKEND_SOFTWARE   // Cairo-based software rendering
} EditorRenderBackend;

// Rendering System
typedef struct EditorRenderer {
  EditorRenderBackend backend;

  // Modern GL renderer (if available)
  OzGLModern *modern_gl;

  // Legacy GL resources
  unsigned int legacy_texture;
  int legacy_tex_w, legacy_tex_h;

  // Software rendering context
  cairo_surface_t *software_surface;
  cairo_t *software_context;

  // Viewport state
  int viewport_width;
  int viewport_height;

  // Rendering options
  bool show_grid;
  bool show_axes;
  bool show_wireframe;
  bool show_lighting;
  bool show_textures;

  // Debug rendering
  bool show_debug_info;
  bool show_performance_overlay;
  bool show_memory_usage;
} EditorRenderer;

// Renderer Creation and Management
EditorRenderer *editor_renderer_create(void);
void editor_renderer_destroy(EditorRenderer *renderer);
bool editor_renderer_initialize_gl(EditorRenderer *renderer,
                                   GtkGLArea *gl_area);
bool editor_renderer_initialize_software(EditorRenderer *renderer, int width,
                                         int height);

// Backend Detection and Switching
EditorRenderBackend editor_renderer_detect_best_backend(void);
bool editor_renderer_switch_backend(EditorRenderer *renderer,
                                    EditorRenderBackend backend);
const char *editor_renderer_get_backend_name(EditorRenderBackend backend);

// Viewport Management
void editor_renderer_set_viewport(EditorRenderer *renderer, int width,
                                  int height);
void editor_renderer_clear(EditorRenderer *renderer, float r, float g, float b,
                           float a);

// Camera and View
void editor_renderer_set_camera(EditorRenderer *renderer,
                                const OzCamera *camera);
void editor_renderer_set_projection(EditorRenderer *renderer, float fov,
                                    float aspect, float near, float far);

// Scene Rendering
void editor_renderer_begin_frame(EditorRenderer *renderer);
void editor_renderer_end_frame(EditorRenderer *renderer);
void editor_renderer_render_scene(EditorRenderer *renderer,
                                  const EditorScene *scene);

// Geometry Rendering
void editor_renderer_render_map(EditorRenderer *renderer, const OzMap *map,
                                int selected_index);
void editor_renderer_render_objects(EditorRenderer *renderer,
                                    const EditorScene *scene);
void editor_renderer_render_grid(EditorRenderer *renderer, float extent,
                                 float step);
void editor_renderer_render_axes(EditorRenderer *renderer, float length);
void editor_renderer_render_gizmo(EditorRenderer *renderer,
                                  const OzVec3 *center, float scale,
                                  int highlight_axis);

// UI and Overlay Rendering
void editor_renderer_render_selection_outline(EditorRenderer *renderer,
                                              const EditorScene *scene);
void editor_renderer_render_csg_preview(EditorRenderer *renderer,
                                        const EditorScene *scene);
void editor_renderer_render_debug_overlay(EditorRenderer *renderer,
                                          const EditorScene *scene);
void editor_renderer_render_performance_info(EditorRenderer *renderer,
                                             float fps, float frame_time);

// Lighting Control
void editor_renderer_set_ambient_light(EditorRenderer *renderer, float r,
                                       float g, float b);
void editor_renderer_add_point_light(EditorRenderer *renderer,
                                     const OzVec3 *position,
                                     const OzVec3 *color, float intensity,
                                     float range);
void editor_renderer_clear_lights(EditorRenderer *renderer);

// Rendering Options
void editor_renderer_set_wireframe_mode(EditorRenderer *renderer, bool enabled);
void editor_renderer_set_lighting_enabled(EditorRenderer *renderer,
                                          bool enabled);
void editor_renderer_set_textures_enabled(EditorRenderer *renderer,
                                          bool enabled);
void editor_renderer_set_grid_visible(EditorRenderer *renderer, bool visible);
void editor_renderer_set_axes_visible(EditorRenderer *renderer, bool visible);

// Debug and Performance
void editor_renderer_enable_debug_info(EditorRenderer *renderer, bool enabled);
void editor_renderer_enable_performance_overlay(EditorRenderer *renderer,
                                                bool enabled);
void editor_renderer_capture_frame(EditorRenderer *renderer,
                                   const char *filename);

// Utility Functions
void editor_renderer_project_point(EditorRenderer *renderer,
                                   const OzVec3 *world_pos, float *screen_x,
                                   float *screen_y);
void editor_renderer_unproject_point(EditorRenderer *renderer, float screen_x,
                                     float screen_y, OzVec3 *world_pos);
bool editor_renderer_ray_from_screen(EditorRenderer *renderer, float screen_x,
                                     float screen_y, OzVec3 *ray_origin,
                                     OzVec3 *ray_direction);

#ifdef __cplusplus
}
#endif

#endif // OZ_EDITOR_RENDERING_H
