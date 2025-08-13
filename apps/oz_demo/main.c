#include "oz/oz_core.h"
#include "oz/oz_platform.h"
#include "oz/oz_log.h"

#include <math.h>
#include "oz/oz_bsp.h"
#include "oz/oz_camera.h"
#include "oz/oz_render.h"
#include "oz/oz_audio.h"
#ifdef OZ_HAVE_SDL2
#include <GL/gl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <cairo/cairo.h>
#endif

static void draw_box_wireframe(const OzBrushBox* b) {
#ifdef OZ_HAVE_SDL2
    if (!b) return;
    const float cx = b->center.x, cy = b->center.y, cz = b->center.z;
    const float hx = b->half.x, hy = b->half.y, hz = b->half.z;
    float v[8][3] = {
        {cx - hx, cy - hy, cz - hz},
        {cx + hx, cy - hy, cz - hz},
        {cx + hx, cy + hy, cz - hz},
        {cx - hx, cy + hy, cz - hz},
        {cx - hx, cy - hy, cz + hz},
        {cx + hx, cy - hy, cz + hz},
        {cx + hx, cy + hy, cz + hz},
        {cx - hx, cy + hy, cz + hz},
    };
    int edges[12][2] = {
        {0,1},{1,2},{2,3},{3,0}, // bottom
        {4,5},{5,6},{6,7},{7,4}, // top
        {0,4},{1,5},{2,6},{3,7}  // verticals
    };
    glColor3f(0.05f, 0.05f, 0.05f);
    glBegin(GL_LINES);
    for (int i = 0; i < 12; ++i) {
        float* a = v[edges[i][0]];
        float* c = v[edges[i][1]];
        glVertex3f(a[0], a[1], a[2]);
        glVertex3f(c[0], c[1], c[2]);
    }
    glEnd();
#else
    (void)b;
#endif
}

static void draw_cylinder_wireframe(const OzBrushCylinder* c) {
#ifdef OZ_HAVE_SDL2
    if (!c) return;
    const float cx = c->center.x, cy = c->center.y, cz = c->center.z;
    const float rx = c->radius_x, ry = c->radius_y, hz = c->height * 0.5f;
    int seg = c->segments > 3 ? c->segments : 16;
    glColor3f(0.05f, 0.05f, 0.05f);
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < seg; ++i) {
        float a = (float)i / (float)seg * 6.2831853f;
        glVertex3f(cx + cosf(a)*rx, cy + sinf(a)*ry, cz - hz);
    }
    glEnd();
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < seg; ++i) {
        float a = (float)i / (float)seg * 6.2831853f;
        glVertex3f(cx + cosf(a)*rx, cy + sinf(a)*ry, cz + hz);
    }
    glEnd();
    glBegin(GL_LINES);
    for (int i = 0; i < seg; ++i) {
        float a = (float)i / (float)seg * 6.2831853f;
        float x = cx + cosf(a)*rx, y = cy + sinf(a)*ry;
        glVertex3f(x, y, cz - hz);
        glVertex3f(x, y, cz + hz);
    }
    glEnd();
#else
    (void)c;
#endif
}

static void draw_map_wireframe(const OzMap* map) {
#ifdef OZ_HAVE_SDL2
    if (!map) return;
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    for (size_t i = 0; i < map->count; ++i) {
        const OzBrush* br = &map->brushes[i];
        if (br->type == OZ_BRUSH_BOX) {
            draw_box_wireframe(&br->as.box);
        } else if (br->type == OZ_BRUSH_CYLINDER) {
            draw_cylinder_wireframe(&br->as.cyl);
        }
    }
#else
    (void)map;
#endif
}

static void apply_camera(const OzCamera* cam) {
#ifdef OZ_HAVE_SDL2
    int w=0,h=0; oz_platform_get_window_size(&w,&h); if (h==0) h=1;
    oz_render_gl_apply_camera(cam, w, h);
#else
    (void)cam;
#endif
}

