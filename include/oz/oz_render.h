#ifndef OZ_RENDER_H
#define OZ_RENDER_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct OzMap;
struct OzCamera;
struct _cairo;
typedef struct _cairo cairo_t; // forward decl from cairo.h

// --- Lighting API (shared between software and GL renderers) ---
typedef enum OzLightType {
  OZ_LIGHT_POINT = 1,
  OZ_LIGHT_SPOT = 2,
  OZ_LIGHT_WAVE = 3,
} OzLightType;

typedef struct OzLight {
  OzLightType type;
  float position[3];  // world-space position
  float color_rgb[3]; // per-light color multiplier in [0,1]
  float intensity;    // scalar multiplier (typical: 0.5..4)
  float radius;       // attenuation radius (0 -> infinite-ish)
  // Spot-only
  float direction[3];   // unit vector
  float cone_angle_deg; // inner cone (full-strength)
  float falloff;        // exponent falloff outside inner cone (>= 1)
  // Wave-only
  float wave_frequency; // Hz
  float wave_amplitude; // intensity amplitude 0..1 (additive)
  float wave_phase;     // radians
  int enabled;          // bool-like
} OzLight;

// Light system control (lives inside the render common module)
void oz_lights_clear(void);
// Returns index or -1 on failure
int oz_lights_add_point(float x, float y, float z, float r, float g, float b,
                        float intensity, float radius);
int oz_lights_add_spot(float x, float y, float z, float dx, float dy, float dz,
                       float cone_angle_deg, float falloff, float r, float g,
                       float b, float intensity, float radius);
int oz_lights_add_wave(float x, float y, float z, float r, float g, float b,
                       float base_intensity, float radius, float freq_hz,
                       float amp, float phase);
void oz_lights_enable(int index, bool enabled);
size_t oz_lights_count(void);
const OzLight *oz_lights_get(size_t index);
// Ambient color for the scene (defaults from OzWorldProps)
void oz_lights_set_ambient(float r, float g, float b);
void oz_lights_get_ambient(float out_rgb[3]);
// Update the time reference used by wave/shimmer lights
void oz_lights_set_time_seconds(float t);
// Evaluate accumulated lighting at a world-space point with a given surface
// normal out_rgb written in [0,1]
void oz_lights_evaluate_at(const float position[3], const float normal[3],
                           float out_rgb[3]);

// Shared projection parameters (must be used by both GL and software renderers)
typedef struct OzProjectionParams {
  float verticalFovDegrees; // e.g., 60.0
  float nearPlane;          // e.g., 0.1 (positive)
  float farPlane;           // e.g., 100.0 (positive)
} OzProjectionParams;

// Returns the default engine projection parameters
OzProjectionParams oz_render_default_projection(void);

// OpenGL-based rendering (expects a current GL context)
void oz_render_gl_apply_camera(const struct OzCamera *camera,
                               int viewport_width, int viewport_height);
void oz_render_gl_draw_map_filled(const struct OzMap *map);
void oz_render_gl_draw_map_wireframe(const struct OzMap *map);

// Software (Cairo) rendering for editor and tools
void oz_render_soft_draw_grid_axes(cairo_t *cr, int width, int height,
                                   const struct OzCamera *camera,
                                   bool draw_grid, bool draw_axes);
void oz_render_soft_draw_map(cairo_t *cr, int width, int height,
                             const struct OzMap *map,
                             const struct OzCamera *camera, int selected_index);
// Shared Cairo gizmo/nodes rendering
void oz_render_soft_draw_gizmos(cairo_t *cr, int width, int height,
                                const struct OzCamera *camera);
// Utility: project a world-space point to screen coordinates (software path
// math)
void oz_render_soft_project_point(const struct OzCamera *camera, float x,
                                  float y, float z, int width, int height,
                                  float *out_x, float *out_y);
// Placeholder for lit quad helper (pre-projected)
void oz_render_soft_draw_lit_quad(cairo_t *cr, float const v[4][3], float nx,
                                  float ny, float nz, float light_pos[3],
                                  float light_intensity, bool unlit);

#ifdef __cplusplus
}
#endif

#endif // OZ_RENDER_H
