#include "oz/oz_render.h"
#include "oz/oz_bsp.h"
#include "oz/oz_camera.h"
#include <math.h>
#include <cairo/cairo.h>
#include "oz/oz_render.h"

// Provide a weak default implementation in case oz_render_common is not linked
#if defined(__GNUC__)
__attribute__((weak)) OzProjectionParams oz_render_default_projection(void) {
    OzProjectionParams p; p.verticalFovDegrees = 60.0f; p.nearPlane = 0.1f; p.farPlane = 100.0f; return p;
}
#endif

// Transform a world-space point into camera space.
static void world_to_camera(const OzCamera* cam, float x, float y, float z, float* out_x, float* out_y, float* out_z) {
    float yaw = cam ? cam->yaw : 0.0f;
    float pitch = cam ? cam->pitch : 0.0f;
    float px = cam ? cam->position.x : 0.0f;
    float py = cam ? cam->position.y : -5.0f;
    float pz = cam ? cam->position.z : -2.5f;
    // Translate by camera position (inverse)
    x -= px; y -= py; z -= pz;
    // Rotate by inverse yaw (around Z)
    float cy = cosf(-yaw), sy = sinf(-yaw);
    float x1 =  cy * x + sy * y;
    float y1 = -sy * x + cy * y;
    float z1 = z;
    // Rotate by inverse pitch (around X)
    float cp = cosf(-pitch), sp = sinf(-pitch);
    float x2 = x1;
    float y2 = cp * y1 - sp * z1;
    float z2 = sp * y1 + cp * z1;
    if (out_x) *out_x = x2;
    if (out_y) *out_y = y2;
    if (out_z) *out_z = z2;
}

static void project_point(const OzCamera* cam, float x, float y, float z, int w, int h, float* out_x, float* out_y) {
    float cx, cy, cz; world_to_camera(cam, x, y, z, &cx, &cy, &cz);
    if (h <= 0) { h = 1; }
    if (w <= 0) { w = 1; }
    OzProjectionParams pp = oz_render_default_projection();
    const float PI = 3.14159265358979323846f;
    const float f = 1.0f / tanf(pp.verticalFovDegrees * PI / 360.0f);
    const float aspect = (float)w / (float)h;
    float denom = -cz; if (denom < pp.nearPlane) denom = pp.nearPlane;
    float ndc_x = (cx * (f / aspect)) / denom;
    float ndc_y = (cy * f) / denom;
    *out_x = (ndc_x * 0.5f + 0.5f) * (float)w;
    *out_y = (1.0f - (ndc_y * 0.5f + 0.5f)) * (float)h;
}

// Project a camera-space point to screen
static inline void project_point_camera(float cx, float cy, float cz, int w, int h, float* out_x, float* out_y) {
    if (h <= 0) { h = 1; }
    if (w <= 0) { w = 1; }
    OzProjectionParams pp = oz_render_default_projection();
    const float PI = 3.14159265358979323846f;
    const float f = 1.0f / tanf(pp.verticalFovDegrees * PI / 360.0f);
    const float aspect = (float)w / (float)h;
    float denom = -cz; if (denom < pp.nearPlane) denom = pp.nearPlane;
    float ndc_x = (cx * (f / aspect)) / denom;
    float ndc_y = (cy * f) / denom;
    *out_x = (ndc_x * 0.5f + 0.5f) * (float)w;
    *out_y = (1.0f - (ndc_y * 0.5f + 0.5f)) * (float)h;
}

