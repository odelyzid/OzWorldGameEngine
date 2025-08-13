#include "oz/oz_render.h"
#include "oz/oz_bsp.h"
#include "oz/oz_camera.h"
#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif
#include <math.h>

static void draw_box_faces(const OzBrushBox* b) {
    if (!b) return;
    const float cx=b->center.x, cy=b->center.y, cz=b->center.z;
    const float hx=b->half.x, hy=b->half.y, hz=b->half.z;
    float v[8][3] = {
        {cx - hx, cy - hy, cz - hz}, {cx + hx, cy - hy, cz - hz}, {cx + hx, cy + hy, cz - hz}, {cx - hx, cy + hy, cz - hz},
        {cx - hx, cy - hy, cz + hz}, {cx + hx, cy - hy, cz + hz}, {cx + hx, cy + hy, cz + hz}, {cx - hx, cy + hy, cz + hz},
    };
    int faces[6][4] = {{0,1,2,3},{4,5,6,7},{0,1,5,4},{2,3,7,6},{1,2,6,5},{0,3,7,4}};
    glBegin(GL_QUADS);
    for (int f=0; f<6; ++f) {
        for (int vi=0; vi<4; ++vi) {
            float* a = v[faces[f][vi]]; glVertex3f(a[0],a[1],a[2]);
        }
    }
    glEnd();
}

static void draw_box_wire(const OzBrushBox* b) {
    if (!b) return;
    const float cx=b->center.x, cy=b->center.y, cz=b->center.z;
    const float hx=b->half.x, hy=b->half.y, hz=b->half.z;
    float v[8][3] = {
        {cx - hx, cy - hy, cz - hz}, {cx + hx, cy - hy, cz - hz}, {cx + hx, cy + hy, cz - hz}, {cx - hx, cy + hy, cz - hz},
        {cx - hx, cy - hy, cz + hz}, {cx + hx, cy - hy, cz + hz}, {cx + hx, cy + hy, cz + hz}, {cx - hx, cy + hy, cz + hz},
    };
    int edges[12][2] = { {0,1},{1,2},{2,3},{3,0}, {4,5},{5,6},{6,7},{7,4}, {0,4},{1,5},{2,6},{3,7} };
    glBegin(GL_LINES);
    for (int e=0;e<12;++e){ float* a=v[edges[e][0]]; float* b2=v[edges[e][1]]; glVertex3f(a[0],a[1],a[2]); glVertex3f(b2[0],b2[1],b2[2]); }
    glEnd();
}

// oz_render_default_projection is implemented in oz_render_common.c

void oz_render_gl_apply_camera(const OzCamera* cam, int w, int h) {
    if (!cam) return;
    if (h<=0) h = 1;
    if (w<=0) w = 1;
    glViewport(0,0,w,h);
    glEnable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    float aspect = (float)w/(float)h;
    OzProjectionParams pp = oz_render_default_projection();
    float top = pp.nearPlane * tanf(pp.verticalFovDegrees * 3.14159f/360.0f);
    float right = top * aspect;
    glFrustum(-right, right, -top, top, pp.nearPlane, pp.farPlane);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    glRotatef(-cam->pitch * 57.29578f, 1,0,0);
    glRotatef(-cam->yaw * 57.29578f, 0,0,1);
    glTranslatef(-cam->position.x, -cam->position.y, -cam->position.z);
}

void oz_render_gl_draw_map_filled(const OzMap* map) {
    if (!map) return;
    glEnable(GL_CULL_FACE);
    for (size_t i=0;i<map->count;++i){const OzBrush* br=&map->brushes[i]; if (br->type==OZ_BRUSH_BOX) draw_box_faces(&br->as.box); /* TODO: other types */}
}

void oz_render_gl_draw_map_wireframe(const OzMap* map) {
    if (!map) return;
    glDisable(GL_CULL_FACE);
    for (size_t i=0;i<map->count;++i){const OzBrush* br=&map->brushes[i]; if (br->type==OZ_BRUSH_BOX) draw_box_wire(&br->as.box); /* TODO: other types */}
}
