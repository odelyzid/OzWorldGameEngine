#include "../../include/oz/render/oz_camera.h"
#include "../../include/oz/render/oz_render.h"
#include <cairo/cairo.h>
#include <math.h>
#include <string.h>

// --- Simple global lighting state (shared across GL/Software) ---
static OzLight g_lights[64];
static size_t g_light_count = 0;
static float g_ambient_rgb[3] = {0.15f, 0.15f, 0.18f};
static float g_time_seconds = 0.0f;

void oz_lights_clear(void) { g_light_count = 0; }

int oz_lights_add_point(float x, float y, float z, float r, float g, float b,
                        float intensity, float radius) {
  if (g_light_count >= (sizeof(g_lights) / sizeof(g_lights[0])))
    return -1;
  OzLight *L = &g_lights[g_light_count++];
  memset(L, 0, sizeof(*L));
  L->type = OZ_LIGHT_POINT;
  L->position.x = x;
  L->position.y = y;
  L->position.z = z;
  L->color_rgb.r = r;
  L->color_rgb.g = g;
  L->color_rgb.b = b;
  L->intensity = intensity;
  L->radius = radius;
  L->enabled = 1;
  return (int)g_light_count - 1;
}

int oz_lights_add_spot(float x, float y, float z, float dx, float dy, float dz,
                       float cone_angle_deg, float falloff, float r, float g,
                       float b, float intensity, float radius) {
  if (g_light_count >= (sizeof(g_lights) / sizeof(g_lights[0])))
    return -1;
  OzLight *L = &g_lights[g_light_count++];
  memset(L, 0, sizeof(*L));
  L->type = OZ_LIGHT_SPOT;
  L->position.x = x;
  L->position.y = y;
  L->position.z = z;
  float len = sqrtf(dx * dx + dy * dy + dz * dz);
  if (len < 1e-6f) {
    dx = 0;
    dy = 0;
    dz = -1;
    len = 1;
  }
  L->direction[0] = dx / len;
  L->direction[1] = dy / len;
  L->direction[2] = dz / len;
  L->cone_angle_def = cone_angle_deg <= 0.0f ? 15.0f : cone_angle_deg;
  L->falloff = falloff <= 0.0f ? 2.0f : falloff;
  L->color_rgb.r = r;
  L->color_rgb.g = g;
  L->color_rgb.b = b;
  L->intensity = intensity;
  L->radius = radius;
  L->enabled = 1;
  return (int)g_light_count - 1;
}

int oz_lights_add_wave(float x, float y, float z, float r, float g, float b,
                       float base_intensity, float radius, float freq_hz,
                       float amp, float phase) {
  if (g_light_count >= (sizeof(g_lights) / sizeof(g_lights[0])))
    return -1;
  OzLight *L = &g_lights[g_light_count++];
  memset(L, 0, sizeof(*L));
  L->type = OZ_LIGHT_WAVE;
  L->position.x = x;
  L->position.y = y;
  L->position.z = z;
  L->color_rgb.r = r;
  L->color_rgb.g = g;
  L->color_rgb.b = b;
  L->intensity = base_intensity;
  L->radius = radius;
  L->enabled = 1;
  L->wave_frequency = freq_hz;
  L->wave_amplitude = amp;
  L->wave_phase = phase;
  return (int)g_light_count - 1;
}

void oz_lights_enable(int index, bool enabled) {
  if (index >= 0 && (size_t)index < g_light_count)
    g_lights[index].enabled = enabled ? 1 : 0;
}
size_t oz_lights_count(void) { return g_light_count; }
const OzLight *oz_lights_get(size_t index) {
  return (index < g_light_count) ? &g_lights[index] : NULL;
}
void oz_lights_set_ambient(float r, float g, float b) {
  g_ambient_rgb[0] = r;
  g_ambient_rgb[1] = g;
  g_ambient_rgb[2] = b;
}
void oz_lights_get_ambient(float out_rgb[3]) {
  if (out_rgb) {
    out_rgb[0] = g_ambient_rgb[0];
    out_rgb[1] = g_ambient_rgb[1];
    out_rgb[2] = g_ambient_rgb[2];
  }
}
void oz_lights_set_time_seconds(float t) { g_time_seconds = t; }

static inline float saturatef(float v) {
  return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}