// Clip polygon in camera space against near plane (cz <= -near)
static int clip_polygon_near(const float in_pts[][3], int in_count, float nearPlane, float out_pts[][3]) {
    if (in_count <= 0) return 0;
    float tmp[12][3];
    int out_n = 0;
    for (int i = 0; i < in_count; ++i) {
        int j = (i + 1) % in_count;
        const float* a = in_pts[i];
        const float* b = in_pts[j];
        bool a_in = (a[2] <= -nearPlane);
        bool b_in = (b[2] <= -nearPlane);
        if (a_in && b_in) {
            // keep b
            tmp[out_n][0] = b[0]; tmp[out_n][1] = b[1]; tmp[out_n][2] = b[2];
            ++out_n;
        } else if (a_in && !b_in) {
            // leaving: add intersection
            float t = ( (-nearPlane) - a[2]) / (b[2] - a[2]);
            tmp[out_n][0] = a[0] + t * (b[0] - a[0]);
            tmp[out_n][1] = a[1] + t * (b[1] - a[1]);
            tmp[out_n][2] = -nearPlane;
            ++out_n;
        } else if (!a_in && b_in) {
            // entering: add intersection then b
            float t = ( (-nearPlane) - a[2]) / (b[2] - a[2]);
            tmp[out_n][0] = a[0] + t * (b[0] - a[0]);
            tmp[out_n][1] = a[1] + t * (b[1] - a[1]);
            tmp[out_n][2] = -nearPlane;
            ++out_n;
            tmp[out_n][0] = b[0]; tmp[out_n][1] = b[1]; tmp[out_n][2] = b[2];
            ++out_n;
        } else {
            // both outside: add nothing
        }
        if (out_n >= 12) break;
    }
    for (int i = 0; i < out_n; ++i) { out_pts[i][0]=tmp[i][0]; out_pts[i][1]=tmp[i][1]; out_pts[i][2]=tmp[i][2]; }
    return out_n;
}

void oz_render_soft_draw_grid_axes(cairo_t* cr, int w, int h, const OzCamera* cam, bool draw_grid, bool draw_axes) {
    if (!cr) return;
    if (draw_grid) {
        cairo_set_source_rgba(cr, 0.45, 0.48, 0.52, 0.6);
        const float gridExtent = 20.0f, gridStep = 1.0f;
        for (float v=-gridExtent; v<=gridExtent+0.001f; v+=gridStep){
            float x0,y0,x1,y1;
            project_point(cam, -gridExtent, v, 0.0f, w,h,&x0,&y0);
            project_point(cam,  gridExtent, v, 0.0f, w,h,&x1,&y1);
            cairo_move_to(cr,x0,y0); cairo_line_to(cr,x1,y1); cairo_stroke(cr);
            project_point(cam, v, -gridExtent, 0.0f, w,h,&x0,&y0);
            project_point(cam, v,  gridExtent, 0.0f, w,h,&x1,&y1);
            cairo_move_to(cr,x0,y0); cairo_line_to(cr,x1,y1); cairo_stroke(cr);
        }
    }
    if (draw_axes) {
        cairo_set_source_rgba(cr, 0.7, 0.7, 0.8, 0.9);
        float ax0,ay0,ax1,ay1;
        project_point(cam,-20.0f,0.0f,0.0f,w,h,&ax0,&ay0); project_point(cam,20.0f,0.0f,0.0f,w,h,&ax1,&ay1);
        cairo_move_to(cr,ax0,ay0); cairo_line_to(cr,ax1,ay1); cairo_stroke(cr);
        project_point(cam,0.0f,-20.0f,0.0f,w,h,&ax0,&ay0); project_point(cam,0.0f,20.0f,0.0f,w,h,&ax1,&ay1);
        cairo_move_to(cr,ax0,ay0); cairo_line_to(cr,ax1,ay1); cairo_stroke(cr);
    }
}

