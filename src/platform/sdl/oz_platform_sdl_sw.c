// Strict software-only X11/Cairo path to avoid any GLX requests during init.
#include "oz/oz_platform.h"
#include "oz/oz_log.h"

#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <cairo/cairo.h>
#include <cairo/cairo-xlib.h>

typedef struct OzX11Context {
    Display* display;
    int screen;
    Window window;
    Atom wm_delete;
    int width;
    int height;
    cairo_surface_t* cairo_surface; // xlib surface for the window
} OzX11Context;

static OzX11Context g_x11 = {0};
static bool g_keys[256];
static bool g_mouse_look = false;
static float g_rel_dx = 0.0f, g_rel_dy = 0.0f;

bool oz_platform_init(const struct OzWindowConfig* config) {
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    const char* disp = getenv("DISPLAY");
    const char* libgl_ind = getenv("LIBGL_ALWAYS_INDIRECT");
    const char* libgl_dbg = getenv("LIBGL_DEBUG");
    const char* mesa_dbg = getenv("MESA_DEBUG");
    const char* gdk_gl = getenv("GDK_GL");
    const char* force_sw_env = getenv("OZ_FORCE_SOFTWARE");
    const bool is_remote = (disp && disp[0] != ':');
    OZ_INFO("Platform env (SW): DISPLAY=%s remote=%s LIBGL_ALWAYS_INDIRECT=%s LIBGL_DEBUG=%s MESA_DEBUG=%s GDK_GL=%s OZ_FORCE_SOFTWARE=%s",
            disp ? disp : "(null)", is_remote ? "yes" : "no",
            libgl_ind ? libgl_ind : "(null)",
            libgl_dbg ? libgl_dbg : "(null)",
            mesa_dbg ? mesa_dbg : "(null)",
            gdk_gl ? gdk_gl : "(null)",
            force_sw_env ? force_sw_env : "(null)");

    // Open X display directly to avoid any GLX probing that SDL X11 might do
    g_x11.display = XOpenDisplay(NULL);
    if (!g_x11.display) { OZ_ERROR("XOpenDisplay failed (DISPLAY=%s)", disp ? disp : "(null)"); return false; }
    g_x11.screen = DefaultScreen(g_x11.display);
    int w = config ? config->width : 640;
    int h = config ? config->height : 480;
    Window root = RootWindow(g_x11.display, g_x11.screen);
    XSetWindowAttributes swa; memset(&swa, 0, sizeof(swa));
    swa.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask | StructureNotifyMask | PointerMotionMask | ButtonPressMask | ButtonReleaseMask;
    g_x11.window = XCreateWindow(
        g_x11.display,
        root,
        0, 0, (unsigned)w, (unsigned)h,
        0,
        CopyFromParent,
        InputOutput,
        CopyFromParent,
        CWEventMask,
        &swa
    );
    if (!g_x11.window) { XCloseDisplay(g_x11.display); g_x11.display=NULL; OZ_ERROR("XCreateWindow failed"); return false; }
    // Set WM_DELETE_WINDOW
    g_x11.wm_delete = XInternAtom(g_x11.display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(g_x11.display, g_x11.window, &g_x11.wm_delete, 1);
    // Title
    const char* title = (config && config->title) ? config->title : "OzWorld (SW)";
    XStoreName(g_x11.display, g_x11.window, title);
    XMapWindow(g_x11.display, g_x11.window);
    XFlush(g_x11.display);
    g_x11.width = w; g_x11.height = h;
    // Create a Cairo xlib surface bound to the window for presentation
    g_x11.cairo_surface = cairo_xlib_surface_create(g_x11.display, g_x11.window, DefaultVisual(g_x11.display, g_x11.screen), g_x11.width, g_x11.height);
    if (cairo_surface_status(g_x11.cairo_surface) != CAIRO_STATUS_SUCCESS) {
        OZ_WARN("Failed to create Cairo Xlib surface; software presentation may fail");
    }
    memset(g_keys, 0, sizeof(g_keys));
    OZ_INFO("Platform ready (SW/X11): window=0x%lx size=%dx%d", (unsigned long)g_x11.window, g_x11.width, g_x11.height);
    return true;
}

void oz_platform_shutdown(void) {
    if (g_x11.cairo_surface) { cairo_surface_destroy(g_x11.cairo_surface); g_x11.cairo_surface = NULL; }
    if (g_x11.window) { XDestroyWindow(g_x11.display, g_x11.window); g_x11.window = 0; }
    if (g_x11.display) { XCloseDisplay(g_x11.display); g_x11.display = NULL; }
}

static void set_key_state(KeySym sym, bool down) {
    // Map basic keys into small table
    switch (sym) {
        case XK_w: case XK_W: g_keys['W'] = down; break;
        case XK_a: case XK_A: g_keys['A'] = down; break;
        case XK_s: case XK_S: g_keys['S'] = down; break;
        case XK_d: case XK_D: g_keys['D'] = down; break;
        case XK_q: case XK_Q: g_keys['Q'] = down; break;
        case XK_e: case XK_E: g_keys['E'] = down; break;
        case XK_space: g_keys[' '] = down; break;
        case XK_l: case XK_L: g_keys['L'] = down; break;
        case XK_Left:  g_keys[0x25] = down; break;
        case XK_Right: g_keys[0x27] = down; break;
        case XK_Up:    g_keys[0x26] = down; break;
        case XK_Down:  g_keys[0x28] = down; break;
        case XK_Escape: g_keys[0x1B] = down; break;
        default: break;
    }
}

bool oz_platform_pump_events(bool* out_should_quit) {
    if (out_should_quit) *out_should_quit = false;
    if (!g_x11.display) return false;
    while (XPending(g_x11.display)) {
        XEvent ev; XNextEvent(g_x11.display, &ev);
        switch (ev.type) {
            case ConfigureNotify:
                if (ev.xconfigure.width != g_x11.width || ev.xconfigure.height != g_x11.height) {
                    g_x11.width = ev.xconfigure.width;
                    g_x11.height = ev.xconfigure.height;
                    if (g_x11.cairo_surface) { cairo_surface_destroy(g_x11.cairo_surface); }
                    g_x11.cairo_surface = cairo_xlib_surface_create(g_x11.display, g_x11.window, DefaultVisual(g_x11.display, g_x11.screen), g_x11.width, g_x11.height);
                }
                break;
            case ClientMessage:
                if ((Atom)ev.xclient.data.l[0] == g_x11.wm_delete) { if (out_should_quit) *out_should_quit = true; }
                break;
            case KeyPress: {
                KeySym sym = XLookupKeysym(&ev.xkey, 0); set_key_state(sym, true);
                if (sym == XK_Escape && g_mouse_look) { g_mouse_look = false; }
            } break;
            case KeyRelease: {
                // Handle auto-repeat: peek next event
                if (XEventsQueued(g_x11.display, QueuedAfterReading)) {
                    XEvent next; XPeekEvent(g_x11.display, &next);
                    if (next.type == KeyPress && next.xkey.time == ev.xkey.time && next.xkey.keycode == ev.xkey.keycode) {
                        // auto-repeat, ignore this release
                        break;
                    }
                }
                KeySym sym = XLookupKeysym(&ev.xkey, 0); set_key_state(sym, false);
            } break;
            case ButtonPress: {
                if (ev.xbutton.button == Button1) {
                    g_mouse_look = true;
                    // Center the pointer to reduce immediate delta jump
                    XWarpPointer(g_x11.display, None, g_x11.window, 0,0,0,0, g_x11.width/2, g_x11.height/2);
                    XFlush(g_x11.display);
                    g_rel_dx = g_rel_dy = 0.0f;
                }
            } break;
            case MotionNotify: {
                if (g_mouse_look) {
                    // Accumulate relative movement
                    int cx = g_x11.width/2;
                    int cy = g_x11.height/2;
                    g_rel_dx += (float)(ev.xmotion.x - cx);
                    g_rel_dy += (float)(ev.xmotion.y - cy);
                    // Recenter to simulate relative motion
                    XWarpPointer(g_x11.display, None, g_x11.window, 0,0,0,0, cx, cy);
                    XFlush(g_x11.display);
                }
            } break;
            default: break;
        }
    }
    return true;
}

void oz_platform_clear(float r, float g, float b, float a) { (void)r; (void)g; (void)b; (void)a; }
void oz_platform_swap_buffers(void) {}
void oz_platform_sleep_ms(unsigned int ms) { usleep(ms * 1000u); }

double oz_platform_time_now_seconds(void) {
    struct timeval tv; gettimeofday(&tv, NULL); return (double)tv.tv_sec + (double)tv.tv_usec/1e6;
}

void oz_platform_get_window_size(int* out_width, int* out_height) {
    if (out_width) *out_width = g_x11.width;
    if (out_height) *out_height = g_x11.height;
}

bool oz_platform_is_software_renderer(void) { return true; }

bool oz_platform_key_down(enum OzKey key) {
    switch (key) {
        case OZ_KEY_W: return g_keys['W'];
        case OZ_KEY_A: return g_keys['A'];
        case OZ_KEY_S: return g_keys['S'];
        case OZ_KEY_D: return g_keys['D'];
        case OZ_KEY_Q: return g_keys['Q'];
        case OZ_KEY_E: return g_keys['E'];
        case OZ_KEY_SPACE: return g_keys[' '];
        case OZ_KEY_L: return g_keys['L'];
        case OZ_KEY_F1: return g_keys[0x70];
        case OZ_KEY_TAB: return g_keys[0x09];
        case OZ_KEY_ENTER: return g_keys[0x0D];
        case OZ_KEY_1: return g_keys['1'];
        case OZ_KEY_2: return g_keys['2'];
        case OZ_KEY_3: return g_keys['3'];
        case OZ_KEY_U: return g_keys['U'];
        case OZ_KEY_I: return g_keys['I'];
        case OZ_KEY_O: return g_keys['O'];
        case OZ_KEY_MINUS: return g_keys['-'];
        case OZ_KEY_PLUS: return g_keys['='];
        case OZ_KEY_LEFT:  return g_keys[0x25];
        case OZ_KEY_RIGHT: return g_keys[0x27];
        case OZ_KEY_UP:    return g_keys[0x26];
        case OZ_KEY_DOWN:  return g_keys[0x28];
        case OZ_KEY_ESCAPE: return g_keys[0x1B];
        default: return false;
    }
}

void oz_platform_present_software_rgba32(const unsigned char* pixels, int width, int height, int stride) {
    if (!g_x11.display || !g_x11.window || !pixels || width<=0 || height<=0 || stride<=0) return;
    if (!g_x11.cairo_surface) return;
    cairo_surface_t* img = cairo_image_surface_create_for_data((unsigned char*)pixels, CAIRO_FORMAT_ARGB32, width, height, stride);
    if (cairo_surface_status(img) != CAIRO_STATUS_SUCCESS) { cairo_surface_destroy(img); return; }
    cairo_t* cr = cairo_create(g_x11.cairo_surface);
    // Letterbox fit
    double sx = (double)g_x11.width / (double)width;
    double sy = (double)g_x11.height / (double)height;
    double s = sx < sy ? sx : sy;
    double rw = (double)width * s, rh = (double)height * s;
    double dx = ((double)g_x11.width - rw) * 0.5;
    double dy = ((double)g_x11.height - rh) * 0.5;
    cairo_set_source_rgb(cr, 26.0/255.0, 31.0/255.0, 38.0/255.0);
    cairo_paint(cr);
    cairo_save(cr);
    cairo_translate(cr, dx, dy);
    cairo_scale(cr, s, s);
    cairo_set_source_surface(cr, img, 0, 0);
    cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_NEAREST);
    cairo_paint(cr);
    cairo_restore(cr);
    cairo_destroy(cr);
    cairo_surface_destroy(img);
    cairo_surface_flush(g_x11.cairo_surface);
    XFlush(g_x11.display);
}

void oz_platform_get_relative_mouse_delta(float* out_dx, float* out_dy) {
    if (out_dx) *out_dx = g_rel_dx;
    if (out_dy) *out_dy = g_rel_dy;
    g_rel_dx = g_rel_dy = 0.0f;
}

void oz_platform_set_mouse_look_active(bool active) {
    g_mouse_look = active;
    if (g_mouse_look) {
        XWarpPointer(g_x11.display, None, g_x11.window, 0,0,0,0, g_x11.width/2, g_x11.height/2);
        XFlush(g_x11.display);
        g_rel_dx = g_rel_dy = 0.0f;
    }
}

bool oz_platform_is_mouse_look_active(void) { return g_mouse_look; }
