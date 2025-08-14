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
#include <math.h>
#include "oz/editor.h"

int main(int argc, char** argv) {
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    OZ_INFO("OzWorld SW demo %s", oz_core_version());

    struct OzWindowConfig cfg = { .width = 640, .height = 480, .title = "OzWorld Demo (SW)", .vsync = true };
    if (!oz_platform_init(&cfg)) { OZ_ERROR("Platform init failed"); return 1; }

    OzCamera cam; oz_camera_init(&cam, OZ_CAMERA_FREEMOVE);
    // Parse optional --mode fps|freemove|cinematic
    OzCameraMode mode = OZ_CAMERA_FREEMOVE;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--mode") == 0 && i + 1 < argc) {
            const char* m = argv[i+1];
            if (strcmp(m, "fps") == 0) mode = OZ_CAMERA_FPS;
            else if (strcmp(m, "freemove") == 0) mode = OZ_CAMERA_FREEMOVE;
            else if (strcmp(m, "cinematic") == 0) mode = OZ_CAMERA_CINEMATIC;
        }
    }
    cam.mode = mode;
    if (argc >= 3 && strcmp(argv[1], "--playerstart") == 0 && argv[2]) {
        double x=0,y=0,z=0,yaw=0; if (sscanf(argv[2], "%lf,%lf,%lf,%lf", &x,&y,&z,&yaw) == 4) {
            oz_camera_set_position(&cam, (float)x, (float)y, (float)z);
            oz_camera_set_angles(&cam, (float)yaw, 0.0f, 0.0f);
        }
    }
    OzMap map; oz_map_init(&map);
    oz_map_add_box(&map, (OzVec3){0.0f, 0.0f, 0.0f}, (OzVec3){1.0f, 1.0f, 1.0f});
    oz_map_add_cylinder(&map, (OzVec3){1.5f, 0.0f, 0.0f}, 0.5f, 0.5f, 1.0f, 16);
    oz_editor_init(&map);

    bool quit = false; (void)quit;
    double last_time = oz_platform_time_now_seconds();
    // Lighting toggle (L key): start unlit
    bool unlit = true;
    float light_pos[3] = { 2.0f, 2.0f, 3.0f };
    float light_intensity = 1.0f;
    // Simple editor UI state
    bool show_menu = true; // F1 toggles
    int menu_index = 0;    // 0: CSG/Brushes, 1: World
    while (!quit) {
        if (!oz_platform_pump_events(&quit)) break;
        double now = oz_platform_time_now_seconds();
        float dt = (float)(now - last_time); if (dt < 0.0001f) dt = 0.0001f; last_time = now;
        // Mouse look: hold left mouse to rotate
        if (oz_platform_is_mouse_look_active()) {
            float mdx=0.0f, mdy=0.0f; oz_platform_get_relative_mouse_delta(&mdx, &mdy);
            const float sens_yaw = 0.0025f;   // rad/pixel
            const float sens_pitch = 0.0020f; // rad/pixel
            cam.yaw   += mdx * sens_yaw;
            cam.pitch -= mdy * sens_pitch;
        }
        // Toggle unlit/lit with L key (edge detect)
        static bool last_l = false; bool l_now = oz_platform_key_down(OZ_KEY_L);
        if (l_now && !last_l) unlit = !unlit; last_l = l_now;
        // Update camera per mode using platform key state
        if (cam.mode == OZ_CAMERA_FPS) (void)oz_camera_update_fps(&cam, dt, oz_platform_key_down);
        else if (cam.mode == OZ_CAMERA_FREEMOVE) (void)oz_camera_update_freemove(&cam, dt, oz_platform_key_down);
        else {
            // Simple placeholder cinematic path: circle around origin at z=1.5
            static float t = 0.0f; t += dt * 0.5f; float r=5.0f;
            cam.position.x = cosf(t) * r; cam.position.y = sinf(t) * r; cam.position.z = 1.5f;
            cam.yaw = t + 3.14159f; cam.pitch = 0.0f;
        }
        int w=0,h=0; oz_platform_get_window_size(&w,&h); if (w<=0||h<=0){ w=640; h=480; }
        const int stride = w * 4;
        unsigned char* pixels = (unsigned char*)malloc((size_t)stride * (size_t)h);
        if (!pixels) break;
        // Solid background once per frame (no random flicker)
        memset(pixels, 0, (size_t)stride * (size_t)h);
        for (int y = 0; y < h; ++y) {
            unsigned char* row = pixels + (size_t)y * (size_t)stride;
            for (int x = 0; x < w; ++x) { row[x*4+0]=0x38; row[x*4+1]=0x2A; row[x*4+2]=0x1A; row[x*4+3]=0xFF; }
        }
        cairo_surface_t* surf = cairo_image_surface_create_for_data(pixels, CAIRO_FORMAT_ARGB32, w, h, stride);
        cairo_t* cr = cairo_create(surf);
        oz_render_soft_draw_grid_axes(cr, w, h, &cam, true, true);
        oz_render_soft_draw_map(cr, w, h, &map, &cam, -1);
        // Example: draw a lit quad placeholder near origin (pre-projected call interface kept minimal)
        float q[4][3] = { {0,0,0}, {1,0,0}, {1,1,0}, {0,1,0} }; // XY unit quad at z=0
        oz_render_soft_draw_lit_quad(cr, (float const (*)[3])q, 0,0,1, light_pos, light_intensity, unlit);
        oz_render_soft_draw_gizmos(cr, w, h, &cam);
        // Minimal text UI using Cairo: top-left menu
        if (show_menu) {
            cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
            cairo_set_font_size(cr, 13.0);
            cairo_set_source_rgba(cr, 1,1,1,0.95);
            float y = 16.0f;
            const char* tabs[2] = { "[CSG/Brushes]", "[World]" };
            char hdr[128]; snprintf(hdr, sizeof(hdr), "%s  %s", menu_index==0?">CSG/Brushes":" CSG/Brushes", menu_index==1?">World":" World");
            cairo_move_to(cr, 8.0, y); cairo_show_text(cr, hdr); y += 18.0f;
            if (menu_index == 0) {
                cairo_move_to(cr, 8.0, y); cairo_show_text(cr, "1) Add Box"); y += 16.0f;
                cairo_move_to(cr, 8.0, y); cairo_show_text(cr, "2) Add Cylinder"); y += 16.0f;
                cairo_move_to(cr, 8.0, y); cairo_show_text(cr, "U) CSG Union  I) Intersect  O) Subtract (uses last two brushes)"); y += 16.0f;
            } else {
                OzWorldProps* wp = oz_editor_world_props();
                char ln[160]; snprintf(ln, sizeof(ln), "Ambient: R=%.2f G=%.2f B=%.2f  GravityZ=%.2f (+/- to adjust)", wp->ambient_rgb[0], wp->ambient_rgb[1], wp->ambient_rgb[2], wp->gravity_z);
                cairo_move_to(cr, 8.0, y); cairo_show_text(cr, ln); y += 16.0f;
            }
        }
        cairo_destroy(cr);
        cairo_surface_destroy(surf);
        oz_platform_present_software_rgba32(pixels, w, h, stride);
        free(pixels);
        oz_platform_sleep_ms(16);

        // Handle simple UI hotkeys (edge detection would be nicer; keep it simple)
        static bool f1_last=false, tab_last=false; bool f1=oz_platform_key_down(OZ_KEY_F1); bool tab=oz_platform_key_down(OZ_KEY_TAB);
        if (f1 && !f1_last) show_menu = !show_menu; f1_last = f1;
        if (tab && !tab_last) menu_index = (menu_index+1)%2; tab_last = tab;
        if (show_menu) {
            if (menu_index == 0) {
                if (oz_platform_key_down(OZ_KEY_1)) { oz_editor_begin_add_box(); oz_editor_apply_current_tool(&cam); }
                if (oz_platform_key_down(OZ_KEY_2)) { oz_editor_begin_add_cyl(); oz_editor_apply_current_tool(&cam); }
                // CSG ops use last two brushes
                int a = (int)map.count - 2, b = (int)map.count - 1;
                if (a >= 0 && b >= 0) {
                    if (oz_platform_key_down(OZ_KEY_U)) { (void)oz_editor_csg_add(a,b); }
                    if (oz_platform_key_down(OZ_KEY_I)) { (void)oz_editor_csg_isect(a,b); }
                    if (oz_platform_key_down(OZ_KEY_O)) { (void)oz_editor_csg_sub(a,b); }
                }
            } else {
                OzWorldProps* wp = oz_editor_world_props();
                if (oz_platform_key_down(OZ_KEY_PLUS)) { wp->ambient_rgb[0]+=0.02f; wp->ambient_rgb[1]+=0.02f; wp->ambient_rgb[2]+=0.02f; }
                if (oz_platform_key_down(OZ_KEY_MINUS)) { wp->ambient_rgb[0]-=0.02f; wp->ambient_rgb[1]-=0.02f; wp->ambient_rgb[2]-=0.02f; }
                if (wp->ambient_rgb[0]<0) wp->ambient_rgb[0]=0; if (wp->ambient_rgb[1]<0) wp->ambient_rgb[1]=0; if (wp->ambient_rgb[2]<0) wp->ambient_rgb[2]=0;
                if (wp->ambient_rgb[0]>1) wp->ambient_rgb[0]=1; if (wp->ambient_rgb[1]>1) wp->ambient_rgb[1]=1; if (wp->ambient_rgb[2]>1) wp->ambient_rgb[2]=1;
                // Gravity adjust with Q/E while on World tab
                if (oz_platform_key_down(OZ_KEY_Q)) wp->gravity_z -= 0.1f;
                if (oz_platform_key_down(OZ_KEY_E)) wp->gravity_z += 0.1f;
            }
        }
    }

    oz_map_free(&map);
    oz_platform_shutdown();
    return 0;
}