void oz_render_soft_draw_map(cairo_t* cr, int w, int h, const OzMap* map, const OzCamera* cam, int selected_index) {
    if (!cr || !map) return;

    for (size_t brush_index = 0; brush_index < map->count; ++brush_index) {
        const OzBrush* brush = &map->brushes[brush_index];
        if (brush->type == OZ_BRUSH_BOX) {
            const OzBrushBox* box = &brush->as.box;
            float cx = box->center.x, cy = box->center.y, cz = box->center.z;
            float hx = box->half.x,    hy = box->half.y,    hz = box->half.z;
            float v[8][3] = {
                {cx-hx, cy-hy, cz-hz}, {cx+hx, cy-hy, cz-hz}, {cx+hx, cy+hy, cz-hz}, {cx-hx, cy+hy, cz-hz},
                {cx-hx, cy-hy, cz+hz}, {cx+hx, cy-hy, cz+hz}, {cx+hx, cy+hy, cz+hz}, {cx-hx, cy+hy, cz+hz}
            };
            int faces[6][4] = { {0,1,2,3}, {4,5,6,7}, {0,1,5,4}, {2,3,7,6}, {1,2,6,5}, {0,3,7,4} };

            // Precompute camera-space vertices
            float cv[8][3];
            for (int k = 0; k < 8; ++k) {
                world_to_camera(cam, v[k][0], v[k][1], v[k][2], &cv[k][0], &cv[k][1], &cv[k][2]);
            }

            // Prepare face order by average camera-space z (far to near)
            float depth[6];
            int order[6];
            for (int f = 0; f < 6; ++f) {
                order[f] = f;
                float zsum = 0.0f;
                for (int vi = 0; vi < 4; ++vi) zsum += cv[faces[f][vi]][2];
                depth[f] = zsum * 0.25f;
            }
            for (int a = 0; a < 5; ++a) {
                for (int b = a + 1; b < 6; ++b) {
                    if (depth[b] > depth[a]) { // draw far (larger z) first
                        float td = depth[a]; depth[a] = depth[b]; depth[b] = td;
                        int to = order[a]; order[a] = order[b]; order[b] = to;
                    }
                }
            }

            for (int oi = 0; oi < 6; ++oi) {
                int f = order[oi];
                // Backface culling in camera space
                float p0x = cv[faces[f][0]][0], p0y = cv[faces[f][0]][1], p0z = cv[faces[f][0]][2];
                float p1x = cv[faces[f][1]][0], p1y = cv[faces[f][1]][1], p1z = cv[faces[f][1]][2];
                float p2x = cv[faces[f][2]][0], p2y = cv[faces[f][2]][1], p2z = cv[faces[f][2]][2];
                float e1x = p1x - p0x, e1y = p1y - p0y, e1z = p1z - p0z;
                float e2x = p2x - p0x, e2y = p2y - p0y, e2z = p2z - p0z;
                float nx = e1y * e2z - e1z * e2y;
                float ny = e1z * e2x - e1x * e2z;
                float nz = e1x * e2y - e1y * e2x;
                float cxm = 0.0f, cym = 0.0f, czm = 0.0f;
                for (int vi = 0; vi < 4; ++vi) { cxm += cv[faces[f][vi]][0]; cym += cv[faces[f][vi]][1]; czm += cv[faces[f][vi]][2]; }
                cxm *= 0.25f; cym *= 0.25f; czm *= 0.25f;
                // If normal points away from origin (camera), skip
                float facing = nx * cxm + ny * cym + nz * czm;
                if (facing >= 0.0f) continue;

                // Near-plane clip polygon in camera space
                OzProjectionParams pp = oz_render_default_projection();
                float poly_in[8][3];
                for (int vi = 0; vi < 4; ++vi) { int idx = faces[f][vi]; poly_in[vi][0]=cv[idx][0]; poly_in[vi][1]=cv[idx][1]; poly_in[vi][2]=cv[idx][2]; }
                float poly_out[8][3];
                int pn = clip_polygon_near(poly_in, 4, pp.nearPlane, poly_out);
                if (pn < 3) continue;
                float sx, sy; project_point_camera(poly_out[0][0], poly_out[0][1], poly_out[0][2], w, h, &sx, &sy);
                cairo_move_to(cr, sx, sy);
                for (int vi = 1; vi < pn; ++vi) { project_point_camera(poly_out[vi][0], poly_out[vi][1], poly_out[vi][2], w, h, &sx, &sy); cairo_line_to(cr, sx, sy); }
                cairo_close_path(cr);
                cairo_set_source_rgba(cr, 0.7, 0.75, 0.8, 0.30);
                cairo_fill_preserve(cr);
                if ((int)brush_index == selected_index) cairo_set_source_rgba(cr, 1.0, 0.2, 0.2, 0.9);
                else cairo_set_source_rgba(cr, 0.9, 0.9, 0.95, 0.9);
                cairo_stroke(cr);
            }
        } else if (brush->type == OZ_BRUSH_CYLINDER) {
            const OzBrushCylinder* cyl = &brush->as.cyl;
            int seg = cyl->segments > 6 ? cyl->segments : 16;
            float hz = cyl->height * 0.5f;
            typedef struct { float depth; int n; float cpts[8][3]; } Poly;
            Poly polys[512]; int qc = 0; if (seg > 512) seg = 512;
            for (int s = 0; s < seg; ++s) {
                float a0 = (float)s / (float)seg * 6.2831853f;
                float a1 = (float)(s + 1) / (float)seg * 6.2831853f;
                float x0 = cyl->center.x + cosf(a0) * cyl->radius_x;
                float y0 = cyl->center.y + sinf(a0) * cyl->radius_y;
                float x1 = cyl->center.x + cosf(a1) * cyl->radius_x;
                float y1 = cyl->center.y + sinf(a1) * cyl->radius_y;
                float face4[4][3] = {
                    { x0, y0, cyl->center.z - hz },
                    { x1, y1, cyl->center.z - hz },
                    { x1, y1, cyl->center.z + hz },
                    { x0, y0, cyl->center.z + hz }
                };
                float cpts4[4][3];
                for (int k = 0; k < 4; ++k) world_to_camera(cam, face4[k][0], face4[k][1], face4[k][2], &cpts4[k][0], &cpts4[k][1], &cpts4[k][2]);
                // Backface test
                float e1x = cpts4[1][0] - cpts4[0][0], e1y = cpts4[1][1] - cpts4[0][1], e1z = cpts4[1][2] - cpts4[0][2];
                float e2x = cpts4[2][0] - cpts4[0][0], e2y = cpts4[2][1] - cpts4[0][1], e2z = cpts4[2][2] - cpts4[0][2];
                float nx = e1y * e2z - e1z * e2y;
                float ny = e1z * e2x - e1x * e2z;
                float nz = e1x * e2y - e1y * e2x;
                float cxm = 0.0f, cym = 0.0f, czm = 0.0f;
                for (int k = 0; k < 4; ++k) { cxm += cpts4[k][0]; cym += cpts4[k][1]; czm += cpts4[k][2]; }
                cxm *= 0.25f; cym *= 0.25f; czm *= 0.25f;
                float facing = nx * cxm + ny * cym + nz * czm;
                if (facing >= 0.0f) continue; // backface

                // Clip against near plane
                OzProjectionParams pp = oz_render_default_projection();
                float clipped[8][3];
                int pn = clip_polygon_near((const float (*)[3])cpts4, 4, pp.nearPlane, clipped);
                if (pn < 3) continue;
                if (qc < (int)(sizeof(polys)/sizeof(polys[0]))) {
                    polys[qc].n = pn;
                    polys[qc].depth = 0.0f; for (int k = 0; k < pn; ++k) { polys[qc].cpts[k][0]=clipped[k][0]; polys[qc].cpts[k][1]=clipped[k][1]; polys[qc].cpts[k][2]=clipped[k][2]; polys[qc].depth += clipped[k][2]; }
                    polys[qc].depth /= (float)pn;
                    ++qc;
                }
            }
            // Sort quads far-to-near
            for (int a = 0; a < qc - 1; ++a) { for (int b = a + 1; b < qc; ++b) { if (polys[b].depth > polys[a].depth) { Poly tmp = polys[a]; polys[a] = polys[b]; polys[b] = tmp; } } }
            for (int qi = 0; qi < qc; ++qi) {
                float sx, sy; project_point_camera(polys[qi].cpts[0][0], polys[qi].cpts[0][1], polys[qi].cpts[0][2], w, h, &sx, &sy);
                cairo_move_to(cr, sx, sy);
                for (int k = 1; k < polys[qi].n; ++k) { project_point_camera(polys[qi].cpts[k][0], polys[qi].cpts[k][1], polys[qi].cpts[k][2], w, h, &sx, &sy); cairo_line_to(cr, sx, sy); }
                cairo_close_path(cr);
                cairo_set_source_rgba(cr, 0.7, 0.75, 0.8, 0.22);
                cairo_fill_preserve(cr);
                cairo_set_source_rgba(cr, 0.9, 0.9, 0.95, 0.9);
                cairo_stroke(cr);
            }
        }
    }
}