void oz_lights_evaluate_at(const float position[3], const float normal[3],
                           float out_rgb[3]) {
  float nx = normal ? normal[0] : 0.0f, ny = normal ? normal[1] : 0.0f,
        nz = normal ? normal[2] : 1.0f;

  float nlen = sqrtf(nx * nx + ny * ny + nz * nz);
  if (nlen > 1e-6f) {
    nx /= nlen;
    ny /= nlen;
    nz /= nlen;
  }
  float acc[3] = {g_ambient_rgb[0], g_ambient_rgb[1], g_ambient_rgb[2]};
  for (size_t i = 0; i < g_light_count; ++i) {
    const OzLight *L = &g_lights[i];
    if (!L->enabled)
      continue;
    float vx = L->position.x - position[0];
    float vy = L->position.y - position[1];
    float vz = L->position.z - position[2];
    float d = sqrtf(vx * vx + vy * vy + vz * vz);
    if (d < 1e-6f)
      d = 1e-6f;
    float vxn = vx / d, vyn = vy / d, vzn = vz / d;
    float att = 1.0f;
    if (L->radius > 0.0f) {
      float t = 1.0f - (d / L->radius);
      att = t > 0.0f ? (t * t) : 0.0f; // smooth falloff within radius
    } else {
      att = 1.0f / (1.0f + 0.15f * d + 0.02f * d * d);
    }
    float I = L->intensity;
    if (L->type == OZ_LIGHT_WAVE) {
      float s =
          sinf(6.2831853f * L->wave_frequency * g_time_seconds + L->wave_phase);
      I = L->intensity * (1.0f + L->wave_amplitude * s);
    }
    float lambert = nx * vxn + ny * vyn + nz * vzn;
    if (lambert < 0.0f)
      lambert = 0.0f;
    float cone = 1.0f;
    if (L->type == OZ_LIGHT_SPOT) {
      float ndotm =
          -(L->direction[0] * vxn + L->direction[1] * vyn +
            L->direction[2] * vzn); // -dir points along spotlight axis
      float cmin = cosf((L->cone_angle_def > 0.0f ? L->cone_angle_def : 15.0f) *
                        3.14159265f / 180.0f);
      if (ndotm <= cmin)
        cone = 0.0f;
      else
        cone = powf((ndotm - cmin) / (1.0f - cmin),
                    (L->falloff > 0.0f ? L->falloff : 2.0f));
    }
    float s = I * att * lambert * cone;
    acc[0] += L->color_rgb.r * s;
    acc[1] += L->color_rgb.g * s;
    acc[2] += L->color_rgb.b * s;
  }
  out_rgb[0] = saturatef(acc[0]);
  out_rgb[1] = saturatef(acc[1]);
  out_rgb[2] = saturatef(acc[2]);
}

// Render editor gizmos/nodes in software mode using Cairo.
// Currently draws a 3D-projected tripod at world origin to match GL gizmo
// style.
void oz_render_soft_draw_gizmos(cairo_t *cr, int w, int h,
                                const struct OzCamera *cam) {
  if (!cr || !cam)
    return;
  cairo_save(cr);
  float ox = 0.0f, oy = 0.0f, oz = 0.0f;
  float len = 3.0f;
  float x0, y0, x1, y1;
  // X axis (red)
  oz_render_soft_project_point(cam, ox, oy, oz, w, h, &x0, &y0);
  oz_render_soft_project_point(cam, ox + len, oy, oz, w, h, &x1, &y1);
  cairo_set_source_rgb(cr, 1.0, 0.2, 0.2);
  cairo_move_to(cr, x0, y0);
  cairo_line_to(cr, x1, y1);
  cairo_set_line_width(cr, 2.5);
  cairo_stroke(cr);
  // Y axis (green)
  oz_render_soft_project_point(cam, ox, oy, oz, w, h, &x0, &y0);
  oz_render_soft_project_point(cam, ox, oy + len, oz, w, h, &x1, &y1);
  cairo_set_source_rgb(cr, 0.2, 1.0, 0.2);
  cairo_move_to(cr, x0, y0);
  cairo_line_to(cr, x1, y1);
  cairo_stroke(cr);
  // Z axis (blue)
  oz_render_soft_project_point(cam, ox, oy, oz, w, h, &x0, &y0);
  oz_render_soft_project_point(cam, ox, oy, oz + len, w, h, &x1, &y1);
  cairo_set_source_rgb(cr, 0.3, 0.6, 1.0);
  cairo_move_to(cr, x0, y0);
  cairo_line_to(cr, x1, y1);
  cairo_stroke(cr);
  cairo_restore(cr);
}

// Simple unlit or lit (diffuse-only) shading of a point in world space
// Returns grayscale intensity in [0,1]
static float diffuse_point_light(float x, float y, float z, float nx, float ny,
                                 float nz, float lx, float ly, float lz,
                                 float intensity) {
  float nlen = sqrtf(nx * nx + ny * ny + nz * nz);
  if (nlen > 1e-6f) {
    nx /= nlen;
    ny /= nlen;
    nz /= nlen;
  }
  float vx = lx - x, vy = ly - y, vz = lz - z;
  float vlen = sqrtf(vx * vx + vy * vy + vz * vz);
  if (vlen < 1e-6f)
    vlen = 1.0f;
  vx /= vlen;
  vy /= vlen;
  vz /= vlen;
  float d = nx * vx + ny * vy + nz * vz;
  if (d < 0.0f)
    d = 0.0f;
  float att = 1.0f / (1.0f + 0.2f * vlen + 0.05f * vlen * vlen);
  float i = intensity * d * att;
  if (i > 1.0f)
    i = 1.0f;
  return i;
}

// Placeholder: draw a lit quad given normal using Cairo with simple diffuse
// light
void oz_render_soft_draw_lit_quad(cairo_t *cr, float const v[4][3], float nx,
                                  float ny, float nz, float light_pos[3],
                                  float light_intensity, bool unlit) {
  if (!cr)
    return;
  // Compute face center for lighting
  float cx = 0, cy = 0, cz = 0;
  for (int i = 0; i < 4; ++i) {
    cx += v[i][0];
    cy += v[i][1];
    cz += v[i][2];
  }
  cx *= 0.25f;
  cy *= 0.25f;
  cz *= 0.25f;
  float lit =
      unlit
          ? 1.0f
          : (0.25f + 0.75f * diffuse_point_light(
                                 cx, cy, cz, nx, ny, nz, light_pos[0],
                                 light_pos[1], light_pos[2], light_intensity));
  // Convert to a soft gray for fill
  cairo_set_source_rgba(cr, lit, lit, lit, 0.25);
  // This function assumes caller already projected to screen; here we only
  // provide a placeholder API for future integration
}

OzProjectionParams oz_render_default_projection(void) {
  OzProjectionParams p;
  p.verticalFovDegrees = 60.0f;
  p.nearPlane = 0.1f;
  p.farPlane = 100.0f;
  return p;
}