static void draw_map_filled(const OzMap* map) {
#ifdef OZ_HAVE_SDL2
    if (!map) return;
    glEnable(GL_CULL_FACE);
    glColor3f(0.9f, 0.9f, 0.9f);
    for (size_t i = 0; i < map->count; ++i) {
        const OzBrush* br = &map->brushes[i];
        if (br->type == OZ_BRUSH_BOX) {
            const OzBrushBox* b = &br->as.box;
            const float cx=b->center.x, cy=b->center.y, cz=b->center.z;
            const float hx=b->half.x, hy=b->half.y, hz=b->half.z;
            float v[8][3] = {
                {cx - hx, cy - hy, cz - hz},
                {cx + hx, cy - hy, cz - hz},
                {cx + hx, cy + hy, cz - hz},
                {cx - hx, cy + hy, cz - hz},
                {cx - hx, cy - hy, cz + hz},
                {cx + hx, cy - hy, cz + hz},
                {cx + hx, cy + hy, cz + hz},
                {cx - hx, cy + hy, cz + hz},
            };
            int faces[6][4] = {
                {0,1,2,3}, {4,5,6,7}, {0,1,5,4}, {2,3,7,6}, {1,2,6,5}, {0,3,7,4}
            };
            glBegin(GL_QUADS);
            for (int f = 0; f < 6; ++f) {
                for (int vi = 0; vi < 4; ++vi) {
                    float* a = v[faces[f][vi]];
                    glVertex3f(a[0], a[1], a[2]);
                }
            }
            glEnd();
        } else if (br->type == OZ_BRUSH_CYLINDER) {
            const OzBrushCylinder* c = &br->as.cyl;
            const float cx=c->center.x, cy=c->center.y, cz=c->center.z;
            const float rx=c->radius_x, ry=c->radius_y, hz=c->height*0.5f;
            int seg = c->segments > 3 ? c->segments : 16;
            // sides
            glBegin(GL_QUAD_STRIP);
            for (int i2 = 0; i2 <= seg; ++i2) {
                int i = (i2 == seg) ? 0 : i2;
                float a = (float)i / (float)seg * 6.2831853f;
                float x = cx + cosf(a)*rx, y = cy + sinf(a)*ry;
                glVertex3f(x, y, cz - hz);
                glVertex3f(x, y, cz + hz);
            }
            glEnd();
            // bottom
            glBegin(GL_TRIANGLE_FAN);
            glVertex3f(cx, cy, cz - hz);
            for (int i = 0; i <= seg; ++i) {
                float a = (float)(i % seg) / (float)seg * 6.2831853f;
                glVertex3f(cx + cosf(a)*rx, cy + sinf(a)*ry, cz - hz);
            }
            glEnd();
            // top
            glBegin(GL_TRIANGLE_FAN);
            glVertex3f(cx, cy, cz + hz);
            for (int i = 0; i <= seg; ++i) {
                float a = (float)(i % seg) / (float)seg * 6.2831853f;
                glVertex3f(cx + cosf(a)*rx, cy + sinf(a)*ry, cz + hz);
            }
            glEnd();
        }
    }
#else
    (void)map;
#endif
}

