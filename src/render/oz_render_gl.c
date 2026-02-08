#include "oz/render/oz_bsp.h"
#include "oz/render/oz_camera.h"
#include "oz/render/oz_render.h"
#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif
#include <math.h>
#include <string.h>

static void ensure_checker_texture(void) {
  static GLuint tex = 0;
  if (tex != 0) {
    glBindTexture(GL_TEXTURE_2D, tex);
    return;
  }
  unsigned char pixels[64 * 64 * 3];
  for (int y = 0; y < 64; ++y) {
    for (int x = 0; x < 64; ++x) {
      int c = (((x >> 3) ^ (y >> 3)) & 1) ? 200 : 120;
      int i = (y * 64 + x) * 3;
      pixels[i + 0] = (unsigned char)c;
      pixels[i + 1] = (unsigned char)c;
      pixels[i + 2] = (unsigned char)c;
    }
  }
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 64, 64, 0, GL_RGB, GL_UNSIGNED_BYTE,
               pixels);
}

static void face_light_rgb(const float face_center[3], float nx, float ny,
                           float nz, float out_rgb[3]) {
  float N[3] = {nx, ny, nz};
  oz_lights_evaluate_at(face_center, N, out_rgb);
}

static void draw_box_faces(const OzBrushBox *b) {
  if (!b)
    return;
  const float cx = b->center.x, cy = b->center.y, cz = b->center.z;
  const float hx = b->half.x, hy = b->half.y, hz = b->half.z;
  float v[8][3] = {
      {cx - hx, cy - hy, cz - hz}, {cx + hx, cy - hy, cz - hz},
      {cx + hx, cy + hy, cz - hz}, {cx - hx, cy + hy, cz - hz},
      {cx - hx, cy - hy, cz + hz}, {cx + hx, cy - hy, cz + hz},
      {cx + hx, cy + hy, cz + hz}, {cx - hx, cy + hy, cz + hz},
  };
  int faces[6][4] = {{0, 1, 2, 3}, {4, 5, 6, 7}, {0, 1, 5, 4},
                     {2, 3, 7, 6}, {1, 2, 6, 5}, {0, 3, 7, 4}};
  // Normals for axis-aligned box faces
  float fn[6][3] = {{0, 0, -1}, {0, 0, 1}, {0, -1, 0},
                    {0, 1, 0},  {1, 0, 0}, {-1, 0, 0}};
  ensure_checker_texture();
  glEnable(GL_TEXTURE_2D);
  for (int f = 0; f < 6; ++f) {
    float cx = 0, cy = 0, cz = 0;
    for (int vi = 0; vi < 4; ++vi) {
      float *a = v[faces[f][vi]];
      cx += a[0];
      cy += a[1];
      cz += a[2];
    }
    cx *= 0.25f;
    cy *= 0.25f;
    cz *= 0.25f;
    float rgb[3];
    face_light_rgb((float[3]){cx, cy, cz}, fn[f][0], fn[f][1], fn[f][2], rgb);
    glColor3f(rgb[0], rgb[1], rgb[2]);
    // Choose UV mapping axes based on face normal
    int ax0 = 0, ax1 = 1; // default XY
    if (f == 0 || f == 1) {
      ax0 = 0;
      ax1 = 1;
    } // Z faces -> map X,Y
    else if (f == 2 || f == 3) {
      ax0 = 0;
      ax1 = 2;
    } // Y faces -> map X,Z
    else {
      ax0 = 1;
      ax1 = 2;
    }                        // X faces -> map Y,Z
    const float tile = 1.0f; // 1 unit per tile
    glBegin(GL_QUADS);
    for (int vi = 0; vi < 4; ++vi) {
      float *a = v[faces[f][vi]];
      float u = a[ax0] * (1.0f / tile);
      float vv = a[ax1] * (1.0f / tile);
      glTexCoord2f(u, vv);
      glVertex3f(a[0], a[1], a[2]);
    }
    glEnd();
  }
  glDisable(GL_TEXTURE_2D);
}

