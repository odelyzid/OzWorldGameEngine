#include "oz/oz_platform.h"
#include "oz/oz_log.h"

#ifdef OZ_HAVE_SDL2

#include <SDL.h>
#ifdef _WIN32
#include <windows.h>
#endif
#include <GL/gl.h>

static SDL_Window* s_window = NULL;
static SDL_GLContext s_gl = NULL;
static Uint8 s_keys[SDL_NUM_SCANCODES];
static bool s_software_renderer = false;

bool oz_platform_init(const struct OzWindowConfig* config) {
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    // Early environment diagnostics for display/GL issues
    const char* disp = getenv("DISPLAY");
    const char* libgl_ind = getenv("LIBGL_ALWAYS_INDIRECT");
    const char* libgl_dbg = getenv("LIBGL_DEBUG");
    const char* mesa_dbg = getenv("MESA_DEBUG");
    const char* sdl_videodrv = getenv("SDL_VIDEODRIVER");
    const char* gdk_gl = getenv("GDK_GL");
    const char* force_sw_env = getenv("OZ_FORCE_SOFTWARE");
    const bool is_remote = (disp && disp[0] != ':');
    OZ_INFO("Platform env: DISPLAY=%s remote=%s LIBGL_ALWAYS_INDIRECT=%s LIBGL_DEBUG=%s MESA_DEBUG=%s SDL_VIDEODRIVER=%s GDK_GL=%s OZ_FORCE_SOFTWARE=%s",
            disp ? disp : "(null)", is_remote ? "yes" : "no",
            libgl_ind ? libgl_ind : "(null)",
            libgl_dbg ? libgl_dbg : "(null)",
            mesa_dbg ? mesa_dbg : "(null)",
            sdl_videodrv ? sdl_videodrv : "(null)",
            gdk_gl ? gdk_gl : "(null)",
            force_sw_env ? force_sw_env : "(null)");

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        OZ_ERROR("SDL_Init failed: %s", SDL_GetError());
        return false;
    }

    const char* force_sw = force_sw_env;
    if (force_sw && (force_sw[0]=='1' || force_sw[0]=='t' || force_sw[0]=='T' || force_sw[0]=='y' || force_sw[0]=='Y')) {
        s_software_renderer = true;
    }

    if (!s_software_renderer) {
        OZ_INFO("Configuring SDL GL attributes for OpenGL 2.1 context");
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    }

    Uint32 flags = (s_software_renderer ? 0 : SDL_WINDOW_OPENGL) | SDL_WINDOW_SHOWN;
    OZ_INFO("Creating SDL window %dx%d flags=0x%x (OPENGL=%s)",
            config ? config->width : 800,
            config ? config->height : 600,
            (unsigned)flags, (flags & SDL_WINDOW_OPENGL) ? "yes" : "no");
    s_window = SDL_CreateWindow(
        config && config->title ? config->title : "OzWorld",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        config ? config->width : 800,
        config ? config->height : 600,
        flags
    );
    if (!s_window) {
        OZ_ERROR("SDL_CreateWindow failed: %s", SDL_GetError());
        // If there is no DISPLAY or remote X without GLX, try a headless no-window mode
        if (!disp || disp[0] == '\0') {
            OZ_WARN("No DISPLAY available; running headless software mode");
            s_software_renderer = true;
            return true;
        }
        return false;
    }

    if (!s_software_renderer) {
        // Try to create a GL context, but tolerate GLX failure over remote X
        OZ_INFO("Creating SDL GL context...");
        s_gl = SDL_GL_CreateContext(s_window);
        if (!s_gl) {
            OZ_WARN("SDL_GL_CreateContext failed: %s", SDL_GetError());
            return false;
        } else {
            const GLubyte* vendor = glGetString(GL_VENDOR);
            const GLubyte* renderer = glGetString(GL_RENDERER);
            const GLubyte* version = glGetString(GL_VERSION);
            OZ_INFO("GL context created: vendor=%s renderer=%s version=%s",
                    vendor ? (const char*)vendor : "?",
                    renderer ? (const char*)renderer : "?",
                    version ? (const char*)version : "?");
        }
    }

    if (!s_software_renderer && s_gl) {
        SDL_GL_SetSwapInterval(config && config->vsync ? 1 : 0);
    }

    OZ_INFO("Platform ready: software=%s window=%p glctx=%p",
            s_software_renderer ? "yes" : "no", (void*)s_window, s_gl);
    return true;
}

void oz_platform_shutdown(void) {
    if (s_gl) {
        SDL_GL_DeleteContext(s_gl);
        s_gl = NULL;
    }
    if (s_window) {
        SDL_DestroyWindow(s_window);
        s_window = NULL;
    }
    SDL_Quit();
}

bool oz_platform_pump_events(bool* out_should_quit) {
    if (out_should_quit) *out_should_quit = false;
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) {
            if (out_should_quit) *out_should_quit = true;
        }
    }
    // Get current keyboard state snapshot
    const Uint8* state = SDL_GetKeyboardState(NULL);
    memcpy(s_keys, state, sizeof(s_keys));
    return true;
}

void oz_platform_clear(float r, float g, float b, float a) {
    int w = 0, h = 0;
    oz_platform_get_window_size(&w, &h);
    if (w <= 0 || h <= 0) {
        // Fallback to querying SDL directly if needed
        if (s_window) SDL_GL_GetDrawableSize(s_window, &w, &h);
    }
    if (w > 0 && h > 0) {
        glViewport(0, 0, w, h);
    }
    glClearColor(r, g, b, a);
    glClearDepth(1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void oz_platform_swap_buffers(void) {
    if (!s_software_renderer && s_window) {
        SDL_GL_SwapWindow(s_window);
    }
}

void oz_platform_sleep_ms(unsigned int ms) {
    SDL_Delay(ms);
}

double oz_platform_time_now_seconds(void) {
    return (double)SDL_GetTicks() / 1000.0;
}

void oz_platform_get_window_size(int* out_width, int* out_height) {
    int w = 0, h = 0;
    if (s_window) {
        SDL_GL_GetDrawableSize(s_window, &w, &h);
    }
    if (out_width) *out_width = w;
    if (out_height) *out_height = h;
}

bool oz_platform_is_software_renderer(void) {
    return s_software_renderer;
}

static SDL_Scancode key_to_scancode(enum OzKey key) {
    switch (key) {
        case OZ_KEY_W: return SDL_SCANCODE_W;
        case OZ_KEY_A: return SDL_SCANCODE_A;
        case OZ_KEY_S: return SDL_SCANCODE_S;
        case OZ_KEY_D: return SDL_SCANCODE_D;
        case OZ_KEY_Q: return SDL_SCANCODE_Q;
        case OZ_KEY_E: return SDL_SCANCODE_E;
        case OZ_KEY_LEFT: return SDL_SCANCODE_LEFT;
        case OZ_KEY_RIGHT: return SDL_SCANCODE_RIGHT;
        case OZ_KEY_UP: return SDL_SCANCODE_UP;
        case OZ_KEY_DOWN: return SDL_SCANCODE_DOWN;
        case OZ_KEY_ESCAPE: return SDL_SCANCODE_ESCAPE;
        default: return SDL_SCANCODE_UNKNOWN;
    }
}

bool oz_platform_key_down(enum OzKey key) {
    SDL_Scancode sc = key_to_scancode(key);
    if (sc == SDL_SCANCODE_UNKNOWN) return false;
    return s_keys[sc] != 0;
}

#else // OZ_HAVE_SDL2

#warning "oz_platform_sdl.c compiled without OZ_HAVE_SDL2; provide another platform backend"

#endif // OZ_HAVE_SDL2
