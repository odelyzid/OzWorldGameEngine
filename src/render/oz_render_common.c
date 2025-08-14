#include "oz/oz_render.h"
#include "oz/oz_camera.h"
#include <cairo/cairo.h>
#include <math.h>

// Render editor gizmos/nodes in software mode using Cairo.
// Currently draws a 3D-projected tripod at world origin to match GL gizmo style.
void oz_render_soft_draw_gizmos(cairo_t* cr, int w, int h, const struct OzCamera* cam) {
    if (!cr || !cam) return;
    cairo_save(cr);
    float ox = 0.0f, oy = 0.0f, oz = 0.0f;
    float len = 3.0f;
    float x0,y0,x1,y1;
    // X axis (red)
    oz_render_soft_project_point(cam, ox, oy, oz, w, h, &x0, &y0);
    oz_render_soft_project_point(cam, ox + len, oy, oz, w, h, &x1, &y1);
    cairo_set_source_rgb(cr, 1.0, 0.2, 0.2);
    cairo_move_to(cr, x0, y0); cairo_line_to(cr, x1, y1); cairo_set_line_width(cr, 2.5); cairo_stroke(cr);
    // Y axis (green)
    oz_render_soft_project_point(cam, ox, oy, oz, w, h, &x0, &y0);
    oz_render_soft_project_point(cam, ox, oy + len, oz, w, h, &x1, &y1);
    cairo_set_source_rgb(cr, 0.2, 1.0, 0.2);
    cairo_move_to(cr, x0, y0); cairo_line_to(cr, x1, y1); cairo_stroke(cr);
    // Z axis (blue)
    oz_render_soft_project_point(cam, ox, oy, oz, w, h, &x0, &y0);
    oz_render_soft_project_point(cam, ox, oy, oz + len, w, h, &x1, &y1);
    cairo_set_source_rgb(cr, 0.3, 0.6, 1.0);
    cairo_move_to(cr, x0, y0); cairo_line_to(cr, x1, y1); cairo_stroke(cr);
    cairo_restore(cr);
}

// Simple unlit or lit (diffuse-only) shading of a point in world space
// Returns grayscale intensity in [0,1]
static float diffuse_point_light(float x, float y, float z,
                                 float nx, float ny, float nz,
                                 float lx, float ly, float lz,
                                 float intensity) {
    float nlen = sqrtf(nx*nx+ny*ny+nz*nz); if (nlen > 1e-6f) { nx/=nlen; ny/=nlen; nz/=nlen; }
    float vx = lx - x, vy = ly - y, vz = lz - z; float vlen = sqrtf(vx*vx+vy*vy+vz*vz); if (vlen < 1e-6f) vlen = 1.0f; vx/=vlen; vy/=vlen; vz/=vlen;
    float d = nx*vx + ny*vy + nz*vz; if (d < 0.0f) d = 0.0f; float att = 1.0f / (1.0f + 0.2f*vlen + 0.05f*vlen*vlen);
    float i = intensity * d * att; if (i > 1.0f) i = 1.0f; return i;
}

// Placeholder: draw a lit quad given normal using Cairo with simple diffuse light
void oz_render_soft_draw_lit_quad(cairo_t* cr,
                                  float const v[4][3],
                                  float nx, float ny, float nz,
                                  float light_pos[3], float light_intensity,
                                  bool unlit) {
    if (!cr) return;
    // Compute face center for lighting
    float cx=0,cy=0,cz=0; for (int i=0;i<4;++i){cx+=v[i][0]; cy+=v[i][1]; cz+=v[i][2];} cx*=0.25f; cy*=0.25f; cz*=0.25f;
    float lit = unlit ? 1.0f : (0.25f + 0.75f*diffuse_point_light(cx,cy,cz, nx,ny,nz, light_pos[0], light_pos[1], light_pos[2], light_intensity));
    // Convert to a soft gray for fill
    cairo_set_source_rgba(cr, lit, lit, lit, 0.25);
    // This function assumes caller already projected to screen; here we only provide a placeholder API for future integration
}

OzProjectionParams oz_render_default_projection(void) {
    OzProjectionParams p;
    p.verticalFovDegrees = 60.0f;
    p.nearPlane = 0.1f;
    p.farPlane = 100.0f;
    return p;
}