static void draw_cylinder_faces(const OzBrushCylinder *c) {
  if (!c)
    return;
  int seg = c->segments > 3 ? c->segments : 16;
  float hz = c->height * 0.5f;
  glEnable(GL_CULL_FACE);
  // Draw side quads as triangle strips with per-segment lighting
  for (int i = 0; i < seg; ++i) {
    float a0 = (float)i / (float)seg * 6.2831853f;
    float a1 = (float)(i + 1) / (float)seg * 6.2831853f;
    float x0 = c->center.x + cosf(a0) * c->radius_x;
    float y0 = c->center.y + sinf(a0) * c->radius_y;
    float x1 = c->center.x + cosf(a1) * c->radius_x;
    float y1 = c->center.y + sinf(a1) * c->radius_y;
    // Approx normal for this segment (averaged)
    float nx = (cosf(a0) + cosf(a1));
    float ny = (sinf(a0) + sinf(a1));
    float nz = 0.0f;
    float nlen = sqrtf(nx * nx + ny * ny + nz * nz);
    if (nlen > 1e-6f) {
      nx /= nlen;
      ny /= nlen;
    }
    float cx = (x0 + x1) * 0.5f;
    float cy = (y0 + y1) * 0.5f;
    float cz = c->center.z;
    float rgb[3];
    face_light_rgb((float[3]){cx, cy, cz}, nx, ny, nz, rgb);
    glColor3f(rgb[0], rgb[1], rgb[2]);
    glBegin(GL_TRIANGLE_STRIP);
    glVertex3f(x0, y0, c->center.z - hz);
    glVertex3f(x0, y0, c->center.z + hz);
    glVertex3f(x1, y1, c->center.z - hz);
    glVertex3f(x1, y1, c->center.z + hz);
    glEnd();
  }
  // Caps as triangle fans with face lighting
  for (int cap = 0; cap < 2; ++cap) {
    float z = c->center.z + (cap == 0 ? -hz : hz);
    float fnz = (cap == 0 ? -1.0f : 1.0f);
    float rgb[3];
    face_light_rgb((float[3]){c->center.x, c->center.y, z}, 0.0f, 0.0f, fnz,
                   rgb);
    glColor3f(rgb[0], rgb[1], rgb[2]);
    glBegin(GL_TRIANGLE_FAN);
    glVertex3f(c->center.x, c->center.y, z);
    for (int i = 0; i <= seg; ++i) {
      float a = (float)(i % seg) / (float)seg * 6.2831853f;
      float x = c->center.x + cosf(a) * c->radius_x;
      float y = c->center.y + sinf(a) * c->radius_y;
      glVertex3f(x, y, z);
    }
    glEnd();
  }
}

static void draw_box_wire(const OzBrushBox *b) {
  if (!b)
    return;
  const float cx = b->center.x, cy = b->center.y, cz = b->center.z;
  const float hx = b->half.x, hy = b->half.y, hz = b->half.z;
  float v[8][3] = {
      {cx - hx, cy - hy, cz - hz}, {cx + hx, cy - hy, cz - hz},
      {cx + hx, cy + hy, cz - hz}, {cx - hx, cy + hy, cz - hz},
      {cx - hx, cy - hy, cz + hz}, {cx + hx, cy - hy, cz + hz},
      {cx + hx, cy + hy, cz + hz}, {cx - hx, cy + hy, cz + hz},
  };
  int edges[12][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6},
                      {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
  glBegin(GL_LINES);
  for (int e = 0; e < 12; ++e) {
    float *a = v[edges[e][0]];
    float *b2 = v[edges[e][1]];
    glVertex3f(a[0], a[1], a[2]);
    glVertex3f(b2[0], b2[1], b2[2]);
  }
  glEnd();
}

// oz_render_default_projection is implemented in oz_render_common.c

void oz_render_gl_apply_camera(const OzCamera *cam, int w, int h) {
  if (!cam)
    return;
  if (h <= 0)
    h = 1;
  if (w <= 0)
    w = 1;
  glViewport(0, 0, w, h);
  glEnable(GL_DEPTH_TEST);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  float aspect = (float)w / (float)h;
  OzProjectionParams pp = oz_render_default_projection();
  float top = pp.nearPlane * tanf(pp.verticalFovDegrees * 3.14159f / 360.0f);
  float right = top * aspect;
  glFrustum(-right, right, -top, top, pp.nearPlane, pp.farPlane);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glRotatef(-cam->pitch * 57.29578f, 1, 0, 0);
  glRotatef(-cam->yaw * 57.29578f, 0, 0, 1);
  glTranslatef(-cam->position.x, -cam->position.y, -cam->position.z);
}

void oz_render_gl_draw_map_filled(const OzMap *map) {
  if (!map)
    return;
  glEnable(GL_CULL_FACE);
  for (size_t i = 0; i < map->count; ++i) {
    const OzBrush *br = &map->brushes[i];
    if (br->type == OZ_BRUSH_BOX)
      draw_box_faces(&br->as.box);
    else if (br->type == OZ_BRUSH_CYLINDER)
      draw_cylinder_faces(&br->as.cyl);
    // TODO: spheres/pyramids/planes if needed
  }
}

void oz_render_gl_draw_map_wireframe(const OzMap *map) {
  if (!map)
    return;
  glDisable(GL_CULL_FACE);
  for (size_t i = 0; i < map->count; ++i) {
    const OzBrush *br = &map->brushes[i];
    if (br->type == OZ_BRUSH_BOX)
      draw_box_wire(&br->as.box); /* TODO: other types */
  }
}
