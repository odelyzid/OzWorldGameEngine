#include "oz/oz_core.h"
#include "oz/oz_platform.h"
#include "oz/oz_log.h"
#include "oz/oz_bsp.h"
#include "oz/oz_camera.h"
#include "oz/oz_render.h"
#include <cairo/cairo.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char** argv) {
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    OZ_INFO("OzWorld SW demo %s", oz_core_version());

    struct OzWindowConfig cfg = { .width = 640, .height = 480, .title = "OzWorld Demo (SW)", .vsync = true };
    if (!oz_platform_init(&cfg)) { OZ_ERROR("Platform init failed"); return 1; }

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

    bool quit = false; (void)quit;
    while (!quit) {
        if (!oz_platform_pump_events(&quit)) break;
        int w=0,h=0; oz_platform_get_window_size(&w,&h); if (w<=0||h<=0){ w=640; h=480; }
        const int stride = w * 4;
        unsigned char* pixels = (unsigned char*)malloc((size_t)stride * (size_t)h);
        if (!pixels) break;
        for (int y = 0; y < h; ++y) {
            unsigned char* row = pixels + (size_t)y * (size_t)stride;
            for (int x = 0; x < w; ++x) { row[x*4+0]=0xFF; row[x*4+1]=0x1F; row[x*4+2]=0x1A; row[x*4+3]=0xFF; }
        }
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
