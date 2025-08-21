#ifndef OZ_GL_MODERN_H
#define OZ_GL_MODERN_H

#include "../oz_math.h"
#include "oz_bsp.h"
#include "oz_camera.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Modern OpenGL 3.2+ core profile renderer
// Uses VBO/VAO + shaders instead of immediate mode

typedef struct OzGLModern OzGLModern;

// Initialize the modern GL renderer (creates shaders, VAOs, etc.)
// Returns NULL on failure
OzGLModern *oz_gl_modern_init(void);

// Cleanup renderer resources
void oz_gl_modern_destroy(OzGLModern *renderer);

// Check if modern GL is available (core profile 3.2+)
bool oz_gl_modern_is_available(void);

// Set viewport and apply camera transform
void oz_gl_modern_set_viewport(OzGLModern *renderer, int width, int height);
void oz_gl_modern_apply_camera(OzGLModern *renderer, const OzCamera *camera);

// Lighting control
void oz_gl_modern_set_ambient_light(OzGLModern *renderer, float r, float g,
                                    float b);
void oz_gl_modern_add_point_light(OzGLModern *renderer, const OzVec3 *pos,
                                  const OzVec3 *color, float intensity,
                                  float range);
void oz_gl_modern_clear_lights(OzGLModern *renderer);

// Drawing functions
void oz_gl_modern_clear(OzGLModern *renderer, float r, float g, float b,
                        float a);
void oz_gl_modern_draw_grid(OzGLModern *renderer, float extent, float step,
                            const OzVec3 *color);
void oz_gl_modern_draw_axes(OzGLModern *renderer, float length);
void oz_gl_modern_draw_map_filled(OzGLModern *renderer, const OzMap *map);
void oz_gl_modern_draw_map_wireframe(OzGLModern *renderer, const OzMap *map,
                                     int selected_index);
void oz_gl_modern_draw_gizmo(OzGLModern *renderer, const OzVec3 *center,
                             float scale, int highlight_axis);

// Object drawing
void oz_gl_modern_draw_zone_circle(OzGLModern *renderer, const OzVec3 *center,
                                   float radius, const OzVec3 *color);
void oz_gl_modern_draw_pickup_cube(OzGLModern *renderer, const OzVec3 *center,
                                   float size, const OzVec3 *color);
void oz_gl_modern_draw_player_start(OzGLModern *renderer, const OzVec3 *pos,
                                    float yaw, const OzVec3 *color);

#ifdef __cplusplus
}
#endif

#endif // OZ_GL_MODERN_H
