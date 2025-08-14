#ifndef OZ_RENDER_H
#define OZ_RENDER_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct OzMap;
struct OzCamera;
struct _cairo; typedef struct _cairo cairo_t; // forward decl from cairo.h

// Shared projection parameters (must be used by both GL and software renderers)
typedef struct OzProjectionParams {
    float verticalFovDegrees; // e.g., 60.0
    float nearPlane;          // e.g., 0.1 (positive)
    float farPlane;           // e.g., 100.0 (positive)
} OzProjectionParams;

// Returns the default engine projection parameters
OzProjectionParams oz_render_default_projection(void);

// OpenGL-based rendering (expects a current GL context)
void oz_render_gl_apply_camera(const struct OzCamera* camera, int viewport_width, int viewport_height);
void oz_render_gl_draw_map_filled(const struct OzMap* map);
void oz_render_gl_draw_map_wireframe(const struct OzMap* map);

// Software (Cairo) rendering for editor and tools
void oz_render_soft_draw_grid_axes(cairo_t* cr, int width, int height, const struct OzCamera* camera, bool draw_grid, bool draw_axes);
void oz_render_soft_draw_map(cairo_t* cr, int width, int height, const struct OzMap* map, const struct OzCamera* camera, int selected_index);
// Shared Cairo gizmo/nodes rendering
void oz_render_soft_draw_gizmos(cairo_t* cr, int width, int height, const struct OzCamera* camera);
// Utility: project a world-space point to screen coordinates (software path math)
void oz_render_soft_project_point(const struct OzCamera* camera,
                                  float x, float y, float z,
                                  int width, int height,
                                  float* out_x, float* out_y);
// Placeholder for lit quad helper (pre-projected)
void oz_render_soft_draw_lit_quad(cairo_t* cr,
                                  float const v[4][3],
                                  float nx, float ny, float nz,
                                  float light_pos[3], float light_intensity,
                                  bool unlit);

#ifdef __cplusplus
}
#endif

#endif // OZ_RENDER_H
