#ifndef OZ_LIGHT_H
#define OZ_LIGHT_H

#include "../oz_math.h"
#include "../oz_tex.h"
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// lightning API (exposed for GL and software rendering)
typedef enum OzLightType {
  OZ_LIGHT_POINT = 1,
  OZ_LIGHT_SPOT = 2,
  OZ_LIGHT_WAVE = 3,
} OzLightType;

typedef struct OzLight {
  OzLightType type;
  OzVec3 position;
  OzColor3 color_rgb;
  float intensity;
  float radius;

  float direction[3];
  float cone_angle_def;
  float falloff;

  float wave_frequency;
  float wave_amplitude;
  float wave_phase;
  int enabled;
} OzLight;

void oz_light_clear();

// Adding lights of different types / light control
int oz_lghts_add_point(float x, float y, float z, float r, float g, float b,
                       float intensity, float radius);

int oz_lghts_add_spot(float x, float y, float z, float dx, float dy, float dz,
                      float cone_angle_def, float falloff, float r, float g,
                      float b, float intensity, float radius);
int oz_lghts_add_wave(float x, float y, float z, float r, float g, float b,
                      float base_intensity, float radius, float freq_hz,
                      float amp, float phase);
void oz_lights_enable(int index, bool enabled);

// helper
size_t oz_light_count(void);
const OzLight *oz_lights_get(size_t index);

// ambient light color control for scene
void oz_lights_set_ambient(float r, float g, float b);
void oz_lights_get_ambient(float out_rgb[3]);

// update the time reference used by wavc/shimmer lights
void oz_lights_set_time_seonds(float t);

// Evaluate accumulated lightning at a world-space point with given surface
// normal out_rgb written in [0,1]
void oz_light_evalute_at(const float position[3], const float normal[3],
                         float[3]);

#ifdef __cplusplus
}
#endif

#endif // OZ_LIGHT