int main(int argc, char** argv) {
    // Disable stdio buffering so logs flush before any crash
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    OZ_INFO("OzWorld version %s", oz_core_version());
    // Echo environment that influences rendering
    const char* disp = getenv("DISPLAY");
    const char* libgl_ind = getenv("LIBGL_ALWAYS_INDIRECT");
    const char* libgl_dbg = getenv("LIBGL_DEBUG");
    const char* mesa_dbg = getenv("MESA_DEBUG");
    const char* gdk_gl = getenv("GDK_GL");
    const char* force_sw = getenv("OZ_FORCE_SOFTWARE");
    OZ_INFO("Demo env: DISPLAY=%s LIBGL_ALWAYS_INDIRECT=%s LIBGL_DEBUG=%s MESA_DEBUG=%s GDK_GL=%s OZ_FORCE_SOFTWARE=%s",
            disp ? disp : "(null)",
            libgl_ind ? libgl_ind : "(null)",
            libgl_dbg ? libgl_dbg : "(null)",
            mesa_dbg ? mesa_dbg : "(null)",
            gdk_gl ? gdk_gl : "(null)",
            force_sw ? force_sw : "(null)");
    fprintf(stderr, "[demo-raw] DISPLAY=%s LIBGL_ALWAYS_INDIRECT=%s LIBGL_DEBUG=%s MESA_DEBUG=%s GDK_GL=%s OZ_FORCE_SOFTWARE=%s\n",
            disp ? disp : "(null)", libgl_ind ? libgl_ind : "(null)", libgl_dbg ? libgl_dbg : "(null)", mesa_dbg ? mesa_dbg : "(null)", gdk_gl ? gdk_gl : "(null)", force_sw ? force_sw : "(null)");

    struct OzWindowConfig cfg = { .width = 640, .height = 480, .title = "OzWorld Demo", .vsync = true };
    if (!oz_platform_init(&cfg)) {
        OZ_ERROR("Failed to initialize platform");
        return 1;
    }

    if (oz_platform_is_software_renderer()) {
        OZ_WARN("Running in software mode (no GL context). Using Cairo-style software renderer.");
        OzCamera cam; oz_camera_init(&cam, OZ_CAMERA_FREEMOVE);
        if (argc >= 3 && strcmp(argv[1], "--playerstart") == 0 && argv[2]) {
            double x=0,y=0,z=0,yaw=0; if (sscanf(argv[2], "%lf,%lf,%lf,%lf", &x,&y,&z,&yaw) == 4) {
                oz_camera_set_position(&cam, (float)x, (float)y, (float)z);
                oz_camera_set_angles(&cam, (float)yaw, 0.0f, 0.0f);
            }
        }
        OzMap map; oz_map_init(&map);
        oz_map_add_box(&map, (OzVec3){0.0f, 0.0f, 0.0f}, (OzVec3){1.0f, 1.0f, 1.0f});
        oz_map_add_cylinder(&map, (OzVec3){1.5f, 0.0f, 0.0f}, 0.5f, 0.5f, 1.0f, 16);
        bool quit = false;
        while (!quit) {
            if (!oz_platform_pump_events(&quit)) break;
            int w=0,h=0; oz_platform_get_window_size(&w,&h); if (w<=0||h<=0){ w=640; h=480; }
            // Allocate ARGB32 image
            const int stride = w * 4;
            unsigned char* pixels = (unsigned char*)malloc((size_t)stride * (size_t)h);
            if (!pixels) break;
            // Clear background
            for (int y = 0; y < h; ++y) {
                unsigned char* row = pixels + (size_t)y * (size_t)stride;
                for (int x = 0; x < w; ++x) {
                    row[x*4+0] = 0xFF; // B
                    row[x*4+1] = 0x1F; // G
                    row[x*4+2] = 0x1A; // R
                    row[x*4+3] = 0xFF; // A
                }
            }
            // Very simple wireframe overlay using oz_render_soft helpers via Cairo surface
            cairo_surface_t* surf = cairo_image_surface_create_for_data(pixels, CAIRO_FORMAT_ARGB32, w, h, stride);
            cairo_t* cr = cairo_create(surf);
            oz_render_soft_draw_grid_axes(cr, w, h, &cam, true, true);
            oz_render_soft_draw_map(cr, w, h, &map, &cam, -1);
            cairo_destroy(cr);
            cairo_surface_destroy(surf);
            oz_platform_present_software_rgba32(pixels, w, h, stride);
            free(pixels);
            oz_platform_sleep_ms(16);
        }
        oz_map_free(&map);
        oz_platform_shutdown();
        return 0;
    }

    // Initialize audio (optional)
    if (!oz_audio_init()) { OZ_WARN("Audio init failed or disabled."); }

    // Create a simple map with two brushes, save & load back
    OzMap map; oz_map_init(&map);
    oz_map_add_box(&map, (OzVec3){0.0f, 0.0f, 0.0f}, (OzVec3){1.0f, 1.0f, 1.0f});
    oz_map_add_cylinder(&map, (OzVec3){1.5f, 0.0f, 0.0f}, 0.5f, 0.5f, 1.0f, 16);
    oz_map_save_text("sample.ozone", &map);
    OzMap loaded; oz_map_init(&loaded);
    oz_map_load_text("sample.ozone", &loaded);

    double t0 = oz_platform_time_now_seconds();
    OzCamera cam; oz_camera_init(&cam, OZ_CAMERA_FREEMOVE);
    // Optional: parse --playerstart x,y,z,yaw
    if (argc >= 3 && strcmp(argv[1], "--playerstart") == 0 && argv[2]) {
        double x=0,y=0,z=0,yaw=0; if (sscanf(argv[2], "%lf,%lf,%lf,%lf", &x,&y,&z,&yaw) == 4) {
            oz_camera_set_position(&cam, (float)x, (float)y, (float)z);
            oz_camera_set_angles(&cam, (float)yaw, 0.0f, 0.0f);
        }
    }
    bool quit = false;
    // Attempt to play background music if present
    (void)oz_audio_play_music("music.ozmux", -1);

    while (!quit) {
        if (!oz_platform_pump_events(&quit)) break;

        float now = (float)oz_platform_time_now_seconds();
        static float last = 0.0f; if (last == 0.0f) last = now;
        float dt = now - last; last = now;
        oz_camera_update_freemove(&cam, dt, oz_platform_key_down);

        oz_platform_clear(0.1f, 0.12f, 0.15f, 1.0f);
        apply_camera(&cam);
        oz_render_gl_draw_map_filled(&loaded);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glColor3f(0,0,0);
        oz_render_gl_draw_map_wireframe(&loaded);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        // Debug overlay (simple bitmap text via glRasterPos + GLUT-like is absent; use color bars instead)
        // For now, just draw axes at the camera position for reference
        glDisable(GL_DEPTH_TEST);
        glBegin(GL_LINES);
        glColor3f(1,0,0); glVertex3f(cam.position.x, cam.position.y, cam.position.z); glVertex3f(cam.position.x+1.0f, cam.position.y, cam.position.z);
        glColor3f(0,1,0); glVertex3f(cam.position.x, cam.position.y, cam.position.z); glVertex3f(cam.position.x, cam.position.y+1.0f, cam.position.z);
        glColor3f(0,0,1); glVertex3f(cam.position.x, cam.position.y, cam.position.z); glVertex3f(cam.position.x, cam.position.y, cam.position.z+1.0f);
        glEnd();
        glEnable(GL_DEPTH_TEST);
        oz_platform_swap_buffers();

        oz_platform_sleep_ms(16);
    }

    oz_map_free(&map);
    oz_map_free(&loaded);
    oz_audio_shutdown();
    oz_platform_shutdown();
    return 0;
}
